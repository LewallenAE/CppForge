# CinderDB

CinderDB is a C++20 TCP key-value service for Linux/WSL. It combines newline-framed socket requests, a fixed connection-worker pool, a synchronized in-memory map, and an append-only write-ahead log (WAL) that fsyncs every successful mutation.

```text
TCP clients → listener → bounded connection queue → worker pool → parser
                                                              ├─ KV store
                                                              └─ fsync WAL
```

## Protocol

Every request is a UTF-8 text line terminated by `\n`; CRLF is accepted. A line is limited to 4,096 bytes. Keys are 1–64 characters from `[A-Za-z0-9_.-]`. PUT values are nonempty and may contain spaces.

```text
PUT <key> <value...>  → OK
GET <key>             → VALUE <value> | NOT_FOUND
DELETE <key>          → OK
malformed request     → ERROR <reason>
```

TCP is treated as a byte stream: each connection buffers partial/coalesced reads and extracts newline frames. An oversized line receives `ERROR request too large` and its connection closes.

## Concurrency and ownership

The accept thread puts move-only `UniqueFd` client sockets into a bounded queue. Fixed workers pop one connection and serve its sequential request stream until disconnect. This bounds threads and queued sockets, but long-lived idle clients occupy workers; size workers for expected concurrent connections.

`UniqueFd` is non-copyable, movable, and closes its descriptor in its destructor. The server owns its listener, worker threads, accept thread, queue, and store. No threads are detached.

The store uses one mutex. PUT/DELETE lock it, append a WAL record, `fsync`, mutate the map, then return `OK`. GET copies its value while locked. The WAL mutex is acquired only while the store mutex is held; recovery runs before serving. This coarse policy is easy to audit, at the cost of serializing durable mutations and reads during fsync.

## Persistence and recovery

WAL records are deterministic binary data:

```text
magic "CDB1" | operation byte | big-endian key length | value length | payload | FNV-1a checksum
```

PUT and DELETE replay in file order at startup. A partial final header/payload is an incomplete crash tail and is ignored. Invalid headers, lengths, operations, or checksums are corruption and fail startup. Every successful mutation has been appended and fsynced before `OK` is sent.

## Shutdown

SIGINT/SIGTERM handlers only set an async-signal-safe flag. The main loop invokes `stop()`, which shuts down the listener, closes the connection queue, wakes workers, joins threads, and closes descriptors. Idle reads have a short receive timeout so workers observe shutdown. Accepted queued/idle connections may close during shutdown; already acknowledged mutations are durable.

## Build and tests

Requires CMake 3.20+, a C++20 compiler, and POSIX sockets; tested with GCC 13.3.0 on WSL2.

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build --output-on-failure

cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release
cmake --build build-release
ctest --test-dir build-release --output-on-failure
```

Targets link `Threads::Threads` and use `-Wall -Wextra -Wconversion -Wsign-conversion -Werror -pedantic-errors` on GCC/Clang.

```bash
cmake -S . -B build-sanitize -DCMAKE_BUILD_TYPE=Debug -DCINDERDB_ENABLE_SANITIZERS=ON
cmake --build build-sanitize
ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 ctest --test-dir build-sanitize --output-on-failure

cmake -S . -B build-tsan -DCMAKE_BUILD_TYPE=Debug -DCINDERDB_ENABLE_TSAN=ON
cmake --build build-tsan
setarch x86_64 -R bash -lc 'TSAN_OPTIONS=halt_on_error=1 ctest --test-dir build-tsan --output-on-failure'
```

On this WSL2 host GCC TSan needs `setarch x86_64 -R`; without it, the runtime aborts before `main` with `FATAL: ThreadSanitizer: unexpected memory mapping`. LeakSanitizer is disabled in the shown ASan run because it cannot operate under the validation environment’s ptrace wrapper.

## Running and clients

```bash
./build-release/cinderdb --port 6380 --data ./data/cinder.wal --workers 8 --queue-capacity 64
printf 'PUT user Anthony\nGET user\nDELETE user\nGET user\n' | nc -N 127.0.0.1 6380
```

Expected replies: `OK`, `VALUE Anthony`, `OK`, and `NOT_FOUND`.

## Test coverage

The C++ suite tests parsing, FD move ownership, store overwrite/delete, concurrent direct writes, WAL replay, incomplete-tail recovery, corruption rejection, and socket-level PUT/GET/DELETE. Its network stress path runs 8 concurrent clients through 640 persisted mutations/reads, then restarts the server and verifies both persisted PUT and DELETE state.

## Load testing and measurements

The included generator keeps one TCP connection per client:

```bash
python3 tools/load_test.py --host 127.0.0.1 --port 6380 --clients 16 --requests 2000 --mode get
python3 tools/load_test.py --host 127.0.0.1 --port 6380 --clients 16 --requests 200 --mode put
```

Measured Release results: 16 server workers, 16 localhost Python clients, GCC 13.3.0, Linux 6.6.87.2 under WSL2.

| Workload | Operations | Elapsed | Ops/s | Errors |
|---|---:|---:|---:|---:|
| GET missing-key network throughput | 32,000 | 23.753110 s | 1,347.19 | 0 |
| PUT, WAL append + fsync per mutation | 3,200 | 12.872369 s | 248.59 | 0 |

The durable result is deliberately lower because the store serializes WAL fsync with the map update. These are localhost/WSL2 measurements, not comparisons to Redis.

## Tradeoffs

v1 chooses a worker pool rather than thread-per-client, newline framing over a complex protocol, copied GET values for safe lifetimes, coarse store locking for auditable WAL/map ordering, and per-mutation fsync for a simple durability contract. Sharding, batching, and alternate connection scheduling are explicitly outside v1 until measurement justifies them.
