# DAM-Alloc

DAM (Defensive And Measurable) is a general-purpose memory allocator for x86_64 Linux, implemented in C from scratch. It targets near-glibc performance while exposing a runtime diagnostics API — allocation tracing, heap validation, pool quarantining, and fragmentation introspection — that standard allocators don't expose to the application layer or don't provide at all.

## Architecture

DAM routes every allocation through one of three layers based on size:

1. **Small** (≤ 256 bytes) — segregated free lists with fixed size classes. A thread-local cache provides a lockless fast path, bypassing synchronization entirely for the common case.
2. **General** (≤ 64KB) — pool-based allocator with per-pool free lists, splitting, and two-way coalescing. Pools grow on demand up to a configurable maximum.
3. **Direct** (> 64KB) — one mmap call per allocation. No pool overhead, no fragmentation.

All three boundaries are configurable at compile time via `dam_config.h`.

The general layer uses a payload overlay technique for its free list: when a block is free, the first 16 bytes of the user data region store the doubly linked free list pointers. This means no header inflation — the block header stays the same size whether the block is allocated or free.

## Features
### Runtime Diagnostics API

Beyond the standard `malloc`/`free`/`realloc`/`calloc`, DAM exposes:

* **Allocation tracing** — tag any allocation with a 16-byte string, retrievable at any point during its lifetime via `dam_get_trace()`. Traces are stored positionally between the block header and user data, and are naturally overwritten when the block is freed — tagged data never persists in free memory.
* **Heap snapshots** — `dam_snapshot()` returns a point-in-time view of allocation counts and bytes used across all three layers, including thread cache statistics.
* **Fragmentation reporting** — `dam_fragmentation()` reports per-pool free bytes, largest free block, and fragmentation ratio.
* **Pointer validation** — `dam_validate_ptr()` checks magic values, canaries, and alignment on any live pointer. Corruption is detected and reported without crashing the process.
* **Pool quarantining** — on detected corruption, the affected pool is marked read-only. Future allocations skip it entirely; existing pointers into it remain accessible but the pool accepts no new allocations.

These features compose. A 16-byte trace is enough to store an IPv4 address. If a validated pointer shows corruption, the pool is quarantined and the trace on the offending allocation is still readable — giving you the origin of the allocation that caused the corruption.

### Configurability

All major tunables live in `dam_config.h`:

* Layer boundaries (`DAM_SMALL_MAX`, `DAM_GENERAL_MAX`)
* Size class count and sizes
* Pool growth parameters
* Thread cache capacity per size class

## Trade-offs
* **Lock granularity** — DAM uses per-layer locks rather than per-arena locks. Under high thread contention on the general or direct layer, threads will serialize. The thread-local cache mitigates this for small allocations, which are the common case.
* **Pool memory is not returned to the OS** — general layer pools are retained for reuse rather than `munmap`’d when empty. This trades virtual memory for allocation speed.
* **Fragmentation** — the general layer can fragment under adversarial free patterns. The free list and coalescing mitigate this but don’t eliminate it.
* **No calloc optimization** — DAM’s calloc explicitly zeroes memory rather than exploiting the OS guarantee that `mmap`’d pages arrive zeroed. This is a minor throughput loss on large zeroed allocations.

## Benchmarks
***Coming soon** — single-threaded throughput comparison against glibc malloc across all three allocation layers.*

## Building

DAM requires GCC or Clang and CMake 3.16+, targeting x86_64 Linux.

```bash
git clone https://github.com/PieterEdoardo/DAM-Alloc.git
cd DAM-Alloc
mkdir build && cd build
cmake ..
make
```
This produces `libdam.a` in the build directory.

### Linking against DAM

In your own CMake project:
```cmake
add_subdirectory(DAM-Alloc)
target_link_libraries(your_target dam)
```
Or manually with gcc:
```bash
gcc your_program.c -I/path/to/DAM-Alloc/include -L/path/to/build -ldam -lpthread -o your_program
```
Note: DAM uses pthreads internally, so `-lpthread` is required.

## Planned Work
* Per-pool locking for improved multithreaded scalability
* Benchmark suite comparing DAM against glibc malloc across single and multithreaded workloads

