# Overview

RogueDB firmly believes benchmarks ought to publish the source code for replication purposes by others. This directory contains all of our benchmarks for transparent sourcing of our numbers via external review. We take great pains for the source code to utilize minimal abstractions and indirection for ease of understanding internally and externally.

This directory serves two purposes:

- Provide source code for external replication of our numbers with the setup details
- Provide educational resources for individuals interested in HPC and distributed systems performance

## Long-Term Objectives

Systematically benchmark software architectures, communication patterns, design trade-offs, and any other associated "knobs" that allows tuning of performance. Specifically, we aim to document the performance associated with each knob to further improve the performance of RogueDB. While proprietary source code will never be included, the benchmarks are still universally applicable for individuals designing high-performance systems.

For our users, we plan on expanding extensively beyond the YCSB General Purpose benchmark utilized by BenchAnt to identify bottlenecks and fine tune areas of performance. Workloads will eventually cover a very large number of use cases such that those considering or actively using RogueDB can gain insights on expected performance that matches their workload best. We will notate, as appropriate, the optimal use pattern with RogueDB for you to maximize performance for specific workloads.
