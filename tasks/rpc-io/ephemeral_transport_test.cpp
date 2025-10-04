
#include <folly/experimental/coro/GtestHelpers.h>
#include <folly/experimental/coro/Task.h>
#include <gtest/gtest.h>
#include <chrono>
#include "folly/init/Init.h"

#include "ephemeral_transport.hpp"

#include "folly/MPMCQueue.h"
#include "folly/Random.h"
#include "folly/coro/AsyncScope.h"
#include "folly/coro/Collect.h"
#include "folly/executors/CPUThreadPoolExecutor.h"

using namespace getrafty::rpc::io;
using namespace std::chrono_literals;

struct Struct {
  uint32_t client_id;
  uint32_t request_id;
};

class EphemeralSocketTest : public ::testing::Test {
 public:
  void SetUp() override {
    broker = std::make_shared<Broker>(/*packet_drop=0*/);
    server_sock =
        std::make_shared<EphemeralServerSocket>("127.0.0.1:8080", broker);
    client_sock =
        std::make_shared<EphemeralClientSocket>("127.0.0.1:8080", broker);
  }

  [[nodiscard]] auto coScheduleAccept() const {
    return server_sock->accept()
        .scheduleOn(folly::getGlobalCPUExecutor())
        .start();
  }

 protected:
  std::shared_ptr<Broker> broker;
  std::shared_ptr<EphemeralServerSocket> server_sock;
  std::shared_ptr<EphemeralClientSocket> client_sock;
};

CO_TEST_F(EphemeralSocketTest, JustWorks) {
  server_sock->start();

  // Schedule accept
  auto accept_future = coScheduleAccept();

  // Attempt to connect (blocking)
  client_sock->connect();

  // Ensure connected
  const auto connected_sock = co_await std::move(accept_future);
  CO_ASSERT_NE(connected_sock, nullptr);

  co_await client_sock->send("Request{}");
  // co_await folly::coro::sleep(100ms);

  const auto m = co_await connected_sock->recv();
  EXPECT_EQ(m, "Request{}");

  co_await connected_sock->send("Response{}");

  const auto m2 = co_await client_sock->recv();
  EXPECT_EQ(m2, "Response{}");

  connected_sock->disconnect();
  client_sock->disconnect();
  server_sock->stop();

  co_return;
}

CO_TEST_F(EphemeralSocketTest, LifetimeConnectedSocketDestructed) {
  server_sock->start();

  // Schedule accept
  auto accept_future = coScheduleAccept();

  // Attempt to connect (blocking)
  client_sock->connect();

  // Ensure connected
  auto connected_sock = co_await std::move(accept_future);
  EXPECT_TRUE(connected_sock && connected_sock->isConnected());

  connected_sock.reset();

  auto recv_try = co_await co_awaitTry(client_sock->recv());
  EXPECT_TRUE(recv_try.withException([&](const std::runtime_error& e) mutable {
    EXPECT_STRCASEEQ(e.what(), "connection lost");
  }));

  co_return;
}

CO_TEST_F(EphemeralSocketTest, LifetimeServerDestructedBeforeClient) {
  server_sock->start();

  // Schedule accept
  auto accept_future = coScheduleAccept();

  // Attempt to connect (blocking)
  client_sock->connect();

  // Ensure connected
  const auto connected_sock = co_await std::move(accept_future);
  EXPECT_TRUE(connected_sock && connected_sock->isConnected());

  server_sock.reset();

  EXPECT_FALSE(connected_sock->isConnected());

  co_return;
}

CO_TEST_F(EphemeralSocketTest, ClientDisconnectJustAfterAccept) {
  server_sock->start();

  // Schedule accept
  auto accept_future = coScheduleAccept();

  // Attempt to connect (blocking)
  client_sock->connect();

  // Ensure connected
  const auto connected_sock = co_await std::move(accept_future);
  CO_ASSERT_NE(connected_sock, nullptr);

  // Client disconnect
  client_sock->disconnect();

  auto recv_try = co_await co_awaitTry(connected_sock->recv());

  EXPECT_TRUE(recv_try.withException([&](const std::runtime_error& e) mutable {
    EXPECT_STRCASEEQ(e.what(), "connection lost");
  }));

  auto send_try = co_await co_awaitTry(connected_sock->send("{}"));
  EXPECT_TRUE(send_try.withException([&](const std::runtime_error& e) mutable {
    EXPECT_STRCASEEQ(e.what(), "connection lost");
  }));

  co_return;
}

