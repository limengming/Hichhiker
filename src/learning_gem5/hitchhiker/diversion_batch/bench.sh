#!/bin/bash

#list="perlbench_r gcc_r omnetpp_r omnetpp_s xalancbmk_r xalancbmk_s";
list="wrf_r pop2_s nab_r nab_s specrand_fs specrand_fr"
for bench in $list;
do
	./gem5.opt run4.py -I 100000000 -b $bench;
done
