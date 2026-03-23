✦ Based on the provided paper, PHAST (PatH-Aware STore-distance) is a highly
  accurate, context-sensitive memory dependence predictor designed to prevent
  memory order violations without incurring unnecessary delays from false
  dependencies.


  Here is a breakdown of its core insights, components, and how it functions.


  Key Insights Behind PHAST
  PHAST is built on two primary observations that challenge previous predictor
  designs (like Store Sets or MDP-TAGE):
   1. Single-Store Dependence: Each time a load executes, it almost always
      depends on at most one store (the youngest conflicting store). Therefore,
      predicting a group or set of stores is unnecessary and leads to false
      dependencies.
   2. Minimal Path Context: The only context information required for precise
      dependence prediction is the execution path strictly between the
      conflicting store and the dependent load, plus the divergent branch
      immediately preceding the store. Including older branch history adds
      noise, causes aliasing, and pollutes the prediction tables.

  ---

  Components of PHAST

  To achieve its cost-effective implementation, PHAST utilizes several key
  hardware components:


  1. Global Divergent Branch Counter
   * Purpose: Calculates the exact "distance" (in terms of branches) between a
     conflicting store and a load to determine the optimal history length for
     training.
   * How it works: A global register tracks an ever-increasing count of decoded
     conditional and indirect branches. When a store or load decodes, it
     records this value. The difference between their values gives the exact
     number of divergent branches between them.


  2. Global History Register (GHR)
   * Purpose: Stores the actual execution path taken, which serves as the
     context signature for predictions.
   * How it works: It records the outcomes of divergent branches
     (taken/not-taken for conditional branches, and the 5 least significant
     bits of the destination target for indirect branches).


  3. Parallel Prediction Tables
   * Purpose: Stores and retrieves the predicted memory dependencies based on
     different possible history lengths.
   * How it works: PHAST uses a set of independent tables structured somewhat
     like a TAGE branch predictor. For its cost-effective implementation, it
     uses 8 tables corresponding to 8 geometric-like history lengths (e.g., 0,
     2, 4, 6, 8, 12, 16, and 32 branches). Each entry in these tables contains:
     * Tag (16-bit): Resolves aliasing to ensure the load and history path
       exactly match.
     * Store Distance (7-bit): The actual prediction—how many stores back in
       the Store Queue (SQ) the load needs to wait for.
     * Confidence Counter (4-bit): Discards predictions with low confidence due
       to aliasing.
     * LRU field (2-bit): Used to evict older entries when a table set is full.

  ---

  How PHAST Works

  PHAST operates across the lifecycle of load instructions—from prediction at
  decode, to execution, and finally updating at the commit stage.


  1. Predicting Dependencies (Decode Stage)
  When a load instruction is decoded, its Program Counter (PC) and compressed
  versions of the global history are used to hash and access the 8 parallel
  prediction tables simultaneously.
   * If multiple tables report a match with a confidence greater than zero,
     PHAST selects the prediction from the table with the longest history
     length.
   * If there is no match or the confidence is zero, PHAST predicts no
     dependence, allowing the load to execute speculatively.


  2. Propagating Dependencies (Allocation Stage)
  If a dependence is predicted, the load must wait for the conflicting store
  before executing. When the load is allocated into the Load Queue (LQ), it
  subtracts the predicted Store Distance from the index of the most recently
  added store in the Store Queue (SQ). This identifies the exact older store
  the load must wait for.


  3. Detecting Conflicts & Updating (Commit Stage)
  If the predictor was wrong and a load executes speculatively but is later
  found to conflict with an older store, a memory order violation occurs. PHAST
  uses lazy updating (waiting until the commit stage) to train the predictor:
   * Using the Global Divergent Branch Counter, it calculates the exact history
     length (let's call it $N$) between the conflicting store and the load.
   * PHAST then uses the history of those $N+1$ divergent branches to allocate
     a new entry strictly in the specific prediction table that handles that
     history length.
   * In that new entry, the actual Store Distance is recorded, and the
     Confidence Counter is set to maximum.


  4. Confidence Updates (Commit Stage)
  Every time a load with a predicted distance commits, it updates its table
  entry. If the load successfully waited for the correct store, the confidence
  counter is reset to its maximum value. If it waited for the wrong store (or
  didn't need to wait), the counter is decremented.


  Summary
  By determining the exact minimum history length dynamically upon a conflict,
  PHAST trains incredibly efficiently. Unlike older predictors that guess
  history lengths via brute-force (allocating multiple entries across tables)
  or use fixed lengths (causing massive false dependencies), PHAST surgically
  allocates one entry at the optimal history length. This allows it to achieve
  near-ideal performance with a very small hardware footprint (14.5KB).
