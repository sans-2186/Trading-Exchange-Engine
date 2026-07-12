// Integration tests — multiple components (MatchingEngine + OrderBook +
// PriceLevel) exercised together through realistic multi-order scenarios.
// Unlike unit tests, these verify the *wiring* between components:
// full end-to-end state after a sequence of operations.

#include <gtest/gtest.h>

#include "mercury/core/matching_engine.hpp"
#include "mercury/core/order.hpp"
#include "mercury/core/trade.hpp"
#include "mercury/core/types.hpp"

using namespace mercury;

namespace {

uint64_t next_id() {
    static uint64_t id = 1000;
    return id++;
}

Order limit(Side side, int64_t price_ticks, uint64_t qty) {
    return Order{
        .id        = OrderId{next_id()},
        .side      = side,
        .type      = OrderType::Limit,
        .price     = Price{price_ticks},
        .quantity  = Quantity{qty},
        .filled    = Quantity{0},
        .timestamp = 0,
    };
}

Order market(Side side, uint64_t qty) {
    return Order{
        .id        = OrderId{next_id()},
        .side      = side,
        .type      = OrderType::Market,
        .price     = Price{0},
        .quantity  = Quantity{qty},
        .filled    = Quantity{0},
        .timestamp = 0,
    };
}

}  // namespace

// ── Scenario: build a book, trade through it, verify full state ───────────────

TEST(IntegrationTest, RealisticSessionEndToEnd) {
    MatchingEngine engine;

    // Market makers build a two-sided book:
    //   Bids: 5000 x 100, 4990 x 200
    //   Asks: 5010 x 150, 5020 x 300
    (void)engine.submit_order(limit(Side::Buy,  5000, 100));
    (void)engine.submit_order(limit(Side::Buy,  4990, 200));
    (void)engine.submit_order(limit(Side::Sell, 5010, 150));
    (void)engine.submit_order(limit(Side::Sell, 5020, 300));

    EXPECT_EQ(engine.book().best_bid()->ticks, 5000);
    EXPECT_EQ(engine.book().best_ask()->ticks, 5010);
    EXPECT_EQ(engine.book().spread()->ticks,   10);

    // Aggressive buyer takes out the entire first ask level and part of the second.
    // Wants 250: fills 150 @ 5010, then 100 @ 5020.
    auto trades = engine.submit_order(limit(Side::Buy, 5020, 250));

    ASSERT_EQ(trades.size(), 2u);
    EXPECT_EQ(trades[0].price.ticks, 5010);
    EXPECT_EQ(trades[0].quantity.value, 150u);
    EXPECT_EQ(trades[1].price.ticks, 5020);
    EXPECT_EQ(trades[1].quantity.value, 100u);

    // Book state after the sweep:
    //   Bids unchanged: 5000 x 100, 4990 x 200
    //   Asks: 5020 x 200 remaining (300 - 100)
    EXPECT_EQ(engine.book().best_bid()->ticks, 5000);
    EXPECT_EQ(engine.book().best_ask()->ticks, 5020);
    EXPECT_EQ(engine.book().asks().at(Price{5020}).total_quantity().value, 200u);
    EXPECT_EQ(engine.book().ask_level_count(), 1u);
    EXPECT_EQ(engine.book().bid_level_count(), 2u);

    // A market sell hits the bids: wants 150, fills 100 @ 5000 then 50 @ 4990.
    auto sell_trades = engine.submit_order(market(Side::Sell, 150));

    ASSERT_EQ(sell_trades.size(), 2u);
    EXPECT_EQ(sell_trades[0].price.ticks, 5000);
    EXPECT_EQ(sell_trades[0].quantity.value, 100u);
    EXPECT_EQ(sell_trades[1].price.ticks, 4990);
    EXPECT_EQ(sell_trades[1].quantity.value, 50u);

    // Final book:
    //   Bids: 4990 x 150 remaining
    //   Asks: 5020 x 200
    EXPECT_EQ(engine.book().best_bid()->ticks, 4990);
    EXPECT_EQ(engine.book().bids().at(Price{4990}).total_quantity().value, 150u);
    EXPECT_EQ(engine.book().best_ask()->ticks, 5020);
}

// ── Scenario: cancel interleaved with matching ────────────────────────────────

