/*
 * Copyright (c) 2004-2005 The Regents of The University of Michigan
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
 */

#ifndef __CPU_O3_PHAST_HH__
#define __CPU_O3_PHAST_HH__

#include <list>
#include <map>
#include <string_view>
#include <utility>
#include <vector>
#include <unordered_map>

#include "base/statistics.hh"
#include "base/cache/associative_cache.hh"
#include "base/cache/cache_entry.hh"
#include "base/named.hh"
#include "base/types.hh"
#include "cpu/inst_seq.hh"
#include "cpu/o3/dyn_inst_ptr.hh"

class BaseIndexingPolicy;

namespace replacement_policy {
class Base;
}

namespace gem5
{

namespace o3
{

class CPU;

struct PhastEntry
{
    uint16_t tag;
    uint8_t storeDist;
    uint8_t confidence;
    uint8_t lru;
    bool valid;

    PhastEntry() : tag(0), storeDist(0), confidence(0), lru(0), valid(false) {}
};

struct PhastTable
{
    int historyLength;
    std::vector<std::vector<PhastEntry>> sets; // sets[128][4]

    PhastTable(int hist_len) : historyLength(hist_len) {
        sets.resize(128, std::vector<PhastEntry>(4));
    }
};

/**
 * Implements the PatH-Aware STore-distance (PHAST) predictor.
 */
class Phast : public Named
{
  public:
    /** Default constructor.  init() must be called prior to use. */
    Phast() : Named("Phast"), stats(nullptr) {};

    /** Creates store set predictor with given table sizes. */
    Phast(std::string_view name, uint64_t clear_period,
             size_t SSIT_entries, int SSIT_assoc,
             replacement_policy::Base *replPolicy,
             BaseIndexingPolicy *indexingPolicy, int LFST_size);

    /** Default destructor. */
    ~Phast();

    /** Initializes the predictor. (Legacy params kept for compatibility) */
    void init(CPU *cpu_ptr, ThreadID tid, uint64_t clear_period,
              size_t SSIT_entries, int SSIT_assoc,
              replacement_policy::Base *_replPolicy,
              BaseIndexingPolicy *_indexingPolicy, int LFST_size);

    // Will also need how many read/write ports the Dcache has.  Or keep track
    // of that in stage that is one level up, and only call executeLoad/Store
    // the appropriate number of times.
    struct PhastStats : public statistics::Group
    {
        PhastStats(statistics::Group *parent);

        statistics::Scalar numCorrectPredictions;
        statistics::Scalar numIncorrectPredictions;
        statistics::Scalar numPredictionsNotFound;
    } stats;

    /** Records a memory ordering violation. */
    void violation(const DynInstPtr &store_inst, const DynInstPtr &load_inst);

    /** Periodically clears the predictor to remove false dependencies. */
    void checkClear();

    /** Inserts a load into the predictor. */
    void insertLoad(const DynInstPtr &load_inst);

    /** Inserts a store into the predictor. */
    void insertStore(const DynInstPtr &store_inst);

    /** Checks if the instruction is dependent on a store. */
    InstSeqNum checkInst(const DynInstPtr &load_inst);

    /** Records this PC/sequence number as issued. */
    void issued(Addr issued_PC, InstSeqNum issued_seq_num, bool is_store);

    /** Squashes for a specific thread until the given sequence number. */
    void squash(InstSeqNum squashed_num, ThreadID tid);

    /** Updates the confidence of a prediction on commit. */
    void updateConfidence(const DynInstPtr &load_inst);

    /** Resets all tables. */
    void clear();

    /** Debug function. */
    void dump();

    /** PHAST specific: Records a branch into history. */
    void recordBranch(bool is_indirect, bool is_taken, Addr target, InstSeqNum sn);
    
    /** PHAST specific: Gets current branch count. */
    uint64_t getBranchCount() const { return ghbCounter; }

    /** PHAST specific: Restore branch count on mispredict. */
    void squashBranches(uint64_t recovered_count);

  private:
    std::vector<PhastTable> tables;
    const std::vector<int> HISTORY_LENGTHS = {0, 2, 4, 6, 8, 12, 16, 32};
    
    static constexpr int NUM_SETS = 128;
    static constexpr int NUM_WAYS = 4;
    static constexpr int MAX_CONFIDENCE = 15;

    uint64_t ghbCounter;
    int ghbSize;
    std::vector<uint8_t> globalHistoryBuffer;
    std::vector<InstSeqNum> ghbSeqNum;

    InstSeqNum storeDistToSeqNum(const DynInstPtr &load_inst, int store_dist) const;

    uint64_t clearPeriod;
    int memOpsPred;

    uint32_t foldHistory(uint64_t load_branch_count, int hist_len) const;
    void getIndexAndTag(Addr pc, uint32_t folded_hist, int &index, uint16_t &tag) const;
    void updateLRU(std::vector<PhastEntry>& set, int accessed_way);
};

} // namespace o3

} // namespace gem5

#endif // __CPU_O3_PHAST_HH__
