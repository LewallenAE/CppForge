# C++ Systems Engineering

A collection of production-minded C++20 systems projects focused on the parts of C++ that matter most for systems engineering:

**ownership • lifetime • concurrency • networking • persistence • debugging • performance**

The projects are intentionally progressive. Each one adds another systems boundary while keeping the implementation small enough to understand, test, benchmark, and defend technically.

> The goal is not to build the largest projects possible.
>
> The goal is to build software correctly, understand how it works, measure it, harden the important parts, and ship it.

---

## Projects

| Project | Status | Primary Focus |
|---|---|---|
| [LogForge](./LogForge) | Shipped v1 | Streaming I/O, parsing, STL, testing, performance |
| [TaskForge](./TaskForge) | Shipped v1 | Concurrency, bounded queues, worker pools, synchronization |
| [CinderDB](./CinderDB) | Shipped v1 | TCP networking, concurrent state, WAL persistence, recovery |
| [EmberMQ](./EmberMQ) | In Progress | Integrated C++ systems engineering / message-broker architecture |

---

# LogForge

**Streaming Log Analysis CLI**

LogForge is a C++20 command-line log analysis system designed to process large log streams without loading the entire input into memory.

It provides typed log parsing, malformed-record recovery, composable filtering, and deterministic summaries while keeping memory usage essentially independent of input size.

### Engineering focus

- streaming file I/O
- typed parsing
- `std::optional`
- STL containers and algorithms
- level and service filtering
- malformed-input recovery
- deterministic aggregation
- CMake
- automated tests
- AddressSanitizer
- UndefinedBehaviorSanitizer
- Release benchmarking

### Measured result

A Release build processed:

```text
10,000,000 records
699.4 MiB input
1.09 seconds
~9.17 million records/sec
3,712 KiB maximum RSS
```

The benchmark is environment-specific and is included as an engineering measurement rather than a universal performance claim.

[View LogForge →](./LogForge)

---

# TaskForge

**Bounded Multithreaded Task Execution Engine**

TaskForge is a C++20 worker-pool execution engine built around a bounded blocking FIFO queue.

The project focuses on the difficult parts of concurrent C++: ownership across threads, synchronization, backpressure, result propagation, exception handling, and deterministic shutdown.

### Architecture

```text
Producers
    │
    ▼
Bounded FIFO Queue
    │
    ▼
Worker Pool
    │
    ▼
Task Execution
    │
    ├── Results
    └── Exceptions
```

### Engineering focus

- `std::thread`
- mutexes
- condition variables
- predicate-based waiting
- bounded queues
- producer backpressure
- typed futures
- move-only callables
- exception propagation
- deterministic shutdown
- stress testing
- ThreadSanitizer
- AddressSanitizer / UBSan
- concurrency benchmarking

### Correctness validation

A stress workload executed:

```text
12,000 submitted tasks
4 concurrent producers
8 workers
11,953 successful tasks
47 intentional failures
```

Every submitted task was accounted for exactly once:

```text
12,000 = 11,953 + 47
```

The implementation was also exercised under ThreadSanitizer and ASan/UBSan.

### Measured result

On the deterministic CPU workload used for the project, the best measured median was approximately:

```text
427,128 tasks/sec
2 workers
100,000 tasks
```

Scaling was measured rather than assumed; higher worker counts did not produce linear speedup under the test environment.

[View TaskForge →](./TaskForge)

---

# CinderDB

**Concurrent Networked Key-Value Storage Engine**

CinderDB is a C++20 TCP key-value service combining networking, concurrency, explicit OS-resource ownership, and durable storage.

It supports persistent client connections and a small newline-framed protocol:

```text
PUT <key> <value>
GET <key>
DELETE <key>
```

### Architecture

```text
Clients
   │
   ▼
TCP Listener
   │
   ▼
Bounded Connection Queue
   │
   ▼
Worker Pool
   │
   ▼
Protocol Parser
   │
   ▼
Thread-Safe Key/Value Store
   │
   ▼
Write-Ahead Log
```

### Engineering focus

- POSIX TCP sockets
- persistent connections
- newline framing
- partial-read handling
- bounded request sizes
- fixed worker pool
- bounded connection queue
- thread-safe in-memory state
- move-only RAII file-descriptor ownership
- append-only write-ahead logging
- `fsync`-before-ack durability
- startup recovery
- incomplete-tail handling
- WAL corruption detection
- graceful `SIGINT` / `SIGTERM` shutdown
- ASan / UBSan
- ThreadSanitizer
- concurrent socket testing
- localhost load testing

### Ownership

File descriptors are wrapped in a move-only `UniqueFd` abstraction.

The wrapper:

- cannot be copied
- can transfer ownership through moves
- closes the descriptor deterministically on destruction

The server owns its listener, worker threads, connection queue, accept thread, WAL, and store without detached threads or raw owning pointers.

