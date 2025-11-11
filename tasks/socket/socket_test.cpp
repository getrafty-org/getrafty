#include "socket.hpp"
#include <arpa/inet.h>
#include <dirent.h>
#include <gtest/gtest.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <atomic>
#include <bits/ttl/ttl.hpp>
#include <chrono>
#include <cstdint>
#include <future>
#include <latch>
#include <string>
#include <thread>
#include <utility>
#include <vector>
#include "event_watcher.hpp"
#include "framed_transport.hpp"
#include "tcp_transport.hpp"
#include "transport.hpp"

using namespace getrafty::rpc;

namespace {

using SocketPtr = std::shared_ptr<getrafty::rpc::Socket>;

SocketPtr makeSocket(const Address& address,
                     getrafty::io::EventWatcher& watcher) {
  auto transport = std::make_unique<FramedTransport>(
      std::make_unique<TcpTransport>(address));
  return std::make_shared<getrafty::rpc::Socket>(watcher, std::move(transport));
}

}  // namespace

using namespace getrafty::io;
using namespace getrafty::rpc;

class MockEventWatcher : public EventWatcher {
 public:
  MockEventWatcher() = default;

  void watch(int fd, WatchFlag flag, WatchCallback callback) override {
    watch_count_.fetch_add(1);
    EventWatcher::watch(fd, flag, std::move(callback));
  }

  void unwatch(int fd, WatchFlag flag) override {
    unwatch_count_.fetch_add(1);
    EventWatcher::unwatch(fd, flag);
  }

  [[nodiscard]] int watch_count() const { return watch_count_.load(); }

  [[nodiscard]] int unwatch_count() const { return unwatch_count_.load(); }

  void reset_counts() {
    watch_count_.store(0);
    unwatch_count_.store(0);
  }

 private:
  std::atomic<int> watch_count_{0};
  std::atomic<int> unwatch_count_{0};
};

class BaseSocketTest : public ::testing::Test {
 protected:
  void SetUp() override {
    bits::ttl::Ttl::init("stdout://");
    watcher_ = std::make_unique<EventWatcher>();
  }

  void TearDown() override {
    watcher_.reset();
    bits::ttl::Ttl::shutdown();
  }

  std::unique_ptr<EventWatcher> watcher_;
};

TEST_F(BaseSocketTest, Bind) {
  auto sock        = makeSocket("127.0.0.1:0", *watcher_);
  auto bind_status = IOStatus::Fatal;
  std::string bind_addr;
  std::latch bind_done{1};
  sock->bind([&](IOStatus s, const Address& addr) {
    bind_status = s;
    bind_addr   = std::string(addr);
    bind_done.count_down();
  });

  bind_done.wait();
  ASSERT_EQ(bind_status, IOStatus::Ok);
  ASSERT_FALSE(bind_addr.empty());

  {
    std::latch close_done{1};
    sock->close([&close_done]() { close_done.count_down(); });
    close_done.wait();
  }
}

TEST_F(BaseSocketTest, Connect) {
  auto server      = makeSocket("127.0.0.1:0", *watcher_);
  auto bind_status = IOStatus::Fatal;
  std::string bind_addr;
  std::latch bind_done(1);
  server->bind([&](IOStatus s, const Address& addr) {
    bind_status = s;
    bind_addr   = std::string(addr);
    bind_done.count_down();
  });
  bind_done.wait();
  ASSERT_EQ(bind_status, IOStatus::Ok);
  ASSERT_FALSE(bind_addr.empty());

  auto client         = makeSocket(bind_addr, *watcher_);
  auto connect_status = IOStatus::Fatal;
  std::latch connect_done(1);
  client->connect([&](IOStatus s) {
    connect_status = s;
    connect_done.count_down();
  });
  connect_done.wait();
  ASSERT_EQ(connect_status, IOStatus::Ok);

  {
    std::latch close_done{1};
    client->close([&close_done]() { close_done.count_down(); });
    close_done.wait();
  }
  {
    std::latch close_done{1};
    server->close([&close_done]() { close_done.count_down(); });
    close_done.wait();
  }
}

TEST_F(BaseSocketTest, WriteBasic) {
  auto server      = makeSocket("127.0.0.1:0", *watcher_);
  auto bind_status = IOStatus::Fatal;
  std::string bind_addr;
  std::latch bind_done(1);
  server->bind([&](IOStatus s, const Address& addr) {
    bind_status = s;
    bind_addr   = std::string(addr);
    bind_done.count_down();
  });
  bind_done.wait();
  ASSERT_EQ(bind_status, IOStatus::Ok);

  auto client         = makeSocket(bind_addr, *watcher_);
  auto connect_status = IOStatus::Fatal;
  std::latch connect_done(1);
  client->connect([&](IOStatus s) {
    connect_status = s;
    connect_done.count_down();
  });
  connect_done.wait();
  ASSERT_EQ(connect_status, IOStatus::Ok);

  std::latch write_done(1);
  IOStatus write_status = IOStatus::Fatal;
  Buffer data         = {'h', 'e', 'l', 'l', 'o'};
  client->write(std::move(data), {}, [&](IOStatus s) {
    write_status = s;
    write_done.count_down();
  });
  write_done.wait();
  ASSERT_EQ(write_status, IOStatus::Ok);

  {
    std::latch close_done{1};
    client->close([&close_done]() { close_done.count_down(); });
    close_done.wait();
  }
  {
    std::latch close_done{1};
    server->close([&close_done]() { close_done.count_down(); });
    close_done.wait();
  }
}

