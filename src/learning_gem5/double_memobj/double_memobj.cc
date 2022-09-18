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

#include "learning_gem5/double_memobj/double_memobj.hh"
#include "base/random.hh"
#include "debug/DoubleMemobj.hh"

DoubleMemobj::DoubleMemobj(DoubleMemobjParams *params) :
    ClockedObject(params),
    // instPort(params->name + ".inst_port", this),
    // dataPort(params->name + ".data_port", this),
    memPort(params->name + ".mem_side", this),
    doublePort(params->name + ".double_port", this),
    blocked(false), waitingPortId(-1)
{
    for (int i = 0; i < params->port_cpu_side_connection_count; ++i) {
        cpuPorts.emplace_back(name() + csprintf(".cpu_side[%d]", i), i, this);
    }
    for (Addr a = 0; a < 0x1000000; a += 64)
        freeBlocks.push_back(a + (1UL << 31));
    DPRINTF(DoubleMemobj, "Create success\n");
}

Port &
DoubleMemobj::getPort(const std::string &if_name, PortID idx)
{
    // panic_if(idx != InvalidPortID, "This object doesn't support vector ports");

    // This is the name from the Python SimObject declaration (DoubleMemobj.py)
    if (if_name == "mem_side") {
        panic_if(idx != InvalidPortID, "This object doesn't support vector ports");
        return memPort;
    } else if (if_name == "cpu_side" && idx < cpuPorts.size()) {
        // We should have already created all of the ports in the constructor
        return cpuPorts[idx];
    } else if (if_name == "double_port") {
        return doublePort;
    } else {
        // pass it along to our super class
        return SimObject::getPort(if_name, idx);
    }
}

void
DoubleMemobj::CPUSidePort::sendPacket(PacketPtr pkt)
{
    // Note: This flow control is very simple since the memobj is blocking.

    panic_if(blockedPacket != nullptr, "Should never try to send if blocked!");

    // If we can't send the packet across the port, store it for later.
    if (!sendTimingResp(pkt)) {
        blockedPacket = pkt;
    }
}

AddrRangeList
DoubleMemobj::CPUSidePort::getAddrRanges() const
{
    return owner->getAddrRanges();
}

void
DoubleMemobj::CPUSidePort::trySendRetry()
{
    if (needRetry && blockedPacket == nullptr) {
        // Only send a retry if the port is now completely free
        needRetry = false;
        DPRINTF(DoubleMemobj, "Sending retry req for %d\n", id);
        sendRetryReq();
    }
}

void
DoubleMemobj::CPUSidePort::recvFunctional(PacketPtr pkt)
{
    // Just forward to the memobj.
    return owner->handleFunctional(pkt);
}

bool
DoubleMemobj::CPUSidePort::recvTimingReq(PacketPtr pkt)
{
    // Just forward to the memobj.
    if (!owner->handleRequest(pkt, id)) {
        needRetry = true;
        return false;
    } else {
        return true;
    }
}

void
DoubleMemobj::CPUSidePort::recvRespRetry()
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
DoubleMemobj::MemSidePort::sendPacket(PacketPtr pkt)
{
    // Note: This flow control is very simple since the memobj is blocking.

    panic_if(blockedPacket != nullptr, "Should never try to send if blocked!");

    // If we can't send the packet across the port, store it for later.
    if (!sendTimingReq(pkt)) {
        blockedPacket = pkt;
    }
}

bool
DoubleMemobj::MemSidePort::recvTimingResp(PacketPtr pkt)
{
    // Just forward to the memobj.
    return owner->handleResponse(pkt);
}

void
DoubleMemobj::MemSidePort::recvReqRetry()
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
DoubleMemobj::MemSidePort::recvRangeChange()
{
    owner->sendRangeChange();
}

bool
DoubleMemobj::handleRequest(PacketPtr pkt, int port)
{
    if (blocked) {
        // There is currently an outstanding request. Stall.
        return false;
    }

    DPRINTF(DoubleMemobj, "Got request for addr %#x\n", pkt->getAddr());

    // This memobj is now blocked waiting for the response to this packet.
    assert(!blocked && (waitingPortId == -1));
    // panic_if(blocked);
    blocked = true;
    waitingPortId = port;

    // schedule(new EventFunctionWrapper([this, pkt]{ accessTiming(pkt); },
    //                                   name() + ".accessEvent", true),
    //          clockEdge(Cycles(10)));
    accessTiming(pkt);

    return true;
}

