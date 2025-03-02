#pragma once

#include "folly/coro/Task.h"

namespace getrafty::rpc::io {

using Message = std::string;
using Address = std::string;

class IClientSocket {
 public:
  virtual ~IClientSocket() = default;

  virtual folly::coro::Task<> send(const Message& message) = 0;

  virtual folly::coro::Task<Message> recv() = 0;

  virtual void connect() = 0;

  virtual void disconnect() = 0;

  virtual bool isConnected() = 0;

  virtual Address getAddress() = 0;

  virtual Address getPeerAddress() = 0;
};

using IClientSocketPtr = std::shared_ptr<IClientSocket>;

class IServerSocket {
 public:
  virtual ~IServerSocket() = default;

  virtual folly::coro::Task<IClientSocketPtr> accept() = 0;

  virtual bool start() = 0;

  virtual bool stop() = 0;

  virtual Address getAddress() = 0;
};

}  // namespace getrafty::rpc::io