#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <fcntl.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstring>
#include <future>
#include <mutex>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

#include "event_watcher.hpp"

using namespace std::chrono_literals;
using namespace getrafty::io;
using ::testing::StrictMock;

namespace {

constexpr auto kEventDispatchTimeout = 500ms;
constexpr auto kIdleWait = 150ms;
constexpr size_t kMockCallbackBufferSize = 1024;

template <typename T>
T waitForFuture(std::future<T>& future) {
  const auto status = future.wait_for(kEventDispatchTimeout);
  EXPECT_EQ(status, std::future_status::ready);
  if (status != std::future_status::ready) {
    return T{};
  }
  return future.get();
}

void waitForFuture(std::future<void>& future) {
  const auto status = future.wait_for(kEventDispatchTimeout);
  EXPECT_EQ(status, std::future_status::ready);
  if (status == std::future_status::ready) {
    future.get();
  }
}


template <typename Predicate>
bool WaitUntil(std::chrono::milliseconds timeout, Predicate&& predicate) {
  const auto deadline = std::chrono::steady_clock::now() + timeout;
  while (std::chrono::steady_clock::now() < deadline) {
    if (predicate()) {
      return true;
    }
    std::this_thread::sleep_for(1ms);
  }
  return predicate();
}

int getPipeBufferSize(int fd) {
  const auto size = fcntl(fd, F_GETPIPE_SZ);
  if (size < 0) {
    throw std::runtime_error("failed to get pipe buffer size");
  }
  return size;
}

}  // namespace

using Pipe = getrafty::io::detail::Pipe;

class ReadMockCallback {
 public:
  MOCK_METHOD(void, onReadReadyMock, (int fd, const std::string& data));

  WatchCallback makeCallback(int fd) {
    return [this, fd]() {
      std::array<char, kMockCallbackBufferSize> buffer{};
      const auto bytes_read = ::read(fd, buffer.data(), buffer.size());
      if (bytes_read > 0) {
        onReadReadyMock(fd, std::string(buffer.data(), bytes_read));
      }
    };
  }
};

class WriteMockCallback {
 public:
  MOCK_METHOD(void, onWriteReadyMock, (int fd, const std::string& data));

  WatchCallback makeCallback(int fd) {
    return [this, fd]() {
      const auto payload = getLastWritten();
      if (payload.empty()) {
        return;
      }
      const auto bytes_written = ::write(fd, payload.data(), payload.size());
      if (bytes_written > 0) {
        onWriteReadyMock(fd, payload);
      }
    };
  }

  void setLastWritten(std::string value) {
    const std::lock_guard lock{mutex_};
    last_written_ = std::move(value);
  }

 private:
  [[nodiscard]] std::string getLastWritten() const {
    const std::lock_guard lock{mutex_};
    return last_written_;
  }

  mutable std::mutex mutex_;
  std::string last_written_;
};

class EventWatcherTest : public ::testing::Test {
 protected:
  EventWatcher watcher{};
  void TearDown() override { watcher.unwatchAll(); }

  static ssize_t writeToPipe(int fd, std::string_view data) {
    return ::write(fd, data.data(), data.size());
  }

  static void assertFullWrite(int fd, std::string_view data) {
    ASSERT_EQ(writeToPipe(fd, data), static_cast<ssize_t>(data.size()));
  }
};

TEST_F(EventWatcherTest, ReadCallbackCalledWhenReady) {
  Pipe pipe;
  auto mock_callback = std::make_shared<StrictMock<ReadMockCallback>>();
  const std::string test_data = "Test Data";

  std::promise<std::string> promise;
  auto future = promise.get_future();

  EXPECT_CALL(*mock_callback, onReadReadyMock(pipe.read_end_, test_data))
      .WillOnce([&promise](int, const std::string& payload) {
        promise.set_value(payload);
      });

  watcher.watch(pipe.read_end_, RDONLY, mock_callback->makeCallback(pipe.read_end_));
  assertFullWrite(pipe.write_end_, test_data);

  EXPECT_EQ(waitForFuture(future), test_data);
  watcher.unwatch(pipe.read_end_, RDONLY);
}

TEST_F(EventWatcherTest, WriteCallbackNotCalledWhenBufferFull) {
  Pipe pipe;

  const auto buffer_size = getPipeBufferSize(pipe.write_end_);
  const std::string fill_data(buffer_size, 'x');
  assertFullWrite(pipe.write_end_, fill_data);

  auto mock_callback = std::make_shared<StrictMock<WriteMockCallback>>();
  mock_callback->setLastWritten("y");

  EXPECT_CALL(*mock_callback, onWriteReadyMock).Times(0);

  watcher.watch(pipe.write_end_, WRONLY, mock_callback->makeCallback(pipe.write_end_));
  std::this_thread::sleep_for(kIdleWait);
  watcher.unwatch(pipe.write_end_, WRONLY);
}

