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

#ifndef __LEARNING_GEM5_SIMPLE_CACHE_SIMPLE_CACHE_HH__
#define __LEARNING_GEM5_SIMPLE_CACHE_SIMPLE_CACHE_HH__

#include <unordered_map>
#include <queue>
#include <list>
#include <set>

#include "base/statistics.hh"
#include "mem/port.hh"
#include "params/HitchhikerR.hh"
#include "sim/clocked_object.hh"

/**
 * A very simple cache object. Has a fully-associative data store with random
 * replacement.
 * This cache is fully blocking (not non-blocking). Only a single request can
 * be outstanding at a time.
 * This cache is a writeback cache.
 */
class HitchhikerR : public ClockedObject
{
  private:

    /**
     * Port on the CPU-side that receives requests.
     * Mostly just forwards requests to the cache (owner)
     */
    class CPUSidePort : public SlavePort
    {
      private:
        /// Since this is a vector port, need to know what number this one is
        int id;

        /// The object that owns this object (HitchhikerR)
        HitchhikerR *owner;

        /// True if the port needs to send a retry req.
        bool needRetry;

        /// If we tried to send a packet and it was blocked, store it here
        PacketPtr blockedPacket;

      public:
        /**
         * Constructor. Just calls the superclass constructor.
         */
        CPUSidePort(const std::string& name, int id, HitchhikerR *owner) :
            SlavePort(name, owner), id(id), owner(owner), needRetry(false),
            blockedPacket(nullptr)
        { }

        /**
         * Send a packet across this port. This is called by the owner and
         * all of the flow control is hanled in this function.
         * This is a convenience function for the HitchhikerR to send pkts.
         *
         * @param packet to send.
         */
        void sendPacket(PacketPtr pkt);

        /**
         * Get a list of the non-overlapping address ranges the owner is
         * responsible for. All slave ports must override this function
         * and return a populated list with at least one item.
         *
         * @return a list of ranges responded to
         */
        AddrRangeList getAddrRanges() const override;

        /**
         * Send a retry to the peer port only if it is needed. This is called
         * from the HitchhikerR whenever it is unblocked.
         */
        void trySendRetry();
      protected:
        /**
         * Receive an atomic request packet from the master port.
         * No need to implement in this simple cache.
         */
        Tick recvAtomic(PacketPtr pkt) override
        { panic("recvAtomic unimpl."); }

        /**
         * Receive a functional request packet from the master port.
         * Performs a "debug" access updating/reading the data in place.
         *
         * @param packet the requestor sent.
         */
        void recvFunctional(PacketPtr pkt) override;

        /**
         * Receive a timing request from the master port.
         *
         * @param the packet that the requestor sent
         * @return whether this object can consume to packet. If false, we
         *         will call sendRetry() when we can try to receive this
         *         request again.
         */
        bool recvTimingReq(PacketPtr pkt) override;

        /**
         * Called by the master port if sendTimingResp was called on this
         * slave port (causing recvTimingResp to be called on the master
         * port) and was unsuccesful.
         */
        void recvRespRetry() override;
    };

    /**
     * Port on the memory-side that receives responses.
     * Mostly just forwards requests to the cache (owner)
     */
    class MemSidePort : public MasterPort
    {
      private:
        /// The object that owns this object (HitchhikerR)
        HitchhikerR *owner;

        /// If we tried to send a packet and it was blocked, store it here
        PacketPtr blockedPacket;

      public:
        /**
         * Constructor. Just calls the superclass constructor.
         */
        MemSidePort(const std::string& name, HitchhikerR *owner) :
            MasterPort(name, owner), owner(owner), blockedPacket(nullptr)
        { }

        /**
         * Send a packet across this port. This is called by the owner and
         * all of the flow control is hanled in this function.
         * This is a convenience function for the HitchhikerR to send pkts.
         *
         * @param packet to send.
         */
        void sendPacket(PacketPtr pkt);

      protected:
        /**
         * Receive a timing response from the slave port.
         */
        bool recvTimingResp(PacketPtr pkt) override;

        /**
         * Called by the slave port if sendTimingReq was called on this
         * master port (causing recvTimingReq to be called on the slave
         * port) and was unsuccesful.
         */
        void recvReqRetry() override;