TEST_F(BaseSocketTest, ReadBasic) {
  auto server      = makeSocket("127.0.0.1:0", *watcher_);
  auto bind_status = IOStatus::Fatal;
  std::string bind_addr;
  std::latch bind_done(1);
  server->bind([&](IOStatus s, const Address& addr) {
    bind_status = s;
    bind_addr   = std::string(addr);
    bind_done.count_down();
  });
  bind_done.wait();
  ASSERT_EQ(bind_status, IOStatus::Ok);

  auto client         = makeSocket(bind_addr, *watcher_);
  auto connect_status = IOStatus::Fatal;
  std::latch connect_done(1);
  client->connect([&](IOStatus s) {
    connect_status = s;
    connect_done.count_down();
  });
  connect_done.wait();
  ASSERT_EQ(connect_status, IOStatus::Ok);

  std::latch read_done(1);
  IOStatus read_status = IOStatus::Fatal;
  Buffer received;

  server->read([&](IOStatus s, Buffer&& data, const Peer&) {
    read_status = s;
    received    = std::move(data);
    read_done.count_down();
  });

  std::latch write_done{1};
  auto write_status = IOStatus::Fatal;
  client->write({'h', 'e', 'l', 'l', 'o'}, {}, [&](IOStatus s) {
    write_status = s;
    write_done.count_down();
  });
  write_done.wait();
  ASSERT_EQ(write_status, IOStatus::Ok);

  read_done.wait();
  ASSERT_EQ(read_status, IOStatus::Ok);
  ASSERT_EQ(received.size(), 5);
  ASSERT_EQ(std::string(received.begin(), received.end()), "hello");

  {
    std::latch close_done{1};
    client->close([&close_done]() { close_done.count_down(); });
    close_done.wait();
  }
  {
    std::latch close_done{1};
    server->close([&close_done]() { close_done.count_down(); });
    close_done.wait();
  }
}

TEST_F(BaseSocketTest, WriteHeavy) {
  auto server      = makeSocket("127.0.0.1:0", *watcher_);
  auto bind_status = IOStatus::Fatal;
  std::string bind_addr;
  std::latch bind_done(1);
  server->bind([&](IOStatus s, const Address& addr) {
    bind_status = s;
    bind_addr   = std::string(addr);
    bind_done.count_down();
  });
  bind_done.wait();
  ASSERT_EQ(bind_status, IOStatus::Ok);

  auto client         = makeSocket(bind_addr, *watcher_);
  auto connect_status = IOStatus::Fatal;
  std::latch connect_done(1);
  client->connect([&](IOStatus s) {
    connect_status = s;
    connect_done.count_down();
  });
  connect_done.wait();
  ASSERT_EQ(connect_status, IOStatus::Ok);

  constexpr size_t data_size = 32UL * 1024;
  Buffer large_data;
  large_data.reserve(data_size);
  for (size_t i = 0; i < data_size; ++i) {
    large_data.push_back(static_cast<uint8_t>(i % 256));
  }

  std::latch write_done(1);
  int callback_count  = 0;
  IOStatus write_status = IOStatus::Fatal;

  client->write(std::move(large_data), {}, [&](IOStatus s) {
    write_status = s;
    callback_count++;
    write_done.count_down();
  });

  Buffer received;
  std::latch read_done(1);

  server->read([&](IOStatus s, Buffer&& data, const Peer&) {
    ASSERT_EQ(s, IOStatus::Ok);
    received = std::move(data);
    read_done.count_down();
  });

  write_done.wait();
  read_done.wait();

  ASSERT_EQ(write_status, IOStatus::Ok);
  ASSERT_EQ(callback_count, 1);
  ASSERT_EQ(received.size(), data_size);

  for (size_t i = 0; i < data_size; ++i) {
    EXPECT_EQ(received[i], static_cast<uint8_t>(i % 256));
  }

  {
    std::latch close_done{1};
    client->close([&close_done]() { close_done.count_down(); });
    close_done.wait();
  }
  {
    std::latch close_done{1};
    server->close([&close_done]() { close_done.count_down(); });
    close_done.wait();
  }
}

TEST_F(BaseSocketTest, PingPong) {
  auto sock1 = makeSocket("127.0.0.1:0", *watcher_);
  std::string addr1;
  std::latch bind1_done{1};
  sock1->bind([&](IOStatus s, const Address& addr) {
    ASSERT_EQ(s, IOStatus::Ok);
    addr1 = std::string(addr);
    bind1_done.count_down();
  });
  bind1_done.wait();

  auto sock2 = makeSocket("127.0.0.1:0", *watcher_);
  std::string addr2;
  std::latch bind2_done{1};
  sock2->bind([&](IOStatus s, const Address& addr) {
    ASSERT_EQ(s, IOStatus::Ok);
    addr2 = std::string(addr);
    bind2_done.count_down();
  });
  bind2_done.wait();

  auto client1 = makeSocket(addr2, *watcher_);
  std::latch connect1_done{1};
  client1->connect([&](IOStatus s) {
    ASSERT_EQ(s, IOStatus::Ok);
    connect1_done.count_down();
  });
  connect1_done.wait();

  auto client2 = makeSocket(addr1, *watcher_);
  std::latch connect2_done{1};
  client2->connect([&](IOStatus s) {
    ASSERT_EQ(s, IOStatus::Ok);
    connect2_done.count_down();
  });
  connect2_done.wait();

  std::latch read2_done{1};
  Buffer received2;
  sock2->read([&](IOStatus s, Buffer&& data, const Peer&) {
    ASSERT_EQ(s, IOStatus::Ok);
    received2 = std::move(data);
    read2_done.count_down();
  });

  std::latch write1_done{1};
  client1->write({'h', 'e', 'l', 'l', 'o'}, {}, [&](IOStatus s) {
    ASSERT_EQ(s, IOStatus::Ok);
    write1_done.count_down();
  });

  write1_done.wait();
  read2_done.wait();

  ASSERT_EQ(received2.size(), 5u);
  ASSERT_EQ(std::string(received2.begin(), received2.end()), "hello");

  std::latch read1_done{1};
  Buffer received1;
  sock1->read([&](IOStatus s, Buffer&& data, const Peer&) {
    ASSERT_EQ(s, IOStatus::Ok);
    received1 = std::move(data);
    read1_done.count_down();
  });

  std::latch write2_done{1};
  client2->write({'w', 'o', 'r', 'l', 'd'}, {}, [&](IOStatus s) {
    ASSERT_EQ(s, IOStatus::Ok);
    write2_done.count_down();
  });

  write2_done.wait();
  read1_done.wait();

  ASSERT_EQ(received1.size(), 5U);
  ASSERT_EQ(std::string(received1.begin(), received1.end()), "world");

  {
    std::latch close_done{1};
    sock1->close([&close_done]() { close_done.count_down(); });
    close_done.wait();
  }
  {
    std::latch close_done{1};
    sock2->close([&close_done]() { close_done.count_down(); });
    close_done.wait();
  }
  {
    std::latch close_done{1};
    client1->close([&close_done]() { close_done.count_down(); });
    close_done.wait();
  }
  {
    std::latch close_done{1};
    client2->close([&close_done]() { close_done.count_down(); });
    close_done.wait();
  }
}

