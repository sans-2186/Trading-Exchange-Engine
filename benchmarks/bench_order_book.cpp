// Throughput benchmarks for the core engine hot paths.
//
// Google Benchmark runs each case repeatedly until timing stabilizes and
// reports mean time per iteration. These are THROUGHPUT numbers — for
// latency percentiles (p50/p99/p999) see bench_latency_percentiles.cpp.
//
// Methodology notes:
//   - Book state is rebuilt outside the timed region wherever possible
//     (state.PauseTiming/ResumeTiming is avoided: it costs ~100ns itself
//     and would swamp operations in the tens of nanoseconds).
//   - benchmark::DoNotOptimize prevents the compiler from deleting work
//     whose results are never read.

#include <benchmark/benchmark.h>

#include <cstdint>
#include <random>
#include <vector>

#include "mercury/core/matching_engine.hpp"
#include "mercury/core/order.hpp"
#include "mercury/core/order_book.hpp"

namespace {

using namespace mercury;

Order make_limit(uint64_t id, Side side, int64_t price, uint64_t qty) {
    return Order{
        .id        = OrderId{id},
        .side      = side,
        .type      = OrderType::Limit,
        .price     = Price{price},
        .quantity  = Quantity{qty},
        .filled    = Quantity{0},
        .timestamp = id,
    };
}

// Builds a book with `levels` price levels per side, `orders_per_level` each.
void fill_book(OrderBook& book, int levels, int orders_per_level, uint64_t& next_id) {
    for (int lv = 0; lv < levels; ++lv) {
        for (int i = 0; i < orders_per_level; ++i) {
            book.add_order(make_limit(next_id++, Side::Buy,  4999 - lv, 100));
            book.add_order(make_limit(next_id++, Side::Sell, 5001 + lv, 100));
        }
    }
}

}  // namespace

// ── OrderBook: add into an existing level (the common case) ──────────────────

static void BM_AddOrder_ExistingLevel(benchmark::State& state) {
    OrderBook book;
    uint64_t id = 1;
    fill_book(book, 10, 5, id);

    for (auto _ : state) {
        book.add_order(make_limit(id, Side::Buy, 4995, 100));
        benchmark::DoNotOptimize(book);
        ++id;
    }
}
BENCHMARK(BM_AddOrder_ExistingLevel);

// ── OrderBook: add creating a brand-new level each time ──────────────────────

static void BM_AddOrder_NewLevel(benchmark::State& state) {
    OrderBook book;
    uint64_t id = 1;
    int64_t price = 1'000'000;  // walk downward so every insert is a new level

    for (auto _ : state) {
        book.add_order(make_limit(id, Side::Buy, price, 100));
        benchmark::DoNotOptimize(book);
        ++id;
        --price;
    }
}
BENCHMARK(BM_AddOrder_NewLevel);

// ── OrderBook: cancel (the O(1) claim under test) ─────────────────────────────

static void BM_CancelOrder(benchmark::State& state) {
    // Pre-create a large pool of resting orders, then cancel one per iteration.
    // If we run out, refill — refill time pollutes a tiny fraction of iterations
    // but keeps the common path clean.
    OrderBook book;
    uint64_t next_id = 1;
    std::vector<uint64_t> pending;

    auto refill = [&] {
        pending.clear();
        for (int i = 0; i < 100'000; ++i) {
            book.add_order(make_limit(next_id, Side::Buy, 4000 + (i % 500), 100));
            pending.push_back(next_id);
            ++next_id;
        }
    };
    refill();

    size_t cursor = 0;
    for (auto _ : state) {
        if (cursor == pending.size()) refill(), cursor = 0;
        benchmark::DoNotOptimize(book.cancel_order(OrderId{pending[cursor++]}));
    }
}
BENCHMARK(BM_CancelOrder);

// ── OrderBook: best bid/ask lookup ────────────────────────────────────────────

static void BM_BestBid(benchmark::State& state) {
    OrderBook book;
    uint64_t id = 1;
    fill_book(book, 50, 10, id);

    for (auto _ : state) {
        benchmark::DoNotOptimize(book.best_bid());
    }
}
BENCHMARK(BM_BestBid);

// ── MatchingEngine: submit with no match (rests in book) ─────────────────────

static void BM_Submit_RestNoMatch(benchmark::State& state) {
    MatchingEngine engine;
    uint64_t id = 1;
    int64_t price = 1'000'000;

    for (auto _ : state) {
        // Bids walking downward never cross an empty ask side.
        auto trades = engine.submit_order(make_limit(id++, Side::Buy, price--, 100));
        benchmark::DoNotOptimize(trades);
    }
}
BENCHMARK(BM_Submit_RestNoMatch);

// ── MatchingEngine: submit with immediate full match ──────────────────────────

static void BM_Submit_FullMatch(benchmark::State& state) {
    MatchingEngine engine;
    uint64_t id = 1;

    for (auto _ : state) {
        // Each iteration: rest a sell, then a buy that consumes it exactly.
        // Measures a rest + a full match cycle; book returns to empty each time.
        (void)engine.submit_order(make_limit(id++, Side::Sell, 5000, 100));
        auto trades = engine.submit_order(make_limit(id++, Side::Buy, 5000, 100));
        benchmark::DoNotOptimize(trades);
    }
}
BENCHMARK(BM_Submit_FullMatch);

// ── MatchingEngine: market order sweeping multiple levels ─────────────────────

static void BM_Submit_MarketSweep5Levels(benchmark::State& state) {
    MatchingEngine engine;
    uint64_t id = 1;

    for (auto _ : state) {
        // Build 5 ask levels of 100 each, then sweep all of them with one market buy.
        for (int lv = 0; lv < 5; ++lv) {
            (void)engine.submit_order(make_limit(id++, Side::Sell, 5000 + lv, 100));
        }
        auto trades = engine.submit_order(Order{
            .id        = OrderId{id++},
            .side      = Side::Buy,
            .type      = OrderType::Market,
            .price     = Price{0},
            .quantity  = Quantity{500},
            .filled    = Quantity{0},
            .timestamp = id,
        });
        benchmark::DoNotOptimize(trades);
    }
}
BENCHMARK(BM_Submit_MarketSweep5Levels);
