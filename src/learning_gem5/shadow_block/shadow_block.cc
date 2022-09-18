/*
 * Copyright (c) 2017 Jason Lowe-Power
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met: redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer;
 * redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution;
 * neither the name of the copyright holders nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Authors: Jason Lowe-Power
 */

// #include "learning_gem5/path_shuffle/path_shuffle.hh"
#include "shadow_block.hh"

#include "base/random.hh"
#include "debug/ShadowBlock.hh"
#include "debug/InstProgress.hh"
#include "sim/system.hh"

static unsigned log_binary(uint64_t num)
{
    assert(num != 0);
    for (unsigned i = 1; i < 64; i++)
        if (num >> i == 0)
            return i - 1;
    return -1; // error
}

ShadowBlock::ShadowBlock(ShadowBlockParams *params) :
    ClockedObject(params),
    system(params->system),
    warmupCnt(params->warmup_cnt),
    warmup(warmupCnt == 0 ? false : true),
    interval(params->progress_interval),
    progresses(params->system->numContexts(), 0ULL),
    latency(params->latency),
    blockSize(params->system->cacheLineSize()),
    blockNum(params->system->memSize() / blockSize),
    bucketSize(params->bucket_size),   
    capacity(params->stash_size / blockSize),     
    queueCapacity(params->queue_size),
    utilization(params->utilization),
    memPort(params->name + ".mem_side", this),
    blocked(false), acState(AccessState::Idle), originalPacket(nullptr), waitingPortId(-1),
    currentBlkAddr(blockNum + 1)
{
    // Since the CPU side ports are a vector of ports, create an instance of
    // the CPUSidePort for each connection. This member of params is
    // automatically created depending on the name of the vector port and
    // holds the number of connections to this port name
    for (int i = 0; i < params->port_cpu_side_connection_count; ++i) {
        cpuPorts.emplace_back(name() + csprintf(".cpu_side[%d]", i), i, this);
    }

    // blockNum = params->system->memSize() / blockSize;
    DPRINTF(ShadowBlock, "memSize = %#lx\n", params->system->memSize());
    DPRINTF(ShadowBlock, "blockSize = %d\n", blockSize);
    uint64_t bucketNum = blockNum / bucketSize;	
    blockNum = bucketNum * bucketSize;
    maxLevel = log_binary(bucketNum) - 1;
    validBlockNum = blockNum * utilization;
    validMap.resize(blockNum, blockNum);

    AddrRangeList list = params->system->getPhysMem().getConfAddrRanges();
    for (auto &range: list)
    	DPRINTF(ShadowBlock, "Start: %#lx; End: %#lx\n", range.start(), range.end());

    posMapInit();

    DPRINTF(ShadowBlock, "Initialize Done:\n");
    DPRINTF(ShadowBlock, "utilization = %.2f\n", utilization);
    DPRINTF(ShadowBlock, "validBlockNum = %#lx\n", validBlockNum);
    DPRINTF(ShadowBlock, "maxLevel = %d\n", maxLevel);
    DPRINTF(ShadowBlock, "stash capacity = %d\n", capacity);
    DPRINTF(ShadowBlock, "latency = %d\n", latency);
}

static uint64_t generateRandomLeaf(uint64_t bucketAddr, unsigned maxLevel)
{
    uint64_t buckerAddrFix = bucketAddr + 1;
    unsigned level = log_binary(buckerAddrFix);
    unsigned levelDif = maxLevel - level;
    uint64_t base = buckerAddrFix << levelDif;
    uint64_t mask = rand() % (1 << levelDif);
    return (base | mask) - 1;
}

static uint64_t generateRandomLeaf(unsigned maxLevel)
{
    uint64_t base = 1 << maxLevel;
    uint64_t mask = rand() % (1 << maxLevel);
    return (base | mask) - 1;
}

void ShadowBlock::posMapInit()
{
    for (Addr i = 0; i < validBlockNum; i++)
    {
        posMap[i] = {generateRandomLeaf(i / bucketSize, maxLevel), i};
        validMap[i] = i;
    }
}

