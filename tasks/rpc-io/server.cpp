#include "server.hpp"

#include <folly/coro/BlockingWait.h>
#include <folly/logging/Logger.h>
#include <utility>
#include "folly/logging/xlog.h"
#include "util.hpp"

namespace getrafty::rpc {

Server::Server(std::string address)
    : address_(std::move(address)),
      listener_factory_(std::make_shared<io::IListenerFactory>(
          [](const std::string&) -> io::IListener {
            throw std::runtime_error("No listener factory provided");
          })) {}

void Server::setListenerFactory(
    const std::shared_ptr<io::IListenerFactory>& factory) {
  listener_factory_ = factory;
}

folly::coro::Task<> Server::run() {
  while (is_running_.load(std::memory_order_acquire)) {
    const auto channel = co_await listener_->accept();
    const auto conn = std::make_shared<Connection>(channel, this);
    co_await asyncScope_.co_schedule(conn->run());
  }
}

folly::coro::Task<> Server::start() {
  if (!is_running_.exchange(true, std::memory_order_release)) {
    co_await asyncScope_.co_schedule(run());
  }
}

folly::coro::Task<> Server::stop() {
  if (is_running_.exchange(false, std::memory_order_release)) {
    co_await asyncScope_.cancelAndJoinAsync();
  }
}

folly::coro::Task<> Server::Connection::run() const {
  while (server_->is_running_.load(std::memory_order_acquire)) {
    auto [status, message] = co_await awaitCallback<io::Result>(
        [this](auto callback) { channel_->recvMessage(std::move(callback), 60s); }); // TODO: keep alive timeout

    if (status != io::OK || !message) {
      XLOG_EVERY_MS(ERR, 500) << "IO error:" << status;
      continue;
    }

    io::MessagePtr resp;
    const auto respTry = co_await co_awaitTry(server_->dispatch(message));
    if(!respTry.hasException()) {
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

  if(!handler) {
    co_yield folly::coro::co_error(
      std::runtime_error("Handler not found for method: " + method)
    );
  }

  co_return co_await handler->invoke(std::move(msg));
}

}  // namespace getrafty::rpc
