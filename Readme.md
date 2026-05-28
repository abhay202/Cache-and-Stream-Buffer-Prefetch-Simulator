# Cache and Stream-Buffer Prefetch Simulator

## Overview

This project implements a flexible cache and memory hierarchy simulator capable of modeling:
- A single L1 cache
- An L1 + L2 cache hierarchy
- An L1 cache augmented with stream-buffer prefetching
- An L1 + L2 hierarchy with stream-buffer prefetching at the L2 level

The simulator processes memory access traces and reports cache performance statistics including miss rates, writebacks, prefetches, and total memory traffic.

---

## Building

```bash
make
```

To rebuild from scratch:

```bash
make clean && make
```

---

## Running

```
./sim <BLOCKSIZE> <L1_SIZE> <L1_ASSOC> <L2_SIZE> <L2_ASSOC> <PREF_N> <PREF_M> <trace_file>
```

### Parameters

| Parameter   | Description |
|-------------|-------------|
| `BLOCKSIZE` | Cache block size in bytes (power of 2) |
| `L1_SIZE`   | Total L1 cache size in bytes |
| `L1_ASSOC`  | L1 associativity (1 = direct-mapped) |
| `L2_SIZE`   | Total L2 cache size in bytes (0 = no L2) |
| `L2_ASSOC`  | L2 associativity (0 = no L2) |
| `PREF_N`    | Number of stream buffers (0 = no prefetching) |
| `PREF_M`    | Number of blocks per stream buffer |
| `trace_file`| Path to the memory access trace file |

### Example Commands

```bash
./sim 16 1024 1 8192 4 3 4 gcc_trace.txt
```

---

## Cache Design

### Replacement Policy
LRU (Least-Recently-Used) is used at all cache levels.

### Write Policy
Write-back, Write-allocate (WBWA):
- **Write hit**: update the block in cache and mark it dirty.
- **Write miss**: allocate the block in cache (fetching from the next level), then write.
- **Eviction of a dirty block**: write back to the next level (L2 or memory).

### Address Decomposition
```
| tag | index | block offset |
```
- `block offset` = log2(BLOCKSIZE) bits
- `index` = log2(SIZE / (BLOCKSIZE × ASSOC)) bits
- `tag` = remaining bits

---

## Stream-Buffer Prefetching (ECE 563 Extension)

The prefetch unit consists of **N stream buffers**, each holding **M consecutive** memory blocks. Setting `PREF_N = 0` disables prefetching entirely.

### Placement
- **No L2 cache**: stream buffers sit between L1 and main memory.
- **With L2 cache**: stream buffers sit between L2 and main memory.

### Operation — Four Scenarios

On every cache access (read or write) to block X, both the cache and all stream buffers are checked simultaneously:

| Scenario | Cache | Stream Buffer | Action |
|----------|-------|---------------|--------|
| **#1** | Miss | Miss | Normal miss: fetch X from next level, allocate in cache. Start a new stream [X+1 … X+M] in the LRU stream buffer. Counts as a cache miss. |
| **#2** | Miss | Hit | Copy X from stream buffer into cache (no fetch from next level). Advance the stream: remove blocks up to and including X, prefetch new blocks to refill to M. **Not counted as a cache miss.** |
| **#3** | Hit  | Miss | Normal cache hit. No stream buffer action. |
| **#4** | Hit  | Hit  | Normal cache hit. Advance the stream buffer identically to Scenario #2 (no transfer to cache). |

### Stream Buffer Advancement (Scenarios #2 and #4)

If block X is found at position `j` (0-indexed from the head of the stream buffer):
- Remove blocks at positions 0 through j.
- Prefetch **j + 1** new blocks from memory to refill the buffer tail.
- The stream buffer now holds blocks [X+1 … X+M].

### Multiple Stream Buffer Policy

When multiple buffers contain block X, only the **most-recently-used** (MRU) buffer among those that hit is advanced. The rest are ignored.

The LRU buffer is selected for replacement when a new stream must be started (Scenario #1).

### Counters

- **L1 prefetches** (g): total blocks fetched from memory into stream buffers (when SB is at L1).
- **L2 prefetches** (p): total blocks fetched from memory into stream buffers (when SB is at L2).
- **Memory traffic** (q): demand fetches to memory + writebacks + prefetch fetches.

---

## Source Files

| File | Description |
|------|-------------|
| `sim.cc` | Main entry point; parses arguments, creates caches and stream buffers, drives simulation, prints output |
| `Class.cpp` | `CacheSet`, `Cache`, and `StreamBufferUnit` class implementations |
| `sim.h` | `cache_params_t` struct definition |
| `Makefile` | Build rules |

