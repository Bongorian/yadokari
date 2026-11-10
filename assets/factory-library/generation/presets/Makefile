# MK3 signature preset pack — host tools.
# Use the current repository codec; historical source provenance is retained
# in vendor/mk3/PROVENANCE.md and Git, not a second live engine registry.
REPO_ROOT := $(abspath ../../../..)
CODEC_SUPPORT := $(REPO_ROOT)/build/ui-tool.a
CXX ?= g++
CXXFLAGS ?= -std=c++17 -O1 -Wall -DGROOVE_PLAITS -DTEST
CXXFLAGS += -I$(REPO_ROOT)/src

$(CODEC_SUPPORT):
	$(MAKE) -C $(REPO_ROOT) build/ui-tool.a

.PHONY: all gen verify gen-sampler verify-sampler clean
all: tools/packgen

tools/packgen: tools/packgen.cpp $(CODEC_SUPPORT) $(wildcard $(REPO_ROOT)/src/*.h) $(wildcard $(REPO_ROOT)/src/ui/*.h)
	$(CXX) $(CXXFLAGS) tools/packgen.cpp $(CODEC_SUPPORT) -o $@

# MGXC v12 regeneration: outputs go to the versioned v12 directories;
# the v11 out/ and out-sampler/ trees are kept untouched for comparison.
gen: tools/packgen
	mkdir -p out-v12
	tools/packgen gen tools/presets.json out-v12 preset-guids.json

verify: tools/packgen
	tools/packgen verify tools/presets.json out-v12 preset-guids.json

gen-sampler: tools/packgen
	mkdir -p out-sampler-v12
	tools/packgen gen tools/sampler-presets.json out-sampler-v12 preset-guids.json

verify-sampler: tools/packgen
	tools/packgen verify tools/sampler-presets.json out-sampler-v12 preset-guids.json

clean:
	rm -f tools/packgen

# MGXC12 factory submission: masters whose device placement is
# FACTORY/PRESETS/LIBRARY/<file> (asset paths stored for that base).
gen-factory: tools/packgen
	mkdir -p out-v12-factory
	tools/packgen gen tools/presets.json out-v12-factory preset-guids.json factory

verify-factory: tools/packgen
	tools/packgen verify tools/presets.json out-v12-factory preset-guids.json factory

inspect: tools/inspect
tools/inspect: tools/inspect.cpp $(CODEC_SUPPORT) $(wildcard $(REPO_ROOT)/src/*.h) $(wildcard $(REPO_ROOT)/src/ui/*.h)
	$(CXX) $(CXXFLAGS) tools/inspect.cpp $(CODEC_SUPPORT) -o $@
