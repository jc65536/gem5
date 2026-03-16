#include "cpu/o3/phast.hh"

#include <algorithm>
#include "base/intmath.hh"
#include "base/logging.hh"
#include "base/trace.hh"
#include "cpu/o3/dyn_inst.hh"
#include "debug/Phast.hh"

namespace gem5
{

namespace o3
{

Phast::Phast(std::string_view name_, uint64_t clear_period,
             size_t _SSIT_entries, int _SSIT_assoc,
             replacement_policy::Base *_replPolicy,
             BaseIndexingPolicy *_indexingPolicy, int _LFST_size)
  : Named(name_),
    globalDivergentBranchCounter(0),
    globalHistoryBuffer(512, 0),
    recentStores(1024, 0),
    storeIndex(0),
    clearPeriod(clear_period),
    memOpsPred(0)
{
    DPRINTF(Phast, "Phast: Creating PHAST object.\n");
    for (int len : HISTORY_LENGTHS) {
        tables.emplace_back(len);
    }
}

Phast::~Phast()
{
}

void
Phast::init(uint64_t clear_period, size_t _SSIT_entries,
            int _SSIT_assoc, replacement_policy::Base *_replPolicy,
            BaseIndexingPolicy *_indexingPolicy, int _LFST_size)
{
    clearPeriod = clear_period;
    memOpsPred = 0;
    globalDivergentBranchCounter = 0;
    globalHistoryBuffer.assign(512, 0);
    recentStores.assign(1024, 0);
    storeIndex = 0;
    
    tables.clear();
    for (int len : HISTORY_LENGTHS) {
        tables.emplace_back(len);
    }
}

void
Phast::recordBranch(bool is_indirect, bool is_taken, Addr target)
{
    uint8_t hist_entry = 0;
    if (is_indirect) hist_entry |= (1 << 6);
    if (is_taken) hist_entry |= (1 << 5);
    uint8_t target_bits = (target >> 2) & 0x1F;
    hist_entry |= target_bits;

    globalHistoryBuffer[globalDivergentBranchCounter % 512] = hist_entry;
    globalDivergentBranchCounter++;
}

uint32_t
Phast::foldHistory(uint64_t load_branch_count, int hist_len) const
{
    uint32_t folded = 0;
    for (int i = 0; i < hist_len; ++i) {
        if (load_branch_count <= i) break;
        uint64_t idx = load_branch_count - 1 - i;
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
    int actual_store_dist = -1;
    for (int i = 0; i < recentStores.size(); ++i) {
        if (storeIndex <= i) break;
        if (storeDistToSeqNum(i) == store_inst->seqNum) {
            actual_store_dist = i;
            break;
        }
    }

    if (actual_store_dist == -1) return; // Store too old, fallen off recent stores
    if (actual_store_dist > 127) actual_store_dist = 127; // Max 7-bit distance

    int hist_len = 0;
    if (load_inst->phastDecodeBranchCount > store_inst->phastDecodeBranchCount) {
        hist_len = load_inst->phastDecodeBranchCount - store_inst->phastDecodeBranchCount;
    }

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
    recentStores[storeIndex % recentStores.size()] = store_inst->seqNum;
    storeIndex++;
}

InstSeqNum
Phast::checkInst(const DynInstPtr &load_inst)
{
    int best_store_dist = -1;
    int best_hist_len = -1;
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
                }
                updateLRU(set, w); 
                break;
            }
        }
    }

    if (best_store_dist >= 0) {
        load_inst->predictedEntrySetPtr = best_set;
        load_inst->predictedWayInSet = best_way;
        return storeDistToSeqNum(best_store_dist);
    } else {
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
    while (storeIndex > 0) {
        if (storeDistToSeqNum(0) > squashed_num) {
            storeIndex--;
        } else {
            break;
        }
    }
}

void
Phast::updateConfidence(const DynInstPtr &load_inst)
{
    if (!load_inst->predictedEntrySetPtr || load_inst->predictedWayInSet < 0) {
        return;
    }

    std::vector<PhastEntry> &set = *(load_inst->predictedEntrySetPtr);
    int w = load_inst->predictedWayInSet;

    if (set[w].valid) {
        InstSeqNum predicted_forwarding_store_seq_num = storeDistToSeqNum(set[w].storeDist);
        bool is_correct = load_inst->forwardingStoreSeqNum == predicted_forwarding_store_seq_num;
        if (is_correct) {
            set[w].confidence = MAX_CONFIDENCE;
        } else if (set[w].confidence > 0) {
            set[w].confidence--;
        }
        updateLRU(set, w);
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

InstSeqNum Phast::storeDistToSeqNum(int store_dist) const {
    return recentStores[(storeIndex - 1 - store_dist) % recentStores.size()];
}

} // namespace o3
} // namespace gem5
