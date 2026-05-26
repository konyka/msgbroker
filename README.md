# msgbroker

High-performance messaging library in pure C (C99-C23).

A complete reimplementation of the nanomsg SP protocol family with memory pools, thread pools, setjmp/longjmp coroutines, io_uring support, and distributed cluster capabilities.

## Features

- **Pure C** — C99 through C23 compatible, `-Wall -Wextra -Werror` clean
- **10 Protocol Socket Types** — PAIR, PUSH/PULL, REQ/REP, PUB/SUB, BUS, SURVEYOR/RESPONDENT
- **6 Transports** — inproc (zero-copy), IPC (Unix domain), TCP, TLS (OpenSSL), WebSocket (RFC 6455), WSS (WebSocket Secure)
- **IPv6 + DNS** — Dual-stack IPv4/IPv6, hostname resolution via `getaddrinfo`
- **Auto-Reconnect** — Exponential backoff with configurable intervals on all connect transports (TCP, TLS, WS, WSS)
- **io_uring** — Runtime detection with automatic fallback to epoll on Linux
- **Memory Pool** — Custom allocator with slab/arena/chunkref for zero-copy messaging
- **Thread Pool** — Per-worker event loops with task queues
- **Coroutines** — Stackful ucontext (Unix) / Win32 Fibers cooperative scheduling
- **Distributed** — SWIM gossip membership, UDP multicast discovery, consistent hashing ring, cluster routing

## Build

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
```

### Install

```bash
cmake --install build --prefix=/usr/local
pkg-config --cflags --libs msgbroker
```

### Build Options

| Option | Default | Description |
|--------|---------|-------------|
| `MB_BUILD_TESTS` | ON | Build test suite |
| `MB_BUILD_EXAMPLES` | ON | Build examples |
| `MB_BUILD_PERF` | ON | Build benchmarks |
| `MB_BUILD_FUZZ` | OFF | Build fuzz testers |
| `MB_STATIC_LIB` | ON | Build static library |
| `MB_SHARED_LIB` | ON | Build shared library |
| `MB_C_STANDARD` | 99 | C standard (99, 11, 17, 23) |

## Quick Start

```c
#include <msgbroker/mb.h>
#include <msgbroker/mb_pipeline.h>

int main (void)
{
    int push = mb_socket (AF_MB, MB_PUSH);
    int pull = mb_socket (AF_MB, MB_PULL);

    mb_bind (pull, "tcp://127.0.0.1:9000");
    mb_connect (push, "tcp://127.0.0.1:9000");

    mb_send (push, "HELLO", 5, 0);

    char buf[64];
    int n = mb_recv (pull, buf, sizeof (buf), 0);

    mb_close (push);
    mb_close (pull);
    return 0;
}
```

## API

### Socket Lifecycle

| Function | Description |
|----------|-------------|
| `mb_socket(AF_MB, type)` | Create a socket |
| `mb_close(s)` | Close a socket |
| `mb_bind(s, addr)` | Bind to address |
| `mb_connect(s, addr)` | Connect to address |
| `mb_shutdown(s, how)` | Shutdown socket direction |

### Messaging

| Function | Description |
|----------|-------------|
| `mb_send(s, buf, len, flags)` | Send a message |
| `mb_recv(s, buf, len, flags)` | Receive a message |
| `mb_sendmsg(s, msghdr, flags)` | Scatter/gather send |
| `mb_recvmsg(s, msghdr, flags)` | Scatter/gather receive with cmsg |
| `mb_coro_send(s, buf, len)` | Coroutine-aware send |
| `mb_coro_recv(s, buf, len)` | Coroutine-aware recv |

### Socket Options

| Function | Description |
|----------|-------------|
| `mb_setsockopt(s, level, opt, val, len)` | Set socket option |
| `mb_getsockopt(s, level, opt, val, len)` | Get socket option |

Key options: `MB_SNDTIMEO`, `MB_RCVTIMEO`, `MB_SNDBUF`, `MB_RCVBUF`, `MB_SNDFD`, `MB_RCVFD`, `MB_RECONNECT_IVL`, `MB_RECONNECT_IVL_MAX`

### Send/Recv Flags

| Flag | Description |
|------|-------------|
| `MB_DONTWAIT` | Non-blocking operation |

### Multiplexing

| Function | Description |
|----------|-------------|
| `mb_poll(fds, nfds, timeout)` | Poll multiple sockets |
| `mb_device(s1, s2)` | Forward messages between sockets |

### Socket Types

| Type | Header | Pattern |
|------|--------|---------|
| `MB_PAIR` | `mb_pair.h` | Bidirectional 1:1 |
| `MB_PUSH` / `MB_PULL` | `mb_pipeline.h` | Unidirectional pipeline (fan-out) |
| `MB_REQ` / `MB_REP` | `mb_reqrep.h` | Request-reply |
| `MB_PUB` / `MB_SUB` | `mb_pubsub.h` | Publish-subscribe |
| `MB_BUS` | `mb_bus.h` | Mesh broadcast |
| `MB_SURVEYOR` / `MB_RESPONDENT` | `mb_survey.h` | Survey pattern |

### Transport Addresses

| Transport | Format | Header |
|-----------|--------|--------|
| inproc | `inproc://name` | `mb_inproc.h` |
| IPC | `ipc:///path/to/socket` | `mb_ipc.h` |
| TCP | `tcp://host:port` | `mb_tcp.h` |
| TLS | `tls://host:port` | `mb_tls.h` |
| WebSocket | `ws://host:port` | `mb_ws.h` |
| WSS | `wss://host:port` | `mb_wss.h` |

