# msgbroker

High-performance messaging library in pure C (C99-C23).

A complete reimplementation of the nanomsg SP protocol family with memory pools, thread pools, setjmp/longjmp coroutines, io_uring support, and distributed cluster capabilities.

## Features

- **Pure C** — C99 through C23 compatible, `-Wall -Wextra -Werror` clean
- **10 Protocol Socket Types** — PAIR, PUSH/PULL, REQ/REP, PUB/SUB, BUS, SURVEYOR/RESPONDENT
- **3 Transports** — inproc (zero-copy), IPC (Unix domain), TCP
- **io_uring** — Runtime detection with automatic fallback to epoll on Linux
- **Memory Pool** — Custom allocator with slab/arena/chunkref for zero-copy messaging
- **Thread Pool** — Per-worker event loops with task queues
- **Coroutines** — Stackful ucontext-based cooperative scheduling (setjmp/longjmp replaced)
- **Distributed** — SWIM gossip membership, UDP multicast discovery, consistent hashing ring, cluster routing

## Build

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
```

### Options

| Option | Default | Description |
|--------|---------|-------------|
| `MB_BUILD_TESTS` | ON | Build test suite |
| `MB_BUILD_EXAMPLES` | ON | Build examples |
| `MB_BUILD_PERF` | ON | Build benchmarks |
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
| `mb_send(s, buf, len, 0)` | Send a message |
| `mb_recv(s, buf, len, 0)` | Receive a message |

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

| Transport | Format |
|-----------|--------|
| inproc | `inproc://name` |
| IPC | `ipc:///path/to/socket` |
| TCP | `tcp://host:port` |

### Cluster (Distributed)

| Function | Description |
|----------|-------------|
| `mb_cluster_join(s, addr)` | Join a cluster |
| `mb_cluster_leave(s)` | Leave the cluster |
| `mb_cluster_route(s, key, len)` | Route key to a node |

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
                    │  Transport      │  inproc, IPC, TCP
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

37 tests covering all layers:

- PAL: atomic, mutex, condvar, thread, efd, clock, semaphore
- Data structures: list, queue, hash, trie, wire, chunk, msg, slab, arena, pool
- AIO: fsm, evloop, timer, threadpool, coroutine
- Core: global, socket, endpoint, pipe, poll, device
- Transport: inproc, IPC, TCP
- Protocol: pipeline (PUSH/PULL), reqrep (REQ/REP), protocols (PUB/SUB, BUS, survey)
- Distributed: ring, gossip, protocol serialization

## Benchmarks

64-byte messages, Release build, TCP over loopback:

| Benchmark | Messages | Throughput | Latency |
|-----------|----------|------------|---------|
| inproc_thr (PAIR) | 1,000,000 | 12.2M msg/sec (744 MB/sec) | 0.08 us round-trip |
| inproc_lat (PAIR) | 100,000 | 12.5M msg/sec | 0.04 us one-way |
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
