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

// #include "learning_gem5/path_oram/path_oram.hh"
#include "hitchhikerD.hh"

#include "base/random.hh"
#include "debug/HitchhikerD.hh"
#include "debug/InstProgress.hh"
#include "sim/system.hh"

unsigned HitchhikerD::log_binary(uint64_t num)
{
    assert(num != 0);
    for (unsigned i = 1; i < 64; i++)
        if (num >> i == 0)
            return i - 1;
    return -1; // error
}

HitchhikerD::HitchhikerD(HitchhikerDParams *params) :
    ClockedObject(params),
    system(params->system),
    warmupCnt(params->warmup_cnt),
    warmup(warmupCnt == 0 ? false : true),
    interval(params->progress_interval),
    progresses(params->system->numContexts(), 0ULL),
    latency(params->latency),
    blockSize(params->system->cacheLineSize()),
    bucketSize(params->bucket_size),   
    capacity(params->stash_size / blockSize),     
    queueCapacity(params->queue_size),
    utilization(params->utilization),
    memPort(params->name + ".mem_side", this),
    blocked(false), acState(AccessState::Idle), 
    originalPacket(nullptr), waitingPortId(-1), 
    ageThreshold(params->age_threshold), currentLevel(0), currentBucket(0), cachedReads(0), qOcc(0)
{
    // Since the CPU side ports are a vector of ports, create an instance of
    // the CPUSidePort for each connection. This member of params is
    // automatically created depending on the name of the vector port and
    // holds the number of connections to this port name
    for (int i = 0; i < params->port_cpu_side_connection_count; ++i) {
        cpuPorts.emplace_back(name() + csprintf(".cpu_side[%d]", i), i, this);
    }

    blockNum = params->system->memSize() / blockSize;
    uint64_t bucketNum = blockNum / bucketSize;	
    blockNum = bucketNum * bucketSize;
    maxLevel = log_binary(bucketNum) - 1;
    validBlockNum = blockNum * utilization;    

    AddrRangeList list = params->system->getPhysMem().getConfAddrRanges();
    for (auto &range: list)
    	DPRINTF(HitchhikerD, "Start: %#lx; End: %#lx\n", range.start(), range.end());

    posMapInit();

    DPRINTF(HitchhikerD, "Initialize Done:\n");
    DPRINTF(HitchhikerD, "validBlockNum = %d\n", validBlockNum);
    DPRINTF(HitchhikerD, "maxLevel = %d\n", maxLevel);
    DPRINTF(HitchhikerD, "stash capacity = %d\n", capacity);
    DPRINTF(HitchhikerD, "latency = %d\n", latency);  
}

HitchhikerD::PosMapEntry HitchhikerD::generateRandomLeaf(uint64_t bucketAddr)
{
    uint64_t buckerAddrFix = bucketAddr + 1;
    unsigned level = log_binary(buckerAddrFix);
    unsigned levelDif = maxLevel - level;
    uint64_t base = buckerAddrFix << levelDif;
    uint64_t mask = rand() % (1 << levelDif);
    return { (base|mask) - 1, level };
}

void HitchhikerD::posMapInit()
{
    for (Addr i = 0; i < validBlockNum - bucketSize; i++)
        posMap[i] = generateRandomLeaf(i / bucketSize);
}

bool HitchhikerD::isDescendant(const HitchhikerD::PosMapEntry &root, const HitchhikerD::PosMapEntry &node)
{
    if (node.level <= root.level) return false;
    uint64_t index1 = node.leaf, index2 = root.leaf;
    for (int height = maxLevel; height > root.level; height--)
    {
        index1 = (index1 - 1) >> 1;
        index2 = (index2 - 1) >> 1;
    }
    return index1 == index2;
}

