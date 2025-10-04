#pragma once

#include <folly/coro/Task.h>
#include <chrono>
#include <condition_variable>
#include <future>
#include <iostream>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <utility>
#include "coro/queue.hpp"
#include "folly/coro/Sleep.h"
#include "folly/coro/Timeout.h"
#include "transport.hpp"

namespace getrafty::rpc::io {

using namespace std::chrono_literals;

using MQ = wheels::concurrent::coro::UnboundedBlockingQueue<Message>;

class Broker;
class EphemeralClientSocket;
class EphemeralServerSocket;

// ----------------------------------------------------------------
// Broker
// ----------------------------------------------------------------

struct IHandle {
  virtual ~IHandle() = default;
  virtual bool send(const Message& m) = 0;
  virtual bool isConnected() = 0;
};

using IHandlePtr = std::unique_ptr<IHandle>;

struct IBroker {
  virtual ~IBroker() = default;
  virtual void attachServer(std::weak_ptr<EphemeralServerSocket>) = 0;
  virtual void detachServer(std::weak_ptr<EphemeralServerSocket>) = 0;
  virtual IHandlePtr attachClient(std::weak_ptr<IClientSocket>) = 0;
};

using IBrokerPtr = std::shared_ptr<IBroker>;

namespace detail {

class ConnectedSocket final
    : public IClientSocket,
      public std::enable_shared_from_this<ConnectedSocket> {
 public:
  explicit ConnectedSocket(std::string local_server_address,
                           std::string remote_client_address,
                           std::weak_ptr<EphemeralServerSocket> server)
      : server_address_(std::move(local_server_address)),
        client_address_(std::move(remote_client_address)),
        server_(std::move(server)) {}

  ~ConnectedSocket() override { disconnect(); }

  folly::coro::Task<> send(const Message& message) override;

  folly::coro::Task<Message> recv() override;

  void connect() override;

  void disconnect() override;

  bool isConnected() override;

  Address getAddress() override { return server_address_; };

  Address getPeerAddress() override { return client_address_; };

  std::string server_address_;
  std::string client_address_;

  std::atomic<bool> is_connected_{false};

  std::unique_ptr<IHandle> peer_handle_ptr_;
  std::weak_ptr<EphemeralServerSocket> server_;

  MQ inbox_;
};

template <typename TSocket>
struct Handle final : IHandle {
  explicit Handle(std::weak_ptr<TSocket> peer_ptr)
      : peer_ptr_(std::move(peer_ptr)){};

  bool send(const Message& m) override {
    if (const auto p = peer_ptr_.lock()) {
      if (p->isConnected()) {
        p->inbox_.put(m);
        return true;
      }
    }
    return false;
  }

  bool isConnected() override {
    if (const auto p = peer_ptr_.lock()) {
      if (p->isConnected()) {
        return true;
      }
    }
    return false;
  }

  std::weak_ptr<TSocket> peer_ptr_;
};

}  // namespace detail

// ----------------------------------------------------------------
// EphemeralClientSocket
// ----------------------------------------------------------------
class EphemeralClientSocket final
    : public IClientSocket,
      public std::enable_shared_from_this<EphemeralClientSocket> {

 public:
  explicit EphemeralClientSocket(std::string host, IBrokerPtr broker);

  ~EphemeralClientSocket() override;

  folly::coro::Task<> send(const Message& message) override;

  folly::coro::Task<Message> recv() override;

  void connect() override;

  void disconnect() override;

  bool isConnected() override;

  Address getAddress() override;

  Address getPeerAddress() override;

 private:
  friend class detail::Handle<EphemeralClientSocket>;

  IBrokerPtr broker_;
  const std::string server_address_;
  const std::string client_address_;
  IHandlePtr peer_sock_ptr_;

  std::atomic<bool> is_connected_{false};

  MQ inbox_;
};

// ----------------------------------------------------------------
// EphemeralServerSocket
// ----------------------------------------------------------------
class EphemeralServerSocket final
    : public IServerSocket,
      public std::enable_shared_from_this<EphemeralServerSocket> {
 public:
  explicit EphemeralServerSocket(std::string address, IBrokerPtr broker);

  ~EphemeralServerSocket() override;

  bool start() override;

  bool stop() override;

  folly::coro::Task<IClientSocketPtr> accept() override;

  Address getAddress() override;

 private:
  friend class Broker;
  friend class detail::ConnectedSocket;
  friend class detail::Handle<EphemeralServerSocket>;

  IBrokerPtr broker_;
  std::string address_;
  std::atomic<bool> is_connected_{false};

  std::mutex connected_sockets_mutex_;
  std::vector<std::weak_ptr<detail::ConnectedSocket>> connected_sockets_;

  // Inbox
  MQ inbox_;
};

// ----------------------------------------------------------------
// Broker
// ----------------------------------------------------------------

class Broker final : public IBroker {
 public:
  ~Broker() override = default;

  void attachServer(std::weak_ptr<EphemeralServerSocket>) override;
  void detachServer(std::weak_ptr<EphemeralServerSocket>) override;
  IHandlePtr attachClient(std::weak_ptr<IClientSocket> s) override;

 private:
  class Rendezvous {
   public:
    explicit Rendezvous(std::weak_ptr<EphemeralClientSocket> earlycomer)
        : earlycomer_(std::move(earlycomer)), future_(promise_.get_future()){};

    std::weak_ptr<EphemeralClientSocket> signal(
        const std::weak_ptr<detail::ConnectedSocket>& sock) {
      promise_.set_value(sock);
      return earlycomer_;
    }

    std::weak_ptr<detail::ConnectedSocket> wait() {
      return future_.get();
    }

   private:
    const std::weak_ptr<EphemeralClientSocket> earlycomer_;
    std::promise<std::weak_ptr<detail::ConnectedSocket>> promise_;
    std::future<std::weak_ptr<detail::ConnectedSocket>> future_;
  };

  std::mutex mutex_;
  std::unordered_map<Address, std::weak_ptr<EphemeralServerSocket>>
      live_server_sockets_;
  std::unordered_map<Address, std::shared_ptr<Rendezvous>> rendezvous_queue_;
};

}  // namespace getrafty::rpc::io
