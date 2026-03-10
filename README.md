# Reactor Server

C++20 epoll-based server that can run echo traffic and simple KV-cache workloads.

The codebase is compact, but it still covers the core mechanics of a production-style event-driven server:

- edge-triggered `epoll` reactor with explicit connection lifecycle
- per-connection `EventHandler` abstraction
- allocator-free hot path I/O buffers in the connection and KV handler
- in-memory KV cache with LRU-style recency tracking
- framed request parsing with deterministic partial read and partial write handling

## Layout

- [src/reactor.cpp](src/reactor.cpp): main event loop, epoll dispatch, connection shutdown
- [src/connection.cpp](src/connection.cpp): non-blocking `recv`/`send`, write buffering, handler integration
- [include/utils/event_handler.hpp](include/utils/event_handler.hpp): pluggable per-connection interface
- [src/kvcache/cache.cpp](src/kvcache/cache.cpp): hashmap + list cache backend
- [src/kvcache/kv_handler.cpp](src/kvcache/kv_handler.cpp): KV protocol framing and handler-side buffering
- [tester.cpp](tester.cpp): cache, protocol, handler, and socket-level checks

## Build

```bash
cmake -S . -B build
cmake --build build
```

## Run

Start the server:

```bash
./build/reactor
```

Run the test binary:

```bash
./build/tester
```

## KV Protocol

Wire format:

```text
OP#KEY[#VALUE]\r\n
```

Supported operations:

- `SET#key#value\r\n`
- `GET#key\r\n`
- `DEL#key\r\n`
- `MOD#key#value\r\n`
- `EXIST#key\r\n`

Example:

```text
SET#alpha#42\r\n
GET#alpha\r\n
```

Responses are ASCII status lines or values terminated by `\r\n`.

## Notes

- Why ET `epoll` requires draining `recv`/`send` until `EAGAIN`
- Why partial reads and writes must be modeled explicitly in the protocol layer
- Tradeoffs of linear bounded buffers versus true ring buffers
- How the current `EventHandler` split supports multiple protocols on the same reactor core
- Next steps: benchmark harness, ring-buffer cleanup, and more explicit backpressure limits