TEST_F(BaseSocketTest, ServerReceivesPeerClosedStatus) {
  auto server = makeSocket("127.0.0.1:0", *watcher_);
  std::string bind_addr;
  std::latch bind_done{1};
  server->bind([&](IOStatus s, const Address& addr) {
    ASSERT_EQ(s, IOStatus::Ok);
    bind_addr = std::string(addr);
    bind_done.count_down();
  });
  bind_done.wait();

  auto client = makeSocket(bind_addr, *watcher_);
  std::latch connect_done{1};
  client->connect([&](IOStatus s) {
    ASSERT_EQ(s, IOStatus::Ok);
    connect_done.count_down();
  });
  connect_done.wait();

  std::latch read_done{1};
  IOStatus read_status = IOStatus::Fatal;

  server->read([&](IOStatus s, Buffer&&, const Peer&) {
    read_status = s;
    read_done.count_down();
  });

  std::latch close_done{1};
  client->close([&]() { close_done.count_down(); });
  close_done.wait();

  read_done.wait();
  EXPECT_EQ(read_status, IOStatus::Eof);

  std::latch server_close{1};
  server->close([&]() { server_close.count_down(); });
  server_close.wait();
}

TEST_F(BaseSocketTest, WriteFailsWhenPeerClosesDuringTransfer) {
  auto server = makeSocket("127.0.0.1:0", *watcher_);
  std::string bind_addr;
  std::latch bind_done{1};
  server->bind([&](IOStatus s, const Address& addr) {
    ASSERT_EQ(s, IOStatus::Ok);
    bind_addr = std::string(addr);
    bind_done.count_down();
  });
  bind_done.wait();

  auto client = makeSocket(bind_addr, *watcher_);
  std::latch connect_done{1};
  client->connect([&](IOStatus s) {
    ASSERT_EQ(s, IOStatus::Ok);
    connect_done.count_down();
  });
  connect_done.wait();

  std::string peer_id;
  std::latch peer_known{1};
  server->read([&](IOStatus s, Buffer&&, const Peer& peer) {
    ASSERT_EQ(s, IOStatus::Ok);
    peer_id = peer;
    peer_known.count_down();
  });

  std::latch write_done_client{1};
  client->write({'p', 'i', 'n', 'g'}, {}, [&](IOStatus s) {
    ASSERT_EQ(s, IOStatus::Ok);
    write_done_client.count_down();
  });
  write_done_client.wait();
  peer_known.wait();

  Buffer large_data;
  large_data.resize(128UL * 1024, 0x42);

  auto write_promise = std::make_shared<std::promise<IOStatus>>();
  auto write_future  = write_promise->get_future();
  std::atomic<bool> callback_invoked{false};

  server->write(std::move(large_data), peer_id,
                [write_promise, &callback_invoked](IOStatus s) {
                  write_promise->set_value(s);
                  callback_invoked.store(true, std::memory_order_release);
                });

  std::this_thread::sleep_for(std::chrono::milliseconds(10));

  std::latch client_close{1};
  client->close([&]() { client_close.count_down(); });
  client_close.wait();

  const auto wait_status = write_future.wait_for(std::chrono::seconds(5));
  EXPECT_TRUE(callback_invoked.load(std::memory_order_acquire));
  ASSERT_EQ(wait_status, std::future_status::ready);
  const IOStatus server_write_status = write_future.get();
  EXPECT_TRUE(server_write_status == IOStatus::Ok ||
              server_write_status == IOStatus::Eof);

  std::latch server_close{1};
  server->close([&]() { server_close.count_down(); });
  server_close.wait();
}

