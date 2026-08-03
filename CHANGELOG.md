# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).

## [Unreleased]

### Added

- **CI sanitizer jobs** — GitHub Actions now runs the full test suite under AddressSanitizer (+LeakSanitizer) and UndefinedBehaviorSanitizer on Linux. These previously-uncovered failure modes (allocation-size-too-big aborts, leaks, UB) are now caught on every push and pull request.
- **CI C-standard matrix** — A dedicated `standards` job now builds and tests the library with `MB_C_STANDARD` set to 99, 11, 17, and 23, validating the README's C99-through-C23 compatibility claim.
- **Threadpool efficient submit** — Workers now sleep on a condition variable instead of `nanosleep(1ms)`. The submit path signals the condvar so a newly submitted task wakes a sleeping worker immediately rather than waiting up to 1 ms. Round-robin scheduling moves from a process-global static counter to an `mb_threadpool` struct member, so multiple independent pool instances each maintain their own distribution state.
- **Hash auto-rehash on insert** — `mb_hash_put` and `mb_hash_put2` now call `mb_hash_rehash` when `count` exceeds `nbuckets`, doubling the bucket count. This keeps the average chain length bounded and prevents the O(n) lookup degradation that followed from a long static chain after many inserts without removals. A `test_hash_rehash` case verifies the table is findable after a rehash.
- **io_uring submission queue size configurable** — `io_uring_queue_init` was hardcoded to 64 SQ entries. Workloads that submit many async operations could exhaust the queue and stall. `mb_evloop_set_sq_size()` (in `src/aio/evloop.h`) lets callers configure the queue depth before `mb_evloop_init`; 0 restores the 64-entry default. On non-Linux backends the setter is a no-op.

### Changed

- **Deterministic OOM test behaviour under ASan** — CTest now sets `ASAN_OPTIONS=allocator_may_return_null=1` for every test. AddressSanitizer's default (`allocator_may_return_null=0`) aborts on absurdly large allocations instead of returning `NULL` the way production `malloc` does; the OOM-path tests (`test_hash`, `test_msg`, `test_arena`, `test_timeout::test_send_oom_large_body`) probe the library's `NULL`-return handling and were therefore aborting under ASan. The option makes ASan match production semantics. It is ignored on non-ASan builds, so plain Debug/Release runs are unaffected.
- **Chunk refcount-aware realloc** — `mb_chunk_realloc` now checks the refcount of the chunk being resized. If it is greater than 1 (shared), the function makes a full copy of the data into the new allocation instead of calling `realloc` directly. This prevents other in-flight references to the old buffer from becoming dangling pointers, which could otherwise produce use-after-free or double-free faults.
- **mb_errno thread-local as documented** — `mb_errno` was implemented as a global variable instead of the thread-local storage the documentation promised. It is now `thread_local int mb_errno` (C11) / `_Thread_local int mb_errno` (C99), matching the documented behaviour that each thread sees its own errno without mutex overhead.

### Fixed