Port &
ShadowBlock::getPort(const std::string &if_name, PortID idx)
{
    // This is the name from the Python SimObject declaration in ShadowBlock.py
    if (if_name == "mem_side") {
        panic_if(idx != InvalidPortID,
                 "Mem side of simple cache not a vector port");
        return memPort;
    } else if (if_name == "cpu_side" && idx < cpuPorts.size()) {
        // We should have already created all of the ports in the constructor
        return cpuPorts[idx];
    } else {
        // pass it along to our super class
        return ClockedObject::getPort(if_name, idx);
    }
}

void
ShadowBlock::CPUSidePort::sendPacket(PacketPtr pkt)
{
    // Note: This flow control is very simple since the cache is blocking.

    panic_if(blockedPacket != nullptr, "Should never try to send if blocked! Blocked packet: %s ; packet: %s", blockedPacket->print(), pkt->print());

    // If we can't send the packet across the port, store it for later.
    // DPRINTF(ShadowBlock, "Sending %s to CPU\n", pkt->print());
    if (!sendTimingResp(pkt)) {
        DPRINTF(ShadowBlock, "failed!\n");
        blockedPacket = pkt;
    }
}

AddrRangeList
ShadowBlock::CPUSidePort::getAddrRanges() const
{
    return owner->getAddrRanges();
}

void
ShadowBlock::CPUSidePort::trySendRetry()
{
    // DPRINTF(ShadowBlock, "needRetry: %d; blockedPacket: %s\n", (int)needRetry, blockedPacket?blockedPacket->print():"NULL");
    if (needRetry && blockedPacket == nullptr) {
        // Only send a retry if the port is now completely free
        needRetry = false;
        // DPRINTF(ShadowBlock, "Sending retry req.\n");
        sendRetryReq();
    }
}

void
ShadowBlock::CPUSidePort::recvFunctional(PacketPtr pkt)
{
    // Just forward to the cache.
    return owner->handleFunctional(pkt);
}

bool
ShadowBlock::CPUSidePort::recvTimingReq(PacketPtr pkt)
{
    // DPRINTF(ShadowBlock, "Got request %s\n", pkt->print());

    if (blockedPacket || needRetry) {
        // The cache may not be able to send a reply if this is blocked
        // DPRINTF(ShadowBlock, "Request blocked\n");
        needRetry = true;
        return false;
    }
    // Just forward to the cache.
    if (!owner->handleRequest(pkt, id)) {
        // DPRINTF(ShadowBlock, "Request failed\n");
        // stalling
        needRetry = true;
        return false;
    } else {
        // DPRINTF(ShadowBlock, "Request succeeded\n");
        return true;
    }
}

void
ShadowBlock::CPUSidePort::recvRespRetry()
{
    // We should have a blocked packet if this function is called.
    assert(blockedPacket != nullptr);

    // Grab the blocked packet.
    PacketPtr pkt = blockedPacket;
    blockedPacket = nullptr;

    // DPRINTF(ShadowBlock, "Retrying response pkt %s\n", pkt->print());
    // Try to resend it. It's possible that it fails again.
    sendPacket(pkt);

    // We may now be able to accept new packets
    trySendRetry();
}

void
ShadowBlock::MemSidePort::sendPacket(PacketPtr pkt)
{
    // Note: This flow control is very simple since the cache is blocking.

    panic_if(blockedPacket != nullptr, "Should never try to send if blocked! Blocked packet: %s ; packet: %s", blockedPacket->print(), pkt->print());

    // If we can't send the packet across the port, store it for later.
    if (!sendTimingReq(pkt)) {
        blockedPacket = pkt;
    }
}

bool
ShadowBlock::MemSidePort::recvTimingResp(PacketPtr pkt)
{
    // Just forward to the cache.
    return owner->handleResponse(pkt);
}

void
ShadowBlock::MemSidePort::recvReqRetry()
{
    // We should have a blocked packet if this function is called.
    assert(blockedPacket != nullptr);

    // Grab the blocked packet.
    PacketPtr pkt = blockedPacket;
    blockedPacket = nullptr;

    // Try to resend it. It's possible that it fails again.
    sendPacket(pkt);
}

void
ShadowBlock::MemSidePort::recvRangeChange()
{
    owner->sendRangeChange();
}

