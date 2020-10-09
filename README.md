# Parallel Packed CSR
A parallel implementation of the packed CSR data structure, based on its initial single threaded design[[1]](#1)(https://github.com/wheatman/Packed-Compressed-Sparse-Row/) and further parallel extension [[2]](#2).
Accepts a core graph, which it loads first and an update file, which it uses to update the core graph (insert or delete edges).
Performs edge insertions and deletions in parallel and uses NUMA-aware placement of data and worker threads.

# Prerequisites
* CMAKE 3.8 or newer required.

# Build
Create a build directory and run cmake & make there:
```
$ mkdir build && cd build
$ cmake ..
$ make
```
# Running
Run the `parallel-packed-csr` binary from your build directory.

## Command line options
* `-threads=`: specifies number of threads to use for updates, default=8
* `-size=`: specifies number of edges that will be read from the update file, default=1000000
* `-lock_free`: runs the data structure lock-free version of binary search, locks during binary search by default
* `-partitions_per_domain=`: specifies the number of graph partitions per NUMA domain
* `-insert`: inserts the edges from the update file to the core graph
* `-delete`: deletes the edges from the update file from the core graph
* `-core_graph=`: specifies the filename of the core graph
* `-update_file=`: specifies the filename of the update file
* Available partitioning strategies (if multiple strategies are given, the last one is used):
  * `-ppcsr`: No partitioning
  * `-pppcsr`: Partitioning (1 partition per NUMA domain)
  * `-pppcsrnuma`: Partitioning with explicit NUMA optimizations (default)

# Authors
* Eleni Alevra
* Christian Menges 
* Dom Margan 

# References
<a id="1">[1]</a>
Wheatman, B., & Xu, H. (2018).
Packed Compressed Sparse Row: A Dynamic Graph
Representation. 
2018 IEEE High Performance Extreme Computing Conference, HPEC 2018.

<a id="2">[2]</a>
Alevra, E., & Pietzuch, P. (2020).
A Parallel Data Structure for Streaming Graphs. 
Master’s thesis, Imperial College London, 2020.
