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
      timer_(std::make_shared<Timer>(EventWatcher::getInstance(), tp_)) {}

EphemeralChannel::~EphemeralChannel() {
  detachChannel();
}

void EphemeralChannel::sendMessage(AsyncCallback&& cob, MessagePtr message,
                                   std::chrono::milliseconds) {
  if (const auto peer = findPeer(); !peer) {
    tp_->submit([cob = std::move(cob)]() mutable {
      cob({SOCK_CLOSED});
    });
    return;
  }

  tp_->submit([this, cob = std::move(cob), message = std::move(message)]() mutable {
    if (const auto peer = findPeer(); peer) {
      cob({OK});
      peer->deliver(std::move(message));
    }
  });
}

void EphemeralChannel::recvMessage(AsyncCallback&& cob,
                                   const std::chrono::milliseconds timeout) {
  std::unique_lock lock(channelMutex_);
  if (!inbox_.empty()) {
    const auto msg = pickMessage();
    lock.unlock();
    cob({IOStatus::OK, msg});
    return;
  }

  auto cob_ptr = std::make_shared<std::atomic<bool>>(false);
  auto wrapped_cob = std::make_shared<AsyncCallback>(std::move(cob));
  auto ticket = timer_->schedule(timeout, [cob_ptr, wrapped_cob]() mutable {
    if (!cob_ptr->exchange(true)) {
      (*wrapped_cob)({IO_TIMEOUT});
    }
  });

  callbacks_.emplace_back([this, cob_ptr, wrapped_cob, ticket](const Result& result) mutable {
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
  std::unique_lock lock(channelMutex_);
  if (callbacks_.empty()) {
    inbox_.push_back(std::move(msg));
    return;
  }

  auto cob = std::move(callbacks_.front());
  callbacks_.erase(callbacks_.begin());
  lock.unlock();
  tp_->submit([cob = std::move(cob), msg = std::move(msg)]() mutable {
    cob({OK, msg});
  });
}

MessagePtr EphemeralChannel::pickMessage() {
  thread_local std::mt19937 rng{std::random_device{}()};
  auto dist = std::uniform_int_distribution<size_t>(0, inbox_.size() - 1);
  const size_t idx = dist(rng);
  auto msg = inbox_[idx];
  inbox_.erase(idx + inbox_.begin());
  return msg;
}


}  // namespace getrafty::rpc::io