TEST(IntegrationTest, CancelAfterPartialFillRemovesOnlyRemainder) {
    MatchingEngine engine;

    auto resting = limit(Side::Sell, 5000, 100);
    OrderId resting_id = resting.id;
    (void)engine.submit_order(resting);

    // Partial fill: 40 of 100.
    auto trades = engine.submit_order(limit(Side::Buy, 5000, 40));
    ASSERT_EQ(trades.size(), 1u);
    EXPECT_EQ(trades[0].quantity.value, 40u);
    EXPECT_EQ(engine.book().asks().at(Price{5000}).total_quantity().value, 60u);

    // Cancel the remainder.
    EXPECT_TRUE(engine.cancel_order(resting_id));
    EXPECT_EQ(engine.book().ask_level_count(), 0u);

    // A new buy at that price now rests instead of matching.
    auto trades2 = engine.submit_order(limit(Side::Buy, 5000, 10));
    EXPECT_TRUE(trades2.empty());
    EXPECT_EQ(engine.book().best_bid()->ticks, 5000);
}

TEST(IntegrationTest, InterleavedAddsAndCancelsAtSameLevel) {
    MatchingEngine engine;

    auto a = limit(Side::Buy, 5000, 10);
    auto b = limit(Side::Buy, 5000, 20);
    auto c = limit(Side::Buy, 5000, 30);
    OrderId ida = a.id, idb = b.id, idc = c.id;

    (void)engine.submit_order(a);
    (void)engine.submit_order(b);
    (void)engine.submit_order(c);
    EXPECT_EQ(engine.book().bids().at(Price{5000}).total_quantity().value, 60u);

    // Cancel the middle order — FIFO order of the others must be preserved.
    EXPECT_TRUE(engine.cancel_order(idb));
    EXPECT_EQ(engine.book().bids().at(Price{5000}).total_quantity().value, 40u);

    // Incoming sell for 10 must fill order `a` (oldest), not `c`.
    auto trades = engine.submit_order(limit(Side::Sell, 5000, 10));
    ASSERT_EQ(trades.size(), 1u);
    EXPECT_EQ(trades[0].buy_order_id.value, ida.value);

    // Next sell fills `c`.
    auto trades2 = engine.submit_order(limit(Side::Sell, 5000, 30));
    ASSERT_EQ(trades2.size(), 1u);
    EXPECT_EQ(trades2[0].buy_order_id.value, idc.value);

    // Level is now empty and must be gone.
    EXPECT_EQ(engine.book().bid_level_count(), 0u);
}

// ── Edge cases ────────────────────────────────────────────────────────────────

TEST(IntegrationTest, ZeroQuantityOrderDoesNothing) {
    MatchingEngine engine;
    (void)engine.submit_order(limit(Side::Sell, 5000, 100));

    // Zero-quantity order: remaining() == 0, so it is already "fully filled".
    // It must produce no trades and must not rest.
    auto trades = engine.submit_order(limit(Side::Buy, 5000, 0));
    EXPECT_TRUE(trades.empty());
    EXPECT_EQ(engine.book().bid_level_count(), 0u);
    // The resting ask must be untouched.
    EXPECT_EQ(engine.book().asks().at(Price{5000}).total_quantity().value, 100u);
}

TEST(IntegrationTest, MarketOrderSweepsEntireBook) {
    MatchingEngine engine;
    (void)engine.submit_order(limit(Side::Sell, 5000, 10));
    (void)engine.submit_order(limit(Side::Sell, 5010, 20));
    (void)engine.submit_order(limit(Side::Sell, 5020, 30));

    // Market buy for more than total book depth (60).
    auto trades = engine.submit_order(market(Side::Buy, 100));

    ASSERT_EQ(trades.size(), 3u);
    EXPECT_EQ(trades[0].quantity.value, 10u);
    EXPECT_EQ(trades[1].quantity.value, 20u);
    EXPECT_EQ(trades[2].quantity.value, 30u);

    // Entire ask side consumed; nothing rested on the bid side.
    EXPECT_EQ(engine.book().ask_level_count(), 0u);
    EXPECT_EQ(engine.book().bid_level_count(), 0u);
}

TEST(IntegrationTest, TradeConservationAcrossSession) {
    // Property: total quantity traded on the buy side == sell side, always.
    // Every Trade has one buyer and one seller for the same quantity, so
    // summing trade quantities from both perspectives must agree.
    MatchingEngine engine;

    (void)engine.submit_order(limit(Side::Sell, 5000, 75));
    (void)engine.submit_order(limit(Side::Sell, 5010, 25));

    auto t1 = engine.submit_order(limit(Side::Buy, 5010, 60));
    auto t2 = engine.submit_order(limit(Side::Buy, 5010, 40));

    uint64_t total_traded = 0;
    for (const auto& t : t1) total_traded += t.quantity.value;
    for (const auto& t : t2) total_traded += t.quantity.value;

    // 100 was offered, 100 was demanded at compatible prices — all must trade.
    EXPECT_EQ(total_traded, 100u);
    EXPECT_EQ(engine.book().ask_level_count(), 0u);
    EXPECT_EQ(engine.book().bid_level_count(), 0u);
}
