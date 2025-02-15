#include "client.hpp"

namespace getrafty::rpc {

folly::coro::Task<std::shared_ptr<io::IMessage>> Client::doCall(
    const io::MessagePtr& message, CallOptions options) {
  const auto xid = next_xid_++;
  message->writeXID(xid);

  auto promise = folly::Promise<io::MessagePtr>();
  auto future = promise.getFuture();
  {
    std::lock_guard lock(inflight_requests_mutex_);
    inflight_requests_[xid] = {std::move(promise)};
  }

  auto send_timeout_timer = timer_->schedule(
      options.send_timeout, [this, xid, send_timeout = options.send_timeout] {
        completeInflightWithFunc(xid, [send_timeout](Inflight& in) {
          in.future.setException(RpcError{
              RpcError::SEND_TIMEOUT,
              fmt::format("send timeout after {}ms", send_timeout.count())});
        });
      });

  channel_->sendMessage(
      [this, options, xid, send_timeout_timer](const io::Result& send_result) {
        timer_->cancel(send_timeout_timer);

        if (send_result.status != io::OK) {
          completeInflight(send_result);
          return;
        }

        auto recv_timeout_timer = timer_->schedule(
            options.recv_timeout,
            [this, xid, recv_timeout = options.recv_timeout]() {
              completeInflightWithFunc(xid, [recv_timeout](Inflight& in) {
                in.future.setException(
                    RpcError{RpcError::RECV_TIMEOUT,
                             fmt::format("recv timeout after {}ms",
                                         recv_timeout.count())});
              });
            });

        channel_->recvMessage(
            [this, recv_timeout_timer](const io::Result& recv_result) {
              timer_->cancel(recv_timeout_timer);
              completeInflight(recv_result);
            });
      },
      message);

  co_return co_await std::move(future).semi();
};

void Client::completeInflight(io::Result result) {
  if (!result.message) {
    throw RpcError{RpcError::FAILURE, "result.message is empty"};
  }
  const auto xid = result.message->consumeXID();

  if (result.status != io::OK) {
    completeInflightWithFunc(xid, [](Inflight& p) {
      p.future.setException(RpcError{RpcError::FAILURE, "IO failed"});
    });
    return;
  }

  // ok
  completeInflightWithFunc(xid,
                           [message = std::move(result.message)](Inflight& p) {
                             p.future.setValue(message);
                           });
}

void Client::completeInflightWithFunc(
    const uint64_t xid, std::function<void(Inflight&)>&& do_set_func) {
  std::optional<Inflight> p;
  {
    std::lock_guard lock(inflight_requests_mutex_);
    auto it = inflight_requests_.find(xid);  // NOLINT
    if (it != inflight_requests_.end()) {
      p = std::move(it->second);
      inflight_requests_.erase(it);
    }
  }

  if (p) {
    do_set_func(*p);
  }
}
}  // namespace getrafty::rpc