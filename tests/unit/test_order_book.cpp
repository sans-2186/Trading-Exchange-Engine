#include <gtest/gtest.h>

#include "mercury/core/order.hpp"
#include "mercury/core/order_book.hpp"
#include "mercury/core/types.hpp"

using namespace mercury;

// ── Helpers ──────────────────────────────────────────────────────────────────

static Order make_order(uint64_t id, Side side, int64_t price_ticks, uint64_t qty,
                        uint64_t timestamp = 0) {
    return Order{
        .id        = OrderId{id},
        .side      = side,
        .type      = OrderType::Limit,
        .price     = Price{price_ticks},
        .quantity  = Quantity{qty},
        .filled    = Quantity{0},
        .timestamp = timestamp,
    };
}

// ── Order struct tests ────────────────────────────────────────────────────────

TEST(OrderTest, RemainingQuantity) {
    Order o = make_order(1, Side::Buy, 5000, 100);
    EXPECT_EQ(o.remaining().value, 100u);

    o.filled.value = 40;
    EXPECT_EQ(o.remaining().value, 60u);
}

TEST(OrderTest, FullyFilled) {
    Order o = make_order(1, Side::Buy, 5000, 100);
    EXPECT_FALSE(o.is_fully_filled());

    o.filled.value = 100;
    EXPECT_TRUE(o.is_fully_filled());
}

// ── Empty book ────────────────────────────────────────────────────────────────

TEST(OrderBookTest, EmptyBookHasNoBestPrices) {
    OrderBook book;
    EXPECT_EQ(book.best_bid(), std::nullopt);
    EXPECT_EQ(book.best_ask(), std::nullopt);
    EXPECT_EQ(book.spread(),   std::nullopt);
}

TEST(OrderBookTest, EmptyBookContainsNoOrders) {
    OrderBook book;
    EXPECT_FALSE(book.contains(OrderId{1}));
}

// ── Add orders ────────────────────────────────────────────────────────────────

TEST(OrderBookTest, AddBidUpdatedBestBid) {
    OrderBook book;
    book.add_order(make_order(1, Side::Buy, 5000, 10));
    ASSERT_TRUE(book.best_bid().has_value());
    EXPECT_EQ(book.best_bid()->ticks, 5000);
}

TEST(OrderBookTest, AddAskUpdatesBestAsk) {
    OrderBook book;
    book.add_order(make_order(1, Side::Sell, 5100, 10));
    ASSERT_TRUE(book.best_ask().has_value());
    EXPECT_EQ(book.best_ask()->ticks, 5100);
}

TEST(OrderBookTest, BestBidIsHighestBuyPrice) {
    OrderBook book;
    book.add_order(make_order(1, Side::Buy, 4900, 10));
    book.add_order(make_order(2, Side::Buy, 5000, 10));  // higher — should be best
    book.add_order(make_order(3, Side::Buy, 4800, 10));

    EXPECT_EQ(book.best_bid()->ticks, 5000);
}

TEST(OrderBookTest, BestAskIsLowestSellPrice) {
    OrderBook book;
    book.add_order(make_order(1, Side::Sell, 5200, 10));
    book.add_order(make_order(2, Side::Sell, 5100, 10));  // lower — should be best
    book.add_order(make_order(3, Side::Sell, 5300, 10));

    EXPECT_EQ(book.best_ask()->ticks, 5100);
}

TEST(OrderBookTest, SpreadIsAskMinusBid) {
    OrderBook book;
    book.add_order(make_order(1, Side::Buy,  5000, 10));
    book.add_order(make_order(2, Side::Sell, 5100, 10));

    ASSERT_TRUE(book.spread().has_value());
    EXPECT_EQ(book.spread()->ticks, 100);
}

TEST(OrderBookTest, SpreadNulloptWhenOneSideEmpty) {
    OrderBook book;
    book.add_order(make_order(1, Side::Buy, 5000, 10));
    EXPECT_EQ(book.spread(), std::nullopt);  // no asks yet
}