void
ShadowBlock::updateWarmupState()
{
    for (auto &&ctx : system->threadContexts)
        warmup &= (ctx->getCurrentInstCount() <= warmupCnt);
}

void
ShadowBlock::updateProgress()
{
    static bool flag = false;
    if (!flag)
    {
        flag = true;
	    progresses.resize(system->numContexts());
    }
    assert(progresses.size() == system->numContexts());
    for (size_t i = 0; i < progresses.size(); i++)
    {
        uint64_t prog = system->threadContexts[i]->getCurrentInstCount() / interval;
        if (prog > progresses[i])
        {
            progresses[i] = prog;
            DPRINTF(InstProgress, "Instruction count for cpu[%d]: %lld\n", i, system->threadContexts[i]->getCurrentInstCount());
        }
    }
}

bool
ShadowBlock::handleRequest(PacketPtr pkt, int port_id)
{
    if (warmup) updateWarmupState();

    updateProgress();

    if (blocked && acState != AccessState::Idle) {
        // There is currently an outstanding request so we can't respond. Stall
        if (reqQueue.size() < queueCapacity)
        {
            reqTimes[pkt->getBlockAddr(blockSize)] = curTick();
            reqQueue.push({ pkt, port_id });
            return true;
        }
        return false;        
    }

    if (reqTimes.find(pkt->getBlockAddr(blockSize)) == reqTimes.cend())
        reqTimes[pkt->getBlockAddr(blockSize)] = curTick();

    // DPRINTF(ShadowBlock, "Got request for addr %#x\n", pkt->getAddr());

    // This cache is now blocked waiting for the response to this packet.
    blocked = true;
    acState = AccessState::ReadPath;

    // Store the port for when we get the response
    assert(waitingPortId == -1);
    waitingPortId = port_id;

    // Schedule an event after cache access latency to actually access
    schedule(new EventFunctionWrapper([this, pkt]{ accessTiming(pkt); },
                                      name() + ".accessEvent", true),
             clockEdge(latency));

    return true;
}

bool
ShadowBlock::handleResponse(PacketPtr pkt)
{
    assert(blocked && acState != AccessState::Idle);
    // DPRINTF(ShadowBlock, "Got response for addr %#x\n", pkt->getAddr());

    if (acState == AccessState::ReadPath)
    {
        // For now assume that inserts are off of the critical path and don't count
        // for any added latency.
        bool reqHit = validMap[pkt->getBlockAddr(blockSize) / blockSize] == currentBlkAddr;
        insert(pkt);

        if (reqHit)
        {
            currentBlkAddr = blockNum + 1;
            panic_if(originalPacket == nullptr, "Original packet is null\n");
            if (originalPacket != nullptr) {
                // DPRINTF(ShadowBlock, "Copying data from new packet to old\n");
                // We had to upgrade a previous packet. We can functionally deal with
                // the cache access now. It better be a hit.
                bool hit M5_VAR_USED = accessFunctional(originalPacket);
                panic_if(!hit, "Should always hit after inserting");
                if (originalPacket->needsResponse())
                    originalPacket->makeResponse();
                delete pkt; // We may need to delay this, I'm not sure.
                pkt = originalPacket;
                originalPacket = nullptr;
            } // else, pkt contains the data it needs
            
            auto it = reqTimes.find(pkt->getBlockAddr(blockSize));
            panic_if(it == reqTimes.cend(), "Not found in request arrival time map!");
            if (!warmup) reqLatency.sample(curTick() - it->second);
            reqTimes.erase(it);
            
            if (pkt->needsResponse() || pkt->isResponse())
                sendResponse(pkt);
            else
            {
                // DPRINTF(ShadowBlock, "Packet %s needn't response, deleted\n", pkt->print());
                waitingPortId = -1;
                delete pkt; // ?
            }
            if (!warmup) missLatency.sample(curTick() - missTime);
        }
        else
            delete pkt; // We may need to delay this, I'm not sure.

        if (!pathBufferQueue.empty()) {
            ReqQueueEntry next = pathBufferQueue.top();
            pathBufferQueue.pop();
            memPort.sendPacket(next.pkt);
            // DPRINTF(ShadowBlock, "Read next level\n");
        } else {
            panic_if(currentBlkAddr != blockNum + 1, "Current request not responsed!");
            acState = AccessState::WriteBack;
            startPathWrite();
        }
    }
    else if (acState == AccessState::WriteBack)
    {
        if (!pathBufferQueue.empty()) {
            delete pkt;
            ReqQueueEntry next = pathBufferQueue.top();
            pathBufferQueue.pop();
            memPort.sendPacket(next.pkt);
            // DPRINTF(ShadowBlock, "Write next level\n");
        } else {
            // Write back completed, start next request
            delete pkt;
            blocked = false;
            acState = AccessState::Idle;

            
            while(!evictQueue.empty())
            {
                auto entry = evictQueue.front();
                delete[] entry.second;
                evictQueue.pop();
            }

            // DPRINTF(ShadowBlock, "Completed path write, start next request\n");
            if (!reqQueue.empty())
            {
                ReqQueueEntry next = reqQueue.front();
                reqQueue.pop();
		        // DPRINTF(ShadowBlock, "Next resquest %s popped from queue\n", next.pkt->print());
                handleRequest(next.pkt, next.port_id);
            }
            // else
                // DPRINTF(ShadowBlock, "Request queue empty\n");
            // DPRINTF(ShadowBlock, "Try send retry\n");
            for (auto& port : cpuPorts) {
                port.trySendRetry();
            }      
        }
    }
    else
    {
        panic("Unknown access state!");
    }

    // missLatency.sample(curTick() - missTime);

    return true;
}

