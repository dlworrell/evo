CC ?= cc
PKG_CONFIG ?= pkg-config
PYTHON ?= python3
BUILD_DIR ?= build

STRICT_CFLAGS = -std=c17 -Wall -Wextra -Wpedantic -Wconversion \
	-Wsign-conversion -Wshadow -Werror
EVO_CFLAGS = $(shell $(PKG_CONFIG) --cflags catalyst-evo)
EVO_LIBS = $(shell $(PKG_CONFIG) --libs --static catalyst-evo)

PROGRAMS = \
	$(BUILD_DIR)/evo_repository_scoring_adapter \
	$(BUILD_DIR)/evo_compiler_options_adapter \
	$(BUILD_DIR)/evo_scheduler_tuning_adapter \
	$(BUILD_DIR)/evo_fpga_placement_adapter

.PHONY: all validate clean

all: $(PROGRAMS)

$(BUILD_DIR)/evo_repository_scoring_adapter: \
	repository-scoring/repository_scoring.c common/reference_adapter.h
	mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(STRICT_CFLAGS) $(EVO_CFLAGS) \
		$< $(LDFLAGS) $(EVO_LIBS) $(LDLIBS) -o $@

$(BUILD_DIR)/evo_compiler_options_adapter: \
	compiler/compiler_options.c common/reference_adapter.h
	mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(STRICT_CFLAGS) $(EVO_CFLAGS) \
		$< $(LDFLAGS) $(EVO_LIBS) $(LDLIBS) -o $@

$(BUILD_DIR)/evo_scheduler_tuning_adapter: \
	scheduler/scheduler_tuning.c common/reference_adapter.h
	mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(STRICT_CFLAGS) $(EVO_CFLAGS) \
		$< $(LDFLAGS) $(EVO_LIBS) $(LDLIBS) -o $@

$(BUILD_DIR)/evo_fpga_placement_adapter: \
	fpga-placement/fpga_placement.c common/reference_adapter.h
	mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(STRICT_CFLAGS) $(EVO_CFLAGS) \
		$< $(LDFLAGS) $(EVO_LIBS) $(LDLIBS) -o $@

validate: all
	$(PYTHON) validate_reference_adapters.py \
		--repository $(BUILD_DIR)/evo_repository_scoring_adapter \
		--compiler $(BUILD_DIR)/evo_compiler_options_adapter \
		--scheduler $(BUILD_DIR)/evo_scheduler_tuning_adapter \
		--fpga $(BUILD_DIR)/evo_fpga_placement_adapter \
		--json $(BUILD_DIR)/EVO-ADAPTERS-001.json \
		--markdown $(BUILD_DIR)/EVO-ADAPTERS-001.md

clean:
	rm -f $(PROGRAMS) \
		$(BUILD_DIR)/EVO-ADAPTERS-001.json \
		$(BUILD_DIR)/EVO-ADAPTERS-001.md
