#include <gtest/gtest.h>
#include <latch>
#include <memory>
#include "event_watcher.hpp"
#include "framed_transport.hpp"
#include "socket.hpp"
#include "tcp_transport.hpp"
#include "transport.hpp"

using namespace getrafty::io;
using namespace getrafty::rpc;

namespace {

using SocketPtr = std::shared_ptr<Socket>;

SocketPtr makeSocket(const Address& address, EventWatcher& watcher) {
  auto transport = std::make_unique<FramedTransport>(
      std::make_unique<TcpTransport>(address));
  return std::make_shared<Socket>(watcher, std::move(transport));
}

}  // namespace

class BaseSocketStressTest : public ::testing::Test {
 protected:
  void SetUp() override { watcher_ = std::make_unique<EventWatcher>(); }

  void TearDown() override { watcher_.reset(); }

  std::unique_ptr<EventWatcher> watcher_;
};

TEST_F(BaseSocketStressTest, PingPong) {
  constexpr int kIterations = 1000;

  for (int iteration = 0; iteration < kIterations; ++iteration) {
    std::latch bind_done(1);
    std::latch connect_done(1);
    std::latch read_done(1);
    std::latch write_done(1);

    IOStatus bind_status    = IOStatus::Fatal;
    IOStatus connect_status = IOStatus::Fatal;
    IOStatus read_status    = IOStatus::Ok;
    IOStatus write_status   = IOStatus::Ok;

    auto local_watcher = std::make_unique<EventWatcher>();
    auto server        = makeSocket("127.0.0.1:0", *local_watcher);
    std::string server_address;

    server->bind([&](IOStatus s, const Address& addr) {
      bind_status    = s;
      server_address = std::string(addr);
      bind_done.count_down();
    });

    bind_done.wait();
    ASSERT_EQ(bind_status, IOStatus::Ok) << "Failed on iteration " << iteration;

    auto client = makeSocket(server_address, *local_watcher);

    client->connect([&](IOStatus s) {
      connect_status = s;
      connect_done.count_down();
    });

    connect_done.wait();
    ASSERT_EQ(connect_status, IOStatus::Ok)
        << "Failed on iteration " << iteration;

    client->read([&](IOStatus s, Buffer&&, const Peer&) {
      read_status = s;
      read_done.count_down();
    });

    Buffer large_data(1024);
    client->write(std::move(large_data), {}, [&](IOStatus s) {
      write_status = s;
      write_done.count_down();
    });

    {
      std::latch close_done{1};
      client->close([&close_done]() { close_done.count_down(); });
      close_done.wait();
      client.reset();
    }
    {
      std::latch close_done{1};
      server->close([&close_done]() { close_done.count_down(); });
      close_done.wait();
      server.reset();
    }

    read_done.wait();
    write_done.wait();

    EXPECT_EQ(read_status, IOStatus::Error)
        << "Read should be canceled with CLOSED on iteration " << iteration;
    EXPECT_TRUE(write_status == IOStatus::Ok || write_status == IOStatus::Error)
        << "Write status was " << static_cast<int>(write_status)
        << " on iteration " << iteration;
  }
}
