#pragma once

#include <folly/coro/AsyncScope.h>
#include <folly/coro/Task.h>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include "channel.hpp"
#include "folly/coro/BlockingWait.h"
#include "listener.hpp"
#include "client.hpp"
#include "error.hpp"

namespace getrafty::rpc {

template <typename TReq, typename TResp>
using HandlerFunc = std::function<folly::coro::Task<TResp>(const TReq&)>;

class IHandlerWrapper {
public:
  virtual ~IHandlerWrapper() = default;
  virtual folly::coro::Task<std::shared_ptr<io::IMessage>> invoke(std::shared_ptr<io::IMessage> msg) = 0;
};

class Server {
 public:
  explicit Server(const std::shared_ptr<io::IListener>&);
  ~Server() = default;

  template <typename TReq, typename TResp>
  requires SerializableCallPair<TReq, TResp> void addHandler(
      const std::string& method, HandlerFunc<TReq, TResp> handler);

  folly::coro::Task<> start();
  folly::coro::Task<> stop();

 private:
  std::string address_;

  folly::coro::AsyncScope scope_;
  std::shared_ptr<io::IListener> listener_;
  folly::Synchronized<std::unordered_map<std::string, std::shared_ptr<IHandlerWrapper>>> handlerRegistry_;

  std::atomic<bool> is_running_{false};

  folly::coro::Task<> run();
  folly::coro::Task<io::MessagePtr> dispatch(io::MessagePtr msg);

  struct Connection {
    explicit Connection(const std::shared_ptr<io::IAsyncChannel>& channel,
                        Server* server)
        : server_(server), channel_(channel) {}

    folly::coro::Task<> run() const;

    // TODO: stop

    Server* server_;
    std::shared_ptr<io::IAsyncChannel> channel_;
  };
};


template <typename TReq, typename TResp>
class HandlerWrapper final : public IHandlerWrapper {
public:
  explicit HandlerWrapper(HandlerFunc<TReq, TResp> func)
    : func_(std::move(func)) {}

  folly::coro::Task<std::shared_ptr<io::IMessage>> invoke(std::shared_ptr<io::IMessage> msg) override {
    TReq req;
    req.deserialize(*msg);
    TResp resp = co_await func_(req);
    auto respMsg = msg->constructFromCurrent();
    resp.serialize(*respMsg);
    co_return respMsg;
  }

private:
  HandlerFunc<TReq, TResp> func_;
};

template <typename TReq, typename TResp>
requires SerializableCallPair<TReq, TResp> void Server::addHandler(
    const std::string& method, HandlerFunc<TReq, TResp> handler) {
  auto wrapper = std::make_shared<HandlerWrapper<TReq, TResp>>(std::move(handler));
  (*handlerRegistry_.wlock())[method] = std::move(wrapper);
}

}  // namespace getrafty::rpc