TEST_F(BaseSocketTest, CallbackInvokedExactlyOnce) {
  auto server      = makeSocket("127.0.0.1:0", *watcher_);
  auto bind_status = IOStatus::Fatal;
  std::string bind_addr;
  std::latch bind_done{1};
  server->bind([&](IOStatus s, const Address& addr) {
    bind_status = s;
    bind_addr   = std::string(addr);
    bind_done.count_down();
  });
  bind_done.wait();
  ASSERT_EQ(bind_status, IOStatus::Ok);

  auto client         = makeSocket(bind_addr, *watcher_);
  auto connect_status = IOStatus::Fatal;
  std::latch connect_done{1};
  client->connect([&](IOStatus s) {
    connect_status = s;
    connect_done.count_down();
  });
  connect_done.wait();
  ASSERT_EQ(connect_status, IOStatus::Ok);

  std::latch write_done{1};
  int write_callback_count = 0;
  client->write({'h', 'e', 'l', 'l', 'o'}, {}, [&](IOStatus s) {
    ASSERT_EQ(s, IOStatus::Ok);
    write_callback_count++;
    write_done.count_down();
  });

  std::latch read_done{1};
  int read_callback_count = 0;
  server->read([&](IOStatus s, Buffer&&, const Peer&) {
    ASSERT_EQ(s, IOStatus::Ok);
    read_callback_count++;
    read_done.count_down();
  });

  write_done.wait();
  read_done.wait();

  ASSERT_EQ(write_callback_count, 1);
  ASSERT_EQ(read_callback_count, 1);

  {
    std::latch close_done{1};
    client->close([&close_done]() { close_done.count_down(); });
    close_done.wait();
  }
  {
    std::latch close_done{1};
    server->close([&close_done]() { close_done.count_down(); });
    close_done.wait();
  }
}

TEST_F(BaseSocketTest, MoveOnlyAware) {
  auto server      = makeSocket("127.0.0.1:0", *watcher_);
  auto bind_status = IOStatus::Fatal;
  std::string bind_addr;
  std::latch bind_done{1};
  server->bind([&](IOStatus s, const Address& addr) {
    bind_status = s;
    bind_addr   = std::string(addr);
    bind_done.count_down();
  });
  bind_done.wait();
  ASSERT_EQ(bind_status, IOStatus::Ok);

  auto client         = makeSocket(bind_addr, *watcher_);
  auto connect_status = IOStatus::Fatal;
  std::latch connect_done{1};
  client->connect([&](IOStatus s) {
    connect_status = s;
    connect_done.count_down();
  });
  connect_done.wait();
  ASSERT_EQ(connect_status, IOStatus::Ok);

  auto resource = std::make_unique<int>(42);
  std::latch done{1};
  int captured_value = 0;
  client->write({'x'}, {}, [&, resource = std::move(resource)](IOStatus s) {
    ASSERT_EQ(s, IOStatus::Ok);
    captured_value = *resource;
    done.count_down();
  });
  done.wait();

  ASSERT_EQ(captured_value, 42);

  {
    std::latch close_done{1};
    client->close([&close_done]() { close_done.count_down(); });
    close_done.wait();
  }
  {
    std::latch close_done{1};
    server->close([&close_done]() { close_done.count_down(); });
    close_done.wait();
  }
}

TEST_F(BaseSocketTest, Shutdown) {
  std::latch read_done{1};
  std::latch write_done{1};
  IOStatus read_status  = IOStatus::Ok;
  IOStatus write_status = IOStatus::Ok;

  {
    auto local_watcher = std::make_unique<getrafty::io::EventWatcher>();
    auto server        = makeSocket("127.0.0.1:0", *local_watcher);
    auto bind_status   = IOStatus::Fatal;
    std::string bind_addr;
    std::latch bind_done{1};
    server->bind([&](IOStatus s, const Address& addr) {
      bind_status = s;
      bind_addr   = std::string(addr);
      bind_done.count_down();
    });
    bind_done.wait();
    ASSERT_EQ(bind_status, IOStatus::Ok);

    auto client         = makeSocket(bind_addr, *local_watcher);
    auto connect_status = IOStatus::Fatal;
    std::latch connect_done{1};
    client->connect([&](IOStatus s) {
      connect_status = s;
      connect_done.count_down();
    });
    connect_done.wait();
    ASSERT_EQ(connect_status, IOStatus::Ok);

    client->read([&](IOStatus s, Buffer&&, const Peer&) {
      read_status = s;
      read_done.count_down();
    });

    Buffer large_data;
    large_data.resize(32UL * 1024);
    client->write(std::move(large_data), {}, [&](IOStatus s) {
      write_status = s;
      write_done.count_down();
    });

    {
      std::latch close_done{1};
      client->close([&close_done]() { close_done.count_down(); });
      close_done.wait();
    }
    {
      std::latch close_done{1};
      server->close([&close_done]() { close_done.count_down(); });
      close_done.wait();
    }
  }

  read_done.wait();
  write_done.wait();

  ASSERT_NE(read_status, IOStatus::Fatal);
  ASSERT_NE(write_status, IOStatus::Fatal);
}

TEST_F(BaseSocketTest, Lifetime) {
  auto server      = makeSocket("127.0.0.1:0", *watcher_);
  auto bind_status = IOStatus::Fatal;
  std::string bind_addr;
  std::latch bind_done{1};
  server->bind([&](IOStatus s, const Address& addr) {
    bind_status = s;
    bind_addr   = std::string(addr);
    bind_done.count_down();
  });
  bind_done.wait();
  ASSERT_EQ(bind_status, IOStatus::Ok);

  {
    auto client         = makeSocket(bind_addr, *watcher_);
    auto connect_status = IOStatus::Fatal;
    std::latch connect_done{1};
    client->connect([&](IOStatus s) {
      connect_status = s;
      connect_done.count_down();
    });
    connect_done.wait();
    ASSERT_EQ(connect_status, IOStatus::Ok);

    client->write({'x'}, {}, [](IOStatus) {});

    {
      std::latch close_done{1};
      client->close([&close_done]() { close_done.count_down(); });
      close_done.wait();
    }
  }

  {
    std::latch close_done{1};
    server->close([&close_done]() { close_done.count_down(); });
    close_done.wait();
  }
}