TEST_F(EventWatcherTest, WriteCallbackCalledOnceWhenBufferHasCapacity) {
  Pipe pipe;

  const auto buffer_size = getPipeBufferSize(pipe.write_end_);
  const std::string fill_data(buffer_size, 'x');

  auto mock_callback = std::make_shared<StrictMock<WriteMockCallback>>();
  mock_callback->setLastWritten(fill_data);

  std::promise<std::string> promise;
  auto future = promise.get_future();

  EXPECT_CALL(*mock_callback, onWriteReadyMock(pipe.write_end_, fill_data))
      .WillOnce([&](int, const std::string& payload) {
        promise.set_value(payload);
        mock_callback->setLastWritten("");
      });

  watcher.watch(pipe.write_end_, WRONLY, mock_callback->makeCallback(pipe.write_end_));
  EXPECT_EQ(waitForFuture(future), fill_data);

  std::this_thread::sleep_for(kIdleWait);
  watcher.unwatch(pipe.write_end_, WRONLY);

  std::vector<char> buffer(buffer_size);
  ASSERT_EQ(::read(pipe.read_end_, buffer.data(), buffer.size()), buffer_size);
  EXPECT_EQ(std::string(buffer.data(), buffer_size), fill_data);
}

TEST_F(EventWatcherTest, NoCallbackIfFdNotReadyForRead) {
  Pipe pipe;
  auto mock_callback = std::make_shared<StrictMock<ReadMockCallback>>();

  EXPECT_CALL(*mock_callback, onReadReadyMock).Times(0);

  watcher.watch(pipe.read_end_, RDONLY, mock_callback->makeCallback(pipe.read_end_));
  std::this_thread::sleep_for(kIdleWait);
  watcher.unwatch(pipe.read_end_, RDONLY);
}

TEST_F(EventWatcherTest, DuplicateWatchRequests) {
  Pipe pipe;
  auto mock_callback = std::make_shared<StrictMock<ReadMockCallback>>();
  const std::string test_data = "Test Data";

  std::promise<void> promise;
  auto future = promise.get_future();

  EXPECT_CALL(*mock_callback, onReadReadyMock(pipe.read_end_, ::testing::_))
      .WillOnce([&promise](int, const std::string&) { promise.set_value(); });

  watcher.watch(pipe.read_end_, RDONLY, mock_callback->makeCallback(pipe.read_end_));
  watcher.watch(pipe.read_end_, RDONLY, mock_callback->makeCallback(pipe.read_end_));

  assertFullWrite(pipe.write_end_, test_data);

  waitForFuture(future);
  watcher.unwatch(pipe.read_end_, RDONLY);
}

TEST_F(EventWatcherTest, NoCallbackAfterUnwatch) {
  Pipe pipe;
  auto mock_callback = std::make_shared<StrictMock<ReadMockCallback>>();

  EXPECT_CALL(*mock_callback, onReadReadyMock).Times(0);

  watcher.watch(pipe.read_end_, RDONLY, mock_callback->makeCallback(pipe.read_end_));
  watcher.unwatch(pipe.read_end_, RDONLY);

  const std::string test_data = "Test Data";
  assertFullWrite(pipe.write_end_, test_data);

  std::this_thread::sleep_for(kIdleWait);
}

