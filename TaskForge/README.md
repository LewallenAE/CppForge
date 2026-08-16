# TaskForge

TaskForge is a C++20 bounded task-execution engine with a fixed worker pool. The reusable
executor accepts move-only callables, applies queue backpressure, returns typed futures,
propagates task exceptions, exposes runtime metrics, and shuts down by draining all accepted
work. The CLI runs deterministic simulated jobs or a deterministic CPU benchmark.

## Architecture

```text
producer(s) -- submit() --> BoundedQueue<packaged_task<void()>>
                                  │
                   ┌──────────────┼──────────────┐
                   ▼              ▼              ▼
                worker         worker          worker
                   │              │              │
                   └──── promise / exception ────┘
                                  │
                                  ▼
                            typed std::future
```

- `bounded_queue.hpp` is a generic, mutex-protected FIFO with fixed capacity and close/drain semantics.
- `task_executor.hpp` / `task_executor.cpp` own worker threads, submission state, futures, metrics, and shutdown.
- `job.hpp` / `job.cpp` parse the CLI's `<task-id> <duration-ms> [fail]` workload.
- `cli.hpp` / `cli.cpp` validate commands, options, and positive numeric configuration.
- `main.cpp` runs files, collects results, reports failures, and implements the CPU benchmark.
- `tests/` covers components, concurrent state transitions, stress invariants, and executable behavior.

## Concurrency model

`BoundedQueue` protects its deque, closed flag, and depth metrics with one mutex. Producers
wait on `not_full`; consumers wait on `not_empty`. Every wait uses a predicate, so spurious
wakeups are safe. Push and pop release the mutex before notifying, and workers execute tasks
only after `pop` has released the queue lock. There is no polling or sleep-based queue
synchronization.

When the queue reaches capacity, `submit` blocks the producer until a worker removes an item
or shutdown closes the queue. Closing broadcasts to both condition variables. Waiting
producers return a predictable `SubmissionRejected`; consumers drain already accepted items
and then stop.

An additional submission-state mutex makes shutdown atomic with respect to admission.
Shutdown stops new admission, closes the queue, waits for in-flight submissions to resolve,
joins every worker, and then returns stable final metrics. Repeated shutdown calls are safe.

## Ownership and results

`TaskExecutor` exclusively owns its queue and joinable `std::thread` objects. It is neither
copyable nor movable. Queue entries are move-only `std::packaged_task<void()>` wrappers. Each
wrapper owns the submitted callable and a typed `std::promise`; its matching `std::future`
is returned to the caller. No shared ownership or detached thread is used.

A callable exception is caught inside its task wrapper, counted as a failure, and installed
on the promise. Calling `future.get()` rethrows it without terminating a worker. Successful
tasks publish their value, reference, or void completion normally.

The owning thread must keep the executor alive while submitting and must not call
`shutdown()` or destroy the executor from one of its own worker tasks, because shutdown joins
the worker set. The destructor calls shutdown automatically from the owning thread.

## Build and test

Requirements:

- CMake 3.20 or newer
- A C++20 compiler with pthread support (tested with GCC 13.3.0)

Debug:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build --output-on-failure
```

Release:

```bash
cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release
cmake --build build-release
ctest --test-dir build-release --output-on-failure
```

The build uses `Threads::Threads` and applies `-Wall -Wextra -Wconversion
-Wsign-conversion -Werror -pedantic-errors` to project targets on GCC and Clang.

### ThreadSanitizer

```bash
cmake -S . -B build-tsan \
  -DCMAKE_BUILD_TYPE=Debug \
  -DTASKFORGE_ENABLE_TSAN=ON
cmake --build build-tsan
TSAN_OPTIONS=halt_on_error=1 \
ctest --test-dir build-tsan --output-on-failure
```

GCC TSan on the recorded WSL2 environment initially terminated before `main` with
`FATAL: ThreadSanitizer: unexpected memory mapping`. Disabling address randomization for the
test process allowed the runtime to initialize and the complete suite to pass cleanly:

```bash
setarch x86_64 -R bash -lc \
  'TSAN_OPTIONS=halt_on_error=1 ctest --test-dir build-tsan --output-on-failure'