- **CI platform portability** - Eventfd fallback writes now consume `read`/`write` return values so GCC Release and C-standard `-Werror` builds do not fail on `warn_unused_result`. macOS now uses a pthread mutex/condition-variable counting semaphore instead of deprecated unnamed POSIX semaphores and unavailable `sem_timedwait`. The CI matrix now tracks supported Unix-like targets and drops the currently non-building Windows jobs until the Windows PAL/socket layer is completed.
- **TLS/WSS client hostname verification and SNI** - When `MB_TLS_CONFIG_VERIFY` is enabled, the TLS and WSS client connect paths now (1) extract the host from the endpoint address (e.g. `tls://example.com:443`, `wss://[::1]:8443/path`) and (2) set the OpenSSL X.509 verification host parameter against that host (DNS name or literal IPv4/IPv6), enabling certificate identity checks against `CN`/`SAN` (or `iPAddress` SANs for literal IPs). For DNS hostnames the client also sends the Server Name Indication (SNI) extension so the server can present the correct virtual-host certificate. Wildcard matching is restricted to full-label wildcards only (`X509_CHECK_FLAG_NO_PARTIAL_WILDCARDS`) to avoid the historical `a*b` partial-match vulnerability. When verification is disabled, behavior is unchanged (no SNI, no hostname check) so existing insecure/test setups continue to work. `tests/unit/test_tls.c` and `tests/unit/test_wss.c` add matching/mismatch cases that connect to an in-process server with a leaf cert whose SAN is a DNS name and assert both the success and the failure path.
- **Gossip on_change callback use-after-free** - `mb_gossip_tick` invoked the user `on_change` callback while still holding the gossip mutex and handed it a pointer into the live `mb_gossip_node`. A user callback that called `mb_gossip_remove_node` (legitimate, e.g. on `MB_GOSSIP_NODE_DEAD`) would then free that node behind the callback's back, producing a classic use-after-free. The callback now runs with the lock released, and is passed only a snapshot of `node_id`, `addr`, `old_state`, and `new_state` so it never touches the live node. The signature is updated to `(void *ctx, uint32_t node_id, const char *addr, enum old_state, enum new_state)`; `mb_cluster_on_gossip_change` and the distributed test are updated to match. This removes the deadlock risk too: callbacks can now safely re-enter `mb_gossip_add_node` / `mb_gossip_remove_node`.
- **Consistent-hash ring duplicate vnodes and OOM unwind** - `mb_ring_add` did not check whether the `node_id` was already on the ring. A redelivered discovery broadcast therefore appended a second batch of virtual nodes, biasing the consistent-hash distribution. It also mishandled the OOM unwind path (called `mb_list_item_term` on list items it had not yet initialised, leaving the list in a bad state when a mid-loop allocation failed). The function now rejects duplicates with `-EEXIST`, and the OOM unwind walks the vnodes list forward to remove only the vnodes this call inserted.
- **ASan parallel-suite flake on `test_pipeline`** - The OOM-trigger tests (`test_msg`, `test_slab`, `test_pool`) deliberately exercise huge allocations. Under ASan this inflated the shadow-memory quarantine enough that, when ctest ran the suite in parallel, an unrelated test such as `test_pipeline` would fail its first `mb_bind` because the address space was exhausted, producing a spurious `Subprocess aborted`. These three tests are now marked `RUN_SERIAL` so they cannot run concurrently with the rest of the suite. The flake no longer reproduces across 5 consecutive `-j8` ASan runs.
- **Transport stop/reconnect data races** - TCP, IPC, TLS, WS, and WSS endpoint control loops now use `mb_atomic_int` for cross-thread `running` state instead of `volatile int`. The cancellable sleep, TCP/Unix connect, TLS handshake, and WebSocket handshake helpers now read those flags through `mb_atomic_load`, matching the existing worker/discovery/gossip atomic pattern and avoiding undefined behaviour during concurrent `stop()`/accept/reconnect paths without adding locks to hot data paths.
- **io_uring evloop re-arm bookkeeping** - The Linux io_uring event loop now stores fd/event/callback entries explicitly, re-arms poll requests from that metadata, and marks removed entries inactive so stale completions cannot invoke callbacks. This replaces the invalid `cqe->res >> 16` fd reconstruction and no-op remove path that could lose readiness notifications or dispatch callbacks after removal.
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
- **mb_close NULL guard** — `mb_close` now checks `g_self.socks` before dereferencing it and returns `-EBADF` instead of SIGSEGV when called on an invalid socket.
- **mb_term idempotent and lock-safe** — `mb_term` could race with concurrent `mb_term` calls or with library-initiated termination in worker threads. It now acquires the global init lock, checks an atomic `g_terminating` flag, and returns immediately if already shutting down; the cleanup path sets that flag and is itself idempotent.
- **aio fsm raise events that race ctx_term** — `mb_aio_ctx_term` cleared the `ctx->fsm` pointer before draining in-flight events, so an in-flight `mb_aio_fsm_raise` could write to freed memory. The terminate sequence now waits for all active events to drain before clearing the pointer.
- **gossip worker wakeup on stop** — `mb_gossip_stop` woke the gossip worker with `usleep(10000)` polling, causing up to 10 ms of unnecessary delay before the worker noticed the stop signal and exited its loop. The stop path now signals the same condition variable the worker waits on, so the worker wakes immediately.
- **Slab magic header for free validation** — Each slab object is now stamped with a magic constant (`MB_SLAB_MAGIC`) and its allocation size at the start of the allocation. `mb_slab_free` validates both fields before freeing; a mismatch (corruption or double-free) triggers `abort()`. This mirrors the `mb_arena` guard-page pattern.
- **Build hidden symbol visibility** — Non-public symbols in the shared library are now hidden by default (`__attribute__((visibility("hidden")))`) on GCC and Clang. Only the explicitly `MB_EXPORT`-tagged public API surface is exposed. A new link-time test (`tests/unit/test_symbol_visibility`) asserts that a hardcoded list of internal symbols cannot be dlsym'd from the loaded library.

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