CO_TEST_F(EphemeralSocketTest, ServerDisconnectJustAfterAccept) {
  server_sock->start();

  // Schedule accept
  auto accept_future = coScheduleAccept();

  // Attempt to connect (blocking)
  client_sock->connect();

  // Ensure connected
  const auto connected_sock = co_await std::move(accept_future);
  CO_ASSERT_NE(connected_sock, nullptr);

  // Server disconnect
  server_sock->stop();

  auto recv_try = co_await co_awaitTry(connected_sock->recv());
  EXPECT_TRUE(recv_try.withException([&](const std::runtime_error& e) mutable {
    EXPECT_STRCASEEQ(e.what(), "connection closed");
  }));

  auto send_try = co_await co_awaitTry(client_sock->send("Request{}"));
  EXPECT_TRUE(send_try.withException([&](const std::runtime_error& e) mutable {
    EXPECT_STRCASEEQ(e.what(), "connection lost");
  }));

  co_return;
}

CO_TEST_F(EphemeralSocketTest, MultipleConcurrentClients) {
  constexpr size_t kNumClients = 50;
  constexpr size_t kNumHops = 20;

  // connect can block thus leave extra thread for accept coro to process connection messages
  const auto exec =
      std::make_shared<folly::CPUThreadPoolExecutor>(kNumClients + 1);

  server_sock->start();
  std::vector<folly::SemiFuture<folly::Unit>> servers;
  servers.reserve(kNumClients);
  for (int i = 0; i < kNumClients; ++i) {
    auto task = folly::coro::co_invoke([this]() -> folly::coro::Task<> {
      const auto sock = co_await server_sock->accept();

      std::array<size_t, kNumHops> hops_freq{};
      for (auto j = 0; j < kNumHops; ++j) {
        const auto msg = co_await sock->recv();
        Struct s{};
        std::memcpy(&s, msg.data(), sizeof(Struct));
        EXPECT_EQ(++hops_freq[s.request_id], 1);
        co_await folly::coro::sleep(
            std::chrono::milliseconds(folly::Random::rand32(100)));  // jitter
        co_await sock->send(msg);
      }

      sock->disconnect();
      co_return;
    });
    servers.emplace_back(std::move(task).scheduleOn(exec.get()).start());
  }

  std::vector<folly::SemiFuture<folly::Unit>> clients;
  clients.reserve(kNumClients);
  for (uint32_t i = 0; i < kNumClients; ++i) {
    auto task =
        folly::coro::co_invoke([this, client_id = i]() -> folly::coro::Task<> {
          const auto client =
              std::make_shared<EphemeralClientSocket>("127.0.0.1:8080", broker);
          client->connect();

          std::array<size_t, kNumHops> hops_freq{};
          for (uint32_t j = 0; j < kNumHops; ++j) {

            Struct request{.client_id = client_id, .request_id = j};
            co_await client->send(std::string{
                reinterpret_cast<const char*>(&request), sizeof(Struct)});

            co_await folly::coro::sleep(std::chrono::milliseconds(
                folly::Random::rand32(100)));  // jitter

            const auto msg = co_await client->recv();
            Struct response{};
            std::memcpy(&response, msg.data(), sizeof(Struct));
            EXPECT_EQ(response.client_id, request.client_id);
            EXPECT_EQ(++hops_freq[response.request_id], 1);
          }
          client->disconnect();
          co_return;
        });
    clients.emplace_back(std::move(task).scheduleOn(exec.get()).start());
  }

  co_await folly::coro::collectAllRange(std::move(servers));
  co_await folly::coro::collectAllRange(std::move(clients));
  co_return;
}

int main(int argc, char** argv) {
  testing::InitGoogleTest(&argc, argv);
  folly::Init init(&argc, &argv);
  return RUN_ALL_TESTS();
}
