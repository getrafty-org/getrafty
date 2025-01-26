#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <fcntl.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <atomic>
#include <barrier>
#include <chrono>
#include <condition_variable>
#include <latch>
#include <string>
#include <thread>
#include <vector>

#include "event_watcher.hpp"

using namespace std::chrono_literals;
using namespace getrafty::rpc::io;
using ::testing::AtLeast;
using ::testing::StrictMock;

constexpr auto kWarmupDuration = 100ms;
constexpr auto kCallbackRepeatDuration = 500ms;

class NoOpMockCallback /* NOLINT */ : public IWatchCallback {
 public:
  MOCK_METHOD(void, onReadReadyMock /* NOLINT */, (int fd));
  MOCK_METHOD(void, onWriteReadyMock /* NOLINT */, (int fd));

  void onReadReady(const int fd) override { onReadReadyMock(fd); }

  void onWriteReady(const int fd) override { onWriteReadyMock(fd); }
};

class MockCallback /* NOLINT */ : public IWatchCallback {
 public:
  MOCK_METHOD(void, onReadReadyMock /* NOLINT */,
              (int fd, const std::string& data));
  MOCK_METHOD(void, onWriteReadyMock /* NOLINT */,
              (int fd, const std::string& data));

  void onReadReady(const int fd) override {
    char buffer[1024];
    if (const ssize_t bytesRead = read(fd, buffer, sizeof(buffer));
        bytesRead > 0) {
      lastRead = std::string(buffer, bytesRead);
      onReadReadyMock(fd, lastRead);
    }
  }

  void onWriteReady(const int fd) override {
    if (const ssize_t bytesWritten =
            write(fd, lastWritten.c_str(), lastWritten.size());
        bytesWritten > 0) {
      onWriteReadyMock(fd, lastWritten);
    }
  }

  std::string lastRead;
  std::string lastWritten;
};

class EventWatcherTest : public ::testing::Test {
 protected:
  EventWatcher watcher{};
  void TearDown() override { watcher.unwatchAll(); }

  static void setupPipe(int pipe_fd[2]) { ASSERT_EQ(pipe(pipe_fd), 0); }

  static int getPipeBufferSize(const int fd) {
    const int buffer_size = fcntl(fd, F_GETPIPE_SZ);
    EXPECT_GE(buffer_size, 0);
    return buffer_size;
  }
};

TEST_F(EventWatcherTest, ReadCallbackCalledWhenReady) {
  int test_fd[2];
  setupPipe(test_fd);

  StrictMock<MockCallback> mock_callback;
  const std::string test_data = "Test Data";

  EXPECT_CALL(mock_callback, onReadReadyMock(test_fd[0], test_data)).Times(1);

  watcher.watch(test_fd[0], CB_RDONLY, &mock_callback);
  write(test_fd[1], test_data.c_str(), test_data.size());

  std::this_thread::sleep_for(kWarmupDuration);

  EXPECT_EQ(mock_callback.lastRead, test_data);

  watcher.unwatch(test_fd[0], CB_RDONLY);
  close(test_fd[0]);
  close(test_fd[1]);
}

TEST_F(EventWatcherTest, WriteCallbackNotCalledWhenBufferFull) {
  int test_fd[2];
  setupPipe(test_fd);

  const int buffer_size = getPipeBufferSize(test_fd[1]);
  const std::string buffer_fill(buffer_size, 'x');

  ASSERT_EQ(write(test_fd[1], buffer_fill.c_str(), buffer_fill.size()),
            buffer_size);

  StrictMock<MockCallback> mock_callback;
  mock_callback.lastWritten = "y";

  EXPECT_CALL(mock_callback, onWriteReadyMock(test_fd[1], ::testing::_))
      .Times(0);

  watcher.watch(test_fd[1], CB_WRONLY, &mock_callback);

  std::this_thread::sleep_for(kWarmupDuration);

  watcher.unwatch(test_fd[1], CB_WRONLY);
  close(test_fd[0]);
  close(test_fd[1]);
}

TEST_F(EventWatcherTest, WriteCallbackCalledOnceWhenBufferHasCapacity) {
  int pipe_fd[2];
  setupPipe(pipe_fd);

  int buffer_size = getPipeBufferSize(pipe_fd[1]);
  buffer_size += buffer_size % 2;

  std::string first_half(buffer_size / 2, 'x');
  std::string second_half(buffer_size / 2, 'y');

  ASSERT_EQ(write(pipe_fd[1], first_half.c_str(), first_half.size()),
            first_half.size());

  MockCallback mock_callback;
  mock_callback.lastWritten = second_half;

  EXPECT_CALL(mock_callback, onWriteReadyMock(pipe_fd[1], ::testing::_))
      .Times(1);

  watcher.watch(pipe_fd[1], CB_WRONLY, &mock_callback);

  std::this_thread::sleep_for(kWarmupDuration);

  watcher.unwatch(pipe_fd[1], CB_WRONLY);

  std::vector<char> buffer(buffer_size);
  ASSERT_EQ(read(pipe_fd[0], buffer.data(), buffer.size()), buffer_size);

  EXPECT_EQ(std::string(buffer.begin(), buffer.begin() + buffer_size / 2),
            first_half);
  EXPECT_EQ(std::string(buffer.begin() + buffer_size / 2, buffer.end()),
            second_half);

  close(pipe_fd[0]);
  close(pipe_fd[1]);
}

TEST_F(EventWatcherTest, NoCallbackIfFdNotReadyForRead) {
  int test_fd[2];
  setupPipe(test_fd);

  StrictMock<MockCallback> mock_callback;

  EXPECT_CALL(mock_callback, onReadReadyMock(test_fd[0], ::testing::_))
      .Times(0);

  watcher.watch(test_fd[0], CB_RDONLY, &mock_callback);

  std::this_thread::sleep_for(kWarmupDuration);

  watcher.unwatch(test_fd[0], CB_RDONLY);
  close(test_fd[0]);
  close(test_fd[1]);
}

