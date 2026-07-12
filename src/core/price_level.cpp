#include "mercury/core/price_level.hpp"

namespace mercury {

PriceLevel::Iterator PriceLevel::add(Order order) {
    // Update the cached total before insertion — maintains the invariant
    // even if an exception were thrown (strong guarantee on list::push_back).
    total_quantity_.value += order.remaining().value;
    orders_.push_back(std::move(order));
    return std::prev(orders_.end());
}

void PriceLevel::remove(Iterator it) {
    // Decrement cached total before erasing — iterator stays valid until erase.
    total_quantity_.value -= it->remaining().value;
    orders_.erase(it);
}

void PriceLevel::apply_fill(Iterator it, uint64_t fill_qty) noexcept {
    // Update the order's filled counter and the cached total in one step.
    // This is the only correct way to record a partial fill — going through
    // front() and mutating directly would silently break total_quantity_.
    it->filled.value     += fill_qty;
    total_quantity_.value -= fill_qty;
}

}  // namespace mercury
