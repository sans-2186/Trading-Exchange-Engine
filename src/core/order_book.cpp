#include "mercury/core/order_book.hpp"

#include <stdexcept>

namespace mercury {

// ── Internal helper ──────────────────────────────────────────────────────────

template <typename SideMap>
PriceLevel::Iterator OrderBook::insert_into_side(SideMap& side, Order order) {
    Price price = order.price;

    // try_emplace: constructs the PriceLevel in-place only if the key is new.
    // If the price level already exists, it returns the existing one.
    // This avoids an unnecessary copy/move of PriceLevel.
    auto [map_it, inserted] = side.try_emplace(price, price);
    return map_it->second.add(std::move(order));
}

// ── Public API ────────────────────────────────────────────────────────────────

void OrderBook::add_order(Order order) {
    const OrderId id    = order.id;
    const Side    side  = order.side;
    const Price   price = order.price;

    PriceLevel::Iterator list_it;

    if (side == Side::Buy) {
        list_it = insert_into_side(bids_, std::move(order));
    } else {
        list_it = insert_into_side(asks_, std::move(order));
    }

    // Record location so cancel_order can find this order in O(1).
    order_index_.emplace(id.value, OrderLocation{side, price, list_it});
}

bool OrderBook::cancel_order(OrderId id) {
    auto index_it = order_index_.find(id.value);
    if (index_it == order_index_.end()) {
        return false;  // Unknown order — already filled, cancelled, or never added.
    }

    const OrderLocation& loc = index_it->second;

    // Remove from the correct price level.
    if (loc.side == Side::Buy) {
        auto level_it = bids_.find(loc.price);
        level_it->second.remove(loc.it);
        // Clean up the price level entirely if it is now empty.
        // An empty level in the map is wasteful and pollutes best_bid/ask queries.
        if (level_it->second.empty()) {
            bids_.erase(level_it);
        }
    } else {
        auto level_it = asks_.find(loc.price);
        level_it->second.remove(loc.it);
        if (level_it->second.empty()) {
            asks_.erase(level_it);
        }
    }

    order_index_.erase(index_it);
    return true;
}

std::optional<Price> OrderBook::best_bid() const noexcept {
    if (bids_.empty()) return std::nullopt;
    return bids_.begin()->first;
}

std::optional<Price> OrderBook::best_ask() const noexcept {
    if (asks_.empty()) return std::nullopt;
    return asks_.begin()->first;
}

std::optional<Price> OrderBook::spread() const noexcept {
    auto bid = best_bid();
    auto ask = best_ask();
    if (!bid || !ask) return std::nullopt;
    return Price{ask->ticks - bid->ticks};
}

bool OrderBook::contains(OrderId id) const noexcept {
    return order_index_.count(id.value) > 0;
}

// Explicit template instantiation — required because the template is defined
// in a .cpp file. This tells the linker to generate code for both map types.
template PriceLevel::Iterator OrderBook::insert_into_side<OrderBook::BidSide>(
    BidSide&, Order);
template PriceLevel::Iterator OrderBook::insert_into_side<OrderBook::AskSide>(
    AskSide&, Order);

}  // namespace mercury
