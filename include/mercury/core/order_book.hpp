#pragma once

#include <functional>
#include <map>
#include <optional>
#include <unordered_map>

#include "mercury/core/order.hpp"
#include "mercury/core/price_level.hpp"
#include "mercury/core/types.hpp"

namespace mercury {

// Two-sided limit order book with price-time priority.
//
// Bids (buyers):  sorted highest price first  — std::greater<Price>
// Asks (sellers): sorted lowest price first   — default std::less<Price>
//
// Complexity:
//   add_order    — O(log n) where n = number of distinct price levels
//   cancel_order — O(1)
//   best_bid     — O(1)
//   best_ask     — O(1)
class OrderBook {
public:
    // Adds a limit order to the book.
    // Precondition: order.type == OrderType::Limit
    // Market orders are handled by the MatchingEngine, not rested in the book.
    void add_order(Order order);

    // Removes an order by ID.
    // Returns true if the order was found and removed.
    // Returns false if the ID is unknown (already filled, cancelled, or never added).
    bool cancel_order(OrderId id);

    // Best prices — nullopt when that side of the book is empty.
    [[nodiscard]] std::optional<Price> best_bid() const noexcept;
    [[nodiscard]] std::optional<Price> best_ask() const noexcept;

    // Spread = best_ask - best_bid. nullopt if either side is empty.
    [[nodiscard]] std::optional<Price> spread() const noexcept;

    // Queries
    [[nodiscard]] bool contains(OrderId id) const noexcept;
    [[nodiscard]] bool bids_empty() const noexcept { return bids_.empty(); }
    [[nodiscard]] bool asks_empty() const noexcept { return asks_.empty(); }
    [[nodiscard]] std::size_t bid_level_count() const noexcept { return bids_.size(); }
    [[nodiscard]] std::size_t ask_level_count() const noexcept { return asks_.size(); }

    // Read-only access to sides (for market data feed and tests).
    using BidSide = std::map<Price, PriceLevel, std::greater<Price>>;
    using AskSide = std::map<Price, PriceLevel>;

    [[nodiscard]] const BidSide& bids() const noexcept { return bids_; }
    [[nodiscard]] const AskSide& asks() const noexcept { return asks_; }

    // Mutable level access for the MatchingEngine (apply_fill on partial fills).
    // Precondition: price exists in the respective side — caller must verify.
    [[nodiscard]] PriceLevel& bid_level(Price price) { return bids_.at(price); }
    [[nodiscard]] PriceLevel& ask_level(Price price) { return asks_.at(price); }

private:
    // Tracks where each order lives so cancel is O(1).
    struct OrderLocation {
        Side                      side;
        Price                     price;
        PriceLevel::Iterator      it;
    };

    BidSide bids_;
    AskSide asks_;
    std::unordered_map<uint64_t, OrderLocation> order_index_;  // keyed by OrderId::value

    // Internal helper — inserts into the correct side and level.
    template <typename SideMap>
    PriceLevel::Iterator insert_into_side(SideMap& side, Order order);
};

}  // namespace mercury
