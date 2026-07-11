# Mercury Exchange Engine — Architecture

## Overview

Mercury Exchange Engine is a modular, low-latency electronic exchange simulator written in Modern C++20. It recreates the engineering behind an electronic exchange — not a trading app, brokerage, or prediction system.

## System Diagram (Target v1.0)

```
┌─────────────┐     ┌─────────────┐     ┌──────────────────┐
│ CLI Client  │     │  Network    │     │  Qt Dashboard    │
│ (Simulator) │     │  Clients    │     │  (Engineering)   │
└──────┬──────┘     └──────┬──────┘     └────────┬─────────┘
       │                   │                      │
       │            ┌──────▼──────┐               │
       │            │ TCP Server  │               │
       │            │ + Protocol  │               │
       │            └──────┬──────┘               │
       │                   │                      │
       └───────────┬───────┘                      │
                   ▼                              │
            ┌──────────────┐                      │
            │   Matching   │                      │
            │   Engine     │                      │
            └──────┬───────┘                      │
                   │                              │
            ┌──────▼───────┐    ┌────────────┐    │
            │  Order Book  │    │ Market Data│────┘
            └──────────────┘    │    Feed    │
                                └─────┬──────┘
                                      │
                                ┌─────▼──────┐
                                │ WAL / Log  │
                                └────────────┘
```

## Module Boundaries

Dependencies flow **inward**. Core knows nothing about networking or Qt.

| Module | Directory | Depends on |
|--------|-----------|------------|
| Core | `include/mercury/core/` | Nothing (stdlib only) |
| Market Data | `include/mercury/market_data/` | Core |
| Network | `include/mercury/network/` | Core |
| Metrics | `include/mercury/metrics/` | Core |
| CLI | `src/cli/` | Core |
| Dashboard | `apps/dashboard/` | Core, Market Data, Metrics |

## CMake Targets

| Target | Type | Phase |
|--------|------|-------|
| `mercury_core` | Static library | 0 (scaffold) |
| `mercury_cli` | Executable | 0 |
| `mercury_tests` | Test runner | 0 |
| `mercury_bench` | Benchmark runner | 4 |
| `mercury_dashboard` | Qt executable | 8 |

## Version Roadmap

| Version | Scope |
|---------|-------|
| v0.1 | Order Book + Matching Engine + CLI |
| v0.2 | Unit tests + Benchmarks |
| v0.3 | Networking + Market Data Feed |
| v0.4 | Concurrency + lock-free queues |
| v1.0 | Qt Dashboard + full documentation |

## Key Design Decisions

See [`docs/decisions/`](decisions/) for Architecture Decision Records (ADRs).
