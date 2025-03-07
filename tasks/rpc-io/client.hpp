#pragma once

#include <folly/coro/Task.h>
#include <folly/futures/Future.h>
#include <future>
#include <utility>
#include "error.hpp"
#include "folly/coro/AsyncScope.h"
#include "folly/stop_watch.h"
#include "timer.hpp"
#include "util.hpp"

namespace getrafty::rpc {

using enum getrafty::rpc::RpcError::Code;
using namespace std::literals::chrono_literals;

template <typename TReq, typename TResp>
concept SerializableCallPair = std::is_base_of_v<io::ISerializable, TReq> &&
                               std::is_base_of_v<io::ISerializable, TResp> &&
                               std::is_default_constructible_v<TResp>;

class Client {
 public:
  struct CallOptions {
    std::chrono::milliseconds send_timeout;
    std::chrono::milliseconds recv_timeout;
  };

  explicit Client(std::shared_ptr<io::IClientSocket> channel);
  ~Client();

  template <typename TReq, typename TResp>
  requires SerializableCallPair<TReq, TResp> folly::coro::Task<TResp> call(
      const std::string& method, const TReq& request,
      CallOptions options = {.send_timeout = 500ms, .recv_timeout = 5000ms});

 private:
  struct Inflight {
    bool setException(const RpcError& rpc_error);
    bool setValue(const io::MessagePtr& message);

    explicit Inflight(folly::Promise<io::MessagePtr>&& p) : promise(std::move(p)) {}

    std::atomic<bool> fulfilled_;
    folly::Promise<io::MessagePtr> promise;
  };

  std::shared_ptr<io::IClientSocket> channel_;

  std::atomic<uint64_t> next_xid_{0};
  folly::Synchronized<std::unordered_map<uint64_t, std::shared_ptr<Inflight>>>
      inflight_requests_;

  std::shared_ptr<Inflight> peekInflight(uint64_t xid);
  std::shared_ptr<Inflight> popInflight(uint64_t xid);
  std::pair<uint64_t, folly::Future<std::shared_ptr<io::IMessage>>>
  pushInflight();
};

template <typename TReq, typename TResp>
requires SerializableCallPair<TReq, TResp> folly::coro::Task<TResp>
Client::call(const std::string& method, const TReq& request,
             const CallOptions options) {
  auto requestMessage = channel_->createMessage();
  request.serialize(*requestMessage);

  auto [current_xid, future] = pushInflight();
  requestMessage->setSequenceId(current_xid);
  requestMessage->setMethod(method);

  auto [sendStatus, _] = co_await awaitCallback<io::Result>([&](auto callback) {
    channel_->sendMessage(std::move(callback), requestMessage,
                          options.send_timeout);
  });

  if (sendStatus != io::OK) {
    if (sendStatus == io::IO_TIMEOUT) {
      co_yield folly::coro::co_error(RpcError(SEND_TIMEOUT));
    }
    co_yield folly::coro::co_error(RpcError(FAILURE));
  }

  const folly::stop_watch<std::chrono::milliseconds> sw;
  auto err = io::OK;
  while (true) {
    co_await folly::coro::co_current_cancellation_token;

    if (const auto inflight = peekInflight(current_xid); !inflight) {
      err = io::ERR_UNKNOWN;
      break;
    }

    if(sw.elapsed() > options.recv_timeout) {
      err = io::IO_TIMEOUT;
      break;
    }

    auto [recvStatus, responseMessage] =
        co_await awaitCallback<io::Result>([&](auto callback) {
          channel_->recvMessage(std::move(callback), options.recv_timeout);
        });

    if(recvStatus == io::IO_TIMEOUT) {
      err = recvStatus;
      break;
    }

    if (recvStatus != io::OK) {
      err = recvStatus;
      continue; // Retry
    }

    assert(responseMessage);
    
    const auto other_xid = responseMessage->getSequenceId();
    if (const auto inflight = peekInflight(other_xid)) {
      if (const auto error = responseMessage->getErrorCode(); error != OK) {
        inflight->setException(RpcError(error, responseMessage->getBody()));
      } else {
        inflight->setValue(responseMessage);
      }
    }

    if (other_xid == current_xid) {
      break;
    }
  }

  if (err == io::IO_TIMEOUT) {
    const auto inflight = peekInflight(current_xid);
    assert(inflight);
    inflight->setException(RpcError(RECV_TIMEOUT));
  }

  auto responseMsg = co_await std::move(future).semi();
  TResp response;
  response.deserialize(*responseMsg);

  co_return response;
}

}  // namespace getrafty::rpc