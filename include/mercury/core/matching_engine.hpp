#pragma once

#include <vector>

#include "mercury/core/order.hpp"
#include "mercury/core/order_book.hpp"
#include "mercury/core/trade.hpp"
#include "mercury/core/types.hpp"

namespace mercury {

// Processes incoming orders against the resting order book.
//
// Responsibilities:
//   - Match incoming orders using price-time priority
//   - Generate Trade records for every fill
//   - Rest unmatched Limit order remainders in the book
//   - Discard unmatched Market order remainders (market orders never rest)
//
// The MatchingEngine owns the OrderBook. External code accesses the book
// only through read-only queries exposed here.
class MatchingEngine {
public:
    // Submit an order for matching — hot-path overload.
    // Clears `out_trades`, then fills it with all Trade records generated.
    // The caller owns the buffer: reuse the same vector across calls to
    // amortize its allocation to zero after warmup.
    void submit_order(Order order, std::vector<Trade>& out_trades);

    // Convenience overload — allocates a fresh vector per call.
    // Prefer the out-parameter overload on performance-sensitive paths.
    [[nodiscard]] std::vector<Trade> submit_order(Order order);

    // Cancel a resting order by ID.
    // Returns true if found and removed, false if unknown.
    bool cancel_order(OrderId id);

    // Read-only access to the book (for CLI display, market data, tests).
    [[nodiscard]] const OrderBook& book() const noexcept { return book_; }

private:
    OrderBook book_;

    // Match an incoming buy order against resting asks.
    // Fills trades in-place and updates `order` remaining quantity.
    void match_buy(Order& order, std::vector<Trade>& trades);

    // Match an incoming sell order against resting bids.
    void match_sell(Order& order, std::vector<Trade>& trades);

    // Price compatibility check — always true for Market orders.
    [[nodiscard]] static bool buy_price_compatible(const Order& incoming,
                                                    Price best_ask) noexcept;
    [[nodiscard]] static bool sell_price_compatible(const Order& incoming,
                                                     Price best_bid) noexcept;
};

}  // namespace mercury