// Send Response to CPU side
void ShadowBlock::sendResponse(PacketPtr pkt)
{
    assert(blocked && acState == AccessState::ReadPath);
    assert(pkt->needsResponse() || pkt->isResponse());
    // DPRINTF(ShadowBlock, "Sending resp for addr %#x\n", pkt->getAddr());

    int port = waitingPortId;

    // The packet is now done. We're about to put it in the port, no need for
    // this object to continue to stall.
    // We need to free the resource before sending the packet in case the CPU
    // tries to send another request immediately (e.g., in the same callchain).
    // blocked = false;


    // acState = AccessState::WriteBack;
    waitingPortId = -1;

    // Simply forward to the memory port
    cpuPorts[port].sendPacket(pkt);

    // DPRINTF(ShadowBlock, "Completed path read, send response to port %d, start path write\n", port);

    // startPathWrite();

    // For each of the cpu ports, if it needs to send a retry, it should do it
    // now since this memory object may be unblocked now.
    // for (auto& port : cpuPorts) {
    //     port.trySendRetry();
    // }
    // if (!reqQueue.empty())
    // {
    //     ReqQueueEntry next = reqQueue.front();
    //     reqQueue.pop();
    //     handleRequest(next.pkt, next.port_id);
    // }
}

void
ShadowBlock::handleFunctional(PacketPtr pkt)
{
    if (accessFunctional(pkt)) {
        if (pkt->needsResponse())
            pkt->makeResponse();
    } else {
        Addr addr = pkt->getAddr(), blkAddr = pkt->getBlockAddr(blockSize);
        Addr physBlk = posMap[blkAddr / blockSize].blkAddr;
        pkt->setAddr(physBlk * blockSize + (addr - blkAddr));
        memPort.sendFunctional(pkt);
    }
}

