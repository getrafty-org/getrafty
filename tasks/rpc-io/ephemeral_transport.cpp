#include "ephemeral_transport.hpp"

static std::atomic<uint64_t> next_port_ = 0;

namespace getrafty::rpc::io {


// ----------------------------------------------------------------
// EphemeralClientSocket
// ----------------------------------------------------------------
EphemeralClientSocket::EphemeralClientSocket(std::string host, std::shared_ptr<IBroker> broker)
    : broker_(std::move(broker)), server_address_(std::move(host)), client_address_(folly::to<std::string>(next_port_.fetch_add(1))) {}

EphemeralClientSocket::~EphemeralClientSocket() {
  assert(!is_connected_.load(std::memory_order_relaxed));
}

folly::coro::Task<> EphemeralClientSocket::send(const Message& message) {
  if (!peer_ptr_->send(message)) {
    co_yield folly::coro::co_error(std::runtime_error("connection lost"));
  }
  co_return;
}

folly::coro::Task<Message> EphemeralClientSocket::recv() {
  co_return co_await inbox_.take();
}

void EphemeralClientSocket::connect() {
  if (!is_connected_.exchange(true, std::memory_order_relaxed)) {
    peer_ptr_ = broker_->attachClientSocket(weak_from_this());
  }
}

void EphemeralClientSocket::disconnect() {
  assert(is_connected_.exchange(false, std::memory_order_relaxed));
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
  assert(!stop());
}

bool EphemeralServerSocket::start() {
  if (!is_connected_.exchange(true, std::memory_order_relaxed)) {
    broker_->attachServerSocket(weak_from_this());
    return true;
  }

  return false;
}

bool EphemeralServerSocket::stop() {
  if(is_connected_.exchange(false, std::memory_order_relaxed)) {
    broker_->detachServerSocket(weak_from_this());
    return true;
  }

  return false;
}

folly::coro::Task<IClientSocketPtr> EphemeralServerSocket::accept() {
  auto source_address = co_await inbox_.take();
  auto sock = std::make_shared<detail::InternalClientSocket>(source_address, broker_);
  sock->connect();
  co_return sock;
}

Address EphemeralServerSocket::getAddress() {
  return address_;
}

// ----------------------------------------------------------------
// Broker
// ----------------------------------------------------------------

void Broker::attachServerSocket(const std::weak_ptr<EphemeralServerSocket> s) {
  std::lock_guard lock(mutex_);
  if (const auto sp = s.lock()) {
    live_server_sockets_[sp->getAddress()] = s;
  }
}

void Broker::detachServerSocket(std::weak_ptr<EphemeralServerSocket> s) {
  std::lock_guard lock(mutex_);
  if (const auto sp = s.lock()) {
    if (const auto it = live_server_sockets_.find(sp->getAddress()); it != live_server_sockets_.end()) {
      live_server_sockets_.erase(it);
    }
  }
}

std::unique_ptr<IHandle> Broker::attachClientSocket(const std::weak_ptr<IClientSocket> s) {
  if (const auto sock_ptr = std::dynamic_pointer_cast<EphemeralClientSocket>(s.lock())) {
    std::unique_lock lock(mutex_);
    auto rendezvous = std::make_shared<Rendezvous>(sock_ptr);
    rendezvous_queue_[sock_ptr->getPeerAddress()] = rendezvous;

    if (const auto server_it = live_server_sockets_.find(sock_ptr->getPeerAddress()); server_it != live_server_sockets_.end()) {
      if (const auto server_socket = server_it->second.lock()) {
        // Signal server
        server_socket->inbox_.put(sock_ptr->getPeerAddress());
        //
        // Wait
        lock.unlock();
        auto other_sock = rendezvous->wait();
        //
        // Cleanup
        lock.lock();
        if (const auto it = rendezvous_queue_.find(sock_ptr->getPeerAddress()); it != rendezvous_queue_.end()) {
          rendezvous_queue_.erase(it);
        }
        //
        return std::make_unique<detail::Handle<detail::InternalClientSocket>>(other_sock);
      }
    }
  } else if (const auto sock_ptr = std::dynamic_pointer_cast<detail::InternalClientSocket>(s.lock())) {
    std::unique_lock lock(mutex_);
    if (const auto it = rendezvous_queue_.find(sock_ptr->getPeerAddress()); it != rendezvous_queue_.end()) {
      auto other_sock = it->second->signal(sock_ptr);
      return std::make_unique<detail::Handle<EphemeralClientSocket>>(other_sock);
    }
  }

  return nullptr;
}

}  // namespace getrafty::rpc::io