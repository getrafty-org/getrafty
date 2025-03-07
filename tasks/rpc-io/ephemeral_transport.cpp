#include "ephemeral_transport.hpp"

static std::atomic<uint64_t> next_port_ = 0;

namespace getrafty::rpc::io {

folly::coro::Task<> detail::ConnectedSocket::send(const Message& message) {
  if (isConnected() && !peer_handle_ptr_->send(message)) {
    co_yield folly::coro::co_error(std::runtime_error("connection lost"));
  }

  co_return;
}

folly::coro::Task<Message> detail::ConnectedSocket::recv() {
  // In real impl the recv should block until socket read timeout.
  while (isConnected()) {
    const auto m = co_await co_awaitTry(timeout(inbox_.take(), 1s));
    if (m.hasValue()) {
      co_return* m;
    }

    if(!peer_handle_ptr_->isConnected()) {
      co_yield folly::coro::co_error(std::runtime_error("connection lost"));
    }
  }

  co_yield folly::coro::co_error(std::runtime_error("connection closed"));
}

void detail::ConnectedSocket::connect() {
  if (!is_connected_.exchange(true, std::memory_order_relaxed)) {
    if(const auto s = server_.lock()) {
      peer_handle_ptr_ = s->broker_->attachClient(weak_from_this());
      if (!peer_handle_ptr_) {
        throw std::runtime_error("connection refused");
      }
    }
  }
}

void detail::ConnectedSocket::disconnect() {
  if(is_connected_.exchange(false, std::memory_order_relaxed)) {
    if(const auto s = server_.lock()) {
      std::lock_guard lock( s->connected_sockets_mutex_);
      for(auto it = s->connected_sockets_.begin(); it != s->connected_sockets_.end(); ++it) {
        auto connected_sock = it->lock();
        if(connected_sock && (this == connected_sock.get())) {
          s->connected_sockets_.erase(it);
          break;
        }
      }
    }
  }
}

bool detail::ConnectedSocket::isConnected() {
  return is_connected_.load(std::memory_order_relaxed);
}

// ----------------------------------------------------------------
// EphemeralClientSocket
// ----------------------------------------------------------------
EphemeralClientSocket::EphemeralClientSocket(std::string host, std::shared_ptr<IBroker> broker)
    : broker_(std::move(broker)), server_address_(std::move(host)), client_address_(folly::to<std::string>(next_port_.fetch_add(1))) {}

EphemeralClientSocket::~EphemeralClientSocket() {
  disconnect();
}

folly::coro::Task<> EphemeralClientSocket::send(const Message& message) {
  if (!peer_sock_ptr_->send(message)) {
    co_yield folly::coro::co_error(std::runtime_error("connection lost"));
  }
  co_return;
}

folly::coro::Task<Message> EphemeralClientSocket::recv() {
  while (isConnected()) {
    const auto m = co_await co_awaitTry(timeout(inbox_.take(), 1s));
    if (m.hasValue()) {
      co_return *m;
    }

    if (!peer_sock_ptr_->isConnected()) {
      co_yield folly::coro::co_error(std::runtime_error("connection lost"));
    }
  }

  co_yield folly::coro::co_error(std::runtime_error("connection closed"));
}

void EphemeralClientSocket::connect() {
  if (!is_connected_.exchange(true, std::memory_order_relaxed)) {
    peer_sock_ptr_ = broker_->attachClient(weak_from_this());
    if(!peer_sock_ptr_) {
      throw std::runtime_error("connection refused");
    }
  }
}

void EphemeralClientSocket::disconnect() {
  is_connected_.exchange(false, std::memory_order_relaxed);
}

bool EphemeralClientSocket::isConnected() {
  return is_connected_.load(std::memory_order_relaxed);
}

Address EphemeralClientSocket::getAddress() {
  return client_address_;
}

Address EphemeralClientSocket::getPeerAddress() {
  return server_address_;
}

// ----------------------------------------------------------------
// EphemeralServerSocket
// ----------------------------------------------------------------
EphemeralServerSocket::EphemeralServerSocket(std::string address, std::shared_ptr<IBroker> broker) : broker_(std::move(broker)), address_(std::move(address)) {}

EphemeralServerSocket::~EphemeralServerSocket() {
  if(is_connected_.load(std::memory_order_relaxed)) {
    stop();
  }
}

bool EphemeralServerSocket::start() {
  if (!is_connected_.exchange(true, std::memory_order_relaxed)) {
    broker_->attachServer(weak_from_this());
    return true;
  }

  return false;
}

bool EphemeralServerSocket::stop() {
  if (!is_connected_.exchange(false, std::memory_order_relaxed)) {
    return false;
  }

  broker_->detachServer(weak_from_this());

  std::vector<std::weak_ptr<detail::ConnectedSocket>> connectedSockets;
  {
    std::unique_lock lock(connected_sockets_mutex_);
    connectedSockets = std::move(connected_sockets_);
    connected_sockets_.clear();
  }

  for (const auto& weakSocket : connectedSockets) {
    if (const auto socket = weakSocket.lock()) {
      socket->disconnect();
    }
  }

  return true;
}

folly::coro::Task<IClientSocketPtr> EphemeralServerSocket::accept() {
  if(is_connected_.load(std::memory_order_relaxed)) {
    // TODO: can block during shutdown
    auto remote_client_address = co_await inbox_.take();
    auto sock = std::make_shared<detail::ConnectedSocket>(getAddress(), remote_client_address, shared_from_this());
    {
      std::lock_guard lock(connected_sockets_mutex_);
      connected_sockets_.push_back(sock);
    }
    sock->connect();
    co_return sock;
  }

  co_return {};
}

Address EphemeralServerSocket::getAddress() {
  return address_;
}

// ----------------------------------------------------------------
// Broker
// ----------------------------------------------------------------

void Broker::attachServer(const std::weak_ptr<EphemeralServerSocket> s) {
  std::lock_guard lock(mutex_);
  if (const auto sp = s.lock()) {
    live_server_sockets_[sp->getAddress()] = s;
  }
}

void Broker::detachServer(std::weak_ptr<EphemeralServerSocket> s) {
  std::lock_guard lock(mutex_);
  if (const auto sp = s.lock()) {
    if (const auto it = live_server_sockets_.find(sp->getAddress()); it != live_server_sockets_.end()) {
      live_server_sockets_.erase(it);
    }
  }
}

std::unique_ptr<IHandle> Broker::attachClient(const std::weak_ptr<IClientSocket> s) {
  if (const auto sock_initiator = std::dynamic_pointer_cast<EphemeralClientSocket>(s.lock())) {
    std::unique_lock lock(mutex_);
    const auto rendezvous = std::make_shared<Rendezvous>(sock_initiator);
    // server:client
    rendezvous_queue_[sock_initiator->getPeerAddress() + ":" + sock_initiator->getAddress()] = rendezvous;

    if (const auto server_it = live_server_sockets_.find(sock_initiator->getPeerAddress());
        server_it != live_server_sockets_.end()) {
      if (const auto server_socket = server_it->second.lock()) {
        // Signal server
        server_socket->inbox_.put(sock_initiator->getAddress());
        //
        // Wait
        lock.unlock();
        auto other_sock = rendezvous->wait();
        //
        // Cleanup
        lock.lock();
        if (const auto it = rendezvous_queue_.find(sock_initiator->getPeerAddress() + ":" + sock_initiator->getAddress()); it != rendezvous_queue_.end()) {
          rendezvous_queue_.erase(it);
        }
        //
        return std::make_unique<detail::Handle<detail::ConnectedSocket>>(other_sock);
      }
    }
  } else if (const auto sock_connected = std::dynamic_pointer_cast<detail::ConnectedSocket>(s.lock())) {
    std::unique_lock lock(mutex_);
    // server:client
    if (const auto it = rendezvous_queue_.find(sock_connected->getAddress() + ":" + sock_connected->getPeerAddress()); it != rendezvous_queue_.end()) {
      auto other_sock = it->second->signal(sock_connected);
      return std::make_unique<detail::Handle<EphemeralClientSocket>>(other_sock);
    }
  }

  return nullptr;
}


}  // namespace getrafty::rpc::io