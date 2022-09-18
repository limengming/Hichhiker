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

// #include "learning_gem5/rho_oram/rho_oram.hh"
#include "hitchhikerB.hh"

#include "base/random.hh"
#include "debug/HitchhikerB.hh"
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

// static unsigned get_level(Addr addr, uint64_t blockSize, unsigned bucketSize)
// {
//     uint64_t bucketNum = addr / blockSize / bucketSize;
//     unsigned level = 0;
//     while (bucketNum)
//     {
//         bucketNum = (bucketNum - 1) >> 1;
//         level++;
//     }
//     return level;
// }

HitchhikerB::HitchhikerB(HitchhikerBParams *params) :
    ClockedObject(params),
    system(params->system),
    warmupCnt(params->warmup_cnt),
    warmup(warmupCnt == 0 ? false : true),
    interval(params->progress_interval),
    progresses(params->system->numContexts(), 0ULL),
    latency(params->latency),
    blockSize(params->system->cacheLineSize()),
    bucketSize(params->bucket_size),
    rhoBktSize(params->rho_bktsize),
    capacity(params->stash_size / blockSize),     
    rhoStashCapty(params->rho_stash_size / blockSize),
    queueCapacity(params->queue_size),
    utilization(params->utilization),
    memPort(params->name + ".mem_side", this),
    rhoPort(params->name + ".rho_side", this),
    blocked(false), acState(AccessState::Idle), originalPacket(nullptr), waitingPortId(-1)
{
    // Since the CPU side ports are a vector of ports, create an instance of
    // the CPUSidePort for each connection. This member of params is
    // automatically created depending on the name of the vector port and
    // holds the number of connections to this port name
    for (int i = 0; i < params->port_cpu_side_connection_count; ++i) {
        cpuPorts.emplace_back(name() + csprintf(".cpu_side[%d]", i), i, this);
    }

    Addr memSize = params->system->getPhysMem().getConfAddrRanges().front().size();
    DPRINTF(HitchhikerB, "Memory size: %#lx\n", memSize);
    rhoBase = memSize;
    blockNum = memSize / blockSize;
    uint64_t bucketNum = blockNum / bucketSize;	
    blockNum = bucketNum * bucketSize;
    maxLevel = log_binary(bucketNum) - 1;
    validBlockNum = blockNum * utilization;

    Addr rhoSize = params->tag_size * 10 / params->rho_util;
    rhoBlkNum = rhoSize / blockSize;
    uint64_t rhoBktNum = rhoBlkNum / rhoBktSize;
    rhoLevel = log_binary(rhoBktNum) - 1;
    rhoCapty = rhoBlkNum * params->rho_util;
    DPRINTF(HitchhikerB, "Rho size: %#lx, utilization: %.2f, level: %d, capacity: %d\n", rhoSize, params->rho_util, rhoLevel, rhoCapty);

    // AddrRangeList list = params->system->getPhysMem().getConfAddrRanges();
    // for (auto &range: list)
    // 	DPRINTF(HitchhikerB, "Start: %#lx; End: %#lx\n", range.start(), range.end());

    posMapInit();
    rhoTagInit();

    DPRINTF(HitchhikerB, "Initialize Done:\n");
    DPRINTF(HitchhikerB, "validBlockNum = %d\n", validBlockNum);
    DPRINTF(HitchhikerB, "maxLevel = %d\n", maxLevel);
    DPRINTF(HitchhikerB, "stash capacity = %d\n", capacity);
    DPRINTF(HitchhikerB, "latency = %d\n", latency);
}

static HitchhikerB::PosMapEntry generateRandomLeaf(uint64_t bucketAddr, unsigned maxLevel)
{
    uint64_t buckerAddrFix = bucketAddr + 1;
    unsigned level = log_binary(buckerAddrFix);
    unsigned levelDif = maxLevel - level;
    uint64_t base = buckerAddrFix << levelDif;
    uint64_t mask = rand() % (1 << levelDif);
    return { (base|mask) - 1 };
}

void HitchhikerB::rhoTagInit()
{
    for (unsigned i = 0; i < rhoCapty; i++)
        freeRhoBlocks.push_back(i * blockSize);
}

void HitchhikerB::posMapInit()
{
    for (Addr i = 0; i < validBlockNum - bucketSize; i++)
        posMap[i] = generateRandomLeaf(i / bucketSize, maxLevel);
}

