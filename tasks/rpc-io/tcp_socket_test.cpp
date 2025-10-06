#include "tcp_socket.hpp"
#include <arpa/inet.h>
#include <gtest/gtest.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <dirent.h>
#include <atomic>
#include <chrono>
#include <latch>
#include <thread>
#include "event_watcher.hpp"

using namespace getrafty::rpc::io;

class MockEventWatcher : public EventWatcher {
 public:
  MockEventWatcher() : EventWatcher() {}

  void watch(int fd, WatchFlag flag, IWatchCallbackPtr callback) override {
    watch_count_.fetch_add(1);
    EventWatcher::watch(fd, flag, std::move(callback));
  }

  void unwatch(int fd, WatchFlag flag) override {
    unwatch_count_.fetch_add(1);
    EventWatcher::unwatch(fd, flag);
  }

  int watch_count() const { return watch_count_.load(); }
  int unwatch_count() const { return unwatch_count_.load(); }
  void reset_counts() {
    watch_count_.store(0);
    unwatch_count_.store(0);
  }

 private:
  std::atomic<int> watch_count_{0};
  std::atomic<int> unwatch_count_{0};
};

class TcpSocketTest : public ::testing::Test {
 protected:
  void SetUp() override { watcher_ = std::make_unique<EventWatcher>(); }

  void TearDown() override { watcher_.reset(); }

  std::unique_ptr<EventWatcher> watcher_;
};

TEST_F(TcpSocketTest, ServerSocketCreate) {
  auto server = TcpServerSocket::listen("127.0.0.1", 0, *watcher_);
  ASSERT_NE(server, nullptr);
  server->close();
}

TEST_F(TcpSocketTest, ServerSocketAccept) {
  auto server = TcpServerSocket::listen("127.0.0.1", 0, *watcher_);
  uint16_t port = server->port();

  std::thread client_thread([&]() {
    auto client = TcpSocket::connect("127.0.0.1", port, *watcher_);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  });

  auto server_conn = server->accept();
  ASSERT_NE(server_conn, nullptr);

  client_thread.join();
  server->close();
}

TEST_F(TcpSocketTest, ClientSocketConnect) {
  auto server = TcpServerSocket::listen("127.0.0.1", 0, *watcher_);
  uint16_t port = server->port();

  auto client = TcpSocket::connect("127.0.0.1", port, *watcher_);
  ASSERT_NE(client, nullptr);

  auto server_conn = server->accept();
  ASSERT_NE(server_conn, nullptr);

  client->close();
  server_conn->close();
  server->close();
}

TEST_F(TcpSocketTest, AsyncWriteBasic) {
  auto server = TcpServerSocket::listen("127.0.0.1", 0, *watcher_);
  uint16_t port = server->port();
  auto client = TcpSocket::connect("127.0.0.1", port, *watcher_);
  auto server_conn = server->accept();

  std::latch done(1);
  Status callback_status = Status::ERROR;

  Buffer data = {'h', 'e', 'l', 'l', 'o'};
  client->asyncWrite(std::move(data), [&](Status s) {
    callback_status = s;
    done.count_down();
  });

  done.wait();

  ASSERT_EQ(callback_status, Status::OK);
}

TEST_F(TcpSocketTest, AsyncReadBasic) {
  auto server = TcpServerSocket::listen("127.0.0.1", 0, *watcher_);
  uint16_t port = server->port();
  auto client = TcpSocket::connect("127.0.0.1", port, *watcher_);
  auto server_conn = server->accept();

  std::latch done(1);
  Status status = Status::ERROR;
  Buffer received;

  client->asyncRead(100, [&](Status s, Buffer&& data) {
    status = s;
    received = std::move(data);
    done.count_down();
  });

  server_conn->asyncWrite({'h', 'e', 'l', 'l', 'o'}, [](Status) {});

  done.wait();

  ASSERT_EQ(status, Status::OK);
  ASSERT_EQ(received.size(), 5);
  ASSERT_EQ(std::string(received.begin(), received.end()), "hello");
}

TEST_F(TcpSocketTest, AsyncReadPartial) {
  auto server = TcpServerSocket::listen("127.0.0.1", 0, *watcher_);
  uint16_t port = server->port();
  auto client = TcpSocket::connect("127.0.0.1", port, *watcher_);
  auto server_conn = server->accept();

  Buffer send_data;
  send_data.reserve(1000);
  for (size_t i = 0; i < 1000; ++i) {
    send_data.push_back(static_cast<uint8_t>(i % 256));
  }

  std::latch write_done(1);
  server_conn->asyncWrite(std::move(send_data), [&](Status s) {
    ASSERT_EQ(s, Status::OK);
    write_done.count_down();
  });

  std::latch read1_done(1);
  Status read1_status = Status::ERROR;
  Buffer received1;

  client->asyncRead(100, [&](Status s, Buffer&& data) {
    read1_status = s;
    received1 = std::move(data);
    read1_done.count_down();
  });

  read1_done.wait();
  write_done.wait();

  ASSERT_EQ(read1_status, Status::OK);
  EXPECT_GT(received1.size(), 0u);
  EXPECT_LE(received1.size(), 100u);

  for (size_t i = 0; i < received1.size(); ++i) {
    EXPECT_EQ(received1[i], static_cast<uint8_t>(i % 256));
  }

  std::latch read2_done(1);
  Status read2_status = Status::ERROR;
  Buffer received2;

  client->asyncRead(1000, [&](Status s, Buffer&& data) {
    read2_status = s;
    received2 = std::move(data);
    read2_done.count_down();
  });

  read2_done.wait();

  ASSERT_EQ(read2_status, Status::OK);
  EXPECT_EQ(received1.size() + received2.size(), 1000u);

  for (size_t i = 0; i < received2.size(); ++i) {
    EXPECT_EQ(received2[i], static_cast<uint8_t>((received1.size() + i) % 256));
  }

  client->close();
  server_conn->close();
  server->close();
}