        /**
         * Called to receive an address range change from the peer slave
         * port. The default implementation ignores the change and does
         * nothing. Override this function in a derived class if the owner
         * needs to be aware of the address ranges, e.g. in an
         * interconnect component like a bus.
         */
        void recvRangeChange() override;
    };

public:  
    struct PosMapEntry
    {
        uint64_t leaf;
        unsigned level;
        bool operator== (const PosMapEntry &rhs) const
        { return this->leaf == rhs.leaf && this->level == rhs.level; }
    };

	struct RhoTagEntry
    {
        uint64_t leaf;
        Addr addr;
    };

    struct ReqQueueEntry
    {
        PacketPtr pkt;
        int port_id;
        PosMapEntry posInfo;
        unsigned age;
        bool operator< (const ReqQueueEntry &rhs) const { return this->posInfo.level < rhs.posInfo.level; }
        bool operator== (const ReqQueueEntry &rhs) const
        { return this->pkt == rhs.pkt && this->port_id == rhs.port_id && this->posInfo == rhs.posInfo && this->age == rhs.age; }
    };

    struct EvictSetEntry
    {
        PacketPtr pkt;
        unsigned level;
        bool operator> (const EvictSetEntry &rhs) const { return this->level > rhs.level; }
    };

    enum AccessState
    {
        Idle,
        ReadPath,
        WriteBack,
        ReadRho,
        WriteRho,
        RhoEvictRead,
        RhoEvictWrite
    };
private:
    void updateWarmupState();
    void updateProgress();

    /**
     * Handle the request from the CPU side. Called from the CPU port
     * on a timing request.
     *
     * @param requesting packet
     * @param id of the port to send the response
     * @return true if we can handle the request this cycle, false if the
     *         requestor needs to retry later
     */
    bool handleRequest(PacketPtr pkt, int port_id);

    /**
     * Handle the respone from the memory side. Called from the memory port
     * on a timing response.
     *
     * @param responding packet
     * @return true if we can handle the response this cycle, false if the
     *         responder needs to retry later
     */
    bool handleResponse(PacketPtr pkt);

    void tryNextRequest();

    /**
     * Send the packet to the CPU side.
     * This function assumes the pkt is already a response packet and forwards
     * it to the correct port. This function also unblocks this object and
     * cleans up the whole request.
     *
     * @param the packet to send to the cpu side
     */
    void sendResponse(PacketPtr pkt);

    /**
     * Handle a packet functionally. Update the data on a write and get the
     * data on a read. Called from CPU port on a recv functional.
     *
     * @param packet to functionally handle
     */
    void handleFunctional(PacketPtr pkt);

    /**
     * Access the cache for a timing access. This is called after the cache
     * access latency has already elapsed.
     */
    void accessTiming(PacketPtr pkt);

    /**
     * This is where we actually update / read from the cache. This function
     * is executed on both timing and functional accesses.
     *
     * @return true if a hit, false otherwise
     */
    bool accessFunctional(PacketPtr pkt);

    /**
     * Insert a block into the cache. If there is no room left in the cache,
     * then this function evicts a random entry t make room for the new block.
     *
     * @param packet with the data (and address) to insert into the cache
     */
    void insert(PacketPtr pkt);

    /**
     * Return the address ranges this cache is responsible for. Just use the
     * same as the next upper level of the hierarchy.
     *
     * @return the address ranges this cache is responsible for
     */
    AddrRangeList getAddrRanges() const;

    /**
     * Tell the CPU side to ask for our memory ranges.
     */
    void sendRangeChange() const;

    // static unsigned log_binary(uint64_t num);
    // PosMapEntry generateRandomLeaf(uint64_t bucketAddr);
    bool isDescendant(const PosMapEntry &root, const PosMapEntry &node);

    void tryPathDiversion();

    System *system;
    uint64_t warmupCnt;
    bool warmup;
    uint64_t interval;
    // uint64_t progress;
    std::vector<uint64_t> progresses;

    /// Latency to check the cache. Number of cycles for both hit and miss
    const Cycles latency;

    /// The block size for the cache
    const unsigned blockSize;    

    uint64_t blockNum;
	uint64_t rhoBlkNum;
    Addr rhoBase;

     /// The bucket size
    unsigned bucketSize;
	unsigned rhoBktSize;