TEST_F(BaseSocketTest, MultipleSocketsShareWatcher) {
  constexpr int num_sockets = 100;
  std::vector<SocketPtr> clients;
  std::vector<SocketPtr> servers;

  for (int i = 0; i < num_sockets; ++i) {
    auto srv         = makeSocket("127.0.0.1:0", *watcher_);
    auto bind_status = IOStatus::Fatal;
    std::string bind_addr;
    std::latch bind_done{1};
    srv->bind([&](IOStatus s, const Address& addr) {
      bind_status = s;
      bind_addr   = std::string(addr);
      bind_done.count_down();
    });
    bind_done.wait();
    ASSERT_EQ(bind_status, IOStatus::Ok);

    auto cli            = makeSocket(bind_addr, *watcher_);
    auto connect_status = IOStatus::Fatal;
    std::latch connect_done{1};
    cli->connect([&](IOStatus s) {
      connect_status = s;
      connect_done.count_down();
    });
    connect_done.wait();
    ASSERT_EQ(connect_status, IOStatus::Ok);

    servers.push_back(std::move(srv));
    clients.push_back(std::move(cli));
  }

  std::latch writes_done{num_sockets};
  std::latch reads_done{num_sockets};
  std::atomic<int> write_success_count{0};
  std::atomic<int> read_success_count{0};
  std::vector<Buffer> received(num_sockets);

  for (int i = 0; i < num_sockets; ++i) {
    clients[i]->write({'a', static_cast<uint8_t>('0' + i)}, {}, [&](IOStatus s) {
      if (s == IOStatus::Ok) {
        write_success_count.fetch_add(1);
      }
      writes_done.count_down();
    });

    servers[i]->read([&, i](IOStatus s, Buffer&& data, const Peer&) {
      if (s == IOStatus::Ok) {
        received[i] = std::move(data);
        read_success_count.fetch_add(1);
      }
      reads_done.count_down();
    });
  }

  writes_done.wait();
  reads_done.wait();

  ASSERT_EQ(write_success_count.load(), num_sockets);
  ASSERT_EQ(read_success_count.load(), num_sockets);

  for (int i = 0; i < num_sockets; ++i) {
    ASSERT_EQ(received[i].size(), 2u);
    ASSERT_EQ(received[i][0], 'a');
    ASSERT_EQ(received[i][1], static_cast<uint8_t>('0' + i));
  }

  for (auto& client : clients) {
    std::latch close_done{1};
    client->close([&close_done]() { close_done.count_down(); });
    close_done.wait();
  }

  for (auto& srv : servers) {
    std::latch close_done{1};
    srv->close([&close_done]() { close_done.count_down(); });
    close_done.wait();
  }
}

TEST_F(BaseSocketTest, ReadFromClosedSocket) {
  auto server      = makeSocket("127.0.0.1:0", *watcher_);
  auto bind_status = IOStatus::Fatal;
  std::string bind_addr;
  std::latch bind_done{1};
  server->bind([&](IOStatus s, const Address& addr) {
    bind_status = s;
    bind_addr   = std::string(addr);
    bind_done.count_down();
  });
  bind_done.wait();
  ASSERT_EQ(bind_status, IOStatus::Ok);

  auto client         = makeSocket(bind_addr, *watcher_);
  auto connect_status = IOStatus::Fatal;
  std::latch connect_done{1};
  client->connect([&](IOStatus s) {
    connect_status = s;
    connect_done.count_down();
  });
  connect_done.wait();
  ASSERT_EQ(connect_status, IOStatus::Ok);

  std::latch read_done{1};
  auto read_status = IOStatus::Fatal;
  client->read([&](IOStatus s, Buffer&&, const Peer&) {
    read_status = s;
    read_done.count_down();
  });

  std::latch close_done{1};
  client->close([&]() { close_done.count_down(); });
  close_done.wait();

  read_done.wait();
  ASSERT_EQ(read_status, IOStatus::Error);
}

TEST_F(BaseSocketTest, WriteToClosedSocket) {
  auto server      = makeSocket("127.0.0.1:0", *watcher_);
  auto bind_status = IOStatus::Fatal;
  std::string bind_addr;
  std::latch bind_done{1};
  server->bind([&](IOStatus s, const Address& addr) {
    bind_status = s;
    bind_addr   = std::string(addr);
    bind_done.count_down();
  });
  bind_done.wait();
  ASSERT_EQ(bind_status, IOStatus::Ok);

  auto client         = makeSocket(bind_addr, *watcher_);
  auto connect_status = IOStatus::Fatal;
  std::latch connect_done{1};
  client->connect([&](IOStatus s) {
    connect_status = s;
    connect_done.count_down();
  });
  connect_done.wait();
  ASSERT_EQ(connect_status, IOStatus::Ok);

  std::latch done{1};
  IOStatus status;
  client->write({'x'}, {}, [&](IOStatus s) {
    status = s;
    done.count_down();
  });
  done.wait();

  ASSERT_TRUE(status == IOStatus::Error || status == IOStatus::Ok);

  std::latch close_done{1};
  client->close([&]() { close_done.count_down(); });
  close_done.wait();
}

TEST_F(BaseSocketTest, DoubleClose) {
  auto server      = makeSocket("127.0.0.1:0", *watcher_);
  auto bind_status = IOStatus::Fatal;
  std::string bind_addr;
  std::latch bind_done{1};
  server->bind([&](IOStatus s, const Address& addr) {
    bind_status = s;
    bind_addr   = std::string(addr);
    bind_done.count_down();
  });
  bind_done.wait();
  ASSERT_EQ(bind_status, IOStatus::Ok);

  auto client         = makeSocket(bind_addr, *watcher_);
  auto connect_status = IOStatus::Fatal;
  std::latch connect_done{1};
  client->connect([&](IOStatus s) {
    connect_status = s;
    connect_done.count_down();
  });
  connect_done.wait();
  ASSERT_EQ(connect_status, IOStatus::Ok);

  std::latch close_done{1};
  client->close([&close_done]() { close_done.count_down(); });
  close_done.wait();
  std::latch close_done2{1};
  client->close([&close_done2]() { close_done2.count_down(); });
  close_done2.wait();
}

