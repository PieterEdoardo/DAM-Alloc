# DAM-Alloc

DAM (Defensive And Measurable allocator) is a general purpose malloc implementation meant as a serious portfolio project. It doesn't strive to be faster than anything, but it instead aims to get close to it, with an additonal suite of diagnostic, validation, and security features. It achieves this by abstracting them to the public API, and gives the programmer the ability to use them when they need it. DAM is also almost entirely configurable, and let's you even configure internals such as layer boundaries, or size class mount and size, to match programmer and/or system needs. It's core aim not just to be educational, but also be actually useful in production environments that require runtime memory diagnostics.


## Features overview
### Extended API
Apart from the standard malloc, realloc, calloc, free, DAM features:
* 16 byte traces to identify and follow allocations. Fully optional.
* Memory snapshots that give an approximation of current allocation layer status.
* Memory snapshots that give an apporximation of pool fragmentation.
* Pointer metadata validation to check memory block integrity at any time.
* Memory pool quarantining if corruption is found, making the pool 'read only' and will be skipped for future allocations.
* Canaries and magic values to detect buffer overflows.

The elegance is in how these features work together. 16 byes is enough to fit an IP4 address. An allocation can be made with a ip-number set as it's trace, later user attempts to perform a buffer overflow attack which can be detected with the validation functinon. Pool get's quarantined, and the trace can be retrieved since it's stored right before the user data. User's ip-number can then be for exmaple blacklisted.

### Optimization
With memory allocators strucuture often dictates a large portion of the performance, DAM features:
* 3 allocation layers with configurable boundaries.
* Separate private threads for each allocation layer.
* seggregated lists with size classes for small allocations.
* Thread-local caches fast-path for small allocations. This skips the locks completely.
* Arena's for medium allocations with two way O(1) coalescing and splitting, free lists, and configurable pool growing.
* Direct mmap allocation layer for huge allocations. Configurable boundary means you can completely opt out for this layer.

## Notable trade offs

## Benchmarks

## Example uses
Snapshot example:
```
Pool: 0
free: 66527136
largest_free: 47136320
fragmentation: 0.708528

tlc_used: 320
tlc_free: 0
size_classes: 5
classes_bytes_used: 576240
pools_active: 8
pools_bytes_used: 534773760
quarantined_pools: 1
direct_allocations: 4
direct_bytes_used: 13209600
Grand total used: 535702 Kilobytes 
```