    /// Number of blocks in the stash (size of cache / block size)
    unsigned capacity;
	unsigned rhoCapty;

    unsigned maxLevel;
	unsigned rhoLevel;

    /// quest queue size
    unsigned queueCapacity;   

    double utilization;

    uint64_t validBlockNum;

    std::unordered_map<Addr, PosMapEntry> posMap;
	std::unordered_map<Addr, RhoTagEntry> rhoTag;
    std::list<Addr> freeRhoBlocks;

    /// Instantiation of the CPU-side port
    std::vector<CPUSidePort> cpuPorts;

    /// Instantiation of the memory-side port
    MemSidePort memPort;
	MemSidePort rhoPort;

    /// True if this cache is currently blocked waiting for a response.
    bool blocked;
    AccessState acState;

    /// Packet that we are currently handling. Used for upgrading to larger
    /// cache line sizes
    PacketPtr originalPacket;
	PacketPtr evictPkt;

    /// The port to send the response when we recieve it back
    int waitingPortId;

    /// For tracking the miss latency
    Tick missTime;

    /// An incredibly simple cache storage. Maps block addresses to data
    std::unordered_map<Addr, uint8_t*> cacheStore;
	std::unordered_map<Addr, uint8_t*> rhoStash;

    // std::queue<ReqQueueEntry> reqQueue;
    std::list<ReqQueueEntry> reqQueue;
    unsigned ageThreshold;
    
    std::priority_queue<ReqQueueEntry> schedQueue;
	std::queue<PacketPtr> pathBufferQueue;

    std::queue<uint64_t> batchPathQueue;
    std::multiset<EvictSetEntry, std::greater<EvictSetEntry>> evictSet;

    // std::queue<ReqQueueEntry> pathBufferQueue;
    unsigned currentLevel;
    unsigned currentBucket;
    unsigned cachedReads;
    PosMapEntry currentPath;
    // uint64_t currentLeaf;
	uint64_t curRhoLeaf;
    /// Cache statistics
    Stats::Scalar hits;
    Stats::Scalar misses;
	Stats::Scalar oramRequests;
    Stats::Scalar stashReads;
    Stats::Scalar stashWrites;
    Stats::Scalar stashScans;
    // Stats::Scalar savedWrites;
    // Stats::Scalar savedReads;
    unsigned qOcc;
    Stats::Scalar queueOcc;
    Stats::Histogram savedReads;
    Stats::Histogram savedReadRate;
    Stats::Scalar diversions;
    Stats::Histogram batchPaths;
    Stats::Histogram savedWrites;
    Stats::Histogram savedWriteRate;
    Stats::Histogram missLatency;
    std::unordered_map<Addr, uint64_t> reqTimes;
    Stats::Histogram reqLatency;
    Stats::Formula hitRatio;

    void posMapInit();
	void rhoTagInit();
    // void access(PacketPtr pkt);
    void startPathRead(PacketPtr pkt);
    void startPathWrite();
    std::unordered_map<Addr, uint8_t*>::const_iterator scanStashForEvict(uint64_t evictLeaf, unsigned curLevel);
    void readPath(int64_t interest, uint64_t leaf, PacketPtr pkt);
    void writePath(int64_t interest, uint64_t leaf, PacketPtr pkt);
    // void ScanCurPath(int64_t interest, uint64_t leaf);
    // void ScanStash(int64_t interest, uint64_t leaf);
	PacketPtr allocateRho(PacketPtr pkt);
    bool rhoAccess(PacketPtr pkt);
    bool accessRhoStash(PacketPtr pkt);
    void swapEvictBlock();
    void insertRhoStash(PacketPtr pkt);
    void startRhoRead(PacketPtr pkt);
    void startRhoWrite();

  public:

    /** constructor
     */
    HitchhikerR(HitchhikerRParams *params);

    /**
     * Get a port with a given name and index. This is used at
     * binding time and returns a reference to a protocol-agnostic
     * port.
     *
     * @param if_name Port name
     * @param idx Index in the case of a VectorPort
     *
     * @return A reference to the given port
     */
    Port &getPort(const std::string &if_name,
                  PortID idx=InvalidPortID) override;

    /**
     * Register the stats
     */
    void regStats() override;
};


#endif // __LEARNING_GEM5_SIMPLE_CACHE_SIMPLE_CACHE_HH__
