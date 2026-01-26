# DAM Allocator

DAM is a custom memory allocator written in C, designed as an educational yet production-grade exploration of allocator architecture.

It implements **three allocation layers**, each optimized for a specific size range, with clear separation of responsibilities and explicit bookkeeping.

This repository uses AI generated placeholder documention during development. The final version will include handwritten documentation instead.

---

## Architecture Overview
```
dam_malloc()

├── Small allocator (size classes)
│ └── Fixed-size blocks (32B → 256B)
│
├── General allocator (growing pools)
│ └── Variable-size blocks with splitting & coalescing
│
└── Direct allocator (mmap)
  └── One allocation per mapping (large objects)
```

---

## Allocation Layers

### Small Allocator
- Power-of-two size classes
- Preallocated pools
- O(1) allocation/free
- Minimal fragmentation

### General Allocator
- Growing memory pools
- Block splitting and coalescing
- Canary-based overflow detection

### Direct Allocator
- Backed directly by `mmap`
- One pool per allocation
- Efficient for very large objects
- Supports realloc with configurable shrink policy

---

## Realloc Semantics

`dam_realloc` fully supports cross-layer transitions:

- Small → General
- General → Direct
- Direct → General / Small

All realloc operations strictly preserve:

min(old_size, new_size)

bytes, in accordance with C standard semantics.

---

## Thread Safety

Multithreading support is **planned**.

Current design intentionally separates allocator layers and pool bookkeeping to enable:
- Coarse-grained locking (initial)
- Per-layer and per-size-class locks (future)
- Thread-local optimizations (advanced)

---

## Status

- [x] Small allocator
- [x] General allocator
- [x] Direct allocator
- [x] Cross-layer realloc
- [x] Torture-tested realloc correctness
- [ ] Multithreading (in progress)
- [ ] Performance benchmarks
- [ ] Extensive diagnostics tooling

---

## Goals

- Correctness-first design
- Explicit and debuggable internals
- Educational clarity over clever tricks
- Gradual evolution toward high-performance allocator techniques