TEST_F(EventWatcherTest, DuplicateWatchRequests) {
  int test_fd[2];
  setupPipe(test_fd);

  StrictMock<MockCallback> mock_callback;
  const std::string test_data = "Test Data";

  EXPECT_CALL(mock_callback, onReadReadyMock(test_fd[0], ::testing::_))
      .Times(1);

  watcher.watch(test_fd[0], CB_RDONLY, &mock_callback);
  watcher.watch(test_fd[0], CB_RDONLY, &mock_callback);

  write(test_fd[1], test_data.c_str(), test_data.size());

  std::this_thread::sleep_for(kWarmupDuration);

  watcher.unwatch(test_fd[0], CB_RDONLY);

  close(test_fd[0]);
  close(test_fd[1]);
}

TEST_F(EventWatcherTest, NoCallbackAfterUnwatch) {
  int test_fd[2];
  setupPipe(test_fd);

  StrictMock<MockCallback> mock_callback;

  EXPECT_CALL(mock_callback, onReadReadyMock(test_fd[0], ::testing::_))
      .Times(0);

  watcher.watch(test_fd[0], CB_RDONLY, &mock_callback);
  watcher.unwatch(test_fd[0], CB_RDONLY);

  const std::string test_data = "Test Data";
  write(test_fd[1], test_data.c_str(), test_data.size());

  std::this_thread::sleep_for(kWarmupDuration);

  close(test_fd[0]);
  close(test_fd[1]);
}

TEST_F(EventWatcherTest, RetryOnEINTR) {
  int eintr_count = 0;
  int success_count = 0;
  constexpr auto kNumWrites = 5;
  std::atomic<size_t> callback_count{0};

  auto alternating_epoll_mock = [&](int epfd, epoll_event* events,
                                    int maxevents, int timeout) {
    if ((eintr_count + success_count) % 2) {
      errno = EINTR;
      ++eintr_count;
      return -1;
    } else {
      ++success_count;
      return ::epoll_wait(epfd, events, maxevents, timeout);
    }
  };

  EventWatcher watcher{alternating_epoll_mock};
  int test_fd[2];
  setupPipe(test_fd);

  StrictMock<MockCallback> mock_callback;
  std::mutex mtx;
  std::condition_variable cv;
  bool ready_for_next_write = true;

  EXPECT_CALL(mock_callback, onReadReadyMock(test_fd[0], ::testing::_))
      .WillRepeatedly([&](int, const std::string&) {
        ++callback_count;
        std::lock_guard<std::mutex> lock(mtx);
        ready_for_next_write = true;
        cv.notify_one();
      });

  watcher.watch(test_fd[0], CB_RDONLY, &mock_callback);

  auto doWrite = [&](const char* data) {
    std::unique_lock<std::mutex> lock(mtx);
    cv.wait(lock, [&] { return ready_for_next_write; });
    ready_for_next_write = false;
    write(test_fd[1], data, strlen(data));
  };

  for (auto i = 0; i < kNumWrites; ++i) {
    doWrite("Test Data");
  }

  std::this_thread::sleep_for(kCallbackRepeatDuration);

  EXPECT_GE(eintr_count, 1);
  EXPECT_GE(success_count, 1);
  EXPECT_EQ(callback_count.load(), kNumWrites);

  watcher.unwatch(test_fd[0], CB_RDONLY);
  close(test_fd[0]);
  close(test_fd[1]);
}

TEST_F(EventWatcherTest, EpollBlocksWithNoWatchers) {
  std::atomic<uint64_t> epoll_wait_call_count = 0;

  auto epoll_mock = [&](int, epoll_event*, int, int) {
    ++epoll_wait_call_count;
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    return 0;
  };

  EventWatcher watcher{epoll_mock};

  std::this_thread::sleep_for(std::chrono::seconds(1));

  EXPECT_LT(epoll_wait_call_count, 10);
}

TEST_F(EventWatcherTest, ManyWatchers) {
  constexpr int kNumWatchers = 1000;
  int pipe_fd[kNumWatchers][2];
  std::vector<MockCallback> callbacks(kNumWatchers);
  std::latch latch{kNumWatchers};

  // Setup pipes and watchers
  for (int i = 0; i < kNumWatchers; ++i) {
    setupPipe(pipe_fd[i]);
    watcher.watch(pipe_fd[i][0], CB_RDONLY, &callbacks[i]);
    EXPECT_CALL(callbacks[i], onReadReadyMock(pipe_fd[i][0], testing::_))
        .WillOnce([&latch] { latch.count_down(); });
  }

  // Barrier for synchronizing threads
  std::barrier sync{kNumWatchers};
  std::vector<std::thread> threads(kNumWatchers);

  // Start threads to write to the pipes
  for (int i = 0; i < kNumWatchers; ++i) {
    threads[i] = std::thread([i, &pipe_fd, &sync] {
        sync.arrive_and_wait();
        constexpr char trigger_data = 'x';
        ASSERT_EQ(write(pipe_fd[i][1], &trigger_data, sizeof(trigger_data)), 1);
    });
  }

  // Join all threads
  for (auto& thread : threads) {
    thread.join();
  }

  // Wait for all callbacks to be invoked
  latch.wait();

  // Cleanup
  for (auto & [i,j] : pipe_fd) {
    watcher.unwatch(i, CB_RDONLY);
    for (const int fd : {i, j}) {
      close(fd);
    }
  }
}

