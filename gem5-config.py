"""
Script with a specialized O3 CPU
IS takes about 2-3 minutes with this script

Run with `gem5 03-processor.py`
"""

from gem5.components.boards.simple_board import SimpleBoard
from gem5.components.cachehierarchies.classic.private_l1_shared_l2_cache_hierarchy import (
    PrivateL1SharedL2CacheHierarchy,
)
from gem5.components.memory.single_channel import SingleChannelDDR4_2400
from gem5.simulate.simulator import Simulator
from gem5.isas import ISA

from gem5.components.processors.base_cpu_core import BaseCPUCore
from gem5.components.processors.base_cpu_processor import BaseCPUProcessor

from m5.objects import X86O3CPU
from m5.objects import LRURP

from gem5.resources.resource import BinaryResource

import argparse
import shlex
from m5 import stats as m5_stats
parser = argparse.ArgumentParser(description="Parse arguments for gem5 O3 config")

parser.add_argument('--cmd', type=str, required=True, help='Command to run')
parser.add_argument('--options', type=str, default="", help='Program options')
parser.add_argument('--run-insts', type=int, default=0, help='Run length: detailed phase if switching, else total run length (0 = run to completion)')
parser.add_argument('--l1d-size', type=str, default="64KiB", help='L1D cache size')
parser.add_argument('--l2-size', type=str, default="2MiB", help='L2 cache size')
parser.add_argument('--l1d-assoc', type=int, default=8, help='L1D associativity')
parser.add_argument('--l2-assoc', type=int, default=8, help='L2 associativity')
parser.add_argument('--mem-size', type=str, default="16GB", help='Main memory size')
parser.add_argument('--clk-freq', type=str, default="2GHz", help='System clock frequency')
args = parser.parse_args()


class MyOutOfOrderCore(BaseCPUCore):
    def __init__(self, width, rob_size, num_int_regs, num_fp_regs, core_id=0):
        super().__init__(X86O3CPU(), ISA.X86)
        self.core.cpu_id = core_id
        self.core.fetchWidth = width
        self.core.decodeWidth = width
        self.core.renameWidth = width
        self.core.issueWidth = width
        self.core.wbWidth = width
        self.core.commitWidth = width

        self.core.numROBEntries = rob_size

        self.core.numPhysIntRegs = num_int_regs
        self.core.numPhysFloatRegs = num_fp_regs

        self.core.LQEntries = 128
        self.core.SQEntries = 128

class MyOutOfOrderProcessor(BaseCPUProcessor):
    def __init__(self, width, rob_size, num_int_regs, num_fp_regs):
        """
        :param width: sets the width of fetch, decode, raname, issue, wb, and
        commit stages.
        :param rob_size: determine the number of entries in the reorder buffer.
        :param num_int_regs: determines the size of the integer register file.
        :param num_int_regs: determines the size of the vector/floating point
        register file.
        """
        cores = [MyOutOfOrderCore(width, rob_size, num_int_regs, num_fp_regs, core_id=0)]
        super().__init__(cores)

main_memory = SingleChannelDDR4_2400(size=args.mem_size)

class CustomPrivateL1SharedL2CacheHierarchy(PrivateL1SharedL2CacheHierarchy):
    def incorporate_cache(self, board):
        super().incorporate_cache(board)
        # Keep L1I fixed at LRU regardless of the selected data-cache policy.
        for c in getattr(self, "l1icaches", []):
            c.replacement_policy = LRURP()
        # Apply replacement policy to L1/L2 caches after creation.
        for c in getattr(self, "l1dcaches", []):
            c.replacement_policy = LRURP()
        if hasattr(self, "l2cache"):
            self.l2cache.replacement_policy = LRURP()


caches = CustomPrivateL1SharedL2CacheHierarchy(
    l1d_size=args.l1d_size,
    l1d_assoc=args.l1d_assoc,
    l1i_size="32KiB",
    l1i_assoc=8,
    l2_size=args.l2_size,
    l2_assoc=args.l2_assoc
)

print("--------------------------------------------------------------------")
print(vars(caches))

def build_ooo_processor():
    return MyOutOfOrderProcessor(
        width=8, rob_size=192, num_int_regs=256, num_fp_regs=256
    )

my_processor = build_ooo_processor()

board = SimpleBoard(
    processor=my_processor,
    memory=main_memory,
    cache_hierarchy=caches,
    clk_freq=args.clk_freq,
)

binary = BinaryResource(local_path=args.cmd)
options = shlex.split(args.options) if args.options else []

board.set_se_binary_workload(
    binary,
    arguments=options,
)

simulator = None

def _run_simulation():
    global simulator
    simulator = Simulator(board)
    if args.run_insts and args.run_insts > 0:
        simulator.schedule_max_insts(args.run_insts)

    simulator.run()
    # Ensure detailed-phase stats are dumped at end (after any reset at switch).
    m5_stats.dump()

_run_simulation()
