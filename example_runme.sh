#!/bin/bash
data=/home/abasak/sniper-6.1/test/gapbs/data/
executable=/home/abasak/gem5/tests/test-progs/gapbs/pr
cfg_file=/home/abasak/sniper-6.1_new-trial/config/

/home/abasak/sniper-6.1_new-trial/run-sniper -n 4 -c ${cfg_file}gainestown -c rob -c prefetcher -g perf_model/core/interval_timer/lll_cutoff=35 -g perf_model/dram/per_controller_bandwidth=30 -s stop-by-icount:600000000 --roi -- ${executable} -f ${data}soc-LiveJournal1.sg -n 1 -i 1 -t1e-4
