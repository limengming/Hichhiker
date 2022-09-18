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
(options, args) = parser.parse_args()
if options.benchmark:
    print('Selected SPEC_CPU2017 benchmark')
    if options.benchmark == 'perlbench_r':
        print('--> perlbench_r')
        process = spec17_benchmarks.perlbench_r
    elif options.benchmark == 'perlbench_s':
        print('--> perlbench_s')
        process = spec17_benchmarks.perlbench_s
    elif options.benchmark == 'gcc_r':
        print('--> gcc_r')
        process = spec17_benchmarks.gcc_r
    elif options.benchmark == 'gcc_s':
        print('--> gcc_s')
        process = spec17_benchmarks.gcc_s
    elif options.benchmark == 'mcf_r':
        print('--> mcf_r')
        process = spec17_benchmarks.mcf_r
    elif options.benchmark == 'mcf_s':
        print('--> mcf_s')
        process = spec17_benchmarks.mcf_s
    elif options.benchmark == 'omnetpp_r':
        print('--> omnetpp_r')
        process = spec17_benchmarks.omnetpp_r
    elif options.benchmark == 'omnetpp_s':
        print('--> omnetpp_s')
        process = spec17_benchmarks.omnetpp_s
    elif options.benchmark == 'xalancbmk_r':
        print('--> xalancbmk_r')
        process = spec17_benchmarks.xalancbmk_r
    elif options.benchmark == 'xalancbmk_s':
        print('--> xalancbmk_s')
        process = spec17_benchmarks.xalancbmk_s
    elif options.benchmark == 'x264_r':
        print('--> x264_r')
        process = spec17_benchmarks.x264_r
    elif options.benchmark == 'x264_s':
        print('--> x264_s')
        process = spec17_benchmarks.x264_s
    elif options.benchmark == 'deepsjeng_r':
        print('--> deepsjeng_r')
        process = spec17_benchmarks.deepsjeng_r
    elif options.benchmark == 'deepsjeng_s':
        print('--> deepsjeng_s')
        process = spec17_benchmarks.deepsjeng_s
    elif options.benchmark == 'leela_r':
        print('--> leela_r')
        process = spec17_benchmarks.leela_r
    elif options.benchmark == 'leela_s':
        print('--> leela_s')
        process = spec17_benchmarks.leela_s
    elif options.benchmark == 'exchange2_r':
        print('--> exchange2_r')
        process = spec17_benchmarks.exchange2_r
    elif options.benchmark == 'exchange2_s':
        print('--> exchange2_s')
        process = spec17_benchmarks.exchange2_s
    elif options.benchmark == 'xz_r':
        print('--> xz_r')
        process = spec17_benchmarks.xz_r
    elif options.benchmark == 'xz_s':
        print('--> xz_s')
        process = spec17_benchmarks.xz_s
    elif options.benchmark == 'bwaves_r':
        print('--> bwaves_r')
        process = spec17_benchmarks.bwaves_r
    elif options.benchmark == 'bwaves_s':
        print('--> bwaves_s')
        process = spec17_benchmarks.bwaves_s
    elif options.benchmark == 'cactuBSSN_r':
        print('--> cactuBSSN_r')
        process = spec17_benchmarks.cactuBSSN_r
    elif options.benchmark == 'cactuBSSN_s':
        print('--> cactuBSSN_s')
        process = spec17_benchmarks.cactuBSSN_s
    elif options.benchmark == 'namd_r':
        print('--> namd_r')
        process = spec17_benchmarks.namd_r
    elif options.benchmark == 'parest_r':
        print('--> parest_r')
        process = spec17_benchmarks.parest_r
    elif options.benchmark == 'povray_r':
        print('--> povray_r')
        process = spec17_benchmarks.povray_r
    elif options.benchmark == 'lbm_r':
        print('--> lbm_r')
        process = spec17_benchmarks.lbm_r
    elif options.benchmark == 'lbm_s':
        print('--> lbm_s')
        process = spec17_benchmarks.lbm_s
    elif options.benchmark == 'wrf_r':
        print('--> wrf_r')
        process = spec17_benchmarks.wrf_r
    elif options.benchmark == 'wrf_s':
        print('--> wrf_s')
        process = spec17_benchmarks.wrf_s
    elif options.benchmark == 'blender_r':
        print('--> blender_r')
        process = spec17_benchmarks.blender_r
    elif options.benchmark == 'cam4_r':
        print('--> cam4_r')
        process = spec17_benchmarks.cam4_r
    elif options.benchmark == 'cam4_s':
        print('--> cam4_s')
        process = spec17_benchmarks.cam4_s
    elif options.benchmark == 'pop2_s':
        print('--> pop2_s')
        process = spec17_benchmarks.pop2_s
    elif options.benchmark == 'imagick_r':
        print('--> imagick_r')
        process = spec17_benchmarks.imagick_r
    elif options.benchmark == 'imagick_s':
        print('--> imagick_s')
        process = spec17_benchmarks.imagick_s
    elif options.benchmark == 'nab_r':
        print('--> nab_r')
        process = spec17_benchmarks.nab_r
    elif options.benchmark == 'nab_s':
        print('--> nab_s')
        process = spec17_benchmarks.nab_s
    elif options.benchmark == 'fotonik3d_r':
        print('--> fotonik3d_r')
        process = spec17_benchmarks.fotonik3d_r
    elif options.benchmark == 'fotonik3d_s':
        print('--> fotonik3d_s')
        process = spec17_benchmarks.fotonik3d_s
    elif options.benchmark == 'roms_r':
        print('--> roms_r')
        process = spec17_benchmarks.roms_r
    elif options.benchmark == 'roms_s':
        print('--> roms_s')
        process = spec17_benchmarks.roms_s
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
system = System()