Port &
HitchhikerB::getPort(const std::string &if_name, PortID idx)
{
    // This is the name from the Python SimObject declaration in HitchhikerB.py
    if (if_name == "mem_side") {
        panic_if(idx != InvalidPortID,
                 "Mem side of simple cache not a vector port");
        return memPort;
    } else if (if_name == "cpu_side" && idx < cpuPorts.size()) {
        // We should have already created all of the ports in the constructor
        return cpuPorts[idx];
    } else if (if_name == "rho_side") {
        panic_if(idx != InvalidPortID,
                 "Mem side of simple cache not a vector port");
        return rhoPort;
    } else {
        // pass it along to our super class
        return ClockedObject::getPort(if_name, idx);
    }
}

void
HitchhikerB::CPUSidePort::sendPacket(PacketPtr pkt)
{
    // Note: This flow control is very simple since the cache is blocking.

    panic_if(blockedPacket != nullptr, "Should never try to send if blocked! Blocked packet: %s ; packet: %s", blockedPacket->print(), pkt->print());

    // If we can't send the packet across the port, store it for later.
    DPRINTF(HitchhikerB, "Sending %s to CPU\n", pkt->print());
    if (!sendTimingResp(pkt)) {
        DPRINTF(HitchhikerB, "failed!\n");
        blockedPacket = pkt;
    }
}

AddrRangeList
HitchhikerB::CPUSidePort::getAddrRanges() const
{
    return owner->getAddrRanges();
}

void
HitchhikerB::CPUSidePort::trySendRetry()
{
    DPRINTF(HitchhikerB, "needRetry: %d; blockedPacket: %s\n", (int)needRetry, blockedPacket?blockedPacket->print():"NULL");
    if (needRetry && blockedPacket == nullptr) {
        // Only send a retry if the port is now completely free
        needRetry = false;
        DPRINTF(HitchhikerB, "Sending retry req.\n");
        sendRetryReq();
    }
}

void
HitchhikerB::CPUSidePort::recvFunctional(PacketPtr pkt)
{
    // Just forward to the cache.
    return owner->handleFunctional(pkt);
}

bool
HitchhikerB::CPUSidePort::recvTimingReq(PacketPtr pkt)
{
    DPRINTF(HitchhikerB, "Got request %s\n", pkt->print());

    if (blockedPacket || needRetry) {
        // The cache may not be able to send a reply if this is blocked
        DPRINTF(HitchhikerB, "Request blocked\n");
        needRetry = true;
        return false;
    }
    // Just forward to the cache.
    if (!owner->handleRequest(pkt, id)) {
        DPRINTF(HitchhikerB, "Request failed\n");
        // stalling
        needRetry = true;
        return false;
    } else {
        DPRINTF(HitchhikerB, "Request succeeded\n");
        return true;
    }
}

void
HitchhikerB::CPUSidePort::recvRespRetry()
{
    // We should have a blocked packet if this function is called.
    assert(blockedPacket != nullptr);

    // Grab the blocked packet.
    PacketPtr pkt = blockedPacket;
    blockedPacket = nullptr;

    DPRINTF(HitchhikerB, "Retrying response pkt %s\n", pkt->print());
    // Try to resend it. It's possible that it fails again.
    sendPacket(pkt);

    // We may now be able to accept new packets
    trySendRetry();
}

void
HitchhikerB::MemSidePort::sendPacket(PacketPtr pkt)
{
    // Note: This flow control is very simple since the cache is blocking.

    panic_if(blockedPacket != nullptr, "Should never try to send if blocked! Blocked packet: %s ; packet: %s", blockedPacket->print(), pkt->print());

    // If we can't send the packet across the port, store it for later.
    if (!sendTimingReq(pkt)) {
        blockedPacket = pkt;
    }
}

bool
HitchhikerB::MemSidePort::recvTimingResp(PacketPtr pkt)
{
    // Just forward to the cache.
    return owner->handleResponse(pkt);
}

void
HitchhikerB::MemSidePort::recvReqRetry()
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
HitchhikerB::MemSidePort::recvRangeChange()
{
    owner->sendRangeChange();
}

void
HitchhikerB::updateWarmupState()
{
    for (auto &&ctx : system->threadContexts)
        warmup &= (ctx->getCurrentInstCount() <= warmupCnt);
}

