# Parallel Packed CSR
A parallel implementation of the packed CSR data structure, based on its initial single threaded design[[1]](#1)(https://github.com/wheatman/Packed-Compressed-Sparse-Row/) and further parallel extension [[2]](#2).
Accepts a core graph, which it loads first and an update file, which it uses to update the core graph (insert or delete edges).
Performs edge insertions and deletions in parallel and uses NUMA-aware placement of data and worker threads.

# Prerequisites
* CMAKE 3.5 or newer required.

# Build
The commands to build are the following:

```
$ cd src
$ cmake .
$ make
```

# Data files
The sample/test data files included in the `data/` directory are the following:

* `shuffled_higgs.txt`: core graph, a shuffled version of the [Higgs Twitter dataset](https://snap.stanford.edu/data/higgs-twitter.html).
* `update_files/insertions.txt`: a list of 1 million edges, randomly sampled from a power law distribution, to be inserted.
* `update_files/deletions.txt`: a list of 1 million edges in the core graph to be deleted.

# Running
Run with `./cmake-build-BUILD_TYPE/parallel-packed-csr`

## Command line options
* `-threads=`: specifies number of threads to use for updates, default=8
* `-size=`: specifies number of edges that will be read from the update file, default=1000000
* `-lock_free`: runs the data structure lock-free version of binary search, locks during binary search by default
* `-partitions_per_domain=`: specifies the number of graph partitions per NUMA domain
* `-insert`: inserts the edges from the update file to the core graph
* `-delete`: deletes the edges from the update file from the core graph
* `-core_graph=`: specifies the filename of the core graph, default=shuffled_higgs.txt
* `-update_file=`: specifies the filename of the update file, default=insertions.txt

# Authors
* Eleni Alevra
* Christian Menges 
* Dom Margan 

# References
[1]
Wheatman, B., & Xu, H. (2018).
Packed Compressed Sparse Row: A Dynamic Graph
Representation. 
2018 IEEE High Performance Extreme Computing Conference, HPEC 2018.

<a id="2">[2]</a>
Alevra, E., & Pietzuch, P. (2020).
A Parallel Data Structure for Streaming Graphs. 
Master’s thesis, Imperial College London, 2020.