# Set the clock fequency of the system (and all of its children)
system.clk_domain = SrcClockDomain()
system.clk_domain.clock = '1GHz'
system.clk_domain.voltage_domain = VoltageDomain()

# Set up the system
system.mem_mode = 'timing'               # Use timing accesses
system.mem_ranges = [AddrRange('2048MB')] # Create an address range

# Create a simple CPU
system.cpu = TimingSimpleCPU()
# system.cpu = DerivO3CPU()
# system.cpu = MinorCPU()
# system.cpu = TraceCPU()
# system.cpu = MinorTrace()

#Set the maxinsts
if options.maxinsts:
    system.cpu.max_insts_any_thread = options.maxinsts

# Create a memory bus, a coherent crossbar, in this case
system.membus = SystemXBar()
# system.doublebus = SystemXBar()

system.cache = SimpleCacheQueue(size='256kB')
# Create a simple cache
# system.l2cache = DoubleMemobj()
# system.icache = L1ICache()
# system.dcache = L1DCache()

# Connect the I and D cache ports of the CPU to the memobj.
# Since cpu_side is a vector port, each time one of these is connected, it will
# create a new instance of the CPUSidePort class
# system.cpu.icache_port = system.icache.cpu_side
# system.cpu.dcache_port = system.dcache.cpu_side
system.cpu.icache_port = system.cache.cpu_side
system.cpu.dcache_port = system.cache.cpu_side

# system.cache.mem_side = system.l2cache.cpu_side
system.cache.mem_side = system.membus.slave

# system.l2bus = L2XBar()

# Hook the cache up to the memory bus
# system.icache.mem_side = system.l2bus.slave
# system.dcache.mem_side = system.l2bus.slave
# system.cache.mem_side = system.l2bus.slave

# system.l2cache.mem_side = system.membus.slave
# system.l2cache.double_port = system.doublebus.slave

# create the interrupt controller for the CPU and connect to the membus
system.cpu.createInterruptController()
system.cpu.interrupts[0].pio = system.membus.master
system.cpu.interrupts[0].int_master = system.membus.slave
system.cpu.interrupts[0].int_slave = system.membus.master

# Create a DDR3 memory controller and connect it to the membus
system.mem_ctrl = DDR4_2400_8x8()
# system.mem_ctrl = DDR3_1600_8x8()
system.mem_ctrl.range = AddrRange('2048MB')
system.mem_ctrl.port = system.membus.master

# system.cache_mem = DDR4_2400_8x8()
# system.cache_mem.range = AddrRange('2048MB', '2064MB')
# system.cache_mem.port = system.doublebus.master

# Connect the system up to the membus
system.system_port = system.membus.slave

# Create a process for a simple "Hello World" application
#process = Process()
# Set the command
# grab the specific path to the binary
#thispath = os.path.dirname(os.path.realpath(__file__))
#binpath = os.path.join(thispath, '../../../',
#                      'tests/test-progs/hello/bin/x86/linux/hello')
# cmd is a list which begins with the executable (like argv)
# process.cmd = [binpath]
#process.cmd = ['tests/test-progs/hello/bin/x86/linux/hello']
# process.cmd = ['/root/test/matrix_mul']
# Set the cpu to use the process as its workload and create thread contexts
system.cpu.workload = process
print(process.cmd)
system.cpu.createThreads()

# set up the root SimObject and start the simulation
root = Root(full_system = False, system = system)
# instantiate all of the objects we've created above
m5.instantiate()

print("Beginning simulation!")
exit_event = m5.simulate()
print('Exiting @ tick %i because %s' % (m5.curTick(), exit_event.getCause()))
