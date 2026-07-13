// Allocation counter — verifies the "allocations cause the tail" hypothesis
// with hard evidence before we optimize anything.
//
// Technique: override global operator new/delete with versions that bump a
// thread-local counter. Reset the counter before an engine call, read it
// after — the delta is the exact number of heap allocations that call made.
//
// This is a diagnostic tool, not a benchmark: it measures COUNTS, not time.

#include <benchmark/benchmark.h>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <new>
#include <random>
#include <vector>

#include "mercury/core/matching_engine.hpp"
#include "mercury/core/order.hpp"

// ── Global allocation counters ────────────────────────────────────────────────

namespace {
thread_local std::uint64_t g_alloc_count = 0;
thread_local std::uint64_t g_free_count  = 0;
}  // namespace

void* operator new(std::size_t size) {
    ++g_alloc_count;
    if (void* p = std::malloc(size)) return p;
    throw std::bad_alloc{};
}

void operator delete(void* p) noexcept {
    ++g_free_count;
    std::free(p);
}

void operator delete(void* p, std::size_t) noexcept {
    ++g_free_count;
    std::free(p);
}

// ── Diagnostic run ────────────────────────────────────────────────────────────

namespace {

using namespace mercury;

struct AllocStats {
    std::uint64_t total = 0;
    std::uint64_t max_single = 0;
    std::uint64_t calls = 0;
    std::uint64_t zero_alloc_calls = 0;
};

void report(const char* label, const AllocStats& s) {
    std::printf("  %-24s calls=%-7llu  total_allocs=%-8llu  avg=%.2f  max_in_one_call=%llu  zero-alloc_calls=%llu (%.1f%%)\n",
                label,
                static_cast<unsigned long long>(s.calls),
                static_cast<unsigned long long>(s.total),
                s.calls ? static_cast<double>(s.total) / static_cast<double>(s.calls) : 0.0,
                static_cast<unsigned long long>(s.max_single),
                static_cast<unsigned long long>(s.zero_alloc_calls),
                s.calls ? 100.0 * static_cast<double>(s.zero_alloc_calls) /
                              static_cast<double>(s.calls)
                        : 0.0);
}

static void BM_AllocationCount(benchmark::State& state) {
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

        AllocStats submit_stats;
        AllocStats cancel_stats;

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

                const auto before = g_alloc_count;
                auto trades = engine.submit_order(std::move(order));
                const auto delta = g_alloc_count - before;
                benchmark::DoNotOptimize(trades);

                submit_stats.total += delta;
                submit_stats.calls += 1;
                if (delta == 0) submit_stats.zero_alloc_calls += 1;
                if (delta > submit_stats.max_single) submit_stats.max_single = delta;

                if (engine.book().contains(id)) live.push_back(id);
            } else {
                std::uniform_int_distribution<size_t> pick(0, live.size() - 1);
                const size_t idx = pick(rng);
                const OrderId id = live[idx];

                const auto before = g_alloc_count;
                benchmark::DoNotOptimize(engine.cancel_order(id));
                const auto delta = g_alloc_count - before;

                cancel_stats.total += delta;
                cancel_stats.calls += 1;
                if (delta == 0) cancel_stats.zero_alloc_calls += 1;
                if (delta > cancel_stats.max_single) cancel_stats.max_single = delta;

                live.erase(live.begin() + static_cast<std::ptrdiff_t>(idx));
            }
        }

        state.PauseTiming();
        std::printf("\nHeap allocations per engine call (mixed workload, %d ops):\n", kOps);
        report("submit_order", submit_stats);
        report("cancel_order", cancel_stats);
        std::printf("\n");
        state.ResumeTiming();
    }
}
BENCHMARK(BM_AllocationCount)->Iterations(1)->Unit(benchmark::kMillisecond);

}  // namespace
