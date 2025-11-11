#include "queue.hpp"

#include <benchmark/benchmark.h>

#include <chrono>
#include <latch>
#include <thread>

using namespace getrafty::concurrent;

static void BM_MultipleProducers(benchmark::State& state) {
  const auto num_producers      = static_cast<size_t>(state.range(0));
  const auto items_per_producer = static_cast<size_t>(state.range(1));
  const auto total_items        = num_producers * items_per_producer;

  Queue<int> queue;

  for (auto _ : state) {
    std::latch start_latch(static_cast<ptrdiff_t>(num_producers + 1));

    std::vector<std::thread> producers;
    producers.reserve(num_producers);

    for (size_t p = 0; p < num_producers; ++p) {
      producers.emplace_back([&]() {
        start_latch.count_down();
        start_latch.wait();

        for (size_t i = 0; i < items_per_producer; ++i) {
          queue.push(static_cast<int>(i));
        }
      });
    }

    start_latch.count_down();
    start_latch.wait();

    for (auto& producer : producers) {
      producer.join();
    }

    auto start = std::chrono::steady_clock::now();

    size_t consumed = 0;
    while (consumed < total_items) {
      if (queue.tryTake()) {
        consumed++;
      }
    }

    auto elapsed = std::chrono::steady_clock::now() - start;
    auto ns =
        std::chrono::duration_cast<std::chrono::nanoseconds>(elapsed).count();
    state.counters["mean_latency_ns"] =
        static_cast<double>(ns) / static_cast<double>(total_items);
  }
}

BENCHMARK(BM_MultipleProducers)
    ->Args({1, 10000})
    ->Args({2, 10000})
    ->Args({4, 10000})
    ->Args({8, 10000})
    ->Args({16, 10000})
    ->Args({32, 10000})
    ->Args({64, 10000})
    ->ReportAggregatesOnly(true);

BENCHMARK_MAIN();