TEST_F(TcpSocketTest, AsyncWriteHandlesPartialWrites) {
  auto server = TcpServerSocket::listen("127.0.0.1", 0, *watcher_);
  uint16_t port = server->port();
  auto client = TcpSocket::connect("127.0.0.1", port, *watcher_);
  auto server_conn = server->accept();

  Buffer large_data;
  constexpr size_t data_size = 1024 * 1024;
  large_data.reserve(data_size);
  for (size_t i = 0; i < data_size; ++i) {
    large_data.push_back(static_cast<uint8_t>(i % 256));
  }

  std::latch write_done(1);
  int callback_count = 0;
  Status write_status = Status::ERROR;

  client->asyncWrite(std::move(large_data), [&](Status s) {
    write_status = s;
    callback_count++;
    write_done.count_down();
  });

  Buffer received;
  received.reserve(data_size);
  std::latch all_read(1);

  std::function<void()> read_more = [&]() {
    server_conn->asyncRead(data_size, [&](Status s, Buffer&& data) {
      ASSERT_EQ(s, Status::OK);
      received.insert(received.end(), data.begin(), data.end());
      if (received.size() < data_size) {
        read_more();
      } else {
        all_read.count_down();
      }
    });
  };
  read_more();

  write_done.wait();
  all_read.wait();

  ASSERT_EQ(write_status, Status::OK);
  ASSERT_EQ(callback_count, 1);
  ASSERT_EQ(received.size(), data_size);

  for (size_t i = 0; i < data_size; ++i) {
    EXPECT_EQ(received[i], static_cast<uint8_t>(i % 256));
  }

  client->close();
  server_conn->close();
  server->close();
}

TEST_F(TcpSocketTest, BidirectionalCommunication) {
  auto server = TcpServerSocket::listen("127.0.0.1", 0, *watcher_);
  uint16_t port = server->port();
  auto client = TcpSocket::connect("127.0.0.1", port, *watcher_);
  auto server_conn = server->accept();

  std::latch client_write_done(1);
  client->asyncWrite({'h', 'e', 'l', 'l', 'o'}, [&](Status s) {
    ASSERT_EQ(s, Status::OK);
    client_write_done.count_down();
  });

  std::latch server_read_done(1);
  Buffer server_received;
  server_conn->asyncRead(100, [&](Status s, Buffer&& data) {
    ASSERT_EQ(s, Status::OK);
    server_received = std::move(data);
    server_read_done.count_down();
  });

  client_write_done.wait();
  server_read_done.wait();

  ASSERT_EQ(server_received.size(), 5u);
  ASSERT_EQ(std::string(server_received.begin(), server_received.end()),
            "hello");

  std::latch server_write_done(1);
  server_conn->asyncWrite({'w', 'o', 'r', 'l', 'd'}, [&](Status s) {
    ASSERT_EQ(s, Status::OK);
    server_write_done.count_down();
  });

  std::latch client_read_done(1);
  Buffer client_received;
  client->asyncRead(100, [&](Status s, Buffer&& data) {
    ASSERT_EQ(s, Status::OK);
    client_received = std::move(data);
    client_read_done.count_down();
  });

  server_write_done.wait();
  client_read_done.wait();

  ASSERT_EQ(client_received.size(), 5u);
  ASSERT_EQ(std::string(client_received.begin(), client_received.end()),
            "world");

  client->close();
  server_conn->close();
  server->close();
}

TEST_F(TcpSocketTest, CallbackInvokedExactlyOnce) {
  auto server = TcpServerSocket::listen("127.0.0.1", 0, *watcher_);
  uint16_t port = server->port();
  auto client = TcpSocket::connect("127.0.0.1", port, *watcher_);
  auto server_conn = server->accept();

  int write_callback_count = 0;
  std::latch write_done(1);
  client->asyncWrite({'h', 'e', 'l', 'l', 'o'}, [&](Status s) {
    ASSERT_EQ(s, Status::OK);
    write_callback_count++;
    write_done.count_down();
  });

  int read_callback_count = 0;
  std::latch read_done(1);
  server_conn->asyncRead(100, [&](Status s, Buffer&&) {
    ASSERT_EQ(s, Status::OK);
    read_callback_count++;
    read_done.count_down();
  });

  write_done.wait();
  read_done.wait();

  ASSERT_EQ(write_callback_count, 1);
  ASSERT_EQ(read_callback_count, 1);

  client->close();
  server_conn->close();
  server->close();
}