void
ShadowBlock::accessTiming(PacketPtr pkt)
{
    bool hit = accessFunctional(pkt);

    DPRINTF(ShadowBlock, "%s for packet: %s\n", hit ? "Hit" : "Miss",
            pkt->print());

    if (hit) { // get the block from stash
        // Respond to the CPU side
        if (!warmup) hits++; // update stats
        DPRINTF(ShadowBlock, "Stash hit for packet: %s\n", pkt->print());
        // DDUMP(ShadowBlock, pkt->getConstPtr<uint8_t>(), pkt->getSize());
        if (pkt->needsResponse())
        {
            pkt->makeResponse();
            sendResponse(pkt);
        }
        else
        {
            // DPRINTF(ShadowBlock, "Packet %s needn't response, deleted\n", pkt->print());
            waitingPortId = -1;
            delete pkt; // ?
        }
        waitingPortId = -1;     
        blocked = false;
        acState = AccessState::Idle;
        if (!reqQueue.empty())
        {
            ReqQueueEntry next = reqQueue.front();
            reqQueue.pop();
            // DPRINTF(ShadowBlock, "Next resquest %s popped from queue\n", next.pkt->print());
            handleRequest(next.pkt, next.port_id);
        }
        else
            // DPRINTF(ShadowBlock, "Request queue empty\n");
        // DPRINTF(ShadowBlock, "Try send retry\n");
        for (auto& port : cpuPorts) {
            port.trySendRetry();
        }     
    } else {
        if (!warmup) misses++; // update stats
        missTime = curTick();
        // Forward to the memory side.
        // We can't directly forward the packet unless it is exactly the size
        // of the cache line, and aligned. Check for that here.
        Addr addr = pkt->getAddr();
        Addr block_addr = pkt->getBlockAddr(blockSize);
        unsigned size = pkt->getSize();

        // Save the old packet
        originalPacket = pkt;
        if (addr == block_addr && size == blockSize) {
            // Aligned and block size. We can just forward.
            // DPRINTF(ShadowBlock, "forwarding packet\n");
            // access(pkt); 
            startPathRead(pkt);           
        } else {
            // DPRINTF(ShadowBlock, "Upgrading packet to block size\n");
            panic_if(addr - block_addr + size > blockSize,
                     "Cannot handle accesses that span multiple cache lines");
            // Unaligned access to one cache block
            assert(pkt->needsResponse());
            MemCmd cmd;
            if (pkt->isWrite() || pkt->isRead()) {
                // Read the data from memory to write into the block.
                // We'll write the data in the cache (i.e., a writeback cache)
                cmd = MemCmd::ReadReq;
            } else {
                panic("Unknown packet type in upgrade size");
            }

            // Create a new packet that is blockSize
            PacketPtr new_pkt = new Packet(pkt->req, cmd, blockSize);
            new_pkt->allocate();

            // Should now be block aligned
            assert(new_pkt->getAddr() == new_pkt->getBlockAddr(blockSize));      

            // DPRINTF(ShadowBlock, "forwarding packet\n");

            // access(new_pkt);   
            startPathRead(new_pkt);     
        }
    }
}

bool
ShadowBlock::accessFunctional(PacketPtr pkt)
{
    Addr block_addr = pkt->getBlockAddr(blockSize);
    auto it = cacheStore.find(block_addr);
    if (it != cacheStore.end()) {
        if (pkt->isWrite() || pkt->isWriteback()) {
            // Write the data into the block in the cache
            pkt->writeDataToBlock(it->second, blockSize);
            stashWrites++;
        } else if (pkt->isRead()) {
            // Read the data out of the cache block into the packet
            pkt->setDataFromBlock(it->second, blockSize);
            stashReads++;
        } else {
            panic("Unknown packet type!");
        }
        return true;
    }
    return false;
}

void
ShadowBlock::insert(PacketPtr pkt)
{
    // The packet should be aligned.
    assert(pkt->getAddr() ==  pkt->getBlockAddr(blockSize));
    // The address should not be in the cache
    // assert(cacheStore.find(pkt->getAddr()) == cacheStore.end());
    // if (cacheStore.find(pkt->getAddr()) != cacheStore.end())
    //     return;
    // The pkt should be a response
    assert(pkt->isResponse());

    Addr addr = pkt->getAddr();
    if (validMap[addr / blockSize] != blockNum) {
        // panic_if(cacheStore.size() >= capacity, "Stash Overflow!");

        // Allocate space for the cache block data
        uint8_t *data = new uint8_t[blockSize];

        // Insert the data and address into the cache store
        cacheStore[validMap[addr / blockSize] * blockSize] = data;
        // Mark memory block as dummy
        validMap[addr / blockSize] = blockNum;
        stashWrites++;

        // Write the data into the cache
        pkt->writeDataToBlock(data, blockSize);
    }
}

AddrRangeList
ShadowBlock::getAddrRanges() const
{
    // DPRINTF(ShadowBlock, "Sending new ranges\n");
    // Just use the same ranges as whatever is on the memory side.
    return memPort.getAddrRanges();
}

