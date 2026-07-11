#pragma once

#include <cstdint>
#include <compare>
#include <ostream>

namespace mercury {

enum class Side : std::uint8_t { Buy, Sell };

enum class OrderType : std::uint8_t { Limit, Market };

// Strong typedefs — prevent passing a Price where a Quantity was expected.
struct OrderId {
    std::uint64_t value{};

    constexpr auto operator<=>(const OrderId&) const = default;
};

struct Price {
    std::int64_t ticks{};  // Fixed-point: price in minimum tick units (e.g. cents)

    constexpr auto operator<=>(const Price&) const = default;
};

struct Quantity {
    std::uint64_t value{};

    constexpr auto operator<=>(const Quantity&) const = default;
};

inline std::ostream& operator<<(std::ostream& os, Side side) {
    return os << (side == Side::Buy ? "Buy" : "Sell");
}

}  // namespace mercury
