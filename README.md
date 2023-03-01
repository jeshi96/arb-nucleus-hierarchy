# Shared-Memory Parallel Nucleus Decomposition Hierarchy

Organization
--------

This repository contains code for our shared-memory parallel (r, s)
nucleus decomposition algorithms. Note
that the repository uses the
[Graph-Based Benchmark Suite (GBBS)](https://github.com/ParAlg/gbbs)
for parallel primitives and benchmarks.

The `benchmarks/NucleusDecomposition/` directory contains the main executable for the exact hierarchy construction, 
`NucleusDecomposition_main`. The `benchmarks/ApproxNucleusDecomposition/` 
directory contains the main executable for the approximate hierarchy construction, 
`NucleusDecomposition_main`.

The `benchmarks/KCore/JulienneDBS17/` and `benchmarks/KTruss/` directories contain
the main executables for the k-core hierarchy and k-truss hierarchy constructions
respectively, where the executables are `KCore_main` and `KTruss_main` respectively.


Compilation
--------

Compiler:
* g++ &gt;= 7.4.0 with pthread support for the [ParlayLib](https://github.com/cmuparlay/parlaylib) scheduler
* g++ &gt;= 7.4.0 with support for Cilk Plus


Build system:
* [Bazel](https:://bazel.build) 2.0.0

We describe the build system for our exact hierarchy construction, but note that
the same concept applies to our approximate, k-core, and k-truss hierarchy
constructions.
To build, navigate to the `benchmarks/NucleusDecomposition/` directory, and run:
```sh
$ bazel build :NucleusDecomposition_main
```

Most optionality from [GBBS](https://github.com/ParAlg/gbbs)
applies. In particular, to compile benchmarks for graphs with
more than 2^32 edges, the `LONG` command-line parameter should be set.

The default compilation is the ParlayLib scheduler (Homegrown).
To compile with the Cilk Plus scheduler instead of the Homegrown scheduler, use
the Bazel configuration `--config=cilk`. To compile using OpenMP instead, use
the Bazel configuration `--config=openmp`. To compile serially instead, use the
Bazel configuration `--config=serial`.

Note that the default compilation mode in bazel is to build optimized binaries
(stripped of debug symbols). You can compile debug binaries by supplying `-c
dbg` to the bazel build command.

The following command cleans the directory:
```sh
$ bazel clean  # removes all executables
```

Graph Format
-------

The applications take as input the adjacency graph format used by
[GBBS](https://github.com/ParAlg/gbbs).

The adjacency graph format starts with a sequence of offsets one for each
vertex, followed by a sequence of directed edges ordered by their source vertex.
The offset for a vertex i refers to the location of the start of a contiguous
block of out edges for vertex i in the sequence of edges. The block continues
until the offset of the next vertex, or the end if i is the last vertex. All
vertices and offsets are 0 based and represented in decimal. The specific format
is as follows:

```
AdjacencyGraph
<n>
<m>
<o0>
<o1>
...
<o(n-1)>
<e0>
<e1>
...
<e(m-1)>
```

This file is represented as plain text.

**Using SNAP Graphs**

Graphs from the [SNAP dataset
collection](https://snap.stanford.edu/data/index.html) are used in our experiments. 
We use a tool from the [GBBS](https://github.com/ParAlg/gbbs) 
that converts the most common SNAP
graph format to the adjacency graph format that GBBS accepts. Usage example:
```sh
# Download a graph from the SNAP collection.
wget https://snap.stanford.edu/data/wiki-Vote.txt.gz
gzip --decompress ${PWD}/wiki-Vote.txt.gz
# Run the SNAP-to-adjacency-graph converter.
# Run with Bazel:
bazel run //utils:snap_converter -- -s -i ${PWD}/wiki-Vote.txt -o <output file>
```


Running Code
-------
The applications take the input graph as input, as well as flags to specify
the parameters of the (r, s) nucleus decomposition algorithm and desired 
optimizations. Note that the `-s` flag must be set to indicate a symmetric 
(undirected) graph, and the `-rounds 1` argument must be passed in.
Also, the following default flags must necessarily be passed in as well: 
`-compress -relabel -efficient 5 -tt 5 -contig`.



inline = ["", "-efficient_inline"] #"", "-inline", "-efficient_inline"]
inline_pre = ["ni", "ei"] #"ni", "i", "ei"]


The options for arguments are:
* `-r` followed by an integer specifying r
* `-ss` followed by an integer specifying s
* `-efficient_inline`, which should be used if ANH-EL is desired (the default is ANH-TE)
* `-inline`, which should be used if ANH-BL is desired (the default is ANH-TE)

**Example Usage**

The main executable is `NucleusDecomposition_main` in the `benchmarks/NucleusDecomposition/` directory.

After navigating to the `benchmarks/NucleusDecomposition/` directory, a template command is:

```sh
$ bazel run :NucleusDecomposition_main -- -s -rounds 1 -compress -relabel -efficient 5 -tt 5 -contig -r 3 -ss 4 -efficient_inline </path/to/input/graph>
```