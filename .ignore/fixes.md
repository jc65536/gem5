## Fix 1

1. Incorrect Distance Calculation at Violation (Execution Time vs.
Dispatch Time)
The Paper: "The distance can be obtained... by calculating the
difference between the store queue indexes of the store previous to
the load and the store involved in the conflict" (i.e., the distance
strictly from the load's perspective).
The Error: In Phast::violation(), actual_store_dist is computed by
iterating backwards through recentStores starting from the current
storeIndex. Because a violation is detected during execution, the
pipeline has continued dispatching instructions, meaning storeIndex
has advanced well beyond where the load was dispatched. The
calculated actual_store_dist incorrectly includes all the stores
dispatched after the load.
Impact on Stats: The predictor learns an artificially inflated store
distance. When the load executes again, it predicts a dependency on a
much older, unrelated store. Since the load doesn't wait for the
correct store, it results in a Memory Order Violation
(memOrderViolation = 57,770) and is subsequently squashed
(squashedLoads = 310,762).

2. Broken Confidence Updates at Commit
The Paper: "When a load with a predicted distance commits, it updates
the confidence. If the load waited for the correct store, the counter
is reset to the maximum value. Otherwise, the counter is
decremented."
The Error: Phast::updateConfidence() checks if a prediction was
correct by evaluating storeDistToSeqNum(set[w].storeDist). However,
this is called at commit time. By the time a load commits, storeIndex
has advanced by thousands of instructions. storeDistToSeqNum thus
returns an entirely incorrect, much newer sequence number that will
almost never match the load's actual forwarding store.
Impact on Stats: Because the predictor always evaluates correct
predictions as "incorrect" at commit time, the confidence counter is
immediately slashed to zero. Once confidence is zero, the predictor
ignores the entry entirely. PHAST is effectively suffering from
amnesia—constantly learning dependencies and immediately unlearning
them at commit. This causes loads to be repeatedly squashed and
re-executed, heavily inflating the Inserted Loads metric
(insertedLoads = 456,652 vs 160,663 for Store Sets).

3. Missing "+1" in the Branch History Length
The Paper: "The predictor is trained with a history length
representative of that dependence, namely N + 1 divergent branches
older than the load, where N is the number of divergent branches
between the load and the store."
The Error: In Phast::violation(), hist_len is calculated as
load_inst->phastDecodeBranchCount -
store_inst->phastDecodeBranchCount, which equals exactly N. The code
never adds the crucial + 1 branch.
Impact on Stats: According to the paper (Section III-B), adding the
divergent branch prior to the store is strictly necessary to
differentiate paths that would otherwise alias to the same distance
of 0. Missing this branch causes path aliasing, resulting in
additional false negatives (more squashes/violations) and false
positives.

## Fix 2

1. Total History Wipeout on Branch Mispredictions
When a branch mispredicted, commit.cc attempted to restore the global divergent branch
counter using the phastDecodeBranchCount of the mispredicted branch.
 * The Error: In decode.cc, phastDecodeBranchCount was only ever captured for loads,
   stores, and atomics. Branches defaulted to 0.
 * The Consequence: Every time a branch misprediction occurred, the entire global branch
   history was reset to 0, permanently wiping all learned paths from the predictor's
   memory. Furthermore, because the mispredicted branch had already executed correctly,
   it was not re-fetched, meaning its correct outcome was never added back to the
   history.


2. History Bloat on Memory Order Violations
When a load executes prematurely, it causes a memory order violation, forcing the
pipeline to squash the violator load and all younger instructions. The violator load is
then re-fetched and re-executed.
 * The Error: commit.cc contained absolutely no logic to restore the branch history when
   a memory order violation triggered a squash.
 * The Consequence: Any speculatively decoded branches that occurred after the squashed
   load remained permanently in the global history array. When the squashed load was
   re-fetched, it saw a completely shifted, corrupted branch history containing future
   speculative branches.


Why this caused your specific stats:
Because the branch history was constantly being zeroed out or randomly shifted, the
predictor effectively functioned on random noise rather than actual program paths.
 * memOrderViolation & squashedLoads (8,615 vs 82): Because the predictor's history
   context was scrambled, it could not recognize recurring paths. It repeatedly suffered
   false negatives, predicting no dependency for loads that genuinely needed to wait.
   The load would then overtake the store, trigger a memory order violation, and get
   squashed.
 * insertedLoads (268,261 vs 160,663): This metric counts how many times a load is
   dispatched. Because your loads were constantly causing memory order violations and
   being squashed, they were repeatedly re-dispatched into the pipeline over and over
   again, artificially inflating the total number of load insertions.


The Fix
I have successfully implemented the fixes to resolve this:
 1. decode.cc: We now snapshot the phastDecodeBranchCount for control instructions as
    well.
 2. commit.cc (Branch Mispredicts): We now correctly rewind the history to precisely
    before the mispredicted branch, and then immediately re-record that branch using its
    actual, committed outcome and target.
 3. commit.cc (Memory Violations): Since IEW doesn't directly pass the violating
    instruction pointer for memory violations (it passes NULL and just the sequence
    number), we now use rob->findInst to look up the exact squashed load instruction in
    the Reorder Buffer. This allows us to retrieve its original phastDecodeBranchCount
    and accurately rewind the history to the state prior to the load.
