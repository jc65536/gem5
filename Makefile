SRCS = $(wildcard bench/*.c)
BENCH_NAMES = $(patsubst bench/%.c,%,$(SRCS))
BENCHES = $(patsubst %,bench/%,$(BENCH_NAMES))

GEM5_CONFIG = gem5-config.py
CFLAGS = -march=x86-64

GEM5_DIR=/home/jason/projects/gem5
GEM5_BIN=$(GEM5_DIR)/build/X86/gem5.opt

DEBUGFLAGS ?=

all: $(BENCHES)

$(BENCHES): bench/%: bench/%.c bench/common.h

sim-phast-%: bench/%
	mkdir -p results/phast
	MEM_DEP_PREDICTOR=phast $(GEM5_BIN) \
		-d results/phast/$* \
		$(if $(DEBUGFLAGS),--debug-flags=$(DEBUGFLAGS)) \
		$(GEM5_CONFIG) \
		--cmd=$< \
		--options="$(ARGS)" \
		$(SIM_OPTIONS)

sim-phast-all: $(patsubst %,sim-phast-%,$(BENCH_NAMES))

sim-ss-%: bench/%
	mkdir -p results/ss
	MEM_DEP_PREDICTOR=ss $(GEM5_BIN) \
		-d results/ss/$* \
		$(if $(DEBUGFLAGS),--debug-flags=$(DEBUGFLAGS)) \
		$(GEM5_CONFIG) \
		--cmd=$< \
		--options="$(ARGS)" \
		$(SIM_OPTIONS)

sim-ss-all: $(patsubst %,sim-ss-%,$(BENCH_NAMES))

sim-all: sim-phast-all sim-ss-all
	clear
	python parse_results.py

clean:
	rm -rf $(BENCHES) results