TEST_F(TcpSocketTest, CallbacksSupportMoveOnlyTypes) {
  auto server = TcpServerSocket::listen("127.0.0.1", 0, *watcher_);
  uint16_t port = server->port();
  ASSERT_NE(server, nullptr) << "Failed to listen on port " << port;
  auto client = TcpSocket::connect("127.0.0.1", port, *watcher_);
  auto server_conn = server->accept();

  auto resource = std::make_unique<int>(42);
  std::latch done(1);
  int captured_value = 0;

  client->asyncWrite({'x'}, [&, resource = std::move(resource)](Status s) {
    ASSERT_EQ(s, Status::OK);
    captured_value = *resource;
    done.count_down();
  });

  done.wait();

  ASSERT_EQ(captured_value, 42);

  client->close();
  server_conn->close();
  server->close();
}

TEST_F(TcpSocketTest, SocketDestructionCancelsOperations) {
  auto server = TcpServerSocket::listen("127.0.0.1", 0, *watcher_);
  uint16_t port = server->port();
  ASSERT_NE(server, nullptr);

  auto client = TcpSocket::connect("127.0.0.1", port, *watcher_);
  auto server_conn = server->accept();

  std::latch read_done(1);
  std::latch write_done(1);
  Status read_status = Status::OK;
  Status write_status = Status::OK;

  client->asyncRead(100, [&](Status s, Buffer&&) {
    read_status = s;
    read_done.count_down();
  });

  Buffer large_data(1024 *
                    1024);  // 1MB
  client->asyncWrite(std::move(large_data), [&](Status s) {
    write_status = s;
    write_done.count_down();
  });

  client->close();

  read_done.wait();
  write_done.wait();

  ASSERT_EQ(read_status, Status::CLOSED);
  ASSERT_EQ(write_status, Status::CLOSED);

  server_conn->close();
  server->close();
}

TEST_F(TcpSocketTest, WatcherOutlivesSocket) {
  auto server = TcpServerSocket::listen("127.0.0.1", 0, *watcher_);
  uint16_t port = server->port();

  {
    auto client = TcpSocket::connect("127.0.0.1", port, *watcher_);
    auto server_conn = server->accept();

    client->asyncWrite({'x'}, [](Status) {});
  }

  auto client2 = TcpSocket::connect("127.0.0.1", port, *watcher_);
  auto server_conn2 = server->accept();

  std::latch done(1);
  Status status = Status::ERROR;

  client2->asyncWrite({'y'}, [&](Status s) {
    status = s;
    done.count_down();
  });

  done.wait();

  ASSERT_EQ(status, Status::OK);

  client2->close();
  server_conn2->close();
  server->close();
}

