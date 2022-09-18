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

from m5.params import *
from m5.proxy import *
from m5.objects.ClockedObject import ClockedObject

class HitchhikerR(ClockedObject):
    type = 'HitchhikerR'
    cxx_header = "learning_gem5/hitchhiker/rho/hitchhikerR.hh"

    # Vector port example. Both the instruction and data ports connect to this
    # port which is automatically split out into two ports.
    cpu_side = VectorSlavePort("CPU side port, receives requests")
    mem_side = MasterPort("Memory side port, sends requests")
    rho_side = MasterPort("Rho side port, sends requests")

    latency = Param.Cycles(1, "Cycles taken on a hit or to resolve a miss")

    stash_size = Param.MemorySize('16kB', "The size of the cache") # 256 blocks

    tag_size = Param.MemorySize('256kB', "The size of Rho tag")

    queue_size = Param.Int(16, "The size of the request queue")

    age_threshold = Param.Int(8, "The threshold of age in request queue")

    bucket_size = Param.Int(4, "The bucket size of ORAM")

    rho_bktsize = Param.Int(2, "The bucket size of Rho cache")

    utilization = Param.Float(1.0, "Utilization of memory")

    rho_util = Param.Float(0.25, "Utilization of Rho cache")

    warmup_cnt = Param.UInt64(0, "Number of instructions of warm-up")

    progress_interval = Param.UInt64(1000000, "Number of instructions between two progress reports")

    system = Param.System(Parent.any, "The system this cache is part of")
