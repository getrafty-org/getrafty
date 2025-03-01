#include "ephemeral_channel.hpp"

#include "folly/Singleton.h"
#include "timer.hpp"

namespace getrafty::rpc::io {

std::shared_ptr<IAsyncChannel> EphemeralChannel::create(
    const uint16_t address, const std::shared_ptr<ThreadPool>& pool) {
  auto channel =
      std::shared_ptr<EphemeralChannel>(new EphemeralChannel(address, pool));
  channel->attachChannel();
  return channel;
}

EphemeralChannel::EphemeralChannel(const uint16_t address,
                                   std::shared_ptr<ThreadPool> pool)
    : address_(address),
      tp_(std::move(pool)),
      timer_(std::make_shared<Timer>(EventWatcher::getInstance(), tp_)){}

EphemeralChannel::~EphemeralChannel() {
  detachChannel();
}
void EphemeralChannel::open() {
  if(!is_open_.exchange(true)) {
  }
}

void EphemeralChannel::close() {
  if (is_open_.exchange(false)) {
    if (on_close_) {
      (*on_close_)(shared_from_this());
    }
  }
}

bool EphemeralChannel::isOpen() {
  return is_open_;
}

void EphemeralChannel::sendMessage(AsyncCallback&& cob, MessagePtr message,
                                   std::chrono::milliseconds) {
  assert(is_open_);
  assert(message);
  if (const auto peer = findPeer(); !peer) {
    tp_->submit([cob = std::move(cob)]() mutable {
      cob({SOCK_CLOSED});
    });
  } else {
    tp_->submit([this, cob = std::move(cob)]() mutable {
      cob({OK});
    });
    peer->deliver(std::move(message));
  }
}

void EphemeralChannel::recvMessage(AsyncCallback&& cob,
                                   const std::chrono::milliseconds timeout) {
  assert(is_open_);
  const auto lock = inbox_.wlock();
  if (!lock->ready.empty()) {
    const auto m = lock->ready.front();
    lock->ready.erase(lock->ready.begin());
    if(cob) {
      cob({OK, m});
    }

  }

  auto cob_ptr = std::make_shared<std::atomic<bool>>(false);
  auto wrapped_cob = std::make_shared<AsyncCallback>(std::move(cob));
  auto ticket = timer_->schedule(timeout, [cob_ptr, wrapped_cob]() mutable {
    if (!cob_ptr->exchange(true)) {
      (*wrapped_cob)({IO_TIMEOUT});
    }
  });

  lock->consumers.emplace_back([this, cob_ptr, wrapped_cob, ticket](const Result& result) mutable {
    if (!cob_ptr->exchange(true)) {
      timer_->cancel(ticket);
      (*wrapped_cob)(result);
    }
  });
}

void EphemeralChannel::attachChannel() {
  std::lock_guard g(registryMutex_);
  auto& [first, second] = registry_[address_];
  const auto selfWeak = weak_from_this();

  if (first.expired()) {
    first = selfWeak;
  } else if (second.expired()) {
    second = selfWeak;
  }
}

void EphemeralChannel::detachChannel() const {
  std::lock_guard g(registryMutex_);
  const auto it = registry_.find(address_);
  if (it == registry_.end()) {
    return;
  }
  const auto selfPtr = this;
  auto& [first, second] = it->second;
  const auto f = first.lock();
  if (f.get() == selfPtr) {
    first.reset();
  }
  const auto s = second.lock();
  if (s.get() == selfPtr) {
    second.reset();
  }

  if (first.expired() && second.expired()) {
    registry_.erase(it);
  }
}
std::shared_ptr<EphemeralChannel> EphemeralChannel::findPeer() const {
  std::lock_guard g(registryMutex_);
  const auto it = registry_.find(address_);
  if (it == registry_.end()) {
    return nullptr;
  }
  const auto selfRaw = this;
  const auto& [first, second] = it->second;
  auto f = first.lock();
  auto s = second.lock();
  if (f.get() == selfRaw) {
    return s;
  }
  if (s.get() == selfRaw) {
    return f;
  }
  return nullptr;
}

void EphemeralChannel::deliver(MessagePtr msg) {
  assert(msg);
  auto lock = inbox_.wlock();
  if (lock->consumers.empty()) {
    lock->ready.emplace_back(msg);
    return;
  }

  // somebody is ready to consume message right now
  auto cob = std::move(lock->consumers.front());
  lock->consumers.erase(lock->consumers.begin());
  lock.unlock();
  tp_->submit([cob = std::move(cob), msg = std::move(msg)]() mutable {
    cob({OK, msg});
  });
}


void EphemeralChannel::setOnCloseCallback(
    std::shared_ptr<std::function<void(std::shared_ptr<IAsyncChannel>)>> callback) {
  on_close_ = std::move(callback);
}

}  // namespace getrafty::rpc::io