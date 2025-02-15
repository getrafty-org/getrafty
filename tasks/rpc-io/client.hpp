#pragma once

#include <folly/futures/Future.h>
#include <future>
#include <utility>

#include "channel.hpp"
#include "timer.hpp"

namespace getrafty::rpc {

using namespace std::literals::chrono_literals;

class ISerializable {
 public:
  virtual ~ISerializable() = default;
  virtual void serialize(io::IMessage&) const = 0;
  virtual void deserialize(io::IMessage&) = 0;
};

class RpcError final : public std::runtime_error {
 public:
  enum ErrorCode : uint16_t {
    OK,
    SEND_TIMEOUT,
    RECV_TIMEOUT,
    PROC_TIMEOUT,
    OVERALL_TIMEOUT,
    APP_ERROR,
    FAILURE,
  };
  explicit RpcError(const ErrorCode code) : RpcError(code, "err"){};
  explicit RpcError(const ErrorCode code, const std::string& m)
      : runtime_error(m), code_(code){};

  [[nodiscard]] ErrorCode code() const { return code_; }

 private:
  const ErrorCode code_;
};

template <typename TReq, typename TResp>
concept SerializableCallPair = std::is_base_of_v<ISerializable, TReq> &&
                               std::is_base_of_v<ISerializable, TResp> &&
                               std::is_default_constructible_v<TResp>;

class Client {
 public:
  struct CallOptions {
    std::chrono::milliseconds send_timeout;
    std::chrono::milliseconds recv_timeout;
  };

  explicit Client(std::shared_ptr<io::IAsyncChannel> channel,
                  std::shared_ptr<io::Timer> timer)
      : channel_(std::move(channel)), timer_(std::move(timer)){};

  ~Client() = default;

  template <typename TReq, typename TResp>
  requires SerializableCallPair<TReq, TResp> folly::Future<TResp> call(
      const TReq& request,
      CallOptions options = {.send_timeout = 300ms, .recv_timeout = 1500ms});

 private:
  struct Inflight {
    folly::Promise<io::MessagePtr> future;
  };

  std::shared_ptr<io::IAsyncChannel> channel_;
  std::shared_ptr<io::Timer> timer_;

  std::atomic<uint64_t> next_xid_{};
  std::mutex inflight_requests_mutex_;
  std::unordered_map<uint64_t, Inflight> inflight_requests_;

  folly::Future<std::shared_ptr<io::IMessage>> doCall(
      const io::MessagePtr& message, CallOptions options);

  void completeInflight(io::Result result);

  void completeInflightWithFunc(uint64_t xid,
                                std::function<void(Inflight&)>&& do_set_func);
};

template <typename TReq, typename TResp>
requires SerializableCallPair<TReq, TResp> folly::Future<TResp> Client::call(
    const TReq& request, const CallOptions options) {
  auto message = channel_->createMessage();
  request.serialize(*message);

  return std::move(doCall(message, options))
      .thenValue([](const io::MessagePtr& m) {
        TResp response;
        response.deserialize(*m);
        return response;
      });
}

}  // namespace getrafty::rpc