# DROPLET
This repository contains the code of the modified architecture simulator (Sniper) used to implement DROPLET in the following [HPCA 2019](http://hpca2019.seas.gwu.edu/) [paper](https://seal.ece.ucsb.edu/sites/default/files/publications/hpca-2019-abanti.pdf): 

> **A. Basak, S. Li, X. Hu, S. M. Oh, X. Xie, L. Zhao, X. Jiang, and Y. Xie, *Analysis and Optimization of the Memory Hierarchy for Graph Processing Workloads***

The repository contains the modifications made to the Sniper-6.1 simulator for the characterization and the DROPLET prefetcher design in the paper. 
Note that this is only the modified simualtor code (the folders of the Sniper simualtor where we changes the source code). The workloads that we ran for the paper are [GAP Benchmark Suite](https://github.com/sbeamer/gapbs) with the datasets mentioned in the paper. 

**example_runme.sh** is an example command line that we ran for our experiments. 