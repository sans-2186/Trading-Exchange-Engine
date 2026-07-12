#pragma once

#include <list>

#include "mercury/core/order.hpp"
#include "mercury/core/types.hpp"

namespace mercury {

// Holds all resting orders at a single price point.
//
// Invariant: total_quantity_ == sum of remaining() for all orders in orders_.
// This invariant is maintained by add() and remove() — never expose mutable
// Order references to outsiders.
//
// std::list is chosen so that any order can be erased in O(1) given its
// iterator — critical for fast cancel. The caller (OrderBook) stores these
// iterators in its order index.
class PriceLevel {
public:
    using Iterator = std::list<Order>::iterator;

    explicit PriceLevel(Price price) noexcept : price_(price) {}

    // Appends an order to the back of the queue (FIFO time priority).
    // Returns an iterator to the newly inserted order for O(1) cancel later.
    // O(1)
    [[nodiscard]] Iterator add(Order order);

    // Removes the order pointed to by the iterator.
    // Caller must ensure the iterator belongs to this level.
    // O(1)
    void remove(Iterator it);

    // Read-only access to the oldest (highest-priority) order.
    [[nodiscard]] const Order& front() const noexcept { return orders_.front(); }
    [[nodiscard]] Order& front() noexcept { return orders_.front(); }

    [[nodiscard]] bool empty() const noexcept { return orders_.empty(); }

    // Cached sum of remaining quantities — O(1), no iteration needed.
    [[nodiscard]] Quantity total_quantity() const noexcept { return total_quantity_; }

    [[nodiscard]] Price price() const noexcept { return price_; }

    [[nodiscard]] std::size_t order_count() const noexcept { return orders_.size(); }

    // Iteration support (for debugging and market data snapshots).
    [[nodiscard]] auto begin() const noexcept { return orders_.cbegin(); }
    [[nodiscard]] auto end() const noexcept { return orders_.cend(); }

private:
    Price            price_;
    std::list<Order> orders_;
    Quantity         total_quantity_{};  // Protected invariant — only mutated by add/remove.
};

}  // namespace mercury
