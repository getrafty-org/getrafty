#pragma once

#include <folly/futures/Future.h>
#include <future>
#include <utility>

#include "channel.hpp"
#include "event_watcher.hpp"
#include "folly/stop_watch.h"
#include "clock.hpp"


namespace getrafty::rpc {

using namespace std::literals::chrono_literals;

const std::string kXid = "xid";

class Serializable {
 public:
  virtual ~Serializable() = default;
  virtual void writeToMessage(
      std::shared_ptr<io::IAsyncChannel::IMessage> msg) const = 0;
  virtual void readFromMessage(std::shared_ptr<io::IAsyncChannel::IMessage> msg) = 0;
};


class RpcError final : public std::runtime_error {
public:
explicit RpcError(const std::string& m) : runtime_error(m) {};
};

class Client {
 public:
  struct CallOptions {
    std::chrono::microseconds send_timeout;
    std::chrono::microseconds recv_timeout;

    static CallOptions makeDefault() {
      return {
        .recv_timeout = 1500ms,
        .send_timeout = 300ms
      };
    }
  };

  explicit Client(std::shared_ptr<io::IAsyncChannel> channel, std::shared_ptr<io::IClock> clock)
      : channel_(std::move(channel)), clock_(std::move(clock)){};

  ~Client() = default;

  template <class TReq, class TResp>
  std::enable_if_t<std::is_base_of_v<Serializable, TReq> &&
                       std::is_base_of_v<Serializable, TResp> &&
                       std::is_default_constructible_v<TResp>,
                   folly::Future<TResp>>
  call(const TReq& request,
       CallOptions options = CallOptions::makeDefault());

 private:
  std::shared_ptr<io::IAsyncChannel> channel_;
  std::shared_ptr<io::IClock> clock_;

  std::atomic<uint64_t> next_xid_{};
  std::mutex pending_requests_mutex_;
  std::unordered_map<uint64_t, folly::Promise<io::MessagePtr>>
      pending_requests_;

private:
  void setResult(io::Result result) {
    if(!result.message) {
      throw std::runtime_error{"internal error: result.message is empty"};
    }

    const auto xid = result.message->getHeader(kXid);
    if(!xid) {
      // garbage message
      return;
    }

    std::optional<folly::Promise<io::MessagePtr>> p;
    {
      std::lock_guard lock(pending_requests_mutex_);
      const auto it = pending_requests_.find(folly::to<uint64_t>(*xid));
      if (it != pending_requests_.end()) {
        p = std::move(it->second);
        pending_requests_.erase(it);
        }
    }

    if(!p) {
      // likely expired
      return;
    }

    if(result.status != io::OK) {
      p->setException(std::runtime_error{"Failed"});
      return;
    }

    // ok
    p->setValue(std::move(result.message));
  };
};

template <typename TReq, typename TResp>
auto Client::call(const TReq& request, CallOptions options)
    -> std::enable_if_t<std::is_base_of_v<Serializable, TReq> &&
                            std::is_base_of_v<Serializable, TResp> &&
                            std::is_default_constructible_v<TResp>,
                        folly::Future<TResp>> {
  const auto xid = next_xid_++;
  auto message = channel_->createMessage();
  message->setHeader(kXid, std::to_string(xid));
  request.writeToMessage(message);

  auto promise = folly::Promise<io::MessagePtr>();
  auto future = promise.getFuture();
  {
    std::lock_guard lock(pending_requests_mutex_);
    pending_requests_[xid] = std::move(promise);
  }

  channel_->sendMessage(
      message,
      [this, recv_timeout = options.recv_timeout] (const io::Result& send_result) {
        if(send_result.status != io::OK) {
          setResult(send_result);
          return;
        }
        channel_->recvMessage(
            [this](const io::Result& recv_result) {
              if (recv_result.status != io::OK) {
                setResult(recv_result);
                return;
              }
              setResult(recv_result);
            },
            recv_timeout);
      },
      options.send_timeout);

  return std::move(future).thenValue([](io::MessagePtr m) {
    TResp response;
    response.readFromMessage(m);
    return response;
  });
}

}  // namespace getrafty::rpc