void
HitchhikerB::updateProgress()
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
HitchhikerB::handleRequest(PacketPtr pkt, int port_id)
{
    if (warmup) updateWarmupState();

    updateProgress();

    if (blocked && acState != AccessState::Idle) {
        // There is currently an outstanding request so we can't respond. Stall
        if (reqQueue.size() < queueCapacity)
        {
            reqTimes[pkt->getBlockAddr(blockSize)] = curTick();
            DPRINTF(HitchhikerB, "Request for addr %#x added into queue\n", pkt->getAddr());
            reqQueue.push({ pkt, port_id });
            return true;
        }
        return false;        
    }

    if (reqTimes.find(pkt->getBlockAddr(blockSize)) == reqTimes.cend())
        reqTimes[pkt->getBlockAddr(blockSize)] = curTick();

    DPRINTF(HitchhikerB, "Handling request for addr %#x\n", pkt->getAddr());

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
HitchhikerB::handleResponse(PacketPtr pkt)
{
    assert(blocked && acState != AccessState::Idle);
    // DPRINTF(HitchhikerB, "Got response for addr %#x\n", pkt->getAddr());

    if (acState == AccessState::ReadPath)
    {
        // For now assume that inserts are off of the critical path and don't count
        // for any added latency.
        insert(pkt);

        if (!pathBufferQueue.empty()) {
            delete pkt; // We may need to delay this, I'm not sure.
            // ReqQueueEntry next = pathBufferQueue.front();
            PacketPtr next = pathBufferQueue.front();
            pathBufferQueue.pop();
            // memPort.sendPacket(next.pkt);
            memPort.sendPacket(next);
            // DPRINTF(HitchhikerB, "Read next level\n");
        } else {/*
            // Record request latency
            auto it = reqTimes.find(pkt->getBlockAddr(blockSize));
            panic_if(it == reqTimes.cend(), "Not found in request arrival time map!");
            reqLatency.sample(curTick() - it->second);
            reqTimes.erase(it);*/

            // If we had to upgrade the request packet to a full cache line, now we
            // can use that packet to construct the response.
            if (originalPacket != nullptr) {
                DPRINTF(HitchhikerB, "Copying data from new packet to old\n");
                // We had to upgrade a previous packet. We can functionally deal with
                // the cache access now. It better be a hit.
                bool hit M5_VAR_USED = accessFunctional(originalPacket);
                panic_if(!hit, "Should always hit after inserting, pkt: %s", originalPacket->print());
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
                DPRINTF(HitchhikerB, "Packet %s needn't response, deleted\n", pkt->print());
                waitingPortId = -1;
                delete pkt; // ?
            }
            if (!warmup) missLatency.sample(curTick() - missTime);
            if (capacity - cacheStore.size() < (maxLevel + 1)*bucketSize)
            {
                acState = AccessState::WriteBack;
                startPathWrite();
            }
            else
            {
                DPRINTF(HitchhikerB, "Skipped path write, start next request\n");
                tryNextRequest();
            }
        }
    }
    else if (acState == AccessState::RhoEvictInvalid)
    {
        insertRhoStash(pkt);
        if (!pathBufferQueue.empty())
        {
            delete pkt;
            PacketPtr next = pathBufferQueue.front();
            pathBufferQueue.pop();
            rhoPort.sendPacket(next);
        }
        else
        {
            panic_if(evictPkt == nullptr || originalPacket == nullptr, "Original packet is null\n");
            DPRINTF(HitchhikerB, "Invalidated original evict packet %s\n", originalPacket->print());
            delete originalPacket;
            originalPacket = nullptr;
            waitingPortId = -1;
            if (rhoStashCapty - rhoStash.size() < (rhoLevel + 1)*rhoBktSize)
            {
                acState = AccessState::RhoEvictWrite;
                startRhoWrite();
            }
            else
            {
                assert(evictPkt);
                blocked = false;
                acState = AccessState::Idle;
                DPRINTF(HitchhikerB, "Skipped rho path write, start evicting packet %s to main memory\n", evictPkt->print());
                handleRequest(evictPkt, 0);
            }
        }
    }
    else if (acState == AccessState::RhoEvictRead)
    {
        insertRhoStash(pkt);
        if (!pathBufferQueue.empty())
        {
            delete pkt;
            PacketPtr next = pathBufferQueue.front();
            pathBufferQueue.pop();
            rhoPort.sendPacket(next);
        }
        else
        {
            panic_if(evictPkt == nullptr || originalPacket == nullptr, "Original packet is null\n");
            
            swapEvictBlock();

            delete pkt;
            pkt = originalPacket;
            originalPacket = nullptr;

            auto it = reqTimes.find(pkt->getBlockAddr(blockSize));
            panic_if(it == reqTimes.cend(), "Not found in request arrival time map!");
            if (!warmup) reqLatency.sample(curTick() - it->second);
            reqTimes.erase(it);
            
            // if (pkt->needsResponse() || pkt->isResponse())     
            //     sendResponse(pkt);
            // else
            {
                DPRINTF(HitchhikerB, "Packet %s needn't response, deleted\n", pkt->print());
                waitingPortId = -1;
                delete pkt; // ?
            }
            if (!warmup) missLatency.sample(curTick() - missTime);
            if (rhoStashCapty - rhoStash.size() < (rhoLevel + 1)*rhoBktSize)
            {
                acState = AccessState::RhoEvictWrite;
                startRhoWrite();
            }
            else
            {
                assert(evictPkt);
                blocked = false;
                acState = AccessState::Idle;
                DPRINTF(HitchhikerB, "Skipped rho path write, start evicting packet %s to main memory\n", evictPkt->print());
                handleRequest(evictPkt, 0);
            }
        }
    }
    else if (acState == AccessState::ReadRho)
    {
        // TODO: insert to rho stash
        insertRhoStash(pkt);
        if (!pathBufferQueue.empty())
        {
            delete pkt;
            PacketPtr next = pathBufferQueue.front();
            pathBufferQueue.pop();
            rhoPort.sendPacket(next);
        }
        else
        {
            panic_if(originalPacket == nullptr, "Original packet is null\n");
            DPRINTF(HitchhikerB, "Copying data from new packet to old\n");
            bool hit = accessRhoStash(originalPacket);
            panic_if(!hit, "Should always hit after rho inserting, pkt: %s", originalPacket->print());
            if (originalPacket->needsResponse())
                originalPacket->makeResponse();
            delete pkt;
            pkt = originalPacket;
            originalPacket = nullptr;

            auto it = reqTimes.find(pkt->getBlockAddr(blockSize));
            panic_if(it == reqTimes.cend(), "Not found in request arrival time map!");
            if (!warmup) reqLatency.sample(curTick() - it->second);
            reqTimes.erase(it);
            
            if (pkt->needsResponse() || pkt->isResponse())     
                sendResponse(pkt);
            else
            {
                DPRINTF(HitchhikerB, "Packet %s needn't response, deleted\n", pkt->print());
                waitingPortId = -1;
                delete pkt; // ?
            }
            if (!warmup) missLatency.sample(curTick() - missTime);
            if (rhoStashCapty - rhoStash.size() < (rhoLevel + 1)*rhoBktSize)
            {
                acState = AccessState::WriteRho;
                startRhoWrite();
            }
            else
            {
                DPRINTF(HitchhikerB, "Skipped rho path write, start next request\n");
                tryNextRequest();
            }
        }
    }
    else if (acState == AccessState::WriteBack)
    {
        if (!pathBufferQueue.empty()) {
            delete pkt;
            // ReqQueueEntry next = pathBufferQueue.front();
            PacketPtr next = pathBufferQueue.front();
            pathBufferQueue.pop();
            // memPort.sendPacket(next.pkt);
            memPort.sendPacket(next);
            // DPRINTF(HitchhikerB, "Write next level\n");
        } else {
            // Write back completed, start next request
            delete pkt;
            tryNextRequest();
        }
    }
    else if (acState == AccessState::WriteRho || acState == AccessState::RhoEvictWrite)
    {
        if (!pathBufferQueue.empty()) {
            delete pkt;
            // ReqQueueEntry next = pathBufferQueue.front();
            PacketPtr next = pathBufferQueue.front();
            pathBufferQueue.pop();
            // memPort.sendPacket(next.pkt);
            rhoPort.sendPacket(next);
            // DPRINTF(HitchhikerB, "Write next level\n");
        } else {
            // Write back completed, start next request
            delete pkt;
            bool needEvict = (acState == AccessState::RhoEvictWrite);

            if (needEvict)
            {
                assert(evictPkt);
                pkt = evictPkt;
                blocked = false;
                acState = AccessState::Idle;
                // evictPkt = nullptr;
                DPRINTF(HitchhikerB, "Start evicting packet %s to main memory\n", pkt->print());
                handleRequest(pkt, 0); //dummy port id 0
                return true;
            }
            DPRINTF(HitchhikerB, "Completed path write, start next request\n");
            tryNextRequest();   
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
void HitchhikerB::sendResponse(PacketPtr pkt)
{
    assert(blocked && (acState == AccessState::ReadPath || acState == AccessState::ReadRho));
    assert(pkt->needsResponse() || pkt->isResponse());
    DPRINTF(HitchhikerB, "Sending resp for addr %#x\n", pkt->getAddr());

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

    DPRINTF(HitchhikerB, "Completed path read, send response to port %d, start path write\n", port);

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
HitchhikerB::handleFunctional(PacketPtr pkt)
{
    Addr block_addr = pkt->getBlockAddr(blockSize);
    auto entry = rhoTag.find(block_addr);
    if (accessFunctional(pkt)) {
        if (entry != rhoTag.end())
        {
            freeRhoBlocks.push_back(entry->second.addr - rhoBase);
            DPRINTF(HitchhikerB, "Invalidate rho tag %#lx for functional request\n", entry->second.addr);
            rhoTag.erase(entry);
        }
        if (pkt->needsResponse())
            pkt->makeResponse();
    } else if (accessRhoStash(pkt)) {
        if (pkt->needsResponse())
            pkt->makeResponse();
    } else {
        // memPort.sendFunctional(pkt);
        if (entry == rhoTag.end())
            memPort.sendFunctional(pkt);
        else
        {
            if (pkt->isWrite())
            {
                freeRhoBlocks.push_back(entry->second.addr - rhoBase);
                DPRINTF(HitchhikerB, "Invalidate rho tag %#lx for functional request\n", entry->second.addr);
                rhoTag.erase(entry);
                memPort.sendFunctional(pkt);
                if (acState == AccessState::RhoEvictRead)
                {
                    acState = AccessState::RhoEvictInvalid;
                }
            }
            else
            {
                // panic_if(acState != AccessState::Idle, "Busy when receiving functional request");
                // panic("Cannot handle functional request %s in rho\n", pkt->print());
                memPort.sendFunctional(pkt);
            }
        }
    }
}

void
HitchhikerB::accessTiming(PacketPtr pkt)
{
    bool hit = accessFunctional(pkt) || accessRhoStash(pkt);
    // bool hit = (rhoTag.find(pkt->getBlockAddr(blockSize)) != rhoTag.end()) ? false : accessFunctional(pkt);

    DPRINTF(HitchhikerB, "%s for packet: %s\n", hit ? "Hit" : "Miss",
            pkt->print());

    if (hit) { // get the block from stash
        // Respond to the CPU side
        if (!warmup) hits++; // update stats
        DPRINTF(HitchhikerB, "Stash hit for packet: %s\n", pkt->print());
        // DDUMP(HitchhikerB, pkt->getConstPtr<uint8_t>(), pkt->getSize());
        if (pkt == evictPkt)
            evictPkt = nullptr;
        if (pkt->needsResponse())
        {
            pkt->makeResponse();
            sendResponse(pkt);
        }
        else
        {
            DPRINTF(HitchhikerB, "Packet %s needn't response, deleted\n", pkt->print());
            waitingPortId = -1;
            delete pkt; // ?
        }
        waitingPortId = -1;     
        tryNextRequest();     
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
            if (pkt == evictPkt)
            {
                evictPkt = nullptr;
                DPRINTF(HitchhikerB, "Forwarding evict packet\n");
                startPathRead(pkt);
                return;
            }
            if (rhoAccess(pkt))
                return;
            if (pkt->isWriteback())
            {
                evictPkt = allocateRho(pkt);
                // assert(rhoAccess(pkt) && "Must hit after allocating rho block\n");
                if (evictPkt)
                    acState = AccessState::RhoEvictRead;
                assert(rhoAccess(pkt) && "Must hit after allocating rho block\n");
                return;
            }

            // Aligned and block size. We can just forward.
            DPRINTF(HitchhikerB, "Rho miss, forwarding packet\n");
            // access(pkt); 
            startPathRead(pkt);           
        } else {
            DPRINTF(HitchhikerB, "Upgrading packet to block size\n");
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

            if (rhoAccess(new_pkt))
                return;

            DPRINTF(HitchhikerB, "Rho miss, forwarding packet\n");

            // access(new_pkt);   
            startPathRead(new_pkt);     
        }
    }
}

bool
HitchhikerB::accessFunctional(PacketPtr pkt)
{
    Addr block_addr = pkt->getBlockAddr(blockSize);
    // panic_if(rhoTag.find(block_addr) != rhoTag.end(), "Rho conflict");
    if (rhoTag.find(block_addr) != rhoTag.end())
        return false;
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
HitchhikerB::insert(PacketPtr pkt)
{
    // The packet should be aligned.
    assert(pkt->getAddr() ==  pkt->getBlockAddr(blockSize));
    // The address should not be in the cache
    // assert(cacheStore.find(pkt->getAddr()) == cacheStore.end());
    if (cacheStore.find(pkt->getAddr()) != cacheStore.end())
        return;
        // delete[] cacheStore[pkt->getAddr()];
    // The pkt should be a response
    assert(pkt->isResponse());    
    // }
    Addr addr = pkt->getAddr();
    if (addr <= validBlockNum * blockSize) {
        panic_if(cacheStore.size() >= capacity, "Stash Overflow!");

        // DPRINTF(HitchhikerB, "Inserting %s\n", pkt->print());
        // DDUMP(HitchhikerB, pkt->getConstPtr<uint8_t>(), blockSize);

        // Allocate space for the cache block data
        uint8_t *data = new uint8_t[blockSize];

        // Insert the data and address into the cache store
        cacheStore[addr] = data;
        stashWrites++;

        // Write the data into the cache
        pkt->writeDataToBlock(data, blockSize);
    }
}

AddrRangeList
HitchhikerB::getAddrRanges() const
{
    DPRINTF(HitchhikerB, "Sending new ranges\n");
    // Just use the same ranges as whatever is on the memory side.
    return memPort.getAddrRanges();
}

void
HitchhikerB::sendRangeChange() const
{
    for (auto& port : cpuPorts) {
        port.sendRangeChange();
    }
}

void
HitchhikerB::regStats()
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

HitchhikerB*
HitchhikerBParams::create()
{
    return new HitchhikerB(this);
}

// void HitchhikerB::access(PacketPtr pkt) 
void HitchhikerB::startPathRead(PacketPtr pkt) 
{
    Addr addr = pkt->getBlockAddr(blockSize);
    Addr blockAddr = addr / blockSize;
    // Step 1: Lookup Position map and remap
	// int64_t oldPosition	= posMap[blockAddr].leaf;
    currentLeaf = posMap[blockAddr].leaf;
	int64_t newPosition = generateRandomLeaf(blockAddr / bucketSize, maxLevel).leaf;
    posMap[blockAddr].leaf = newPosition;
    DPRINTF(HitchhikerB, "Remapping addr %#lx from %#lx to %#lx\n", addr, currentLeaf, newPosition);

    // Step 2: Preparing packets along the whole path in the path buffer queue
    uint64_t bucketIndex = currentLeaf;
	for (int height = maxLevel; height >= 0; height--)
	{
        for (int bi = 0; bi < bucketSize; bi++) // bi: index inside block
        {
            // assert(bucketIndex || (bucketIndex == 0 && height == 0));
            // assert(bucketIndex * bucketSize < blockNum);
            // Create a new packet that is blockSize
            Addr addr = (bucketIndex * bucketSize + bi) * blockSize;
            if (cacheStore.find(addr) != cacheStore.end())
                continue;
            RequestPtr req = std::make_shared<Request>(addr, blockSize, 0, 0);
            PacketPtr new_pkt = new Packet(req, MemCmd::ReadReq, blockSize);
            new_pkt->allocate();

            // new_pkt->setAddr((bucketIndex * bucketSize + bi) * blockSize);
            // new_pkt->setSize(sizeof(int64_t) * bucketSize); // bug here

            // Should now be block aligned
            // assert(new_pkt->getAddr() == new_pkt->getBlockAddr(blockSize));
            pathBufferQueue.push(new_pkt);        
            // bucketIndex = (bucketIndex - 1) / 2;
        }
        if (bucketIndex == 0) break; // reach root, stop
        bucketIndex = (bucketIndex - 1) >> 1;
	}
    DPRINTF(HitchhikerB, "Start accessing path %#lx for original addr %#lx\n", currentLeaf, originalPacket->getBlockAddr(blockSize));

    // ReqQueueEntry next = pathBufferQueue.front();
    PacketPtr next = pathBufferQueue.front();
    pathBufferQueue.pop();
    // memPort.sendPacket(next.pkt);
    memPort.sendPacket(next);
}

std::unordered_map<Addr, uint8_t*>::const_iterator HitchhikerB::scanStashForEvict(unsigned curLevel)
{
    assert(acState == AccessState::WriteBack);
    for (auto it = cacheStore.cbegin(); it != cacheStore.cend(); it++)
    {
        Addr addr = it->first;
        uint64_t blockAddr = addr / blockSize;
        uint64_t leaf = posMap[blockAddr].leaf;
        uint64_t bucketAddr = blockAddr / bucketSize;
        unsigned levelDif = maxLevel - curLevel;
        if (((leaf + 1) >> levelDif == (currentLeaf + 1) >> levelDif) && ((leaf + 1) >> levelDif == bucketAddr + 1))
            return it;
    }
    return cacheStore.cend();
}

void HitchhikerB::startPathWrite()
{
    assert(pathBufferQueue.empty());

    for (auto &&entry : cacheStore)
    {
        RequestPtr req = std::make_shared<Request>(entry.first, blockSize, 0, 0);
        PacketPtr new_pkt = new Packet(req, MemCmd::WriteReq, blockSize);
        new_pkt->dataDynamic(entry.second);
        pathBufferQueue.push(new_pkt);
        stashWrites++;
    }
    cacheStore.clear();
    
    DPRINTF(HitchhikerB, "Start writing back to path %#lx, %d accesses in total\n", currentLeaf, pathBufferQueue.size());

    // ReqQueueEntry next = pathBufferQueue.front();
    PacketPtr next = pathBufferQueue.front();
    pathBufferQueue.pop();
    // memPort.sendPacket(next.pkt);
    memPort.sendPacket(next);
}

void HitchhikerB::tryNextRequest()
{
    assert(blocked && acState != AccessState::Idle);
    blocked = false;
    acState = AccessState::Idle;
    if (!reqQueue.empty())
    {
        ReqQueueEntry next = reqQueue.front();
        reqQueue.pop();
        DPRINTF(HitchhikerB, "Next resquest %s popped from queue\n", next.pkt->print());
        handleRequest(next.pkt, next.port_id);
    }
    else
        DPRINTF(HitchhikerB, "Request queue empty\n");
    DPRINTF(HitchhikerB, "Try send retry\n");
    for (auto& port : cpuPorts) {
        port.trySendRetry();
    }
}

PacketPtr HitchhikerB::allocateRho(PacketPtr pkt)
{
    // panic_if(cacheStore.find(pkt->getAddr()) != cacheStore.end(), "Stash hit in rho!");
    if (rhoTag.size() < rhoCapty)
    {
        assert(!freeRhoBlocks.empty());
        auto it = std::next(freeRhoBlocks.begin(), random_mt.random(0UL, freeRhoBlocks.size() - 1));
        Addr rhoAddr = *it;
        freeRhoBlocks.erase(it);
        uint64_t rhoBktAddr = rhoAddr / blockSize / rhoBktSize;
        // unsigned level = log_binary(rhoBktAddr + 1);
        auto leaf = generateRandomLeaf(rhoBktAddr, rhoLevel).leaf;
        DPRINTF(HitchhikerB, "Allocated new rho block %#lx for original block %#lx, leaf: %#lx\n", rhoAddr + rhoBase, pkt->getAddr(), leaf);
        rhoTag[pkt->getAddr()] = { leaf, rhoAddr + rhoBase };
        return nullptr;
    }
    assert(freeRhoBlocks.empty());
    auto evictIt = std::next(rhoTag.begin(), random_mt.random(0UL, rhoTag.size() - 1));
    Addr evictAddr = evictIt->first;
    auto freeEntry = evictIt->second;
    DPRINTF(HitchhikerB, "Evict rho block %#lx (original block %#lx) for new block %#lx, leaf: %#lx\n", freeEntry.addr, evictAddr, pkt->getAddr(), freeEntry.leaf);
    rhoTag.erase(evictIt);
    rhoTag[pkt->getAddr()] = freeEntry;

    RequestPtr req = std::make_shared<Request>(evictAddr, blockSize, 0, 0);
    PacketPtr new_pkt = new Packet(req, MemCmd::WritebackDirty, blockSize);
    return new_pkt;
    // return true;
}

void HitchhikerB::swapEvictBlock()
{
    assert(originalPacket->isWriteback() && evictPkt->isWriteback());
    Addr block_addr = originalPacket->getBlockAddr(blockSize);
    auto entry = rhoTag.find(block_addr);
    panic_if(entry == rhoTag.end(), "Should always hit when swapping, original addr: %#lx", block_addr);
    Addr rhoAddr = entry->second.addr;
    auto it = rhoStash.find(rhoAddr);
    panic_if(it == rhoStash.end(), "Should always hit when swapping, rho addr: %#lx", rhoAddr);
    uint8_t *data = new uint8_t[blockSize];
    uint8_t *oldData = it->second;
    it->second = data;
    stashWrites++;
    evictPkt->dataDynamic(oldData);
    originalPacket->writeDataToBlock(data, blockSize);
}

bool HitchhikerB::accessRhoStash(PacketPtr pkt)
{
    Addr block_addr = pkt->getBlockAddr(blockSize);
    auto entry = rhoTag.find(block_addr);
    // panic_if(entry == rhoTag.);
    if (entry == rhoTag.end()) return false;
    // panic_if(cacheStore.find(block_addr) != cacheStore.end(), "Stash hit in rho!");
    Addr rhoAddr = entry->second.addr;
    auto it = rhoStash.find(rhoAddr);
    if (it != rhoStash.end()) {
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

void HitchhikerB::insertRhoStash(PacketPtr pkt)
{
    assert(pkt->getAddr() ==  pkt->getBlockAddr(blockSize));
    // The address should not be in the cache
    // assert(cacheStore.find(pkt->getAddr()) == cacheStore.end());
    if (rhoStash.find(pkt->getAddr()) != rhoStash.end())
        return;
    // The pkt should be a response
    assert(pkt->isResponse());

    Addr addr = pkt->getAddr();
    // Allocate space for the cache block data
    uint8_t *data = new uint8_t[blockSize];

    // Insert the data and address into the cache store
    rhoStash[addr] = data;
    stashWrites++;

    // Write the data into the cache
    pkt->writeDataToBlock(data, blockSize);
}

bool HitchhikerB::rhoAccess(PacketPtr pkt)
{
    auto it = rhoTag.find(pkt->getBlockAddr(blockSize));
    if (it == rhoTag.cend()) return false;
    auto entry = it->second;
    assert(pathBufferQueue.empty());
    if (acState != AccessState::RhoEvictRead)
        acState = AccessState::ReadRho;

    DPRINTF(HitchhikerB, "Rho hit for packet: %s\n", originalPacket->print());
    // panic_if(cacheStore.find(pkt->getBlockAddr(blockSize)) != cacheStore.end(), "Stash hit in rho!");
    auto stashEntry = rhoStash.find(entry.addr);
    if (stashEntry != rhoStash.end())
    {
        if (acState == AccessState::RhoEvictRead)
        {
            panic_if(evictPkt == nullptr || originalPacket == nullptr, "Original packet is null\n");
            swapEvictBlock();
            assert(pkt == originalPacket);
            originalPacket = nullptr;
            
            auto it = reqTimes.find(pkt->getBlockAddr(blockSize));
            panic_if(it == reqTimes.cend(), "Not found in request arrival time map!");
            if (!warmup) reqLatency.sample(curTick() - it->second);
            reqTimes.erase(it);

            waitingPortId = -1;
            delete pkt;

            blocked = false;
            acState = AccessState::Idle;
            DPRINTF(HitchhikerB, "Skipped rho path write, start evicting packet %s to main memory\n", evictPkt->print());
            handleRequest(evictPkt, 0);
            return true;
        }
        else if (acState == AccessState::ReadRho && pkt->isWriteback())
        {
            assert(accessRhoStash(pkt));
            waitingPortId = -1;
            delete pkt;
            tryNextRequest();
            return true;
        }
        else
        {
            panic("Unknown rho hit for packet %s!", pkt->print());
            return false;
        }
    }

    currentLeaf = entry.leaf;
    uint64_t bucketIndex = currentLeaf;
    for (int height = rhoLevel; height >= 0; height--)
	{
        for (int bi = 0; bi < rhoBktSize; bi++) // bi: index inside block
        {
            Addr addr = (bucketIndex * rhoBktSize + bi) * blockSize + rhoBase;
            if (rhoStash.find(addr) != rhoStash.end())
                continue;
            RequestPtr req = std::make_shared<Request>(addr, blockSize, 0, 0);
            PacketPtr new_pkt = new Packet(req, MemCmd::ReadReq, blockSize);
            new_pkt->allocate();
            pathBufferQueue.push(new_pkt);        
        }
        if (bucketIndex == 0) break; // reach root, stop
        bucketIndex = (bucketIndex - 1) >> 1;
	}
    DPRINTF(HitchhikerB, "Start accessing rho path %#lx for original addr %#lx, %d accesses in total\n", currentLeaf, originalPacket->getBlockAddr(blockSize), pathBufferQueue.size());

    PacketPtr next = pathBufferQueue.front();
    pathBufferQueue.pop();
    rhoPort.sendPacket(next);
    return true;
}

void HitchhikerB::startRhoWrite()
{
    assert(pathBufferQueue.empty());
    // panic_if(rhoStash.size() != (rhoLevel + 1) * rhoBktSize, "Incomplete rho path with %d blocks, L = %d, Z = %d\n", rhoStash.size(), rhoLevel, rhoBktSize);

    for (auto &&entry : rhoStash)
    {
        RequestPtr req = std::make_shared<Request>(entry.first, blockSize, 0, 0);
        PacketPtr new_pkt = new Packet(req, MemCmd::WriteReq, blockSize);
        new_pkt->dataDynamic(entry.second);
        pathBufferQueue.push(new_pkt);
        stashWrites++;
    }

    rhoStash.clear();

    DPRINTF(HitchhikerB, "Start writing back to rho path %#lx, %d accesses in total\n", currentLeaf, pathBufferQueue.size());

    // ReqQueueEntry next = pathBufferQueue.front();
    PacketPtr next = pathBufferQueue.front();
    pathBufferQueue.pop();
    // memPort.sendPacket(next.pkt);
    rhoPort.sendPacket(next);
}