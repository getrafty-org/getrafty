#include "ephemeral_channel.hpp"

#include "folly/Singleton.h"

namespace getrafty::rpc::io {

EphemeralChannel::EphemeralChannel(const uint16_t address, std::shared_ptr<ThreadPool> pool)
    : address_(address), tp_(std::move(pool)) {}

EphemeralChannel::~EphemeralChannel() {
  detachChannel();
}

void EphemeralChannel::sendMessage(AsyncCallback&& cob, MessagePtr message,
                                   std::chrono::milliseconds) {
  const auto peer = findPeer();
  if (!peer) {
    cob({SOCK_CLOSED, nullptr});
    // scheduleCallback(std::move([cob = std::move(cob)]() mutable {
    //   cob({SOCK_CLOSED, nullptr});
    // }));
    return;
  }

  cob({OK, nullptr});
  // scheduleCallback(std::move([cob = std::move(cob)]() mutable {
  //   cob({OK, nullptr});
  // }));

  peer->deliver(std::move(message));
}

void EphemeralChannel::recvMessage(AsyncCallback&& cob,
                                   std::chrono::milliseconds) {
  std::unique_lock lock(channelMutex_);
  if (!inbox_.empty()) {
    const auto msg = pickMessage();
    lock.unlock();
    cob({OK, msg});
    // scheduleCallback(std::move([msg = std::move(msg), cob = std::move(cob)]() mutable {
    //       cob({OK, msg});
    // }));
    return;
  }

  callbacks_.push_back(std::move(cob));
}
std::shared_ptr<IAsyncChannel> EphemeralChannel::create(
    const uint16_t address, const std::shared_ptr<ThreadPool>& pool) {
  auto channel = std::shared_ptr<EphemeralChannel>(new EphemeralChannel(address, pool));
  channel->attachChannel();
  return channel;
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
  auto& pair = it->second;
  auto selfPtr = this;

  auto f = pair.first.lock();
  if (f.get() == selfPtr) {
    pair.first.reset();
  }
  auto s = pair.second.lock();
  if (s.get() == selfPtr) {
    pair.second.reset();
  }
  if (pair.first.expired() && pair.second.expired()) {
    registry_.erase(it);
  }
}
std::shared_ptr<EphemeralChannel> EphemeralChannel::findPeer() const {
  std::lock_guard<std::mutex> g(registryMutex_);
  auto it = registry_.find(address_);
  if (it == registry_.end()) {
    return nullptr;
  }
  auto& pair = it->second;
  auto selfRaw = this;
  auto f = pair.first.lock();
  auto s = pair.second.lock();
  if (f.get() == selfRaw) {
    return s;
  } else if (s.get() == selfRaw) {
    return f;
  }
  return nullptr;
}

void EphemeralChannel::deliver(MessagePtr msg) {
  std::unique_lock lock(channelMutex_);
  if (!callbacks_.empty()) {
    auto cob = std::move(callbacks_.front());
    callbacks_.erase(callbacks_.begin());
    lock.unlock();
    cob({OK, msg});
    // scheduleCallback([cob = std::move(cob), msg = std::move(msg)]() mutable {
    //     cob({OK, msg});
    // });
    return;
  }

  inbox_.push_back(std::move(msg));
}

MessagePtr EphemeralChannel::pickMessage() {
  thread_local std::mt19937 rng{std::random_device{}()};
  auto dist = std::uniform_int_distribution<size_t>(0, inbox_.size() - 1);
  const size_t idx = dist(rng);
  auto msg = inbox_[idx];
  inbox_.erase(idx + inbox_.begin());
  return msg;
}

void EphemeralChannel::scheduleCallback(std::function<void()>&& fn) const {
  if (!tp_->submit(std::move(fn))) {
    // If the queue is full or the pool is stopping, callback is lost
    // (in a real implementation you might want to handle this gracefully)
  }
}

}  // namespace getrafty::rpc::io