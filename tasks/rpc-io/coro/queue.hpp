#pragma once

#include <folly/CancellationToken.h>
#include <folly/ScopeGuard.h>
#include <folly/experimental/coro/AsyncGenerator.h>
#include <folly/experimental/coro/Baton.h>
#include <folly/experimental/coro/Task.h>
#include <deque>
#include <mutex>

#include "folly/coro/Promise.h"

namespace getrafty::wheels::concurrent::coro {

/**
 * UnboundedBlockingQueue with coroutine-friendly take operation.
 */
template <typename T>
class UnboundedBlockingQueue {
 public:
  UnboundedBlockingQueue() = default;
  UnboundedBlockingQueue(const UnboundedBlockingQueue&) = delete;
  UnboundedBlockingQueue& operator=(const UnboundedBlockingQueue&) = delete;
  UnboundedBlockingQueue(UnboundedBlockingQueue&&) = delete;
  ~UnboundedBlockingQueue() = default;

  void put(T value) {
    std::unique_lock lock(mutex_);
    if (waiters_head_) {
      auto node = waiters_head_;
      removeWaiter(node);

      node->has_value = true;
      node->value = std::move(value);
      node->baton.post();
    } else {
      queue_.push_back(std::move(value));
    }
  }

  folly::coro::Task<T> take() {
    {
      std::unique_lock lock(mutex_);
      if (!queue_.empty()) {
        T front = std::move(queue_.front());
        queue_.pop_front();
        co_return front;
      }
    }

    Node node;
    {
      std::unique_lock lock(mutex_);
      if (!queue_.empty()) {
        T front = std::move(queue_.front());
        queue_.pop_front();
        co_return front;
      }
      pushBackWaiter(&node);
    }

    // Handle cancellation via a callback that removes us from waiters if invoked
    auto ct = co_await folly::coro::co_current_cancellation_token;
    folly::CancellationCallback cancel_cb(
        ct,
        [this, &node]() noexcept {
          std::unique_lock lock(mutex_);
          if (!node.canceled && (node.prev || node.next || waiters_head_ == &node)) {
            removeWaiter(&node);
            node.canceled = true;
            node.baton.post();
          }
        });

    co_await node.baton;

    if (node.canceled) {
      throw folly::OperationCancelled();
    }

    co_return std::move(node.value);
  }

 private:
  struct Node {
    Node* prev{nullptr};
    Node* next{nullptr};
    folly::coro::Baton baton;
    bool canceled{false};
    bool has_value{false};
    T value;
  };

  void pushBackWaiter(Node* node) {
    node->prev = waiters_tail_;
    node->next = nullptr;
    if (waiters_tail_) {
      waiters_tail_->next = node;
    } else {
      waiters_head_ = node;
    }
    waiters_tail_ = node;
  }

  void removeWaiter(Node* node) {
    Node* p = node->prev;
    Node* n = node->next;
    if (p) {
      p->next = n;
    } else {
      waiters_head_ = n;
    }
    if (n) {
      n->prev = p;
    } else {
      waiters_tail_ = p;
    }
    node->prev = nullptr;
    node->next = nullptr;
  }

  std::mutex mutex_;
  std::deque<T> queue_;

  Node* waiters_head_{nullptr};
  Node* waiters_tail_{nullptr};
};

} // namespace getrafty::wheels::concurrent::coro
