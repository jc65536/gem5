```
scons build/X86/gem5.opt -j12
make sim-all -j10
python parse_results.py > bench/results.txt
```
