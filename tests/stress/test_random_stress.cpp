// Randomized stress tests — throw thousands of random operations at the
// engine and verify system-wide invariants after every single step.
//
// Philosophy: unit tests check the cases we thought of. Randomized tests
// find the cases we did not. A fixed seed keeps failures reproducible.
//
// Invariants checked after EVERY operation:
//   1. No crossed book: best_bid < best_ask whenever both sides exist.
//      (A crossed book means the engine missed a match — correctness bug.)
//   2. Cancel bookkeeping: an ID cancelled once can never cancel again.
//   3. Level integrity: every level reachable from the maps is non-empty
//      and its cached total equals the sum of its orders' remainders.

#include <gtest/gtest.h>

#include <cstdint>
#include <random>
#include <vector>

#include "mercury/core/matching_engine.hpp"
#include "mercury/core/order.hpp"
#include "mercury/core/types.hpp"

using namespace mercury;

namespace {

// Verifies all structural invariants of the book. Called after every operation.
void assert_book_invariants(const OrderBook& book) {
    // Invariant 1: never crossed.
    auto bid = book.best_bid();
    auto ask = book.best_ask();
    if (bid && ask) {
        ASSERT_LT(bid->ticks, ask->ticks) << "CROSSED BOOK: bid " << bid->ticks
                                          << " >= ask " << ask->ticks;
    }

    // Invariant 3: no empty levels; cached totals must match actual sums.
    for (const auto& [price, level] : book.bids()) {
        ASSERT_FALSE(level.empty()) << "Empty bid level at " << price.ticks;
        uint64_t sum = 0;
        for (const auto& o : level) sum += o.remaining().value;
        ASSERT_EQ(sum, level.total_quantity().value)
            << "Bid level total_quantity out of sync at " << price.ticks;
    }
    for (const auto& [price, level] : book.asks()) {
        ASSERT_FALSE(level.empty()) << "Empty ask level at " << price.ticks;
        uint64_t sum = 0;
        for (const auto& o : level) sum += o.remaining().value;
        ASSERT_EQ(sum, level.total_quantity().value)
            << "Ask level total_quantity out of sync at " << price.ticks;
    }
}

}  // namespace

TEST(StressTest, TenThousandRandomOrdersNeverViolateInvariants) {
    // Fixed seed — failures reproduce identically on every run.
    std::mt19937_64 rng(42);

    std::uniform_int_distribution<int>      op_dist(0, 9);       // operation selector
    std::uniform_int_distribution<int64_t>  price_dist(4900, 5100);  // 200-tick band
    std::uniform_int_distribution<uint64_t> qty_dist(1, 500);
    std::uniform_int_distribution<int>      side_dist(0, 1);

    MatchingEngine engine;
    std::vector<OrderId> live_ids;  // candidates for cancellation
    uint64_t next_order_id = 1;

    constexpr int kOperations = 10'000;

    for (int i = 0; i < kOperations; ++i) {
        const int op = op_dist(rng);

        if (op < 6 || live_ids.empty()) {
            // 60%: submit a limit order.
            Order order{
                .id        = OrderId{next_order_id++},
                .side      = side_dist(rng) == 0 ? Side::Buy : Side::Sell,
                .type      = OrderType::Limit,
                .price     = Price{price_dist(rng)},
                .quantity  = Quantity{qty_dist(rng)},
                .filled    = Quantity{0},
                .timestamp = static_cast<uint64_t>(i),
            };
            OrderId id = order.id;
            (void)engine.submit_order(std::move(order));
            // The order may have fully matched — only track if it actually rests.
            if (engine.book().contains(id)) {
                live_ids.push_back(id);
            }
        } else if (op < 8) {
            // 20%: submit a market order.
            Order order{
                .id        = OrderId{next_order_id++},
                .side      = side_dist(rng) == 0 ? Side::Buy : Side::Sell,
                .type      = OrderType::Market,
                .price     = Price{0},
                .quantity  = Quantity{qty_dist(rng)},
                .filled    = Quantity{0},
                .timestamp = static_cast<uint64_t>(i),
            };
            (void)engine.submit_order(std::move(order));
        } else {
            // 20%: cancel a random tracked order.
            std::uniform_int_distribution<size_t> pick(0, live_ids.size() - 1);
            const size_t idx = pick(rng);
            const OrderId id = live_ids[idx];

            const bool was_in_book = engine.book().contains(id);
            const bool cancelled   = engine.cancel_order(id);

            // Invariant 2: cancel succeeds iff the order was actually in the book.
            ASSERT_EQ(cancelled, was_in_book);

            live_ids.erase(live_ids.begin() + static_cast<std::ptrdiff_t>(idx));
        }

        assert_book_invariants(engine.book());
    }
}

TEST(StressTest, DoubleCancelAlwaysFailsSecondTime) {
    std::mt19937_64 rng(1234);
    std::uniform_int_distribution<int64_t>  price_dist(4950, 5050);
    std::uniform_int_distribution<uint64_t> qty_dist(1, 100);

    MatchingEngine engine;

    for (uint64_t i = 1; i <= 1'000; ++i) {
        Order order{
            .id        = OrderId{i},
            .side      = (i % 2 == 0) ? Side::Buy : Side::Sell,
            .type      = OrderType::Limit,
            .price     = Price{price_dist(rng)},
            .quantity  = Quantity{qty_dist(rng)},
            .filled    = Quantity{0},
            .timestamp = i,
        };
        (void)engine.submit_order(std::move(order));

        if (engine.book().contains(OrderId{i})) {
            EXPECT_TRUE(engine.cancel_order(OrderId{i}));
            EXPECT_FALSE(engine.cancel_order(OrderId{i}));  // second cancel must fail
        }
    }
}

TEST(StressTest, AlternatingCrossingOrdersDrainCompletely) {
    // Every pair of orders crosses exactly: the book must be empty after
    // each pair, and total traded quantity must equal total submitted.
    MatchingEngine engine;
    uint64_t total_traded = 0;
    constexpr uint64_t kPairs = 1'000;
    constexpr uint64_t kQty   = 10;

    for (uint64_t i = 0; i < kPairs; ++i) {
        (void)engine.submit_order(Order{
            .id        = OrderId{2 * i + 1},
            .side      = Side::Sell,
            .type      = OrderType::Limit,
            .price     = Price{5000},
            .quantity  = Quantity{kQty},
            .filled    = Quantity{0},
            .timestamp = 2 * i + 1,
        });
        auto trades = engine.submit_order(Order{
            .id        = OrderId{2 * i + 2},
            .side      = Side::Buy,
            .type      = OrderType::Limit,
            .price     = Price{5000},
            .quantity  = Quantity{kQty},
            .filled    = Quantity{0},
            .timestamp = 2 * i + 2,
        });

        for (const auto& t : trades) total_traded += t.quantity.value;

        ASSERT_EQ(engine.book().bid_level_count(), 0u);
        ASSERT_EQ(engine.book().ask_level_count(), 0u);
    }

    EXPECT_EQ(total_traded, kPairs * kQty);
}
