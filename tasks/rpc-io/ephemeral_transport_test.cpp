#include "ephemeral_transport.hpp"
#include <folly/experimental/coro/GtestHelpers.h>
#include <folly/experimental/coro/Sleep.h>
#include <folly/experimental/coro/Task.h>
#include <gtest/gtest.h>
#include <chrono>
#include <latch>
#include <mutex>
#include <vector>

#include "folly/init/Init.h"

using namespace getrafty::rpc::io;
using namespace std::chrono_literals;


class EphemeralSocketTest : public ::testing::Test {
};

CO_TEST_F(EphemeralSocketTest, JustWorks) {
  auto broker = std::make_shared<Broker>(/*buff_size=1MB*/);

  const auto serverSocket = std::make_shared<EphemeralServerSocket>("127.0.0.1:8080", broker);
  serverSocket->start();

  std::shared_ptr<IClientSocket> conn = nullptr;
  std::thread accept([&] {
    conn = folly::coro::blockingWait(serverSocket->accept());
  });

  const auto clientSocket = std::make_shared<EphemeralClientSocket>("127.0.0.1:8080", broker);
  clientSocket->connect();


  accept.join();
  CO_ASSERT_NE(conn, nullptr);

  co_await clientSocket->send("{request}");
  co_await folly::coro::sleep(100ms);

  const auto m = co_await conn->recv();
  EXPECT_EQ(m, "{request}");


  co_await conn->send("{response}");

  const auto m2 = co_await clientSocket->recv();
  EXPECT_EQ(m2, "{response}");

  conn->disconnect();
  clientSocket->disconnect();
  serverSocket->stop();

  co_return;
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  folly::Init init(&argc, &argv);
  return RUN_ALL_TESTS();
}
