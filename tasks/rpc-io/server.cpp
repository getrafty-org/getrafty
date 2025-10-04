#include "server.hpp"

#include <folly/coro/BlockingWait.h>
#include <folly/logging/Logger.h>
#include <utility>
#include "folly/gen/Base.h"
#include "folly/logging/xlog.h"
#include "util.hpp"

namespace getrafty::rpc {

Server::Server(const std::shared_ptr<io::IListener>& listener)
    : listener_(listener) {}

folly::coro::Task<> Server::start() {
  if (!is_running_.exchange(true, std::memory_order_relaxed)) {
    scope_.add(run().scheduleOn(folly::getGlobalCPUExecutor()));
  }
  co_return;
}

folly::coro::Task<> Server::run() {
  while (is_running_.load(std::memory_order_relaxed)) {
    const auto channel = co_await listener_->accept();
    assert(channel);
    assert(channel->isOpen());
    const auto conn = std::make_shared<Connection>(channel, this);
    scope_.add(conn->run().scheduleOn(folly::getGlobalCPUExecutor()));
  }
}

folly::coro::Task<> Server::stop() {
  if (is_running_.exchange(false, std::memory_order_relaxed)) {
    co_await scope_.joinAsync();
  }
}

folly::coro::Task<> Server::Connection::run() const {
  while (server_->is_running_.load(std::memory_order_relaxed)) {
    auto [status, message] =
        co_await awaitCallback<io::Result>([this](auto callback) {
          channel_->recvMessage(std::move(callback), /*keep_alive_timeout=*/1s);
        });

    if (status != io::OK ) {
      XLOG_EVERY_MS(ERR, 500) << "IO error:" << status;
      continue;
    }

    assert(message);

    io::MessagePtr resp;
    const auto respTry = co_await co_awaitTry(server_->dispatch(message));
    if (!respTry.hasException()) {
      resp = *respTry;
    } else {
      resp = message->constructFromCurrent();
      resp->setErrorCode(APP_ERROR);
      resp->setBody(respTry.exception().what().c_str());
    }

    co_await awaitCallback<io::Result>(
        [this, resp = std::move(resp)](auto callback) {
          channel_->sendMessage(std::move(callback), resp, 300ms);
        });
  }

  channel_->close();

  LOG(INFO) << "Closed";
  co_return;
}

folly::coro::Task<io::MessagePtr> Server::dispatch(io::MessagePtr msg) {
  std::shared_ptr<IHandlerWrapper> handler;
  const auto& method = msg->getMethod();
  {
    const auto r_lock = handlerRegistry_.rlock();
    if (const auto it = r_lock->find(method); it != r_lock->end()) {
      handler = it->second;
    }
  }

  if (!handler) {
    co_yield folly::coro::co_error(
        std::runtime_error("Handler not found for method: " + method));
  }


  co_return co_await handler->invoke(std::move(msg));
}

}  // namespace getrafty::rpc
