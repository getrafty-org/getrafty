#pragma once

#include <folly/futures/Future.h>
#include <future>
#include <utility>

#include "channel.hpp"
#include "event_watcher.hpp"

const std::string kCorrelationId = "xid";

namespace getrafty::rpc {

class Serializable {
 public:
  virtual ~Serializable() = default;
  virtual void writeToMessage(
      std::shared_ptr<io::IChannel::IMessage> msg) const = 0;
  virtual void readFromMessage(std::shared_ptr<io::IChannel::IMessage> msg) = 0;
};

class Client {
 public:
  struct CallOptions {
    std::uint32_t timeout_ms;
  };

  explicit Client(std::shared_ptr<io::IChannel> channel)
      : channel_(std::move(channel)){};

  ~Client() = default;

  template <typename TRequest, typename TResponse>
  std::enable_if_t<std::is_base_of_v<Serializable, TRequest> &&
                       std::is_base_of_v<Serializable, TResponse> &&
                       std::is_default_constructible_v<TResponse>,
                   folly::Future<TResponse>>
  call(const TRequest& request,
       const std::optional<CallOptions>& options = std::nullopt);

 private:
  std::shared_ptr<io::IChannel> channel_;

  std::atomic<uint64_t> next_correlation_id_{};
  std::mutex pending_requests_mutex_;
  std::unordered_map<uint64_t, folly::Promise<io::IChannel::MessagePtr>>
      pending_requests_;
};

template <typename TRequest, typename TResponse>
std::enable_if_t<std::is_base_of_v<Serializable, TRequest> &&
                     std::is_base_of_v<Serializable, TResponse> &&
                     std::is_default_constructible_v<TResponse>,
                 folly::Future<TResponse>>
Client::call(const TRequest& request,
             const std::optional<CallOptions>& options) {
  auto correlation_id = next_correlation_id_++;
  auto message = channel_->createMessage();
  message->setHeader(kCorrelationId, std::to_string(correlation_id));
  request.writeToMessage(message);

  auto promise = folly::Promise<io::IChannel::MessagePtr>();
  auto future = promise.getFuture();

  {
    std::lock_guard lock(pending_requests_mutex_);
    pending_requests_[correlation_id] = std::move(promise);
  }

  auto timeout = options ? options->timeout_ms : 1000;

  // TODO: timer

  channel_->sendAsync(
      message,
      [this, correlation_id, timeout] {
        channel_->receiveAsync(
            [this, correlation_id](const io::IChannel::MessagePtr& m) {
              const auto received_id_str = m->getHeader(kCorrelationId);
              if (!received_id_str) {
                // TODO: handle it
                return;
              }

              std::optional<folly::Promise<io::IChannel::MessagePtr>> p;

              {
                const auto received_id = folly::to<uint32_t>(*received_id_str);
                std::lock_guard lock(pending_requests_mutex_);
                if (const auto it = pending_requests_.find(received_id);
                    it != pending_requests_.end()) {
                  p = std::move(it->second);
                  pending_requests_.erase(it);
                }
              }

              if (!p) {
                // TODO: should we retry receive here?
                return;
              }

              p->setValue(m);
            },
            timeout);
      },
      timeout);

  return std::move(future).thenValue([](io::IChannel::MessagePtr m) {
    TResponse response;
    response.readFromMessage(m);
    return response;
  });
}

}  // namespace getrafty::rpc