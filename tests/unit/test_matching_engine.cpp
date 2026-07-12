#include <gtest/gtest.h>

#include "mercury/core/matching_engine.hpp"
#include "mercury/core/order.hpp"
#include "mercury/core/trade.hpp"
#include "mercury/core/types.hpp"

using namespace mercury;

// ── Helpers ───────────────────────────────────────────────────────────────────

static uint64_t next_id() {
    static uint64_t id = 1;
    return id++;
}

static Order make_limit(Side side, int64_t price_ticks, uint64_t qty) {
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

static Order make_market(Side side, uint64_t qty) {
    return Order{
        .id        = OrderId{next_id()},
        .side      = side,
        .type      = OrderType::Market,
        .price     = Price{0},  // Ignored for market orders.
        .quantity  = Quantity{qty},
        .filled    = Quantity{0},
        .timestamp = 0,
    };
}

// ── No match: orders rest in book ─────────────────────────────────────────────

TEST(MatchingEngineTest, LimitBuyWithNoSellsRestsInBook) {
    MatchingEngine engine;
    auto trades = engine.submit_order(make_limit(Side::Buy, 5000, 10));

    EXPECT_TRUE(trades.empty());
    EXPECT_EQ(engine.book().best_bid()->ticks, 5000);
}

TEST(MatchingEngineTest, LimitSellWithNoBuysRestsInBook) {
    MatchingEngine engine;
    auto trades = engine.submit_order(make_limit(Side::Sell, 5100, 10));

    EXPECT_TRUE(trades.empty());
    EXPECT_EQ(engine.book().best_ask()->ticks, 5100);
}

TEST(MatchingEngineTest, LimitBuyBelowBestAskRestsInBook) {
    MatchingEngine engine;
    (void)engine.submit_order(make_limit(Side::Sell, 5100, 10));  // ask at 5100

    auto trades = engine.submit_order(make_limit(Side::Buy, 5000, 10));  // bid at 5000 — no match
    EXPECT_TRUE(trades.empty());
    EXPECT_EQ(engine.book().bid_level_count(), 1u);
    EXPECT_EQ(engine.book().ask_level_count(), 1u);
}

// ── Full fill ─────────────────────────────────────────────────────────────────

TEST(MatchingEngineTest, ExactFullFillProducesOneTrade) {
    MatchingEngine engine;
    (void)engine.submit_order(make_limit(Side::Sell, 5000, 100));  // resting ask

    auto trades = engine.submit_order(make_limit(Side::Buy, 5000, 100));

    ASSERT_EQ(trades.size(), 1u);
    EXPECT_EQ(trades[0].quantity.value, 100u);
    EXPECT_EQ(trades[0].price.ticks,   5000);

    // Book should be completely empty after a perfect match.
    EXPECT_EQ(engine.book().bid_level_count(), 0u);
    EXPECT_EQ(engine.book().ask_level_count(), 0u);
}

TEST(MatchingEngineTest, TradeExecutesAtRestingPrice) {
    MatchingEngine engine;
    (void)engine.submit_order(make_limit(Side::Sell, 5000, 10));  // ask at 5000

    // Buyer willing to pay up to 5050 — matches at the resting ask price (5000), not 5050.
    auto trades = engine.submit_order(make_limit(Side::Buy, 5050, 10));

    ASSERT_EQ(trades.size(), 1u);
    EXPECT_EQ(trades[0].price.ticks, 5000);  // resting price, not incoming price
}

// ── Partial fill ──────────────────────────────────────────────────────────────

TEST(MatchingEngineTest, IncomingLargerThanRestingPartialFill) {
    MatchingEngine engine;
    (void)engine.submit_order(make_limit(Side::Sell, 5000, 30));  // resting: 30 available

    auto trades = engine.submit_order(make_limit(Side::Buy, 5000, 100));  // wants 100

    ASSERT_EQ(trades.size(), 1u);
    EXPECT_EQ(trades[0].quantity.value, 30u);  // only 30 filled

    // Remaining 70 should rest as a bid in the book.
    ASSERT_TRUE(engine.book().best_bid().has_value());
    EXPECT_EQ(engine.book().best_bid()->ticks, 5000);
    EXPECT_EQ(engine.book().bids().at(Price{5000}).total_quantity().value, 70u);

    // The resting sell should be gone.
    EXPECT_EQ(engine.book().ask_level_count(), 0u);
}

TEST(MatchingEngineTest, RestingLargerThanIncomingPartialFillOfResting) {
    MatchingEngine engine;
    (void)engine.submit_order(make_limit(Side::Sell, 5000, 100));  // resting: 100 available

    auto trades = engine.submit_order(make_limit(Side::Buy, 5000, 30));  // wants 30

    ASSERT_EQ(trades.size(), 1u);
    EXPECT_EQ(trades[0].quantity.value, 30u);

    // 70 should remain resting in the ask book.
    EXPECT_EQ(engine.book().asks().at(Price{5000}).total_quantity().value, 70u);

    // Incoming buy is fully filled — should not rest.
    EXPECT_EQ(engine.book().bid_level_count(), 0u);
}

// ── Book sweep: multiple levels ───────────────────────────────────────────────

TEST(MatchingEngineTest, BuySweeepsTwoPriceLevels) {
    MatchingEngine engine;
    (void)engine.submit_order(make_limit(Side::Sell, 5000, 50));  // level 1
    (void)engine.submit_order(make_limit(Side::Sell, 5100, 50));  // level 2

    // Buyer willing to pay up to 5100 — sweeps both levels.
    auto trades = engine.submit_order(make_limit(Side::Buy, 5100, 100));

    ASSERT_EQ(trades.size(), 2u);
    EXPECT_EQ(trades[0].price.ticks, 5000);   // best ask filled first
    EXPECT_EQ(trades[0].quantity.value, 50u);
    EXPECT_EQ(trades[1].price.ticks, 5100);
    EXPECT_EQ(trades[1].quantity.value, 50u);

    EXPECT_EQ(engine.book().ask_level_count(), 0u);
    EXPECT_EQ(engine.book().bid_level_count(), 0u);
}

TEST(MatchingEngineTest, BuyStopsAtPriceLimitDuringSweeep) {
    MatchingEngine engine;
    (void)engine.submit_order(make_limit(Side::Sell, 5000, 50));
    (void)engine.submit_order(make_limit(Side::Sell, 5200, 50));  // too expensive for buyer

    auto trades = engine.submit_order(make_limit(Side::Buy, 5100, 100));

    // Only the 5000 level should be consumed.
    ASSERT_EQ(trades.size(), 1u);
    EXPECT_EQ(trades[0].price.ticks, 5000);
    EXPECT_EQ(trades[0].quantity.value, 50u);

    // Remaining 50 should rest at 5100.
    EXPECT_EQ(engine.book().best_bid()->ticks, 5100);
    EXPECT_EQ(engine.book().ask_level_count(), 1u);  // 5200 level still there
}

// ── Market orders ─────────────────────────────────────────────────────────────

TEST(MatchingEngineTest, MarketBuyMatchesAtAnyAskPrice) {
    MatchingEngine engine;
    (void)engine.submit_order(make_limit(Side::Sell, 9999, 10));  // very high ask

    auto trades = engine.submit_order(make_market(Side::Buy, 10));

    ASSERT_EQ(trades.size(), 1u);
    EXPECT_EQ(trades[0].price.ticks, 9999);   // matched at whatever price was available
    EXPECT_EQ(trades[0].quantity.value, 10u);
}

TEST(MatchingEngineTest, MarketOrderRemainderDiscardedWhenBookInsufficient) {
    MatchingEngine engine;
    (void)engine.submit_order(make_limit(Side::Sell, 5000, 30));  // only 30 available

    auto trades = engine.submit_order(make_market(Side::Buy, 100));  // wants 100

    ASSERT_EQ(trades.size(), 1u);
    EXPECT_EQ(trades[0].quantity.value, 30u);

    // Market order remainder must NOT rest in the book.
    EXPECT_EQ(engine.book().bid_level_count(), 0u);
}

TEST(MatchingEngineTest, MarketBuyWithEmptyBookProducesNoTrades) {
    MatchingEngine engine;
    auto trades = engine.submit_order(make_market(Side::Buy, 100));
    EXPECT_TRUE(trades.empty());
    EXPECT_EQ(engine.book().bid_level_count(), 0u);  // nothing rested
}

// ── FIFO time priority within a price level ───────────────────────────────────

TEST(MatchingEngineTest, FIFOPriorityWithinPriceLevel) {
    MatchingEngine engine;

    auto first_sell  = make_limit(Side::Sell, 5000, 10);
    auto second_sell = make_limit(Side::Sell, 5000, 10);
    OrderId first_id  = first_sell.id;
    OrderId second_id = second_sell.id;

    (void)engine.submit_order(first_sell);   // arrives first
    (void)engine.submit_order(second_sell);  // arrives second

    // First buy fills the first (oldest) resting order.
    auto trades1 = engine.submit_order(make_limit(Side::Buy, 5000, 10));
    ASSERT_EQ(trades1.size(), 1u);
    EXPECT_EQ(trades1[0].sell_order_id.value, first_id.value);

    // Second buy fills the second resting order.
    auto trades2 = engine.submit_order(make_limit(Side::Buy, 5000, 10));
    ASSERT_EQ(trades2.size(), 1u);
    EXPECT_EQ(trades2[0].sell_order_id.value, second_id.value);
}

// ── Cancel ────────────────────────────────────────────────────────────────────

TEST(MatchingEngineTest, CancelRestingOrderPreventsMatch) {
    MatchingEngine engine;
    auto sell = make_limit(Side::Sell, 5000, 10);
    OrderId sell_id = sell.id;
    (void)engine.submit_order(sell);

    EXPECT_TRUE(engine.cancel_order(sell_id));

    auto trades = engine.submit_order(make_limit(Side::Buy, 5000, 10));
    EXPECT_TRUE(trades.empty());
    EXPECT_EQ(engine.book().bid_level_count(), 1u);  // buy rested — nothing to match
}
