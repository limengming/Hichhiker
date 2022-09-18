# -*- coding: utf-8 -*-
# Copyright (c) 2017 Jason Lowe-Power
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met: redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer;
# redistributions in binary form must reproduce the above copyright
# notice, this list of conditions and the following disclaimer in the
# documentation and/or other materials provided with the distribution;
# neither the name of the copyright holders nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#
# Authors: Jason Lowe-Power

""" This file creates a barebones system and executes 'hello', a simple Hello
World application. Adds a simple cache between the CPU and the membus.

This config file assumes that the x86 ISA was built.
"""

from __future__ import print_function
from __future__ import absolute_import

# import the m5 (gem5) library created when gem5 is built
import m5
import spec17_benchmarks
# import all of the SimObjects
from m5.objects import *
from optparse import OptionParser
#from caches import *
#add spec option
parser = OptionParser()
parser.add_option("-b", "--benchmark", type="string", default="", help="The SPEC benchmark to be loaded.")
parser.add_option("--benchmark_stdout", type="string", default="", help="Absolute path for stdout redirection for the benchmark.")
parser.add_option("--benchmark_stderr", type="string", default="", help="Absolute path for stderr redirection for the benchmark.")
parser.add_option("-I", "--maxinsts", action="store", type="int", default=None, help="""Total number of instructions to simulate (default: run forever)""")
parser.add_option("-W", "--warmupinsts", action="store", type="int", default=None, help="""Total number of instructions to warm up (default: no warm up)""")
(options, args) = parser.parse_args()
process = []
if options.benchmark:
    print('Selected SPEC_CPU2017 benchmark')
    benches = options.benchmark.split(",")
    for i in range(len(benches)):
        if benches[i] == 'perlbench_r':
            print('--> perlbench_r')
            process.append(spec17_benchmarks.perlbench_r)
        elif benches[i] == 'perlbench_s':
            print('--> perlbench_s')
            process.append(spec17_benchmarks.perlbench_s)
        elif benches[i] == 'gcc_r':
            print('--> gcc_r')
            process.append(spec17_benchmarks.gcc_r)
        elif benches[i] == 'gcc_s':
            print('--> gcc_s')
            process.append(spec17_benchmarks.gcc_s)
        elif benches[i] == 'mcf_r':
            print('--> mcf_r')
            process.append(spec17_benchmarks.mcf_r)
        elif benches[i] == 'mcf_s':
            print('--> mcf_s')
            process.append(spec17_benchmarks.mcf_s)
        elif benches[i] == 'omnetpp_r':
            print('--> omnetpp_r')
            process.append(spec17_benchmarks.omnetpp_r)
        elif benches[i] == 'omnetpp_s':
            print('--> omnetpp_s')
            process.append(spec17_benchmarks.omnetpp_s)
        elif benches[i] == 'xalancbmk_r':
            print('--> xalancbmk_r')
            process.append(spec17_benchmarks.xalancbmk_r)
        elif benches[i] == 'xalancbmk_s':
            print('--> xalancbmk_s')
            process.append(spec17_benchmarks.xalancbmk_s)
        elif benches[i] == 'x264_r':
            print('--> x264_r')
            process.append(spec17_benchmarks.x264_r)
        elif benches[i] == 'x264_s':
            print('--> x264_s')
            process.append(spec17_benchmarks.x264_s)
        elif benches[i] == 'deepsjeng_r':
            print('--> deepsjeng_r')
            process.append(spec17_benchmarks.deepsjeng_r)
        elif benches[i] == 'deepsjeng_s':
            print('--> deepsjeng_s')
            process.append(spec17_benchmarks.deepsjeng_s)
        elif benches[i] == 'leela_r':
            print('--> leela_r')
            process.append(spec17_benchmarks.leela_r)
        elif benches[i] == 'leela_s':
            print('--> leela_s')
            process.append(spec17_benchmarks.leela_s)
        elif benches[i] == 'exchange2_r':
            print('--> exchange2_r')
            process.append(spec17_benchmarks.exchange2_r)
        elif benches[i] == 'exchange2_s':
            print('--> exchange2_s')
            process.append(spec17_benchmarks.exchange2_s)
        elif benches[i] == 'xz_r':
            print('--> xz_r')
            process.append(spec17_benchmarks.xz_r)
        elif benches[i] == 'xz_s':
            print('--> xz_s')
            process.append(spec17_benchmarks.xz_s)
        elif benches[i] == 'bwaves_r':
            print('--> bwaves_r')
            process.append(spec17_benchmarks.bwaves_r)
        elif benches[i] == 'bwaves_s':
            print('--> bwaves_s')
            process.append(spec17_benchmarks.bwaves_s)
        elif benches[i] == 'cactuBSSN_r':
            print('--> cactuBSSN_r')
            process.append(spec17_benchmarks.cactuBSSN_r)
        elif benches[i] == 'cactuBSSN_s':
            print('--> cactuBSSN_s')
            process.append(spec17_benchmarks.cactuBSSN_s)
        elif benches[i] == 'namd_r':
            print('--> namd_r')
            process.append(spec17_benchmarks.namd_r)
        elif benches[i] == 'parest_r':
            print('--> parest_r')
            process.append(spec17_benchmarks.parest_r)
        elif benches[i] == 'povray_r':
            print('--> povray_r')
            process.append(spec17_benchmarks.povray_r)
        elif benches[i] == 'lbm_r':
            print('--> lbm_r')
            process.append(spec17_benchmarks.lbm_r)
        elif benches[i] == 'lbm_s':
            print('--> lbm_s')
            process.append(spec17_benchmarks.lbm_s)
        elif benches[i] == 'wrf_r':
            print('--> wrf_r')
            process.append(spec17_benchmarks.wrf_r)
        elif benches[i] == 'wrf_s':
            print('--> wrf_s')
            process.append(spec17_benchmarks.wrf_s)
        elif benches[i] == 'blender_r':
            print('--> blender_r')
            process.append(spec17_benchmarks.blender_r)
        elif benches[i] == 'cam4_r':
            print('--> cam4_r')
            process.append(spec17_benchmarks.cam4_r)
        elif benches[i] == 'cam4_s':
            print('--> cam4_s')
            process.append(spec17_benchmarks.cam4_s)
        elif benches[i] == 'pop2_s':
            print('--> pop2_s')
            process.append(spec17_benchmarks.pop2_s)
        elif benches[i] == 'imagick_r':
            print('--> imagick_r')
            process.append(spec17_benchmarks.imagick_r)
        elif benches[i] == 'imagick_s':
            print('--> imagick_s')
            process.append(spec17_benchmarks.imagick_s)
        elif benches[i] == 'nab_r':
            print('--> nab_r')
            process.append(spec17_benchmarks.nab_r)
        elif benches[i] == 'nab_s':
            print('--> nab_s')
            process.append(spec17_benchmarks.nab_s)
        elif benches[i] == 'fotonik3d_r':
            print('--> fotonik3d_r')
            process.append(spec17_benchmarks.fotonik3d_r)
        elif benches[i] == 'fotonik3d_s':
            print('--> fotonik3d_s')
            process.append(spec17_benchmarks.fotonik3d_s)
        elif benches[i] == 'roms_r':
            print('--> roms_r')
            process.append(spec17_benchmarks.roms_r)
        elif benches[i] == 'roms_s':
            print('--> roms_s')
            process.append(spec17_benchmarks.roms_s)
        else:
            print("No recognized SPEC2006 benchmark selected! Exiting.")
            sys.exit(1)
