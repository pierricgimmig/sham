# sham

Header-only C++20 queues and shared-memory helpers for low-latency, cross-process messaging.

`sham` (shared memory) packages single-producer/single-consumer (SPSC) and multi-producer/multi-consumer (MPMC) lock-free queues that store their slots in-place. That layout avoids heap pointers, so a queue can live in a shared-memory mapping and be used from more than one process. A mutex-protected MPMC queue is included as an in-process baseline. Cross-platform primitives wrap POSIX `shm_open`/`mmap` and Windows file mappings.

The lock-free queues are adaptations of [rigtorp/SPSCQueue](https://github.com/rigtorp/SPSCQueue) and [rigtorp/MPMCQueue](https://github.com/rigtorp/MPMCQueue): internal allocations and runtime capacity fields were replaced with an in-place, compile-time-sized slot array.

Tested on Linux, macOS, and Windows.

## Why it exists

Most lock-free queue implementations allocate an internal buffer and store a pointer to it. Those pointers are meaningless in another process’s address space. `sham` keeps the entire queue object (indices and slots) in one contiguous, relocatable block so you can placement-new it into shared memory and open that mapping elsewhere.

## Features

- Lock-free SPSC queue (`sham::SPSCQueue<T, Capacity>`)
- Lock-free MPMC queue (`sham::mpmc::Queue<T, Capacity>`)
- Mutex-protected MPMC queue (`sham::mpmc::LockingQueue<T, Capacity>`) for in-process use and benchmarks
- Named shared-memory mappings (`CreateFileMapping` / `OpenFileMapping` / `MapViewOfFile`)
- `SharedMemoryBuffer` helper that can create or attach, allocate aligned objects, and unlink only from the creating owner
- Optional adapters around [atomic_queue](https://github.com/max0x7ba/atomic_queue) and [moodycamel::ConcurrentQueue](https://github.com/cameron314/concurrentqueue) for apples-to-apples benchmarks (not required to use the library)

## Requirements

- C++20 compiler (GCC 10+, Clang 10+, MSVC 2019+)
- [CMake](https://cmake.org/) 3.14 or newer, **or** [Bazel](https://bazel.build/) 8 (via [Bazelisk](https://github.com/bazelbuild/bazelisk))
- For CMake tests and benchmarks: `git` (`FetchContent` downloads GoogleTest and Google Benchmark)
- For Bazel: network on the first fetch of [BCR](https://registry.bazel.build/) modules (GoogleTest, Google Benchmark)
- POSIX platforms: `librt` on older glibc (linked automatically when present)

## Build

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build
```

Or, with GNU Make:

```bash
make          # configure + build
make test     # build + ctest
make bench    # run Google Benchmark binary
make clean
```

CMake options:

| Option | Default (top-level) | Purpose |
| --- | --- | --- |
| `SHAM_BUILD_TESTS` | `ON` | Build `sham_tests` and register CTest cases |
| `SHAM_BUILD_BENCHMARKS` | `ON` | Build `sham_benchmarks` |

When `sham` is added as a CMake subdirectory, both options default to `OFF`.

Install the public headers:

```bash
cmake --install build --prefix /usr/local
```

This installs `include/sham/*.h`. The library is header-only; there is no binary to link beyond the platform shared-memory libraries already attached to the `sham` interface target.

### Bazel

The same targets are available through Bazel modules (`MODULE.bazel`). Pin is Bazel 8.2.1 (see `.bazelversion`).

```bash
bazel build //src/sham:sham
bazel test //src/tests:sham_tests          # or: bazel test //:all_tests
bazel run -c opt //src/benchmarks:sham_benchmarks
bazel run -c opt //src/benchmarks:sham_queue_compare
```

`//src/sham:sham` is the header-only library (`#include "sham/..."`). Tests and benchmarks pull GoogleTest / Google Benchmark from the Bazel Central Registry. Vendored `atomic_queue` and `concurrentqueue` are local `//third_party` targets used only by adapters.

Makefile wrappers: `make bazel-test`, `make bazel-bench`.

## Tests

From the `build` directory (or via `make test`):

```bash
ctest --output-on-failure --timeout 120
```

The suite covers:

- Sequential and concurrent SPSC behavior, wrap-around, move-only types, and destruction
- Sequential and concurrent MPMC behavior, including a one-slot queue
- Locking-queue capacity, full/empty, and non-`2^n-1` sizes
- Shared-memory create/open/map, missing-name failure, and (on POSIX) cross-process write/read
- `SharedMemoryBuffer` allocation, alignment, move semantics, owner vs accessor lifetime

The heavier MPMC thread combinations use tens of thousands of operations so they stay CI-friendly. Throughput numbers printed by those tests are informational; use `sham_benchmarks` or the `sham::Benchmark` helper for real measurements.

```bash
# Google Benchmark (try_push / try_pop microbenchmarks)
./build/src/benchmarks/sham_benchmarks

# Multi-threaded comparison across queue types (writes benchmark_summary.txt)
./build/src/benchmarks/sham_queue_compare
```

Or `make bench` / `bazel run -c opt //src/benchmarks:sham_queue_compare`.

## Usage

### In-process SPSC

```cpp
#include "sham/queue_spsc.h"

sham::SPSCQueue<int, 1024> queue;
queue.try_push(1);

if (int* front = queue.front()) {
  // use *front
  queue.pop();
}
```

### In-process MPMC

```cpp
#include "sham/queue_mpmc.h"

sham::mpmc::Queue<int, 1023> queue;
queue.try_push(42);

int value = 0;
if (queue.try_pop(value)) {
  // value == 42
}
```

`push` / `pop` / `emplace` wait until they can complete. `try_*` variants return immediately.

The lock-free MPMC queue requires `T` to be nothrow constructible, assignable, and destructible (same constraint as rigtorp’s queue). Capacity is a compile-time constant and is the number of elements the queue can hold. The SPSC queue stores one extra slack slot internally so full and empty stay distinguishable.

### Queue in shared memory

The creating process placement-news the queue into a named mapping. Other processes open the same name and `reinterpret_cast` at the same offset. They must **not** run the queue constructor again.

```cpp
#include "sham/queue_mpmc.h"
#include "sham/shared_memory_buffer.h"

using Queue = sham::mpmc::Queue<int, 1023>;

// Creator
sham::SharedMemoryBuffer shm("app_events", sizeof(Queue) + 64,
                             sham::SharedMemoryBuffer::Type::kCreate);
Queue* queue = shm.Allocate<Queue>();
queue->try_push(7);

// Other process
sham::SharedMemoryBuffer view("app_events", sizeof(Queue) + 64,
                              sham::SharedMemoryBuffer::Type::kAccessExisting);
Queue* remote = view.As<Queue>();
int value = 0;
remote->try_pop(value);
```

POSIX mapping names are normalized to start with `/` (for example `app_events` becomes `/app_events`). Some platforms cap the name length (macOS is around 31 characters). Destroying the creating `SharedMemoryBuffer` unlinks the name; destroying an accessor only closes its handle.

## Project layout

```
MODULE.bazel / BUILD.bazel Bazel module and root aliases
cmake/                     Find modules for bundled third-party queues
src/sham/include/sham/     Public headers (the library)
src/adapters/              Benchmark-only wrappers for external queues
src/benchmarks/            Google Benchmark + multi-thread comparison
src/tests/                 GoogleTest suite
third_party/               Bundled atomic_queue and concurrentqueue
```

Public headers:

| Header | Role |
| --- | --- |
| `queue_spsc.h` | Lock-free SPSC queue |
| `queue_mpmc.h` | Lock-free MPMC queue |
| `queue_locking.h` | Mutex MPMC queue (in-process) |
| `shared_memory.h` | Create/open/map/unmap/destroy |
| `shared_memory_buffer.h` | Owned named buffer + aligned allocate |
| `benchmark.h`, `timer.h`, `string_format.h` | Internal helpers used by tests/benchmarks |

## Limitations

- The locking queue’s `std::mutex` is **not** process-shared. Do not place `LockingQueue` in shared memory.
- Shared-memory queues must be constructed once by the owner. Attachers only cast to the existing object.
- `size()` / `empty()` on the lock-free MPMC queue are best-effort while threads are still running.
- POSIX mappings are created world-readable/writable (`0666`) so non-root processes can attach. Tighten permissions if that does not match your threat model.
- Windows multi-process shared-memory tests are not automated yet (`fork` is POSIX-only).
- Third-party adapters and Google Benchmark are for comparison, not part of the supported library API.

## Contributing

1. Build and run the tests (`make test`, `ctest`, or `bazel test //src/tests:sham_tests`).
2. Format C++ in `src/` with the repo `.clang-format` (Google style, 100-column limit). Keep CMake and Bazel targets in sync when adding sources.
3. Keep changes focused: the queues are small, well-known algorithms; prefer local fixes over redesigns.

Pull requests should add or update tests for any behavior change.

## License

MIT. See [LICENSE](LICENSE). Bundled third-party queues keep their own MIT licenses under `third_party/`.
