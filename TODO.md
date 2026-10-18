# Project Roadmap & Future Enhancements (TODO)

This document tracks some ideas of architectural upgrades, low-latency optimizations, and tooling expansions for this Limit Order Book engine.

---

## Phase 1: Telemetry & Microstructure Analytics
- [ ] **Nanosecond Latency Histogram:** Instrument LOB state updates (`on_add_order`, `on_order_delete`, etc.) using hardware cycle counters (`__rdtsc`) or `std::chrono::steady_clock` with a pre-allocated histogram to report `p50`, `p90`, `p99`, and `p99.9` tail latencies without distorting the hot path.
- [ ] **Quantitative Signal Extraction:** Compute real-time Level-1/Level-2 microstructure features on every Book update:
  - Mid-Price & Micro-Price (volume-weighted mid-price)
  - Bid-Ask Spread (in ticks and basis points)
  - Order Book Imbalance (OBI) across the Top-$N$ levels
- [ ] **L2 Snapshot Exporter (`--export`):** Periodically serialize Top-5/Top-10 Bid/Ask price-volume ladders at fixed market-time intervals (e.g., every `100ms` via the 48-bit ITCH timestamp) into binary/CSV datasets for downstream Python/Pandas research.

---

## Phase 2: Real-Time Market Replay & Visualization
- [ ] **Timestamp-Paced Replay Engine (`--replay --speed=<N>x`):** Use `itch::decode_timestamp()` to compute inter-message nanosecond deltas ($\Delta t = t_i - t_{i-1}$) and pace message dispatch to simulate live market feeds at `1x`, `10x`, or custom replay speeds.
- [ ] **UDP Multicast / Shared-Memory Feed Simulator:** Decouple the reader into a standalone `itch_publisher` process that broadcasts raw ITCH packets over local UDP sockets or POSIX shared memory (`/dev/shm`) to a listening `itch_lob` consumer.
- [ ] **Live Terminal UI (`--tui`) / Dear ImGui Dashboard:** Build a real-time visual order book depth ladder displaying Top-10 Bid/Ask queues, live spread changes, and cumulative volume bars refreshed at 60 FPS during paced replay.

---

## Phase 3: Hardware & Data Structure Optimizations
- [ ] **Open-Addressing Flat Hash Map:** Replace `std::unordered_map<uint64_t, uint32_t>` (which relies on heap-allocated bucket nodes) with a custom pre-allocated linear-probing / Robin-Hood flat hash table to eliminate cache misses during order ID lookups.
- [ ] **Flat Price Level Ladder:** Replace `std::map` Red-Black trees for `bids_` and `asks_` with a hybrid flat array / bitset-indexed price ladder anchored around the current mid-price to achieve true $O(1)$ price level insertion and deletion.
- [ ] **Lock-Free SPSC Ring Buffer:** Introduce a Single-Producer Single-Consumer (SPSC) lock-free queue with CPU core pinning (`pthread_setaffinity_np`) to separate the parser thread from the LOB & analytics thread.
- [ ] **Unit & Invariant Testing Suite:** Add GoogleTest unit tests verifying crossed-book checks (`Best Bid < Best Ask`), pool exhaustion edge cases, and synthetic ITCH packet fuzzing.