### Durability model

Mutating operations follow:

```text
append WAL record
        ↓
fsync WAL
        ↓
mutate in-memory state
        ↓
send OK
```

The WAL contains structured records with checksums.

During recovery:

- valid records are replayed in order
- an incomplete final record is ignored
- corruption in a complete record causes recovery to fail rather than silently accepting damaged state

### Validation

CinderDB passed:

```text
Debug build        PASS
Release build      PASS
ASan / UBSan       PASS
ThreadSanitizer    PASS
Socket integration PASS
Restart recovery   PASS
```

Concurrent validation included:

```text
8 TCP clients
640 persisted mutations / reads
restart verification
0 protocol failures
0 crashes
0 deadlocks
```

### Measured performance

Release build on localhost WSL2 with 16 server workers and 16 clients:

| Workload | Operations | Throughput | Errors |
|---|---:|---:|---:|
| Missing-key GET | 32,000 | 1,347 ops/s | 0 |
| Durable PUT (`fsync` per mutation) | 3,200 | 249 ops/s | 0 |

The durable-write result intentionally reflects the cost of serialized `fsync`-before-ack semantics rather than hiding durability behind buffering.

[View CinderDB →](./CinderDB)

---

# EmberMQ

**Concurrent Persistent Message Broker — In Progress**

EmberMQ is the long-running C++ systems-learning project in this repository.

Unlike the other projects, its purpose is not simply to reach a feature checklist quickly. EmberMQ is being built incrementally as C++ concepts become natural enough to implement from a blank editor.

Conceptually:

```text
Producers
    │
    ▼
TCP Server
    │
    ▼
Protocol Parser
    │
    ▼
Topic Registry
    │
    ▼
Bounded Message Queues
    │
    ├───────────────┐
    ▼               ▼
Consumers       Persistence
                    │
                    ▼
                Append Log
```

The eventual system is intended to explore:

- ownership and object lifetime
- RAII
- copy and move semantics
- STL data-structure engineering
- bounded queues
- producer backpressure
- worker lifecycle
- mutexes and condition variables
- TCP framing
- socket ownership
- multiple clients
- append-only persistence
- recovery
- metrics
- sanitizers
- load testing
- profiling and benchmarking

Features are documented as complete only after they are actually implemented and validated.

[View EmberMQ →](./EmberMQ)

---

# Engineering Progression

The first three projects form a deliberate systems-engineering ladder:

```text
LogForge
Professional C++ application engineering
streaming • parsing • STL • testing
                │
                ▼
TaskForge
Concurrent systems C++
threads • queues • synchronization • backpressure
                │
                ▼
CinderDB
Networked + persistent systems C++
TCP • POSIX • concurrency • storage • recovery
```

EmberMQ runs alongside that progression as the permanent blank-editor learning project where those concepts are reconstructed and integrated deliberately.

---

# Engineering Principles

Across the repository, the emphasis is on:

- C++20
- explicit ownership
- RAII and deterministic cleanup
- const-correct interfaces
- simple, understandable architecture
- strict compiler warnings
- meaningful automated tests
- ASan / UBSan where applicable
- TSan for concurrent code
- failure-path testing
- benchmarking before optimization
- measuring scalability rather than assuming it
- avoiding unnecessary frameworks and abstractions

Performance work follows:

```text
correct implementation
        ↓
benchmark
        ↓
measurement
        ↓
hypothesis
        ↓
change
        ↓
re-measure
        ↓
conclusion
```

---

# Repository Layout

```text
.
├── LogForge/
│   ├── src/
│   ├── tests/
│   ├── CMakeLists.txt
│   └── README.md
│
├── TaskForge/
│   ├── include/
│   ├── src/
│   ├── tests/
│   ├── CMakeLists.txt
│   └── README.md
│
├── CinderDB/
│   └── ...
│
├── EmberMQ/
│   └── ...
│
└── README.md
```

Each project is intentionally self-contained.

See the README inside each project directory for its exact:

```text
build
run
test
sanitizer
benchmark
architecture
```

instructions.

---

## Toolchain

Primary development environment:

```text
C++20
GCC / G++
Linux / WSL
CMake
GDB
AddressSanitizer
UndefinedBehaviorSanitizer
ThreadSanitizer
```

Additional Linux/POSIX facilities are introduced where the project requires them rather than hidden behind unnecessary frameworks.

---

## Why This Repository Exists

This repository documents a progression toward being able to reason about C++ systems software at multiple levels:

```text
source code
↓
types and object lifetime
↓
resource ownership
↓
threads and synchronization
↓
system calls
↓
TCP
↓
storage and durability
↓
hardware and operating-system behavior
↓
measured performance
```

The standard is not:

> "I completed a C++ course."

The standard is:

> **I can build, debug, reason about, measure, and ship systems software in modern C++.**