```

### AddressSanitizer and UndefinedBehaviorSanitizer

```bash
cmake -S . -B build-sanitize \
  -DCMAKE_BUILD_TYPE=Debug \
  -DTASKFORGE_ENABLE_SANITIZERS=ON
cmake --build build-sanitize
ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 \
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
ctest --test-dir build-sanitize --output-on-failure
```

Leak detection was disabled because LeakSanitizer cannot operate under the validation
environment's ptrace wrapper. Address and undefined-behavior checks remain enabled.

## CLI

Job-file format:

```text
<task-id> <duration-ms> [fail]
1 100
2 25
3 75 fail
```

Task IDs must be unique within a file. The optional literal `fail` makes that deterministic
task throw after its simulated duration, which demonstrates exception propagation.

```bash
./build-release/taskforge run sample/jobs.txt --workers 3 --queue-capacity 2
./build-release/taskforge run sample/jobs-with-failure.txt --workers 2 --queue-capacity 1
./build-release/taskforge benchmark --workers 8 --tasks 100000 --queue-capacity 256
```

Normal results go to `stdout`; invocation and file diagnostics go to `stderr`. Invalid CLI
input returns 2, file/open failures return 1, and a completed run containing failed tasks
returns 3.

Example successful output:

```text
task=1 status=completed
task=2 status=completed
task=3 status=completed
task=4 status=completed
task=5 status=completed
workers=3 queue_capacity=2 peak_queue_depth=2 current_queue_depth=0 submitted=5 completed=5 failed=0 elapsed_ms=100.62
```

## Stress testing

The test executable launches 4 producer threads into an executor with 8 workers and queue
capacity 7. It submits 12,000 deterministic IDs, including 47 intentional failures, then
verifies every ID executed exactly once and checks:

```text
submitted == completed + failed
12000 == 11953 + 47
```

This suite passed ten consecutive Debug repetitions and also passed under ASan/UBSan and
TSan. Other tests use latches and condition variables—not timing sleeps—to exercise full and
empty queue waits, wakeups, close behavior, concurrent execution, blocked-submit shutdown,
exception recovery, and destructor draining.

## Benchmark

The Release benchmark submits 100,000 deterministic CPU tasks. Each task performs 2,048
iterations of integer mixing and returns a value consumed into a final checksum. Timing
includes executor construction, submission/backpressure, execution, shutdown, and future
collection. Three sequential samples were taken for each worker count with queue capacity
256:

```bash
bash tools/run_benchmarks.sh ./build-release/taskforge
```

| Workers | Elapsed samples (ms) | Median tasks/s |
|---:|---:|---:|
| 1 | 573.25, 568.69, 563.22 | 175,843.76 |
| 2 | 239.68, 234.12, 220.97 | 427,128.30 |
| 4 | 309.65, 1,974.38, 556.24 | 179,779.92 |
| 8 | 1,974.10, 2,013.87, 1,941.51 | 50,655.88 |

Every run completed 100,000 tasks with zero failures and checksum
`10995514063171028914`. Environment: GCC 13.3.0, Linux 6.6.87.2 under WSL2, x86-64,
with 16 logical processors visible. Two workers performed best. Four workers showed high
variance and eight workers regressed, consistent with scheduling and shared-queue contention
for this fine-grained workload; no linear-scaling claim is made.

## Design tradeoffs

The central bounded queue is intentionally conventional and auditable. It provides strong
backpressure and shutdown behavior but becomes a contention point for very small tasks, as
the benchmark demonstrates. Summary counters use sequentially consistent atomics for clear
cross-thread behavior rather than custom memory ordering. The CLI validates the complete
job file before executing any work, trading memory proportional to job count for avoiding
partial execution of malformed input.
