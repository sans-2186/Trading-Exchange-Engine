# Mercury Exchange Engine

A modular, low-latency electronic exchange simulator written in Modern C++20.

This is **not** a trading application, stock prediction project, or brokerage platform. It recreates the engineering behind an electronic exchange — the matching engine, order book, market data feed, and network gateway that power real financial markets.

## Current Status — v0.2

| Component | Status |
|-----------|--------|
| Order Book (price-time priority) | Done |
| Matching Engine (limit + market orders, partial fills, sweeps) | Done |
| Interactive CLI | Done |
| Test suite (unit + integration + randomized stress) | Done — 46 tests, 100% passing |
| Benchmarks + latency percentile analysis | Done |
| Networking / market data feed | Planned (v0.3) |
| Concurrency (lock-free queues) | Planned (v0.4) |
| Qt engineering dashboard | Planned (v1.0) |

## Measured Performance

Measured with Google Benchmark + a custom per-call percentile harness
(MacBook Air, macOS 12, Release build — full methodology and caveats in
[docs/benchmarks/](docs/benchmarks/report-2026-07-13.md)):

| Operation | Result |
|-----------|--------|
| Best bid lookup | 2.6 ns (O(1) — `map::begin()`) |
| Submit order — p50 | ~440 ns |
| Submit order — p99 | ~2.3 µs |
| Cancel order — p50 | ~100 ns, zero heap allocations |

Design choices behind these numbers:

- **Fixed-point integer prices** — no floating point anywhere on the matching path
- **`std::map` per side** (bids high-to-low, asks low-to-high) — O(1) best price, O(log n) insert
- **O(1) cancel** — `unordered_map` order index holding `std::list` iterators
- **Strong typedefs** (`OrderId`, `Price`, `Quantity`) — type confusion caught at compile time
- **Caller-provided trade buffers** — 47% of submits are allocation-free after warmup

## Architecture

```
Clients (CLI / Network / Qt Dashboard)
        │
   Gateway Layer (TCP + Binary Protocol)        ← v0.3
        │
   Core Engine (Matching Engine + Order Book)   ← done
        │
   Output Layer (Market Data Feed + WAL)        ← v0.3+
```

Dependencies flow inward: the core engine has zero knowledge of networking or UI.
See [docs/architecture.md](docs/architecture.md) and
[docs/decisions/](docs/decisions/) (Architecture Decision Records).

## Quick Demo

```
mercury> buy 100 50.00
  order #1 submitted
mercury> sell 80 50.00
  order #2 submitted
  TRADE  80 @ 50.00  (buy #1 / sell #2)
mercury> book

  ASKS (sellers)
    <empty>
  ---- spread: n/a ----
  BIDS (buyers)
    50.00  x 20  (1 order)
```

## Build & Run

Prerequisites: C++20 compiler (Clang 14+ / GCC 12+), CMake 3.20+.
Google Test and Google Benchmark are fetched automatically.

```bash
git clone https://github.com/sans-2186/Trading-Exchange-Engine.git
cd Trading-Exchange-Engine

./scripts/build.sh              # configure + build (Release)
ctest --test-dir build          # run all 46 tests
./build/mercury_cli             # interactive exchange shell
./build/mercury_bench           # benchmark suite + latency percentiles
./build/mercury_alloc_count     # heap allocation audit
```

## Project Structure

```
├── include/mercury/     Public API headers
│   └── core/            Order book, matching engine, domain types
├── src/                 Implementations (static library + CLI)
├── tests/
│   ├── unit/            Per-component tests
│   ├── integration/     Multi-component end-to-end scenarios
│   └── stress/          10k randomized ops with invariant checks
├── benchmarks/          Google Benchmark + percentile/allocation harnesses
├── docs/
│   ├── architecture.md  System design and module boundaries
│   ├── decisions/       Architecture Decision Records
│   ├── benchmarks/      Measured performance reports
│   └── releases/        Per-version release notes
└── scripts/             Build and format helpers
```

## Version Roadmap

| Version | Features | Status |
|---------|----------|--------|
| **v0.1** | Order Book + Matching Engine + CLI | Released |
| **v0.2** | Tests + Benchmarks + First optimization | Released |
| **v0.3** | Networking + Multi-client + Market Data Feed | Planned |
| **v0.4** | Concurrency + Lock-free Queues | Planned |
| **v1.0** | Qt Dashboard + Full Documentation | Planned |

## License

This project is for educational and portfolio purposes.
