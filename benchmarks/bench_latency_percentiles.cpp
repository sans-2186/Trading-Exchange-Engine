// Latency percentile report — the numbers averages hide.
//
// Google Benchmark reports mean time per operation (throughput view).
// This harness records EVERY individual call duration, sorts them, and
// reports p50 / p90 / p99 / p99.9 / max — the latency distribution view.
//
// Registered as a Google Benchmark "case" so it runs from the same binary,
// but it manages its own timing and prints its own report.

#include <benchmark/benchmark.h>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <random>
#include <vector>

#include "mercury/core/matching_engine.hpp"
#include "mercury/core/order.hpp"

namespace {

using namespace mercury;
using Clock = std::chrono::steady_clock;

void print_percentiles(const char* label, std::vector<int64_t>& ns) {
    std::sort(ns.begin(), ns.end());
    const auto at = [&](double p) {
        return ns[static_cast<size_t>(p * static_cast<double>(ns.size() - 1))];
    };
    std::printf("  %-28s n=%zu  p50=%5lldns  p90=%5lldns  p99=%5lldns  p99.9=%6lldns  max=%7lldns\n",
                label, ns.size(),
                static_cast<long long>(at(0.50)), static_cast<long long>(at(0.90)),
                static_cast<long long>(at(0.99)), static_cast<long long>(at(0.999)),
                static_cast<long long>(ns.back()));
}

// Runs a realistic mixed workload (like the stress test: 60% limit, 20%
// market, 20% cancel) and records per-call latency for submit_order.
static void BM_LatencyPercentiles(benchmark::State& state) {
    for (auto _ : state) {
        constexpr int kOps = 100'000;

        std::mt19937_64 rng(42);
        std::uniform_int_distribution<int>      op_dist(0, 9);
        std::uniform_int_distribution<int64_t>  price_dist(4900, 5100);
        std::uniform_int_distribution<uint64_t> qty_dist(1, 500);
        std::uniform_int_distribution<int>      side_dist(0, 1);

        MatchingEngine engine;
        std::vector<OrderId> live;
        uint64_t next_id = 1;

        std::vector<int64_t> submit_ns;
        std::vector<int64_t> cancel_ns;
        submit_ns.reserve(kOps);
        cancel_ns.reserve(kOps / 4);

        for (int i = 0; i < kOps; ++i) {
            const int op = op_dist(rng);

            if (op < 8 || live.empty()) {
                Order order{
                    .id        = OrderId{next_id++},
                    .side      = side_dist(rng) == 0 ? Side::Buy : Side::Sell,
                    .type      = op < 6 ? OrderType::Limit : OrderType::Market,
                    .price     = Price{price_dist(rng)},
                    .quantity  = Quantity{qty_dist(rng)},
                    .filled    = Quantity{0},
                    .timestamp = static_cast<uint64_t>(i),
                };
                const OrderId id = order.id;

                const auto t0 = Clock::now();
                auto trades = engine.submit_order(std::move(order));
                const auto t1 = Clock::now();
                benchmark::DoNotOptimize(trades);

                submit_ns.push_back(
                    std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count());
                if (engine.book().contains(id)) live.push_back(id);
            } else {
                std::uniform_int_distribution<size_t> pick(0, live.size() - 1);
                const size_t idx = pick(rng);
                const OrderId id = live[idx];

                const auto t0 = Clock::now();
                benchmark::DoNotOptimize(engine.cancel_order(id));
                const auto t1 = Clock::now();

                cancel_ns.push_back(
                    std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count());
                live.erase(live.begin() + static_cast<std::ptrdiff_t>(idx));
            }
        }

        state.PauseTiming();
        std::printf("\nLatency percentiles (mixed workload, %d ops):\n", kOps);
        print_percentiles("submit_order", submit_ns);
        print_percentiles("cancel_order", cancel_ns);
        std::printf("\n");
        state.ResumeTiming();
    }
}
BENCHMARK(BM_LatencyPercentiles)->Iterations(1)->Unit(benchmark::kMillisecond);

}  // namespace
