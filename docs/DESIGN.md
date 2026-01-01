# 📘 DESIGN DOCUMENT — DAM (Defensive And Measurable)

## 1. Overview

DAM is a general-purpose memory allocator designed for systems where *observability, correctness, and controlled performance degradation are as important as raw throughput.

Unlike hyper-optimized allocators that prioritize peak allocation speed, DAM is intentionally defensive by design. It aims to detect memory corruption early, expose allocator behavior to the application, and remain stable under adverse conditions.

DAM is written in C for maximal portability and ABI stability, with optional C++ RAII wrappers for safer integration into higher-level systems.

## 2. Design Goals 

### Primary goals

- Detect memory corruption as early as possible
- Provide allocator behavior metrics at runtime
- Avoid global locks and enable true concurrency
- Preserve near-production allocator performance
- Fail predictably under corruption

### Non-goals

- Absolute peak throughput at all costs
- Perfect fragmentation elimination
- NUMA specialization
- Slab allocation
- Hidden allocator behavior

## 3. High-Level Architecture

DAM uses a three-tier allocation model, selected based on allocation size:

### Tier 1: Small allocations

- Segregated free lists by size class
- Multiple pools per size class
- Extremely fast allocation path
- Minimal per-allocation overhead

### Tier 2: Middle allocations

- General-purpose pools
- First-fit allocation strategy
- Full splitting and coalescing
- Defensive instrumentation enabled

### Tier 3: Large allocations

- Direct mmap / munmap
- Bypasses internal pools
- Minimal bookkeeping
- Used for large or irregular allocations

## 4. Memory Model

### Block layout

Each allocation consists of:

- Header (metadata, magic, flags)
- User payload
- Canary (overflow detection)

All structures are aligned to max_align_t.

### Pool layout

Pools are page-aligned and page-sized multiples.
This ensures:

- Predictable virtual memory behavior
- Efficient page mapping
- Clean interaction with the OS VM subsystem

## 5. Allocation Strategy

DAM intentionally uses first-fit allocation.

Rationale:

- Predictable behavior
- Lower metadata scanning overhead
- Easier corruption diagnosis
- Reduced allocator-induced reordering

Fragmentation is mitigated through:

- Aggressive coalescing
- Tier separation
- Size-class isolation in the small tier

## 6. Defensive Features

DAM treats memory corruption as an expected failure mode.

### Detection mechanisms
- Magic value validation
- Canary verification
- Double-free checks
- Invalid pointer rejection 

### Response strategy
- Mark affected pool as quarantined
- Exclude pool from future allocations
- Preserve allocator integrity
- Expose corruption via stats API

DAM does not attempt silent recovery. Responsibility is explicitly surfaced to the application.

## 7. Concurrency Model

DAM avoids a global allocator lock.

Instead:

- Small-tier pools are independently locked
- Middle-tier pools use per-pool synchronization
- Large allocations are OS-managed
- Statistics aggregation is decoupled from allocation paths

This design allows:
- True parallel allocation
- Graceful contention under load
- Scalable multithreaded behavior

## 8. Statistics & Observability

DAM exposes allocator behavior as a first-class API.

Metrics include:

- Allocation pressure
- Fragmentation ratios
- Pool health states
- Split/coalesce frequency
- Allocation failure rates

Applications can:

- Poll allocator health
- Adapt allocation strategies
- Detect suspicious behavior
- Trigger application-level mitigation

## 9. Failure Modes

DAM explicitly documents what happens when:

- Corruption is detected
- Pools become unusable
- Allocation fails
- Debug assertions trigger
- Undefined behavior is minimized by design, but never hidden.

## 10. C / C++ Integration

DAM exposes a pure C API.

Optional C++ RAII wrappers:

- Automatically free memory
- Enforce lifetime semantics
- Integrate naturally with modern C++ codebases
- The core allocator remains language-agnostic.

## 11. Trade-offs Summary

| Aspect        | DAM Choice         | Trade-off                   |
| ------------- | ------------------ | --------------------------- |
| Speed         | Near-production    | Slight overhead from checks |
| Safety        | High               | More metadata               |
| Concurrency   | Fine-grained locks | Complexity                  |
| Fragmentation | Controlled         | Not zero                    |
| Transparency  | Explicit           | Less “magic”                |

## 12. Intended Use Cases

Security-sensitive systems
Infrastructure services
Debuggable production systems
Research and experimentation
Education in allocator design