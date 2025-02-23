#include "client.hpp"

#include <folly/coro/Promise.h>
#include <folly/futures/Future.h>
#include "folly/coro/BlockingWait.h"
#include "folly/coro/Collect.h"

#include <utility>
#include "channel.hpp"

namespace getrafty::rpc {

Client::Client(std::shared_ptr<io::IAsyncChannel> channel)
    : channel_(std::move(channel)) {}

Client::~Client() = default;

bool Client::Inflight::setException(const RpcError& rpc_error) {
  if(!fulfilled_.exchange(true)) {
    promise.setException(rpc_error);
    return true;
  }
  return false;
}

bool Client::Inflight::setValue(const io::MessagePtr& message) {
  if(!fulfilled_.exchange(true)) {
    promise.setValue(message);
    return true;
  }
  return false;
}

std::shared_ptr<Client::Inflight> Client::peekInflight(uint64_t xid) {
  std::shared_ptr<Inflight> result;
  inflight_requests_.withWLock([xid, &result](auto& lock) {
    auto it = lock.find(xid);
    if (it != lock.end()) {
      result = it->second;
    }
  });
  return result;

}

std::pair<uint64_t, folly::Future<std::shared_ptr<io::IMessage>>>
Client::pushInflight() {
  auto promise = folly::Promise<io::MessagePtr>();
  auto future = promise.getFuture();

  const auto xid = next_xid_++;
  inflight_requests_.withWLock([xid, &promise](auto& lock) {
    lock[xid] = std::make_shared<Inflight>(std::move(promise));
  });

  return std::make_pair(xid, std::move(future));
}

std::shared_ptr<Client::Inflight> Client::popInflight(uint64_t xid) {
  std::shared_ptr<Inflight> result;
  inflight_requests_.withWLock([xid, &result](auto& lock) {
    auto it = lock.find(xid);
    if (it != lock.end()) {
      result = std::move(it->second);
      lock.erase(it);
    }
  });
  return result;
}

}  // namespace getrafty::rpc