TEST_F(BaseSocketTest, ManyToOne) {
  constexpr int kClients = 5;

  auto srv         = makeSocket("127.0.0.1:0", *watcher_);
  auto bind_status = IOStatus::Fatal;
  std::string bind_addr;
  std::latch bind_done{1};
  srv->bind([&](IOStatus s, const Address& addr) {
    bind_status = s;
    bind_addr   = std::string(addr);
    bind_done.count_down();
  });
  bind_done.wait();
  ASSERT_EQ(bind_status, IOStatus::Ok);

  for (int i = 0; i < kClients; ++i) {
    auto cli            = makeSocket(bind_addr, *watcher_);
    auto connect_status = IOStatus::Fatal;
    std::latch connect_done{1};
    cli->connect([&](IOStatus s) {
      connect_status = s;
      connect_done.count_down();
    });
    connect_done.wait();
    ASSERT_EQ(connect_status, IOStatus::Ok);

    auto write_status = IOStatus::Fatal;
    std::latch write_done{1};
    cli->write({static_cast<uint8_t>(i)}, {}, [&](IOStatus s) {
      write_status = s;
      write_done.count_down();
    });
    write_done.wait();
    ASSERT_EQ(write_status, IOStatus::Ok);
  }

  std::vector<uint8_t> received_values;
  received_values.reserve(kClients);

  int attempts = 0;
  while (static_cast<int>(received_values.size()) < kClients &&
         attempts < kClients * 4) {
    ++attempts;
    Buffer read_data{};
    auto read_status = IOStatus::Fatal;
    std::latch read_done{1};
    srv->read([&](IOStatus s, Buffer&& data, const Peer&) {
      read_status = s;
      read_data   = std::move(data);
      read_done.count_down();
    });
    read_done.wait();

    if (read_status == IOStatus::Ok) {
      ASSERT_FALSE(read_data.empty());
      received_values.push_back(read_data.front());
    } else if (read_status == IOStatus::Eof || read_status == IOStatus::Error) {
      continue;
    } else {
      FAIL() << "unexpected read status " << static_cast<int>(read_status);
    }
  }

  ASSERT_EQ(static_cast<int>(received_values.size()), kClients);

  for (uint8_t value : received_values) {
    ASSERT_LT(value, kClients);
  }
}

// TEST_F(BaseSocketTest, OneToMany) {
//   constexpr int kClients = 5;

//   auto srv = makeSocket("127.0.0.1:0", *watcher_);
//   auto bind_status = Status::Fatal;
//   std::string bind_addr;
//   std::latch bind_done{1};
//   srv->bind([&](Status s, const Address& addr) {
//     bind_status = s;
//     bind_addr = std::string(addr);
//     bind_done.count_down();
//   });
//   bind_done.wait();
//   ASSERT_EQ(bind_status, Status::Ok);

//   for (int i = 0; i < kClients; ++i) {
//     auto write_status = Status::Fatal;
//     std::latch write_done{1};
//     srv->write(/*{peer},*/{ static_cast<uint8_t>(i)}, [&](Status s) {
//       write_status = s;
//       write_done.count_down();
//     });
//     write_done.wait();
//     ASSERT_EQ(write_status, Status::Ok);
//   }

//   for (int i = 0; i < kClients; ++i) {
//     auto cli = makeSocket(bind_addr, *watcher_);
//     auto connect_status = Status::Fatal;
//     std::latch connect_done{1};
//     cli->connect([&](Status s) {
//       connect_status = s;
//       connect_done.count_down();
//     });
//     connect_done.wait();
//     ASSERT_EQ(connect_status, Status::Ok);

//     Buffer read_data{};
//     auto read_status = Status::Fatal;
//     std::latch read_done{1};
//     cli->read([&](Status s, Buffer&& data, const Peer&) {
//       read_status = s;
//       read_data = std::move(data);
//       read_done.count_down();
//     });
//     read_done.wait();
//     ASSERT_EQ(read_status, Status::Ok);
//     ASSERT_GE(static_cast<uint8_t>(read_data.front()), 0);
//     ASSERT_LT(static_cast<uint8_t>(read_data.front()), kClients);
//   }
// }

TEST_F(BaseSocketTest, ReadZeroBytes) {
  auto server      = makeSocket("127.0.0.1:0", *watcher_);
  auto bind_status = IOStatus::Fatal;
  std::string bind_addr;
  std::latch bind_done{1};
  server->bind([&](IOStatus s, const Address& addr) {
    bind_status = s;
    bind_addr   = std::string(addr);
    bind_done.count_down();
  });
  bind_done.wait();
  ASSERT_EQ(bind_status, IOStatus::Ok);

  auto client         = makeSocket(bind_addr, *watcher_);
  auto connect_status = IOStatus::Fatal;
  std::latch connect_done{1};
  client->connect([&](IOStatus s) {
    connect_status = s;
    connect_done.count_down();
  });
  connect_done.wait();
  ASSERT_EQ(connect_status, IOStatus::Ok);

  std::latch done{1};
  IOStatus status;
  Buffer received;
  server->read([&](IOStatus s, Buffer&& data, const Peer&) {
    status   = s;
    received = std::move(data);
    done.count_down();
  });

  client->write({'x'}, {}, [](IOStatus) {});
  done.wait();

  ASSERT_EQ(status, IOStatus::Ok);
  ASSERT_EQ(received.size(), 1u);

  std::latch close_done{1};
  client->close([&]() { close_done.count_down(); });
  close_done.wait();
}