void
DoubleMemobj::accessTiming(PacketPtr pkt)
{
    auto it = tag.find(pkt->getAddr());
    if (it == tag.cend())
        // Simply forward to the memory port
        // memPort.sendPacket(pkt);
    {
        DPRINTF(DoubleMemobj, "Request miss\n");
        if (pkt->isWriteback())
        {
            if (freeBlocks.empty())
            {
                // auto eit = std::next(tag.begin)
                Packet *new_pkt = new Packet(pkt->req, MemCmd::WriteReq);
                originalPacket = pkt;
                memPort.sendPacket(new_pkt);
                // blocked = false;
                // waitingPortId = -1;
                DPRINTF(DoubleMemobj, "No free block left to allocate\n");
                // for (auto &&port : cpuPorts)
                //     port.trySendRetry();
            }
            else
            {
                auto lit = std::next(freeBlocks.begin(), random_mt.random(0UL, freeBlocks.size() - 1));
                Addr addr = *lit;
                freeBlocks.erase(lit);
                tag.insert({pkt->getAddr(), addr});
                DPRINTF(DoubleMemobj, "Allocate new block %#x for original block %#x, %#x free blocks left\n", addr, pkt->getAddr(), freeBlocks.size());
                // RequestPtr req = std::make_shared<Request>(*(pkt->req));
                RequestPtr req = std::make_shared<Request>(addr, pkt->getSize(), 0, 0);
                // req->setPaddr(addr);
                Packet *new_pkt = new Packet(req, MemCmd::WriteReq);
                uint8_t *data = new uint8_t[pkt->getSize()];
                pkt->writeData(data);
                new_pkt->dataDynamic(data);
                // delete pkt;
                originalPacket = pkt;
                DPRINTF(DoubleMemobj, "Original packet: %s, new packet: %s\n", originalPacket->print(), new_pkt->print());
                doublePort.sendPacket(new_pkt);
                // blocked = false;
                // waitingPortId = -1;
                // for (auto &&port : cpuPorts)
                //     port.trySendRetry();
            }
        }
        else
            memPort.sendPacket(pkt);
    }
    else
    // if (pkt->getAddr() < 0x1000000)
    {
        DPRINTF(DoubleMemobj, "Request hit at %#x\n", it->second);
        // pkt->setAddr(pkt->getAddr() + (1UL << 31));
        originalPacket = pkt;
        // RequestPtr req = std::make_shared<Request>(*(pkt->req));
        RequestPtr req = std::make_shared<Request>(it->second, pkt->getSize(), 0, 0);
        // req->setPaddr(it->second);
        MemCmd cmd = pkt->isWriteback() ? MemCmd::WriteReq : pkt->cmd;
        Packet *new_pkt = new Packet(req, cmd);
        uint8_t *data = new uint8_t[pkt->getSize()];
        pkt->writeData(data);
        new_pkt->dataDynamic(data);
        DPRINTF(DoubleMemobj, "Original packet: %s, new packet: %s\n", originalPacket->print(), new_pkt->print());
        doublePort.sendPacket(new_pkt);
        // if (pkt->isWriteback())
        // {
        //     DPRINTF(DoubleMemobj, "Write back to %#x\n", it->second);
        //     originalPacket = nullptr;
        //     delete pkt;
        //     blocked = false;
        //     waitingPortId = -1;
        //     for (auto &&port : cpuPorts)
        //         port.trySendRetry();
        // }
    }
    // else
    //     memPort.sendPacket(pkt);
}

bool
DoubleMemobj::handleResponse(PacketPtr pkt)
{
    assert(blocked);
    DPRINTF(DoubleMemobj, "Got response for addr %#x\n", pkt->getAddr());

    // The packet is now done. We're about to put it in the port, no need for
    // this object to continue to stall.
    // We need to free the resource before sending the packet in case the CPU
    // tries to send another request immediately (e.g., in the same callchain).
    assert(blocked && (waitingPortId != -1));

    // if (pkt->getAddr() >= (1UL << 31))
    //     pkt->setAddr(pkt->getAddr() - (1UL << 31));
    if (originalPacket)
    {
        DPRINTF(DoubleMemobj, "Original packet: %s, new packet: %s\n", originalPacket->print(), pkt->print());
        uint8_t *data = new uint8_t[pkt->getSize()];
        pkt->writeData(data);
        originalPacket->setData(data);
        delete[] data;
        delete pkt;
        if (originalPacket->needsResponse())
            originalPacket->makeResponse();
        pkt = originalPacket;
        originalPacket = nullptr;
    }

    // Simply forward to the memory port
    if (pkt->isResponse())
        cpuPorts[waitingPortId].sendPacket(pkt);
    else
        delete pkt;
    DPRINTF(DoubleMemobj, "Sent response for addr %#x\n", pkt->getAddr());
    waitingPortId = -1;
    blocked = false;
    // if (pkt->req->isInstFetch()) {
    //     cpuPorts[waitingPortId].sendPacket(pkt);
    // } else {
    //     cpuPorts[waitingPortId].sendPacket(pkt);
    // }

    // For each of the cpu ports, if it needs to send a retry, it should do it
    // now since this memory object may be unblocked now.
    // instPort.trySendRetry();
    // dataPort.trySendRetry();
    for (auto &&port : cpuPorts)
        port.trySendRetry();

    return true;
}

void
DoubleMemobj::handleFunctional(PacketPtr pkt)
{
    DPRINTF(DoubleMemobj, "Got functional for addr %#x\n", pkt->getAddr());
    // Just pass this on to the memory side to handle for now.
    // if (pkt->getAddr() < 0x1000000)
    //     doublePort.sendPacket(pkt);
    // else
    memPort.sendFunctional(pkt);
}

AddrRangeList
DoubleMemobj::getAddrRanges() const
{
    DPRINTF(DoubleMemobj, "Sending new ranges\n");
    // Just use the same ranges as whatever is on the memory side.
    return memPort.getAddrRanges();
}

void
DoubleMemobj::sendRangeChange()
{
    // instPort.sendRangeChange();
    // dataPort.sendRangeChange();
    for (auto &&port : cpuPorts)
        port.sendRangeChange();
}



DoubleMemobj*
DoubleMemobjParams::create()
{
    return new DoubleMemobj(this);
}
