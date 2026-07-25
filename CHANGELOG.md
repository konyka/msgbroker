# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).

## [Unreleased]

### Added

- **CI sanitizer jobs** — GitHub Actions now runs the full test suite under AddressSanitizer (+LeakSanitizer) and UndefinedBehaviorSanitizer on Linux. These previously-uncovered failure modes (allocation-size-too-big aborts, leaks, UB) are now caught on every push and pull request.
- **CI C-standard matrix** — A dedicated `standards` job now builds and tests the library with `MB_C_STANDARD` set to 99, 11, 17, and 23, validating the README's C99-through-C23 compatibility claim.

### Changed

- **Deterministic OOM test behaviour under ASan** — CTest now sets `ASAN_OPTIONS=allocator_may_return_null=1` for every test. AddressSanitizer's default (`allocator_may_return_null=0`) aborts on absurdly large allocations instead of returning `NULL` the way production `malloc` does; the OOM-path tests (`test_hash`, `test_msg`, `test_arena`, `test_timeout::test_send_oom_large_body`) probe the library's `NULL`-return handling and were therefore aborting under ASan. The option makes ASan match production semantics. It is ignored on non-ASan builds, so plain Debug/Release runs are unaffected.

### Fixed

- **Pipe outbuf double-free across IPC/TLS/WS transports** — `sipc`, `stls`, and `sws` accessed their `outbuf`/`outpos`/`outlen` fields from two threads with no synchronization: the application send thread (`mb_send` → `pipe_send` → `flush_outbuf`) and the reconnect/accept thread (`mb_sock_pipe_add` → `sync_rcvfd` → `pair_events` → `can_send` → `flush_outbuf`). During reconnect the two threads could both free the same pending outbuf (AddressSanitizer "attempting double-free", reproduced ~1/8 runs in `test_reconnect_disc`). Each pipe now serializes outbuf access with a per-pipe `outlock` mutex; `flush_outbuf` defers the `report_error` callback until after the lock is released, since the callback re-enters `cipc`/`ctls`/`cws` `on_disconnect` (taking the transport lock and calling `stop`), preserving a consistent `transport-lock → outlock` ordering. This closes the memory-corruption path. The cross-thread `disconnected` flag is now `mb_atomic_int` (matching the existing `mb_worker.running` / `mb_threadpool_thread.running` precedent), read with `mb_atomic_load` and written with `mb_atomic_store`. Residual same-shape data races remain on logic-only state not covered by `outlock`: the `linger_flush` teardown reads of `outbuf`, and the recv-side `instate`/`inpos`/`inlen` state (read by `has_msg`, written by `recv`). These are benign on x86 (atomic int/pointer reads; worst case a one-shot stale guard decision, never memory corruption) but would be flagged by ThreadSanitizer; a complete fix is an `inlock` mirroring `outlock` for the recv path and should be landed together with a TSan CI job.
- **Unchecked `SSL_CTX_load_verify_locations` return** — `btls.c` (server), `ctls.c` (client), and `cwss.c` (WSS client) all ignored the return value when loading the user-configured CA bundle. A missing or malformed CA file silently left `SSL_VERIFY_PEER` active with no trusted CAs, producing confusing handshake failures (or, where system defaults applied, incomplete verification). Each site now checks the return, mirrors the existing cleanup idiom of its sibling `SSL_CTX_use_*_file` calls, and fails closed with `-EINVAL`/`NULL`.
- **Partial entropy read in `mb_random_seed`** — `read()` of `/dev/urandom` did not loop or check its return, so a short read could leave the PRNG state partially uninitialized. The seed is now read in a loop; any shortfall falls through to the time-based fallback rather than proceeding with a partially-zeroed state.
- **test_slab leak under LeakSanitizer** — `test_slab` allocated 16 slab objects, returned only 2 to the freelist, re-borrowed 2, then called `mb_slab_term` without returning the 16 outstanding objects (`mb_slab_term` only frees objects currently in the freelist). All outstanding objects are now returned before teardown, eliminating the 1024-byte leak reported by LeakSanitizer.
- **test_hash SEGFAULT under -DNDEBUG** — `assert(mb_hash_init(...))` was compiled out under Release builds (`-DNDEBUG`), leaving the hash struct uninitialized. Tests are now compiled with `-UNDEBUG` to ensure assertions are always active.
- **Parallel test port conflicts** — `test_ipv6_dns`, `test_reconnect`, and `test_wss` shared TCP ports with `test_tcp` (18890-18898), causing timeouts under `ctest -j4`. Each test file now uses a unique non-overlapping port range (19010+, 19020+, 19030+).
- **MPSC queue unchecked malloc** — `mb_mpsc_queue_init` did not check the `malloc` return for the stub node, which would dereference NULL on allocation failure. Now aborts explicitly.
- **Threadpool thread-start error ignored** — `mb_threadpool_init` did not check the return value of `mb_thread_start`. If thread creation failed, subsequent `mb_thread_join` would hang or crash. Now rolls back initialized workers and returns the error.
- **Atomic field data races** — `mb_worker.running` and `mb_threadpool_thread.running` were accessed with direct assignment instead of atomic operations. Now consistently uses `mb_atomic_store`/`mb_atomic_load`.
- **Timer subsystem non-functional** — `mb_timerset_tick` was an empty stub, `mb_timerset_cancel` did not update the list head, `mb_timer_init` never allocated a handle, and `mb_timer_start` never inserted into the timerset. Timers now fire correctly: the handle is allocated in `mb_timer_init`, inserted on FSM start, canceled on stop, and expired timers are processed in `mb_timerset_tick` which calls back to raise `MB_TIMER_DONE`.
- **Gossip concurrency defects** — `mb_gossip_find_node`, `mb_gossip_node_count`, and `mb_gossip_set_callback` accessed shared state without the mutex. `mb_gossip_add_node` had a TOCTOU race (find-then-insert without holding the lock). `mb_gossip_tick` invoked the change callback while holding the mutex, risking deadlock if the callback re-entered gossip APIs. All functions now lock the mutex; the tick callback is invoked after unlock.
- **Discovery fd leak on thread-start failure** — `mb_discovery_start` did not close the socket if `mb_thread_start` failed. Now closes the socket and returns the error.
- **Discovery untrusted packet data** — `recvfrom` packet `addr` field was not guaranteed null-terminated. Now explicitly null-terminated before passing to the callback.
- **WebSocket SHA-1 uninitialized digest** — `mb_sha1` in `bws.c` returned early on allocation failure without writing to the output buffer, leaving the handshake hash uninitialized. Now returns `-1` on failure; the caller checks the return value.
- **Protocol pipe data double-free risk** — 14 protocol `_rm` handlers freed per-pipe data but did not clear the pipe's data pointer. A double-removal would dereference a dangling pointer. All handlers now call `mb_pipe_setdata(pipe, NULL)` after freeing.

### Tests

- **50 tests** now passing in both Debug and Release builds with parallel `ctest -j4` (previously 3 tests failed under Release parallel execution).
- Tests are now compiled with `-UNDEBUG` to ensure `assert()` calls are always active, even in Release builds.
- Network tests assigned unique non-overlapping port ranges to prevent parallel execution conflicts.

### Known Issues

- **io_uring poll modify blocks** — `mb_evloop` modify path uses `io_uring_wait_cqe` (blocking) to synchronize poll removal before re-adding, which can stall the event loop under high fd churn. Recommend migrating to non-cancel-based rearm or epoll fallback for modify-heavy workloads.
- **TLS hostname verification** — `ctls.c` does not authenticate the server hostname against the TLS certificate. Do not use `tls://` with untrusted endpoints without additional verification.

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