TEST_F(BaseSocketTest, WriteEmptyBuffer) {
  auto server      = makeSocket("127.0.0.1:0", *watcher_);
  auto bind_status = IOStatus::Fatal;
  std::string bind_addr;
  std::latch bind_done{1};
  server->bind([&](IOStatus s, const Address& addr) {
    bind_status = s;
    bind_addr   = std::string(addr);
    bind_done.count_down();
  });
  bind_done.wait();
  ASSERT_EQ(bind_status, IOStatus::Ok);

  auto client         = makeSocket(bind_addr, *watcher_);
  auto connect_status = IOStatus::Fatal;
  std::latch connect_done{1};
  client->connect([&](IOStatus s) {
    connect_status = s;
    connect_done.count_down();
  });
  connect_done.wait();
  ASSERT_EQ(connect_status, IOStatus::Ok);

  std::latch done{1};
  IOStatus status;
  Buffer empty_buffer;
  client->write(std::move(empty_buffer), {}, [&](IOStatus s) {
    status = s;
    done.count_down();
  });
  done.wait();

  ASSERT_EQ(status, IOStatus::Fatal);

  std::latch close_done{1};
  client->close([&]() { close_done.count_down(); });
  close_done.wait();
}

TEST_F(BaseSocketTest, SharePort) {
  auto server1     = makeSocket("127.0.0.1:0", *watcher_);
  auto bind_status = IOStatus::Fatal;
  std::string bind_addr;
  std::latch bind_done{1};
  server1->bind([&](IOStatus s, const Address& addr) {
    bind_status = s;
    bind_addr   = std::string(addr);
    bind_done.count_down();
  });
  bind_done.wait();
  ASSERT_EQ(bind_status, IOStatus::Ok);

  auto server2 = makeSocket(bind_addr, *watcher_);
  std::latch bind2_done{1};
  IOStatus bind2_status;
  server2->bind([&](IOStatus s, const Address& ) {
    bind2_status = s;
    bind2_done.count_down();
  });
  bind2_done.wait();

  ASSERT_EQ(bind2_status, IOStatus::Ok);

  {
    std::latch close_done{1};
    server1->close([&close_done]() { close_done.count_down(); });
    close_done.wait();
  }
  {
    std::latch close_done{1};
    server2->close([&close_done]() { close_done.count_down(); });
    close_done.wait();
  }
}

TEST_F(BaseSocketTest, MaxBytesLargerThanAvailable) {
  auto server      = makeSocket("127.0.0.1:0", *watcher_);
  auto bind_status = IOStatus::Fatal;
  std::string bind_addr;
  std::latch bind_done{1};
  server->bind([&](IOStatus s, const Address& addr) {
    bind_status = s;
    bind_addr   = std::string(addr);
    bind_done.count_down();
  });
  bind_done.wait();
  ASSERT_EQ(bind_status, IOStatus::Ok);

  auto client         = makeSocket(bind_addr, *watcher_);
  auto connect_status = IOStatus::Fatal;
  std::latch connect_done{1};
  client->connect([&](IOStatus s) {
    connect_status = s;
    connect_done.count_down();
  });
  connect_done.wait();
  ASSERT_EQ(connect_status, IOStatus::Ok);

  std::latch write_done{1};
  Buffer send_data = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9'};
  client->write(std::move(send_data), {}, [&](IOStatus s) {
    ASSERT_EQ(s, IOStatus::Ok);
    write_done.count_down();
  });

  std::latch read_done{1};
  auto read_status = IOStatus::Fatal;
  Buffer received;
  server->read([&](IOStatus s, Buffer&& data, const Peer&) {
    read_status = s;
    received    = std::move(data);
    read_done.count_down();
  });

  write_done.wait();
  read_done.wait();

  ASSERT_EQ(read_status, IOStatus::Ok);
  ASSERT_EQ(received.size(), 10U);
  for (size_t i = 0; i < 10; ++i) {
    ASSERT_EQ(received[i], static_cast<uint8_t>('0' + i));
  }

  std::latch close_done{1};
  client->close([&]() { close_done.count_down(); });
  close_done.wait();
}

TEST_F(BaseSocketTest, ConcurrentReadWriteOnSameSocket) {
  auto server      = makeSocket("127.0.0.1:0", *watcher_);
  auto bind_status = IOStatus::Fatal;
  std::string bind_addr;
  std::latch bind_done{1};
  server->bind([&](IOStatus s, const Address& addr) {
    bind_status = s;
    bind_addr   = std::string(addr);
    bind_done.count_down();
  });
  bind_done.wait();
  ASSERT_EQ(bind_status, IOStatus::Ok);

  auto client         = makeSocket(bind_addr, *watcher_);
  auto connect_status = IOStatus::Fatal;
  std::latch connect_done{1};
  client->connect([&](IOStatus s) {
    connect_status = s;
    connect_done.count_down();
  });
  connect_done.wait();
  ASSERT_EQ(connect_status, IOStatus::Ok);

  std::latch read_done{1};
  std::latch write_done{1};
  auto read_status  = IOStatus::Fatal;
  auto write_status = IOStatus::Fatal;
  Buffer received;

  std::thread reader([&]() {
    server->read([&](IOStatus s, Buffer&& data, const Peer&) {
      read_status = s;
      received    = std::move(data);
      read_done.count_down();
    });
  });

  std::thread writer([&]() {
    Buffer data = {'H', 'e', 'l', 'l', 'o'};
    client->write(std::move(data), {}, [&](IOStatus s) {
      write_status = s;
      write_done.count_down();
    });
  });

  reader.join();
  writer.join();

  read_done.wait();
  write_done.wait();

  ASSERT_EQ(read_status, IOStatus::Ok);
  ASSERT_EQ(write_status, IOStatus::Ok);
  ASSERT_EQ(received.size(), 5U);

  std::latch close_done{1};
  client->close([&]() { close_done.count_down(); });
  close_done.wait();
}

