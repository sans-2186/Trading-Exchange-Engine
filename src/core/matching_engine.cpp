#include "mercury/core/matching_engine.hpp"

#include <algorithm>
#include <cassert>

namespace mercury {

// ── Price compatibility ───────────────────────────────────────────────────────

bool MatchingEngine::buy_price_compatible(const Order& incoming,
                                           Price best_ask) noexcept {
    if (incoming.type == OrderType::Market) return true;  // market orders match anything
    return incoming.price.ticks >= best_ask.ticks;
}

bool MatchingEngine::sell_price_compatible(const Order& incoming,
                                            Price best_bid) noexcept {
    if (incoming.type == OrderType::Market) return true;
    return incoming.price.ticks <= best_bid.ticks;
}

// ── Matching logic ────────────────────────────────────────────────────────────

void MatchingEngine::match_buy(Order& incoming, std::vector<Trade>& trades) {
    while (incoming.remaining().value > 0) {
        auto best = book_.best_ask();
        if (!best) break;

        if (!buy_price_compatible(incoming, *best)) break;

        PriceLevel& level = book_.ask_level(*best);
        // Capture fields before apply_fill mutates the order.
        const OrderId resting_id    = level.front().id;
        const Price   resting_price = level.front().price;
        const uint64_t fill_qty     = std::min(incoming.remaining().value,
                                               level.front().remaining().value);

        trades.push_back(Trade{
            .buy_order_id  = incoming.id,
            .sell_order_id = resting_id,
            .price         = resting_price,
            .quantity      = Quantity{fill_qty},
            .timestamp     = incoming.timestamp,
        });

        incoming.filled.value += fill_qty;

        // apply_fill keeps PriceLevel::total_quantity_ in sync — never mutate directly.
        auto it = level.begin_mut();
        level.apply_fill(it, fill_qty);

        if (it->is_fully_filled()) {
            book_.cancel_order(resting_id);
        }
    }
}

void MatchingEngine::match_sell(Order& incoming, std::vector<Trade>& trades) {
    while (incoming.remaining().value > 0) {
        auto best = book_.best_bid();
        if (!best) break;

        if (!sell_price_compatible(incoming, *best)) break;

        PriceLevel& level = book_.bid_level(*best);
        const OrderId resting_id    = level.front().id;
        const Price   resting_price = level.front().price;
        const uint64_t fill_qty     = std::min(incoming.remaining().value,
                                               level.front().remaining().value);

        trades.push_back(Trade{
            .buy_order_id  = resting_id,
            .sell_order_id = incoming.id,
            .price         = resting_price,
            .quantity      = Quantity{fill_qty},
            .timestamp     = incoming.timestamp,
        });

        incoming.filled.value += fill_qty;

        auto it = level.begin_mut();
        level.apply_fill(it, fill_qty);

        if (it->is_fully_filled()) {
            book_.cancel_order(resting_id);
        }
    }
}

// ── Public API ────────────────────────────────────────────────────────────────

void MatchingEngine::submit_order(Order order, std::vector<Trade>& out_trades) {
    out_trades.clear();

    if (order.side == Side::Buy) {
        match_buy(order, out_trades);
    } else {
        match_sell(order, out_trades);
    }

    // Rest any unfilled remainder — Limit orders only.
    // Market orders that could not fully fill are silently discarded.
    if (!order.is_fully_filled() && order.type == OrderType::Limit) {
        book_.add_order(std::move(order));
    }
}

std::vector<Trade> MatchingEngine::submit_order(Order order) {
    std::vector<Trade> trades;
    submit_order(std::move(order), trades);
    return trades;
}

bool MatchingEngine::cancel_order(OrderId id) {
    return book_.cancel_order(id);
}

}  // namespace mercury
