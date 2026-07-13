// Mercury CLI — an interactive shell around the MatchingEngine.
//
// This is the first external consumer of mercury_core. It exercises the
// library strictly through its public API and acts as the boundary layer:
// human-readable decimal prices are converted to fixed-point ticks here,
// so the core engine never touches floating point.

#include <cstdint>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "mercury/core/matching_engine.hpp"
#include "mercury/core/order.hpp"
#include "mercury/core/trade.hpp"
#include "mercury/core/types.hpp"
#include "mercury/version.hpp"

namespace {

using namespace mercury;

// One tick = $0.01. "50.00" -> 5000 ticks.
constexpr int64_t kTicksPerDollar = 100;

// Parses a decimal price string ("50", "50.1", "50.00") into ticks.
// Returns false on malformed input. Parsing is done with integer math —
// no floating point, so no rounding surprises at the boundary.
bool parse_price(const std::string& text, int64_t& out_ticks) {
    const auto dot = text.find('.');
    try {
        const int64_t dollars = std::stoll(dot == std::string::npos ? text : text.substr(0, dot));
        int64_t cents = 0;
        if (dot != std::string::npos) {
            std::string frac = text.substr(dot + 1);
            if (frac.empty() || frac.size() > 2) return false;
            if (frac.size() == 1) frac += '0';  // "50.1" -> 10 cents
            cents = std::stoll(frac);
        }
        if (dollars < 0 || cents < 0 || cents > 99) return false;
        out_ticks = dollars * kTicksPerDollar + cents;
        return true;
    } catch (...) {
        return false;
    }
}

std::string format_price(Price price) {
    std::ostringstream os;
    os << price.ticks / kTicksPerDollar << '.' << std::setw(2) << std::setfill('0')
       << price.ticks % kTicksPerDollar;
    return os.str();
}

void print_trades(const std::vector<Trade>& trades) {
    if (trades.empty()) {
        std::cout << "  no trades — order rested or expired\n";
        return;
    }
    for (const auto& t : trades) {
        std::cout << "  TRADE  " << t.quantity.value << " @ " << format_price(t.price)
                  << "  (buy #" << t.buy_order_id.value << " / sell #" << t.sell_order_id.value
                  << ")\n";
    }
}

void print_book(const OrderBook& book) {
    std::cout << "\n  ASKS (sellers)\n";
    if (book.asks_empty()) {
        std::cout << "    <empty>\n";
    } else {
        // Asks print highest first so the best ask sits visually adjacent to the best bid.
        std::vector<std::string> lines;
        for (const auto& [price, level] : book.asks()) {
            std::ostringstream os;
            os << "    " << format_price(price) << "  x " << level.total_quantity().value
               << "  (" << level.order_count() << " order"
               << (level.order_count() == 1 ? "" : "s") << ")";
            lines.push_back(os.str());
        }
        for (auto it = lines.rbegin(); it != lines.rend(); ++it) std::cout << *it << '\n';
    }

    if (auto s = book.spread()) {
        std::cout << "  ---- spread: " << format_price(*s) << " ----\n";
    } else {
        std::cout << "  ---- spread: n/a ----\n";
    }

    std::cout << "  BIDS (buyers)\n";
    if (book.bids_empty()) {
        std::cout << "    <empty>\n";
    } else {
        for (const auto& [price, level] : book.bids()) {
            std::cout << "    " << format_price(price) << "  x " << level.total_quantity().value
                      << "  (" << level.order_count() << " order"
                      << (level.order_count() == 1 ? "" : "s") << ")\n";
        }
    }
    std::cout << '\n';
}

void print_help() {
    std::cout << "  commands:\n"
              << "    buy  <qty> <price>   limit buy   (e.g. buy 100 50.00)\n"
              << "    sell <qty> <price>   limit sell  (e.g. sell 50 50.10)\n"
              << "    buy  <qty> market    market buy\n"
              << "    sell <qty> market    market sell\n"
              << "    cancel <order-id>    cancel a resting order\n"
              << "    book                 show the order book\n"
              << "    help                 show this help\n"
              << "    quit                 exit\n";
}

}  // namespace

int main() {
    std::cout << mercury::kProjectName << " v" << mercury::version_string() << "\n"
              << "Type 'help' for commands.\n\n";

    MatchingEngine engine;
    uint64_t next_id = 1;
    uint64_t timestamp = 0;

    std::string line;
    while (std::cout << "mercury> " << std::flush, std::getline(std::cin, line)) {
        std::istringstream in(line);
        std::string cmd;
        in >> cmd;

        if (cmd.empty()) continue;

        if (cmd == "quit" || cmd == "exit") break;

        if (cmd == "help") {
            print_help();
            continue;
        }

        if (cmd == "book") {
            print_book(engine.book());
            continue;
        }

        if (cmd == "cancel") {
            uint64_t id = 0;
            if (!(in >> id)) {
                std::cout << "  usage: cancel <order-id>\n";
                continue;
            }
            std::cout << (engine.cancel_order(OrderId{id}) ? "  cancelled #"
                                                           : "  no such order #")
                      << id << '\n';
            continue;
        }

        if (cmd == "buy" || cmd == "sell") {
            uint64_t qty = 0;
            std::string price_text;
            if (!(in >> qty >> price_text) || qty == 0) {
                std::cout << "  usage: " << cmd << " <qty> <price|market>\n";
                continue;
            }

            const Side side = (cmd == "buy") ? Side::Buy : Side::Sell;
            Order order{
                .id        = OrderId{next_id},
                .side      = side,
                .type      = OrderType::Limit,
                .price     = Price{0},
                .quantity  = Quantity{qty},
                .filled    = Quantity{0},
                .timestamp = timestamp++,
            };

            if (price_text == "market") {
                order.type = OrderType::Market;
            } else {
                int64_t ticks = 0;
                if (!parse_price(price_text, ticks)) {
                    std::cout << "  invalid price: " << price_text << '\n';
                    continue;
                }
                order.price = Price{ticks};
            }

            std::cout << "  order #" << next_id << " submitted\n";
            auto trades = engine.submit_order(std::move(order));
            print_trades(trades);
            ++next_id;
            continue;
        }

        std::cout << "  unknown command: " << cmd << "  (try 'help')\n";
    }

    std::cout << "goodbye\n";
    return 0;
}
