# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).

## [0.2.0] - 2026-05-26

### Added

- **WSS Transport** — WebSocket Secure (`wss://host:port`). Full TLS + WebSocket in a single transport. RFC 6455 binary frames over OpenSSL. (`include/msgbroker/mb_wss.h`)
- **IPv6 Dual-Stack** — All TCP-based transports (TCP, TLS, WS, WSS) now support IPv6 addresses (`[::1]:port`, `[::]:port`) and dual-stack sockets via `IPV6_V6ONLY=0`.
- **DNS Hostname Resolution** — `getaddrinfo`-based hostname resolution for all transports. Connect to `tcp://localhost:9000` instead of only IP addresses. Supports IPv4, IPv6, and DNS with `AF_UNSPEC` (tries IPv6 first).
- **Auto-Reconnect on All Transports** — TCP, TLS, WS, and WSS connect endpoints now automatically retry with exponential backoff when `MB_RECONNECT_IVL > 0` (default: 100ms). Configurable max interval via `MB_RECONNECT_IVL_MAX`.
- **Shared Network Utility** — `src/utils/net.c` consolidates address parsing and connection logic previously duplicated across 6 transport files. `mb_net_parse_addr()`, `mb_net_connect()`, `mb_net_bind()`.
- **`MB_VERSION_STRING`** macro added to `mb.h` for compile-time version string access.
- **Fuzz Testers** — `fuzz_sipc`, `fuzz_ws_frame`, `fuzz_msgqueue` (build with `-DMB_BUILD_FUZZ=ON`).
- **Benchmark Regression CI** — `tests/benchmark/run_benchmarks.sh` compares against baseline with configurable threshold.

### Changed

- **Transport IDs** now include WSS: INPROC=-1, IPC=-2, TCP=-3, WS=-4, TLS=-5, WSS=-6. `MB_MAX_TRANSPORT=6`.
- **Version bumped** from 0.1.0 to 0.2.0.
- **Benchmark baseline** uses conservative values (7M inproc, 140K TCP) with 20% threshold for system variance.
- **README** updated: 6 transports, IPv6, DNS, auto-reconnect, 46+ tests.

### Fixed

- **TLS cert path truncation** — `sock.c` `MB_TLS_CONFIG_CERT/KEY/CA` setsockopt was writing null terminator at `optvallen-1` instead of `optvallen`, truncating paths by 1 character.
- **WebSocket server missing TCP_NODELAY** — `bws.c` accept loop now sets `TCP_NODELAY` on accepted sockets for consistent latency.
- **TLS cert test** — `test_tls_cert.c` now sets `MB_RECONNECT_IVL=0` for connect-failure test to prevent background reconnect thread.

### Tests

- **46 tests** (up from 43): added `test_ipv6_dns`, `test_reconnect`, `test_wss`, `test_tls`, `test_tls_cert`.
- All 46/46 passing in Debug and Release builds.

## [0.1.0] - 2026-05-20

### Added

- Full reimplementation of nanomsg SP protocol family in pure C (C99-C23).
- **10 Protocol Socket Types**: PAIR, PUSH/PULL, REQ/REP, PUB/SUB, BUS, SURVEYOR/RESPONDENT.
- **5 Transports**: inproc (zero-copy sync msgqueue), IPC (Unix domain socket via SIPC), TCP (SIPC over TCP), TLS (OpenSSL), WebSocket (RFC 6455 binary frames).
- **io_uring** backend with runtime detection and automatic epoll fallback on Linux.
- **Memory Pool**: custom allocator with slab, arena, and chunkref for zero-copy messaging.
- **Thread Pool**: per-worker event loops with task queues.
- **Coroutines**: stackful ucontext (Unix) / Win32 Fibers cooperative scheduling.
- **Distributed**: SWIM gossip membership, UDP multicast discovery, consistent hashing ring, cluster routing.
- **43 tests** covering PAL, data structures, AIO, core, transport, protocol, distributed, and API layers.
- **Benchmarks**: inproc ~11.3M msg/sec, TCP ~160K msg/sec (64-byte messages, loopback).
- **CI**: GitHub Actions workflow for Linux GCC/Clang.
- Complete `notes.html` development log.
