#include <benchmark/benchmark.h>

#include <atomic>
#include <bits/algo.hpp>
#include <bits/ttl/ttl.hpp>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <latch>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "event_watcher.hpp"
#include "framed_transport.hpp"
#include "socket.hpp"
#include "tcp_transport.hpp"
#include "transport.hpp"

constexpr uint8_t kPayloadFillByte = 0xAB;

using namespace getrafty::io;
using namespace getrafty::rpc;

std::shared_ptr<Socket> createSocket(const Address& address,
                                     EventWatcher& watcher) {
  auto transport = std::make_unique<FramedTransport>(
      std::make_unique<TcpTransport>(address));
  return std::make_shared<Socket>(watcher, std::move(transport));
}

class EchoServer final : public std::enable_shared_from_this<EchoServer> {
 public:
  explicit EchoServer(const Address& addr, EventWatcher& ew)
      : socket_(createSocket(addr, ew)) {};

  void start() {
    if (running_.exchange(true, std::memory_order_relaxed)) {
      return;
    }

    IOStatus status = IOStatus::Fatal;
    std::latch latch(1);
    socket_->bind([&](IOStatus s, const Address&) {
      status = s;
      latch.count_down();
    });
    latch.wait();

    if (status != IOStatus::Ok) {
      benchmark::Shutdown();
    }
  }

  void stop() { running_.store(false, std::memory_order_relaxed); }

  void run(std::latch& latch) {
    if (!running_.load(std::memory_order_relaxed)) {
      latch.count_down();
      return;
    }

    auto self = shared_from_this();
    socket_->read([self, &latch](IOStatus status, Buffer&& data, Peer peer) {
      if (status != IOStatus::Ok) {
        self->running_.store(false, std::memory_order_relaxed);
        latch.count_down();
        return;
      }

      self->socket_->write(
          std::move(data), std::move(peer), [self, &latch](IOStatus s) {
            if (s != IOStatus::Ok) {
              self->running_.store(false, std::memory_order_relaxed);
            }
            latch.count_down();
          });
    });
  }

  std::atomic<bool> running_{false};
  std::shared_ptr<Socket> socket_;
};

class EchoClient : public std::enable_shared_from_this<EchoClient> {
 public:
  explicit EchoClient(const Address& addr, EventWatcher& ew)
      : socket_(createSocket(addr, ew)) {};

  void start() {
    if (running_.exchange(true, std::memory_order_relaxed)) {
      return;
    }

    IOStatus status = IOStatus::Fatal;
    std::latch latch(1);
    socket_->connect([&](IOStatus s) {
      status = s;
      latch.count_down();
    });
    latch.wait();

    if (status != IOStatus::Ok) {
      benchmark::Shutdown();
    }
  }

  void stop() { running_.store(false, std::memory_order_relaxed); }

  void run(Buffer&& payload, std::latch& latch) {
    if (!running_.load(std::memory_order_relaxed)) {
      latch.count_down();
      return;
    }

    auto self = shared_from_this();
    socket_->write(std::move(payload), {}, [self, &latch](IOStatus s) {
      if (s != IOStatus::Ok) {
        self->running_.store(false, std::memory_order_relaxed);
        latch.count_down();
        return;
      }

      self->socket_->read(
          [self, &latch](IOStatus s, Buffer&& data, const Peer&) {
            if (s != IOStatus::Ok) {
              self->running_.store(false, std::memory_order_relaxed);
            }
            benchmark::DoNotOptimize(data);
            latch.count_down();
          });
    });
  }

  std::atomic<bool> running_{false};
  std::shared_ptr<Socket> socket_;
};

static void BM_SocketPingPong(benchmark::State& state) {
  const auto payload_size = static_cast<size_t>(state.range(0));

  EventWatcher ew;
  auto server = std::make_shared<EchoServer>("127.0.0.1:5678", ew);
  auto client = std::make_shared<EchoClient>("127.0.0.1:5678", ew);

  server->start();
  client->start();

  Buffer baseline(payload_size, kPayloadFillByte);
  std::vector<double> samples;
  samples.reserve(state.iterations());

  for (auto _ : state) {
    std::latch server_latch(1);
    std::latch client_latch(1);

    auto start = std::chrono::steady_clock::now();

    server->run(server_latch);
    client->run(Buffer(baseline), client_latch);

    server_latch.wait();
    client_latch.wait();

    auto elapsed = std::chrono::steady_clock::now() - start;
    auto elapsed_us =
        std::chrono::duration_cast<std::chrono::microseconds>(elapsed).count();
    samples.push_back(elapsed_us);
  }

  server->stop();
  client->stop();

  state.counters["p50(us)"]   = bits::Histogram<50, 100>()(samples);
  state.counters["p99(us)"]   = bits::Histogram<99, 100>()(samples);
  state.counters["p99.9(us)"] = bits::Histogram<999, 1000>()(samples);
  state.counters["p100(us)"]  = bits::Histogram<100, 100>()(samples);
  state.counters["rps"] =
      benchmark::Counter(state.iterations(), benchmark::Counter::kIsRate);
}

BENCHMARK(BM_SocketPingPong)
    ->ArgName("bytes")
    ->Arg(64)
    ->Arg(512)
    ->Arg(4096)
    ->Arg(65536)
    ->Arg(262144)
    ->Arg(524288)
    ->Unit(benchmark::kMicrosecond)
    ->Iterations(10000)
    ->Repetitions(10)
    ->ReportAggregatesOnly(true);

int main(int argc, char** argv) {
  bits::ttl::Ttl::init("discard://");
  benchmark::Initialize(&argc, argv);
  if (benchmark::ReportUnrecognizedArguments(argc, argv)) {
    bits::ttl::Ttl::shutdown();
    return 1;
  }
  benchmark::RunSpecifiedBenchmarks();
  bits::ttl::Ttl::shutdown();
  return 0;
}