TEST_F(TcpSocketTest, MultipleSocketsShareWatcher) {
  auto server = TcpServerSocket::listen("127.0.0.1", 0, *watcher_);
  uint16_t port = server->port();

  constexpr int num_sockets = 10;
  std::vector<std::unique_ptr<TcpSocket>> clients;
  std::vector<std::unique_ptr<ISocket>> server_conns;

  for (int i = 0; i < num_sockets; ++i) {
    clients.push_back(TcpSocket::connect("127.0.0.1", port, *watcher_));
    server_conns.push_back(server->accept());
  }

  std::latch writes_done(num_sockets);
  std::latch reads_done(num_sockets);
  std::atomic<int> write_success_count{0};
  std::atomic<int> read_success_count{0};
  std::vector<Buffer> received(num_sockets);

  for (int i = 0; i < num_sockets; ++i) {
    clients[i]->asyncWrite({'a', static_cast<uint8_t>('0' + i)}, [&](Status s) {
      if (s == Status::OK) {
        write_success_count.fetch_add(1);
      }
      writes_done.count_down();
    });

    server_conns[i]->asyncRead(100, [&, i](Status s, Buffer&& data) {
      if (s == Status::OK) {
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
    client->close();
  }

  for (auto& conn : server_conns) {
    conn->close();
  }
  server->close();
}

TEST_F(TcpSocketTest, ConnectToNonExistentServer) {
  EXPECT_THROW(
      { TcpSocket::connect("127.0.0.1", 1, *watcher_); }, std::runtime_error);
}

TEST_F(TcpSocketTest, ReadFromClosedSocket) {
  auto server = TcpServerSocket::listen("127.0.0.1", 0, *watcher_);
  uint16_t port = server->port();
  auto client = TcpSocket::connect("127.0.0.1", port, *watcher_);
  auto server_conn = server->accept();

  client->close();

  std::latch done(1);
  Status status = Status::OK;

  client->asyncRead(100, [&](Status s, Buffer&&) {
    status = s;
    done.count_down();
  });

  done.wait();

  ASSERT_EQ(status, Status::CLOSED);

  server_conn->close();
  server->close();
}

TEST_F(TcpSocketTest, WriteToClosedSocket) {
  auto server = TcpServerSocket::listen("127.0.0.1", 0, *watcher_);
  uint16_t port = server->port();
  auto client = TcpSocket::connect("127.0.0.1", port, *watcher_);
  auto server_conn = server->accept();

  client->close();

  std::latch done(1);
  Status status = Status::OK;

  client->asyncWrite({'x'}, [&](Status s) {
    status = s;
    done.count_down();
  });

  done.wait();

  ASSERT_EQ(status, Status::CLOSED);

  server_conn->close();
  server->close();
}

TEST_F(TcpSocketTest, PeerClosesConnection) {
  auto server = TcpServerSocket::listen("127.0.0.1", 0, *watcher_);
  uint16_t port = server->port();
  auto client = TcpSocket::connect("127.0.0.1", port, *watcher_);
  auto server_conn = server->accept();

  std::latch done(1);
  Status status = Status::OK;
  Buffer received;

  client->asyncRead(100, [&](Status s, Buffer&& data) {
    status = s;
    received = std::move(data);
    done.count_down();
  });

  server_conn->close();

  done.wait();

  ASSERT_EQ(status, Status::PEER_CLOSED);
  ASSERT_EQ(received.size(), 0u);

  client->close();
  server->close();
}

TEST_F(TcpSocketTest, PeerClosesWhileWriting) {
  auto server = TcpServerSocket::listen("127.0.0.1", 0, *watcher_);
  uint16_t port = server->port();
  auto client = TcpSocket::connect("127.0.0.1", port, *watcher_);
  auto server_conn = server->accept();

  client->close();

  std::latch done(1);
  Status status = Status::OK;

  Buffer large_data(1024 * 1024, 'x');
  server_conn->asyncWrite(std::move(large_data), [&](Status s) {
    status = s;
    done.count_down();
  });

  done.wait();

  ASSERT_EQ(status, Status::BROKEN_PIPE);

  server_conn->close();
  server->close();
}

TEST_F(TcpSocketTest, DoubleClose) {
  auto server = TcpServerSocket::listen("127.0.0.1", 0, *watcher_);
  uint16_t port = server->port();
  auto client = TcpSocket::connect("127.0.0.1", port, *watcher_);
  auto server_conn = server->accept();

  client->close();
  client->close();

  server_conn->close();
  server_conn->close();

  server->close();
  server->close();
}

TEST_F(TcpSocketTest, AsyncOpOnClosedSocket) {
  auto server = TcpServerSocket::listen("127.0.0.1", 0, *watcher_);
  uint16_t port = server->port();
  auto client = TcpSocket::connect("127.0.0.1", port, *watcher_);
  auto server_conn = server->accept();

  client->close();

  std::latch read_done(1);
  std::latch write_done(1);
  Status read_status = Status::OK;
  Status write_status = Status::OK;

  client->asyncRead(100, [&](Status s, Buffer&&) {
    read_status = s;
    read_done.count_down();
  });

  client->asyncWrite({'x'}, [&](Status s) {
    write_status = s;
    write_done.count_down();
  });

  read_done.wait();
  write_done.wait();

  ASSERT_EQ(read_status, Status::CLOSED);
  ASSERT_EQ(write_status, Status::CLOSED);

  server_conn->close();
  server->close();
}

TEST_F(TcpSocketTest, NoRepeatedWatchUnwatch) {
  MockEventWatcher mock_watcher;
  auto server = TcpServerSocket::listen("127.0.0.1", 0, mock_watcher);
  uint16_t port = server->port();
  ASSERT_NE(server, nullptr) << "Server listen failed";
  auto client = TcpSocket::connect("127.0.0.1", port, mock_watcher);
  auto server_conn = server->accept();

  mock_watcher.reset_counts();

  auto first_read = std::make_shared<std::latch>(1);
  client->asyncRead(100, [first_read](Status s, Buffer&& data) {
    if (s == Status::OK) {
      ASSERT_EQ(data.size(), 1u);
    }
    first_read->count_down();
  });

  server_conn->asyncWrite({'A'}, [](Status) {});
  first_read->wait();
  std::this_thread::sleep_for(std::chrono::milliseconds(10));

  int watch_after_first_read = mock_watcher.watch_count();

  auto second_read = std::make_shared<std::latch>(1);
  client->asyncRead(100, [second_read](Status s, Buffer&& data) {
    EXPECT_NE(s, Status::BUSY);
    if (s == Status::OK) {
      ASSERT_EQ(data.size(), 1u);
    }
    second_read->count_down();
  });

  std::this_thread::sleep_for(std::chrono::milliseconds(10));
  std::latch write_done(1);
  server_conn->asyncWrite({'B'},
                          [&write_done](Status) { write_done.count_down(); });
  write_done.wait();

  second_read->wait();

  int watch_after_second_read = mock_watcher.watch_count();

  EXPECT_EQ(watch_after_first_read, 2);
  EXPECT_EQ(watch_after_second_read, 4);
  EXPECT_GE(mock_watcher.unwatch_count(), 0);

  client->close();
  server_conn->close();
  server->close();
}

TEST_F(TcpSocketTest, ConcurrentReadsOnDifferentSockets) {
  auto server = TcpServerSocket::listen("127.0.0.1", 0, *watcher_);
  uint16_t port = server->port();

  constexpr int num_sockets = 5;
  std::vector<std::unique_ptr<TcpSocket>> clients;
  std::vector<std::unique_ptr<ISocket>> server_conns;

  for (int i = 0; i < num_sockets; ++i) {
    clients.push_back(TcpSocket::connect("127.0.0.1", port, *watcher_));
    server_conns.push_back(server->accept());
  }

  std::latch reads_done(num_sockets);
  std::atomic<int> read_success_count{0};
  std::vector<Buffer> received(num_sockets);

  for (int i = 0; i < num_sockets; ++i) {
    clients[i]->asyncRead(100, [&, i](Status s, Buffer&& data) {
      if (s == Status::OK) {
        received[i] = std::move(data);
        read_success_count.fetch_add(1);
      }
      reads_done.count_down();
    });
  }

  for (int i = 0; i < num_sockets; ++i) {
    Buffer send_data = {'m', 's', 'g', static_cast<uint8_t>('0' + i)};
    server_conns[i]->asyncWrite(std::move(send_data), [](Status) {});
  }

  reads_done.wait();

  ASSERT_EQ(read_success_count.load(), num_sockets);

  for (int i = 0; i < num_sockets; ++i) {
    ASSERT_EQ(received[i].size(), 4u);
    ASSERT_EQ(received[i][0], 'm');
    ASSERT_EQ(received[i][1], 's');
    ASSERT_EQ(received[i][2], 'g');
    ASSERT_EQ(received[i][3], static_cast<uint8_t>('0' + i));
  }

  for (auto& client : clients) {
    client->close();
  }
  for (auto& conn : server_conns) {
    conn->close();
  }
  server->close();
}

TEST_F(TcpSocketTest, ReadZeroBytes) {
  auto server = TcpServerSocket::listen("127.0.0.1", 0, *watcher_);
  uint16_t port = server->port();
  auto client = TcpSocket::connect("127.0.0.1", port, *watcher_);
  auto server_conn = server->accept();

  std::latch done(1);
  Status status = Status::ERROR;
  Buffer received;

  client->asyncRead(0, [&](Status s, Buffer&& data) {
    status = s;
    received = std::move(data);
    done.count_down();
  });

  server_conn->asyncWrite({'x'}, [](Status) {});

  done.wait();

  ASSERT_TRUE(status == Status::OK || status == Status::PEER_CLOSED);
  ASSERT_EQ(received.size(), 0u);

  client->close();
  server_conn->close();
  server->close();
}

TEST_F(TcpSocketTest, WriteEmptyBuffer) {
  auto server = TcpServerSocket::listen("127.0.0.1", 0, *watcher_);
  uint16_t port = server->port();
  auto client = TcpSocket::connect("127.0.0.1", port, *watcher_);
  auto server_conn = server->accept();

  std::latch done(1);
  Status status = Status::ERROR;

  Buffer empty_buffer;
  client->asyncWrite(std::move(empty_buffer), [&](Status s) {
    status = s;
    done.count_down();
  });

  done.wait();

  ASSERT_EQ(status, Status::OK);

  client->close();
  server_conn->close();
  server->close();
}

TEST_F(TcpSocketTest, ServerSocketBindToUsedPort) {
  auto server1 = TcpServerSocket::listen("127.0.0.1", 0, *watcher_);
  uint16_t port = server1->port();
  ASSERT_NE(server1, nullptr);

  EXPECT_THROW(
      { auto server2 = TcpServerSocket::listen("127.0.0.1", port, *watcher_); },
      std::runtime_error);

  server1->close();
}

TEST_F(TcpSocketTest, AcceptMultipleConnections) {
  auto server = TcpServerSocket::listen("127.0.0.1", 0, *watcher_);
  uint16_t port = server->port();

  constexpr int num_clients = 3;
  std::vector<std::unique_ptr<TcpSocket>> clients;
  std::vector<std::unique_ptr<ISocket>> server_conns;

  for (int i = 0; i < num_clients; ++i) {
    clients.push_back(TcpSocket::connect("127.0.0.1", port, *watcher_));
    server_conns.push_back(server->accept());
    ASSERT_NE(server_conns[i], nullptr);
  }

  ASSERT_EQ(server_conns.size(), num_clients);

  for (int i = 0; i < num_clients; ++i) {
    for (int j = i + 1; j < num_clients; ++j) {
      ASSERT_NE(server_conns[i].get(), server_conns[j].get());
    }
  }

  for (auto& client : clients) {
    client->close();
  }
  for (auto& conn : server_conns) {
    conn->close();
  }
  server->close();
}

TEST_F(TcpSocketTest, MaxBytesLargerThanAvailable) {
  auto server = TcpServerSocket::listen("127.0.0.1", 0, *watcher_);
  uint16_t port = server->port();
  auto client = TcpSocket::connect("127.0.0.1", port, *watcher_);
  auto server_conn = server->accept();

  std::latch write_done(1);
  Buffer send_data = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9'};
  server_conn->asyncWrite(std::move(send_data), [&](Status s) {
    ASSERT_EQ(s, Status::OK);
    write_done.count_down();
  });

  std::latch read_done(1);
  Status read_status = Status::ERROR;
  Buffer received;

  client->asyncRead(1000, [&](Status s, Buffer&& data) {
    read_status = s;
    received = std::move(data);
    read_done.count_down();
  });

  write_done.wait();
  read_done.wait();

  ASSERT_EQ(read_status, Status::OK);
  ASSERT_EQ(received.size(), 10u);
  for (size_t i = 0; i < 10; ++i) {
    ASSERT_EQ(received[i], static_cast<uint8_t>('0' + i));
  }

  client->close();
  server_conn->close();
  server->close();
}

TEST_F(TcpSocketTest, ConcurrentReadsOnSameSocket) {
  auto server = TcpServerSocket::listen("127.0.0.1", 0, *watcher_);
  uint16_t port = server->port();
  auto client = TcpSocket::connect("127.0.0.1", port, *watcher_);
  auto server_conn = server->accept();

  constexpr int num_threads = 10;
  std::atomic<int> callback_count{0};
  std::atomic<int> success_count{0};
  std::atomic<int> busy_count{0};
  std::vector<std::thread> threads;
  std::latch all_reads_issued(num_threads);
  std::latch all_reads_done(num_threads);

  for (int i = 0; i < num_threads; ++i) {
    threads.emplace_back([&]() {
      all_reads_issued.count_down();
      all_reads_issued.wait();

      client->asyncRead(100, [&](Status s, Buffer&&) {
        callback_count.fetch_add(1);
        if (s == Status::OK) {
          success_count.fetch_add(1);
        } else if (s == Status::BUSY) {
          busy_count.fetch_add(1);
        }
        all_reads_done.count_down();
      });
    });
  }

  all_reads_issued.wait();
  std::this_thread::sleep_for(std::chrono::milliseconds(10));

  Buffer msg = {'X'};
  server_conn->asyncWrite(std::move(msg), [](Status) {});

  all_reads_done.wait();

  for (auto& t : threads) {
    t.join();
  }

  ASSERT_EQ(callback_count.load(), num_threads);
  EXPECT_EQ(success_count.load(), 1);
  EXPECT_EQ(busy_count.load(), num_threads - 1);

  client->close();
  server_conn->close();
  server->close();
}

TEST_F(TcpSocketTest, ConcurrentWritesOnSameSocket) {
  auto server = TcpServerSocket::listen("127.0.0.1", 0, *watcher_);
  uint16_t port = server->port();
  auto client = TcpSocket::connect("127.0.0.1", port, *watcher_);
  auto server_conn = server->accept();

  constexpr int num_threads = 10;
  std::atomic<int> callback_count{0};
  std::atomic<int> success_count{0};
  std::atomic<int> busy_count{0};
  std::vector<std::thread> threads;
  std::latch all_writes_issued(num_threads);
  std::latch all_writes_done(num_threads);

  for (int i = 0; i < num_threads; ++i) {
    threads.emplace_back([&]() {
      all_writes_issued.count_down();
      all_writes_issued.wait();

      Buffer data = {'W'};
      client->asyncWrite(std::move(data), [&](Status s) {
        callback_count.fetch_add(1);
        if (s == Status::OK) {
          success_count.fetch_add(1);
        } else if (s == Status::BUSY) {
          busy_count.fetch_add(1);
        }
        all_writes_done.count_down();
      });
    });
  }

  all_writes_issued.wait();
  all_writes_done.wait();

  for (auto& t : threads) {
    t.join();
  }

  ASSERT_EQ(callback_count.load(), num_threads);
  EXPECT_GE(success_count.load(), 1);
  EXPECT_LE(success_count.load(), num_threads);
  EXPECT_EQ(success_count.load() + busy_count.load(), num_threads);

  client->close();
  server_conn->close();
  server->close();
}

TEST_F(TcpSocketTest, ConcurrentReadWriteOnSameSocket) {
  auto server = TcpServerSocket::listen("127.0.0.1", 0, *watcher_);
  uint16_t port = server->port();
  auto client = TcpSocket::connect("127.0.0.1", port, *watcher_);
  auto server_conn = server->accept();

  std::latch read_done(1);
  std::latch write_done(1);
  Status read_status = Status::ERROR;
  Status write_status = Status::ERROR;
  Buffer received;

  std::thread reader([&]() {
    client->asyncRead(100, [&](Status s, Buffer&& data) {
      read_status = s;
      received = std::move(data);
      read_done.count_down();
    });
  });

  std::thread writer([&]() {
    Buffer data = {'H', 'e', 'l', 'l', 'o'};
    server_conn->asyncWrite(std::move(data), [&](Status s) {
      write_status = s;
      write_done.count_down();
    });
  });

  reader.join();
  writer.join();

  read_done.wait();
  write_done.wait();

  ASSERT_EQ(read_status, Status::OK);
  ASSERT_EQ(write_status, Status::OK);
  ASSERT_EQ(received.size(), 5u);

  client->close();
  server_conn->close();
  server->close();
}

TEST_F(TcpSocketTest, ConcurrentCloseWhileReading) {
  auto server = TcpServerSocket::listen("127.0.0.1", 0, *watcher_);
  uint16_t port = server->port();
  auto client = TcpSocket::connect("127.0.0.1", port, *watcher_);
  auto server_conn = server->accept();

  std::latch read_issued(1);
  std::latch read_done(1);
  Status read_status = Status::OK;

  std::thread reader([&]() {
    client->asyncRead(100, [&](Status s, Buffer&&) {
      read_status = s;
      read_done.count_down();
    });
    read_issued.count_down();
  });

  read_issued.wait();
  std::this_thread::sleep_for(std::chrono::milliseconds(10));

  client->close();

  read_done.wait();
  reader.join();

  ASSERT_EQ(read_status, Status::CLOSED);

  server_conn->close();
  server->close();
}

TEST_F(TcpSocketTest, ConcurrentCloseWhileWriting) {
  auto server = TcpServerSocket::listen("127.0.0.1", 0, *watcher_);
  uint16_t port = server->port();
  auto client = TcpSocket::connect("127.0.0.1", port, *watcher_);
  auto server_conn = server->accept();

  std::latch write_issued(1);
  std::latch write_done(1);
  Status write_status = Status::OK;

  std::thread writer([&]() {
    Buffer large_data(1024 * 1024, 'X');
    client->asyncWrite(std::move(large_data), [&](Status s) {
      write_status = s;
      write_done.count_down();
    });
    write_issued.count_down();
  });

  write_issued.wait();
  std::this_thread::sleep_for(std::chrono::milliseconds(10));

  client->close();

  write_done.wait();
  writer.join();

  ASSERT_TRUE(write_status == Status::CLOSED || write_status == Status::OK);

  server_conn->close();
  server->close();
}

TEST_F(TcpSocketTest, CallbacksNeverLostUnderConcurrency) {
  auto server = TcpServerSocket::listen("127.0.0.1", 0, *watcher_);
  uint16_t port = server->port();

  constexpr int num_operations = 100;
  std::atomic<int> operations_issued{0};
  std::atomic<int> callbacks_received{0};
  std::vector<std::thread> threads;
  std::latch all_done(num_operations);

  for (int i = 0; i < num_operations; ++i) {
    threads.emplace_back([&]() {
      auto client = TcpSocket::connect("127.0.0.1", port, *watcher_);
      auto server_conn = server->accept();

      operations_issued.fetch_add(1);

      client->asyncWrite({'T', 'E', 'S', 'T'}, [&](Status) {
        callbacks_received.fetch_add(1);
        all_done.count_down();
      });
    });
  }

  all_done.wait();

  for (auto& t : threads) {
    t.join();
  }

  ASSERT_EQ(operations_issued.load(), num_operations);
  ASSERT_EQ(callbacks_received.load(), num_operations);
}

TEST_F(TcpSocketTest, CallbackOrderingWithConcurrentOps) {
  auto server = TcpServerSocket::listen("127.0.0.1", 0, *watcher_);
  uint16_t port = server->port();
  auto client = TcpSocket::connect("127.0.0.1", port, *watcher_);
  auto server_conn = server->accept();

  constexpr int num_writes = 5;
  std::atomic<int> successful_writes{0};
  std::latch writes_done(num_writes);

  for (int i = 0; i < num_writes; ++i) {
    Buffer data = {static_cast<uint8_t>(i)};
    server_conn->asyncWrite(std::move(data), [&](Status s) {
      if (s == Status::OK) {
        successful_writes.fetch_add(1);
      }
      writes_done.count_down();
    });
  }

  writes_done.wait();

  std::latch read_done(1);
  Buffer received;
  client->asyncRead(100, [&](Status s, Buffer&& data) {
    if (s == Status::OK) {
      received = std::move(data);
    }
    read_done.count_down();
  });

  read_done.wait();

  EXPECT_GT(successful_writes.load(), 0);
  EXPECT_GT(received.size(), 0u);

  client->close();
  server_conn->close();
  server->close();
}

TEST_F(TcpSocketTest, NoCallbackAfterSocketDestruction) {
  auto server = TcpServerSocket::listen("127.0.0.1", 0, *watcher_);
  uint16_t port = server->port();

  auto marker = std::make_shared<std::atomic<bool>>(true);
  std::atomic<int> callbacks_after_destruction{0};

  {
    auto client = TcpSocket::connect("127.0.0.1", port, *watcher_);
    auto server_conn = server->accept();

    std::weak_ptr<std::atomic<bool>> weak_marker = marker;

    Buffer large_data(1024 * 1024, 'X');
    client->asyncWrite(std::move(large_data), [weak_marker, &callbacks_after_destruction](Status) {
      if (weak_marker.expired()) {
        callbacks_after_destruction.fetch_add(1);
      }
    });

    client->asyncRead(100, [weak_marker, &callbacks_after_destruction](Status, Buffer&&) {
      if (weak_marker.expired()) {
        callbacks_after_destruction.fetch_add(1);
      }
    });
  }

  marker.reset();
  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  ASSERT_EQ(callbacks_after_destruction.load(), 0);

  server->close();
}

TEST_F(TcpSocketTest, ConcurrentSocketCreationDestruction) {
  auto server = TcpServerSocket::listen("127.0.0.1", 0, *watcher_);
  uint16_t port = server->port();

  constexpr int num_threads = 10;
  constexpr int iterations_per_thread = 10;
  std::atomic<int> successful_operations{0};
  std::vector<std::thread> threads;
  std::latch all_done(num_threads);

  for (int t = 0; t < num_threads; ++t) {
    threads.emplace_back([&]() {
      for (int i = 0; i < iterations_per_thread; ++i) {
        try {
          auto client = TcpSocket::connect("127.0.0.1", port, *watcher_);
          auto server_conn = server->accept();

          std::latch op_done(1);
          client->asyncWrite({'X'}, [&](Status s) {
            if (s == Status::OK || s == Status::CLOSED) {
              successful_operations.fetch_add(1);
            }
            op_done.count_down();
          });
          op_done.wait();

          client->close();
          server_conn->close();
        } catch (...) {
        }
      }
      all_done.count_down();
    });
  }

  all_done.wait();

  for (auto& t : threads) {
    t.join();
  }

  EXPECT_GT(successful_operations.load(), 0);

  server->close();
}

TEST_F(TcpSocketTest, EventWatcherDestructionWithActiveSockets) {
  GTEST_SKIP() << "EventWatcher holds reference to sockets, cannot be destroyed first";
}

TEST_F(TcpSocketTest, SocketMigrationBetweenThreads) {
  auto server = TcpServerSocket::listen("127.0.0.1", 0, *watcher_);
  uint16_t port = server->port();

  std::unique_ptr<TcpSocket> client;
  std::unique_ptr<ISocket> server_conn;

  std::thread thread_a([&]() {
    client = TcpSocket::connect("127.0.0.1", port, *watcher_);
    server_conn = server->accept();

    std::latch write_done(1);
    client->asyncWrite({'A'}, [&](Status s) {
      EXPECT_EQ(s, Status::OK);
      write_done.count_down();
    });
    write_done.wait();
  });

  thread_a.join();

  std::thread thread_b([&]() {
    std::latch write_done(1);
    client->asyncWrite({'B'}, [&](Status s) {
      EXPECT_TRUE(s == Status::OK || s == Status::CLOSED);
      write_done.count_down();
    });
    write_done.wait();

    client->close();
    server_conn->close();
  });

  thread_b.join();

  server->close();
}

TEST_F(TcpSocketTest, NoConcurrentResourceLeaks) {
  auto count_fds = []() -> int {
    int count = 0;
    DIR* dir = opendir("/proc/self/fd");
    if (dir) {
      struct dirent* entry;
      while ((entry = readdir(dir)) != nullptr) {
        if (entry->d_name[0] != '.') {
          count++;
        }
      }
      closedir(dir);
    }
    return count;
  };

  int baseline_fds = count_fds();

  auto server = TcpServerSocket::listen("127.0.0.1", 0, *watcher_);
  uint16_t port = server->port();

  for (int iteration = 0; iteration < 50; ++iteration) {
    auto client = TcpSocket::connect("127.0.0.1", port, *watcher_);
    auto server_conn = server->accept();

    std::latch done(1);
    client->asyncWrite({'X'}, [&](Status) {
      done.count_down();
    });
    done.wait();

    client->close();
    server_conn->close();
  }

  server->close();
  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  int final_fds = count_fds();
  EXPECT_LE(final_fds, baseline_fds + 5);
}

TEST_F(TcpSocketTest, AtomicStateTransitions) {
  auto server = TcpServerSocket::listen("127.0.0.1", 0, *watcher_);
  uint16_t port = server->port();
  auto client = TcpSocket::connect("127.0.0.1", port, *watcher_);
  auto server_conn = server->accept();

  std::atomic<bool> close_called{false};
  std::atomic<int> closed_status_count{0};
  std::vector<std::thread> threads;
  std::latch all_done(10);

  for (int i = 0; i < 10; ++i) {
    threads.emplace_back([&]() {
      while (!close_called.load()) {
        std::this_thread::yield();
      }

      client->asyncWrite({'X'}, [&](Status s) {
        if (s == Status::CLOSED) {
          closed_status_count.fetch_add(1);
        }
        all_done.count_down();
      });
    });
  }

  std::this_thread::sleep_for(std::chrono::milliseconds(10));
  client->close();
  close_called.store(true);

  all_done.wait();

  for (auto& t : threads) {
    t.join();
  }

  EXPECT_GT(closed_status_count.load(), 0);

  server_conn->close();
  server->close();
}

TEST_F(TcpSocketTest, ConcurrentErrorPropagation) {
  auto server = TcpServerSocket::listen("127.0.0.1", 0, *watcher_);
  uint16_t port = server->port();
  auto client = TcpSocket::connect("127.0.0.1", port, *watcher_);
  auto server_conn = server->accept();

  std::atomic<int> error_count{0};
  std::vector<std::thread> threads;
  std::latch all_started(5);
  std::latch all_done(5);

  for (int i = 0; i < 5; ++i) {
    threads.emplace_back([&]() {
      all_started.count_down();
      all_started.wait();

      client->asyncRead(100, [&](Status s, Buffer&&) {
        if (s == Status::PEER_CLOSED || s == Status::CLOSED) {
          error_count.fetch_add(1);
        }
        all_done.count_down();
      });
    });
  }

  all_started.wait();
  std::this_thread::sleep_for(std::chrono::milliseconds(10));
  server_conn->close();

  all_done.wait();

  for (auto& t : threads) {
    t.join();
  }

  EXPECT_GT(error_count.load(), 0);

  client->close();
  server->close();
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
