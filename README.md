# Parallel Packed CSR

A Parallel Packed Compressed Sparse Row implementation for large-scale dynamic graphs and streaming graph update workloads.

This repository implements a parallel implementation of the packed CSR data structure, based on its [initial single threaded design](https://github.com/wheatman/Packed-Compressed-Sparse-Row/)[[1]](#1) and further parallel extension [[2]](#2). It keeps the cache-friendly layout of CSR, where outgoing edges of each source vertex are stored together in sorted order, but leaves controlled empty space inside the edge array so that insertions and deletions can be handled locally rather than by rebuilding the whole graph.

The implementation extends the original single-threaded Packed CSR design with parallel update processing, synchronization for concurrent structural changes, and optional NUMA-aware partitioning.

The command-line executable follows a common streaming-graph benchmarking workflow:

1. Load an initial **core graph** from an edge-list file.
2. Load a second file containing edge updates.
3. Apply the updates in parallel.
4. Report elapsed wall-clock time for the update batch.

The current codebase is a systems research prototype for investigating high-throughput updates on dynamic graph data structures. It is designed to support experimentation with concurrent edge insertions and deletions, partitioned versus non-partitioned execution, and NUMA-aware placement strategies for graph update workloads.

---

## Why this exists

Compressed Sparse Row is a widely used graph representation because it stores adjacency information compactly and provides good locality for graph traversals. Its weakness is updates: inserting into a conventional CSR edge array can require shifting or rebuilding a large part of the structure.

Packed CSR addresses this by storing the edge array as a Packed Memory Array. Empty slots are deliberately left between stored edges, and a conceptual tree of density bounds is used to decide when local regions should be rebalanced. This gives substantially better update behaviour while preserving CSR-like locality for reads.

The original Packed CSR design was single-threaded. This project explores how to make that design usable for high-volume dynamic graph workloads where many insertions and deletions should be applied in parallel on a single multi-core or multi-socket machine.

The design goals are:

* keep the CSR-like contiguous adjacency layout;
* support edge insertions and deletions without full rebuilds after each update;
* preserve sorted neighbourhoods for each source vertex;
* allow multiple worker threads to update the structure safely;
* avoid deadlocks during multi-region updates;
* compare strict locking with a retry-based lock-free binary-search path;
* explore partitioning and NUMA-aware placement as extensions of the basic parallel PCSR design.

---

## Features

* Dynamic directed graph representation based on Packed CSR / Packed Memory Array principles.
* Parallel edge insertion and deletion.
* Sorted outgoing neighbourhoods for source vertices.
* Sentinel-based separation of source-vertex regions.
* Local redistribution of dense or sparse PMA regions.
* Array growth and shrinkage through doubling / halving when density bounds require it.
* Fine-grained locking at PCSR leaf-node granularity.
* Version counters for detecting races between search and update phases.
* Deadlock avoidance by acquiring locks in a consistent left-to-right order.
* Global lock fallback for full-structure operations such as resizing.
* Optional lock-free binary-search mode with validation and retry.
* Partitioned `PPPCSR` wrapper around multiple independent `PCSR` instances.
* NUMA-aware partition allocation and worker scheduling.
* GoogleTest-based tests, including sequential, parallel, algorithmic and scheduler tests.
* Benchmarking scripts for partitioning and update-scaling experiments.

---

## Implementation variants

The executable supports three modes.

### PPCSR

Selected with:

```bash
-ppcsr
```

This mode uses one shared `PCSR` instance for the whole graph. Updates are submitted to worker queues in round-robin order using `i % threads`. Every worker updates the same underlying packed CSR structure, so this mode exercises the fine-grained locking and versioning logic most directly.

Use this as the simplest parallel baseline.

### PPPCSR

Selected with:

```bash
-pppcsr
```

This mode uses a partitioned wrapper around multiple `PCSR` instances. Source vertices are divided into contiguous ranges, and each partition owns one range. Public operations such as `add_edge`, `remove_edge`, `edge_exists` and `get_neighbourhood` route the request to the partition that owns the source vertex.

An edge update is partitioned by **source vertex**, not by destination vertex. For example, an update to edge `42 -> 100` is handled by the partition that owns source vertex `42`.

This reduces contention because updates to different source-vertex ranges can proceed independently in different `PCSR` instances.

### PPPCSRNUMA

Selected with:

```bash
-pppcsrnuma
```

This is the default mode.

It uses the same source-vertex partitioning strategy as `PPPCSR`, but additionally attempts to make the data structure NUMA-aware. Partitions are assigned to NUMA domains, memory is allocated with NUMA-aware allocation paths where available, and worker threads are scheduled on the domain associated with the partition they process.

This mode reflects the later direction of the project: the original parallel PCSR work identified NUMA overhead as a performance concern, and the repository now includes an explicit partitioned/NUMA-aware implementation path.

---

## Repository layout

```text
.
├── CMakeLists.txt
├── LICENSE
├── README.md
├── cmake
│   └── modules
├── src
│   ├── main.cpp
│   ├── pcsr
│   │   └── PCSR.h / PCSR.cpp
│   ├── pppcsr
│   │   └── PPPCSR.h / PPPCSR.cpp
│   ├── thread_pool
│   │   └── thread_pool.h / thread_pool.cpp
│   ├── thread_pool_pppcsr
│   │   └── thread_pool_pppcsr.h / thread_pool_pppcsr.cpp
│   ├── utility
│   │   ├── bfs.h
│   │   ├── fastLock.h
│   │   ├── hybridLock.h
│   │   ├── pagerank.h
│   │   └── task.h
│   └── benchmarking
└── test
    ├── DataStructureTest.cpp / DataStructureTest.h
    ├── SchedulerTest.cpp / SchedulerTest.h
    └── tests_main.cpp
```

The main components are:

* `PCSR`: the core packed CSR implementation for one vertex range.
* `PPPCSR`: a partitioned wrapper around multiple `PCSR` instances.
* `ThreadPool`: the worker scheduler for single-structure `PPCSR` mode.
* `ThreadPoolPPPCSR`: the partition-aware and NUMA-aware scheduler for `PPPCSR` and `PPPCSRNUMA` modes.
* `utility/hybridLock.h`: the lock/version primitive used by PCSR leaf regions.
* `utility/fastLock.h`: lower-level lock support.
* `utility`: small graph-algorithm helpers and shared task/lock headers used by the data structures and tests.
* `benchmarking`: scripts for running update benchmarks.

---

## Requirements

The project is Linux-oriented and links against pthreads and `libnuma`.

Required for the main executable:

* CMake 3.8 or newer
* C++14-compatible compiler
* pthreads
* libnuma development headers and library

Test-related:

* OpenMP
* GoogleTest, or internet access during CMake configuration so that CPM can fetch GoogleTest

On Debian/Ubuntu-like systems:

```bash
sudo apt-get install build-essential cmake libnuma-dev
```

Depending on your compiler setup, you may also need the OpenMP runtime/development package for building the test targets.

---

## Building

From a fresh checkout:

```bash
git clone https://github.com/domargan/parallel-packed-csr.git
cd parallel-packed-csr
mkdir -p build
cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j
```

This builds the main executable:

```bash
./parallel-packed-csr
```

For a debug build:

```bash
cmake -DCMAKE_BUILD_TYPE=Debug ..
make -j
```

---

## Input format

The program expects plain text edge-list files.

Each line starts with a source vertex id followed by a destination vertex id:

```text
src dst
```

Example:

```text
0 1
0 4
1 2
2 5
```

The graph is treated as **directed**. A line `0 4` represents edge `0 -> 4`; it does not also insert `4 -> 0`.

Update files may either use the global operation selected on the command line, or specify the operation per line.

Implicit insert/delete mode:

```text
0 3
4 9
2 8
```

Mixed update mode:

```text
0 3 1
4 9 1
0 1 0
```

Where:

* `1` means insert this edge;
* `0` means delete this edge.

Notes:

* The core graph is always loaded as insertions.
* If an update line does not specify an operation, `-insert` or `-delete` decides how it is interpreted.
* The parser assumes clean integer edge-list lines. Avoid headers, comments and blank lines.
* The program infers the number of vertices from the largest vertex id seen in the core and update files.

---

## Running

Minimal example:

```bash
./parallel-packed-csr \
  -core_graph=../data/core.edgelist \
  -update_file=../data/updates.edgelist
```

Example using the default NUMA-aware partitioned mode:

```bash
./parallel-packed-csr \
  -core_graph=../data/core.edgelist \
  -update_file=../data/updates.edgelist \
  -threads=16 \
  -size=1000000 \
  -insert \
  -pppcsrnuma \
  -partitions_per_domain=2
```

Example deletion-only batch:

```bash
./parallel-packed-csr \
  -core_graph=../data/core.edgelist \
  -update_file=../data/deletions.edgelist \
  -threads=16 \
  -delete \
  -pppcsr
```

The executable prints the update file name, core graph size, partition/thread diagnostics and elapsed wall-clock time for the update batch.

---

## Command-line options

| Option                       | Meaning                                                                                                      | Default   |
| ---------------------------- | ------------------------------------------------------------------------------------------------------------ | --------- |
| `-core_graph=<path>`         | Initial graph edge-list file. Required.                                                                      | none      |
| `-update_file=<path>`        | Edge-update file. Required.                                                                                  | none      |
| `-threads=<n>`               | Number of worker threads.                                                                                    | `8`       |
| `-size=<n>`                  | Maximum number of updates to apply from the update file. The code caps this to the number of parsed updates. | `1000000` |
| `-insert`                    | Treat update-file lines as insertions unless a line has an explicit operation.                               | enabled   |
| `-delete`                    | Treat update-file lines as deletions unless a line has an explicit operation.                                | disabled  |
| `-lock_free`                 | Disable locking during binary search and use validation/retry instead. Structural updates still use locks.   | disabled  |
| `-ppcsr`                     | Use one shared `PCSR` instance.                                                                              | disabled  |
| `-pppcsr`                    | Use source-range partitioned `PPPCSR` without explicit NUMA placement.                                       | disabled  |
| `-pppcsrnuma`                | Use partitioned `PPPCSR` with NUMA-aware allocation/scheduling.                                              | enabled   |
| `-partitions_per_domain=<n>` | Number of graph partitions per NUMA domain.                                                                  | `1`       |

If multiple representation flags are supplied, the last one processed by the parser determines the selected mode.

---

## How Packed CSR is represented

At the lowest level, `PCSR` stores graph edges in a sparse ordered array.

Each graph vertex has metadata describing its outgoing-neighbourhood region:

* `beginning`: index of the sentinel marking the start of the vertex region;
* `end`: exclusive end of the vertex region;
* `num_neighbors`: number of stored outgoing edges.

The edge array stores two kinds of entries:

* real edges, containing source, destination and value;
* sentinel entries, used to separate source-vertex regions and keep back-pointers into the node metadata.

A value of zero represents an empty slot. Empty slots are not incidental; they are what allow the structure to absorb updates without rebuilding the whole array.

Conceptually, the edge array is treated as a tree of ranges. Leaf ranges contain `logN` array positions. Each tree level has density bounds. Insertions and deletions may temporarily violate those bounds in a leaf or internal range. The structure restores the bounds by redistributing elements across a suitable range, or by resizing the whole array when the root range becomes too dense or too sparse.

---

## Update algorithm

### Insertion

An insertion follows these stages:

1. **Search** the source vertex's neighbourhood using a modified binary search that can handle empty slots.
2. **Validate** that the edge is not already present, or update the existing value if it is.
3. **Lock** the PCSR leaf region that will be modified.
4. **Check version counters** to detect whether another thread changed the region after the search phase.
5. **Insert directly** if the target position is empty.
6. **Slide right** to the next empty slot if the target position is occupied.
7. **Slide left** in rare cases where sliding right reaches the end of the array.
8. **Redistribute** the smallest enclosing PMA region whose density bounds can absorb the change.
9. **Double the array** and redistribute globally if the root range is too dense.

### Deletion

A deletion follows a similar path:

1. Search for the edge in the source vertex's neighbourhood.
2. Lock the necessary PCSR region.
3. Validate that the searched region is still valid.
4. Mark the edge position as empty.
5. Decrement the source vertex's neighbour count.
6. Redistribute if density falls below the relevant lower bound.
7. Halve the array and redistribute globally if the root range is too sparse.

### Reads

The internal API supports reading a neighbourhood and checking whether an edge exists. A whole-neighbourhood read scans the source vertex's packed region and skips empty slots. A point lookup uses the modified binary search.

The main command-line path is focused on batched insertions and deletions. The internal enum and thread pools include a read operation, but `main.cpp` does not currently expose a read workload format.

---

## Modified binary search

Because Packed CSR contains empty slots, ordinary binary search cannot simply inspect the midpoint and compare it to the target. The midpoint may be empty.

The search procedure therefore probes around the midpoint until it finds a non-empty entry, then continues the binary-search decision from that entry. This preserves the ability to search a sorted neighbourhood while tolerating PMA gaps.

The code supports two approaches:

### Locked search

By default, the code locks the relevant PCSR leaf nodes in shared/read mode during binary search. This prevents concurrent structural movement from changing the searched neighbourhood while the search is in progress.

### Lock-free search with validation

With `-lock_free`, the search itself does not take these read locks. Instead, the code validates the result before applying the update. If a race is detected, the update retries. This can reduce locking overhead when the searched neighbourhood is large, but it does not make the full update path lock-free: insertions, deletions, redistributions and resizes still require synchronization.

This can be useful when strict search locking would otherwise cover a large region, but it is deliberately limited to the search phase. The current repository should be treated as the source of truth for the implemented behaviour.

---

## Locking and correctness model

Parallel Packed CSR is difficult because an update may affect more than one array position, and sometimes more than one PCSR leaf region. A thread might slide elements, move sentinels, redistribute a larger PMA range, or resize the whole array.

The implementation uses the following strategy:

* PCSR leaf regions have associated `HybridLock` instances.
* Locks also act as version counters: updates increment the relevant lock/version objects when releasing modified regions.
* Search records the version of the region where the update intends to write.
* Before modifying that region, the thread compares the recorded version with the current one.
* If the version changed, the operation retries because the insertion/deletion position may no longer be valid.
* When multiple leaf-region locks are required, the code acquires them from left to right.
* If the operation later discovers that it needs a lock further left, it releases what it holds and retries acquisition from the new leftmost point.
* Full-structure changes such as doubling/halving use the global lock.

This gives each successful update a consistent view of the affected region and prevents deadlock from inconsistent lock ordering.

---

## Partitioned design

`PPPCSR` is a wrapper over multiple `PCSR` partitions.

During construction, the source-vertex id range is split into contiguous ranges according to:

```text
number_of_partitions = number_of_NUMA_domains * partitions_per_domain
```

Each partition owns a contiguous source-id interval. `PPPCSR` keeps a distribution table containing the start vertex of each partition. To process an operation, it finds the partition for the source vertex and subtracts the partition offset before forwarding the operation to the local `PCSR` instance.

For example:

```text
global source vertex: 42
owning partition starts at: 40
local source vertex inside partition: 2
```

This keeps each partition internally simple: each `PCSR` still behaves as if it owns a local vertex id range starting at zero.

---

## NUMA-aware execution

The NUMA-aware path is implemented in two places:

* `PPPCSR` passes a NUMA domain id into each `PCSR` partition when NUMA mode is enabled.
* `ThreadPoolPPPCSR` maps worker threads to NUMA domains and routes tasks to the thread group associated with the partition's domain.

When a worker runs, it attempts to execute on its assigned NUMA node. Update submission chooses the owning partition from the source vertex, maps that partition to a domain, and pushes the task into one of the queues for that domain.

This design is intentionally simple: it is range partitioning by source vertex, not graph-cut minimisation, edge-balanced partitioning, work stealing or dynamic repartitioning. Its purpose is to reduce avoidable remote memory access and contention while preserving the underlying packed CSR layout.

---

## Thread pools

There are two scheduler implementations.

### `ThreadPool`

Used by `PPCSR`.

* Owns one shared `PCSR` instance.
* Keeps one task queue per worker thread.
* `main.cpp` submits updates round-robin with `i % threads`.
* Workers register with the global lock, process queued operations, then unregister.

### `ThreadPoolPPPCSR`

Used by `PPPCSR` and `PPPCSRNUMA`.

* Owns one `PPPCSR` instance.
* Divides workers across available NUMA domains.
* Routes each update by source vertex to the domain that owns the partition.
* Pushes work round-robin within that domain's thread group.
* Registers each worker with the current partition before processing its tasks.

This is an important difference from the original project report: the report describes a simple round-robin worker assignment for the initial parallel PCSR design, while the current repository also includes partition-aware routing for the partitioned variants.

---

## Public API overview

The data-structure classes expose operations such as:

```cpp
bool edge_exists(uint32_t src, uint32_t dest);
void add_node();
void add_edge(uint32_t src, uint32_t dest, uint32_t value);
void remove_edge(uint32_t src, uint32_t dest);
void read_neighbourhood(int src);
std::vector<int> get_neighbourhood(int src) const;
uint64_t get_n() const;
```

`PCSR` implements these operations for a single vertex range. `PPPCSR` implements the same style of operations by finding the relevant partition and forwarding to the corresponding local `PCSR`.

Some additional `PCSR` methods for inserting/removing nodes and edges at the front/back of the structure exist as experimental or lower-level support paths. The command-line program does not expose them as user-facing functionality.

---

## Tests

The CMake configuration builds three test executables:

```bash
make -j tests
make -j tests-tsan
make -j tests-ubsan
```

Run them from the build directory:

```bash
./tests
./tests-tsan
./tests-ubsan
```

The normal test target uses GoogleTest. If a system GoogleTest installation is not found, the build configuration attempts to fetch GoogleTest through CPM.

The tests cover areas including:

* initialization;
* node insertion;
* edge insertion;
* edge deletion;
* edge existence checks;
* neighbourhood reads;
* large sequential add/remove workloads;
* large parallel add/remove workloads;
* lock-release checks after updates;
* BFS over the data structure;
* PageRank-style computation over the data structure;
* partition/domain scheduler behaviour.

The sanitizer targets are useful for development, but they are not a substitute for reviewing the concurrency design carefully.

---

## Benchmarking

Benchmarking scripts live under:

```text
src/benchmarking
```

The repository includes scripts for partitioning and scaling experiments. The scripts expect a configuration file containing paths and parameters such as:

* machine name;
* dataset name;
* executable path;
* core graph file;
* insertion update file;
* deletion update file;
* number of repetitions;
* number of cores;
* partition counts;
* initial update size.

Example shape:

```bash
src/benchmarking/benchmark-partitioning.sh path/to/config.sh
```

The scripts create timestamped benchmark output directories and record raw output, CSV-style summary data and plotting data.

---

## Current limitations and notes

* This is research-oriented systems code rather than a packaged graph database or production library.
* The command-line tool is focused on batched edge insertions and deletions.
* The internal `READ` operation exists, but `main.cpp` does not currently implement a read workload from the input file.
* Input parsing is intentionally simple and assumes clean integer edge-list files.
* The graph is directed. Insert both directions explicitly for undirected workloads.
* Source-range partitioning is simple and deterministic, but may be imbalanced on power-law graphs if high-degree source vertices cluster in one partition.
* `PPPCSR` partitions by source vertex; it does not minimise edge cuts or rebalance dynamically after updates.
* `-lock_free` only affects binary search. Structural modifications still use locks.
* Some lower-level node/edge front/back mutation methods are present but not part of the normal CLI path.
* NUMA-aware mode is most meaningful on a multi-socket / multi-NUMA-node machine.
* The project links against `libnuma` even when explicit NUMA placement is disabled at runtime.
* Diagnostic output is printed during execution, including edge-array resizing, partition counts, per-thread task counts and elapsed time.

---

## References
<a id="1">[1]</a>
Wheatman, B., & Xu, H. (2018).
Packed Compressed Sparse Row: A Dynamic Graph
Representation. 
2018 IEEE High Performance Extreme Computing Conference, HPEC 2018.

<a id="2">[2]</a>
Alevra, E., & Pietzuch, P. (2020).
A Parallel Data Structure for Streaming Graphs. 
Master’s thesis, Imperial College London, 2020.

---

## Authors

* Eleni Alevra
* Christian Menges
* Dom Margan

---

## License

This project is licensed under the MIT License. See `LICENSE` for details.
