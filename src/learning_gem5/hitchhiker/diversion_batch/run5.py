# Copyright (c) 2012-2013 ARM Limited
# All rights reserved.
#
# The license below extends only to copyright in the software and shall
# not be construed as granting a license to any other intellectual
# property including but not limited to intellectual property relating
# to a hardware implementation of the functionality of the software
# licensed hereunder.  You may use the software subject to the license
# terms below provided that you ensure that this notice is replicated
# unmodified and in its entirety in all distributions of the software,
# modified or unmodified, in source code or in binary form.
#
# Copyright (c) 2006-2008 The Regents of The University of Michigan
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
# Authors: Steve Reinhardt

# Simple test script
#
# "m5 test.py"

from __future__ import print_function
from __future__ import absolute_import

import optparse
import sys
import os

import m5
import spec17_benchmarks
from m5.defines import buildEnv
from m5.objects import *
from m5.util import addToPath, fatal, warn

addToPath('../')


from common import Options
from common import Simulation
from common import CacheConfig
from common import CpuConfig
from common import ObjectList
from common import MemConfig
from common.FileSystemConfig import config_filesystem
from common.Caches import *
from common.cpu2000 import *

parser = optparse.OptionParser()
Options.addCommonOptions(parser)
Options.addSEOptions(parser)
parser.add_option("-b", "--benchmark", type="string", default="", help="The SPEC benchmark to be loaded.")
parser.add_option("--benchmark_stdout", type="string", default="", help="Absolute path for stdout redirection for the benchmark.")
parser.add_option("--benchmark_stderr", type="string", default="", help="Absolute path for stderr redirection for the benchmark.")


(options, args) = parser.parse_args()

if args:
    print("Error: script doesn't take any positional arguments")
    sys.exit(1)

process = []
numThreads = 1
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
            print("No recognized SPEC2017 benchmark selected! Exiting.")
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
    
options.num_cpus = len(process)

(CPUClass, test_mem_mode, FutureClass) = Simulation.setCPUClass(options)
CPUClass.numThreads = numThreads

print(CPUClass)

# Check -- do not allow SMT with multiple CPUs
if options.smt and options.num_cpus > 1:
    fatal("You cannot use SMT with multiple CPUs!")

np = options.num_cpus
system = System(cpu = [CPUClass(cpu_id=i) for i in range(np)],
                mem_mode = test_mem_mode,
                mem_ranges = [AddrRange('4096MB')])

if numThreads > 1:
    system.multi_thread = True

# Create a top-level voltage domain
system.voltage_domain = VoltageDomain()

# Set the clock fequency of the system (and all of its children)
system.clk_domain = SrcClockDomain()
system.clk_domain.clock = '2GHz'
system.clk_domain.voltage_domain = VoltageDomain()

# If elastic tracing is enabled, then configure the cpu and attach the elastic
# trace probe
if options.elastic_trace_en:
    CpuConfig.config_etrace(CPUClass, system.cpu, options)

if ObjectList.is_kvm_cpu(CPUClass) or ObjectList.is_kvm_cpu(FutureClass):
    if buildEnv['TARGET_ISA'] == 'x86':
        system.kvm_vm = KvmVM()
        for process in multiprocesses:
            process.useArchPT = True
            process.kvmInSE = True
    else:
        fatal("KvmCPU can only be used in SE mode with x86")

# Sanity check
if options.simpoint_profile:
    if not ObjectList.is_noncaching_cpu(CPUClass):
        fatal("SimPoint/BPProbe should be done with an atomic cpu")
    if np > 1:
        fatal("SimPoint generation not supported with more than one CPUs")

# Create a memory bus, a coherent crossbar, in this case
system.membus = SystemXBar()

#Set the oram
system.oramcontroller = HitchhikerD(queue_size=128, stash_size='256kB')
system.oramcontroller.mem_side = system.membus.slave

#Set the L2 cache
system.l2cache = SimpleCacheQueue(size='2MB')
system.l2cache.mem_side = system.oramcontroller.cpu_side

# Create a DDR3 memory controller and connect it to the membus
system.mem_ctrl = DDR4_2400_8x8()
# system.mem_ctrl = DDR3_1600_8x8()
system.mem_ctrl.range = system.mem_ranges[0]
system.mem_ctrl.port = system.membus.master
# Connect the system up to the membus
system.system_port = system.membus.slave

for i in range(np):
    system.cpu[i].cache = SimpleCacheQueue(size='512kB')
    system.cpu[i].icache_port = system.cpu[i].cache.cpu_side
    system.cpu[i].dcache_port = system.cpu[i].cache.cpu_side
    system.cpu[i].cache.mem_side = system.l2cache.cpu_side
        
    #set the interrupt
    system.cpu[i].createInterruptController()
    system.cpu[i].interrupts[0].pio = system.membus.master
    system.cpu[i].interrupts[0].int_master = system.membus.slave
    system.cpu[i].interrupts[0].int_slave = system.membus.master

    #set the workload
    system.cpu[i].workload = process[i]
    print(process[i].cmd)

    system.cpu[i].createThreads()

root = Root(full_system = False, system = system)
Simulation.run(options, root, system, FutureClass)