Host can be IPv4 (`127.0.0.1`), IPv6 (`[::1]`), or a DNS hostname (`localhost`).

### TLS Configuration

```c
mb_setsockopt(s, MB_TLS, MB_TLS_CONFIG_CERT, "server.pem", 11);
mb_setsockopt(s, MB_TLS, MB_TLS_CONFIG_KEY, "server.key", 11);
mb_setsockopt(s, MB_TLS, MB_TLS_CONFIG_CA, "ca.pem", 7);
int verify = 1;
mb_setsockopt(s, MB_TLS, MB_TLS_CONFIG_VERIFY, &verify, sizeof(verify));
```

### Auto-Reconnect

All connect endpoints (TCP, TLS, WS, WSS) support automatic reconnection with exponential backoff:

```c
int ivl = 100;    /* initial retry interval in ms */
int ivl_max = 5000; /* max retry interval (0 = no cap) */
mb_setsockopt(s, MB_SOL_SOCKET, MB_RECONNECT_IVL, &ivl, sizeof(ivl));
mb_setsockopt(s, MB_SOL_SOCKET, MB_RECONNECT_IVL_MAX, &ivl_max, sizeof(ivl_max));
mb_connect(s, "tcp://127.0.0.1:9000");  /* starts retrying in background */
```

Set `MB_RECONNECT_IVL` to `0` to disable auto-reconnect (returns error on connection failure).

### Cluster (Distributed)

| Function | Description |
|----------|-------------|
| `mb_cluster_join(s, addr)` | Join a cluster |
| `mb_cluster_leave(s)` | Leave the cluster |
| `mb_cluster_route(s, key, len)` | Route key to a node |

### Version & Utilities

| Function | Description |
|----------|-------------|
| `mb_errno()` | Get last error |
| `mb_strerror(errnum)` | Error string |
| `mb_version_major()` | Major version |
| `mb_version_minor()` | Minor version |
| `mb_version_patch()` | Patch version |
| `mb_version_string()` | Version string ("0.2.0") || `mb_term()` | Shutdown library |

## Architecture

```
                    ┌─────────────────┐
                    │  Public API     │  mb_socket, mb_send, mb_recv, ...
                    ├─────────────────┤
                    │  Protocol Layer │  PAIR, PUSH/PULL, REQ/REP, ...
                    ├─────────────────┤
                    │  Core           │  sock, pipe, endpoint, poll, device
                    ├─────────────────┤
                    │  AIO            │  FSM, evloop, timer, worker, coroutine
                    ├─────────────────┤
                    │  Transport      │  inproc, IPC, TCP, TLS, WebSocket, WSS
                    ├─────────────────┤
                    │  Distributed    │  gossip, discovery, ring, routing, cluster
                    ├─────────────────┤
                    │  Memory         │  pool, slab, arena, chunkref, msg
                    ├─────────────────┤
                    │  Utilities      │  list, queue, hash, trie, wire, lb, fq
                    ├─────────────────┤
                    │  PAL            │  mutex, condvar, thread, efd, clock, atomic
                    └─────────────────┘
```

## Test Suite

46+ tests covering all layers:

- PAL: atomic, mutex, condvar, thread, efd, clock, semaphore
- Data structures: list, queue, hash, trie, wire, chunk, msg, slab, arena, pool
- AIO: fsm, evloop, timer, threadpool, coroutine
- Core: global, socket, endpoint, pipe, poll, device
- Transport: inproc, IPC, TCP, WebSocket, TLS, WSS
- Network: IPv6, DNS hostname resolution, dual-stack
- Protocol: pipeline (PUSH/PULL), reqrep (REQ/REP), protocols (PUB/SUB, BUS, survey)
- Distributed: ring, gossip, protocol serialization
- API: strerror, sendmsg/recvmsg, cmsg, coro_io, timeout/version, poll, TLS cert, reconnect

## Benchmarks

64-byte messages, Release build, TCP over loopback:

| Benchmark | Messages | Throughput | Latency |
|-----------|----------|------------|---------|
| inproc_thr (PAIR) | 1,000,000 | 12.6M msg/sec (768 MB/sec) | 0.08 us round-trip |
| inproc_lat (PAIR) | 100,000 | 12.6M msg/sec | 0.04 us one-way |
| tcp_thr (PAIR, loopback) | 100,000 | 159K msg/sec (9.7 MB/sec) | 6.27 us round-trip |
| tcp_lat (PAIR, loopback) | 100,000 | 159K msg/sec | 3.14 us one-way |

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release -DMB_BUILD_PERF=ON
cmake --build build
./build/perf/inproc_thr
./build/perf/tcp_thr
```

## License

MIT
