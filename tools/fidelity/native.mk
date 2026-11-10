# A separate pristine build is the oracle, not a second adapter instance.
NATIVE_FLAGS = -std=c++17 -O2 -DTEST -ffp-contract=off -Wno-unused-local-typedefs
RING_NAMES = rings/dsp/part.cc rings/dsp/string.cc rings/dsp/resonator.cc rings/dsp/fm_voice.cc rings/resources.cc stmlib/dsp/units.cc stmlib/utils/random.cc
build/fidelity/resonator-reference: tools/fidelity/resonator_render.cpp
	@mkdir -p $(@D)
	$(CXX) $(NATIVE_FLAGS) -I.deps/pristine-mutable $< $(addprefix .deps/pristine-mutable/,$(RING_NAMES)) -o $@
build/fidelity/resonator-adapter: tools/fidelity/resonator_render.cpp src/resonator_dsp.h src/resonator_dsp.cpp
	@mkdir -p $(@D)
	$(CXX) $(NATIVE_FLAGS) -DGROOVE_ADAPTER -Isrc -Ithird_party/mutable $< src/resonator_dsp.cpp $(addprefix third_party/mutable/,$(RING_NAMES)) -o $@
.PHONY: resonator-fidelity
resonator-fidelity: build/fidelity/resonator-reference build/fidelity/resonator-adapter
	./build/fidelity/resonator-reference build/fidelity/resonator-reference.f32
	./build/fidelity/resonator-adapter build/fidelity/resonator-adapter.f32
	python3 tools/fidelity/compare.py resonator --rate 48000 --frames-per-case 24000
CLOUD_NAMES = stmlib/dsp/atan.cc clouds/dsp/mu_law.cc clouds/dsp/pvoc/frame_transformation.cc clouds/dsp/pvoc/phase_vocoder.cc clouds/dsp/pvoc/stft.cc clouds/resources.cc stmlib/dsp/units.cc stmlib/utils/random.cc
build/fidelity/texture-reference: tools/fidelity/texture_render.cpp build/fidelity/reference-correlator.cc build/fidelity/reference-processor.cc build/fidelity/reference-port/clouds/dsp/looping_sample_player.h
	@mkdir -p $(@D)
	$(CXX) $(NATIVE_FLAGS) -Ibuild/fidelity/reference-port -I.deps/pristine-mutable $< build/fidelity/reference-correlator.cc build/fidelity/reference-processor.cc $(addprefix .deps/pristine-mutable/,$(CLOUD_NAMES)) -o $@
build/fidelity/texture-adapter: tools/fidelity/texture_render.cpp src/texture_dsp.h src/texture_dsp.cpp build/mutable-port/correlator.cc build/mutable-port/granular_processor.cc build/mutable-port/clouds/dsp/looping_sample_player.h
	@mkdir -p $(@D)
	$(CXX) $(NATIVE_FLAGS) -DGROOVE_ADAPTER -Isrc -Ibuild/mutable-port -Ithird_party/mutable $< src/texture_dsp.cpp build/mutable-port/correlator.cc build/mutable-port/granular_processor.cc $(addprefix third_party/mutable/,$(CLOUD_NAMES)) -o $@
.PHONY: texture-fidelity
texture-fidelity: build/fidelity/texture-reference build/fidelity/texture-adapter build/fidelity/texture-pristine build/fidelity/texture-shift-reference
	python3 tools/fidelity/texture_inputs.py
	./build/fidelity/texture-pristine build/fidelity/texture-pristine.f32
	./build/fidelity/texture-shift-reference build/fidelity/texture-shift-reference.f32
	./build/fidelity/texture-reference build/fidelity/texture-reference.f32
	./build/fidelity/texture-adapter build/fidelity/texture-adapter.f32
	python3 tools/fidelity/compare.py texture --rate 32000 --frames-per-case 128000 --raw-reference build/fidelity/texture-pristine.f32 --cpu-reference build/fidelity/texture-shift-reference.f32

build/fidelity/reference-correlator.cc: .deps/pristine-mutable/clouds/dsp/correlator.cc tools/mutable/portable_correlator.py
	python3 tools/mutable/portable_correlator.py $< $@
build/mutable-port/correlator.cc: third_party/mutable/clouds/dsp/correlator.cc tools/mutable/portable_correlator.py
	python3 tools/mutable/portable_correlator.py $< $@

build/fidelity/reference-port/clouds/dsp/looping_sample_player.h: .deps/pristine-mutable/clouds/dsp/looping_sample_player.h tools/mutable/portable_loop.py
	python3 tools/mutable/portable_loop.py $< $@
build/mutable-port/clouds/dsp/looping_sample_player.h: third_party/mutable/clouds/dsp/looping_sample_player.h tools/mutable/portable_loop.py
	python3 tools/mutable/portable_loop.py $< $@

build/fidelity/reference-processor.cc: .deps/pristine-mutable/clouds/dsp/granular_processor.cc tools/mutable/portable_processor.py
	python3 tools/mutable/portable_processor.py $< $@
build/mutable-port/granular_processor.cc: third_party/mutable/clouds/dsp/granular_processor.cc tools/mutable/portable_processor.py
	python3 tools/mutable/portable_processor.py $< $@

# Unmodified upstream host baseline has known UB; keep its result distinct from
# the defined Cortex-M semantics oracle and the sanitized Engine verification.
build/fidelity/texture-pristine: tools/fidelity/texture_render.cpp
	@mkdir -p $(@D)
	$(CXX) $(NATIVE_FLAGS) -I.deps/pristine-mutable $< .deps/pristine-mutable/clouds/dsp/correlator.cc .deps/pristine-mutable/clouds/dsp/granular_processor.cc $(addprefix .deps/pristine-mutable/,$(CLOUD_NAMES)) -o $@

# A source pin change invalidates all independent oracle and adapter binaries.
build/fidelity/resonator-reference build/fidelity/resonator-adapter build/fidelity/texture-reference build/fidelity/texture-adapter build/fidelity/texture-pristine: third_party/mutable/resonator-texture-sources.json tools/fidelity/setup_reference.py

# Isolate the only arithmetic CPU-semantics correction from endpoint safety ports.
build/fidelity/texture-shift-reference: tools/fidelity/texture_render.cpp build/fidelity/reference-correlator.cc third_party/mutable/resonator-texture-sources.json
	$(CXX) $(NATIVE_FLAGS) -I.deps/pristine-mutable $< build/fidelity/reference-correlator.cc .deps/pristine-mutable/clouds/dsp/granular_processor.cc $(addprefix .deps/pristine-mutable/,$(CLOUD_NAMES)) -o $@