TEST_F(BaseSocketTest, NoCallbackAfterSocketDestruction) {
  auto marker = std::make_shared<std::atomic<bool>>(true);
  std::atomic<int> callbacks_after_destruction{0};

  {
    auto local_watcher = std::make_unique<getrafty::io::EventWatcher>();
    auto server        = makeSocket("127.0.0.1:0", *local_watcher);
    auto bind_status   = IOStatus::Fatal;
    std::string bind_addr;
    std::latch bind_done{1};
    server->bind([&](IOStatus s, const Address& addr) {
      bind_status = s;
      bind_addr   = std::string(addr);
      bind_done.count_down();
    });
    bind_done.wait();
    ASSERT_EQ(bind_status, IOStatus::Ok);

    auto client         = makeSocket(bind_addr, *local_watcher);
    auto connect_status = IOStatus::Fatal;
    std::latch connect_done{1};
    client->connect([&](IOStatus s) {
      connect_status = s;
      connect_done.count_down();
    });
    connect_done.wait();
    ASSERT_EQ(connect_status, IOStatus::Ok);

    std::weak_ptr<std::atomic<bool>> weak_marker = marker;

    Buffer large_data(32, 'X');
    client->write(std::move(large_data), {},
                  [weak_marker, &callbacks_after_destruction](IOStatus) {
                    if (weak_marker.expired()) {
                      callbacks_after_destruction.fetch_add(1);
                    }
                  });

    client->read([weak_marker, &callbacks_after_destruction](IOStatus, Buffer&&,
                                                             const Peer&) {
      if (weak_marker.expired()) {
        callbacks_after_destruction.fetch_add(1);
      }
    });

    std::latch close_done{1};
    client->close([&]() { close_done.count_down(); });
    close_done.wait();
  }

  marker.reset();

  ASSERT_EQ(callbacks_after_destruction.load(), 0);
}

TEST_F(BaseSocketTest, AtomicStateTransitions) {
  auto server      = makeSocket("127.0.0.1:0", *watcher_);
  auto bind_status = IOStatus::Fatal;
  std::string bind_addr;
  std::latch bind_done{1};
  server->bind([&](IOStatus s, const Address& addr) {
    bind_status = s;
    bind_addr   = std::string(addr);
    bind_done.count_down();
  });
  bind_done.wait();
  ASSERT_EQ(bind_status, IOStatus::Ok);

  auto client         = makeSocket(bind_addr, *watcher_);
  auto connect_status = IOStatus::Fatal;
  std::latch connect_done{1};
  client->connect([&](IOStatus s) {
    connect_status = s;
    connect_done.count_down();
  });
  connect_done.wait();
  ASSERT_EQ(connect_status, IOStatus::Ok);

  std::atomic<uint8_t> time{0};
  std::atomic<int> closed_count{0};
  std::latch all_done{10};

  std::vector<std::thread> threads;
  threads.reserve(10);
  for (int i = 0; i < 10; ++i) {
    threads.emplace_back([&, delay = 1 + (i % 2)]() {
      while (time.load() < delay) {
        std::this_thread::yield();
      }

      client->write({'X'}, {}, [&](IOStatus s) {
        if (s == IOStatus::Error) {
          closed_count.fetch_add(1);
        }
        all_done.count_down();
      });
    });
  }

  time++;
  client->close([&]() { time++; });
  for (auto& t : threads) {
    t.join();
  }
  all_done.wait();

  EXPECT_GT(closed_count.load(), 0);
}

TEST_F(BaseSocketTest, MultiThread) {
  auto server      = makeSocket("127.0.0.1:0", *watcher_);
  auto bind_status = IOStatus::Fatal;
  std::string bind_addr;
  std::latch bind_done{1};
  server->bind([&](IOStatus s, const Address& addr) {
    bind_status = s;
    bind_addr   = std::string(addr);
    bind_done.count_down();
  });
  bind_done.wait();
  ASSERT_EQ(bind_status, IOStatus::Ok);

  SocketPtr client;

  std::thread thread_a([&]() {
    client              = makeSocket(bind_addr, *watcher_);
    auto connect_status = IOStatus::Fatal;
    std::latch connect_done{1};
    client->connect([&](IOStatus s) {
      connect_status = s;
      connect_done.count_down();
    });
    connect_done.wait();
    EXPECT_EQ(connect_status, IOStatus::Ok);

    std::latch write_done{1};
    client->write({'A'}, {}, [&](IOStatus _) { write_done.count_down(); });
    write_done.wait();
  });

  thread_a.join();

  std::thread thread_b([&]() {
    std::latch write_done{1};
    client->write({'B'}, {}, [&](IOStatus _) { write_done.count_down(); });
    write_done.wait();
  });

  thread_b.join();

  for (int i = 0; i < 2; ++i) {
    auto read_status = IOStatus::Fatal;
    Buffer read_data{};
    std::latch read_done{1};
    server->read([&](IOStatus s, Buffer&& data, const Peer&) {
      read_status = s;
      read_data   = std::move(data);
      read_done.count_down();
    });
    read_done.wait();
    ASSERT_EQ(read_status, IOStatus::Ok);
    ASSERT_TRUE(read_data.front() == 'A' || read_data.front() == 'B');
  }
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
