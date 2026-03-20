#include "cpu/o3/phast.hh"

#include <algorithm>
#include "base/intmath.hh"
#include "base/logging.hh"
#include "base/trace.hh"
#include "cpu/o3/dyn_inst.hh"
#include "debug/Phast.hh"
#include "cpu/o3/cpu.hh"

namespace gem5
{

namespace o3
{

Phast::Phast(std::string_view name_, uint64_t clear_period,
             size_t _SSIT_entries, int _SSIT_assoc,
             replacement_policy::Base *_replPolicy,
             BaseIndexingPolicy *_indexingPolicy, int _LFST_size)
  : Named(name_),
    ghbCounter(0),
    globalHistoryBuffer(512, 0),
    ghbSeqNum(512, 0),
    clearPeriod(clear_period),
    memOpsPred(0),
    stats(nullptr)
{
    for (int len : HISTORY_LENGTHS) {
        tables.emplace_back(len);
    }
}

Phast::~Phast()
{
}

void
Phast::init(CPU *cpu_ptr, ThreadID tid, uint64_t clear_period, size_t _SSIT_entries,
            int _SSIT_assoc, replacement_policy::Base *_replPolicy,
            BaseIndexingPolicy *_indexingPolicy, int _LFST_size)
{
    clearPeriod = clear_period;
    memOpsPred = 0;
    ghbCounter = 0;
    ghbSize = 0;
    globalHistoryBuffer.assign(512, 0);
    ghbSeqNum.assign(512, 0);

    cpu_ptr->addStatGroup(csprintf("phast%i", tid).c_str(), &stats);
    
    tables.clear();
    for (int len : HISTORY_LENGTHS) {
        tables.emplace_back(len);
    }

    DPRINTF(Phast, "init: clearPeriod = %d\n", clearPeriod);
}

void
Phast::recordBranch(bool is_indirect, bool is_taken, Addr target, InstSeqNum sn)
{
    uint8_t hist_entry = 0;
    if (is_indirect) hist_entry |= (1 << 6);
    if (is_taken) hist_entry |= (1 << 5);
    uint8_t target_bits = (target >> 2) & 0x1F;
    hist_entry |= target_bits;

    globalHistoryBuffer[ghbCounter % 512] = hist_entry;
    ghbSeqNum[ghbCounter % 512] = sn;
    ghbCounter++;
    ghbSize = std::min(ghbSize + 1, 512);
}

void Phast::squashBranches(uint64_t recovered_count) {
    ghbSize = std::max(ghbSize - int(ghbCounter - recovered_count), 0);
    DPRINTF(Phast, "squashBranches: ghbCounter = %d, recovered_count = %d, ghbSize after = %d\n", ghbCounter, recovered_count, ghbSize);
    ghbCounter = recovered_count;
}

uint32_t
Phast::foldHistory(uint64_t load_branch_count, int hist_len) const
{
    hist_len += 1;
    uint32_t folded = 0;
    for (int i = 0; i < hist_len; ++i) {
        if (load_branch_count <= i) break;
        uint64_t idx = load_branch_count - 1 - i;
        if (idx < ghbCounter - ghbSize) break;
        uint8_t branch_hist = globalHistoryBuffer[idx % 512];
        folded = (folded << 3) | (folded >> 20); // Rotate left by 3
        folded ^= branch_hist;
    }
    return folded & 0x7FFFFF; // Mask to 23 bits
}

void
Phast::getIndexAndTag(Addr pc, uint32_t folded_hist, int &index, uint16_t &tag) const
{
    Addr hash_pc_idx = pc ^ (pc >> 2) ^ (pc >> 5);
    index = (hash_pc_idx ^ folded_hist) % NUM_SETS;

    Addr hash_pc_tag = (pc >> 3) ^ (pc >> 7);
    tag = (hash_pc_tag ^ folded_hist) & 0xFFFF;
}

void
Phast::updateLRU(std::vector<PhastEntry>& set, int accessed_way)
{
    uint8_t current_lru = set[accessed_way].lru;
    if (!set[accessed_way].valid) {
        current_lru = NUM_WAYS - 1; // Max LRU
    }
    
    for (int i = 0; i < NUM_WAYS; ++i) {
        if (set[i].valid && set[i].lru < current_lru) {
            set[i].lru++;
        }
    }
    set[accessed_way].lru = 0;
}

void
Phast::violation(const DynInstPtr &store_inst, const DynInstPtr &load_inst)
{
    int actual_store_dist = 0;
    actual_store_dist = load_inst->sqIdx - store_inst->sqIdx;

    DPRINTF(Phast, "violation: store_inst sqIdx: %d\n", store_inst->sqIdx);
    DPRINTF(Phast, "violation: load_inst sqIdx: %d\n", load_inst->sqIdx);
    DPRINTF(Phast, "violation: Found actual store dist %d\n", actual_store_dist);

    if (actual_store_dist > 127) actual_store_dist = 127; // Max 7-bit distance

    int hist_len = 0;
    if (load_inst->phastDecodeBranchCount > store_inst->phastDecodeBranchCount) {
        hist_len = load_inst->phastDecodeBranchCount - store_inst->phastDecodeBranchCount;
    }
    DPRINTF(Phast, "violation: store_inst phastDecodeBranchCount %d\n", store_inst->phastDecodeBranchCount);
    DPRINTF(Phast, "violation: load_inst phastDecodeBranchCount %d\n", load_inst->phastDecodeBranchCount);
    DPRINTF(Phast, "violation: hist_len %d\n", hist_len);

    int target_table_idx = tables.size() - 1;
    for (int t = 0; t < tables.size(); ++t) {
        if (tables[t].historyLength >= hist_len) {
            target_table_idx = t;
            break;
        }
    }
    hist_len = tables[target_table_idx].historyLength;

    if (hist_len > load_inst->phastDecodeBranchCount) return;

    Addr pc = load_inst->pcState().instAddr();
    uint32_t folded = foldHistory(load_inst->phastDecodeBranchCount, hist_len);
    int index;
    uint16_t tag;
    getIndexAndTag(pc, folded, index, tag);

    auto& set = tables[target_table_idx].sets[index];

    int hit_way = -1;
    for (int w = 0; w < NUM_WAYS; ++w) {
        if (set[w].valid && set[w].tag == tag) {
            hit_way = w;
            break;
        }
    }

    if (hit_way != -1) {
        set[hit_way].storeDist = actual_store_dist;
        set[hit_way].confidence = MAX_CONFIDENCE;
        updateLRU(set, hit_way);
    } else {
        int repl_way = 0;
        int max_lru = -1;
        for (int w = 0; w < NUM_WAYS; ++w) {
            if (!set[w].valid) {
                repl_way = w;
                break;
            }
            if (set[w].lru > max_lru) {
                max_lru = set[w].lru;
                repl_way = w;
            }
        }
        set[repl_way].valid = true;
        set[repl_way].tag = tag;
        set[repl_way].storeDist = actual_store_dist;
        set[repl_way].confidence = MAX_CONFIDENCE;
        updateLRU(set, repl_way);
    }
}

void
Phast::checkClear()
{
    memOpsPred++;
    if (memOpsPred > clearPeriod) {
        DPRINTF(Phast, "Wiping predictor state\n");
        clear();
    }
}

void
Phast::insertLoad(const DynInstPtr &load_inst)
{
    checkClear();
    // Does nothing.
    return;
}

void
Phast::insertStore(const DynInstPtr &store_inst)
{
    checkClear();
}

InstSeqNum
Phast::checkInst(const DynInstPtr &load_inst)
{
    int best_store_dist = -1;
    int best_hist_len = -1;
    uint16_t best_tag = 0;
    std::vector<PhastEntry> *best_set = nullptr;
    int best_way = -1;
    Addr pc = load_inst->pcState().instAddr();
    uint64_t load_branch_count = load_inst->phastDecodeBranchCount;

    for (int t = 0; t < tables.size(); ++t) {
        int hist_len = tables[t].historyLength;
        if (hist_len > load_branch_count) continue; 

        uint32_t folded = foldHistory(load_branch_count, hist_len);
        int index;
        uint16_t tag;
        getIndexAndTag(pc, folded, index, tag);

        auto& set = tables[t].sets[index];
        for (int w = 0; w < NUM_WAYS; ++w) {
            if (set[w].valid && set[w].tag == tag && set[w].confidence > 0) {
                if (hist_len > best_hist_len) {
                    best_hist_len = hist_len;
                    best_store_dist = set[w].storeDist;
                    best_set = &set;
                    best_way = w;
                    best_tag = tag;
                }
                updateLRU(set, w); 
                break;
            }
        }
    }

    InstSeqNum predicted_sn = 0;

    if (best_store_dist >= 0) {
        predicted_sn = storeDistToSeqNum(load_inst, best_store_dist);
    }

    if (predicted_sn) {
        DPRINTF(Phast, "checkInst: [%d] yes dependence, best_store_dist = %d, best_hist_len = %d, predicted_sn = %d\n",
            load_inst->seqNum,
            best_store_dist,
            best_hist_len,
            predicted_sn);

        load_inst->predictedEntrySetPtr = best_set;
        load_inst->predictedWayInSet = best_way;
        load_inst->predictedTag = best_tag;
        load_inst->predictedStoreSeqNum = predicted_sn;
        load_inst->predictedStoreDist = best_store_dist;
        return predicted_sn;
    } else {
        load_inst->predictedEntrySetPtr = nullptr;
        load_inst->predictedWayInSet = -1;
        load_inst->predictedStoreSeqNum = 0;
        load_inst->predictedStoreDist = -1;
        load_inst->predictedTag = 0;
        DPRINTF(Phast, "checkInst: [%d] no dependence, best_store_dist = %d, best_hist_len = %d\n", load_inst->seqNum, best_store_dist, best_hist_len);
        return 0; // 0 means no dependence
    }
}

void
Phast::issued(Addr issued_PC, InstSeqNum issued_seq_num, bool is_store)
{
}

void
Phast::squash(InstSeqNum squashed_num, ThreadID tid)
{
    int squash_amt = 0;
    for (; squash_amt < ghbSize; squash_amt++) {
        if (ghbSeqNum[ghbCounter - squash_amt - 1] <= squashed_num) {
            if (squash_amt > 0) {
                squash_amt--;
            }
            break;
        }
    }

    if (squash_amt > 0) {
        squashBranches(ghbCounter - squash_amt);
    }
}

void
Phast::updateConfidence(const DynInstPtr &load_inst)
{
    if (!load_inst->predictedEntrySetPtr || load_inst->predictedWayInSet < 0 || load_inst->predictedStoreSeqNum == 0) {
        // No dependence predicted
        bool is_correct = load_inst->forwardingStoreSeqNum == load_inst->predictedStoreSeqNum;
        if (is_correct) {
            DPRINTF(Phast, "updateConfidence: [%d] TN prediction committed predictedStoreDist = %d\n", load_inst->seqNum, load_inst->predictedStoreDist);
            stats.numCorrectPredictions++;
        } else {
            DPRINTF(Phast, "updateConfidence: [%d] FN prediction committed predictedStoreDist = %d\n", load_inst->seqNum, load_inst->predictedStoreDist);
            stats.numIncorrectPredictions++;
        }
        return;
    }

    std::vector<PhastEntry> &set = *(load_inst->predictedEntrySetPtr);
    int w = load_inst->predictedWayInSet;

    if (set[w].valid && set[w].tag == load_inst->predictedTag) {
        InstSeqNum predicted_forwarding_store_seq_num = load_inst->predictedStoreSeqNum;
        bool is_correct = load_inst->forwardingStoreSeqNum == predicted_forwarding_store_seq_num;
        if (is_correct) {
            set[w].confidence = MAX_CONFIDENCE;
            stats.numCorrectPredictions++;
            DPRINTF(Phast, "updateConfidence: [%d] TP prediction committed predictedStoreDist = %d\n", load_inst->seqNum, load_inst->predictedStoreDist);
        } else if (set[w].confidence > 0) {
            set[w].confidence--;
            stats.numIncorrectPredictions++;
            DPRINTF(Phast, "updateConfidence: [%d] FP prediction committed predictedStoreDist = %d\n", load_inst->seqNum, load_inst->predictedStoreDist);
        }
        updateLRU(set, w);
    } else {
        stats.numPredictionsNotFound++;
        DPRINTF(Phast, "updateConfidence: [%d] committed prediction not found\n", load_inst->seqNum);
    }
}

void
Phast::clear()
{
    for (auto& table : tables) {
        for (auto& set : table.sets) {
            for (auto& entry : set) {
                entry.valid = false;
                entry.confidence = 0;
            }
        }
    }
    memOpsPred = 0;
}

void
Phast::dump()
{
}

InstSeqNum Phast::storeDistToSeqNum(const DynInstPtr &load_inst, int store_dist) const {
    auto sqIt = load_inst->sqIt;
    sqIt._idx -= store_dist;
    if (sqIt.dereferenceable()) {
        DPRINTF(Phast, "storeDistToSeqNum: store_dist = %d before sqIdx = %d is valid\n", store_dist, load_inst->sqIdx);
        return sqIt->instruction()->seqNum;
    } else {
        DPRINTF(Phast, "storeDistToSeqNum: store_dist = %d before sqIdx = %d is invalid\n", store_dist, load_inst->sqIdx);
        return 0;
    }
}

Phast::PhastStats::PhastStats(statistics::Group *parent)
    : statistics::Group(parent),
      ADD_STAT(numCorrectPredictions, statistics::units::Count::get(),
               "Number of predictions that were correct"),
      ADD_STAT(numIncorrectPredictions, statistics::units::Count::get(),
               "Number of predictions that were incorrect"),
      ADD_STAT(numPredictionsNotFound, statistics::units::Count::get(),
               "Number of predictions that were not found")
{
}

} // namespace o3
} // namespace gem5
