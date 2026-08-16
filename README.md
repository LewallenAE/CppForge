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
| [CinderDB](./CinderDB) | In Development | TCP networking, concurrent state, persistence, recovery |
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

CinderDB is the networking and storage stage of the project ladder.

It is being built as a small TCP key-value service rather than an attempt to recreate Redis or another production database.

The architecture is centered around:

```text
Clients
   │
   ▼
TCP Socket Layer
   │
   ▼
Protocol Parser
   │
   ▼
Concurrent Execution
   │
   ▼
Key / Value Store
   │
   ▼
Persistence
```

### Development targets

- POSIX TCP sockets
- newline-framed request protocol
- `PUT`, `GET`, and `DELETE`
- multiple concurrent clients
- explicit file-descriptor ownership
- RAII resource wrappers
- thread-safe in-memory state
- append-only write-ahead logging
- restart recovery
- malformed-request handling
- graceful shutdown
- concurrency stress tests
- sanitizer validation
- network load testing
- latency and throughput measurement

CinderDB will only be marked **Shipped v1** once networking, persistence, recovery, concurrency validation, sanitizers, and load testing have all passed.

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