TEST_F(EventWatcherTest, RetryOnEINTR) {
  int eintr_count = 0;
  int success_count = 0;
  constexpr auto kNumWrites = 5;
  std::atomic<size_t> callback_count{0};

  auto alternating_epoll_mock = [&](int epfd, epoll_event* events,
                                    int maxevents, int timeout) -> int {
    if ((eintr_count + success_count) % 2) {
      errno = EINTR;
      ++eintr_count;
      return -1;
    }
    ++success_count;
    return ::epoll_wait(epfd, events, maxevents, timeout);
  };

  EventWatcher watcher{alternating_epoll_mock};
  Pipe pipe;

  auto mock_callback = std::make_shared<StrictMock<ReadMockCallback>>();
  std::mutex mtx;
  std::condition_variable cv;
  bool ready_for_next_write = true;

  EXPECT_CALL(*mock_callback, onReadReadyMock(pipe.read_end_, ::testing::_))
      .WillRepeatedly([&](int, const std::string&) {
        ++callback_count;
        {
          const std::lock_guard lock{mtx};
          ready_for_next_write = true;
        }
        cv.notify_one();
      });

  watcher.watch(pipe.read_end_, RDONLY, mock_callback->makeCallback(pipe.read_end_));

  auto writeWhenReady = [&](std::string_view data) {
    std::unique_lock lock{mtx};
    cv.wait(lock, [&] { return ready_for_next_write; });
    ready_for_next_write = false;
    assertFullWrite(pipe.write_end_, data);
  };

  for (int i = 0; i < kNumWrites; ++i) {
    writeWhenReady("Test Data");
  }

  const auto all_callbacks_observed = WaitUntil(1000ms, [&] {
    return callback_count.load() == kNumWrites;
  });

  EXPECT_TRUE(all_callbacks_observed);
  EXPECT_GE(eintr_count, 1);
  EXPECT_GE(success_count, 1);
  EXPECT_EQ(callback_count.load(), kNumWrites);

  watcher.unwatch(pipe.read_end_, RDONLY);
}

TEST_F(EventWatcherTest, EpollBlocksWithNoWatchers) {
  std::atomic<uint64_t> epoll_wait_call_count{0};

  auto epoll_mock = [&](int, epoll_event*, int, int) -> int {
    ++epoll_wait_call_count;
    std::this_thread::sleep_for(200ms);
    return 0;
  };

  EventWatcher watcher{epoll_mock};

  const auto waited_at_least_once = WaitUntil(1s, [&] {
    return epoll_wait_call_count >= 3;
  });

  EXPECT_TRUE(waited_at_least_once);
  EXPECT_LT(epoll_wait_call_count, 10);
}

TEST_F(EventWatcherTest, SimultaneousReadWriteCallbacksOnSameFd) {
  Pipe pipe;
  auto read_callback = std::make_shared<StrictMock<ReadMockCallback>>();
  auto write_callback = std::make_shared<StrictMock<WriteMockCallback>>();

  const std::string test_data = "Test Data";
  write_callback->setLastWritten(test_data);

  std::promise<void> read_promise;
  auto read_future = read_promise.get_future();

  std::promise<void> write_promise;
  auto write_future = write_promise.get_future();

  EXPECT_CALL(*read_callback, onReadReadyMock(pipe.read_end_, test_data))
      .WillOnce([&](int, const std::string&) {
        read_promise.set_value();
      })
      .WillRepeatedly(::testing::Return());

  EXPECT_CALL(*write_callback, onWriteReadyMock(pipe.write_end_, test_data))
      .WillOnce([&](int, const std::string&) {
        write_promise.set_value();
        write_callback->setLastWritten("");
      })
      .WillRepeatedly(::testing::Return());

  watcher.watch(pipe.read_end_, RDONLY, read_callback->makeCallback(pipe.read_end_));
  watcher.watch(pipe.write_end_, WRONLY, write_callback->makeCallback(pipe.write_end_));

  waitForFuture(write_future);
  waitForFuture(read_future);

  watcher.unwatch(pipe.read_end_, RDONLY);
  watcher.unwatch(pipe.write_end_, WRONLY);
}

TEST_F(EventWatcherTest, UnwatchOneFlagKeepsTheOther) {
  std::array<int, 2> socketpair_fds{-1, -1};
  ASSERT_EQ(::socketpair(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK, 0,
                         socketpair_fds.data()), 0);

  const int fd = socketpair_fds[0];
  auto read_callback = std::make_shared<StrictMock<ReadMockCallback>>();
  auto write_callback = std::make_shared<StrictMock<WriteMockCallback>>();

  std::promise<void> write_promise;
  auto write_future = write_promise.get_future();

  const std::string test_data = "Test";

  EXPECT_CALL(*write_callback, onWriteReadyMock(fd, test_data))
      .WillOnce([&](int, const std::string&) {
        write_promise.set_value();
        write_callback->setLastWritten("");
      })
      .WillRepeatedly(::testing::Return());

  EXPECT_CALL(*read_callback, onReadReadyMock).Times(0);

  write_callback->setLastWritten(test_data);

  watcher.watch(fd, RDONLY, read_callback->makeCallback(fd));
  watcher.watch(fd, WRONLY, write_callback->makeCallback(fd));

  watcher.unwatch(fd, RDONLY);

  waitForFuture(write_future);

  watcher.unwatch(fd, WRONLY);
  ::close(socketpair_fds[0]);
  ::close(socketpair_fds[1]);
}