void
ShadowBlock::sendRangeChange() const
{
    for (auto& port : cpuPorts) {
        port.sendRangeChange();
    }
}

void
ShadowBlock::regStats()
{
    // If you don't do this you get errors about uninitialized stats.
    ClockedObject::regStats();

    hits.name(name() + ".hits")
        .desc("Number of hits")
        ;

    misses.name(name() + ".misses")
        .desc("Number of misses")
        ;

    stashReads.name(name() + ".stashReads")
        .desc("Number of stash reads")
        ;

    stashWrites.name(name() + ".stashWrites")
        .desc("Number of stash writes")
        ;
    
    missLatency.name(name() + ".missLatency")
        .desc("Ticks for misses to the cache")
        .init(16) // number of buckets
        ;
    
    reqLatency.name(name() + ".reqLatency")
        .desc("Ticks from request arrival to request response")
        .init(16) // number of buckets
        ;

    hitRatio.name(name() + ".hitRatio")
        .desc("The ratio of hits to the total accesses to the cache")
        ;

    hitRatio = hits / (hits + misses);

}


ShadowBlock*
ShadowBlockParams::create()
{
    return new ShadowBlock(this);
}

// void ShadowBlock::access(PacketPtr pkt) 
void ShadowBlock::startPathRead(PacketPtr pkt) 
{
    Addr addr = pkt->getBlockAddr(blockSize);
    Addr blockAddr = addr / blockSize;
    // Step 1: Lookup Position map and remap
	// int64_t oldPosition	= posMap[blockAddr].leaf;
    currentLeaf = posMap[blockAddr].leaf;
    currentBlkAddr = addr / blockSize;
	int64_t newPosition = generateRandomLeaf(maxLevel);
    posMap[blockAddr].leaf = newPosition;
    // DPRINTF(ShadowBlock, "Remapping addr %#lx from %#lx to %#lx\n", addr, currentLeaf, newPosition);

    // Step 2: Preparing packets along the whole path in the path buffer queue
    uint64_t bucketIndex = currentLeaf;
	for (int height = maxLevel; height >= 0; height--)
    //while (true)
	{
        for (int bi = 0; bi < bucketSize; bi++) // bi: index inside block
        {
            // assert(bucketIndex || (bucketIndex == 0 && height == 0));
            // assert(bucketIndex * bucketSize < blockNum);
            // Create a new packet that is blockSize
            RequestPtr req = std::make_shared<Request>((bucketIndex * bucketSize + bi) * blockSize, blockSize, 0, 0);
            PacketPtr new_pkt = new Packet(req, MemCmd::ReadReq, blockSize);
            new_pkt->allocate();

            // new_pkt->setAddr((bucketIndex * bucketSize + bi) * blockSize);
            // new_pkt->setSize(sizeof(int64_t) * bucketSize); // bug here

            // Should now be block aligned
            // assert(new_pkt->getAddr() == new_pkt->getBlockAddr(blockSize));
            pathBufferQueue.push({new_pkt, -1});        
            // bucketIndex = (bucketIndex - 1) / 2;
        }
        if (bucketIndex == 0) break; // reach root, stop
        bucketIndex = (bucketIndex - 1) >> 1;
	}
    // DPRINTF(ShadowBlock, "Start accessing path %#lx for original addr %#lx\n", currentLeaf, originalPacket->getBlockAddr(blockSize));

    ReqQueueEntry next = pathBufferQueue.top();
    pathBufferQueue.pop();
    memPort.sendPacket(next.pkt);
}

std::pair<std::unordered_map<Addr, uint8_t*>::const_iterator, Addr> ShadowBlock::scanStashForEvict(unsigned curLevel, unsigned bi)
{
    assert(acState == AccessState::WriteBack);
    for (auto it = cacheStore.cbegin(); it != cacheStore.cend(); it++)
    {
        Addr addr = it->first;
        uint64_t blockAddr = addr / blockSize;
        uint64_t leaf = posMap[blockAddr].leaf;
        unsigned levelDif = maxLevel - curLevel;
        uint64_t bucketAddr = ((leaf + 1) >> levelDif) - 1;
        uint64_t bbAddr = bucketAddr * bucketSize + bi;
        // DPRINTF(ShadowBlock, "%#lx %d %#lx %#lx\n", leaf, levelDif, bucketAddr, bbAddr);
        if ((bucketAddr + 1 == (currentLeaf + 1) >> levelDif) && validMap[bbAddr] == blockNum)
            return {it, bbAddr};
    }
    return {cacheStore.cend(), 0ULL};
}

