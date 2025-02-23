#pragma once

#include <folly/coro/Task.h>
#include <folly/coro/Promise.h>
#include <folly/coro/Timeout.h>
#include <folly/Try.h>


namespace getrafty::rpc {


template <typename T, typename CallbackFunc>
folly::coro::Task<T> awaitCallback(CallbackFunc&& fun) {
  auto [promise, future] = folly::coro::makePromiseContract<T>();
  fun([&](T result) mutable {
    promise.setValue(std::move(result));
  });

  co_return co_await std::move(future);
}


}  // namespace getrafty::rpc