TEST_F(EventWatcherTest, RapidWatchUnwatchCycles) {
  Pipe pipe;
  auto mock_callback = std::make_shared<StrictMock<ReadMockCallback>>();

  EXPECT_CALL(*mock_callback, onReadReadyMock).Times(0);

  for (int i = 0; i < 100; ++i) {
    watcher.watch(pipe.read_end_, RDONLY, mock_callback->makeCallback(pipe.read_end_));
    watcher.unwatch(pipe.read_end_, RDONLY);
  }

  std::promise<void> promise;
  auto future = promise.get_future();

  auto final_callback = std::make_shared<StrictMock<ReadMockCallback>>();
  EXPECT_CALL(*final_callback, onReadReadyMock(pipe.read_end_, ::testing::_))
      .WillOnce([&](int, const std::string&) {
        promise.set_value();
      });

  watcher.watch(pipe.read_end_, RDONLY, final_callback->makeCallback(pipe.read_end_));
  assertFullWrite(pipe.write_end_, "Test");
  waitForFuture(future);

  watcher.unwatch(pipe.read_end_, RDONLY);
}

TEST_F(EventWatcherTest, EventLoopDoesNotBusyWait) {
  std::atomic<int> epoll_wait_count{0};
  std::atomic<bool> stop_counting{false};

  auto counting_epoll = [&](int epfd, epoll_event* events,
                             int maxevents, int timeout) -> int {
    if (!stop_counting.load()) {
      ++epoll_wait_count;
    }
    return ::epoll_wait(epfd, events, maxevents, timeout);
  };

  EventWatcher watcher{counting_epoll};
  Pipe pipe;
  auto mock_callback = std::make_shared<StrictMock<ReadMockCallback>>();

  EXPECT_CALL(*mock_callback, onReadReadyMock(pipe.read_end_, ::testing::_))
      .Times(::testing::AtLeast(1));

  watcher.watch(pipe.read_end_, RDONLY, mock_callback->makeCallback(pipe.read_end_));

  const auto initial_count = epoll_wait_count.load();
  std::this_thread::sleep_for(500ms);
  const auto idle_count = epoll_wait_count.load() - initial_count;

  for (int i = 0; i < 10; ++i) {
    assertFullWrite(pipe.write_end_, "x");
    std::this_thread::sleep_for(10ms);
  }

  std::this_thread::sleep_for(100ms);
  stop_counting.store(true);
  const auto final_count = epoll_wait_count.load();

  EXPECT_LT(idle_count, 5) << "Event loop appears to be busy-waiting";

  EXPECT_GT(final_count, initial_count);

  watcher.unwatch(pipe.read_end_, RDONLY);
}

TEST_F(EventWatcherTest, MultipleFdsWithMixedOperations) {
  std::vector<Pipe> pipes(5);
  std::vector<std::shared_ptr<ReadMockCallback>> read_callbacks;
  std::vector<std::shared_ptr<WriteMockCallback>> write_callbacks;
  std::vector<std::promise<void>> promises(10);
  std::vector<std::future<void>> futures;

  for (size_t i = 0; i < 5; ++i) {
    read_callbacks.push_back(std::make_shared<StrictMock<ReadMockCallback>>());
    write_callbacks.push_back(std::make_shared<StrictMock<WriteMockCallback>>());
    futures.push_back(promises[i].get_future());
    futures.push_back(promises[i + 5].get_future());
  }

  for (size_t i = 0; i < 5; ++i) {
    EXPECT_CALL(*read_callbacks[i],
                onReadReadyMock(pipes[i].read_end_, ::testing::_))
        .WillOnce([&, i](int, const std::string&) {
          promises[i].set_value();
        })
        .WillRepeatedly(::testing::Return());

    write_callbacks[i]->setLastWritten(std::to_string(i));
    EXPECT_CALL(*write_callbacks[i],
                onWriteReadyMock(pipes[i].write_end_, std::to_string(i)))
        .WillOnce([&, i](int, const std::string&) {
          promises[i + 5].set_value();
          write_callbacks[i]->setLastWritten("");
        })
        .WillRepeatedly(::testing::Return());

    watcher.watch(pipes[i].read_end_, RDONLY, read_callbacks[i]->makeCallback(pipes[i].read_end_));
    watcher.watch(pipes[i].write_end_, WRONLY, write_callbacks[i]->makeCallback(pipes[i].write_end_));
  }

  for (auto& future : futures) {
    waitForFuture(future);
  }

  for (size_t i = 0; i < 5; ++i) {
    watcher.unwatch(pipes[i].read_end_, RDONLY);
    watcher.unwatch(pipes[i].write_end_, WRONLY);
  }
}