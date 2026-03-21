import csv
import sys
import json
import re
from pathlib import Path
from collections import defaultdict
import numpy as np

results_dir = Path("results")
bench_dir = Path("bench")

benches = sorted(p.stem for p in bench_dir.glob("*.c"))

def parse_results(predictor: str) -> dict[str, dict[str, int | float]]:
    results = defaultdict(dict)
    for b in benches:
        stats_path = results_dir / predictor / b / "stats.txt"
        with stats_path.open() as stats:
            for line in stats:
                match line.split()[:2]:
                    case ["board.processor.cores.core.MemDepUnit__0.insertedLoads", inserted_loads]:
                        results["insertedLoads"][b] = int(inserted_loads)
                    case ["board.processor.cores.core.MemDepUnit__0.insertedStores", inserted_stores]:
                        results["insertedStores"][b] = int(inserted_stores)
                    case ["board.processor.cores.core.MemDepUnit__0.conflictingLoads", conflicting_loads]:
                        results["conflictingLoads"][b] = int(conflicting_loads)
                    case ["board.processor.cores.core.MemDepUnit__0.conflictingStores", conflicting_stores]:
                        results["conflictingStores"][b] = int(conflicting_stores)
                    case ["board.processor.cores.core.phast0.numCommitsValid", num_commits_valid]:
                        results["numCommitsValid"][b] = int(num_commits_valid)
                    case ["board.processor.cores.core.phast0.numCommitsInvalid", num_commits_invalid]:
                        results["numCommitsInvalid"][b] = int(num_commits_invalid)
                    case ["board.processor.cores.core.phast0.numCommitsNotFound", num_commits_not_found]:
                        results["numCommitsNotFound"][b] = int(num_commits_not_found)
                    case ["board.processor.cores.core.ipc", ipc]:
                        results["ipc"][b] = float(ipc)
                    case ["board.processor.cores.core.lsq0.squashedLoads", squashed_loads]:
                        results["squashedLoads"][b] = int(squashed_loads)
                    case ["board.processor.cores.core.lsq0.squashedStores", squashed_stores]:
                        results["squashedStores"][b] = int(squashed_stores)
                    case ["board.processor.cores.core.lsq0.memOrderViolation", mem_order_violation]:
                        results["memOrderViolation"][b] = int(mem_order_violation)
                    case ["board.processor.cores.core.phast0.checkInstCalls", checkInstCalls]:
                        results["checkInstCalls"][b] = int(checkInstCalls)
                    case ["board.processor.cores.core.phast0.checkInstNoEntry", checkInstNoEntry]:
                        results["checkInstNoEntry"][b] = int(checkInstNoEntry)
                    case ["board.processor.cores.core.phast0.checkInstEntryFound", checkInstEntryFound]:
                        results["checkInstEntryFound"][b] = int(checkInstEntryFound)
                    case ["board.processor.cores.core.phast0.checkInstStoreDistInvalid", checkInstStoreDistInvalid]:
                        results["checkInstStoreDistInvalid"][b] = int(checkInstStoreDistInvalid)
                    case ["board.processor.cores.core.phast0.checkInstProducerNotInHash", checkInstProducerNotInHash]:
                        results["checkInstProducerNotInHash"][b] = int(checkInstProducerNotInHash)
                    case ["board.processor.cores.core.phast0.checkInstConfidenceZero", checkInstConfidenceZero]:
                        results["checkInstConfidenceZero"][b] = int(checkInstConfidenceZero)
                    case ["board.processor.cores.core.phast0.violationCalls", violationCalls]:
                        results["violationCalls"][b] = int(violationCalls)
                    case ["board.processor.cores.core.phast0.violationClears", violationClears]:
                        results["violationClears"][b] = int(violationClears)
                    case ["board.processor.cores.core.phast0.numFPNoForwarding", numFPNoForwarding]:
                        results["numFPNoForwarding"][b] = int(numFPNoForwarding)
                    case ["board.processor.cores.core.phast0.numFPDifferentStore", numFPDifferentStore]:
                        results["numFPDifferentStore"][b] = int(numFPDifferentStore)
                    case ["board.processor.cores.core.phast0.numCorrectPredictions", correct]:
                        results["numCorrectPredictions"][b] = int(correct)
                    case ["board.processor.cores.core.phast0.numIncorrectPredictions", incorrect]:
                        results["numIncorrectPredictions"][b] = int(incorrect)
    
    # json.dump(results, sys.stdout, indent=2)
    return results


def main():
    results: dict[str, dict[str, dict[str, int | float]]] = {}
    results["phast"] = parse_results("phast")
    results["ss"] = parse_results("ss")

    def derive_results(pred: str):
        res = results[pred]
        for b in benches:
            res["loadConflictRatio"][b] = res["conflictingLoads"][b] / res["insertedLoads"][b]
            res["storeConflictRatio"][b] = res["conflictingStores"][b] / res["insertedStores"][b]
    
    derive_results("phast")
    derive_results("ss")

    if len(sys.argv) > 1:
        for b in benches:
            if re.search(sys.argv[1], b):
                print(f"\nBenchmark: {b}")
                for metric, benchmarks in results["phast"].items():
                    val = benchmarks[b]
                    print(f"{"phast":10}{metric:30}{val}")
                for metric, benchmarks in results["ss"].items():
                    val = benchmarks[b]
                    print(f"{"ss":10}{metric:30}{val}")
    else:
        for metric in results["phast"].keys():
            print(f"\nMetric: {metric}")
            print(f"{"bench":20}, {"phast":20}, {"ss":20}")
            for b in benches:
                if isinstance(results["phast"][metric][b],float):
                    print(f"{b:20}, {results["phast"][metric][b]:<20.4f}, {results["ss"][metric][b]:<20.4f}")
                else:
                    print(f"{b:20}, {results["phast"][metric][b]:<20}, {results["ss"][metric][b]:<20}")

        # print(f"\nAverage:")
        # for metric, benchmarks in results["phast"].items():
        #     avg = np.average([x for x in benchmarks.values()])
        #     print(f"{"phast":10}{metric:30}{avg}")

        # for metric, benchmarks in results["ss"].items():
        #     avg = np.average([x for x in benchmarks.values()])
        #     print(f"{"ss":10}{metric:30}{avg}")


if __name__ == "__main__":
    main()
