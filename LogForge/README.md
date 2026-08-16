# LogForge

LogForge is a small C++20 command-line log analyzer. It streams a text log one line at a
time, emits records selected by level and service, and can instead produce deterministic
operational summaries. A malformed record is diagnosed and skipped without stopping the
rest of the file.

## Features

- Streaming processing with memory use independent of file length, except for summary keys
- Level and service filters that compose in any sensible argument order
- Summary counts for input health and matched records, grouped by level and service
- Clear diagnostics on `stderr` and nonzero status codes for fatal CLI/file errors
- Strict warning settings, standard-library tests, CTest integration coverage, and ASan/UBSan support

## Architecture

- `include/logforge/log_record.hpp` owns the typed record representation.
- `parser.hpp` / `parser.cpp` parse one line into an optional `LogRecord`.
- `options.hpp` / `options.cpp` validate the command line without performing I/O.
- `analyzer.hpp` / `analyzer.cpp` stream records, apply filters, collect statistics, and format output.
- `src/main.cpp` owns file I/O, connects the components, and maps failures to exit statuses.
- `tests/` contains focused unit tests and an executable-level CTest integration test.

The core is a library so parsing, option handling, and analysis can be tested without
subprocesses. `std::map` makes grouped summary output lexicographically deterministic.

## Requirements

- CMake 3.20 or newer
- A C++20 compiler (tested with GCC 13.3.0)

No third-party runtime or test dependency is required.

## Build and test

Debug build:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build --output-on-failure
```

Release build:

```bash
cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release
cmake --build build-release
ctest --test-dir build-release --output-on-failure
```

The executable is `build/logforge` or `build-release/logforge`.

Sanitizer build (GCC or Clang):

```bash
cmake -S . -B build-sanitize \
  -DCMAKE_BUILD_TYPE=Debug \
  -DLOGFORGE_ENABLE_SANITIZERS=ON
cmake --build build-sanitize
ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 \
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
ctest --test-dir build-sanitize --output-on-failure
```

The shown `detect_leaks=0` keeps AddressSanitizer's memory-error checks enabled while
avoiding LeakSanitizer's incompatibility with ptrace-controlled environments such as the
one used for the recorded validation. On a normal host, omit that setting or use
`detect_leaks=1` to include leak checks.

## Usage

```bash
./build-release/logforge sample/server.log
./build-release/logforge sample/server.log --level ERROR
./build-release/logforge sample/server.log --service payments
./build-release/logforge sample/server.log --level ERROR --service payments
./build-release/logforge sample/server.log --summary
./build-release/logforge --summary sample/server.log --service payments --level ERROR
```

Without `--summary`, matched records are written to `stdout`:

```text
timestamp=2026-08-15T10:15:03 level=ERROR service=payments message="Payment processing failed"
```

With `--summary`, individual records are suppressed and a summary is written instead:

```text
Summary
-------
Lines examined:    5
Valid records:     4
Malformed records: 1
Matched records:   1

By level (matched):
ERROR  1

By service (matched):
payments  1
```

`Lines examined`, `Valid records`, and `Malformed records` describe the entire input.
`Matched records` and both grouped sections describe the result after all supplied filters.

## Log format and malformed input

Each supported record has four whitespace-separated parts; the message occupies the rest
of the line and may contain spaces:

```text
<TIMESTAMP> <LEVEL> <SERVICE> <MESSAGE...>
2026-08-15T10:15:03 ERROR payments Payment processing failed
```

A line missing any field or containing no message is malformed. LogForge writes a warning
with its line number to `stderr`, counts it, and continues. Malformed lines cannot be
meaningfully filtered, so they are always included in the input-health counters.

## Performance measurement

The deterministic workload generator is included in `tools/generate_benchmark.awk`:

```bash
awk -v records=10000000 -f tools/generate_benchmark.awk \
  > /tmp/logforge-performance-10m.log
/usr/bin/time -f 'elapsed_seconds=%e\nmax_rss_kib=%M' \
  -o /tmp/logforge-performance-10m.time \
  ./build-release/logforge /tmp/logforge-performance-10m.log --summary \
  > /tmp/logforge-performance-10m.out
```

Measured result: 10,000,000 valid records, 733,333,330 bytes (699.4 MiB), processed in
1.09 seconds with 3,712 KiB maximum resident memory. That is approximately 9.17 million
records/s and 642 MiB/s.

Environment: GCC 13.3.0, Linux 6.6.87.2 under WSL2, x86-64, 16 logical processors
available; LogForge itself is single-threaded. The generated `/tmp` file was read
immediately after creation and was therefore likely served from the host page cache.
The measurement demonstrates streaming throughput and bounded resident memory; it is not
presented as a storage-device benchmark.

## Design tradeoffs

LogForge intentionally uses owned strings in each transient record for simple lifetimes and
value semantics. Summary maps grow with the number of distinct levels and services, not the
number of input records. The parser validates field presence but treats timestamp, level,
and service values as opaque text; this keeps v1 aligned with the stated log format without
adding policy the format does not define.