void ShadowBlock::startPathWrite()
{
    assert(pathBufferQueue.empty());
    uint64_t bucketIndex = currentLeaf;
    unsigned cnt = 0;
    unsigned shadows = 0;
    for (int height = maxLevel; height >= 0; height--)  
    //while(true)
	{
        for (int bi = 0; bi < bucketSize; bi++) // bi: index inside block
        {
            // assert(bucketIndex || (bucketIndex == 0 && height == 0));
            // assert(bucketIndex * bucketSize < blockNum);

            // scan the stash for write back
            auto ret = scanStashForEvict(height, bi);

            auto blockToEvict = ret.first;
            Addr bbAddr = ret.second;
            PacketPtr new_pkt = nullptr;

            if (blockToEvict != cacheStore.cend())
            {
                panic_if(bbAddr != bucketIndex * bucketSize + bi, "Inconsistent block addr %#lx and %#lx", bbAddr, bucketIndex * bucketSize + bi);
                RequestPtr req = std::make_shared<Request>(bbAddr * blockSize, blockSize, 0, 0);
                // new_pkt = new Packet(req, MemCmd::WritebackDirty, blockSize);
                validMap[bbAddr] = blockToEvict->first / blockSize;
                posMap[blockToEvict->first / blockSize].blkAddr = bbAddr;
                new_pkt = new Packet(req, MemCmd::WriteReq, blockSize);
                new_pkt->dataStatic(blockToEvict->second); // This will be deleted later
                cacheStore.erase(blockToEvict->first);
                evictQueue.push(*blockToEvict);
                stashWrites++;
                cnt++;
            }
            else
            {
                // shadow block
                if (!evictQueue.empty())
                {
                    Addr bbAddr = bucketIndex * bucketSize + bi;
                    auto entry = evictQueue.front();
                    evictQueue.pop();
                    RequestPtr req = std::make_shared<Request>(bbAddr * blockSize, blockSize, 0, 0);
                    validMap[bbAddr] = entry.first / blockSize;
                    validMap[posMap[entry.first / blockSize].blkAddr] = blockNum;
                    posMap[entry.first / blockSize].blkAddr = bbAddr;
                    new_pkt = new Packet(req, MemCmd::WriteReq, blockSize);
                    new_pkt->dataDynamic(entry.second);
                    shadows++;
                }
                else // dummy block
                {
                    RequestPtr req = std::make_shared<Request>((bucketIndex * bucketSize + bi) * blockSize, blockSize, 0, 0);
                    // Create a new packet that is blockSize
                    // new_pkt = new Packet(req, MemCmd::WritebackDirty, blockSize);
                    new_pkt = new Packet(req, MemCmd::WriteReq, blockSize);
                    new_pkt->allocate();
                }
            }

            // new_pkt->setSize(sizeof(int64_t) * bucketSize); // bug here

            // Should now be block aligned
            // assert(new_pkt->getAddr() == new_pkt->getBlockAddr(blockSize));
            pathBufferQueue.push({new_pkt, -1});        
            // bucketIndex = (bucketIndex - 1) / 2;
        }
        if (bucketIndex == 0) break;
        bucketIndex = (bucketIndex - 1) >> 1;
	}

    DPRINTF(ShadowBlock, "Start writing back to path %#lx, %d accesses in total\n", currentLeaf, pathBufferQueue.size());
    DPRINTF(ShadowBlock, "%d real blocks evicted, %d shadow blocks generated\n", cnt, shadows);
    DPRINTF(ShadowBlock, "Remaining stash blocks: %d\n", cacheStore.size());

    ReqQueueEntry next = pathBufferQueue.top();
    pathBufferQueue.pop();
    memPort.sendPacket(next.pkt);
}