// ── Price-level grouping ──────────────────────────────────────────────────────

TEST(OrderBookTest, MultipleOrdersAtSamePriceLevelOnOneBidLevel) {
    OrderBook book;
    book.add_order(make_order(1, Side::Buy, 5000, 10));
    book.add_order(make_order(2, Side::Buy, 5000, 20));

    // Only one price level at 5000
    EXPECT_EQ(book.bid_level_count(), 1u);
    EXPECT_EQ(book.bids().at(Price{5000}).total_quantity().value, 30u);
}

// ── Cancel orders ─────────────────────────────────────────────────────────────

TEST(OrderBookTest, CancelKnownOrderReturnsTrue) {
    OrderBook book;
    book.add_order(make_order(1, Side::Buy, 5000, 10));
    EXPECT_TRUE(book.cancel_order(OrderId{1}));
}

TEST(OrderBookTest, CancelUnknownOrderReturnsFalse) {
    OrderBook book;
    EXPECT_FALSE(book.cancel_order(OrderId{99}));
}

TEST(OrderBookTest, CancelledOrderNotInBook) {
    OrderBook book;
    book.add_order(make_order(1, Side::Buy, 5000, 10));
    book.cancel_order(OrderId{1});
    EXPECT_FALSE(book.contains(OrderId{1}));
}

TEST(OrderBookTest, CancelLastOrderAtLevelRemovesLevel) {
    OrderBook book;
    book.add_order(make_order(1, Side::Buy, 5000, 10));
    book.cancel_order(OrderId{1});

    // The price level itself should be gone — not just empty
    EXPECT_EQ(book.bid_level_count(), 0u);
    EXPECT_EQ(book.best_bid(), std::nullopt);
}

TEST(OrderBookTest, CancelOneOrderAtLevelLeavesOthers) {
    OrderBook book;
    book.add_order(make_order(1, Side::Buy, 5000, 10));
    book.add_order(make_order(2, Side::Buy, 5000, 20));
    book.cancel_order(OrderId{1});

    EXPECT_TRUE(book.contains(OrderId{2}));
    EXPECT_EQ(book.bids().at(Price{5000}).total_quantity().value, 20u);
    EXPECT_EQ(book.bid_level_count(), 1u);
}

TEST(OrderBookTest, CancelDoesNotAffectOtherSide) {
    OrderBook book;
    book.add_order(make_order(1, Side::Buy,  5000, 10));
    book.add_order(make_order(2, Side::Sell, 5100, 10));
    book.cancel_order(OrderId{1});

    EXPECT_EQ(book.best_bid(), std::nullopt);
    EXPECT_EQ(book.best_ask()->ticks, 5100);
}

// ── Duplicate cancel (idempotency) ────────────────────────────────────────────

TEST(OrderBookTest, DoubleCancelReturnsFalseSecondTime) {
    OrderBook book;
    book.add_order(make_order(1, Side::Buy, 5000, 10));
    EXPECT_TRUE(book.cancel_order(OrderId{1}));
    EXPECT_FALSE(book.cancel_order(OrderId{1}));  // already gone
}

// ── Multiple levels ───────────────────────────────────────────────────────────

TEST(OrderBookTest, BestBidUpdatesAfterTopLevelCancelled) {
    OrderBook book;
    book.add_order(make_order(1, Side::Buy, 5000, 10));  // best
    book.add_order(make_order(2, Side::Buy, 4900, 10));  // second best

    book.cancel_order(OrderId{1});
    EXPECT_EQ(book.best_bid()->ticks, 4900);
}

TEST(OrderBookTest, BestAskUpdatesAfterTopLevelCancelled) {
    OrderBook book;
    book.add_order(make_order(1, Side::Sell, 5100, 10));  // best
    book.add_order(make_order(2, Side::Sell, 5200, 10));  // second best

    book.cancel_order(OrderId{1});
    EXPECT_EQ(book.best_ask()->ticks, 5200);
}