else:
    print >> sys.stderr, "Need --benchmark switch to specify SPEC CPU2017 workload. Exiting!\n"
    sys.exit(1)
 
# Set process stdout/stderr
if options.benchmark_stdout:
    process.output = options.benchmark_stdout
    print("Process stdout file: " + process.output)
if options.benchmark_stderr:
    process.errout = options.benchmark_stderr
    print("Process stderr file: " + process.errout)
# create the system we are going to simulate
# system = System(cpu = [DerivO3CPU(cpu_id=i) for i in range(len(process))])
system = System(cpu = [TimingSimpleCPU(cpu_id=i) for i in range(len(process))])

# Set the clock fequency of the system (and all of its children)
system.clk_domain = SrcClockDomain()
system.clk_domain.clock = '2GHz'
system.clk_domain.voltage_domain = VoltageDomain()

# Set up the system
system.mem_mode = 'timing'               # Use timing accesses
system.mem_ranges = [AddrRange('4096MB'), AddrRange('4096MB', '5120MB')] # Create an address range

# Create a memory bus, a coherent crossbar, in this case
system.membus = SystemXBar()
system.rhobus = SystemXBar()

#Set the oram
#system.oramcontroller = HitchhikerD(queue_size=128, stash_size='256kB')
system.oramcontroller = HitchhikerR(stash_size='256kB')
system.oramcontroller.mem_side = system.membus.slave
system.oramcontroller.rho_side = system.rhobus.slave

