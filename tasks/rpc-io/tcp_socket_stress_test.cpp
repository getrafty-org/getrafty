#include "tcp_socket.hpp"
#include "event_watcher.hpp"
#include <gtest/gtest.h>
#include <latch>
#include <memory>

using namespace getrafty::rpc::io;

class TcpSocketStressTest : public ::testing::Test {
 protected:
  void SetUp() override {
    watcher_ = std::make_unique<EventWatcher>();
  }

  void TearDown() override {
    watcher_.reset();
  }

  std::unique_ptr<EventWatcher> watcher_;
};

TEST_F(TcpSocketStressTest, SocketDestructionCancelsOperations) {
  constexpr int kIterations = 10000;

  for (int iteration = 0; iteration < kIterations; ++iteration) {
    auto server = TcpServerSocket::listen("127.0.0.1", 9989, *watcher_);
    ASSERT_NE(server, nullptr);

    auto client = TcpSocket::connect("127.0.0.1", 9989, *watcher_);
    auto server_conn = server->accept();

    std::latch read_done(1);
    std::latch write_done(1);
    Status read_status = Status::OK;
    Status write_status = Status::OK;

    client->asyncRead(100, [&](Status s, Buffer&&) {
      read_status = s;
      read_done.count_down();
    });

    Buffer large_data(1024 * 1024);
    client->asyncWrite(std::move(large_data), [&](Status s) {
      write_status = s;
      write_done.count_down();
    });

    client->close();

    read_done.wait();
    write_done.wait();

    ASSERT_EQ(read_status, Status::CLOSED) << "Failed on iteration " << iteration;
    ASSERT_EQ(write_status, Status::CLOSED) << "Failed on iteration " << iteration;

    server_conn->close();
    server->close();
  }
}
