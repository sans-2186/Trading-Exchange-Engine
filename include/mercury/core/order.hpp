#pragma once

#include <cstdint>

#include "mercury/core/types.hpp"

namespace mercury {

// Represents a single resting order in the order book.
// An order is immutable after creation except for the `filled` field,
// which is updated by the matching engine as partial fills occur.
struct Order {
    OrderId  id{};
    Side     side{Side::Buy};
    OrderType type{OrderType::Limit};
    Price    price{};       // Fixed-point ticks. Ignored for Market orders.
    Quantity quantity{};    // Total original quantity requested.
    Quantity filled{};      // Quantity matched so far. Starts at zero.
    uint64_t timestamp{};   // Arrival time (nanoseconds since epoch). Determines time priority.

    // Remaining quantity yet to be filled.
    [[nodiscard]] constexpr Quantity remaining() const noexcept {
        return Quantity{quantity.value - filled.value};
    }

    [[nodiscard]] constexpr bool is_fully_filled() const noexcept {
        return filled.value >= quantity.value;
    }

    [[nodiscard]] constexpr bool is_buy() const noexcept { return side == Side::Buy; }
    [[nodiscard]] constexpr bool is_sell() const noexcept { return side == Side::Sell; }
};

}  // namespace mercury