#Set the L2 cache
#system.l2bus = L2XBar()
system.l2cache = SimpleCacheQueue(size='1MB')
#system.l2cache.cpu_side = system.l2bus.master
system.l2cache.mem_side = system.oramcontroller.cpu_side

#system.cpu = []
for i in range(len(process)):
    #system.cpu[i] = DerivO3CPU()
    #set the private cache
    #icache = SimpleCacheQueue(size='512kB')
    #dcache = SimpleCacheQueue(size='512kB')
    #system.cpu[i].icache = icache
    #system.cpu[i].icache.cpu_side = system.cpu[i].icache_port
    #system.cpu[i].icache.mem_side = system.l2cache.cpu_side
    #system.cpu[i].dcache = dcache
    #system.cpu[i].dcache.cpu_side = system.cpu[i].dcache_port
    #system.cpu[i].dcache.mem_side = system.l2cache.cpu_side
    system.cpu[i].cache = SimpleCacheQueue(size='32kB')
    system.cpu[i].icache_port = system.cpu[i].cache.cpu_side
    system.cpu[i].dcache_port = system.cpu[i].cache.cpu_side
    system.cpu[i].cache.mem_side = system.l2cache.cpu_side
    
    #set the maxinsts
    if options.maxinsts:
        system.cpu[i].max_insts_any_thread = options.maxinsts
    
    if options.warmupinsts:
        system.cpu[i].cache.warmup_cnt = options.warmupinsts
        
    #set the interrupt
    system.cpu[i].createInterruptController()
    system.cpu[i].interrupts[0].pio = system.membus.master
    system.cpu[i].interrupts[0].int_master = system.membus.slave
    system.cpu[i].interrupts[0].int_slave = system.membus.master
    
    #set the workload
    system.cpu[i].workload = process[i]
    print(process[i].cmd)
    
    system.cpu[i].createThreads()

if options.warmupinsts:
    system.l2cache.warmup_cnt = options.warmupinsts
    system.oramcontroller.warmup_cnt = options.warmupinsts

# Create a DDR3 memory controller and connect it to the membus
system.mem_ctrl = DDR4_2400_8x8()
# system.mem_ctrl = DDR3_1600_8x8()
system.mem_ctrl.range = system.mem_ranges[0]
system.mem_ctrl.port = system.membus.master

system.rho_ctrl = DDR4_2400_8x8()
system.rho_ctrl.range = system.mem_ranges[1]
system.rho_ctrl.port = system.rhobus.master

# Connect the system up to the membus
system.system_port = system.membus.slave

# set up the root SimObject and start the simulation
root = Root(full_system = False, system = system)
# instantiate all of the objects we've created above
m5.instantiate()

print("Beginning simulation!")
exit_event = m5.simulate()
print('Exiting @ tick %i because %s' % (m5.curTick(), exit_event.getCause()))
