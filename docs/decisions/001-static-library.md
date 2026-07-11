# ADR 001: Static Library for mercury_core

**Status:** Accepted  
**Date:** 2026-07-11  
**Phase:** 0

## Context

Phase 0 requires choosing how to package the core engine (`OrderBook`, `MatchingEngine`, domain types). Two options were considered:

1. **Header-only** — all implementation in `.hpp` files
2. **Static library** — `.hpp` declarations + `.cpp` implementations compiled into `libmercury_core.a`

## Decision

Use a **static library** (`mercury_core`) for all core engine components. Small type wrappers (`Price`, `OrderId`, `Side`, enums) remain header-only.

## Consequences

**Positive:**
- Incremental builds — changing a `.cpp` does not recompile all consumers
- True encapsulation — private members hidden from consumers
- Production-realistic architecture — matches how exchange codebases are structured
- Clean linking — CLI, network gateway, and Qt dashboard all link against `mercury_core`

**Negative:**
- Two files per component (`.hpp` + `.cpp`) — slightly more boilerplate
- Must design public API before implementation

## Alternatives Considered

**Header-only:** Faster iteration in early phases but causes compile-time blowup by Phase 4 (tests + benchmarks + CLI + network all include core headers). Rejected for a multi-phase system.

## Interview Answer

> "I chose a static library because the matching engine is the hot path — I want to hide its internals in `.cpp` files so consumers only see the public API. Small value types like `Price` stay header-only since they have no implementation logic."
