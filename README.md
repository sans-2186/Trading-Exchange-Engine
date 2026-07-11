# Mercury Exchange Engine

A modular, low-latency electronic exchange simulator written in Modern C++20.

This is **not** a trading application, stock prediction project, or brokerage platform. It recreates the engineering behind an electronic exchange — the matching engine, order book, market data feed, and network gateway that power real financial markets.

## What This Demonstrates

- Modern C++20 (concepts, `constexpr`, strong typedefs, move semantics)
- Advanced data structures and algorithms (order book, price-time priority matching)
- System design (modular architecture, inward dependency flow)
- Performance engineering (benchmarks, profiling, hot-path optimization)
- Networking (TCP server, binary protocol)
- Concurrency (worker threads, lock-free queues)
- Testing (unit, randomized, stress)
- Software architecture (SOLID, clean separation of concerns)

## Architecture

```
Clients (CLI / Network / Qt Dashboard)
        │
   Gateway Layer (TCP + Binary Protocol)
        │
   Core Engine (Matching Engine + Order Book)
        │
   Output Layer (Market Data Feed + WAL + Metrics)
```

See [docs/architecture.md](docs/architecture.md) for the full system design.

## Prerequisites

- C++20 compiler (Clang 14+ or GCC 12+)
- CMake 3.20+
- Git

Optional:
- [vcpkg](https://vcpkg.io/) for dependency management (Google Test is fetched automatically if vcpkg is not configured)

### macOS

```bash
xcode-select --install
brew install cmake
```

## Build

```bash
# Clone and build
git clone https://github.com/sans-2186/Trading-Exchange-Engine.git
cd Trading-Exchange-Engine

./scripts/build.sh

# Run tests
ctest --test-dir build

# Run CLI
./build/mercury_cli
```

## Project Structure

```
├── include/mercury/     Public API headers
│   └── core/            Order book, matching engine (Phases 1–2)
├── src/                 Implementations
├── tests/               Unit, integration, and stress tests
├── benchmarks/          Performance benchmarks (Phase 4)
├── apps/dashboard/      Qt engineering dashboard (Phase 8)
├── docs/                Architecture, coding standards, ADRs
└── scripts/             Build and format helpers
```

## Version Roadmap

| Version | Features |
|---------|----------|
| **v0.1** | Order Book + Matching Engine + CLI |
| **v0.2** | Unit Tests + Benchmarks |
| **v0.3** | Networking + Market Data Feed |
| **v0.4** | Concurrency + Lock-free Queues |
| **v1.0** | Qt Dashboard + Full Documentation |

## License

This project is for educational and portfolio purposes.