Port &
HitchhikerD::getPort(const std::string &if_name, PortID idx)
{
    // This is the name from the Python SimObject declaration in HitchhikerD.py
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
HitchhikerD::CPUSidePort::sendPacket(PacketPtr pkt)
{
    // Note: This flow control is very simple since the cache is blocking.

    panic_if(blockedPacket != nullptr, "Should never try to send if blocked! Blocked packet: %s ; packet: %s", blockedPacket->print(), pkt->print());

    // If we can't send the packet across the port, store it for later.
    DPRINTF(HitchhikerD, "Sending %s to CPU\n", pkt->print());
    if (!sendTimingResp(pkt)) {
        DPRINTF(HitchhikerD, "failed!\n");
        blockedPacket = pkt;
    }
}

AddrRangeList
HitchhikerD::CPUSidePort::getAddrRanges() const
{
    return owner->getAddrRanges();
}

void
HitchhikerD::CPUSidePort::trySendRetry()
{
    DPRINTF(HitchhikerD, "needRetry: %d; blockedPacket: %s\n", (int)needRetry, blockedPacket?blockedPacket->print():"NULL");
    if (needRetry && blockedPacket == nullptr) {
        // Only send a retry if the port is now completely free
        needRetry = false;
        DPRINTF(HitchhikerD, "Sending retry req.\n");
        sendRetryReq();
    }
}

void
HitchhikerD::CPUSidePort::recvFunctional(PacketPtr pkt)
{
    // Just forward to the cache.
    return owner->handleFunctional(pkt);
}
bool
HitchhikerD::CPUSidePort::recvTimingReq(PacketPtr pkt)
{
    DPRINTF(HitchhikerD, "Got request %s\n", pkt->print());

    if (blockedPacket || needRetry) {
        // The cache may not be able to send a reply if this is blocked
        DPRINTF(HitchhikerD, "Request blocked\n");
        needRetry = true;
        return false;
    }
    // Just forward to the cache.
    if (!owner->handleRequest(pkt, id)) {
        DPRINTF(HitchhikerD, "Request failed\n");
        // stalling
        needRetry = true;
        return false;
    } else {
        DPRINTF(HitchhikerD, "Request succeeded\n");
        return true;
    }
}

void
HitchhikerD::CPUSidePort::recvRespRetry()
{
    // We should have a blocked packet if this function is called.
    assert(blockedPacket != nullptr);

    // Grab the blocked packet.
    PacketPtr pkt = blockedPacket;
    blockedPacket = nullptr;

    DPRINTF(HitchhikerD, "Retrying response pkt %s\n", pkt->print());
    // Try to resend it. It's possible that it fails again.
    sendPacket(pkt);

    // We may now be able to accept new packets
    trySendRetry();
}

void
HitchhikerD::MemSidePort::sendPacket(PacketPtr pkt)
{
    // Note: This flow control is very simple since the cache is blocking.

    panic_if(blockedPacket != nullptr, "Should never try to send if blocked! Blocked packet: %s ; packet: %s", blockedPacket->print(), pkt->print());

    // If we can't send the packet across the port, store it for later.
    if (!sendTimingReq(pkt)) {
        blockedPacket = pkt;
    }
}

bool
HitchhikerD::MemSidePort::recvTimingResp(PacketPtr pkt)
{
    // Just forward to the cache.
    return owner->handleResponse(pkt);
}

void
HitchhikerD::MemSidePort::recvReqRetry()
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
HitchhikerD::MemSidePort::recvRangeChange()
{
    owner->sendRangeChange();
}

void
HitchhikerD::updateWarmupState()
{
    for (auto &&ctx : system->threadContexts)
        warmup &= (ctx->getCurrentInstCount() <= warmupCnt);
}

void
HitchhikerD::updateProgress()
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
HitchhikerD::handleRequest(PacketPtr pkt, int port_id)
{
    if (warmup) updateWarmupState();

    updateProgress();

    DPRINTF(HitchhikerD, "Got request for addr %#x\n", pkt->getAddr());
    if (blocked && acState != AccessState::Idle) {
        // There is currently an outstanding request so we can't respond. Stall
        auto qSize = reqQueue.size();
        if (qSize < queueCapacity)
        {
            // reqQueue.push({ pkt, port_id });
            reqTimes[pkt->getBlockAddr(blockSize)] = curTick();
            PosMapEntry pos = posMap[pkt->getBlockAddr(blockSize) / blockSize];
            ReqQueueEntry entry = { pkt, port_id, pos, 0 };
            reqQueue.push_back(entry);
            if (!warmup && qSize + 1 > qOcc)
            {
                qOcc = qSize;
                queueOcc = qOcc;
            }
            DPRINTF(HitchhikerD, "Got request for addr %#x\n", pkt->getAddr());
            if (isDescendant(currentPath, pos)) 
            {
                DPRINTF(HitchhikerD, "Request feasible, added to schedule queue\n");
                schedQueue.push(entry);
            }
            return true;
        }
        return false;        
    }

    if (reqTimes.find(pkt->getBlockAddr(blockSize)) == reqTimes.cend())
        reqTimes[pkt->getBlockAddr(blockSize)] = curTick();

    // This cache is now blocked waiting for the response to this packet.
    blocked = true;
    acState = AccessState::ReadPath;
    cachedReads = 0;

    // Store the port for when we get the response
    assert(waitingPortId == -1);
    waitingPortId = port_id;

    // Schedule an event after cache access latency to actually access
    schedule(new EventFunctionWrapper([this, pkt]{ accessTiming(pkt); },
                                      name() + ".accessEvent", true),
             clockEdge(latency));

    return true;
}

void HitchhikerD::tryNextRequest()
{
    assert(blocked && acState != AccessState::Idle);
    blocked = false;
    acState = AccessState::Idle;
    if (!reqQueue.empty())
    {
        auto nextIt = reqQueue.begin();
        for (auto it = reqQueue.begin(); it != reqQueue.end(); it++)
        {
            if (++(it->age) == ageThreshold)
            {
                nextIt = it;
                break;
            }
            if (*it < *nextIt) nextIt = it;
        }
        ReqQueueEntry next = *nextIt;
        reqQueue.erase(nextIt);
        while (!schedQueue.empty()) schedQueue.pop();
        assert(schedQueue.empty());
        // schedQueue.push(next);
        for (auto it = reqQueue.cbegin(); it != reqQueue.cend(); it++)
        {
            if (isDescendant(next.posInfo, it->posInfo))
            {
                DPRINTF(HitchhikerD, "Request feasible, added to schedule queue\n");
                schedQueue.push(*it);
            }               
        }
        DPRINTF(HitchhikerD, "Next resquest %s popped from queue\n", next.pkt->print());
        handleRequest(next.pkt, next.port_id);
    }
    else
        DPRINTF(HitchhikerD, "Request queue empty\n");
    DPRINTF(HitchhikerD, "Try send retry\n");
    for (auto& port : cpuPorts) {
        port.trySendRetry();
    }
}

void HitchhikerD::tryPathDiversion()
{
    DPRINTF(HitchhikerD, "Start trying path diversion\n");
    while (!schedQueue.empty())
    {
        DPRINTF(HitchhikerD, "enter the loop\n");
        ReqQueueEntry req = schedQueue.top();
        schedQueue.pop();
        if (isDescendant(currentPath, req.posInfo))
        {
            originalPacket = req.pkt;
            waitingPortId = req.port_id;
            if (!warmup) diversions++;
            currentPath = req.posInfo;
            reqQueue.remove(req);
            DPRINTF(HitchhikerD, "Path diversion to path %#x\n", currentPath.leaf);
            return;
        }
    }
    DPRINTF(HitchhikerD, "Path diversion faile,%d\n",capacity-cacheStore.size());
    currentPath.level = maxLevel +1 ; // No diversion, to leaf directly
}

bool
HitchhikerD::handleResponse(PacketPtr pkt)
{
    assert(blocked && acState != AccessState::Idle);
    DPRINTF(HitchhikerD, "Got response for addr %#x\n", pkt->getAddr());

    // if (acState == AccessState::ReadPath)
    // {
    //     // For now assume that inserts are off of the critical path and don't count
    //     // for any added latency.
    //     insert(pkt);

    //     Addr blockAddr;
    //     bool reqHit = false;
    //     while (true)
    //     {
    //         if (++currentBucket == bucketSize)
    //         {
    //             if (currentLevel == maxLevel) break;
    //             ++currentLevel;
    //             currentBucket = 0;
    //         }
    //         uint64_t bucketIndex = currentLeaf;
    //         for (int height = maxLevel; height > currentLevel; height--)
    //             bucketIndex = (bucketIndex - 1) >> 1;
    //         blockAddr = (bucketIndex * bucketSize + currentBucket) * blockSize;
    //         if (cacheStore.find(blockAddr) == cacheStore.cend()) break;
    //         DPRINTF(HitchhikerD, "Block %#lx hit in stash, skipped\n", blockAddr);
    //         savedReads++;
    //     }

    //     if (!(currentLevel == maxLevel && currentBucket == bucketSize)) {
    //         delete pkt; // We may need to delay this, I'm not sure.            
    //         RequestPtr req = std::make_shared<Request>(blockAddr, blockSize, 0, 0);
    //         PacketPtr new_pkt = new Packet(req, MemCmd::ReadReq, blockSize);
    //         new_pkt->allocate();
    //         memPort.sendPacket(new_pkt);
    //         DPRINTF(HitchhikerD, "Start packet %s in level %d\n", new_pkt->print(), currentLevel);
    //     } else {
    //         // If we had to upgrade the request packet to a full cache line, now we
    //         // can use that packet to construct the response.
    //         if (originalPacket != nullptr) {
    //             DPRINTF(HitchhikerD, "Copying data from new packet to old\n");
    //             // We had to upgrade a previous packet. We can functionally deal with
    //             // the cache access now. It better be a hit.
    //             bool hit M5_VAR_USED = accessFunctional(originalPacket);
    //             panic_if(!hit, "Should always hit after inserting");
    //             if (originalPacket->needsResponse())
    //                 originalPacket->makeResponse();
    //             delete pkt; // We may need to delay this, I'm not sure.
    //             pkt = originalPacket;
    //             originalPacket = nullptr;
    //         } // else, pkt contains the data it needs
    //         if (pkt->needsResponse() || pkt->isResponse())     
    //             sendResponse(pkt);
    //         else
    //         {
    //             DPRINTF(HitchhikerD, "Packet %s needn't response, deleted\n", pkt->print());
    //             waitingPortId = -1;
    //             delete pkt; // ?
    //         }
    //         currentLevel = currentBucket = 0;
    //         if (capacity - cacheStore.size() < (maxLevel + 1) * bucketSize) // needs eviction
    //         {
    //             acState = AccessState::WriteBack;
    //             batchPathQueue.push(currentLeaf);
    //             startPathWrite();                
    //         }
    //         else
    //         {
    //             // savedWrites++;
    //             DPRINTF(HitchhikerD, "Skipped path write, start next request\n");
    //             batchPathQueue.push(currentLeaf);
    //             blocked = false;
    //             acState = AccessState::Idle;
    //             if (!reqQueue.empty())
    //             {
    //                 ReqQueueEntry next = reqQueue.front();
    //                 reqQueue.pop();
    //                 DPRINTF(HitchhikerD, "Next resquest %s popped from queue\n", next.pkt->print());
    //                 handleRequest(next.pkt, next.port_id);
    //             }
    //             else
    //                 DPRINTF(HitchhikerD, "Request queue empty\n");
    //             DPRINTF(HitchhikerD, "Try send retry\n");
    //             for (auto& port : cpuPorts) {
    //                 port.trySendRetry();
    //             }
    //         }
    //     }
    // }
    if (acState == AccessState::ReadPath)
    {
        // For now assume that inserts are off of the critical path and don't count
        // for any added latency.
        insert(pkt);

        Addr blockAddr;
        bool reqHit = false;
        while (true)
        {
            if (++currentBucket == bucketSize)
            {
                if (currentLevel == currentPath.level)
                {
                    // If we had to upgrade the request packet to a full cache line, now we
                    // can use that packet to construct the response.
                    if (originalPacket != nullptr) {
                        DPRINTF(HitchhikerD, "Copying data from new packet to old\n");
                        // We had to upgrade a previous packet. We can functionally deal with
                        // the cache access now. It better be a hit.
                        bool hit M5_VAR_USED = accessFunctional(originalPacket);
                        DPRINTF(HitchhikerD, "packet content of orginal: %#x\n",*(originalPacket->getPtr<uint64_t>()));
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

                    if (pkt->needsResponse() || pkt->isResponse()){     
                        DPRINTF(HitchhikerD, "this way1\n");
                        sendResponse(pkt);
                    }
                    else
                    {
                        DPRINTF(HitchhikerD, "Packet %s needn't response, deleted\n", pkt->print());
                        waitingPortId = -1;
                        delete pkt; // ?
                    }
                    if (!warmup) missLatency.sample(curTick() - missTime);
                    tryPathDiversion();
                    reqHit = true;
                }
                if (currentLevel == maxLevel) break;
                ++currentLevel;
                currentBucket = 0;
            }
            uint64_t bucketIndex = currentPath.leaf;
            for (int height = maxLevel; height > currentLevel; height--)
                bucketIndex = (bucketIndex - 1) >> 1;
            blockAddr = (bucketIndex * bucketSize + currentBucket) * blockSize;
            
            // stashReads++;
            stashScans++;
            if (cacheStore.find(blockAddr) == cacheStore.cend()) 
            {
                DPRINTF(HitchhikerD, "Block %#lx miss in stash, \n", blockAddr);
                break;
            }
            DPRINTF(HitchhikerD, "Block %#lx hit in stash, skipped.level in %d\n", blockAddr,currentLevel);
            // savedReads++;
            cachedReads++;
        }
        if (!(currentLevel == maxLevel && currentBucket == bucketSize)) {
            if (!reqHit) delete pkt; // We may need to delay this, I'm not sure.            
            RequestPtr req = std::make_shared<Request>(blockAddr, blockSize, 0, 0);
            PacketPtr new_pkt = new Packet(req, MemCmd::ReadReq, blockSize);
            new_pkt->allocate();
            memPort.sendPacket(new_pkt);
            DPRINTF(HitchhikerD, "start packet %s in level %d\n", new_pkt->print(), currentLevel);
        } else {
            currentLevel = currentBucket = 0;
            // schedQueue.clear();
            while (!schedQueue.empty()) schedQueue.pop();
            if (!warmup)
            {
                savedReads.sample(cachedReads);
                savedReadRate.sample(((double)cachedReads)/((maxLevel + 1)*bucketSize));
            }
	        cachedReads = 0;
            if (capacity - cacheStore.size() < (maxLevel + 1)*bucketSize) // needs eviction
            {
                acState = AccessState::WriteBack;
                batchPathQueue.push(currentPath.leaf);
                startPathWrite();                
            }
            else
            {
                // savedWrites++;
                DPRINTF(HitchhikerD, "Skipped path write, start next request\n");
                batchPathQueue.push(currentPath.leaf);
                tryNextRequest();
            }
        }
    }
    else if (acState == AccessState::WriteBack)
    {
        if (!evictSet.empty()) {
            delete pkt;
            EvictSetEntry next = *evictSet.cbegin();
            evictSet.erase(evictSet.cbegin());
            memPort.sendPacket(next.pkt);
            DPRINTF(HitchhikerD, "sent packet to memory: %#lx,%s\n",*(uint64_t*)(next.pkt->getPtr<uint8_t>()+24),next.pkt->print());
            // DPRINTF(HitchhikerD, "Write next level\n");
        } else {
            // Write back completed, start next request
            delete pkt;
            DPRINTF(HitchhikerD, "Completed path write, start next request\n");
            tryNextRequest();
            // blocked = false;
            // acState = AccessState::Idle;
            // DPRINTF(HitchhikerD, "Completed path write, start next request\n");
            // if (!reqQueue.empty())
            // {
            //     ReqQueueEntry next = reqQueue.front();
            //     reqQueue.pop();
		    //     DPRINTF(HitchhikerD, "Next resquest %s popped from queue\n", next.pkt->print());
            //     handleRequest(next.pkt, next.port_id);
            // }
            // else
            //     DPRINTF(HitchhikerD, "Request queue empty\n");
            // DPRINTF(HitchhikerD, "Try send retry\n");
            // for (auto& port : cpuPorts) {
            //     port.trySendRetry();
            // }      
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
void HitchhikerD::sendResponse(PacketPtr pkt)
{
    assert(blocked && acState == AccessState::ReadPath);
    assert(pkt->needsResponse() || pkt->isResponse());
    DPRINTF(HitchhikerD, "Sending resp for addr %#x\n", pkt->getAddr());

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

    DPRINTF(HitchhikerD, "Completed path read, send response to port %d, start path write\n", port);

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
HitchhikerD::handleFunctional(PacketPtr pkt)
{
    if (accessFunctional(pkt)) {
        if (pkt->needsResponse())
            pkt->makeResponse();
    } else {
        memPort.sendFunctional(pkt);
    }
}

void
HitchhikerD::accessTiming(PacketPtr pkt)
{
	if (!warmup) oramRequests++;
    DPRINTF(HitchhikerD, "packet content: %#x\n",*(pkt->getPtr<uint64_t>()));
    assert(acState == AccessState::ReadPath);

    bool hit = accessFunctional(pkt);

    DPRINTF(HitchhikerD, "%s for packet: %s\n", hit ? "Hit" : "Miss",
            pkt->print());

    if (hit) { // get the block from stash
        // Respond to the CPU side
        if (!warmup) hits++; // update stats
        DPRINTF(HitchhikerD, "Stash hit for packet: %s,%s\n", pkt->print(),pkt->isWrite()?"write":"read");
        // DDUMP(HitchhikerD, pkt->getConstPtr<uint8_t>(), pkt->getSize());
        if (pkt->needsResponse())
        {
            pkt->makeResponse();
            sendResponse(pkt);
        }
        else
        {
            DPRINTF(HitchhikerD, "Packet %s needn't response, deleted\n", pkt->print());
            waitingPortId = -1;
            delete pkt; // ?
        }
        waitingPortId = -1;
        DPRINTF(HitchhikerD, "Stash hit, start next request\n");
        tryNextRequest();
        // blocked = false;
        // acState = AccessState::Idle;
        // if (!reqQueue.empty())
        // {
        //     ReqQueueEntry next = reqQueue.front();
        //     reqQueue.pop();
        //     DPRINTF(HitchhikerD, "Next resquest %s popped from queue\n", next.pkt->print());
        //     handleRequest(next.pkt, next.port_id);
        // }
        // else
        //     DPRINTF(HitchhikerD, "Request queue empty\n");
        // DPRINTF(HitchhikerD, "Try send retry\n");
        // for (auto& port : cpuPorts) {
        //     port.trySendRetry();
        // }        
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
            DPRINTF(HitchhikerD, "forwarding packet\n");
            // access(pkt); 
            startPathRead(pkt);           
        } else {
            DPRINTF(HitchhikerD, "Upgrading packet to block size\n");
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

            DPRINTF(HitchhikerD, "forwarding packet\n");

            // access(new_pkt);   
            startPathRead(new_pkt);     
        }
    }
}

bool
HitchhikerD::accessFunctional(PacketPtr pkt)
{
    Addr block_addr = pkt->getBlockAddr(blockSize);
    auto it = cacheStore.find(block_addr);
    if (it != cacheStore.end()) {
        if (pkt->isWrite() || pkt->isWriteback()) {
            // Write the data into the block in the cache
            pkt->writeDataToBlock(it->second, blockSize);
            stashWrites++;
            DPRINTF(HitchhikerD, "packet content111: %#lx,address: %#lx\n",*(uint64_t*)(it->second+40),it->first);
            DPRINTF(HitchhikerD, "packet content222: %#lx,address: %#lx\n",*(pkt->getPtr<uint64_t>()),block_addr);
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
HitchhikerD::insert(PacketPtr pkt)
{
    // The packet should be aligned.
    assert(pkt->getAddr() ==  pkt->getBlockAddr(blockSize));
    // The address should not be in the cache
    // assert(cacheStore.find(pkt->getAddr()) == cacheStore.end());
    if (cacheStore.find(pkt->getAddr()) != cacheStore.end())
        return;
    // The pkt should be a response
    assert(pkt->isResponse());

    // if (cacheStore.size() >= capacity) {
        // Select random thing to evict. This is a little convoluted since we
        // are using a std::unordered_map. See http://bit.ly/2hrnLP2
        // int bucket, bucket_size;
        // do {
        //     bucket = random_mt.random(0, (int)cacheStore.bucket_count() - 1);
        // } while ( (bucket_size = cacheStore.bucket_size(bucket)) == 0 );
        // auto block = std::next(cacheStore.begin(bucket),
        //                        random_mt.random(0, bucket_size - 1));

        // DPRINTF(HitchhikerD, "Removing addr %#x\n", block->first);

        // // Write back the data.
        // // Create a new request-packet pair
        // RequestPtr req = std::make_shared<Request>(
        //     block->first, blockSize, 0, 0);

        // PacketPtr new_pkt = new Packet(req, MemCmd::WritebackDirty, blockSize);
        // new_pkt->dataDynamic(block->second); // This will be deleted later

        // DPRINTF(HitchhikerD, "Writing packet back %s\n", pkt->print());
        // // Send the write to memory
        // memPort.sendPacket(new_pkt);

        // // Delete this entry
        // cacheStore.erase(block->first);        
    // }
    Addr addr = pkt->getAddr();
    if (addr <= validBlockNum * blockSize) {
        panic_if(cacheStore.size() >= capacity, "Stash Overflow!");

        // DPRINTF(HitchhikerD, "Inserting %s\n", pkt->print());
        // DDUMP(HitchhikerD, pkt->getConstPtr<uint8_t>(), blockSize);

        // Allocate space for the cache block data
        uint8_t *data = new uint8_t[blockSize];

        // Insert the data and address into the cache store
        cacheStore[addr] = data;

        // Write the data into the cache
        pkt->writeDataToBlock(data, blockSize);
        stashWrites++;
        //DPRINTF(HitchhikerD, "insert the packet: %#x,%#lx\n",*(pkt->getPtr<uint64_t>()),addr);
        //DPRINTF(HitchhikerD, "insert the packet: %#x,%#lx\n",*(uint64_t*)(data+40),addr);
    }
}

AddrRangeList
HitchhikerD::getAddrRanges() const
{
    DPRINTF(HitchhikerD, "Sending new ranges\n");
    // Just use the same ranges as whatever is on the memory side.
    return memPort.getAddrRanges();
}

void
HitchhikerD::sendRangeChange() const
{
    for (auto& port : cpuPorts) {
        port.sendRangeChange();
    }
}

void
HitchhikerD::regStats()
{
    // If you don't do this you get errors about uninitialized stats.
    ClockedObject::regStats();

	oramRequests.name(name() + ".oramRequests")
				.desc("Number of ORAM Requests")
				;
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
    
    stashScans.name(name() + ".stashScans")
        .desc("Number of stash scans")
        ;

    savedWrites.name(name() + ".savedWrites")
        .desc("Number of block writes saved by batch eviction")
        .init(4)
        ;

    savedWriteRate.name(name() + ".savedWriteRate")
        .desc("The ratio of block writes saved by batch eviction to the total writes in a batch")
        .init(2)
        ;

    savedReads.name(name() + ".savedReads")
        .desc("Number of block reads saved by cached effect")
        .init(4)
	;

    savedReadRate.name(name() + ".savedReadRate")
        .desc("The ratio of block reads saved by cached effect to the total reads in a path")
        .init(2)
        ;

    queueOcc.name(name() + ".queueOcc")
        .desc("Maximum request queue occupation")
        ;
    
    diversions.name(name() + ".diversions")
        .desc("Number of path diversion schedulings made")
        ;

    batchPaths.name(name() + ".batchPaths")
        .desc("Number of paths in a batch set")
        .init(4)
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


HitchhikerD*
HitchhikerDParams::create()
{
    return new HitchhikerD(this);
}

// void HitchhikerD::access(PacketPtr pkt) 
void HitchhikerD::startPathRead(PacketPtr pkt) 
{
    assert(currentLevel == 0 && currentBucket == 0);
    Addr addr = pkt->getBlockAddr(blockSize);
    Addr blockAddr = addr / blockSize;
    // Step 1: Lookup Position map and remap
	// int64_t oldPosition	= posMap[blockAddr].leaf;
    currentPath = posMap[blockAddr];
	int64_t newPosition = generateRandomLeaf(blockAddr / bucketSize).leaf;
    posMap[blockAddr].leaf = newPosition;
    DPRINTF(HitchhikerD, "Remapping addr %#lx from %#lx to %#lx\n", addr, currentPath.leaf, newPosition);

    // Step 2: Preparing packets along the whole path in the path buffer queue
    // uint64_t bucketIndex = currentLeaf;
	// for (int height = maxLevel; height >= 0; height--)
    // while (true)
	// {
    //     for (int bi = 0; bi < bucketSize; bi++) // bi: index inside block
    //     {
    //         // assert(bucketIndex || (bucketIndex == 0 && height == 0));
    //         // assert(bucketIndex * bucketSize < blockNum);
    //         // Create a new packet that is blockSize
    //         RequestPtr req = std::make_shared<Request>((bucketIndex * bucketSize + bi) * blockSize, blockSize, 0, 0);
    //         PacketPtr new_pkt = new Packet(req, MemCmd::ReadReq, blockSize);
    //         new_pkt->allocate();

    //         // new_pkt->setAddr((bucketIndex * bucketSize + bi) * blockSize);
    //         // new_pkt->setSize(sizeof(int64_t) * bucketSize); // bug here

    //         // Should now be block aligned
    //         // assert(new_pkt->getAddr() == new_pkt->getBlockAddr(blockSize));
    //         pathBufferQueue.push({new_pkt, -1});        
    //         // bucketIndex = (bucketIndex - 1) / 2;
    //     }
    //     if (bucketIndex == 0) break; // reach root, stop
    //     bucketIndex = (bucketIndex - 1) >> 1;
	// }
    DPRINTF(HitchhikerD, "Start accessing path %#lx for original addr %#lx\n", currentPath.leaf, originalPacket->getBlockAddr(blockSize));

    // RequestPtr req = std::make_shared<Request>(0, blockSize, 0, 0);
    while (true)
    {
        uint64_t bucketIndex = currentPath.leaf;
        for (int height = maxLevel; height > currentLevel; height--)
            bucketIndex = (bucketIndex - 1) >> 1;
        blockAddr = (bucketIndex * bucketSize + currentBucket) * blockSize;
        // stashReads++;
        stashScans++;
        if (cacheStore.find(blockAddr) == cacheStore.cend()) break;
        DPRINTF(HitchhikerD, "Block %#lx hit in stash, skipped\n", blockAddr);
        cachedReads++;
        if (++currentBucket == bucketSize)
        {
            if (currentLevel == maxLevel) break;
            ++currentLevel;
            currentBucket = 0;
        }
    }

    RequestPtr req = std::make_shared<Request>(blockAddr, blockSize, 0, 0);
    PacketPtr new_pkt = new Packet(req, MemCmd::ReadReq, blockSize);
    new_pkt->allocate();

    memPort.sendPacket(new_pkt);
    DPRINTF(HitchhikerD, "Start packet %s in level %d\n", new_pkt->print(), currentLevel);
}

std::unordered_map<Addr, uint8_t*>::const_iterator HitchhikerD::scanStashForEvict(uint64_t evictLeaf, unsigned height)
{
    assert(acState == AccessState::WriteBack);
    for (auto it = cacheStore.cbegin(); it != cacheStore.cend(); it++)
    {
        Addr addr = it->first;
        uint64_t blockAddr = addr / blockSize;
        uint64_t leaf = posMap[blockAddr].leaf;
        uint64_t bucketAddr = blockAddr / bucketSize;
        unsigned levelDif = maxLevel - height;
        if (((leaf + 1) >> levelDif == (evictLeaf + 1) >> levelDif) && ((leaf + 1) >> levelDif == bucketAddr + 1))
            return it;
    }
    return cacheStore.cend();
}

void HitchhikerD::startPathWrite()
{
    // assert(pathBufferQueue.empty());
    assert(!batchPathQueue.empty());
    assert(evictSet.empty());
    unsigned batchPathCount = batchPathQueue.size();
    while (!batchPathQueue.empty())
    {
        uint64_t leaf = batchPathQueue.front();
        batchPathQueue.pop();
        uint64_t bucketIndex = leaf;
        for (int height = maxLevel; height >= 0; height--)  
        {
            for (int bi = 0; bi < bucketSize; bi++) // bi: index inside block
            {
                // assert(bucketIndex || (bucketIndex == 0 && height == 0));
                // assert(bucketIndex * bucketSize < blockNum);

                // scan the stash for write back
                auto blockToEvict = scanStashForEvict(leaf, height);
                PacketPtr new_pkt = nullptr;

                if (blockToEvict != cacheStore.cend())
                {
                    RequestPtr req = std::make_shared<Request>(blockToEvict->first, blockSize, 0, 0);
                    // new_pkt = new Packet(req, MemCmd::WritebackDirty, blockSize);
                    new_pkt = new Packet(req, MemCmd::WriteReq, blockSize);
                    new_pkt->dataDynamic(blockToEvict->second); // This will be deleted later
                    cacheStore.erase(blockToEvict->first);
                    stashWrites++;
                    DPRINTF(HitchhikerD, "insert address : %s to evictset,%d,%d\n",new_pkt->print(),(unsigned)height,evictSet.size());
                    evictSet.insert({new_pkt, (unsigned)height});
                }
                else // dummy block
                {
                    bool isFound = false;
                    for(auto it = evictSet.begin();it != evictSet.end();it++){
                        if(it->pkt->getAddr() == (bucketIndex * bucketSize + bi) * blockSize){
                            isFound=true;
                            break;
                        }
                    }
                    if(!isFound){
                        RequestPtr req = std::make_shared<Request>((bucketIndex * bucketSize + bi) * blockSize, blockSize, 0, 0);
                        // Create a new packet that is blockSize
                        // new_pkt = new Packet(req, MemCmd::WritebackDirty, blockSize);
                        new_pkt = new Packet(req, MemCmd::WriteReq, blockSize);
                        new_pkt->allocate();
                        evictSet.insert({new_pkt, (unsigned)height});
                        DPRINTF(HitchhikerD, "insert address(dummy) : %s to evictset,%d,%d\n",new_pkt->print(),(unsigned)height,evictSet.size());
                    }
                }

                // new_pkt->setSize(sizeof(int64_t) * bucketSize); // bug here

                // Should now be block aligned
                // assert(new_pkt->getAddr() == new_pkt->getBlockAddr(blockSize));
                // pathBufferQueue.push({new_pkt, -1});   

                
                
                //if (!evictSet.insert({new_pkt/*, (unsigned)height*/}).second)
                //    delete new_pkt;
                // bucketIndex = (bucketIndex - 1) / 2;
            }
            if (bucketIndex == 0) break;
            bucketIndex = (bucketIndex - 1) >> 1;
        }
    }

    DPRINTF(HitchhikerD, "Start writing back, %d paths, %d accesses in total, stash size remaining %d\n", batchPathCount, evictSet.size(), cacheStore.size());
    // savedWrites += (maxLevel + 1) * bucketSize - evictSet.size();
    if (!warmup)
    {
        batchPaths.sample(batchPathCount);
        unsigned sw = (maxLevel + 1) * bucketSize * batchPathCount - evictSet.size();
        savedWrites.sample(sw);
        savedWriteRate.sample(((double)sw)/((maxLevel + 1)*bucketSize*batchPathCount));
    }

    EvictSetEntry next = *evictSet.cbegin();
    evictSet.erase(evictSet.cbegin());
    DPRINTF(HitchhikerD, "sent packet to memory: %#lx,%s\n",*(uint64_t*)(next.pkt->getPtr<uint8_t>()+24),next.pkt->print());
    memPort.sendPacket(next.pkt);
}
