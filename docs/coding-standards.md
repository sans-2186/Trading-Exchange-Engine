# Coding Standards

## Language

- **C++20** minimum
- No C-style casts; use `static_cast`, `reinterpret_cast` only when necessary
- No raw `new`/`delete`; use stack allocation or smart pointers

## Naming

| Entity | Convention | Example |
|--------|-----------|---------|
| Types | `PascalCase` | `OrderBook`, `PriceLevel` |
| Functions / variables | `snake_case` | `add_order`, `best_bid` |
| Constants | `kPascalCase` or `SCREAMING_SNAKE` | `kMaxOrders`, `MERCURY_VERSION` |
| Namespaces | `snake_case` | `mercury::core` |
| Files | `snake_case.hpp` / `.cpp` | `order_book.hpp` |

## Headers

- Use `#pragma once` (not include guards)
- Include order: corresponding header → C++ stdlib → project headers
- Forward-declare when possible to reduce compile times

## Ownership and Memory

- Stack allocation by default
- `std::unique_ptr` when heap ownership is needed
- `std::shared_ptr` only when shared ownership is genuinely required
- Move semantics for containers and large objects passed between functions

## Const and noexcept

- Mark methods `const` when they do not modify state
- Mark non-throwing functions `noexcept` (especially on hot paths)
- Pass by `const&` for read-only access; pass by value + move for ownership transfer

## Prices and Quantities

- **Never use `float` or `double` for prices** — use fixed-point integers (`Price::ticks`)
- Use strong typedefs (`OrderId`, `Price`, `Quantity`) to prevent type confusion

## Formatting

- Run `clang-format` before every commit: `./scripts/format.sh`
- 4-space indentation, 100-character line limit
- Base style: LLVM (see `.clang-format`)

## Commit Messages

Imperative mood with scope prefix:

```
core: add PriceLevel with FIFO queue
build: scaffold CMake project structure
test: add order book cancel edge cases
```

## Review Checklist

Before merging any feature:

- [ ] Builds with zero warnings (`-Wall -Wextra -Wpedantic`)
- [ ] All tests pass (`ctest`)
- [ ] `clang-format` applied
- [ ] Public API documented in header comments
- [ ] No finance assumptions in code comments — use engineering language
