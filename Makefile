UI_BASE_ARCHIVE := build/ui-base.a
UI_TOOL_ARCHIVE := build/ui-tool.a
include src/engine_sources.mk
ENGINE_TOOL_OBJECTS := $(addprefix build/engine-tool/,$(addsuffix .o,$(ENGINE_NAMES)))
MSFA_SOURCES := $(wildcard third_party/msfa/*.cc)
MSFA_OBJECTS := $(patsubst third_party/msfa/%.cc,build/msfa/%.o,$(MSFA_SOURCES))
TEXTURE_SOURCES := third_party/mutable/clouds/dsp/correlator.cc third_party/mutable/clouds/dsp/granular_processor.cc third_party/mutable/clouds/dsp/mu_law.cc third_party/mutable/clouds/dsp/pvoc/frame_transformation.cc third_party/mutable/clouds/dsp/pvoc/phase_vocoder.cc third_party/mutable/clouds/dsp/pvoc/stft.cc third_party/mutable/clouds/resources.cc third_party/mutable/stmlib/dsp/atan.cc
TEXTURE_OBJECTS := $(patsubst third_party/mutable/%.cc,build/plaits/%.o,$(TEXTURE_SOURCES)) build/texture-dsp.o
RING_SOURCES := third_party/mutable/rings/dsp/part.cc third_party/mutable/rings/dsp/resonator.cc third_party/mutable/rings/dsp/string.cc third_party/mutable/rings/dsp/fm_voice.cc third_party/mutable/rings/resources.cc
RING_OBJECTS := $(patsubst third_party/mutable/%.cc,build/plaits/%.o,$(RING_SOURCES)) build/resonator-dsp.o $(TEXTURE_OBJECTS)
PL_SOURCES := $(TEXTURE_SOURCES) $(RING_SOURCES) $(shell find third_party/mutable/plaits/dsp -name '*.cc') third_party/mutable/plaits/resources.cc third_party/mutable/stmlib/dsp/units.cc third_party/mutable/stmlib/utils/random.cc
PL_OBJECTS := $(patsubst third_party/mutable/%.cc,build/plaits/%.o,$(PL_SOURCES)) build/resonator-dsp.o build/texture-dsp.o
HOST_FLAGS = -Ibuild/mutable-port -std=c++17 -DGROOVE_PLAITS -DTEST -Isrc -Ithird_party/mutable -Ithird_party/daisysp/Source -Ithird_party/daisysp/Source/Utility
SAN = -fsanitize=address,undefined
DAISY = src/granular_dsp.cpp third_party/daisysp/Source/Control/phasor.cpp third_party/daisysp/Source/Synthesis/formantosc.cpp third_party/daisysp/Source/Synthesis/vosim.cpp src/soundfont.cpp src/fm6_voice.cpp $(MSFA_OBJECTS) src/wavetable_bank.cpp third_party/daisysp/Source/Filters/ladder.cpp third_party/daisysp/Source/Control/adsr.cpp third_party/daisysp/Source/Synthesis/oscillator.cpp third_party/daisysp/Source/Synthesis/fm2.cpp $(wildcard third_party/daisysp/Source/Drums/*.cpp) third_party/daisysp/Source/Filters/svf.cpp third_party/daisysp/Source/Effects/wavefolder.cpp third_party/daisysp/Source/Effects/overdrive.cpp
HOST_SUPPORT_SOURCES = $(filter %.cpp,$(DAISY))
HOST_SUPPORT_OBJECTS = $(patsubst %.cpp,build/host-support/%.o,$(HOST_SUPPORT_SOURCES)) $(filter %.o,$(DAISY))
build/host-support/%.o: %.cpp Makefile
	@mkdir -p $(@D)
	$(CXX) $(HOST_FLAGS) $(SAN) -O1 -MMD -MP -Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-variable -Wno-unused-local-typedefs -c $< -o $@
-include $(wildcard $(HOST_SUPPORT_OBJECTS:.o=.d))
.PHONY: test sampler-record-test firmware clean demo benchmark-project
benchmark-project: $(UI_TOOL_ARCHIVE)
	mkdir -p build
	$(CXX) -std=c++17 -O2 -Isrc -Wall -Wextra -Werror tools/content/benchmark.cpp -o build/benchmark-project $(UI_TOOL_ARCHIVE)
	./build/benchmark-project build/BENCH.MGXS
PROFILE_MSFA_OBJECTS := $(patsubst third_party/msfa/%.cc,build/fx-profile/msfa/%.o,$(MSFA_SOURCES))
build/fx-profile/msfa/%.o: third_party/msfa/%.cc
	@mkdir -p $(@D)
	$(CXX) -std=c++17 -O2 -g -pg -fwrapv -D__CONTROLLERS_H -Isrc/msfa_adapter/msfa -c $< -o $@
PROFILE_OBJECTS := $(patsubst third_party/mutable/%.cc,build/fx-profile/%.o,$(PL_SOURCES))
FX_PROFILE_ENGINE ?= $(ENGINE_SOURCES)
build/fx-profile/%.o: third_party/mutable/%.cc
	@mkdir -p $(dir $@)
	$(CXX) $(HOST_FLAGS) -O2 -g -pg -c $< -o $@
.PHONY: fx-profile
fx-profile: $(PROFILE_OBJECTS) $(PROFILE_MSFA_OBJECTS) $(UI_TOOL_ARCHIVE)
	$(CXX) $(HOST_FLAGS) -O2 -g -pg tools/update/fx_profile.cpp $(FX_PROFILE_ENGINE) $(filter %.cpp,$(DAISY)) src/resonator_dsp.cpp src/texture_dsp.cpp $(PROFILE_MSFA_OBJECTS) $(PROFILE_OBJECTS) -o build/fx-profile-run $(UI_TOOL_ARCHIVE)
build/plaits/%.o: third_party/mutable/%.cc
	@mkdir -p $(dir $@)
	$(CXX) $(HOST_FLAGS) $(SAN) -g -O1 -MMD -MP -c $< -o $@
# One maintained inventory; serial and parallel runs execute the same tests.
test-serial:
	$(MAKE) --no-print-directory test TEST_JOBS=1

demo: $(PL_OBJECTS) $(HOST_SUPPORT_OBJECTS) build/test-core-engine.o $(UI_BASE_ARCHIVE)
	mkdir -p build
	$(CXX) $(HOST_FLAGS) -Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-variable -Wno-unused-local-typedefs $(SAN) examples/render.cpp build/test-core-engine.o $(HOST_SUPPORT_OBJECTS) $(PL_OBJECTS) -o build/render $(UI_BASE_ARCHIVE)
	./build/render build/demo.wav
	$(CXX) $(HOST_FLAGS) -Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-variable -Wno-unused-local-typedefs $(SAN) examples/plaits_demo.cpp $(PL_OBJECTS) -o build/render-plaits $(UI_BASE_ARCHIVE)
	./build/render-plaits build/plaits-models.wav
firmware:
	./scripts/build-firmware.sh
clean:
	rm -rf build



# Override SDL_CFLAGS / SDL_LIBS for a system SDL2 installation.
DESKTOP_OPT ?= -O2
SDL_CFLAGS ?= $(shell pkg-config --cflags sdl2 2>/dev/null)
SDL_LIBS ?= $(shell pkg-config --libs sdl2 2>/dev/null)
.PHONY: simulator simulator-test
simulator: $(PL_OBJECTS) $(HOST_SUPPORT_OBJECTS) build/test-core-engine.o $(UI_BASE_ARCHIVE)
	@test -n "$(SDL_LIBS)" || (echo 'Install SDL2 development files or set SDL_CFLAGS and SDL_LIBS' && false)
	$(CXX) $(HOST_FLAGS) $(DESKTOP_OPT) $(SDL_CFLAGS) -Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-variable -Wno-unused-local-typedefs $(SAN) platform/desktop/virtual_mk3.cpp build/test-core-engine.o $(HOST_SUPPORT_OBJECTS) $(PL_OBJECTS) $(SDL_LIBS) -o build/virtual-mk3 $(UI_BASE_ARCHIVE)
simulator-test: simulator
	SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy UBSAN_OPTIONS=halt_on_error=1 ./build/virtual-mk3 --self-test

.PHONY: simulator-readiness-test
simulator-readiness-test: simulator
	SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy UBSAN_OPTIONS=halt_on_error=1 ./build/virtual-mk3 --readiness-test

.PHONY: integration-test backend-test
build/test-kit-track: tests/kit_track_test.cpp build/test-core-engine.o src/engine.h src/performance.h src/project.h src/project_codec.h $(HOST_SUPPORT_OBJECTS) $(PL_OBJECTS) $(UI_BASE_ARCHIVE)
	$(CXX) $(HOST_FLAGS) -O1 -Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-variable -Wno-unused-local-typedefs $(SAN) tests/kit_track_test.cpp build/test-core-engine.o $(HOST_SUPPORT_OBJECTS) $(PL_OBJECTS) -o $@ $(UI_BASE_ARCHIVE)
.PHONY: kit-track-test
kit-track-test: build/test-kit-track
	UBSAN_OPTIONS=halt_on_error=1 ./build/test-kit-track
.PHONY: stop-effects-test
stop-effects-test: $(UI_BASE_ARCHIVE)
	$(CXX) -std=c++17 -O2 -Isrc -Wall -Wextra -Werror $(SAN) tests/stop_effects_test.cpp -o build/test-stop-effects $(UI_BASE_ARCHIVE)
	UBSAN_OPTIONS=halt_on_error=1 ./build/test-stop-effects
.PHONY: track-insert-test
track-insert-test: build/plaits/plaits/resources.o $(UI_BASE_ARCHIVE)
	$(CXX) $(HOST_FLAGS) -O2 -Wall -Wextra -Werror -Wno-unused-parameter $(SAN) tests/track_insert_test.cpp third_party/daisysp/Source/Filters/svf.cpp third_party/daisysp/Source/Effects/wavefolder.cpp third_party/daisysp/Source/Effects/overdrive.cpp third_party/daisysp/Source/Control/phasor.cpp build/plaits/plaits/resources.o -o build/test-track-insert $(UI_BASE_ARCHIVE)
	UBSAN_OPTIONS=halt_on_error=1 ./build/test-track-insert
build/test-kit-record-workflow.o: tests/kit_record_workflow_test.cpp $(DEMO_HEADERS)
	$(CXX) $(HOST_FLAGS) $(SAN) -O1 -Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-variable -Wno-unused-local-typedefs -c $< -o $@
build/test-kit-record-workflow: build/test-kit-record-workflow.o build/test-core-engine.o $(HOST_SUPPORT_OBJECTS) $(PL_OBJECTS) $(UI_BASE_ARCHIVE)
	$(CXX) $(HOST_FLAGS) $(SAN) -O1 $^ -o $@ $(UI_BASE_ARCHIVE)
.PHONY: kit-record-workflow-test
kit-record-workflow-test: build/test-kit-record-workflow
	UBSAN_OPTIONS=halt_on_error=1 ./build/test-kit-record-workflow
.PHONY: sample-analysis-worker-test
sample-analysis-worker-test: test-run-sample-analysis-slot
integration-test: $(PL_OBJECTS) $(HOST_SUPPORT_OBJECTS) build/test-core-engine.o $(UI_BASE_ARCHIVE)
	$(CXX) $(HOST_FLAGS) -Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-variable -Wno-unused-local-typedefs $(SAN) tests/integration/session_test.cpp build/test-core-engine.o $(HOST_SUPPORT_OBJECTS) $(PL_OBJECTS) -o build/test-integration $(UI_BASE_ARCHIVE)
	UBSAN_OPTIONS=halt_on_error=1 ./build/test-integration
	$(MAKE) backend-test
	$(CXX) $(HOST_FLAGS) -Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-variable -Wno-unused-local-typedefs $(SAN) tests/integration/chord_test.cpp build/test-core-engine.o $(HOST_SUPPORT_OBJECTS) $(PL_OBJECTS) -o build/test-chord $(UI_BASE_ARCHIVE)
	UBSAN_OPTIONS=halt_on_error=1 ./build/test-chord

# The large backend scenario is kept as a separate optimized object. Without
# optimization, ASan instruments millions of tiny project codec calls.
# Keep line-level debug info without GCC's costly local-variable tracking.
build/test-backends.o: tests/integration/backends_test.cpp src/project_codec.h src/session_files.h src/performance.h
	$(CXX) $(HOST_FLAGS) -O1 -g1 -Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-variable -Wno-unused-local-typedefs $(SAN) -c $< -o $@
build/test-backends: build/test-backends.o build/test-core-engine.o $(HOST_SUPPORT_OBJECTS) $(PL_OBJECTS) $(UI_BASE_ARCHIVE)
	$(CXX) $(HOST_FLAGS) -O1 $(SAN) $^ -o $@ $(UI_BASE_ARCHIVE)
backend-test: build/test-backends
	UBSAN_OPTIONS=halt_on_error=1 ./build/test-backends

sampler-record-test: $(PL_OBJECTS) $(HOST_SUPPORT_OBJECTS) build/test-core-engine.o $(UI_BASE_ARCHIVE)
	$(CXX) $(HOST_FLAGS) -O1 -Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-variable -Wno-unused-local-typedefs $(SAN) tests/loopback_record_test.cpp build/test-core-engine.o $(HOST_SUPPORT_OBJECTS) $(PL_OBJECTS) -o build/test-loopback-record $(UI_BASE_ARCHIVE)
	UBSAN_OPTIONS=halt_on_error=1 ./build/test-loopback-record

# Functional demos share the checked DSP object; keep host timing separate
# from the optimized Circle target. Cache frontends until their inputs change.
DEMO_OPT ?= -O1
DEMO_HEADERS = $(wildcard src/*.h src/*.inc src/ui/*.h src/ui/generated/*.inc platform/desktop/*.h)
build/session-render.o: examples/session_render.cpp $(DEMO_HEADERS)
	$(CXX) $(HOST_FLAGS) $(DEMO_OPT) -Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-variable -Wno-unused-local-typedefs $(SAN) -c $< -o $@
build/session-render: build/session-render.o build/test-core-engine.o $(HOST_SUPPORT_OBJECTS) $(PL_OBJECTS) $(UI_BASE_ARCHIVE)
	$(CXX) $(HOST_FLAGS) $(SAN) -O1 $^ -o $@ $(UI_BASE_ARCHIVE)
.PHONY: session-demo
session-demo: build/session-render
	UBSAN_OPTIONS=halt_on_error=1 ./build/session-render build/ultimate-demo

UPDATE_FLAGS = -std=c++17 -Isrc -Ithird_party/monocypher -Ithird_party/fatfs-host -Wall -Wextra -Werror $(SAN) -g -O1
UPDATE_OBJECTS = build/update/monocypher.o build/update/ed25519.o build/update/fatfs.o build/update/ascii.o
build/update/monocypher.o: third_party/monocypher/monocypher.c third_party/monocypher/monocypher.h
	@mkdir -p build/update
	$(CC) -std=c99 $(SAN) -g -O1 -Ithird_party/monocypher -c $< -o $@
build/update/ed25519.o: third_party/monocypher/monocypher-ed25519.c third_party/monocypher/monocypher-ed25519.h
	@mkdir -p build/update
	$(CC) -std=c99 $(SAN) -g -O1 -Ithird_party/monocypher -c $< -o $@
build/update/fatfs.o: third_party/fatfs-host/ff.c third_party/fatfs-host/ff.h third_party/fatfs-host/ffconf.h
	@mkdir -p build/update
	$(CC) -std=c99 $(SAN) -g -O1 -Ithird_party/fatfs-host -c $< -o $@
build/update/ascii.o: third_party/fatfs-host/ascii.c third_party/fatfs-host/ffconf.h
	@mkdir -p build/update
	$(CC) -std=c99 $(SAN) -g -O1 -Ithird_party/fatfs-host -c $< -o $@
.PHONY: update-simulator update-test
.PHONY: hardware-tool-test
hardware-tool-test:
	PYTHONPATH="$(CURDIR)/.deps/python$${PYTHONPATH:+:$${PYTHONPATH}}" python3 tests/update/raw_usb_control_test.py
update-simulator: $(UPDATE_OBJECTS) $(UI_TOOL_ARCHIVE)
	$(CXX) $(UPDATE_FLAGS) tools/update/simulator.cpp $(UPDATE_OBJECTS) -o build/update-sim $(UI_TOOL_ARCHIVE)
update-test: update-simulator $(UI_BASE_ARCHIVE)
	$(CXX) -std=c++17 -Isrc -Wall -Wextra -Werror $(SAN) tests/update/button_input_test.cpp -o build/test-update-buttons $(UI_BASE_ARCHIVE)
	./build/test-update-buttons
	UBSAN_OPTIONS=halt_on_error=1 python3 tests/update/integration.py
	python3 tests/update/adapters.py
	python3 tests/update/prepare_test.py
	$(CXX) -std=c++17 -Isrc -Wall -Wextra -Werror $(SAN) tests/update/serial_control_test.cpp -o build/test-update-serial $(UI_BASE_ARCHIVE)
	UBSAN_OPTIONS=halt_on_error=1 ./build/test-update-serial
	$(CXX) -std=c++17 -Isrc -Wall -Wextra -Werror $(SAN) tests/update/usb_control_test.cpp -o build/test-update-usb-control $(UI_BASE_ARCHIVE)
	UBSAN_OPTIONS=halt_on_error=1 ./build/test-update-usb-control
	python3 tests/update/uart_host_test.py
	python3 tests/update/benchmark_host_test.py
	python3 tests/update/remote_flow_test.py
	python3 tests/update/usb_remote_flow_test.py
	python3 tests/update/usb_remote_pty_test.py

.PHONY: ui-preview
build/ui-surface-render.o: examples/ui_surface_render.cpp $(DEMO_HEADERS)
	@mkdir -p build
	$(CXX) $(HOST_FLAGS) $(SAN) -O1 -Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-variable -Wno-unused-local-typedefs -c $< -o $@
build/ui-surface-render: build/ui-surface-render.o build/test-core-engine.o $(HOST_SUPPORT_OBJECTS) $(PL_OBJECTS) $(UI_BASE_ARCHIVE)
	$(CXX) $(HOST_FLAGS) $(SAN) -O1 $^ -o $@ $(UI_BASE_ARCHIVE)
ui-preview: build/ui-surface-render
	UBSAN_OPTIONS=halt_on_error=1 ./build/ui-surface-render
	python3 tools/mk3_ui_diagram.py

.PHONY: fonts ui-gallery
fonts:
	python3 tools/fetch-ui-fonts.py
	python3 tools/ui-fonts.py
	python3 tools/build-font-atlas.py
ui-gallery: ui-preview
	python3 tools/fetch-ui-fonts.py
	python3 tools/ui-design-gallery.py

build/workflow-render.o: examples/workflow_render.cpp examples/workflow_demo.h $(DEMO_HEADERS)
	$(CXX) $(HOST_FLAGS) $(DEMO_OPT) -Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-variable -Wno-unused-local-typedefs $(SAN) -c $< -o $@
build/workflow-render: build/workflow-render.o build/test-core-engine.o $(HOST_SUPPORT_OBJECTS) $(PL_OBJECTS) $(UI_BASE_ARCHIVE)
	$(CXX) $(HOST_FLAGS) $(SAN) -O1 $^ -o $@ $(UI_BASE_ARCHIVE)
.PHONY: workflow-demo
workflow-demo: build/workflow-render
	UBSAN_OPTIONS=halt_on_error=1 ./build/workflow-render
	python3 tools/workflow-gallery.py

build/creative-render.o: examples/creative_render.cpp $(DEMO_HEADERS)
	$(CXX) $(HOST_FLAGS) $(DEMO_OPT) -Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-variable -Wno-unused-local-typedefs $(SAN) -c $< -o $@
build/creative-render: build/creative-render.o build/test-core-engine.o $(HOST_SUPPORT_OBJECTS) $(PL_OBJECTS) $(UI_BASE_ARCHIVE)
	$(CXX) $(HOST_FLAGS) $(SAN) -O1 $^ -o $@ $(UI_BASE_ARCHIVE)
.PHONY: creative-demo
creative-demo: build/creative-render
	UBSAN_OPTIONS=halt_on_error=1 ./build/creative-render
	python3 tools/creative-gallery.py

.PHONY: factory-content factory-library factory-check
factory-library:
	python3 tools/content/build_factory_library.py

factory-content: $(PL_OBJECTS) $(HOST_SUPPORT_OBJECTS) $(ENGINE_TOOL_OBJECTS) $(UI_BASE_ARCHIVE)
	$(CXX) $(HOST_FLAGS) -O2 -Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-variable -Wno-unused-local-typedefs -c tools/content/factory.cpp -o build/factory-tool.o
	$(LD) -r $(ENGINE_TOOL_OBJECTS) -o build/factory-engine.o
	$(CXX) $(HOST_FLAGS) -O2 -Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-variable -Wno-unused-local-typedefs $(SAN) build/factory-tool.o build/factory-engine.o $(HOST_SUPPORT_OBJECTS) $(PL_OBJECTS) -o build/factory-content $(UI_BASE_ARCHIVE)
	./build/factory-content

factory-check:
	python3 tests/factory_library_test.py

.PHONY: showcase-render
showcase-render: $(PL_OBJECTS) $(HOST_SUPPORT_OBJECTS) $(ENGINE_TOOL_OBJECTS) $(UI_BASE_ARCHIVE)
	$(LD) -r $(ENGINE_TOOL_OBJECTS) -o build/showcase-engine.o
	$(CXX) $(HOST_FLAGS) -O2 -Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-variable -Wno-unused-local-typedefs $(SAN) tools/content/render_showcase.cpp build/showcase-engine.o $(HOST_SUPPORT_OBJECTS) $(PL_OBJECTS) -o build/render-showcase $(UI_BASE_ARCHIVE)
	./build/render-showcase build/showcase.wav

build/readiness-render.o: examples/readiness_render.cpp $(DEMO_HEADERS)
	$(CXX) $(HOST_FLAGS) $(DEMO_OPT) -Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-variable -Wno-unused-local-typedefs $(SAN) -c $< -o $@
build/readiness-render: build/readiness-render.o build/test-core-engine.o $(HOST_SUPPORT_OBJECTS) $(PL_OBJECTS) $(UI_BASE_ARCHIVE)
	$(CXX) $(HOST_FLAGS) $(SAN) -O1 $^ -o $@ $(UI_BASE_ARCHIVE)
.PHONY: readiness-demo
readiness-demo: factory-content build/readiness-render
	UBSAN_OPTIONS=halt_on_error=1 ./build/readiness-render
	python3 tools/readiness-gallery.py

.PHONY: plaits-load-benchmark
plaits-load-benchmark: $(PL_OBJECTS) $(HOST_SUPPORT_OBJECTS) build/test-core-engine.o $(UI_BASE_ARCHIVE)
	$(CXX) $(HOST_FLAGS) -O2 -Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-variable -Wno-unused-local-typedefs $(SAN) tools/performance/plaits_track_load.cpp build/test-core-engine.o $(HOST_SUPPORT_OBJECTS) $(PL_OBJECTS) -o build/plaits-track-load $(UI_BASE_ARCHIVE)
	UBSAN_OPTIONS=halt_on_error=1 ./build/plaits-track-load

.PHONY: live-fx-lock-test
live-fx-lock-test: test-run-live-fx-lock test-run-live-fx-codec test-run-macro-curves

.PHONY: project-io-buffer-test
project-io-buffer-test: test-run-project-io-buffer

.PHONY: async-project-save-test
async-project-save-test: test-run-async-project-save
build/test-async-project-save: tests/async_project_save_test.cpp $(wildcard src/*.h) $(UI_BASE_ARCHIVE)
	$(CXX) -std=c++17 -O2 -Isrc -Wall -Wextra -Werror -pthread $(SAN) tests/async_project_save_test.cpp -o build/test-async-project-save $(UI_BASE_ARCHIVE)
test-run-async-project-save: build/test-async-project-save
	UBSAN_OPTIONS=halt_on_error=1 ./build/test-async-project-save
test-all: test-run-async-project-save

.PHONY: async-project-read-test
async-project-read-test: test-run-async-project-read
build/test-async-project-read: tests/async_project_read_test.cpp $(wildcard src/*.h) $(UI_BASE_ARCHIVE)
	$(CXX) -std=c++17 -O2 -Isrc -Wall -Wextra -Werror -pthread $(SAN) tests/async_project_read_test.cpp -o build/test-async-project-read $(UI_BASE_ARCHIVE)
test-run-async-project-read: build/test-async-project-read
	UBSAN_OPTIONS=halt_on_error=1 ./build/test-async-project-read
test-all: test-run-async-project-read

.PHONY: kit-asset-verifier-test
kit-asset-verifier-test: test-run-kit-asset-verifier

.PHONY: chunked-sample-read-test
chunked-sample-read-test: test-run-chunked-sample-read

.PHONY: page-descriptor-test
page-descriptor-test: $(HOST_SUPPORT_OBJECTS) build/test-core-engine.o $(UI_BASE_ARCHIVE)
	$(CXX) $(HOST_FLAGS) -Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-variable -Wno-unused-local-typedefs $(SAN) tests/page_descriptor_test.cpp build/test-core-engine.o $(HOST_SUPPORT_OBJECTS) $(PL_OBJECTS) -o build/test-page-descriptors $(UI_BASE_ARCHIVE)
	UBSAN_OPTIONS=halt_on_error=1 ./build/test-page-descriptors

.PHONY: page-nav-test
page-nav-test: $(HOST_SUPPORT_OBJECTS) build/test-core-engine.o $(UI_BASE_ARCHIVE)
	$(CXX) $(HOST_FLAGS) -Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-variable -Wno-unused-local-typedefs $(SAN) tests/page_nav_test.cpp build/test-core-engine.o $(HOST_SUPPORT_OBJECTS) $(PL_OBJECTS) -o build/test-page-nav $(UI_BASE_ARCHIVE)
	UBSAN_OPTIONS=halt_on_error=1 ./build/test-page-nav

.PHONY: source-select-test
source-select-test: $(HOST_SUPPORT_OBJECTS) build/test-core-engine.o $(UI_BASE_ARCHIVE)
	$(CXX) $(HOST_FLAGS) -Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-variable -Wno-unused-local-typedefs $(SAN) tests/source_select_test.cpp build/test-core-engine.o $(HOST_SUPPORT_OBJECTS) $(PL_OBJECTS) -o build/test-source-select $(UI_BASE_ARCHIVE)
	UBSAN_OPTIONS=halt_on_error=1 ./build/test-source-select

.PHONY: engine-pages-test
engine-pages-test: $(HOST_SUPPORT_OBJECTS) build/test-core-engine.o $(UI_BASE_ARCHIVE)
	$(CXX) $(HOST_FLAGS) -Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-variable -Wno-unused-local-typedefs $(SAN) tests/engine_pages_test.cpp build/test-core-engine.o $(HOST_SUPPORT_OBJECTS) $(PL_OBJECTS) -o build/test-engine-pages $(UI_BASE_ARCHIVE)
	UBSAN_OPTIONS=halt_on_error=1 ./build/test-engine-pages

.PHONY: fx-chain-test
fx-chain-test: $(HOST_SUPPORT_OBJECTS) build/test-core-engine.o $(UI_BASE_ARCHIVE)
	$(CXX) $(HOST_FLAGS) -Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-variable -Wno-unused-local-typedefs $(SAN) tests/fx_chain_test.cpp build/test-core-engine.o $(HOST_SUPPORT_OBJECTS) $(PL_OBJECTS) -o build/test-fx-chain $(UI_BASE_ARCHIVE)
	UBSAN_OPTIONS=halt_on_error=1 ./build/test-fx-chain

.PHONY: browser-open-stress-test
browser-open-stress-test: $(HOST_SUPPORT_OBJECTS) build/test-core-engine.o $(UI_BASE_ARCHIVE)
	$(CXX) $(HOST_FLAGS) -Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-variable -Wno-unused-local-typedefs $(SAN) tests/browser_open_stress_test.cpp build/test-core-engine.o $(HOST_SUPPORT_OBJECTS) $(PL_OBJECTS) -o build/test-browser-open-stress $(UI_BASE_ARCHIVE)
	UBSAN_OPTIONS=halt_on_error=1 ./build/test-browser-open-stress

.PHONY: buttons-test
buttons-test: $(HOST_SUPPORT_OBJECTS) build/test-core-engine.o $(UI_BASE_ARCHIVE)
	$(CXX) $(HOST_FLAGS) -Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-variable -Wno-unused-local-typedefs $(SAN) tests/buttons_test.cpp build/test-core-engine.o $(HOST_SUPPORT_OBJECTS) $(PL_OBJECTS) -o build/test-buttons $(UI_BASE_ARCHIVE)
	UBSAN_OPTIONS=halt_on_error=1 ./build/test-buttons

.PHONY: duplicate-test
duplicate-test: $(HOST_SUPPORT_OBJECTS) build/test-core-engine.o $(UI_BASE_ARCHIVE)
	$(CXX) $(HOST_FLAGS) -Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-variable -Wno-unused-local-typedefs $(SAN) tests/duplicate_test.cpp build/test-core-engine.o $(HOST_SUPPORT_OBJECTS) $(PL_OBJECTS) -o build/test-duplicate $(UI_BASE_ARCHIVE)
	UBSAN_OPTIONS=halt_on_error=1 ./build/test-duplicate

.PHONY: daisy-adsr-test
daisy-adsr-test: $(HOST_SUPPORT_OBJECTS) build/test-core-engine.o $(UI_BASE_ARCHIVE)
	$(CXX) $(HOST_FLAGS) -Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-variable -Wno-unused-local-typedefs $(SAN) tests/daisy_adsr_test.cpp build/test-core-engine.o $(HOST_SUPPORT_OBJECTS) $(PL_OBJECTS) -o build/test-daisy-adsr $(UI_BASE_ARCHIVE)
	UBSAN_OPTIONS=halt_on_error=1 ./build/test-daisy-adsr

.PHONY: transition-test
transition-test: $(HOST_SUPPORT_OBJECTS) build/test-core-engine.o $(UI_BASE_ARCHIVE)
	$(CXX) $(HOST_FLAGS) -Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-variable -Wno-unused-local-typedefs $(SAN) tests/transition_test.cpp build/test-core-engine.o $(HOST_SUPPORT_OBJECTS) $(PL_OBJECTS) -o build/test-transition $(UI_BASE_ARCHIVE)
	UBSAN_OPTIONS=halt_on_error=1 ./build/test-transition

.PHONY: lab-test
lab-test: $(HOST_SUPPORT_OBJECTS) build/test-core-engine.o $(UI_BASE_ARCHIVE)
	$(CXX) $(HOST_FLAGS) -Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-variable -Wno-unused-local-typedefs $(SAN) tests/lab_test.cpp build/test-core-engine.o $(HOST_SUPPORT_OBJECTS) $(PL_OBJECTS) -o build/test-lab $(UI_BASE_ARCHIVE)
	UBSAN_OPTIONS=halt_on_error=1 ./build/test-lab

.PHONY: song-test
song-test: $(HOST_SUPPORT_OBJECTS) build/test-core-engine.o $(UI_BASE_ARCHIVE)
	$(CXX) $(HOST_FLAGS) -Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-variable -Wno-unused-local-typedefs $(SAN) tests/song_test.cpp build/test-core-engine.o $(HOST_SUPPORT_OBJECTS) $(PL_OBJECTS) -o build/test-song $(UI_BASE_ARCHIVE)
	$(CXX) $(HOST_FLAGS) -Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-variable -Wno-unused-local-typedefs $(SAN) tests/song_page_test.cpp build/test-core-engine.o $(HOST_SUPPORT_OBJECTS) $(PL_OBJECTS) -o build/test-song-page $(UI_BASE_ARCHIVE)
	$(CXX) $(HOST_FLAGS) -Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-variable -Wno-unused-local-typedefs $(SAN) tests/wave_zones_test.cpp build/test-core-engine.o $(HOST_SUPPORT_OBJECTS) $(PL_OBJECTS) -o build/test-wave-zones $(UI_BASE_ARCHIVE)
	UBSAN_OPTIONS=halt_on_error=1 ./build/test-song

.PHONY: song-page-test
song-page-test: $(HOST_SUPPORT_OBJECTS) build/test-core-engine.o $(UI_BASE_ARCHIVE)
	$(CXX) $(HOST_FLAGS) -Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-variable -Wno-unused-local-typedefs $(SAN) tests/song_page_test.cpp build/test-core-engine.o $(HOST_SUPPORT_OBJECTS) $(PL_OBJECTS) -o build/test-song-page $(UI_BASE_ARCHIVE)
	UBSAN_OPTIONS=halt_on_error=1 ./build/test-song-page

.PHONY: wave-zones-test
capture-test: $(HOST_SUPPORT_OBJECTS) build/test-core-engine.o $(UI_BASE_ARCHIVE)
	$(CXX) $(HOST_FLAGS) -Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-variable -Wno-unused-local-typedefs $(SAN) tests/capture_test.cpp build/test-core-engine.o $(HOST_SUPPORT_OBJECTS) $(PL_OBJECTS) -o build/test-capture $(UI_BASE_ARCHIVE)
	UBSAN_OPTIONS=halt_on_error=1 ./build/test-capture

mute-layers-test: $(HOST_SUPPORT_OBJECTS) build/test-core-engine.o $(UI_BASE_ARCHIVE)
	$(CXX) $(HOST_FLAGS) -Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-variable -Wno-unused-local-typedefs $(SAN) tests/mute_layers_test.cpp build/test-core-engine.o $(HOST_SUPPORT_OBJECTS) $(PL_OBJECTS) -o build/test-mute-layers $(UI_BASE_ARCHIVE)
	UBSAN_OPTIONS=halt_on_error=1 ./build/test-mute-layers

morph-sets-test: $(HOST_SUPPORT_OBJECTS) build/test-core-engine.o $(UI_BASE_ARCHIVE)
	$(CXX) $(HOST_FLAGS) -Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-variable -Wno-unused-local-typedefs $(SAN) tests/morph_sets_test.cpp build/test-core-engine.o $(HOST_SUPPORT_OBJECTS) $(PL_OBJECTS) -o build/test-morph-sets $(UI_BASE_ARCHIVE)
	UBSAN_OPTIONS=halt_on_error=1 ./build/test-morph-sets

reload-test: $(HOST_SUPPORT_OBJECTS) build/test-core-engine.o $(UI_BASE_ARCHIVE)
	$(CXX) $(HOST_FLAGS) -Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-variable -Wno-unused-local-typedefs $(SAN) tests/reload_test.cpp build/test-core-engine.o $(HOST_SUPPORT_OBJECTS) $(PL_OBJECTS) -o build/test-reload $(UI_BASE_ARCHIVE)
	UBSAN_OPTIONS=halt_on_error=1 ./build/test-reload

sound-state-test: $(HOST_SUPPORT_OBJECTS) build/test-core-engine.o $(UI_BASE_ARCHIVE)
	$(CXX) $(HOST_FLAGS) -Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-variable -Wno-unused-local-typedefs $(SAN) tests/sound_state_test.cpp build/test-core-engine.o $(HOST_SUPPORT_OBJECTS) $(PL_OBJECTS) -o build/test-sound-state $(UI_BASE_ARCHIVE)
	UBSAN_OPTIONS=halt_on_error=1 ./build/test-sound-state

warp-test: $(HOST_SUPPORT_OBJECTS) build/test-core-engine.o $(UI_BASE_ARCHIVE)
	$(CXX) $(HOST_FLAGS) -Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-variable -Wno-unused-local-typedefs $(SAN) tests/warp_test.cpp build/test-core-engine.o $(HOST_SUPPORT_OBJECTS) $(PL_OBJECTS) -o build/test-warp $(UI_BASE_ARCHIVE)
	UBSAN_OPTIONS=halt_on_error=1 ./build/test-warp

stretch-playback-test: $(HOST_SUPPORT_OBJECTS) build/test-core-engine.o $(UI_BASE_ARCHIVE)
	$(CXX) $(HOST_FLAGS) -Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-variable -Wno-unused-local-typedefs $(SAN) tests/stretch_playback_test.cpp build/test-core-engine.o $(HOST_SUPPORT_OBJECTS) $(PL_OBJECTS) -o build/test-stretch-playback $(UI_BASE_ARCHIVE)
	UBSAN_OPTIONS=halt_on_error=1 ./build/test-stretch-playback

mod-matrix-test: $(HOST_SUPPORT_OBJECTS) build/test-core-engine.o $(UI_BASE_ARCHIVE)
	$(CXX) $(HOST_FLAGS) -Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-variable -Wno-unused-local-typedefs $(SAN) tests/mod_matrix_test.cpp build/test-core-engine.o $(HOST_SUPPORT_OBJECTS) $(PL_OBJECTS) -o build/test-mod-matrix $(UI_BASE_ARCHIVE)

.PHONY: fx-type-lock-test
fx-type-lock-test: $(PL_OBJECTS) $(HOST_SUPPORT_OBJECTS) build/test-core-engine.o $(UI_BASE_ARCHIVE)
	$(CXX) $(HOST_FLAGS) -O1 -Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-variable -Wno-unused-local-typedefs $(SAN) tests/fx_type_lock_test.cpp build/test-core-engine.o $(HOST_SUPPORT_OBJECTS) $(PL_OBJECTS) -o build/test-fx-type-lock $(UI_BASE_ARCHIVE)
	UBSAN_OPTIONS=halt_on_error=1 ./build/test-fx-type-lock
	UBSAN_OPTIONS=halt_on_error=1 ./build/test-mod-matrix

.PHONY: freeze-test
freeze-test: $(PL_OBJECTS) $(HOST_SUPPORT_OBJECTS) build/test-core-engine.o $(UI_BASE_ARCHIVE)
	$(CXX) $(HOST_FLAGS) -O1 -Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-variable -Wno-unused-local-typedefs $(SAN) tests/freeze_test.cpp build/test-core-engine.o $(HOST_SUPPORT_OBJECTS) $(PL_OBJECTS) -o build/test-freeze $(UI_BASE_ARCHIVE)
	UBSAN_OPTIONS=halt_on_error=1 ./build/test-freeze

stretch-job-test: $(HOST_SUPPORT_OBJECTS) build/test-core-engine.o $(UI_BASE_ARCHIVE)
	$(CXX) $(HOST_FLAGS) -Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-variable -Wno-unused-local-typedefs $(SAN) tests/stretch_job_test.cpp build/test-core-engine.o $(HOST_SUPPORT_OBJECTS) $(PL_OBJECTS) -o build/test-stretch-job $(UI_BASE_ARCHIVE)
	UBSAN_OPTIONS=halt_on_error=1 ./build/test-stretch-job

stretch-test: $(UI_BASE_ARCHIVE)
	$(CXX) $(HOST_FLAGS) -Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-variable -Wno-unused-local-typedefs $(SAN) tests/stretch_test.cpp -o build/test-stretch $(UI_BASE_ARCHIVE)
	UBSAN_OPTIONS=halt_on_error=1 ./build/test-stretch

wave-zones-test: $(HOST_SUPPORT_OBJECTS) build/test-core-engine.o $(UI_BASE_ARCHIVE)
	$(CXX) $(HOST_FLAGS) -Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-variable -Wno-unused-local-typedefs $(SAN) tests/wave_zones_test.cpp build/test-core-engine.o $(HOST_SUPPORT_OBJECTS) $(PL_OBJECTS) -o build/test-wave-zones $(UI_BASE_ARCHIVE)
	UBSAN_OPTIONS=halt_on_error=1 ./build/test-wave-zones

.PHONY: plaits-gate-test
plaits-gate-test: $(UI_BASE_ARCHIVE)
	$(CXX) $(HOST_FLAGS) -Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-variable -Wno-unused-local-typedefs $(SAN) tests/plaits_gate_test.cpp $(PL_OBJECTS) -o build/test-plaits-gate $(UI_BASE_ARCHIVE)
	UBSAN_OPTIONS=halt_on_error=1 ./build/test-plaits-gate

.PHONY: partial-redraw-test
partial-redraw-test: $(HOST_SUPPORT_OBJECTS) build/test-core-engine.o $(UI_BASE_ARCHIVE)
	$(CXX) $(HOST_FLAGS) -Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-variable -Wno-unused-local-typedefs $(SAN) tests/partial_redraw_test.cpp build/test-core-engine.o $(HOST_SUPPORT_OBJECTS) $(PL_OBJECTS) -o build/test-partial-redraw $(UI_BASE_ARCHIVE)
	UBSAN_OPTIONS=halt_on_error=1 ./build/test-partial-redraw

.PHONY: theme-test
theme-test: $(UI_BASE_ARCHIVE)
	$(CXX) $(HOST_FLAGS) -Wall -Wextra -Werror $(SAN) tests/theme_test.cpp -o build/test-theme $(UI_BASE_ARCHIVE)
	UBSAN_OPTIONS=halt_on_error=1 ./build/test-theme

.PHONY: plock-hold-test
plock-hold-test: $(HOST_SUPPORT_OBJECTS) build/test-core-engine.o $(UI_BASE_ARCHIVE)
	$(CXX) $(HOST_FLAGS) -Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-variable -Wno-unused-local-typedefs $(SAN) tests/plock_hold_test.cpp build/test-core-engine.o $(HOST_SUPPORT_OBJECTS) $(PL_OBJECTS) -o build/test-plock-hold $(UI_BASE_ARCHIVE)
	UBSAN_OPTIONS=halt_on_error=1 ./build/test-plock-hold

.PHONY: engine-descriptor-test
engine-descriptor-test: $(HOST_SUPPORT_OBJECTS) build/test-core-engine.o $(UI_BASE_ARCHIVE)
	$(CXX) $(HOST_FLAGS) -Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-variable -Wno-unused-local-typedefs $(SAN) tests/engine_descriptor_test.cpp build/test-core-engine.o $(HOST_SUPPORT_OBJECTS) $(PL_OBJECTS) -o build/test-engine-descriptor $(UI_BASE_ARCHIVE)
	UBSAN_OPTIONS=halt_on_error=1 ./build/test-engine-descriptor

# ==== Parallel test execution (generated rules) ====
# Source of truth for WHAT runs is test-serial above; the rules below
# mirror its commands 1:1 so the same suite can run under -j.
# Each C++ test runs in build/test-run/<name>/ so fixed relative files
# (session.mgx, sample.wav, GROOVE/, ...) never collide across tests.
# Read-only inputs are linked in: tests/fixtures and assets.
# The python update tests drive repo-relative tooling (tools/,
# build/update-sim) and keep the repo root as their CWD.
TEST_JOBS ?= 12
# Sanitized fixtures include full project snapshots. This is host-only;
# firmware stacks and the allocation-free audio contract are unchanged.
TEST_STACK_KB ?= 262144
# Header-only sampler/workflow code must invalidate existing test binaries.
HOST_TEST_BINARIES := $(shell sed -n 's/^\(build\/test[^: ]*\):.*/\1/p' Makefile)
$(HOST_TEST_BINARIES): | canonical-sample
# One shared header dependency node keeps GNU Make 4.3 snap_deps bounded.
# EXTRA_PREREQS still keeps headers/stamp out of recipes using $^.
HOST_TEST_HEADERS := $(wildcard src/*.h src/ui/*.h src/update/*.h platform/desktop/*.h platform/mock/*.h)
build/host-test-headers.stamp: $(HOST_TEST_HEADERS) Makefile
	@mkdir -p $(@D)
	@touch $@
$(HOST_TEST_BINARIES): .EXTRA_PREREQS := build/host-test-headers.stamp
# The writable operational Factory copy belongs to each test sandbox. Do not
# link FACTORY to a shared tree: repair/corruption tests may overwrite it.
TEST_RUN_SETUP = mkdir -p build tests && ln -sfn $(abspath tests/fixtures) tests/fixtures && ln -sfn $(abspath assets) assets && ln -sfn $(abspath ASSETS) ASSETS && mkdir -p FACTORY/DEBUG && cp "$(abspath assets/samples/demo-slices.wav)" FACTORY/DEBUG/demo-slices.wav

build/test-integration: tests/integration/session_test.cpp build/test-core-engine.o $(HOST_SUPPORT_OBJECTS) build/plaits/plaits/dsp/engine2/chiptune_engine.o build/plaits/plaits/dsp/engine2/virtual_analog_vcf_engine.o build/plaits/plaits/dsp/engine2/wave_terrain_engine.o build/plaits/plaits/dsp/engine2/six_op_engine.o build/plaits/plaits/dsp/engine2/phase_distortion_engine.o build/plaits/plaits/dsp/engine2/string_machine_engine.o build/plaits/plaits/dsp/speech/lpc_speech_synth.o build/plaits/plaits/dsp/speech/naive_speech_synth.o build/plaits/plaits/dsp/speech/sam_speech_synth.o build/plaits/plaits/dsp/speech/lpc_speech_synth_phonemes.o build/plaits/plaits/dsp/speech/lpc_speech_synth_words.o build/plaits/plaits/dsp/speech/lpc_speech_synth_controller.o build/plaits/plaits/dsp/engine/particle_engine.o build/plaits/plaits/dsp/engine/modal_engine.o build/plaits/plaits/dsp/engine/noise_engine.o build/plaits/plaits/dsp/engine/waveshaping_engine.o build/plaits/plaits/dsp/engine/string_engine.o build/plaits/plaits/dsp/engine/bass_drum_engine.o build/plaits/plaits/dsp/engine/swarm_engine.o build/plaits/plaits/dsp/engine/wavetable_engine.o build/plaits/plaits/dsp/engine/snare_drum_engine.o build/plaits/plaits/dsp/engine/chord_engine.o build/plaits/plaits/dsp/engine/fm_engine.o build/plaits/plaits/dsp/engine/grain_engine.o build/plaits/plaits/dsp/engine/additive_engine.o build/plaits/plaits/dsp/engine/hi_hat_engine.o build/plaits/plaits/dsp/engine/speech_engine.o build/plaits/plaits/dsp/engine/virtual_analog_engine.o build/plaits/plaits/dsp/voice.o build/plaits/plaits/dsp/fm/dx_units.o build/plaits/plaits/dsp/fm/algorithms.o build/plaits/plaits/dsp/chords/chord_bank.o build/plaits/plaits/dsp/physical_modelling/modal_voice.o build/plaits/plaits/dsp/physical_modelling/string_voice.o build/plaits/plaits/dsp/physical_modelling/resonator.o build/plaits/plaits/dsp/physical_modelling/string.o build/plaits/plaits/resources.o build/plaits/stmlib/dsp/units.o build/plaits/stmlib/utils/random.o $(RING_OBJECTS) $(UI_BASE_ARCHIVE)
	$(CXX) -std=c++17 -DGROOVE_PLAITS -DTEST -Isrc -Ithird_party/mutable -Ithird_party/daisysp/Source -Ithird_party/daisysp/Source/Utility -Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-variable -Wno-unused-local-typedefs -fsanitize=address,undefined tests/integration/session_test.cpp build/test-core-engine.o $(HOST_SUPPORT_OBJECTS) build/plaits/plaits/dsp/engine2/chiptune_engine.o build/plaits/plaits/dsp/engine2/virtual_analog_vcf_engine.o build/plaits/plaits/dsp/engine2/wave_terrain_engine.o build/plaits/plaits/dsp/engine2/six_op_engine.o build/plaits/plaits/dsp/engine2/phase_distortion_engine.o build/plaits/plaits/dsp/engine2/string_machine_engine.o build/plaits/plaits/dsp/speech/lpc_speech_synth.o build/plaits/plaits/dsp/speech/naive_speech_synth.o build/plaits/plaits/dsp/speech/sam_speech_synth.o build/plaits/plaits/dsp/speech/lpc_speech_synth_phonemes.o build/plaits/plaits/dsp/speech/lpc_speech_synth_words.o build/plaits/plaits/dsp/speech/lpc_speech_synth_controller.o build/plaits/plaits/dsp/engine/particle_engine.o build/plaits/plaits/dsp/engine/modal_engine.o build/plaits/plaits/dsp/engine/noise_engine.o build/plaits/plaits/dsp/engine/waveshaping_engine.o build/plaits/plaits/dsp/engine/string_engine.o build/plaits/plaits/dsp/engine/bass_drum_engine.o build/plaits/plaits/dsp/engine/swarm_engine.o build/plaits/plaits/dsp/engine/wavetable_engine.o build/plaits/plaits/dsp/engine/snare_drum_engine.o build/plaits/plaits/dsp/engine/chord_engine.o build/plaits/plaits/dsp/engine/fm_engine.o build/plaits/plaits/dsp/engine/grain_engine.o build/plaits/plaits/dsp/engine/additive_engine.o build/plaits/plaits/dsp/engine/hi_hat_engine.o build/plaits/plaits/dsp/engine/speech_engine.o build/plaits/plaits/dsp/engine/virtual_analog_engine.o build/plaits/plaits/dsp/voice.o build/plaits/plaits/dsp/fm/dx_units.o build/plaits/plaits/dsp/fm/algorithms.o build/plaits/plaits/dsp/chords/chord_bank.o build/plaits/plaits/dsp/physical_modelling/modal_voice.o build/plaits/plaits/dsp/physical_modelling/string_voice.o build/plaits/plaits/dsp/physical_modelling/resonator.o build/plaits/plaits/dsp/physical_modelling/string.o build/plaits/plaits/resources.o build/plaits/stmlib/dsp/units.o build/plaits/stmlib/utils/random.o $(RING_OBJECTS) -o build/test-integration $(UI_BASE_ARCHIVE)
build/test-chord: tests/integration/chord_test.cpp build/test-core-engine.o $(HOST_SUPPORT_OBJECTS) build/plaits/plaits/dsp/engine2/chiptune_engine.o build/plaits/plaits/dsp/engine2/virtual_analog_vcf_engine.o build/plaits/plaits/dsp/engine2/wave_terrain_engine.o build/plaits/plaits/dsp/engine2/six_op_engine.o build/plaits/plaits/dsp/engine2/phase_distortion_engine.o build/plaits/plaits/dsp/engine2/string_machine_engine.o build/plaits/plaits/dsp/speech/lpc_speech_synth.o build/plaits/plaits/dsp/speech/naive_speech_synth.o build/plaits/plaits/dsp/speech/sam_speech_synth.o build/plaits/plaits/dsp/speech/lpc_speech_synth_phonemes.o build/plaits/plaits/dsp/speech/lpc_speech_synth_words.o build/plaits/plaits/dsp/speech/lpc_speech_synth_controller.o build/plaits/plaits/dsp/engine/particle_engine.o build/plaits/plaits/dsp/engine/modal_engine.o build/plaits/plaits/dsp/engine/noise_engine.o build/plaits/plaits/dsp/engine/waveshaping_engine.o build/plaits/plaits/dsp/engine/string_engine.o build/plaits/plaits/dsp/engine/bass_drum_engine.o build/plaits/plaits/dsp/engine/swarm_engine.o build/plaits/plaits/dsp/engine/wavetable_engine.o build/plaits/plaits/dsp/engine/snare_drum_engine.o build/plaits/plaits/dsp/engine/chord_engine.o build/plaits/plaits/dsp/engine/fm_engine.o build/plaits/plaits/dsp/engine/grain_engine.o build/plaits/plaits/dsp/engine/additive_engine.o build/plaits/plaits/dsp/engine/hi_hat_engine.o build/plaits/plaits/dsp/engine/speech_engine.o build/plaits/plaits/dsp/engine/virtual_analog_engine.o build/plaits/plaits/dsp/voice.o build/plaits/plaits/dsp/fm/dx_units.o build/plaits/plaits/dsp/fm/algorithms.o build/plaits/plaits/dsp/chords/chord_bank.o build/plaits/plaits/dsp/physical_modelling/modal_voice.o build/plaits/plaits/dsp/physical_modelling/string_voice.o build/plaits/plaits/dsp/physical_modelling/resonator.o build/plaits/plaits/dsp/physical_modelling/string.o build/plaits/plaits/resources.o build/plaits/stmlib/dsp/units.o build/plaits/stmlib/utils/random.o $(RING_OBJECTS) $(UI_BASE_ARCHIVE)
	$(CXX) -std=c++17 -DGROOVE_PLAITS -DTEST -Isrc -Ithird_party/mutable -Ithird_party/daisysp/Source -Ithird_party/daisysp/Source/Utility -Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-variable -Wno-unused-local-typedefs -fsanitize=address,undefined tests/integration/chord_test.cpp build/test-core-engine.o $(HOST_SUPPORT_OBJECTS) build/plaits/plaits/dsp/engine2/chiptune_engine.o build/plaits/plaits/dsp/engine2/virtual_analog_vcf_engine.o build/plaits/plaits/dsp/engine2/wave_terrain_engine.o build/plaits/plaits/dsp/engine2/six_op_engine.o build/plaits/plaits/dsp/engine2/phase_distortion_engine.o build/plaits/plaits/dsp/engine2/string_machine_engine.o build/plaits/plaits/dsp/speech/lpc_speech_synth.o build/plaits/plaits/dsp/speech/naive_speech_synth.o build/plaits/plaits/dsp/speech/sam_speech_synth.o build/plaits/plaits/dsp/speech/lpc_speech_synth_phonemes.o build/plaits/plaits/dsp/speech/lpc_speech_synth_words.o build/plaits/plaits/dsp/speech/lpc_speech_synth_controller.o build/plaits/plaits/dsp/engine/particle_engine.o build/plaits/plaits/dsp/engine/modal_engine.o build/plaits/plaits/dsp/engine/noise_engine.o build/plaits/plaits/dsp/engine/waveshaping_engine.o build/plaits/plaits/dsp/engine/string_engine.o build/plaits/plaits/dsp/engine/bass_drum_engine.o build/plaits/plaits/dsp/engine/swarm_engine.o build/plaits/plaits/dsp/engine/wavetable_engine.o build/plaits/plaits/dsp/engine/snare_drum_engine.o build/plaits/plaits/dsp/engine/chord_engine.o build/plaits/plaits/dsp/engine/fm_engine.o build/plaits/plaits/dsp/engine/grain_engine.o build/plaits/plaits/dsp/engine/additive_engine.o build/plaits/plaits/dsp/engine/hi_hat_engine.o build/plaits/plaits/dsp/engine/speech_engine.o build/plaits/plaits/dsp/engine/virtual_analog_engine.o build/plaits/plaits/dsp/voice.o build/plaits/plaits/dsp/fm/dx_units.o build/plaits/plaits/dsp/fm/algorithms.o build/plaits/plaits/dsp/chords/chord_bank.o build/plaits/plaits/dsp/physical_modelling/modal_voice.o build/plaits/plaits/dsp/physical_modelling/string_voice.o build/plaits/plaits/dsp/physical_modelling/resonator.o build/plaits/plaits/dsp/physical_modelling/string.o build/plaits/plaits/resources.o build/plaits/stmlib/dsp/units.o build/plaits/stmlib/utils/random.o $(RING_OBJECTS) -o build/test-chord $(UI_BASE_ARCHIVE)
build/update-sim: tools/update/simulator.cpp build/update/monocypher.o build/update/ed25519.o build/update/fatfs.o build/update/ascii.o $(UI_BASE_ARCHIVE)
	$(CXX) -std=c++17 -Isrc -Ithird_party/monocypher -Ithird_party/fatfs-host -Wall -Wextra -Werror -fsanitize=address,undefined -g -O1 tools/update/simulator.cpp build/update/monocypher.o build/update/ed25519.o build/update/fatfs.o build/update/ascii.o -o build/update-sim $(UI_BASE_ARCHIVE)
build/test-update-buttons: tests/update/button_input_test.cpp $(UI_BASE_ARCHIVE)
	$(CXX) -std=c++17 -Isrc -Wall -Wextra -Werror -fsanitize=address,undefined tests/update/button_input_test.cpp -o build/test-update-buttons $(UI_BASE_ARCHIVE)
build/test-update-serial: tests/update/serial_control_test.cpp $(UI_BASE_ARCHIVE)
	$(CXX) -std=c++17 -Isrc -Wall -Wextra -Werror -fsanitize=address,undefined tests/update/serial_control_test.cpp -o build/test-update-serial $(UI_BASE_ARCHIVE)
build/test-update-usb-control: tests/update/usb_control_test.cpp $(UI_BASE_ARCHIVE)
	$(CXX) -std=c++17 -Isrc -Wall -Wextra -Werror -fsanitize=address,undefined tests/update/usb_control_test.cpp -o build/test-update-usb-control $(UI_BASE_ARCHIVE)
build/test-loopback-record.o: tests/loopback_record_test.cpp $(DEMO_HEADERS)
	$(CXX) $(HOST_FLAGS) $(SAN) -O1 -Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-variable -Wno-unused-local-typedefs -c $< -o $@
build/test-loopback-record: build/test-loopback-record.o build/test-core-engine.o $(HOST_SUPPORT_OBJECTS) $(PL_OBJECTS) $(UI_BASE_ARCHIVE)
	$(CXX) $(HOST_FLAGS) $(SAN) -O1 $^ -o $@ $(UI_BASE_ARCHIVE)
build/test-stop-effects: tests/stop_effects_test.cpp $(UI_BASE_ARCHIVE)
	$(CXX) -std=c++17 -O2 -Isrc -Wall -Wextra -Werror -fsanitize=address,undefined tests/stop_effects_test.cpp -o build/test-stop-effects $(UI_BASE_ARCHIVE)
build/test-track-insert: tests/track_insert_test.cpp third_party/daisysp/Source/Filters/svf.cpp third_party/daisysp/Source/Effects/wavefolder.cpp third_party/daisysp/Source/Effects/overdrive.cpp build/plaits/plaits/resources.o $(UI_BASE_ARCHIVE)
	$(CXX) -std=c++17 -DGROOVE_PLAITS -DTEST -Isrc -Ithird_party/mutable -Ithird_party/daisysp/Source -Ithird_party/daisysp/Source/Utility -O2 -Wall -Wextra -Werror -Wno-unused-parameter -fsanitize=address,undefined tests/track_insert_test.cpp third_party/daisysp/Source/Filters/svf.cpp third_party/daisysp/Source/Effects/wavefolder.cpp third_party/daisysp/Source/Effects/overdrive.cpp third_party/daisysp/Source/Control/phasor.cpp build/plaits/plaits/resources.o -o build/test-track-insert $(UI_BASE_ARCHIVE)
build/test-wav-pcm-formats: tests/wav_pcm_formats_test.cpp $(UI_BASE_ARCHIVE)
	$(CXX) -std=c++17 -O2 -Isrc -Wall -Wextra -Werror -fsanitize=address,undefined tests/wav_pcm_formats_test.cpp -o build/test-wav-pcm-formats $(UI_BASE_ARCHIVE)
build/test-sampler-edge-time: tests/sampler_edge_time_test.cpp $(UI_BASE_ARCHIVE)
	$(CXX) -std=c++17 -O2 -Isrc -Wall -Wextra -Werror -fsanitize=address,undefined tests/sampler_edge_time_test.cpp -o build/test-sampler-edge-time $(UI_BASE_ARCHIVE)
build/test-public-overdrive: tests/public_overdrive_test.cpp third_party/daisysp/Source/Effects/overdrive.cpp $(UI_BASE_ARCHIVE)
	$(CXX) -std=c++17 -O2 -Isrc -Ithird_party/daisysp/Source -Ithird_party/daisysp/Source/Utility -Wall -Wextra -Werror -fsanitize=address,undefined tests/public_overdrive_test.cpp third_party/daisysp/Source/Effects/overdrive.cpp third_party/daisysp/Source/Control/phasor.cpp -o build/test-public-overdrive $(UI_BASE_ARCHIVE)
build/test-public-fm2: tests/public_fm2_test.cpp third_party/daisysp/Source/Synthesis/fm2.cpp third_party/daisysp/Source/Synthesis/oscillator.cpp $(UI_BASE_ARCHIVE)
	$(CXX) -std=c++17 -O2 -Ithird_party/daisysp/Source -Ithird_party/daisysp/Source/Utility -Wall -Wextra -Werror -fsanitize=address,undefined tests/public_fm2_test.cpp third_party/daisysp/Source/Synthesis/fm2.cpp third_party/daisysp/Source/Synthesis/oscillator.cpp -o build/test-public-fm2 $(UI_BASE_ARCHIVE)
build/test-public-wavefolder: tests/public_wavefolder_test.cpp third_party/daisysp/Source/Effects/wavefolder.cpp $(UI_BASE_ARCHIVE)
	$(CXX) -std=c++17 -O2 -Isrc -Ithird_party/daisysp/Source -Ithird_party/daisysp/Source/Utility -Wall -Wextra -Werror -fsanitize=address,undefined tests/public_wavefolder_test.cpp third_party/daisysp/Source/Effects/wavefolder.cpp -o build/test-public-wavefolder $(UI_BASE_ARCHIVE)
build/test-daisy-demo-project: tests/daisy_demo_project_test.cpp $(UI_BASE_ARCHIVE)
	$(CXX) -std=c++17 -O2 -Isrc -Wall -Wextra -Werror -fsanitize=address,undefined tests/daisy_demo_project_test.cpp -o build/test-daisy-demo-project $(UI_BASE_ARCHIVE)
build/test-public-drums: tests/public_drum_bank_test.cpp $(filter third_party/daisysp/%,$(DAISY)) $(UI_BASE_ARCHIVE)
	$(CXX) -std=c++17 -O2 -Isrc -Ithird_party/daisysp/Source -Ithird_party/daisysp/Source/Utility -Wall -Wextra -Werror -Wno-unused-parameter -fsanitize=address,undefined tests/public_drum_bank_test.cpp $(filter third_party/daisysp/%,$(DAISY)) -o build/test-public-drums $(UI_BASE_ARCHIVE)
build/test-daisy-engine: tests/daisy_engine_test.cpp build/test-core-engine.o $(HOST_SUPPORT_OBJECTS) build/plaits/plaits/dsp/engine2/chiptune_engine.o build/plaits/plaits/dsp/engine2/virtual_analog_vcf_engine.o build/plaits/plaits/dsp/engine2/wave_terrain_engine.o build/plaits/plaits/dsp/engine2/six_op_engine.o build/plaits/plaits/dsp/engine2/phase_distortion_engine.o build/plaits/plaits/dsp/engine2/string_machine_engine.o build/plaits/plaits/dsp/speech/lpc_speech_synth.o build/plaits/plaits/dsp/speech/naive_speech_synth.o build/plaits/plaits/dsp/speech/sam_speech_synth.o build/plaits/plaits/dsp/speech/lpc_speech_synth_phonemes.o build/plaits/plaits/dsp/speech/lpc_speech_synth_words.o build/plaits/plaits/dsp/speech/lpc_speech_synth_controller.o build/plaits/plaits/dsp/engine/particle_engine.o build/plaits/plaits/dsp/engine/modal_engine.o build/plaits/plaits/dsp/engine/noise_engine.o build/plaits/plaits/dsp/engine/waveshaping_engine.o build/plaits/plaits/dsp/engine/string_engine.o build/plaits/plaits/dsp/engine/bass_drum_engine.o build/plaits/plaits/dsp/engine/swarm_engine.o build/plaits/plaits/dsp/engine/wavetable_engine.o build/plaits/plaits/dsp/engine/snare_drum_engine.o build/plaits/plaits/dsp/engine/chord_engine.o build/plaits/plaits/dsp/engine/fm_engine.o build/plaits/plaits/dsp/engine/grain_engine.o build/plaits/plaits/dsp/engine/additive_engine.o build/plaits/plaits/dsp/engine/hi_hat_engine.o build/plaits/plaits/dsp/engine/speech_engine.o build/plaits/plaits/dsp/engine/virtual_analog_engine.o build/plaits/plaits/dsp/voice.o build/plaits/plaits/dsp/fm/dx_units.o build/plaits/plaits/dsp/fm/algorithms.o build/plaits/plaits/dsp/chords/chord_bank.o build/plaits/plaits/dsp/physical_modelling/modal_voice.o build/plaits/plaits/dsp/physical_modelling/string_voice.o build/plaits/plaits/dsp/physical_modelling/resonator.o build/plaits/plaits/dsp/physical_modelling/string.o build/plaits/plaits/resources.o build/plaits/stmlib/dsp/units.o build/plaits/stmlib/utils/random.o $(RING_OBJECTS) $(UI_BASE_ARCHIVE)
	$(CXX) -std=c++17 -DGROOVE_PLAITS -DTEST -Isrc -Ithird_party/mutable -Ithird_party/daisysp/Source -Ithird_party/daisysp/Source/Utility -O1 -Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-variable -Wno-unused-local-typedefs -fsanitize=address,undefined tests/daisy_engine_test.cpp build/test-core-engine.o $(HOST_SUPPORT_OBJECTS) build/plaits/plaits/dsp/engine2/chiptune_engine.o build/plaits/plaits/dsp/engine2/virtual_analog_vcf_engine.o build/plaits/plaits/dsp/engine2/wave_terrain_engine.o build/plaits/plaits/dsp/engine2/six_op_engine.o build/plaits/plaits/dsp/engine2/phase_distortion_engine.o build/plaits/plaits/dsp/engine2/string_machine_engine.o build/plaits/plaits/dsp/speech/lpc_speech_synth.o build/plaits/plaits/dsp/speech/naive_speech_synth.o build/plaits/plaits/dsp/speech/sam_speech_synth.o build/plaits/plaits/dsp/speech/lpc_speech_synth_phonemes.o build/plaits/plaits/dsp/speech/lpc_speech_synth_words.o build/plaits/plaits/dsp/speech/lpc_speech_synth_controller.o build/plaits/plaits/dsp/engine/particle_engine.o build/plaits/plaits/dsp/engine/modal_engine.o build/plaits/plaits/dsp/engine/noise_engine.o build/plaits/plaits/dsp/engine/waveshaping_engine.o build/plaits/plaits/dsp/engine/string_engine.o build/plaits/plaits/dsp/engine/bass_drum_engine.o build/plaits/plaits/dsp/engine/swarm_engine.o build/plaits/plaits/dsp/engine/wavetable_engine.o build/plaits/plaits/dsp/engine/snare_drum_engine.o build/plaits/plaits/dsp/engine/chord_engine.o build/plaits/plaits/dsp/engine/fm_engine.o build/plaits/plaits/dsp/engine/grain_engine.o build/plaits/plaits/dsp/engine/additive_engine.o build/plaits/plaits/dsp/engine/hi_hat_engine.o build/plaits/plaits/dsp/engine/speech_engine.o build/plaits/plaits/dsp/engine/virtual_analog_engine.o build/plaits/plaits/dsp/voice.o build/plaits/plaits/dsp/fm/dx_units.o build/plaits/plaits/dsp/fm/algorithms.o build/plaits/plaits/dsp/chords/chord_bank.o build/plaits/plaits/dsp/physical_modelling/modal_voice.o build/plaits/plaits/dsp/physical_modelling/string_voice.o build/plaits/plaits/dsp/physical_modelling/resonator.o build/plaits/plaits/dsp/physical_modelling/string.o build/plaits/plaits/resources.o build/plaits/stmlib/dsp/units.o build/plaits/stmlib/utils/random.o $(RING_OBJECTS) -o build/test-daisy-engine $(UI_BASE_ARCHIVE)
build/test-benchmark-requests: tests/benchmark_requests_test.cpp $(UI_BASE_ARCHIVE)
	$(CXX) -std=c++17 -Isrc -Wall -Wextra -Werror -fsanitize=address,undefined tests/benchmark_requests_test.cpp -o build/test-benchmark-requests $(UI_BASE_ARCHIVE)
build/test-plaits-stress-project: tests/plaits_stress_project_test.cpp $(UI_BASE_ARCHIVE)
	$(CXX) -std=c++17 -Isrc -O1 -Wall -Wextra -Werror -fsanitize=address,undefined tests/plaits_stress_project_test.cpp -o build/test-plaits-stress-project $(UI_BASE_ARCHIVE)
build/test-benchmark-project: tests/benchmark_project_test.cpp $(UI_BASE_ARCHIVE)
	$(CXX) -std=c++17 -Isrc -Wall -Wextra -Werror -fsanitize=address,undefined tests/benchmark_project_test.cpp -o build/test-benchmark-project $(UI_BASE_ARCHIVE)
build/test-clean-save: tests/clean_save_test.cpp $(UI_BASE_ARCHIVE)
	$(CXX) -std=c++17 -DGROOVE_PLAITS -DTEST -Isrc -Ithird_party/mutable -Ithird_party/daisysp/Source -Ithird_party/daisysp/Source/Utility -Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-variable -Wno-unused-local-typedefs -fsanitize=address,undefined tests/clean_save_test.cpp -o build/test-clean-save $(UI_BASE_ARCHIVE)
build/test-benchmark-cleanup: tests/benchmark_cleanup_policy_test.cpp $(UI_BASE_ARCHIVE)
	$(CXX) -std=c++17 -Isrc -Wall -Wextra -Werror -fsanitize=address,undefined tests/benchmark_cleanup_policy_test.cpp -o build/test-benchmark-cleanup $(UI_BASE_ARCHIVE)
build/test-group-routing-cache: tests/group_routing_cache_test.cpp $(UI_BASE_ARCHIVE)
	$(CXX) -std=c++17 -Isrc -Wall -Wextra -Werror -fsanitize=address,undefined tests/group_routing_cache_test.cpp -o build/test-group-routing-cache $(UI_BASE_ARCHIVE)
build/test-display-rectangle: tests/display_rectangle_test.cpp src/display_rectangle.h src/display_job_slot.h src/display_rle.h src/mk3_output.h src/ui/framebuffer565.h $(UI_BASE_ARCHIVE)
	$(CXX) -std=c++17 -Isrc -Wall -Wextra -Werror -MMD -MP -MF $@.d -MT $@ -fsanitize=address,undefined tests/display_rectangle_test.cpp -o build/test-display-rectangle $(UI_BASE_ARCHIVE)
build/test-display-job-slot: tests/display_job_slot_test.cpp src/display_job_slot.h src/display_rectangle.h src/display_rle.h src/mk3_output.h src/ui/framebuffer565.h $(UI_BASE_ARCHIVE)
	$(CXX) -std=c++17 -Isrc -Wall -Wextra -Werror -MMD -MP -MF $@.d -MT $@ -pthread -fsanitize=address,undefined tests/display_job_slot_test.cpp -o build/test-display-job-slot $(UI_BASE_ARCHIVE)
build/test-visual-snapshot: tests/visual_snapshot_test.cpp build/test-core-engine.o $(HOST_SUPPORT_OBJECTS) build/plaits/plaits/dsp/engine2/chiptune_engine.o build/plaits/plaits/dsp/engine2/virtual_analog_vcf_engine.o build/plaits/plaits/dsp/engine2/wave_terrain_engine.o build/plaits/plaits/dsp/engine2/six_op_engine.o build/plaits/plaits/dsp/engine2/phase_distortion_engine.o build/plaits/plaits/dsp/engine2/string_machine_engine.o build/plaits/plaits/dsp/speech/lpc_speech_synth.o build/plaits/plaits/dsp/speech/naive_speech_synth.o build/plaits/plaits/dsp/speech/sam_speech_synth.o build/plaits/plaits/dsp/speech/lpc_speech_synth_phonemes.o build/plaits/plaits/dsp/speech/lpc_speech_synth_words.o build/plaits/plaits/dsp/speech/lpc_speech_synth_controller.o build/plaits/plaits/dsp/engine/particle_engine.o build/plaits/plaits/dsp/engine/modal_engine.o build/plaits/plaits/dsp/engine/noise_engine.o build/plaits/plaits/dsp/engine/waveshaping_engine.o build/plaits/plaits/dsp/engine/string_engine.o build/plaits/plaits/dsp/engine/bass_drum_engine.o build/plaits/plaits/dsp/engine/swarm_engine.o build/plaits/plaits/dsp/engine/wavetable_engine.o build/plaits/plaits/dsp/engine/snare_drum_engine.o build/plaits/plaits/dsp/engine/chord_engine.o build/plaits/plaits/dsp/engine/fm_engine.o build/plaits/plaits/dsp/engine/grain_engine.o build/plaits/plaits/dsp/engine/additive_engine.o build/plaits/plaits/dsp/engine/hi_hat_engine.o build/plaits/plaits/dsp/engine/speech_engine.o build/plaits/plaits/dsp/engine/virtual_analog_engine.o build/plaits/plaits/dsp/voice.o build/plaits/plaits/dsp/fm/dx_units.o build/plaits/plaits/dsp/fm/algorithms.o build/plaits/plaits/dsp/chords/chord_bank.o build/plaits/plaits/dsp/physical_modelling/modal_voice.o build/plaits/plaits/dsp/physical_modelling/string_voice.o build/plaits/plaits/dsp/physical_modelling/resonator.o build/plaits/plaits/dsp/physical_modelling/string.o build/plaits/plaits/resources.o build/plaits/stmlib/dsp/units.o build/plaits/stmlib/utils/random.o $(RING_OBJECTS) $(UI_BASE_ARCHIVE)
	$(CXX) -std=c++17 -DGROOVE_PLAITS -DTEST -Isrc -Ithird_party/mutable -Ithird_party/daisysp/Source -Ithird_party/daisysp/Source/Utility -Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-variable -Wno-unused-local-typedefs -fsanitize=address,undefined tests/visual_snapshot_test.cpp build/test-core-engine.o $(HOST_SUPPORT_OBJECTS) build/plaits/plaits/dsp/engine2/chiptune_engine.o build/plaits/plaits/dsp/engine2/virtual_analog_vcf_engine.o build/plaits/plaits/dsp/engine2/wave_terrain_engine.o build/plaits/plaits/dsp/engine2/six_op_engine.o build/plaits/plaits/dsp/engine2/phase_distortion_engine.o build/plaits/plaits/dsp/engine2/string_machine_engine.o build/plaits/plaits/dsp/speech/lpc_speech_synth.o build/plaits/plaits/dsp/speech/naive_speech_synth.o build/plaits/plaits/dsp/speech/sam_speech_synth.o build/plaits/plaits/dsp/speech/lpc_speech_synth_phonemes.o build/plaits/plaits/dsp/speech/lpc_speech_synth_words.o build/plaits/plaits/dsp/speech/lpc_speech_synth_controller.o build/plaits/plaits/dsp/engine/particle_engine.o build/plaits/plaits/dsp/engine/modal_engine.o build/plaits/plaits/dsp/engine/noise_engine.o build/plaits/plaits/dsp/engine/waveshaping_engine.o build/plaits/plaits/dsp/engine/string_engine.o build/plaits/plaits/dsp/engine/bass_drum_engine.o build/plaits/plaits/dsp/engine/swarm_engine.o build/plaits/plaits/dsp/engine/wavetable_engine.o build/plaits/plaits/dsp/engine/snare_drum_engine.o build/plaits/plaits/dsp/engine/chord_engine.o build/plaits/plaits/dsp/engine/fm_engine.o build/plaits/plaits/dsp/engine/grain_engine.o build/plaits/plaits/dsp/engine/additive_engine.o build/plaits/plaits/dsp/engine/hi_hat_engine.o build/plaits/plaits/dsp/engine/speech_engine.o build/plaits/plaits/dsp/engine/virtual_analog_engine.o build/plaits/plaits/dsp/voice.o build/plaits/plaits/dsp/fm/dx_units.o build/plaits/plaits/dsp/fm/algorithms.o build/plaits/plaits/dsp/chords/chord_bank.o build/plaits/plaits/dsp/physical_modelling/modal_voice.o build/plaits/plaits/dsp/physical_modelling/string_voice.o build/plaits/plaits/dsp/physical_modelling/resonator.o build/plaits/plaits/dsp/physical_modelling/string.o build/plaits/plaits/resources.o build/plaits/stmlib/dsp/units.o build/plaits/stmlib/utils/random.o $(RING_OBJECTS) -o build/test-visual-snapshot $(UI_BASE_ARCHIVE)
build/test-timing-boundary: tests/timing_boundary_test.cpp build/test-core-engine.o $(HOST_SUPPORT_OBJECTS) build/plaits/plaits/dsp/engine2/chiptune_engine.o build/plaits/plaits/dsp/engine2/virtual_analog_vcf_engine.o build/plaits/plaits/dsp/engine2/wave_terrain_engine.o build/plaits/plaits/dsp/engine2/six_op_engine.o build/plaits/plaits/dsp/engine2/phase_distortion_engine.o build/plaits/plaits/dsp/engine2/string_machine_engine.o build/plaits/plaits/dsp/speech/lpc_speech_synth.o build/plaits/plaits/dsp/speech/naive_speech_synth.o build/plaits/plaits/dsp/speech/sam_speech_synth.o build/plaits/plaits/dsp/speech/lpc_speech_synth_phonemes.o build/plaits/plaits/dsp/speech/lpc_speech_synth_words.o build/plaits/plaits/dsp/speech/lpc_speech_synth_controller.o build/plaits/plaits/dsp/engine/particle_engine.o build/plaits/plaits/dsp/engine/modal_engine.o build/plaits/plaits/dsp/engine/noise_engine.o build/plaits/plaits/dsp/engine/waveshaping_engine.o build/plaits/plaits/dsp/engine/string_engine.o build/plaits/plaits/dsp/engine/bass_drum_engine.o build/plaits/plaits/dsp/engine/swarm_engine.o build/plaits/plaits/dsp/engine/wavetable_engine.o build/plaits/plaits/dsp/engine/snare_drum_engine.o build/plaits/plaits/dsp/engine/chord_engine.o build/plaits/plaits/dsp/engine/fm_engine.o build/plaits/plaits/dsp/engine/grain_engine.o build/plaits/plaits/dsp/engine/additive_engine.o build/plaits/plaits/dsp/engine/hi_hat_engine.o build/plaits/plaits/dsp/engine/speech_engine.o build/plaits/plaits/dsp/engine/virtual_analog_engine.o build/plaits/plaits/dsp/voice.o build/plaits/plaits/dsp/fm/dx_units.o build/plaits/plaits/dsp/fm/algorithms.o build/plaits/plaits/dsp/chords/chord_bank.o build/plaits/plaits/dsp/physical_modelling/modal_voice.o build/plaits/plaits/dsp/physical_modelling/string_voice.o build/plaits/plaits/dsp/physical_modelling/resonator.o build/plaits/plaits/dsp/physical_modelling/string.o build/plaits/plaits/resources.o build/plaits/stmlib/dsp/units.o build/plaits/stmlib/utils/random.o $(RING_OBJECTS) $(UI_BASE_ARCHIVE)
	$(CXX) -std=c++17 -DGROOVE_PLAITS -DTEST -Isrc -Ithird_party/mutable -Ithird_party/daisysp/Source -Ithird_party/daisysp/Source/Utility -Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-variable -Wno-unused-local-typedefs -fsanitize=address,undefined tests/timing_boundary_test.cpp build/test-core-engine.o $(HOST_SUPPORT_OBJECTS) build/plaits/plaits/dsp/engine2/chiptune_engine.o build/plaits/plaits/dsp/engine2/virtual_analog_vcf_engine.o build/plaits/plaits/dsp/engine2/wave_terrain_engine.o build/plaits/plaits/dsp/engine2/six_op_engine.o build/plaits/plaits/dsp/engine2/phase_distortion_engine.o build/plaits/plaits/dsp/engine2/string_machine_engine.o build/plaits/plaits/dsp/speech/lpc_speech_synth.o build/plaits/plaits/dsp/speech/naive_speech_synth.o build/plaits/plaits/dsp/speech/sam_speech_synth.o build/plaits/plaits/dsp/speech/lpc_speech_synth_phonemes.o build/plaits/plaits/dsp/speech/lpc_speech_synth_words.o build/plaits/plaits/dsp/speech/lpc_speech_synth_controller.o build/plaits/plaits/dsp/engine/particle_engine.o build/plaits/plaits/dsp/engine/modal_engine.o build/plaits/plaits/dsp/engine/noise_engine.o build/plaits/plaits/dsp/engine/waveshaping_engine.o build/plaits/plaits/dsp/engine/string_engine.o build/plaits/plaits/dsp/engine/bass_drum_engine.o build/plaits/plaits/dsp/engine/swarm_engine.o build/plaits/plaits/dsp/engine/wavetable_engine.o build/plaits/plaits/dsp/engine/snare_drum_engine.o build/plaits/plaits/dsp/engine/chord_engine.o build/plaits/plaits/dsp/engine/fm_engine.o build/plaits/plaits/dsp/engine/grain_engine.o build/plaits/plaits/dsp/engine/additive_engine.o build/plaits/plaits/dsp/engine/hi_hat_engine.o build/plaits/plaits/dsp/engine/speech_engine.o build/plaits/plaits/dsp/engine/virtual_analog_engine.o build/plaits/plaits/dsp/voice.o build/plaits/plaits/dsp/fm/dx_units.o build/plaits/plaits/dsp/fm/algorithms.o build/plaits/plaits/dsp/chords/chord_bank.o build/plaits/plaits/dsp/physical_modelling/modal_voice.o build/plaits/plaits/dsp/physical_modelling/string_voice.o build/plaits/plaits/dsp/physical_modelling/resonator.o build/plaits/plaits/dsp/physical_modelling/string.o build/plaits/plaits/resources.o build/plaits/stmlib/dsp/units.o build/plaits/stmlib/utils/random.o $(RING_OBJECTS) -o build/test-timing-boundary $(UI_BASE_ARCHIVE)
build/test: tests/test.cpp build/test-core-engine.o $(HOST_SUPPORT_OBJECTS) build/plaits/plaits/dsp/engine2/chiptune_engine.o build/plaits/plaits/dsp/engine2/virtual_analog_vcf_engine.o build/plaits/plaits/dsp/engine2/wave_terrain_engine.o build/plaits/plaits/dsp/engine2/six_op_engine.o build/plaits/plaits/dsp/engine2/phase_distortion_engine.o build/plaits/plaits/dsp/engine2/string_machine_engine.o build/plaits/plaits/dsp/speech/lpc_speech_synth.o build/plaits/plaits/dsp/speech/naive_speech_synth.o build/plaits/plaits/dsp/speech/sam_speech_synth.o build/plaits/plaits/dsp/speech/lpc_speech_synth_phonemes.o build/plaits/plaits/dsp/speech/lpc_speech_synth_words.o build/plaits/plaits/dsp/speech/lpc_speech_synth_controller.o build/plaits/plaits/dsp/engine/particle_engine.o build/plaits/plaits/dsp/engine/modal_engine.o build/plaits/plaits/dsp/engine/noise_engine.o build/plaits/plaits/dsp/engine/waveshaping_engine.o build/plaits/plaits/dsp/engine/string_engine.o build/plaits/plaits/dsp/engine/bass_drum_engine.o build/plaits/plaits/dsp/engine/swarm_engine.o build/plaits/plaits/dsp/engine/wavetable_engine.o build/plaits/plaits/dsp/engine/snare_drum_engine.o build/plaits/plaits/dsp/engine/chord_engine.o build/plaits/plaits/dsp/engine/fm_engine.o build/plaits/plaits/dsp/engine/grain_engine.o build/plaits/plaits/dsp/engine/additive_engine.o build/plaits/plaits/dsp/engine/hi_hat_engine.o build/plaits/plaits/dsp/engine/speech_engine.o build/plaits/plaits/dsp/engine/virtual_analog_engine.o build/plaits/plaits/dsp/voice.o build/plaits/plaits/dsp/fm/dx_units.o build/plaits/plaits/dsp/fm/algorithms.o build/plaits/plaits/dsp/chords/chord_bank.o build/plaits/plaits/dsp/physical_modelling/modal_voice.o build/plaits/plaits/dsp/physical_modelling/string_voice.o build/plaits/plaits/dsp/physical_modelling/resonator.o build/plaits/plaits/dsp/physical_modelling/string.o build/plaits/plaits/resources.o build/plaits/stmlib/dsp/units.o build/plaits/stmlib/utils/random.o $(RING_OBJECTS) $(UI_BASE_ARCHIVE)
	$(CXX) -std=c++17 -DGROOVE_PLAITS -DTEST -Isrc -Ithird_party/mutable -Ithird_party/daisysp/Source -Ithird_party/daisysp/Source/Utility -Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-variable -Wno-unused-local-typedefs -fsanitize=address,undefined tests/test.cpp build/test-core-engine.o $(HOST_SUPPORT_OBJECTS) build/plaits/plaits/dsp/engine2/chiptune_engine.o build/plaits/plaits/dsp/engine2/virtual_analog_vcf_engine.o build/plaits/plaits/dsp/engine2/wave_terrain_engine.o build/plaits/plaits/dsp/engine2/six_op_engine.o build/plaits/plaits/dsp/engine2/phase_distortion_engine.o build/plaits/plaits/dsp/engine2/string_machine_engine.o build/plaits/plaits/dsp/speech/lpc_speech_synth.o build/plaits/plaits/dsp/speech/naive_speech_synth.o build/plaits/plaits/dsp/speech/sam_speech_synth.o build/plaits/plaits/dsp/speech/lpc_speech_synth_phonemes.o build/plaits/plaits/dsp/speech/lpc_speech_synth_words.o build/plaits/plaits/dsp/speech/lpc_speech_synth_controller.o build/plaits/plaits/dsp/engine/particle_engine.o build/plaits/plaits/dsp/engine/modal_engine.o build/plaits/plaits/dsp/engine/noise_engine.o build/plaits/plaits/dsp/engine/waveshaping_engine.o build/plaits/plaits/dsp/engine/string_engine.o build/plaits/plaits/dsp/engine/bass_drum_engine.o build/plaits/plaits/dsp/engine/swarm_engine.o build/plaits/plaits/dsp/engine/wavetable_engine.o build/plaits/plaits/dsp/engine/snare_drum_engine.o build/plaits/plaits/dsp/engine/chord_engine.o build/plaits/plaits/dsp/engine/fm_engine.o build/plaits/plaits/dsp/engine/grain_engine.o build/plaits/plaits/dsp/engine/additive_engine.o build/plaits/plaits/dsp/engine/hi_hat_engine.o build/plaits/plaits/dsp/engine/speech_engine.o build/plaits/plaits/dsp/engine/virtual_analog_engine.o build/plaits/plaits/dsp/voice.o build/plaits/plaits/dsp/fm/dx_units.o build/plaits/plaits/dsp/fm/algorithms.o build/plaits/plaits/dsp/chords/chord_bank.o build/plaits/plaits/dsp/physical_modelling/modal_voice.o build/plaits/plaits/dsp/physical_modelling/string_voice.o build/plaits/plaits/dsp/physical_modelling/resonator.o build/plaits/plaits/dsp/physical_modelling/string.o build/plaits/plaits/resources.o build/plaits/stmlib/dsp/units.o build/plaits/stmlib/utils/random.o $(RING_OBJECTS) -o build/test $(UI_BASE_ARCHIVE)
build/test-plaits: tests/plaits_test.cpp build/test-core-engine.o $(HOST_SUPPORT_OBJECTS) build/plaits/plaits/dsp/engine2/chiptune_engine.o build/plaits/plaits/dsp/engine2/virtual_analog_vcf_engine.o build/plaits/plaits/dsp/engine2/wave_terrain_engine.o build/plaits/plaits/dsp/engine2/six_op_engine.o build/plaits/plaits/dsp/engine2/phase_distortion_engine.o build/plaits/plaits/dsp/engine2/string_machine_engine.o build/plaits/plaits/dsp/speech/lpc_speech_synth.o build/plaits/plaits/dsp/speech/naive_speech_synth.o build/plaits/plaits/dsp/speech/sam_speech_synth.o build/plaits/plaits/dsp/speech/lpc_speech_synth_phonemes.o build/plaits/plaits/dsp/speech/lpc_speech_synth_words.o build/plaits/plaits/dsp/speech/lpc_speech_synth_controller.o build/plaits/plaits/dsp/engine/particle_engine.o build/plaits/plaits/dsp/engine/modal_engine.o build/plaits/plaits/dsp/engine/noise_engine.o build/plaits/plaits/dsp/engine/waveshaping_engine.o build/plaits/plaits/dsp/engine/string_engine.o build/plaits/plaits/dsp/engine/bass_drum_engine.o build/plaits/plaits/dsp/engine/swarm_engine.o build/plaits/plaits/dsp/engine/wavetable_engine.o build/plaits/plaits/dsp/engine/snare_drum_engine.o build/plaits/plaits/dsp/engine/chord_engine.o build/plaits/plaits/dsp/engine/fm_engine.o build/plaits/plaits/dsp/engine/grain_engine.o build/plaits/plaits/dsp/engine/additive_engine.o build/plaits/plaits/dsp/engine/hi_hat_engine.o build/plaits/plaits/dsp/engine/speech_engine.o build/plaits/plaits/dsp/engine/virtual_analog_engine.o build/plaits/plaits/dsp/voice.o build/plaits/plaits/dsp/fm/dx_units.o build/plaits/plaits/dsp/fm/algorithms.o build/plaits/plaits/dsp/chords/chord_bank.o build/plaits/plaits/dsp/physical_modelling/modal_voice.o build/plaits/plaits/dsp/physical_modelling/string_voice.o build/plaits/plaits/dsp/physical_modelling/resonator.o build/plaits/plaits/dsp/physical_modelling/string.o build/plaits/plaits/resources.o build/plaits/stmlib/dsp/units.o build/plaits/stmlib/utils/random.o $(RING_OBJECTS) $(UI_BASE_ARCHIVE)
	$(CXX) -std=c++17 -DGROOVE_PLAITS -DTEST -Isrc -Ithird_party/mutable -Ithird_party/daisysp/Source -Ithird_party/daisysp/Source/Utility -Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-variable -Wno-unused-local-typedefs -fsanitize=address,undefined tests/plaits_test.cpp build/test-core-engine.o $(HOST_SUPPORT_OBJECTS) build/plaits/plaits/dsp/engine2/chiptune_engine.o build/plaits/plaits/dsp/engine2/virtual_analog_vcf_engine.o build/plaits/plaits/dsp/engine2/wave_terrain_engine.o build/plaits/plaits/dsp/engine2/six_op_engine.o build/plaits/plaits/dsp/engine2/phase_distortion_engine.o build/plaits/plaits/dsp/engine2/string_machine_engine.o build/plaits/plaits/dsp/speech/lpc_speech_synth.o build/plaits/plaits/dsp/speech/naive_speech_synth.o build/plaits/plaits/dsp/speech/sam_speech_synth.o build/plaits/plaits/dsp/speech/lpc_speech_synth_phonemes.o build/plaits/plaits/dsp/speech/lpc_speech_synth_words.o build/plaits/plaits/dsp/speech/lpc_speech_synth_controller.o build/plaits/plaits/dsp/engine/particle_engine.o build/plaits/plaits/dsp/engine/modal_engine.o build/plaits/plaits/dsp/engine/noise_engine.o build/plaits/plaits/dsp/engine/waveshaping_engine.o build/plaits/plaits/dsp/engine/string_engine.o build/plaits/plaits/dsp/engine/bass_drum_engine.o build/plaits/plaits/dsp/engine/swarm_engine.o build/plaits/plaits/dsp/engine/wavetable_engine.o build/plaits/plaits/dsp/engine/snare_drum_engine.o build/plaits/plaits/dsp/engine/chord_engine.o build/plaits/plaits/dsp/engine/fm_engine.o build/plaits/plaits/dsp/engine/grain_engine.o build/plaits/plaits/dsp/engine/additive_engine.o build/plaits/plaits/dsp/engine/hi_hat_engine.o build/plaits/plaits/dsp/engine/speech_engine.o build/plaits/plaits/dsp/engine/virtual_analog_engine.o build/plaits/plaits/dsp/voice.o build/plaits/plaits/dsp/fm/dx_units.o build/plaits/plaits/dsp/fm/algorithms.o build/plaits/plaits/dsp/chords/chord_bank.o build/plaits/plaits/dsp/physical_modelling/modal_voice.o build/plaits/plaits/dsp/physical_modelling/string_voice.o build/plaits/plaits/dsp/physical_modelling/resonator.o build/plaits/plaits/dsp/physical_modelling/string.o build/plaits/plaits/resources.o build/plaits/stmlib/dsp/units.o build/plaits/stmlib/utils/random.o $(RING_OBJECTS) -o build/test-plaits $(UI_BASE_ARCHIVE)
build/test-project-codec: tests/project_codec_test.cpp $(UI_BASE_ARCHIVE)
	$(CXX) -std=c++17 -DGROOVE_PLAITS -DTEST -Isrc -Ithird_party/mutable -Ithird_party/daisysp/Source -Ithird_party/daisysp/Source/Utility -Wall -Wextra -Werror -fsanitize=address,undefined tests/project_codec_test.cpp -o build/test-project-codec $(UI_BASE_ARCHIVE)
build/test-page-descriptors: tests/page_descriptor_test.cpp build/test-core-engine.o $(HOST_SUPPORT_OBJECTS) build/plaits/plaits/dsp/engine2/chiptune_engine.o build/plaits/plaits/dsp/engine2/virtual_analog_vcf_engine.o build/plaits/plaits/dsp/engine2/wave_terrain_engine.o build/plaits/plaits/dsp/engine2/six_op_engine.o build/plaits/plaits/dsp/engine2/phase_distortion_engine.o build/plaits/plaits/dsp/engine2/string_machine_engine.o build/plaits/plaits/dsp/speech/lpc_speech_synth.o build/plaits/plaits/dsp/speech/naive_speech_synth.o build/plaits/plaits/dsp/speech/sam_speech_synth.o build/plaits/plaits/dsp/speech/lpc_speech_synth_phonemes.o build/plaits/plaits/dsp/speech/lpc_speech_synth_words.o build/plaits/plaits/dsp/speech/lpc_speech_synth_controller.o build/plaits/plaits/dsp/engine/particle_engine.o build/plaits/plaits/dsp/engine/modal_engine.o build/plaits/plaits/dsp/engine/noise_engine.o build/plaits/plaits/dsp/engine/waveshaping_engine.o build/plaits/plaits/dsp/engine/string_engine.o build/plaits/plaits/dsp/engine/bass_drum_engine.o build/plaits/plaits/dsp/engine/swarm_engine.o build/plaits/plaits/dsp/engine/wavetable_engine.o build/plaits/plaits/dsp/engine/snare_drum_engine.o build/plaits/plaits/dsp/engine/chord_engine.o build/plaits/plaits/dsp/engine/fm_engine.o build/plaits/plaits/dsp/engine/grain_engine.o build/plaits/plaits/dsp/engine/additive_engine.o build/plaits/plaits/dsp/engine/hi_hat_engine.o build/plaits/plaits/dsp/engine/speech_engine.o build/plaits/plaits/dsp/engine/virtual_analog_engine.o build/plaits/plaits/dsp/voice.o build/plaits/plaits/dsp/fm/dx_units.o build/plaits/plaits/dsp/fm/algorithms.o build/plaits/plaits/dsp/chords/chord_bank.o build/plaits/plaits/dsp/physical_modelling/modal_voice.o build/plaits/plaits/dsp/physical_modelling/string_voice.o build/plaits/plaits/dsp/physical_modelling/resonator.o build/plaits/plaits/dsp/physical_modelling/string.o build/plaits/plaits/resources.o build/plaits/stmlib/dsp/units.o build/plaits/stmlib/utils/random.o $(RING_OBJECTS) $(UI_BASE_ARCHIVE)
	$(CXX) -std=c++17 -DGROOVE_PLAITS -DTEST -Isrc -Ithird_party/mutable -Ithird_party/daisysp/Source -Ithird_party/daisysp/Source/Utility -Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-variable -Wno-unused-local-typedefs -fsanitize=address,undefined tests/page_descriptor_test.cpp build/test-core-engine.o $(HOST_SUPPORT_OBJECTS) build/plaits/plaits/dsp/engine2/chiptune_engine.o build/plaits/plaits/dsp/engine2/virtual_analog_vcf_engine.o build/plaits/plaits/dsp/engine2/wave_terrain_engine.o build/plaits/plaits/dsp/engine2/six_op_engine.o build/plaits/plaits/dsp/engine2/phase_distortion_engine.o build/plaits/plaits/dsp/engine2/string_machine_engine.o build/plaits/plaits/dsp/speech/lpc_speech_synth.o build/plaits/plaits/dsp/speech/naive_speech_synth.o build/plaits/plaits/dsp/speech/sam_speech_synth.o build/plaits/plaits/dsp/speech/lpc_speech_synth_phonemes.o build/plaits/plaits/dsp/speech/lpc_speech_synth_words.o build/plaits/plaits/dsp/speech/lpc_speech_synth_controller.o build/plaits/plaits/dsp/engine/particle_engine.o build/plaits/plaits/dsp/engine/modal_engine.o build/plaits/plaits/dsp/engine/noise_engine.o build/plaits/plaits/dsp/engine/waveshaping_engine.o build/plaits/plaits/dsp/engine/string_engine.o build/plaits/plaits/dsp/engine/bass_drum_engine.o build/plaits/plaits/dsp/engine/swarm_engine.o build/plaits/plaits/dsp/engine/wavetable_engine.o build/plaits/plaits/dsp/engine/snare_drum_engine.o build/plaits/plaits/dsp/engine/chord_engine.o build/plaits/plaits/dsp/engine/fm_engine.o build/plaits/plaits/dsp/engine/grain_engine.o build/plaits/plaits/dsp/engine/additive_engine.o build/plaits/plaits/dsp/engine/hi_hat_engine.o build/plaits/plaits/dsp/engine/speech_engine.o build/plaits/plaits/dsp/engine/virtual_analog_engine.o build/plaits/plaits/dsp/voice.o build/plaits/plaits/dsp/fm/dx_units.o build/plaits/plaits/dsp/fm/algorithms.o build/plaits/plaits/dsp/chords/chord_bank.o build/plaits/plaits/dsp/physical_modelling/modal_voice.o build/plaits/plaits/dsp/physical_modelling/string_voice.o build/plaits/plaits/dsp/physical_modelling/resonator.o build/plaits/plaits/dsp/physical_modelling/string.o build/plaits/plaits/resources.o build/plaits/stmlib/dsp/units.o build/plaits/stmlib/utils/random.o $(RING_OBJECTS) -o build/test-page-descriptors $(UI_BASE_ARCHIVE)
build/test-page-nav: tests/page_nav_test.cpp build/test-core-engine.o $(HOST_SUPPORT_OBJECTS) build/plaits/plaits/dsp/engine2/chiptune_engine.o build/plaits/plaits/dsp/engine2/virtual_analog_vcf_engine.o build/plaits/plaits/dsp/engine2/wave_terrain_engine.o build/plaits/plaits/dsp/engine2/six_op_engine.o build/plaits/plaits/dsp/engine2/phase_distortion_engine.o build/plaits/plaits/dsp/engine2/string_machine_engine.o build/plaits/plaits/dsp/speech/lpc_speech_synth.o build/plaits/plaits/dsp/speech/naive_speech_synth.o build/plaits/plaits/dsp/speech/sam_speech_synth.o build/plaits/plaits/dsp/speech/lpc_speech_synth_phonemes.o build/plaits/plaits/dsp/speech/lpc_speech_synth_words.o build/plaits/plaits/dsp/speech/lpc_speech_synth_controller.o build/plaits/plaits/dsp/engine/particle_engine.o build/plaits/plaits/dsp/engine/modal_engine.o build/plaits/plaits/dsp/engine/noise_engine.o build/plaits/plaits/dsp/engine/waveshaping_engine.o build/plaits/plaits/dsp/engine/string_engine.o build/plaits/plaits/dsp/engine/bass_drum_engine.o build/plaits/plaits/dsp/engine/swarm_engine.o build/plaits/plaits/dsp/engine/wavetable_engine.o build/plaits/plaits/dsp/engine/snare_drum_engine.o build/plaits/plaits/dsp/engine/chord_engine.o build/plaits/plaits/dsp/engine/fm_engine.o build/plaits/plaits/dsp/engine/grain_engine.o build/plaits/plaits/dsp/engine/additive_engine.o build/plaits/plaits/dsp/engine/hi_hat_engine.o build/plaits/plaits/dsp/engine/speech_engine.o build/plaits/plaits/dsp/engine/virtual_analog_engine.o build/plaits/plaits/dsp/voice.o build/plaits/plaits/dsp/fm/dx_units.o build/plaits/plaits/dsp/fm/algorithms.o build/plaits/plaits/dsp/chords/chord_bank.o build/plaits/plaits/dsp/physical_modelling/modal_voice.o build/plaits/plaits/dsp/physical_modelling/string_voice.o build/plaits/plaits/dsp/physical_modelling/resonator.o build/plaits/plaits/dsp/physical_modelling/string.o build/plaits/plaits/resources.o build/plaits/stmlib/dsp/units.o build/plaits/stmlib/utils/random.o $(RING_OBJECTS) $(UI_BASE_ARCHIVE)
	$(CXX) -std=c++17 -DGROOVE_PLAITS -DTEST -Isrc -Ithird_party/mutable -Ithird_party/daisysp/Source -Ithird_party/daisysp/Source/Utility -Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-variable -Wno-unused-local-typedefs -fsanitize=address,undefined tests/page_nav_test.cpp build/test-core-engine.o $(HOST_SUPPORT_OBJECTS) build/plaits/plaits/dsp/engine2/chiptune_engine.o build/plaits/plaits/dsp/engine2/virtual_analog_vcf_engine.o build/plaits/plaits/dsp/engine2/wave_terrain_engine.o build/plaits/plaits/dsp/engine2/six_op_engine.o build/plaits/plaits/dsp/engine2/phase_distortion_engine.o build/plaits/plaits/dsp/engine2/string_machine_engine.o build/plaits/plaits/dsp/speech/lpc_speech_synth.o build/plaits/plaits/dsp/speech/naive_speech_synth.o build/plaits/plaits/dsp/speech/sam_speech_synth.o build/plaits/plaits/dsp/speech/lpc_speech_synth_phonemes.o build/plaits/plaits/dsp/speech/lpc_speech_synth_words.o build/plaits/plaits/dsp/speech/lpc_speech_synth_controller.o build/plaits/plaits/dsp/engine/particle_engine.o build/plaits/plaits/dsp/engine/modal_engine.o build/plaits/plaits/dsp/engine/noise_engine.o build/plaits/plaits/dsp/engine/waveshaping_engine.o build/plaits/plaits/dsp/engine/string_engine.o build/plaits/plaits/dsp/engine/bass_drum_engine.o build/plaits/plaits/dsp/engine/swarm_engine.o build/plaits/plaits/dsp/engine/wavetable_engine.o build/plaits/plaits/dsp/engine/snare_drum_engine.o build/plaits/plaits/dsp/engine/chord_engine.o build/plaits/plaits/dsp/engine/fm_engine.o build/plaits/plaits/dsp/engine/grain_engine.o build/plaits/plaits/dsp/engine/additive_engine.o build/plaits/plaits/dsp/engine/hi_hat_engine.o build/plaits/plaits/dsp/engine/speech_engine.o build/plaits/plaits/dsp/engine/virtual_analog_engine.o build/plaits/plaits/dsp/voice.o build/plaits/plaits/dsp/fm/dx_units.o build/plaits/plaits/dsp/fm/algorithms.o build/plaits/plaits/dsp/chords/chord_bank.o build/plaits/plaits/dsp/physical_modelling/modal_voice.o build/plaits/plaits/dsp/physical_modelling/string_voice.o build/plaits/plaits/dsp/physical_modelling/resonator.o build/plaits/plaits/dsp/physical_modelling/string.o build/plaits/plaits/resources.o build/plaits/stmlib/dsp/units.o build/plaits/stmlib/utils/random.o $(RING_OBJECTS) -o build/test-page-nav $(UI_BASE_ARCHIVE)
build/test-source-select: tests/source_select_test.cpp build/test-core-engine.o $(HOST_SUPPORT_OBJECTS) build/plaits/plaits/dsp/engine2/chiptune_engine.o build/plaits/plaits/dsp/engine2/virtual_analog_vcf_engine.o build/plaits/plaits/dsp/engine2/wave_terrain_engine.o build/plaits/plaits/dsp/engine2/six_op_engine.o build/plaits/plaits/dsp/engine2/phase_distortion_engine.o build/plaits/plaits/dsp/engine2/string_machine_engine.o build/plaits/plaits/dsp/speech/lpc_speech_synth.o build/plaits/plaits/dsp/speech/naive_speech_synth.o build/plaits/plaits/dsp/speech/sam_speech_synth.o build/plaits/plaits/dsp/speech/lpc_speech_synth_phonemes.o build/plaits/plaits/dsp/speech/lpc_speech_synth_words.o build/plaits/plaits/dsp/speech/lpc_speech_synth_controller.o build/plaits/plaits/dsp/engine/particle_engine.o build/plaits/plaits/dsp/engine/modal_engine.o build/plaits/plaits/dsp/engine/noise_engine.o build/plaits/plaits/dsp/engine/waveshaping_engine.o build/plaits/plaits/dsp/engine/string_engine.o build/plaits/plaits/dsp/engine/bass_drum_engine.o build/plaits/plaits/dsp/engine/swarm_engine.o build/plaits/plaits/dsp/engine/wavetable_engine.o build/plaits/plaits/dsp/engine/snare_drum_engine.o build/plaits/plaits/dsp/engine/chord_engine.o build/plaits/plaits/dsp/engine/fm_engine.o build/plaits/plaits/dsp/engine/grain_engine.o build/plaits/plaits/dsp/engine/additive_engine.o build/plaits/plaits/dsp/engine/hi_hat_engine.o build/plaits/plaits/dsp/engine/speech_engine.o build/plaits/plaits/dsp/engine/virtual_analog_engine.o build/plaits/plaits/dsp/voice.o build/plaits/plaits/dsp/fm/dx_units.o build/plaits/plaits/dsp/fm/algorithms.o build/plaits/plaits/dsp/chords/chord_bank.o build/plaits/plaits/dsp/physical_modelling/modal_voice.o build/plaits/plaits/dsp/physical_modelling/string_voice.o build/plaits/plaits/dsp/physical_modelling/resonator.o build/plaits/plaits/dsp/physical_modelling/string.o build/plaits/plaits/resources.o build/plaits/stmlib/dsp/units.o build/plaits/stmlib/utils/random.o $(RING_OBJECTS) $(UI_BASE_ARCHIVE)
	$(CXX) -std=c++17 -DGROOVE_PLAITS -DTEST -Isrc -Ithird_party/mutable -Ithird_party/daisysp/Source -Ithird_party/daisysp/Source/Utility -Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-variable -Wno-unused-local-typedefs -fsanitize=address,undefined tests/source_select_test.cpp build/test-core-engine.o $(HOST_SUPPORT_OBJECTS) build/plaits/plaits/dsp/engine2/chiptune_engine.o build/plaits/plaits/dsp/engine2/virtual_analog_vcf_engine.o build/plaits/plaits/dsp/engine2/wave_terrain_engine.o build/plaits/plaits/dsp/engine2/six_op_engine.o build/plaits/plaits/dsp/engine2/phase_distortion_engine.o build/plaits/plaits/dsp/engine2/string_machine_engine.o build/plaits/plaits/dsp/speech/lpc_speech_synth.o build/plaits/plaits/dsp/speech/naive_speech_synth.o build/plaits/plaits/dsp/speech/sam_speech_synth.o build/plaits/plaits/dsp/speech/lpc_speech_synth_phonemes.o build/plaits/plaits/dsp/speech/lpc_speech_synth_words.o build/plaits/plaits/dsp/speech/lpc_speech_synth_controller.o build/plaits/plaits/dsp/engine/particle_engine.o build/plaits/plaits/dsp/engine/modal_engine.o build/plaits/plaits/dsp/engine/noise_engine.o build/plaits/plaits/dsp/engine/waveshaping_engine.o build/plaits/plaits/dsp/engine/string_engine.o build/plaits/plaits/dsp/engine/bass_drum_engine.o build/plaits/plaits/dsp/engine/swarm_engine.o build/plaits/plaits/dsp/engine/wavetable_engine.o build/plaits/plaits/dsp/engine/snare_drum_engine.o build/plaits/plaits/dsp/engine/chord_engine.o build/plaits/plaits/dsp/engine/fm_engine.o build/plaits/plaits/dsp/engine/grain_engine.o build/plaits/plaits/dsp/engine/additive_engine.o build/plaits/plaits/dsp/engine/hi_hat_engine.o build/plaits/plaits/dsp/engine/speech_engine.o build/plaits/plaits/dsp/engine/virtual_analog_engine.o build/plaits/plaits/dsp/voice.o build/plaits/plaits/dsp/fm/dx_units.o build/plaits/plaits/dsp/fm/algorithms.o build/plaits/plaits/dsp/chords/chord_bank.o build/plaits/plaits/dsp/physical_modelling/modal_voice.o build/plaits/plaits/dsp/physical_modelling/string_voice.o build/plaits/plaits/dsp/physical_modelling/resonator.o build/plaits/plaits/dsp/physical_modelling/string.o build/plaits/plaits/resources.o build/plaits/stmlib/dsp/units.o build/plaits/stmlib/utils/random.o $(RING_OBJECTS) -o build/test-source-select $(UI_BASE_ARCHIVE)
build/test-engine-pages: tests/engine_pages_test.cpp build/test-core-engine.o $(HOST_SUPPORT_OBJECTS) build/plaits/plaits/dsp/engine2/chiptune_engine.o build/plaits/plaits/dsp/engine2/virtual_analog_vcf_engine.o build/plaits/plaits/dsp/engine2/wave_terrain_engine.o build/plaits/plaits/dsp/engine2/six_op_engine.o build/plaits/plaits/dsp/engine2/phase_distortion_engine.o build/plaits/plaits/dsp/engine2/string_machine_engine.o build/plaits/plaits/dsp/speech/lpc_speech_synth.o build/plaits/plaits/dsp/speech/naive_speech_synth.o build/plaits/plaits/dsp/speech/sam_speech_synth.o build/plaits/plaits/dsp/speech/lpc_speech_synth_phonemes.o build/plaits/plaits/dsp/speech/lpc_speech_synth_words.o build/plaits/plaits/dsp/speech/lpc_speech_synth_controller.o build/plaits/plaits/dsp/engine/particle_engine.o build/plaits/plaits/dsp/engine/modal_engine.o build/plaits/plaits/dsp/engine/noise_engine.o build/plaits/plaits/dsp/engine/waveshaping_engine.o build/plaits/plaits/dsp/engine/string_engine.o build/plaits/plaits/dsp/engine/bass_drum_engine.o build/plaits/plaits/dsp/engine/swarm_engine.o build/plaits/plaits/dsp/engine/wavetable_engine.o build/plaits/plaits/dsp/engine/snare_drum_engine.o build/plaits/plaits/dsp/engine/chord_engine.o build/plaits/plaits/dsp/engine/fm_engine.o build/plaits/plaits/dsp/engine/grain_engine.o build/plaits/plaits/dsp/engine/additive_engine.o build/plaits/plaits/dsp/engine/hi_hat_engine.o build/plaits/plaits/dsp/engine/speech_engine.o build/plaits/plaits/dsp/engine/virtual_analog_engine.o build/plaits/plaits/dsp/voice.o build/plaits/plaits/dsp/fm/dx_units.o build/plaits/plaits/dsp/fm/algorithms.o build/plaits/plaits/dsp/chords/chord_bank.o build/plaits/plaits/dsp/physical_modelling/modal_voice.o build/plaits/plaits/dsp/physical_modelling/string_voice.o build/plaits/plaits/dsp/physical_modelling/resonator.o build/plaits/plaits/dsp/physical_modelling/string.o build/plaits/plaits/resources.o build/plaits/stmlib/dsp/units.o build/plaits/stmlib/utils/random.o $(RING_OBJECTS) $(UI_BASE_ARCHIVE)
	$(CXX) -std=c++17 -DGROOVE_PLAITS -DTEST -Isrc -Ithird_party/mutable -Ithird_party/daisysp/Source -Ithird_party/daisysp/Source/Utility -Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-variable -Wno-unused-local-typedefs -fsanitize=address,undefined tests/engine_pages_test.cpp build/test-core-engine.o $(HOST_SUPPORT_OBJECTS) build/plaits/plaits/dsp/engine2/chiptune_engine.o build/plaits/plaits/dsp/engine2/virtual_analog_vcf_engine.o build/plaits/plaits/dsp/engine2/wave_terrain_engine.o build/plaits/plaits/dsp/engine2/six_op_engine.o build/plaits/plaits/dsp/engine2/phase_distortion_engine.o build/plaits/plaits/dsp/engine2/string_machine_engine.o build/plaits/plaits/dsp/speech/lpc_speech_synth.o build/plaits/plaits/dsp/speech/naive_speech_synth.o build/plaits/plaits/dsp/speech/sam_speech_synth.o build/plaits/plaits/dsp/speech/lpc_speech_synth_phonemes.o build/plaits/plaits/dsp/speech/lpc_speech_synth_words.o build/plaits/plaits/dsp/speech/lpc_speech_synth_controller.o build/plaits/plaits/dsp/engine/particle_engine.o build/plaits/plaits/dsp/engine/modal_engine.o build/plaits/plaits/dsp/engine/noise_engine.o build/plaits/plaits/dsp/engine/waveshaping_engine.o build/plaits/plaits/dsp/engine/string_engine.o build/plaits/plaits/dsp/engine/bass_drum_engine.o build/plaits/plaits/dsp/engine/swarm_engine.o build/plaits/plaits/dsp/engine/wavetable_engine.o build/plaits/plaits/dsp/engine/snare_drum_engine.o build/plaits/plaits/dsp/engine/chord_engine.o build/plaits/plaits/dsp/engine/fm_engine.o build/plaits/plaits/dsp/engine/grain_engine.o build/plaits/plaits/dsp/engine/additive_engine.o build/plaits/plaits/dsp/engine/hi_hat_engine.o build/plaits/plaits/dsp/engine/speech_engine.o build/plaits/plaits/dsp/engine/virtual_analog_engine.o build/plaits/plaits/dsp/voice.o build/plaits/plaits/dsp/fm/dx_units.o build/plaits/plaits/dsp/fm/algorithms.o build/plaits/plaits/dsp/chords/chord_bank.o build/plaits/plaits/dsp/physical_modelling/modal_voice.o build/plaits/plaits/dsp/physical_modelling/string_voice.o build/plaits/plaits/dsp/physical_modelling/resonator.o build/plaits/plaits/dsp/physical_modelling/string.o build/plaits/plaits/resources.o build/plaits/stmlib/dsp/units.o build/plaits/stmlib/utils/random.o $(RING_OBJECTS) -o build/test-engine-pages $(UI_BASE_ARCHIVE)
build/test-fx-chain: tests/fx_chain_test.cpp build/test-core-engine.o $(HOST_SUPPORT_OBJECTS) build/plaits/plaits/dsp/engine2/chiptune_engine.o build/plaits/plaits/dsp/engine2/virtual_analog_vcf_engine.o build/plaits/plaits/dsp/engine2/wave_terrain_engine.o build/plaits/plaits/dsp/engine2/six_op_engine.o build/plaits/plaits/dsp/engine2/phase_distortion_engine.o build/plaits/plaits/dsp/engine2/string_machine_engine.o build/plaits/plaits/dsp/speech/lpc_speech_synth.o build/plaits/plaits/dsp/speech/naive_speech_synth.o build/plaits/plaits/dsp/speech/sam_speech_synth.o build/plaits/plaits/dsp/speech/lpc_speech_synth_phonemes.o build/plaits/plaits/dsp/speech/lpc_speech_synth_words.o build/plaits/plaits/dsp/speech/lpc_speech_synth_controller.o build/plaits/plaits/dsp/engine/particle_engine.o build/plaits/plaits/dsp/engine/modal_engine.o build/plaits/plaits/dsp/engine/noise_engine.o build/plaits/plaits/dsp/engine/waveshaping_engine.o build/plaits/plaits/dsp/engine/string_engine.o build/plaits/plaits/dsp/engine/bass_drum_engine.o build/plaits/plaits/dsp/engine/swarm_engine.o build/plaits/plaits/dsp/engine/wavetable_engine.o build/plaits/plaits/dsp/engine/snare_drum_engine.o build/plaits/plaits/dsp/engine/chord_engine.o build/plaits/plaits/dsp/engine/fm_engine.o build/plaits/plaits/dsp/engine/grain_engine.o build/plaits/plaits/dsp/engine/additive_engine.o build/plaits/plaits/dsp/engine/hi_hat_engine.o build/plaits/plaits/dsp/engine/speech_engine.o build/plaits/plaits/dsp/engine/virtual_analog_engine.o build/plaits/plaits/dsp/voice.o build/plaits/plaits/dsp/fm/dx_units.o build/plaits/plaits/dsp/fm/algorithms.o build/plaits/plaits/dsp/chords/chord_bank.o build/plaits/plaits/dsp/physical_modelling/modal_voice.o build/plaits/plaits/dsp/physical_modelling/string_voice.o build/plaits/plaits/dsp/physical_modelling/resonator.o build/plaits/plaits/dsp/physical_modelling/string.o build/plaits/plaits/resources.o build/plaits/stmlib/dsp/units.o build/plaits/stmlib/utils/random.o $(RING_OBJECTS) $(UI_BASE_ARCHIVE)
	$(CXX) -std=c++17 -DGROOVE_PLAITS -DTEST -Isrc -Ithird_party/mutable -Ithird_party/daisysp/Source -Ithird_party/daisysp/Source/Utility -Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-variable -Wno-unused-local-typedefs -fsanitize=address,undefined tests/fx_chain_test.cpp build/test-core-engine.o $(HOST_SUPPORT_OBJECTS) build/plaits/plaits/dsp/engine2/chiptune_engine.o build/plaits/plaits/dsp/engine2/virtual_analog_vcf_engine.o build/plaits/plaits/dsp/engine2/wave_terrain_engine.o build/plaits/plaits/dsp/engine2/six_op_engine.o build/plaits/plaits/dsp/engine2/phase_distortion_engine.o build/plaits/plaits/dsp/engine2/string_machine_engine.o build/plaits/plaits/dsp/speech/lpc_speech_synth.o build/plaits/plaits/dsp/speech/naive_speech_synth.o build/plaits/plaits/dsp/speech/sam_speech_synth.o build/plaits/plaits/dsp/speech/lpc_speech_synth_phonemes.o build/plaits/plaits/dsp/speech/lpc_speech_synth_words.o build/plaits/plaits/dsp/speech/lpc_speech_synth_controller.o build/plaits/plaits/dsp/engine/particle_engine.o build/plaits/plaits/dsp/engine/modal_engine.o build/plaits/plaits/dsp/engine/noise_engine.o build/plaits/plaits/dsp/engine/waveshaping_engine.o build/plaits/plaits/dsp/engine/string_engine.o build/plaits/plaits/dsp/engine/bass_drum_engine.o build/plaits/plaits/dsp/engine/swarm_engine.o build/plaits/plaits/dsp/engine/wavetable_engine.o build/plaits/plaits/dsp/engine/snare_drum_engine.o build/plaits/plaits/dsp/engine/chord_engine.o build/plaits/plaits/dsp/engine/fm_engine.o build/plaits/plaits/dsp/engine/grain_engine.o build/plaits/plaits/dsp/engine/additive_engine.o build/plaits/plaits/dsp/engine/hi_hat_engine.o build/plaits/plaits/dsp/engine/speech_engine.o build/plaits/plaits/dsp/engine/virtual_analog_engine.o build/plaits/plaits/dsp/voice.o build/plaits/plaits/dsp/fm/dx_units.o build/plaits/plaits/dsp/fm/algorithms.o build/plaits/plaits/dsp/chords/chord_bank.o build/plaits/plaits/dsp/physical_modelling/modal_voice.o build/plaits/plaits/dsp/physical_modelling/string_voice.o build/plaits/plaits/dsp/physical_modelling/resonator.o build/plaits/plaits/dsp/physical_modelling/string.o build/plaits/plaits/resources.o build/plaits/stmlib/dsp/units.o build/plaits/stmlib/utils/random.o $(RING_OBJECTS) -o build/test-fx-chain $(UI_BASE_ARCHIVE)
build/test-browser-open-stress: tests/browser_open_stress_test.cpp build/test-core-engine.o $(HOST_SUPPORT_OBJECTS) build/plaits/plaits/dsp/engine2/chiptune_engine.o build/plaits/plaits/dsp/engine2/virtual_analog_vcf_engine.o build/plaits/plaits/dsp/engine2/wave_terrain_engine.o build/plaits/plaits/dsp/engine2/six_op_engine.o build/plaits/plaits/dsp/engine2/phase_distortion_engine.o build/plaits/plaits/dsp/engine2/string_machine_engine.o build/plaits/plaits/dsp/speech/lpc_speech_synth.o build/plaits/plaits/dsp/speech/naive_speech_synth.o build/plaits/plaits/dsp/speech/sam_speech_synth.o build/plaits/plaits/dsp/speech/lpc_speech_synth_phonemes.o build/plaits/plaits/dsp/speech/lpc_speech_synth_words.o build/plaits/plaits/dsp/speech/lpc_speech_synth_controller.o build/plaits/plaits/dsp/engine/particle_engine.o build/plaits/plaits/dsp/engine/modal_engine.o build/plaits/plaits/dsp/engine/noise_engine.o build/plaits/plaits/dsp/engine/waveshaping_engine.o build/plaits/plaits/dsp/engine/string_engine.o build/plaits/plaits/dsp/engine/bass_drum_engine.o build/plaits/plaits/dsp/engine/swarm_engine.o build/plaits/plaits/dsp/engine/wavetable_engine.o build/plaits/plaits/dsp/engine/snare_drum_engine.o build/plaits/plaits/dsp/engine/chord_engine.o build/plaits/plaits/dsp/engine/fm_engine.o build/plaits/plaits/dsp/engine/grain_engine.o build/plaits/plaits/dsp/engine/additive_engine.o build/plaits/plaits/dsp/engine/hi_hat_engine.o build/plaits/plaits/dsp/engine/speech_engine.o build/plaits/plaits/dsp/engine/virtual_analog_engine.o build/plaits/plaits/dsp/voice.o build/plaits/plaits/dsp/fm/dx_units.o build/plaits/plaits/dsp/fm/algorithms.o build/plaits/plaits/dsp/chords/chord_bank.o build/plaits/plaits/dsp/physical_modelling/modal_voice.o build/plaits/plaits/dsp/physical_modelling/string_voice.o build/plaits/plaits/dsp/physical_modelling/resonator.o build/plaits/plaits/dsp/physical_modelling/string.o build/plaits/plaits/resources.o build/plaits/stmlib/dsp/units.o build/plaits/stmlib/utils/random.o $(RING_OBJECTS) $(UI_BASE_ARCHIVE)
	$(CXX) -std=c++17 -DGROOVE_PLAITS -DTEST -Isrc -Ithird_party/mutable -Ithird_party/daisysp/Source -Ithird_party/daisysp/Source/Utility -Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-variable -Wno-unused-local-typedefs -fsanitize=address,undefined tests/browser_open_stress_test.cpp build/test-core-engine.o $(HOST_SUPPORT_OBJECTS) build/plaits/plaits/dsp/engine2/chiptune_engine.o build/plaits/plaits/dsp/engine2/virtual_analog_vcf_engine.o build/plaits/plaits/dsp/engine2/wave_terrain_engine.o build/plaits/plaits/dsp/engine2/six_op_engine.o build/plaits/plaits/dsp/engine2/phase_distortion_engine.o build/plaits/plaits/dsp/engine2/string_machine_engine.o build/plaits/plaits/dsp/speech/lpc_speech_synth.o build/plaits/plaits/dsp/speech/naive_speech_synth.o build/plaits/plaits/dsp/speech/sam_speech_synth.o build/plaits/plaits/dsp/speech/lpc_speech_synth_phonemes.o build/plaits/plaits/dsp/speech/lpc_speech_synth_words.o build/plaits/plaits/dsp/speech/lpc_speech_synth_controller.o build/plaits/plaits/dsp/engine/particle_engine.o build/plaits/plaits/dsp/engine/modal_engine.o build/plaits/plaits/dsp/engine/noise_engine.o build/plaits/plaits/dsp/engine/waveshaping_engine.o build/plaits/plaits/dsp/engine/string_engine.o build/plaits/plaits/dsp/engine/bass_drum_engine.o build/plaits/plaits/dsp/engine/swarm_engine.o build/plaits/plaits/dsp/engine/wavetable_engine.o build/plaits/plaits/dsp/engine/snare_drum_engine.o build/plaits/plaits/dsp/engine/chord_engine.o build/plaits/plaits/dsp/engine/fm_engine.o build/plaits/plaits/dsp/engine/grain_engine.o build/plaits/plaits/dsp/engine/additive_engine.o build/plaits/plaits/dsp/engine/hi_hat_engine.o build/plaits/plaits/dsp/engine/speech_engine.o build/plaits/plaits/dsp/engine/virtual_analog_engine.o build/plaits/plaits/dsp/voice.o build/plaits/plaits/dsp/fm/dx_units.o build/plaits/plaits/dsp/fm/algorithms.o build/plaits/plaits/dsp/chords/chord_bank.o build/plaits/plaits/dsp/physical_modelling/modal_voice.o build/plaits/plaits/dsp/physical_modelling/string_voice.o build/plaits/plaits/dsp/physical_modelling/resonator.o build/plaits/plaits/dsp/physical_modelling/string.o build/plaits/plaits/resources.o build/plaits/stmlib/dsp/units.o build/plaits/stmlib/utils/random.o $(RING_OBJECTS) -o build/test-browser-open-stress $(UI_BASE_ARCHIVE)
build/test-buttons: tests/buttons_test.cpp build/test-core-engine.o $(HOST_SUPPORT_OBJECTS) build/plaits/plaits/dsp/engine2/chiptune_engine.o build/plaits/plaits/dsp/engine2/virtual_analog_vcf_engine.o build/plaits/plaits/dsp/engine2/wave_terrain_engine.o build/plaits/plaits/dsp/engine2/six_op_engine.o build/plaits/plaits/dsp/engine2/phase_distortion_engine.o build/plaits/plaits/dsp/engine2/string_machine_engine.o build/plaits/plaits/dsp/speech/lpc_speech_synth.o build/plaits/plaits/dsp/speech/naive_speech_synth.o build/plaits/plaits/dsp/speech/sam_speech_synth.o build/plaits/plaits/dsp/speech/lpc_speech_synth_phonemes.o build/plaits/plaits/dsp/speech/lpc_speech_synth_words.o build/plaits/plaits/dsp/speech/lpc_speech_synth_controller.o build/plaits/plaits/dsp/engine/particle_engine.o build/plaits/plaits/dsp/engine/modal_engine.o build/plaits/plaits/dsp/engine/noise_engine.o build/plaits/plaits/dsp/engine/waveshaping_engine.o build/plaits/plaits/dsp/engine/string_engine.o build/plaits/plaits/dsp/engine/bass_drum_engine.o build/plaits/plaits/dsp/engine/swarm_engine.o build/plaits/plaits/dsp/engine/wavetable_engine.o build/plaits/plaits/dsp/engine/snare_drum_engine.o build/plaits/plaits/dsp/engine/chord_engine.o build/plaits/plaits/dsp/engine/fm_engine.o build/plaits/plaits/dsp/engine/grain_engine.o build/plaits/plaits/dsp/engine/additive_engine.o build/plaits/plaits/dsp/engine/hi_hat_engine.o build/plaits/plaits/dsp/engine/speech_engine.o build/plaits/plaits/dsp/engine/virtual_analog_engine.o build/plaits/plaits/dsp/voice.o build/plaits/plaits/dsp/fm/dx_units.o build/plaits/plaits/dsp/fm/algorithms.o build/plaits/plaits/dsp/chords/chord_bank.o build/plaits/plaits/dsp/physical_modelling/modal_voice.o build/plaits/plaits/dsp/physical_modelling/string_voice.o build/plaits/plaits/dsp/physical_modelling/resonator.o build/plaits/plaits/dsp/physical_modelling/string.o build/plaits/plaits/resources.o build/plaits/stmlib/dsp/units.o build/plaits/stmlib/utils/random.o $(RING_OBJECTS) $(UI_BASE_ARCHIVE)
	$(CXX) -std=c++17 -DGROOVE_PLAITS -DTEST -Isrc -Ithird_party/mutable -Ithird_party/daisysp/Source -Ithird_party/daisysp/Source/Utility -Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-variable -Wno-unused-local-typedefs -fsanitize=address,undefined tests/buttons_test.cpp build/test-core-engine.o $(HOST_SUPPORT_OBJECTS) build/plaits/plaits/dsp/engine2/chiptune_engine.o build/plaits/plaits/dsp/engine2/virtual_analog_vcf_engine.o build/plaits/plaits/dsp/engine2/wave_terrain_engine.o build/plaits/plaits/dsp/engine2/six_op_engine.o build/plaits/plaits/dsp/engine2/phase_distortion_engine.o build/plaits/plaits/dsp/engine2/string_machine_engine.o build/plaits/plaits/dsp/speech/lpc_speech_synth.o build/plaits/plaits/dsp/speech/naive_speech_synth.o build/plaits/plaits/dsp/speech/sam_speech_synth.o build/plaits/plaits/dsp/speech/lpc_speech_synth_phonemes.o build/plaits/plaits/dsp/speech/lpc_speech_synth_words.o build/plaits/plaits/dsp/speech/lpc_speech_synth_controller.o build/plaits/plaits/dsp/engine/particle_engine.o build/plaits/plaits/dsp/engine/modal_engine.o build/plaits/plaits/dsp/engine/noise_engine.o build/plaits/plaits/dsp/engine/waveshaping_engine.o build/plaits/plaits/dsp/engine/string_engine.o build/plaits/plaits/dsp/engine/bass_drum_engine.o build/plaits/plaits/dsp/engine/swarm_engine.o build/plaits/plaits/dsp/engine/wavetable_engine.o build/plaits/plaits/dsp/engine/snare_drum_engine.o build/plaits/plaits/dsp/engine/chord_engine.o build/plaits/plaits/dsp/engine/fm_engine.o build/plaits/plaits/dsp/engine/grain_engine.o build/plaits/plaits/dsp/engine/additive_engine.o build/plaits/plaits/dsp/engine/hi_hat_engine.o build/plaits/plaits/dsp/engine/speech_engine.o build/plaits/plaits/dsp/engine/virtual_analog_engine.o build/plaits/plaits/dsp/voice.o build/plaits/plaits/dsp/fm/dx_units.o build/plaits/plaits/dsp/fm/algorithms.o build/plaits/plaits/dsp/chords/chord_bank.o build/plaits/plaits/dsp/physical_modelling/modal_voice.o build/plaits/plaits/dsp/physical_modelling/string_voice.o build/plaits/plaits/dsp/physical_modelling/resonator.o build/plaits/plaits/dsp/physical_modelling/string.o build/plaits/plaits/resources.o build/plaits/stmlib/dsp/units.o build/plaits/stmlib/utils/random.o $(RING_OBJECTS) -o build/test-buttons $(UI_BASE_ARCHIVE)
build/test-duplicate: tests/duplicate_test.cpp build/test-core-engine.o $(HOST_SUPPORT_OBJECTS) build/plaits/plaits/dsp/engine2/chiptune_engine.o build/plaits/plaits/dsp/engine2/virtual_analog_vcf_engine.o build/plaits/plaits/dsp/engine2/wave_terrain_engine.o build/plaits/plaits/dsp/engine2/six_op_engine.o build/plaits/plaits/dsp/engine2/phase_distortion_engine.o build/plaits/plaits/dsp/engine2/string_machine_engine.o build/plaits/plaits/dsp/speech/lpc_speech_synth.o build/plaits/plaits/dsp/speech/naive_speech_synth.o build/plaits/plaits/dsp/speech/sam_speech_synth.o build/plaits/plaits/dsp/speech/lpc_speech_synth_phonemes.o build/plaits/plaits/dsp/speech/lpc_speech_synth_words.o build/plaits/plaits/dsp/speech/lpc_speech_synth_controller.o build/plaits/plaits/dsp/engine/particle_engine.o build/plaits/plaits/dsp/engine/modal_engine.o build/plaits/plaits/dsp/engine/noise_engine.o build/plaits/plaits/dsp/engine/waveshaping_engine.o build/plaits/plaits/dsp/engine/string_engine.o build/plaits/plaits/dsp/engine/bass_drum_engine.o build/plaits/plaits/dsp/engine/swarm_engine.o build/plaits/plaits/dsp/engine/wavetable_engine.o build/plaits/plaits/dsp/engine/snare_drum_engine.o build/plaits/plaits/dsp/engine/chord_engine.o build/plaits/plaits/dsp/engine/fm_engine.o build/plaits/plaits/dsp/engine/grain_engine.o build/plaits/plaits/dsp/engine/additive_engine.o build/plaits/plaits/dsp/engine/hi_hat_engine.o build/plaits/plaits/dsp/engine/speech_engine.o build/plaits/plaits/dsp/engine/virtual_analog_engine.o build/plaits/plaits/dsp/voice.o build/plaits/plaits/dsp/fm/dx_units.o build/plaits/plaits/dsp/fm/algorithms.o build/plaits/plaits/dsp/chords/chord_bank.o build/plaits/plaits/dsp/physical_modelling/modal_voice.o build/plaits/plaits/dsp/physical_modelling/string_voice.o build/plaits/plaits/dsp/physical_modelling/resonator.o build/plaits/plaits/dsp/physical_modelling/string.o build/plaits/plaits/resources.o build/plaits/stmlib/dsp/units.o build/plaits/stmlib/utils/random.o $(RING_OBJECTS) $(UI_BASE_ARCHIVE)
	$(CXX) -std=c++17 -DGROOVE_PLAITS -DTEST -Isrc -Ithird_party/mutable -Ithird_party/daisysp/Source -Ithird_party/daisysp/Source/Utility -Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-variable -Wno-unused-local-typedefs -fsanitize=address,undefined tests/duplicate_test.cpp build/test-core-engine.o $(HOST_SUPPORT_OBJECTS) build/plaits/plaits/dsp/engine2/chiptune_engine.o build/plaits/plaits/dsp/engine2/virtual_analog_vcf_engine.o build/plaits/plaits/dsp/engine2/wave_terrain_engine.o build/plaits/plaits/dsp/engine2/six_op_engine.o build/plaits/plaits/dsp/engine2/phase_distortion_engine.o build/plaits/plaits/dsp/engine2/string_machine_engine.o build/plaits/plaits/dsp/speech/lpc_speech_synth.o build/plaits/plaits/dsp/speech/naive_speech_synth.o build/plaits/plaits/dsp/speech/sam_speech_synth.o build/plaits/plaits/dsp/speech/lpc_speech_synth_phonemes.o build/plaits/plaits/dsp/speech/lpc_speech_synth_words.o build/plaits/plaits/dsp/speech/lpc_speech_synth_controller.o build/plaits/plaits/dsp/engine/particle_engine.o build/plaits/plaits/dsp/engine/modal_engine.o build/plaits/plaits/dsp/engine/noise_engine.o build/plaits/plaits/dsp/engine/waveshaping_engine.o build/plaits/plaits/dsp/engine/string_engine.o build/plaits/plaits/dsp/engine/bass_drum_engine.o build/plaits/plaits/dsp/engine/swarm_engine.o build/plaits/plaits/dsp/engine/wavetable_engine.o build/plaits/plaits/dsp/engine/snare_drum_engine.o build/plaits/plaits/dsp/engine/chord_engine.o build/plaits/plaits/dsp/engine/fm_engine.o build/plaits/plaits/dsp/engine/grain_engine.o build/plaits/plaits/dsp/engine/additive_engine.o build/plaits/plaits/dsp/engine/hi_hat_engine.o build/plaits/plaits/dsp/engine/speech_engine.o build/plaits/plaits/dsp/engine/virtual_analog_engine.o build/plaits/plaits/dsp/voice.o build/plaits/plaits/dsp/fm/dx_units.o build/plaits/plaits/dsp/fm/algorithms.o build/plaits/plaits/dsp/chords/chord_bank.o build/plaits/plaits/dsp/physical_modelling/modal_voice.o build/plaits/plaits/dsp/physical_modelling/string_voice.o build/plaits/plaits/dsp/physical_modelling/resonator.o build/plaits/plaits/dsp/physical_modelling/string.o build/plaits/plaits/resources.o build/plaits/stmlib/dsp/units.o build/plaits/stmlib/utils/random.o $(RING_OBJECTS) -o build/test-duplicate $(UI_BASE_ARCHIVE)
build/test-daisy-adsr: tests/daisy_adsr_test.cpp build/test-core-engine.o $(HOST_SUPPORT_OBJECTS) build/plaits/plaits/dsp/engine2/chiptune_engine.o build/plaits/plaits/dsp/engine2/virtual_analog_vcf_engine.o build/plaits/plaits/dsp/engine2/wave_terrain_engine.o build/plaits/plaits/dsp/engine2/six_op_engine.o build/plaits/plaits/dsp/engine2/phase_distortion_engine.o build/plaits/plaits/dsp/engine2/string_machine_engine.o build/plaits/plaits/dsp/speech/lpc_speech_synth.o build/plaits/plaits/dsp/speech/naive_speech_synth.o build/plaits/plaits/dsp/speech/sam_speech_synth.o build/plaits/plaits/dsp/speech/lpc_speech_synth_phonemes.o build/plaits/plaits/dsp/speech/lpc_speech_synth_words.o build/plaits/plaits/dsp/speech/lpc_speech_synth_controller.o build/plaits/plaits/dsp/engine/particle_engine.o build/plaits/plaits/dsp/engine/modal_engine.o build/plaits/plaits/dsp/engine/noise_engine.o build/plaits/plaits/dsp/engine/waveshaping_engine.o build/plaits/plaits/dsp/engine/string_engine.o build/plaits/plaits/dsp/engine/bass_drum_engine.o build/plaits/plaits/dsp/engine/swarm_engine.o build/plaits/plaits/dsp/engine/wavetable_engine.o build/plaits/plaits/dsp/engine/snare_drum_engine.o build/plaits/plaits/dsp/engine/chord_engine.o build/plaits/plaits/dsp/engine/fm_engine.o build/plaits/plaits/dsp/engine/grain_engine.o build/plaits/plaits/dsp/engine/additive_engine.o build/plaits/plaits/dsp/engine/hi_hat_engine.o build/plaits/plaits/dsp/engine/speech_engine.o build/plaits/plaits/dsp/engine/virtual_analog_engine.o build/plaits/plaits/dsp/voice.o build/plaits/plaits/dsp/fm/dx_units.o build/plaits/plaits/dsp/fm/algorithms.o build/plaits/plaits/dsp/chords/chord_bank.o build/plaits/plaits/dsp/physical_modelling/modal_voice.o build/plaits/plaits/dsp/physical_modelling/string_voice.o build/plaits/plaits/dsp/physical_modelling/resonator.o build/plaits/plaits/dsp/physical_modelling/string.o build/plaits/plaits/resources.o build/plaits/stmlib/dsp/units.o build/plaits/stmlib/utils/random.o $(RING_OBJECTS) $(UI_BASE_ARCHIVE)
	$(CXX) -std=c++17 -DGROOVE_PLAITS -DTEST -Isrc -Ithird_party/mutable -Ithird_party/daisysp/Source -Ithird_party/daisysp/Source/Utility -Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-variable -Wno-unused-local-typedefs -fsanitize=address,undefined tests/daisy_adsr_test.cpp build/test-core-engine.o $(HOST_SUPPORT_OBJECTS) build/plaits/plaits/dsp/engine2/chiptune_engine.o build/plaits/plaits/dsp/engine2/virtual_analog_vcf_engine.o build/plaits/plaits/dsp/engine2/wave_terrain_engine.o build/plaits/plaits/dsp/engine2/six_op_engine.o build/plaits/plaits/dsp/engine2/phase_distortion_engine.o build/plaits/plaits/dsp/engine2/string_machine_engine.o build/plaits/plaits/dsp/speech/lpc_speech_synth.o build/plaits/plaits/dsp/speech/naive_speech_synth.o build/plaits/plaits/dsp/speech/sam_speech_synth.o build/plaits/plaits/dsp/speech/lpc_speech_synth_phonemes.o build/plaits/plaits/dsp/speech/lpc_speech_synth_words.o build/plaits/plaits/dsp/speech/lpc_speech_synth_controller.o build/plaits/plaits/dsp/engine/particle_engine.o build/plaits/plaits/dsp/engine/modal_engine.o build/plaits/plaits/dsp/engine/noise_engine.o build/plaits/plaits/dsp/engine/waveshaping_engine.o build/plaits/plaits/dsp/engine/string_engine.o build/plaits/plaits/dsp/engine/bass_drum_engine.o build/plaits/plaits/dsp/engine/swarm_engine.o build/plaits/plaits/dsp/engine/wavetable_engine.o build/plaits/plaits/dsp/engine/snare_drum_engine.o build/plaits/plaits/dsp/engine/chord_engine.o build/plaits/plaits/dsp/engine/fm_engine.o build/plaits/plaits/dsp/engine/grain_engine.o build/plaits/plaits/dsp/engine/additive_engine.o build/plaits/plaits/dsp/engine/hi_hat_engine.o build/plaits/plaits/dsp/engine/speech_engine.o build/plaits/plaits/dsp/engine/virtual_analog_engine.o build/plaits/plaits/dsp/voice.o build/plaits/plaits/dsp/fm/dx_units.o build/plaits/plaits/dsp/fm/algorithms.o build/plaits/plaits/dsp/chords/chord_bank.o build/plaits/plaits/dsp/physical_modelling/modal_voice.o build/plaits/plaits/dsp/physical_modelling/string_voice.o build/plaits/plaits/dsp/physical_modelling/resonator.o build/plaits/plaits/dsp/physical_modelling/string.o build/plaits/plaits/resources.o build/plaits/stmlib/dsp/units.o build/plaits/stmlib/utils/random.o $(RING_OBJECTS) -o build/test-daisy-adsr $(UI_BASE_ARCHIVE)
build/test-transition: tests/transition_test.cpp build/test-core-engine.o $(HOST_SUPPORT_OBJECTS) build/plaits/plaits/dsp/engine2/chiptune_engine.o build/plaits/plaits/dsp/engine2/virtual_analog_vcf_engine.o build/plaits/plaits/dsp/engine2/wave_terrain_engine.o build/plaits/plaits/dsp/engine2/six_op_engine.o build/plaits/plaits/dsp/engine2/phase_distortion_engine.o build/plaits/plaits/dsp/engine2/string_machine_engine.o build/plaits/plaits/dsp/speech/lpc_speech_synth.o build/plaits/plaits/dsp/speech/naive_speech_synth.o build/plaits/plaits/dsp/speech/sam_speech_synth.o build/plaits/plaits/dsp/speech/lpc_speech_synth_phonemes.o build/plaits/plaits/dsp/speech/lpc_speech_synth_words.o build/plaits/plaits/dsp/speech/lpc_speech_synth_controller.o build/plaits/plaits/dsp/engine/particle_engine.o build/plaits/plaits/dsp/engine/modal_engine.o build/plaits/plaits/dsp/engine/noise_engine.o build/plaits/plaits/dsp/engine/waveshaping_engine.o build/plaits/plaits/dsp/engine/string_engine.o build/plaits/plaits/dsp/engine/bass_drum_engine.o build/plaits/plaits/dsp/engine/swarm_engine.o build/plaits/plaits/dsp/engine/wavetable_engine.o build/plaits/plaits/dsp/engine/snare_drum_engine.o build/plaits/plaits/dsp/engine/chord_engine.o build/plaits/plaits/dsp/engine/fm_engine.o build/plaits/plaits/dsp/engine/grain_engine.o build/plaits/plaits/dsp/engine/additive_engine.o build/plaits/plaits/dsp/engine/hi_hat_engine.o build/plaits/plaits/dsp/engine/speech_engine.o build/plaits/plaits/dsp/engine/virtual_analog_engine.o build/plaits/plaits/dsp/voice.o build/plaits/plaits/dsp/fm/dx_units.o build/plaits/plaits/dsp/fm/algorithms.o build/plaits/plaits/dsp/chords/chord_bank.o build/plaits/plaits/dsp/physical_modelling/modal_voice.o build/plaits/plaits/dsp/physical_modelling/string_voice.o build/plaits/plaits/dsp/physical_modelling/resonator.o build/plaits/plaits/dsp/physical_modelling/string.o build/plaits/plaits/resources.o build/plaits/stmlib/dsp/units.o build/plaits/stmlib/utils/random.o $(RING_OBJECTS) $(UI_BASE_ARCHIVE)
	$(CXX) -std=c++17 -DGROOVE_PLAITS -DTEST -Isrc -Ithird_party/mutable -Ithird_party/daisysp/Source -Ithird_party/daisysp/Source/Utility -Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-variable -Wno-unused-local-typedefs -fsanitize=address,undefined tests/transition_test.cpp build/test-core-engine.o $(HOST_SUPPORT_OBJECTS) build/plaits/plaits/dsp/engine2/chiptune_engine.o build/plaits/plaits/dsp/engine2/virtual_analog_vcf_engine.o build/plaits/plaits/dsp/engine2/wave_terrain_engine.o build/plaits/plaits/dsp/engine2/six_op_engine.o build/plaits/plaits/dsp/engine2/phase_distortion_engine.o build/plaits/plaits/dsp/engine2/string_machine_engine.o build/plaits/plaits/dsp/speech/lpc_speech_synth.o build/plaits/plaits/dsp/speech/naive_speech_synth.o build/plaits/plaits/dsp/speech/sam_speech_synth.o build/plaits/plaits/dsp/speech/lpc_speech_synth_phonemes.o build/plaits/plaits/dsp/speech/lpc_speech_synth_words.o build/plaits/plaits/dsp/speech/lpc_speech_synth_controller.o build/plaits/plaits/dsp/engine/particle_engine.o build/plaits/plaits/dsp/engine/modal_engine.o build/plaits/plaits/dsp/engine/noise_engine.o build/plaits/plaits/dsp/engine/waveshaping_engine.o build/plaits/plaits/dsp/engine/string_engine.o build/plaits/plaits/dsp/engine/bass_drum_engine.o build/plaits/plaits/dsp/engine/swarm_engine.o build/plaits/plaits/dsp/engine/wavetable_engine.o build/plaits/plaits/dsp/engine/snare_drum_engine.o build/plaits/plaits/dsp/engine/chord_engine.o build/plaits/plaits/dsp/engine/fm_engine.o build/plaits/plaits/dsp/engine/grain_engine.o build/plaits/plaits/dsp/engine/additive_engine.o build/plaits/plaits/dsp/engine/hi_hat_engine.o build/plaits/plaits/dsp/engine/speech_engine.o build/plaits/plaits/dsp/engine/virtual_analog_engine.o build/plaits/plaits/dsp/voice.o build/plaits/plaits/dsp/fm/dx_units.o build/plaits/plaits/dsp/fm/algorithms.o build/plaits/plaits/dsp/chords/chord_bank.o build/plaits/plaits/dsp/physical_modelling/modal_voice.o build/plaits/plaits/dsp/physical_modelling/string_voice.o build/plaits/plaits/dsp/physical_modelling/resonator.o build/plaits/plaits/dsp/physical_modelling/string.o build/plaits/plaits/resources.o build/plaits/stmlib/dsp/units.o build/plaits/stmlib/utils/random.o $(RING_OBJECTS) -o build/test-transition $(UI_BASE_ARCHIVE)
build/test-lab: tests/lab_test.cpp build/test-core-engine.o $(HOST_SUPPORT_OBJECTS) build/plaits/plaits/dsp/engine2/chiptune_engine.o build/plaits/plaits/dsp/engine2/virtual_analog_vcf_engine.o build/plaits/plaits/dsp/engine2/wave_terrain_engine.o build/plaits/plaits/dsp/engine2/six_op_engine.o build/plaits/plaits/dsp/engine2/phase_distortion_engine.o build/plaits/plaits/dsp/engine2/string_machine_engine.o build/plaits/plaits/dsp/speech/lpc_speech_synth.o build/plaits/plaits/dsp/speech/naive_speech_synth.o build/plaits/plaits/dsp/speech/sam_speech_synth.o build/plaits/plaits/dsp/speech/lpc_speech_synth_phonemes.o build/plaits/plaits/dsp/speech/lpc_speech_synth_words.o build/plaits/plaits/dsp/speech/lpc_speech_synth_controller.o build/plaits/plaits/dsp/engine/particle_engine.o build/plaits/plaits/dsp/engine/modal_engine.o build/plaits/plaits/dsp/engine/noise_engine.o build/plaits/plaits/dsp/engine/waveshaping_engine.o build/plaits/plaits/dsp/engine/string_engine.o build/plaits/plaits/dsp/engine/bass_drum_engine.o build/plaits/plaits/dsp/engine/swarm_engine.o build/plaits/plaits/dsp/engine/wavetable_engine.o build/plaits/plaits/dsp/engine/snare_drum_engine.o build/plaits/plaits/dsp/engine/chord_engine.o build/plaits/plaits/dsp/engine/fm_engine.o build/plaits/plaits/dsp/engine/grain_engine.o build/plaits/plaits/dsp/engine/additive_engine.o build/plaits/plaits/dsp/engine/hi_hat_engine.o build/plaits/plaits/dsp/engine/speech_engine.o build/plaits/plaits/dsp/engine/virtual_analog_engine.o build/plaits/plaits/dsp/voice.o build/plaits/plaits/dsp/fm/dx_units.o build/plaits/plaits/dsp/fm/algorithms.o build/plaits/plaits/dsp/chords/chord_bank.o build/plaits/plaits/dsp/physical_modelling/modal_voice.o build/plaits/plaits/dsp/physical_modelling/string_voice.o build/plaits/plaits/dsp/physical_modelling/resonator.o build/plaits/plaits/dsp/physical_modelling/string.o build/plaits/plaits/resources.o build/plaits/stmlib/dsp/units.o build/plaits/stmlib/utils/random.o $(RING_OBJECTS) $(UI_BASE_ARCHIVE)
	$(CXX) -std=c++17 -DGROOVE_PLAITS -DTEST -Isrc -Ithird_party/mutable -Ithird_party/daisysp/Source -Ithird_party/daisysp/Source/Utility -Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-variable -Wno-unused-local-typedefs -fsanitize=address,undefined tests/lab_test.cpp build/test-core-engine.o $(HOST_SUPPORT_OBJECTS) build/plaits/plaits/dsp/engine2/chiptune_engine.o build/plaits/plaits/dsp/engine2/virtual_analog_vcf_engine.o build/plaits/plaits/dsp/engine2/wave_terrain_engine.o build/plaits/plaits/dsp/engine2/six_op_engine.o build/plaits/plaits/dsp/engine2/phase_distortion_engine.o build/plaits/plaits/dsp/engine2/string_machine_engine.o build/plaits/plaits/dsp/speech/lpc_speech_synth.o build/plaits/plaits/dsp/speech/naive_speech_synth.o build/plaits/plaits/dsp/speech/sam_speech_synth.o build/plaits/plaits/dsp/speech/lpc_speech_synth_phonemes.o build/plaits/plaits/dsp/speech/lpc_speech_synth_words.o build/plaits/plaits/dsp/speech/lpc_speech_synth_controller.o build/plaits/plaits/dsp/engine/particle_engine.o build/plaits/plaits/dsp/engine/modal_engine.o build/plaits/plaits/dsp/engine/noise_engine.o build/plaits/plaits/dsp/engine/waveshaping_engine.o build/plaits/plaits/dsp/engine/string_engine.o build/plaits/plaits/dsp/engine/bass_drum_engine.o build/plaits/plaits/dsp/engine/swarm_engine.o build/plaits/plaits/dsp/engine/wavetable_engine.o build/plaits/plaits/dsp/engine/snare_drum_engine.o build/plaits/plaits/dsp/engine/chord_engine.o build/plaits/plaits/dsp/engine/fm_engine.o build/plaits/plaits/dsp/engine/grain_engine.o build/plaits/plaits/dsp/engine/additive_engine.o build/plaits/plaits/dsp/engine/hi_hat_engine.o build/plaits/plaits/dsp/engine/speech_engine.o build/plaits/plaits/dsp/engine/virtual_analog_engine.o build/plaits/plaits/dsp/voice.o build/plaits/plaits/dsp/fm/dx_units.o build/plaits/plaits/dsp/fm/algorithms.o build/plaits/plaits/dsp/chords/chord_bank.o build/plaits/plaits/dsp/physical_modelling/modal_voice.o build/plaits/plaits/dsp/physical_modelling/string_voice.o build/plaits/plaits/dsp/physical_modelling/resonator.o build/plaits/plaits/dsp/physical_modelling/string.o build/plaits/plaits/resources.o build/plaits/stmlib/dsp/units.o build/plaits/stmlib/utils/random.o $(RING_OBJECTS) -o build/test-lab $(UI_BASE_ARCHIVE)
build/test-song.o: tests/song_test.cpp $(wildcard src/*.h src/*.inc) $(wildcard platform/desktop/*.h)
	$(CXX) $(HOST_FLAGS) $(SAN) -O1 -Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-variable -Wno-unused-local-typedefs -c $< -o $@
build/test-song: build/test-song.o build/test-core-engine.o $(HOST_SUPPORT_OBJECTS) $(PL_OBJECTS) $(UI_BASE_ARCHIVE)
	$(CXX) $(HOST_FLAGS) $(SAN) -O1 $^ -o $@ $(UI_BASE_ARCHIVE)
build/test-song-page.o: tests/song_page_test.cpp $(wildcard src/*.h src/*.inc) $(wildcard platform/desktop/*.h)
	$(CXX) $(HOST_FLAGS) $(SAN) -O1 -Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-variable -Wno-unused-local-typedefs -c $< -o $@
build/test-song-page: build/test-song-page.o build/test-core-engine.o $(HOST_SUPPORT_OBJECTS) $(PL_OBJECTS) $(UI_BASE_ARCHIVE)
	$(CXX) $(HOST_FLAGS) $(SAN) -O1 $^ -o $@ $(UI_BASE_ARCHIVE)
build/test-wave-zones: tests/wave_zones_test.cpp build/test-core-engine.o $(HOST_SUPPORT_OBJECTS) build/plaits/plaits/dsp/engine2/chiptune_engine.o build/plaits/plaits/dsp/engine2/virtual_analog_vcf_engine.o build/plaits/plaits/dsp/engine2/wave_terrain_engine.o build/plaits/plaits/dsp/engine2/six_op_engine.o build/plaits/plaits/dsp/engine2/phase_distortion_engine.o build/plaits/plaits/dsp/engine2/string_machine_engine.o build/plaits/plaits/dsp/speech/lpc_speech_synth.o build/plaits/plaits/dsp/speech/naive_speech_synth.o build/plaits/plaits/dsp/speech/sam_speech_synth.o build/plaits/plaits/dsp/speech/lpc_speech_synth_phonemes.o build/plaits/plaits/dsp/speech/lpc_speech_synth_words.o build/plaits/plaits/dsp/speech/lpc_speech_synth_controller.o build/plaits/plaits/dsp/engine/particle_engine.o build/plaits/plaits/dsp/engine/modal_engine.o build/plaits/plaits/dsp/engine/noise_engine.o build/plaits/plaits/dsp/engine/waveshaping_engine.o build/plaits/plaits/dsp/engine/string_engine.o build/plaits/plaits/dsp/engine/bass_drum_engine.o build/plaits/plaits/dsp/engine/swarm_engine.o build/plaits/plaits/dsp/engine/wavetable_engine.o build/plaits/plaits/dsp/engine/snare_drum_engine.o build/plaits/plaits/dsp/engine/chord_engine.o build/plaits/plaits/dsp/engine/fm_engine.o build/plaits/plaits/dsp/engine/grain_engine.o build/plaits/plaits/dsp/engine/additive_engine.o build/plaits/plaits/dsp/engine/hi_hat_engine.o build/plaits/plaits/dsp/engine/speech_engine.o build/plaits/plaits/dsp/engine/virtual_analog_engine.o build/plaits/plaits/dsp/voice.o build/plaits/plaits/dsp/fm/dx_units.o build/plaits/plaits/dsp/fm/algorithms.o build/plaits/plaits/dsp/chords/chord_bank.o build/plaits/plaits/dsp/physical_modelling/modal_voice.o build/plaits/plaits/dsp/physical_modelling/string_voice.o build/plaits/plaits/dsp/physical_modelling/resonator.o build/plaits/plaits/dsp/physical_modelling/string.o build/plaits/plaits/resources.o build/plaits/stmlib/dsp/units.o build/plaits/stmlib/utils/random.o $(RING_OBJECTS) $(UI_BASE_ARCHIVE)
	$(CXX) -std=c++17 -DGROOVE_PLAITS -DTEST -Isrc -Ithird_party/mutable -Ithird_party/daisysp/Source -Ithird_party/daisysp/Source/Utility -Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-variable -Wno-unused-local-typedefs -fsanitize=address,undefined tests/wave_zones_test.cpp build/test-core-engine.o $(HOST_SUPPORT_OBJECTS) build/plaits/plaits/dsp/engine2/chiptune_engine.o build/plaits/plaits/dsp/engine2/virtual_analog_vcf_engine.o build/plaits/plaits/dsp/engine2/wave_terrain_engine.o build/plaits/plaits/dsp/engine2/six_op_engine.o build/plaits/plaits/dsp/engine2/phase_distortion_engine.o build/plaits/plaits/dsp/engine2/string_machine_engine.o build/plaits/plaits/dsp/speech/lpc_speech_synth.o build/plaits/plaits/dsp/speech/naive_speech_synth.o build/plaits/plaits/dsp/speech/sam_speech_synth.o build/plaits/plaits/dsp/speech/lpc_speech_synth_phonemes.o build/plaits/plaits/dsp/speech/lpc_speech_synth_words.o build/plaits/plaits/dsp/speech/lpc_speech_synth_controller.o build/plaits/plaits/dsp/engine/particle_engine.o build/plaits/plaits/dsp/engine/modal_engine.o build/plaits/plaits/dsp/engine/noise_engine.o build/plaits/plaits/dsp/engine/waveshaping_engine.o build/plaits/plaits/dsp/engine/string_engine.o build/plaits/plaits/dsp/engine/bass_drum_engine.o build/plaits/plaits/dsp/engine/swarm_engine.o build/plaits/plaits/dsp/engine/wavetable_engine.o build/plaits/plaits/dsp/engine/snare_drum_engine.o build/plaits/plaits/dsp/engine/chord_engine.o build/plaits/plaits/dsp/engine/fm_engine.o build/plaits/plaits/dsp/engine/grain_engine.o build/plaits/plaits/dsp/engine/additive_engine.o build/plaits/plaits/dsp/engine/hi_hat_engine.o build/plaits/plaits/dsp/engine/speech_engine.o build/plaits/plaits/dsp/engine/virtual_analog_engine.o build/plaits/plaits/dsp/voice.o build/plaits/plaits/dsp/fm/dx_units.o build/plaits/plaits/dsp/fm/algorithms.o build/plaits/plaits/dsp/chords/chord_bank.o build/plaits/plaits/dsp/physical_modelling/modal_voice.o build/plaits/plaits/dsp/physical_modelling/string_voice.o build/plaits/plaits/dsp/physical_modelling/resonator.o build/plaits/plaits/dsp/physical_modelling/string.o build/plaits/plaits/resources.o build/plaits/stmlib/dsp/units.o build/plaits/stmlib/utils/random.o $(RING_OBJECTS) -o build/test-wave-zones $(UI_BASE_ARCHIVE)
build/test-stretch: tests/stretch_test.cpp $(UI_BASE_ARCHIVE)
	$(CXX) -std=c++17 -DGROOVE_PLAITS -DTEST -Isrc -Ithird_party/mutable -Ithird_party/daisysp/Source -Ithird_party/daisysp/Source/Utility -Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-variable -Wno-unused-local-typedefs -fsanitize=address,undefined tests/stretch_test.cpp -o build/test-stretch $(UI_BASE_ARCHIVE)
build/test-stretch-job: tests/stretch_job_test.cpp build/test-core-engine.o $(HOST_SUPPORT_OBJECTS) build/plaits/plaits/dsp/engine2/chiptune_engine.o build/plaits/plaits/dsp/engine2/virtual_analog_vcf_engine.o build/plaits/plaits/dsp/engine2/wave_terrain_engine.o build/plaits/plaits/dsp/engine2/six_op_engine.o build/plaits/plaits/dsp/engine2/phase_distortion_engine.o build/plaits/plaits/dsp/engine2/string_machine_engine.o build/plaits/plaits/dsp/speech/lpc_speech_synth.o build/plaits/plaits/dsp/speech/naive_speech_synth.o build/plaits/plaits/dsp/speech/sam_speech_synth.o build/plaits/plaits/dsp/speech/lpc_speech_synth_phonemes.o build/plaits/plaits/dsp/speech/lpc_speech_synth_words.o build/plaits/plaits/dsp/speech/lpc_speech_synth_controller.o build/plaits/plaits/dsp/engine/particle_engine.o build/plaits/plaits/dsp/engine/modal_engine.o build/plaits/plaits/dsp/engine/noise_engine.o build/plaits/plaits/dsp/engine/waveshaping_engine.o build/plaits/plaits/dsp/engine/string_engine.o build/plaits/plaits/dsp/engine/bass_drum_engine.o build/plaits/plaits/dsp/engine/swarm_engine.o build/plaits/plaits/dsp/engine/wavetable_engine.o build/plaits/plaits/dsp/engine/snare_drum_engine.o build/plaits/plaits/dsp/engine/chord_engine.o build/plaits/plaits/dsp/engine/fm_engine.o build/plaits/plaits/dsp/engine/grain_engine.o build/plaits/plaits/dsp/engine/additive_engine.o build/plaits/plaits/dsp/engine/hi_hat_engine.o build/plaits/plaits/dsp/engine/speech_engine.o build/plaits/plaits/dsp/engine/virtual_analog_engine.o build/plaits/plaits/dsp/voice.o build/plaits/plaits/dsp/fm/dx_units.o build/plaits/plaits/dsp/fm/algorithms.o build/plaits/plaits/dsp/chords/chord_bank.o build/plaits/plaits/dsp/physical_modelling/modal_voice.o build/plaits/plaits/dsp/physical_modelling/string_voice.o build/plaits/plaits/dsp/physical_modelling/resonator.o build/plaits/plaits/dsp/physical_modelling/string.o build/plaits/plaits/resources.o build/plaits/stmlib/dsp/units.o build/plaits/stmlib/utils/random.o $(RING_OBJECTS) $(UI_BASE_ARCHIVE)
	$(CXX) -std=c++17 -DGROOVE_PLAITS -DTEST -Isrc -Ithird_party/mutable -Ithird_party/daisysp/Source -Ithird_party/daisysp/Source/Utility -Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-variable -Wno-unused-local-typedefs -fsanitize=address,undefined tests/stretch_job_test.cpp build/test-core-engine.o $(HOST_SUPPORT_OBJECTS) build/plaits/plaits/dsp/engine2/chiptune_engine.o build/plaits/plaits/dsp/engine2/virtual_analog_vcf_engine.o build/plaits/plaits/dsp/engine2/wave_terrain_engine.o build/plaits/plaits/dsp/engine2/six_op_engine.o build/plaits/plaits/dsp/engine2/phase_distortion_engine.o build/plaits/plaits/dsp/engine2/string_machine_engine.o build/plaits/plaits/dsp/speech/lpc_speech_synth.o build/plaits/plaits/dsp/speech/naive_speech_synth.o build/plaits/plaits/dsp/speech/sam_speech_synth.o build/plaits/plaits/dsp/speech/lpc_speech_synth_phonemes.o build/plaits/plaits/dsp/speech/lpc_speech_synth_words.o build/plaits/plaits/dsp/speech/lpc_speech_synth_controller.o build/plaits/plaits/dsp/engine/particle_engine.o build/plaits/plaits/dsp/engine/modal_engine.o build/plaits/plaits/dsp/engine/noise_engine.o build/plaits/plaits/dsp/engine/waveshaping_engine.o build/plaits/plaits/dsp/engine/string_engine.o build/plaits/plaits/dsp/engine/bass_drum_engine.o build/plaits/plaits/dsp/engine/swarm_engine.o build/plaits/plaits/dsp/engine/wavetable_engine.o build/plaits/plaits/dsp/engine/snare_drum_engine.o build/plaits/plaits/dsp/engine/chord_engine.o build/plaits/plaits/dsp/engine/fm_engine.o build/plaits/plaits/dsp/engine/grain_engine.o build/plaits/plaits/dsp/engine/additive_engine.o build/plaits/plaits/dsp/engine/hi_hat_engine.o build/plaits/plaits/dsp/engine/speech_engine.o build/plaits/plaits/dsp/engine/virtual_analog_engine.o build/plaits/plaits/dsp/voice.o build/plaits/plaits/dsp/fm/dx_units.o build/plaits/plaits/dsp/fm/algorithms.o build/plaits/plaits/dsp/chords/chord_bank.o build/plaits/plaits/dsp/physical_modelling/modal_voice.o build/plaits/plaits/dsp/physical_modelling/string_voice.o build/plaits/plaits/dsp/physical_modelling/resonator.o build/plaits/plaits/dsp/physical_modelling/string.o build/plaits/plaits/resources.o build/plaits/stmlib/dsp/units.o build/plaits/stmlib/utils/random.o $(RING_OBJECTS) -o build/test-stretch-job $(UI_BASE_ARCHIVE)
build/test-freeze: tests/freeze_test.cpp build/test-core-engine.o $(HOST_SUPPORT_OBJECTS) build/plaits/plaits/dsp/engine2/chiptune_engine.o build/plaits/plaits/dsp/engine2/virtual_analog_vcf_engine.o build/plaits/plaits/dsp/engine2/wave_terrain_engine.o build/plaits/plaits/dsp/engine2/six_op_engine.o build/plaits/plaits/dsp/engine2/phase_distortion_engine.o build/plaits/plaits/dsp/engine2/string_machine_engine.o build/plaits/plaits/dsp/speech/lpc_speech_synth.o build/plaits/plaits/dsp/speech/naive_speech_synth.o build/plaits/plaits/dsp/speech/sam_speech_synth.o build/plaits/plaits/dsp/speech/lpc_speech_synth_phonemes.o build/plaits/plaits/dsp/speech/lpc_speech_synth_words.o build/plaits/plaits/dsp/speech/lpc_speech_synth_controller.o build/plaits/plaits/dsp/engine/particle_engine.o build/plaits/plaits/dsp/engine/modal_engine.o build/plaits/plaits/dsp/engine/noise_engine.o build/plaits/plaits/dsp/engine/waveshaping_engine.o build/plaits/plaits/dsp/engine/string_engine.o build/plaits/plaits/dsp/engine/bass_drum_engine.o build/plaits/plaits/dsp/engine/swarm_engine.o build/plaits/plaits/dsp/engine/wavetable_engine.o build/plaits/plaits/dsp/engine/snare_drum_engine.o build/plaits/plaits/dsp/engine/chord_engine.o build/plaits/plaits/dsp/engine/fm_engine.o build/plaits/plaits/dsp/engine/grain_engine.o build/plaits/plaits/dsp/engine/additive_engine.o build/plaits/plaits/dsp/engine/hi_hat_engine.o build/plaits/plaits/dsp/engine/speech_engine.o build/plaits/plaits/dsp/engine/virtual_analog_engine.o build/plaits/plaits/dsp/voice.o build/plaits/plaits/dsp/fm/dx_units.o build/plaits/plaits/dsp/fm/algorithms.o build/plaits/plaits/dsp/chords/chord_bank.o build/plaits/plaits/dsp/physical_modelling/modal_voice.o build/plaits/plaits/dsp/physical_modelling/string_voice.o build/plaits/plaits/dsp/physical_modelling/resonator.o build/plaits/plaits/dsp/physical_modelling/string.o build/plaits/plaits/resources.o build/plaits/stmlib/dsp/units.o build/plaits/stmlib/utils/random.o $(RING_OBJECTS) $(UI_BASE_ARCHIVE)
	$(CXX) -std=c++17 -DGROOVE_PLAITS -DTEST -Isrc -Ithird_party/mutable -Ithird_party/daisysp/Source -Ithird_party/daisysp/Source/Utility -Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-variable -Wno-unused-local-typedefs -fsanitize=address,undefined tests/freeze_test.cpp build/test-core-engine.o $(HOST_SUPPORT_OBJECTS) build/plaits/plaits/dsp/engine2/chiptune_engine.o build/plaits/plaits/dsp/engine2/virtual_analog_vcf_engine.o build/plaits/plaits/dsp/engine2/wave_terrain_engine.o build/plaits/plaits/dsp/engine2/six_op_engine.o build/plaits/plaits/dsp/engine2/phase_distortion_engine.o build/plaits/plaits/dsp/engine2/string_machine_engine.o build/plaits/plaits/dsp/speech/lpc_speech_synth.o build/plaits/plaits/dsp/speech/naive_speech_synth.o build/plaits/plaits/dsp/speech/sam_speech_synth.o build/plaits/plaits/dsp/speech/lpc_speech_synth_phonemes.o build/plaits/plaits/dsp/speech/lpc_speech_synth_words.o build/plaits/plaits/dsp/speech/lpc_speech_synth_controller.o build/plaits/plaits/dsp/engine/particle_engine.o build/plaits/plaits/dsp/engine/modal_engine.o build/plaits/plaits/dsp/engine/noise_engine.o build/plaits/plaits/dsp/engine/waveshaping_engine.o build/plaits/plaits/dsp/engine/string_engine.o build/plaits/plaits/dsp/engine/bass_drum_engine.o build/plaits/plaits/dsp/engine/swarm_engine.o build/plaits/plaits/dsp/engine/wavetable_engine.o build/plaits/plaits/dsp/engine/snare_drum_engine.o build/plaits/plaits/dsp/engine/chord_engine.o build/plaits/plaits/dsp/engine/fm_engine.o build/plaits/plaits/dsp/engine/grain_engine.o build/plaits/plaits/dsp/engine/additive_engine.o build/plaits/plaits/dsp/engine/hi_hat_engine.o build/plaits/plaits/dsp/engine/speech_engine.o build/plaits/plaits/dsp/engine/virtual_analog_engine.o build/plaits/plaits/dsp/voice.o build/plaits/plaits/dsp/fm/dx_units.o build/plaits/plaits/dsp/fm/algorithms.o build/plaits/plaits/dsp/chords/chord_bank.o build/plaits/plaits/dsp/physical_modelling/modal_voice.o build/plaits/plaits/dsp/physical_modelling/string_voice.o build/plaits/plaits/dsp/physical_modelling/resonator.o build/plaits/plaits/dsp/physical_modelling/string.o build/plaits/plaits/resources.o build/plaits/stmlib/dsp/units.o build/plaits/stmlib/utils/random.o $(RING_OBJECTS) -o build/test-freeze $(UI_BASE_ARCHIVE)
build/test-warp: tests/warp_test.cpp build/test-core-engine.o $(HOST_SUPPORT_OBJECTS) build/plaits/plaits/dsp/engine2/chiptune_engine.o build/plaits/plaits/dsp/engine2/virtual_analog_vcf_engine.o build/plaits/plaits/dsp/engine2/wave_terrain_engine.o build/plaits/plaits/dsp/engine2/six_op_engine.o build/plaits/plaits/dsp/engine2/phase_distortion_engine.o build/plaits/plaits/dsp/engine2/string_machine_engine.o build/plaits/plaits/dsp/speech/lpc_speech_synth.o build/plaits/plaits/dsp/speech/naive_speech_synth.o build/plaits/plaits/dsp/speech/sam_speech_synth.o build/plaits/plaits/dsp/speech/lpc_speech_synth_phonemes.o build/plaits/plaits/dsp/speech/lpc_speech_synth_words.o build/plaits/plaits/dsp/speech/lpc_speech_synth_controller.o build/plaits/plaits/dsp/engine/particle_engine.o build/plaits/plaits/dsp/engine/modal_engine.o build/plaits/plaits/dsp/engine/noise_engine.o build/plaits/plaits/dsp/engine/waveshaping_engine.o build/plaits/plaits/dsp/engine/string_engine.o build/plaits/plaits/dsp/engine/bass_drum_engine.o build/plaits/plaits/dsp/engine/swarm_engine.o build/plaits/plaits/dsp/engine/wavetable_engine.o build/plaits/plaits/dsp/engine/snare_drum_engine.o build/plaits/plaits/dsp/engine/chord_engine.o build/plaits/plaits/dsp/engine/fm_engine.o build/plaits/plaits/dsp/engine/grain_engine.o build/plaits/plaits/dsp/engine/additive_engine.o build/plaits/plaits/dsp/engine/hi_hat_engine.o build/plaits/plaits/dsp/engine/speech_engine.o build/plaits/plaits/dsp/engine/virtual_analog_engine.o build/plaits/plaits/dsp/voice.o build/plaits/plaits/dsp/fm/dx_units.o build/plaits/plaits/dsp/fm/algorithms.o build/plaits/plaits/dsp/chords/chord_bank.o build/plaits/plaits/dsp/physical_modelling/modal_voice.o build/plaits/plaits/dsp/physical_modelling/string_voice.o build/plaits/plaits/dsp/physical_modelling/resonator.o build/plaits/plaits/dsp/physical_modelling/string.o build/plaits/plaits/resources.o build/plaits/stmlib/dsp/units.o build/plaits/stmlib/utils/random.o $(RING_OBJECTS) $(UI_BASE_ARCHIVE)
	$(CXX) -std=c++17 -DGROOVE_PLAITS -DTEST -Isrc -Ithird_party/mutable -Ithird_party/daisysp/Source -Ithird_party/daisysp/Source/Utility -Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-variable -Wno-unused-local-typedefs -fsanitize=address,undefined tests/warp_test.cpp build/test-core-engine.o $(HOST_SUPPORT_OBJECTS) build/plaits/plaits/dsp/engine2/chiptune_engine.o build/plaits/plaits/dsp/engine2/virtual_analog_vcf_engine.o build/plaits/plaits/dsp/engine2/wave_terrain_engine.o build/plaits/plaits/dsp/engine2/six_op_engine.o build/plaits/plaits/dsp/engine2/phase_distortion_engine.o build/plaits/plaits/dsp/engine2/string_machine_engine.o build/plaits/plaits/dsp/speech/lpc_speech_synth.o build/plaits/plaits/dsp/speech/naive_speech_synth.o build/plaits/plaits/dsp/speech/sam_speech_synth.o build/plaits/plaits/dsp/speech/lpc_speech_synth_phonemes.o build/plaits/plaits/dsp/speech/lpc_speech_synth_words.o build/plaits/plaits/dsp/speech/lpc_speech_synth_controller.o build/plaits/plaits/dsp/engine/particle_engine.o build/plaits/plaits/dsp/engine/modal_engine.o build/plaits/plaits/dsp/engine/noise_engine.o build/plaits/plaits/dsp/engine/waveshaping_engine.o build/plaits/plaits/dsp/engine/string_engine.o build/plaits/plaits/dsp/engine/bass_drum_engine.o build/plaits/plaits/dsp/engine/swarm_engine.o build/plaits/plaits/dsp/engine/wavetable_engine.o build/plaits/plaits/dsp/engine/snare_drum_engine.o build/plaits/plaits/dsp/engine/chord_engine.o build/plaits/plaits/dsp/engine/fm_engine.o build/plaits/plaits/dsp/engine/grain_engine.o build/plaits/plaits/dsp/engine/additive_engine.o build/plaits/plaits/dsp/engine/hi_hat_engine.o build/plaits/plaits/dsp/engine/speech_engine.o build/plaits/plaits/dsp/engine/virtual_analog_engine.o build/plaits/plaits/dsp/voice.o build/plaits/plaits/dsp/fm/dx_units.o build/plaits/plaits/dsp/fm/algorithms.o build/plaits/plaits/dsp/chords/chord_bank.o build/plaits/plaits/dsp/physical_modelling/modal_voice.o build/plaits/plaits/dsp/physical_modelling/string_voice.o build/plaits/plaits/dsp/physical_modelling/resonator.o build/plaits/plaits/dsp/physical_modelling/string.o build/plaits/plaits/resources.o build/plaits/stmlib/dsp/units.o build/plaits/stmlib/utils/random.o $(RING_OBJECTS) -o build/test-warp $(UI_BASE_ARCHIVE)
build/test-stretch-playback: tests/stretch_playback_test.cpp build/test-core-engine.o $(HOST_SUPPORT_OBJECTS) build/plaits/plaits/dsp/engine2/chiptune_engine.o build/plaits/plaits/dsp/engine2/virtual_analog_vcf_engine.o build/plaits/plaits/dsp/engine2/wave_terrain_engine.o build/plaits/plaits/dsp/engine2/six_op_engine.o build/plaits/plaits/dsp/engine2/phase_distortion_engine.o build/plaits/plaits/dsp/engine2/string_machine_engine.o build/plaits/plaits/dsp/speech/lpc_speech_synth.o build/plaits/plaits/dsp/speech/naive_speech_synth.o build/plaits/plaits/dsp/speech/sam_speech_synth.o build/plaits/plaits/dsp/speech/lpc_speech_synth_phonemes.o build/plaits/plaits/dsp/speech/lpc_speech_synth_words.o build/plaits/plaits/dsp/speech/lpc_speech_synth_controller.o build/plaits/plaits/dsp/engine/particle_engine.o build/plaits/plaits/dsp/engine/modal_engine.o build/plaits/plaits/dsp/engine/noise_engine.o build/plaits/plaits/dsp/engine/waveshaping_engine.o build/plaits/plaits/dsp/engine/string_engine.o build/plaits/plaits/dsp/engine/bass_drum_engine.o build/plaits/plaits/dsp/engine/swarm_engine.o build/plaits/plaits/dsp/engine/wavetable_engine.o build/plaits/plaits/dsp/engine/snare_drum_engine.o build/plaits/plaits/dsp/engine/chord_engine.o build/plaits/plaits/dsp/engine/fm_engine.o build/plaits/plaits/dsp/engine/grain_engine.o build/plaits/plaits/dsp/engine/additive_engine.o build/plaits/plaits/dsp/engine/hi_hat_engine.o build/plaits/plaits/dsp/engine/speech_engine.o build/plaits/plaits/dsp/engine/virtual_analog_engine.o build/plaits/plaits/dsp/voice.o build/plaits/plaits/dsp/fm/dx_units.o build/plaits/plaits/dsp/fm/algorithms.o build/plaits/plaits/dsp/chords/chord_bank.o build/plaits/plaits/dsp/physical_modelling/modal_voice.o build/plaits/plaits/dsp/physical_modelling/string_voice.o build/plaits/plaits/dsp/physical_modelling/resonator.o build/plaits/plaits/dsp/physical_modelling/string.o build/plaits/plaits/resources.o build/plaits/stmlib/dsp/units.o build/plaits/stmlib/utils/random.o $(RING_OBJECTS) $(UI_BASE_ARCHIVE)
	$(CXX) -std=c++17 -DGROOVE_PLAITS -DTEST -Isrc -Ithird_party/mutable -Ithird_party/daisysp/Source -Ithird_party/daisysp/Source/Utility -Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-variable -Wno-unused-local-typedefs -fsanitize=address,undefined tests/stretch_playback_test.cpp build/test-core-engine.o $(HOST_SUPPORT_OBJECTS) build/plaits/plaits/dsp/engine2/chiptune_engine.o build/plaits/plaits/dsp/engine2/virtual_analog_vcf_engine.o build/plaits/plaits/dsp/engine2/wave_terrain_engine.o build/plaits/plaits/dsp/engine2/six_op_engine.o build/plaits/plaits/dsp/engine2/phase_distortion_engine.o build/plaits/plaits/dsp/engine2/string_machine_engine.o build/plaits/plaits/dsp/speech/lpc_speech_synth.o build/plaits/plaits/dsp/speech/naive_speech_synth.o build/plaits/plaits/dsp/speech/sam_speech_synth.o build/plaits/plaits/dsp/speech/lpc_speech_synth_phonemes.o build/plaits/plaits/dsp/speech/lpc_speech_synth_words.o build/plaits/plaits/dsp/speech/lpc_speech_synth_controller.o build/plaits/plaits/dsp/engine/particle_engine.o build/plaits/plaits/dsp/engine/modal_engine.o build/plaits/plaits/dsp/engine/noise_engine.o build/plaits/plaits/dsp/engine/waveshaping_engine.o build/plaits/plaits/dsp/engine/string_engine.o build/plaits/plaits/dsp/engine/bass_drum_engine.o build/plaits/plaits/dsp/engine/swarm_engine.o build/plaits/plaits/dsp/engine/wavetable_engine.o build/plaits/plaits/dsp/engine/snare_drum_engine.o build/plaits/plaits/dsp/engine/chord_engine.o build/plaits/plaits/dsp/engine/fm_engine.o build/plaits/plaits/dsp/engine/grain_engine.o build/plaits/plaits/dsp/engine/additive_engine.o build/plaits/plaits/dsp/engine/hi_hat_engine.o build/plaits/plaits/dsp/engine/speech_engine.o build/plaits/plaits/dsp/engine/virtual_analog_engine.o build/plaits/plaits/dsp/voice.o build/plaits/plaits/dsp/fm/dx_units.o build/plaits/plaits/dsp/fm/algorithms.o build/plaits/plaits/dsp/chords/chord_bank.o build/plaits/plaits/dsp/physical_modelling/modal_voice.o build/plaits/plaits/dsp/physical_modelling/string_voice.o build/plaits/plaits/dsp/physical_modelling/resonator.o build/plaits/plaits/dsp/physical_modelling/string.o build/plaits/plaits/resources.o build/plaits/stmlib/dsp/units.o build/plaits/stmlib/utils/random.o $(RING_OBJECTS) -o build/test-stretch-playback $(UI_BASE_ARCHIVE)
build/test-mod-matrix: tests/mod_matrix_test.cpp build/test-core-engine.o $(HOST_SUPPORT_OBJECTS) build/plaits/plaits/dsp/engine2/chiptune_engine.o build/plaits/plaits/dsp/engine2/virtual_analog_vcf_engine.o build/plaits/plaits/dsp/engine2/wave_terrain_engine.o build/plaits/plaits/dsp/engine2/six_op_engine.o build/plaits/plaits/dsp/engine2/phase_distortion_engine.o build/plaits/plaits/dsp/engine2/string_machine_engine.o build/plaits/plaits/dsp/speech/lpc_speech_synth.o build/plaits/plaits/dsp/speech/naive_speech_synth.o build/plaits/plaits/dsp/speech/sam_speech_synth.o build/plaits/plaits/dsp/speech/lpc_speech_synth_phonemes.o build/plaits/plaits/dsp/speech/lpc_speech_synth_words.o build/plaits/plaits/dsp/speech/lpc_speech_synth_controller.o build/plaits/plaits/dsp/engine/particle_engine.o build/plaits/plaits/dsp/engine/modal_engine.o build/plaits/plaits/dsp/engine/noise_engine.o build/plaits/plaits/dsp/engine/waveshaping_engine.o build/plaits/plaits/dsp/engine/string_engine.o build/plaits/plaits/dsp/engine/bass_drum_engine.o build/plaits/plaits/dsp/engine/swarm_engine.o build/plaits/plaits/dsp/engine/wavetable_engine.o build/plaits/plaits/dsp/engine/snare_drum_engine.o build/plaits/plaits/dsp/engine/chord_engine.o build/plaits/plaits/dsp/engine/fm_engine.o build/plaits/plaits/dsp/engine/grain_engine.o build/plaits/plaits/dsp/engine/additive_engine.o build/plaits/plaits/dsp/engine/hi_hat_engine.o build/plaits/plaits/dsp/engine/speech_engine.o build/plaits/plaits/dsp/engine/virtual_analog_engine.o build/plaits/plaits/dsp/voice.o build/plaits/plaits/dsp/fm/dx_units.o build/plaits/plaits/dsp/fm/algorithms.o build/plaits/plaits/dsp/chords/chord_bank.o build/plaits/plaits/dsp/physical_modelling/modal_voice.o build/plaits/plaits/dsp/physical_modelling/string_voice.o build/plaits/plaits/dsp/physical_modelling/resonator.o build/plaits/plaits/dsp/physical_modelling/string.o build/plaits/plaits/resources.o build/plaits/stmlib/dsp/units.o build/plaits/stmlib/utils/random.o $(RING_OBJECTS) $(UI_BASE_ARCHIVE)
	$(CXX) -std=c++17 -DGROOVE_PLAITS -DTEST -Isrc -Ithird_party/mutable -Ithird_party/daisysp/Source -Ithird_party/daisysp/Source/Utility -Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-variable -Wno-unused-local-typedefs -fsanitize=address,undefined tests/mod_matrix_test.cpp build/test-core-engine.o $(HOST_SUPPORT_OBJECTS) build/plaits/plaits/dsp/engine2/chiptune_engine.o build/plaits/plaits/dsp/engine2/virtual_analog_vcf_engine.o build/plaits/plaits/dsp/engine2/wave_terrain_engine.o build/plaits/plaits/dsp/engine2/six_op_engine.o build/plaits/plaits/dsp/engine2/phase_distortion_engine.o build/plaits/plaits/dsp/engine2/string_machine_engine.o build/plaits/plaits/dsp/speech/lpc_speech_synth.o build/plaits/plaits/dsp/speech/naive_speech_synth.o build/plaits/plaits/dsp/speech/sam_speech_synth.o build/plaits/plaits/dsp/speech/lpc_speech_synth_phonemes.o build/plaits/plaits/dsp/speech/lpc_speech_synth_words.o build/plaits/plaits/dsp/speech/lpc_speech_synth_controller.o build/plaits/plaits/dsp/engine/particle_engine.o build/plaits/plaits/dsp/engine/modal_engine.o build/plaits/plaits/dsp/engine/noise_engine.o build/plaits/plaits/dsp/engine/waveshaping_engine.o build/plaits/plaits/dsp/engine/string_engine.o build/plaits/plaits/dsp/engine/bass_drum_engine.o build/plaits/plaits/dsp/engine/swarm_engine.o build/plaits/plaits/dsp/engine/wavetable_engine.o build/plaits/plaits/dsp/engine/snare_drum_engine.o build/plaits/plaits/dsp/engine/chord_engine.o build/plaits/plaits/dsp/engine/fm_engine.o build/plaits/plaits/dsp/engine/grain_engine.o build/plaits/plaits/dsp/engine/additive_engine.o build/plaits/plaits/dsp/engine/hi_hat_engine.o build/plaits/plaits/dsp/engine/speech_engine.o build/plaits/plaits/dsp/engine/virtual_analog_engine.o build/plaits/plaits/dsp/voice.o build/plaits/plaits/dsp/fm/dx_units.o build/plaits/plaits/dsp/fm/algorithms.o build/plaits/plaits/dsp/chords/chord_bank.o build/plaits/plaits/dsp/physical_modelling/modal_voice.o build/plaits/plaits/dsp/physical_modelling/string_voice.o build/plaits/plaits/dsp/physical_modelling/resonator.o build/plaits/plaits/dsp/physical_modelling/string.o build/plaits/plaits/resources.o build/plaits/stmlib/dsp/units.o build/plaits/stmlib/utils/random.o $(RING_OBJECTS) -o build/test-mod-matrix $(UI_BASE_ARCHIVE)
build/test-fx-type-lock: tests/fx_type_lock_test.cpp build/test-core-engine.o $(HOST_SUPPORT_OBJECTS) build/plaits/plaits/dsp/engine2/chiptune_engine.o build/plaits/plaits/dsp/engine2/virtual_analog_vcf_engine.o build/plaits/plaits/dsp/engine2/wave_terrain_engine.o build/plaits/plaits/dsp/engine2/six_op_engine.o build/plaits/plaits/dsp/engine2/phase_distortion_engine.o build/plaits/plaits/dsp/engine2/string_machine_engine.o build/plaits/plaits/dsp/speech/lpc_speech_synth.o build/plaits/plaits/dsp/speech/naive_speech_synth.o build/plaits/plaits/dsp/speech/sam_speech_synth.o build/plaits/plaits/dsp/speech/lpc_speech_synth_phonemes.o build/plaits/plaits/dsp/speech/lpc_speech_synth_words.o build/plaits/plaits/dsp/speech/lpc_speech_synth_controller.o build/plaits/plaits/dsp/engine/particle_engine.o build/plaits/plaits/dsp/engine/modal_engine.o build/plaits/plaits/dsp/engine/noise_engine.o build/plaits/plaits/dsp/engine/waveshaping_engine.o build/plaits/plaits/dsp/engine/string_engine.o build/plaits/plaits/dsp/engine/bass_drum_engine.o build/plaits/plaits/dsp/engine/swarm_engine.o build/plaits/plaits/dsp/engine/wavetable_engine.o build/plaits/plaits/dsp/engine/snare_drum_engine.o build/plaits/plaits/dsp/engine/chord_engine.o build/plaits/plaits/dsp/engine/fm_engine.o build/plaits/plaits/dsp/engine/grain_engine.o build/plaits/plaits/dsp/engine/additive_engine.o build/plaits/plaits/dsp/engine/hi_hat_engine.o build/plaits/plaits/dsp/engine/speech_engine.o build/plaits/plaits/dsp/engine/virtual_analog_engine.o build/plaits/plaits/dsp/voice.o build/plaits/plaits/dsp/fm/dx_units.o build/plaits/plaits/dsp/fm/algorithms.o build/plaits/plaits/dsp/chords/chord_bank.o build/plaits/plaits/dsp/physical_modelling/modal_voice.o build/plaits/plaits/dsp/physical_modelling/string_voice.o build/plaits/plaits/dsp/physical_modelling/resonator.o build/plaits/plaits/dsp/physical_modelling/string.o build/plaits/plaits/resources.o build/plaits/stmlib/dsp/units.o build/plaits/stmlib/utils/random.o $(RING_OBJECTS) $(UI_BASE_ARCHIVE)
	$(CXX) -std=c++17 -DGROOVE_PLAITS -DTEST -Isrc -Ithird_party/mutable -Ithird_party/daisysp/Source -Ithird_party/daisysp/Source/Utility -Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-variable -Wno-unused-local-typedefs -fsanitize=address,undefined tests/fx_type_lock_test.cpp build/test-core-engine.o $(HOST_SUPPORT_OBJECTS) build/plaits/plaits/dsp/engine2/chiptune_engine.o build/plaits/plaits/dsp/engine2/virtual_analog_vcf_engine.o build/plaits/plaits/dsp/engine2/wave_terrain_engine.o build/plaits/plaits/dsp/engine2/six_op_engine.o build/plaits/plaits/dsp/engine2/phase_distortion_engine.o build/plaits/plaits/dsp/engine2/string_machine_engine.o build/plaits/plaits/dsp/speech/lpc_speech_synth.o build/plaits/plaits/dsp/speech/naive_speech_synth.o build/plaits/plaits/dsp/speech/sam_speech_synth.o build/plaits/plaits/dsp/speech/lpc_speech_synth_phonemes.o build/plaits/plaits/dsp/speech/lpc_speech_synth_words.o build/plaits/plaits/dsp/speech/lpc_speech_synth_controller.o build/plaits/plaits/dsp/engine/particle_engine.o build/plaits/plaits/dsp/engine/modal_engine.o build/plaits/plaits/dsp/engine/noise_engine.o build/plaits/plaits/dsp/engine/waveshaping_engine.o build/plaits/plaits/dsp/engine/string_engine.o build/plaits/plaits/dsp/engine/bass_drum_engine.o build/plaits/plaits/dsp/engine/swarm_engine.o build/plaits/plaits/dsp/engine/wavetable_engine.o build/plaits/plaits/dsp/engine/snare_drum_engine.o build/plaits/plaits/dsp/engine/chord_engine.o build/plaits/plaits/dsp/engine/fm_engine.o build/plaits/plaits/dsp/engine/grain_engine.o build/plaits/plaits/dsp/engine/additive_engine.o build/plaits/plaits/dsp/engine/hi_hat_engine.o build/plaits/plaits/dsp/engine/speech_engine.o build/plaits/plaits/dsp/engine/virtual_analog_engine.o build/plaits/plaits/dsp/voice.o build/plaits/plaits/dsp/fm/dx_units.o build/plaits/plaits/dsp/fm/algorithms.o build/plaits/plaits/dsp/chords/chord_bank.o build/plaits/plaits/dsp/physical_modelling/modal_voice.o build/plaits/plaits/dsp/physical_modelling/string_voice.o build/plaits/plaits/dsp/physical_modelling/resonator.o build/plaits/plaits/dsp/physical_modelling/string.o build/plaits/plaits/resources.o build/plaits/stmlib/dsp/units.o build/plaits/stmlib/utils/random.o $(RING_OBJECTS) -o build/test-fx-type-lock $(UI_BASE_ARCHIVE)
build/test-sound-state: tests/sound_state_test.cpp build/test-core-engine.o $(HOST_SUPPORT_OBJECTS) build/plaits/plaits/dsp/engine2/chiptune_engine.o build/plaits/plaits/dsp/engine2/virtual_analog_vcf_engine.o build/plaits/plaits/dsp/engine2/wave_terrain_engine.o build/plaits/plaits/dsp/engine2/six_op_engine.o build/plaits/plaits/dsp/engine2/phase_distortion_engine.o build/plaits/plaits/dsp/engine2/string_machine_engine.o build/plaits/plaits/dsp/speech/lpc_speech_synth.o build/plaits/plaits/dsp/speech/naive_speech_synth.o build/plaits/plaits/dsp/speech/sam_speech_synth.o build/plaits/plaits/dsp/speech/lpc_speech_synth_phonemes.o build/plaits/plaits/dsp/speech/lpc_speech_synth_words.o build/plaits/plaits/dsp/speech/lpc_speech_synth_controller.o build/plaits/plaits/dsp/engine/particle_engine.o build/plaits/plaits/dsp/engine/modal_engine.o build/plaits/plaits/dsp/engine/noise_engine.o build/plaits/plaits/dsp/engine/waveshaping_engine.o build/plaits/plaits/dsp/engine/string_engine.o build/plaits/plaits/dsp/engine/bass_drum_engine.o build/plaits/plaits/dsp/engine/swarm_engine.o build/plaits/plaits/dsp/engine/wavetable_engine.o build/plaits/plaits/dsp/engine/snare_drum_engine.o build/plaits/plaits/dsp/engine/chord_engine.o build/plaits/plaits/dsp/engine/fm_engine.o build/plaits/plaits/dsp/engine/grain_engine.o build/plaits/plaits/dsp/engine/additive_engine.o build/plaits/plaits/dsp/engine/hi_hat_engine.o build/plaits/plaits/dsp/engine/speech_engine.o build/plaits/plaits/dsp/engine/virtual_analog_engine.o build/plaits/plaits/dsp/voice.o build/plaits/plaits/dsp/fm/dx_units.o build/plaits/plaits/dsp/fm/algorithms.o build/plaits/plaits/dsp/chords/chord_bank.o build/plaits/plaits/dsp/physical_modelling/modal_voice.o build/plaits/plaits/dsp/physical_modelling/string_voice.o build/plaits/plaits/dsp/physical_modelling/resonator.o build/plaits/plaits/dsp/physical_modelling/string.o build/plaits/plaits/resources.o build/plaits/stmlib/dsp/units.o build/plaits/stmlib/utils/random.o $(RING_OBJECTS) $(UI_BASE_ARCHIVE)
	$(CXX) -std=c++17 -DGROOVE_PLAITS -DTEST -Isrc -Ithird_party/mutable -Ithird_party/daisysp/Source -Ithird_party/daisysp/Source/Utility -Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-variable -Wno-unused-local-typedefs -fsanitize=address,undefined tests/sound_state_test.cpp build/test-core-engine.o $(HOST_SUPPORT_OBJECTS) build/plaits/plaits/dsp/engine2/chiptune_engine.o build/plaits/plaits/dsp/engine2/virtual_analog_vcf_engine.o build/plaits/plaits/dsp/engine2/wave_terrain_engine.o build/plaits/plaits/dsp/engine2/six_op_engine.o build/plaits/plaits/dsp/engine2/phase_distortion_engine.o build/plaits/plaits/dsp/engine2/string_machine_engine.o build/plaits/plaits/dsp/speech/lpc_speech_synth.o build/plaits/plaits/dsp/speech/naive_speech_synth.o build/plaits/plaits/dsp/speech/sam_speech_synth.o build/plaits/plaits/dsp/speech/lpc_speech_synth_phonemes.o build/plaits/plaits/dsp/speech/lpc_speech_synth_words.o build/plaits/plaits/dsp/speech/lpc_speech_synth_controller.o build/plaits/plaits/dsp/engine/particle_engine.o build/plaits/plaits/dsp/engine/modal_engine.o build/plaits/plaits/dsp/engine/noise_engine.o build/plaits/plaits/dsp/engine/waveshaping_engine.o build/plaits/plaits/dsp/engine/string_engine.o build/plaits/plaits/dsp/engine/bass_drum_engine.o build/plaits/plaits/dsp/engine/swarm_engine.o build/plaits/plaits/dsp/engine/wavetable_engine.o build/plaits/plaits/dsp/engine/snare_drum_engine.o build/plaits/plaits/dsp/engine/chord_engine.o build/plaits/plaits/dsp/engine/fm_engine.o build/plaits/plaits/dsp/engine/grain_engine.o build/plaits/plaits/dsp/engine/additive_engine.o build/plaits/plaits/dsp/engine/hi_hat_engine.o build/plaits/plaits/dsp/engine/speech_engine.o build/plaits/plaits/dsp/engine/virtual_analog_engine.o build/plaits/plaits/dsp/voice.o build/plaits/plaits/dsp/fm/dx_units.o build/plaits/plaits/dsp/fm/algorithms.o build/plaits/plaits/dsp/chords/chord_bank.o build/plaits/plaits/dsp/physical_modelling/modal_voice.o build/plaits/plaits/dsp/physical_modelling/string_voice.o build/plaits/plaits/dsp/physical_modelling/resonator.o build/plaits/plaits/dsp/physical_modelling/string.o build/plaits/plaits/resources.o build/plaits/stmlib/dsp/units.o build/plaits/stmlib/utils/random.o $(RING_OBJECTS) -o build/test-sound-state $(UI_BASE_ARCHIVE)
build/test-reload: tests/reload_test.cpp build/test-core-engine.o $(HOST_SUPPORT_OBJECTS) build/plaits/plaits/dsp/engine2/chiptune_engine.o build/plaits/plaits/dsp/engine2/virtual_analog_vcf_engine.o build/plaits/plaits/dsp/engine2/wave_terrain_engine.o build/plaits/plaits/dsp/engine2/six_op_engine.o build/plaits/plaits/dsp/engine2/phase_distortion_engine.o build/plaits/plaits/dsp/engine2/string_machine_engine.o build/plaits/plaits/dsp/speech/lpc_speech_synth.o build/plaits/plaits/dsp/speech/naive_speech_synth.o build/plaits/plaits/dsp/speech/sam_speech_synth.o build/plaits/plaits/dsp/speech/lpc_speech_synth_phonemes.o build/plaits/plaits/dsp/speech/lpc_speech_synth_words.o build/plaits/plaits/dsp/speech/lpc_speech_synth_controller.o build/plaits/plaits/dsp/engine/particle_engine.o build/plaits/plaits/dsp/engine/modal_engine.o build/plaits/plaits/dsp/engine/noise_engine.o build/plaits/plaits/dsp/engine/waveshaping_engine.o build/plaits/plaits/dsp/engine/string_engine.o build/plaits/plaits/dsp/engine/bass_drum_engine.o build/plaits/plaits/dsp/engine/swarm_engine.o build/plaits/plaits/dsp/engine/wavetable_engine.o build/plaits/plaits/dsp/engine/snare_drum_engine.o build/plaits/plaits/dsp/engine/chord_engine.o build/plaits/plaits/dsp/engine/fm_engine.o build/plaits/plaits/dsp/engine/grain_engine.o build/plaits/plaits/dsp/engine/additive_engine.o build/plaits/plaits/dsp/engine/hi_hat_engine.o build/plaits/plaits/dsp/engine/speech_engine.o build/plaits/plaits/dsp/engine/virtual_analog_engine.o build/plaits/plaits/dsp/voice.o build/plaits/plaits/dsp/fm/dx_units.o build/plaits/plaits/dsp/fm/algorithms.o build/plaits/plaits/dsp/chords/chord_bank.o build/plaits/plaits/dsp/physical_modelling/modal_voice.o build/plaits/plaits/dsp/physical_modelling/string_voice.o build/plaits/plaits/dsp/physical_modelling/resonator.o build/plaits/plaits/dsp/physical_modelling/string.o build/plaits/plaits/resources.o build/plaits/stmlib/dsp/units.o build/plaits/stmlib/utils/random.o $(RING_OBJECTS) $(UI_BASE_ARCHIVE)
	$(CXX) -std=c++17 -DGROOVE_PLAITS -DTEST -Isrc -Ithird_party/mutable -Ithird_party/daisysp/Source -Ithird_party/daisysp/Source/Utility -Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-variable -Wno-unused-local-typedefs -fsanitize=address,undefined tests/reload_test.cpp build/test-core-engine.o $(HOST_SUPPORT_OBJECTS) build/plaits/plaits/dsp/engine2/chiptune_engine.o build/plaits/plaits/dsp/engine2/virtual_analog_vcf_engine.o build/plaits/plaits/dsp/engine2/wave_terrain_engine.o build/plaits/plaits/dsp/engine2/six_op_engine.o build/plaits/plaits/dsp/engine2/phase_distortion_engine.o build/plaits/plaits/dsp/engine2/string_machine_engine.o build/plaits/plaits/dsp/speech/lpc_speech_synth.o build/plaits/plaits/dsp/speech/naive_speech_synth.o build/plaits/plaits/dsp/speech/sam_speech_synth.o build/plaits/plaits/dsp/speech/lpc_speech_synth_phonemes.o build/plaits/plaits/dsp/speech/lpc_speech_synth_words.o build/plaits/plaits/dsp/speech/lpc_speech_synth_controller.o build/plaits/plaits/dsp/engine/particle_engine.o build/plaits/plaits/dsp/engine/modal_engine.o build/plaits/plaits/dsp/engine/noise_engine.o build/plaits/plaits/dsp/engine/waveshaping_engine.o build/plaits/plaits/dsp/engine/string_engine.o build/plaits/plaits/dsp/engine/bass_drum_engine.o build/plaits/plaits/dsp/engine/swarm_engine.o build/plaits/plaits/dsp/engine/wavetable_engine.o build/plaits/plaits/dsp/engine/snare_drum_engine.o build/plaits/plaits/dsp/engine/chord_engine.o build/plaits/plaits/dsp/engine/fm_engine.o build/plaits/plaits/dsp/engine/grain_engine.o build/plaits/plaits/dsp/engine/additive_engine.o build/plaits/plaits/dsp/engine/hi_hat_engine.o build/plaits/plaits/dsp/engine/speech_engine.o build/plaits/plaits/dsp/engine/virtual_analog_engine.o build/plaits/plaits/dsp/voice.o build/plaits/plaits/dsp/fm/dx_units.o build/plaits/plaits/dsp/fm/algorithms.o build/plaits/plaits/dsp/chords/chord_bank.o build/plaits/plaits/dsp/physical_modelling/modal_voice.o build/plaits/plaits/dsp/physical_modelling/string_voice.o build/plaits/plaits/dsp/physical_modelling/resonator.o build/plaits/plaits/dsp/physical_modelling/string.o build/plaits/plaits/resources.o build/plaits/stmlib/dsp/units.o build/plaits/stmlib/utils/random.o $(RING_OBJECTS) -o build/test-reload $(UI_BASE_ARCHIVE)
build/test-morph-sets: tests/morph_sets_test.cpp build/test-core-engine.o $(HOST_SUPPORT_OBJECTS) build/plaits/plaits/dsp/engine2/chiptune_engine.o build/plaits/plaits/dsp/engine2/virtual_analog_vcf_engine.o build/plaits/plaits/dsp/engine2/wave_terrain_engine.o build/plaits/plaits/dsp/engine2/six_op_engine.o build/plaits/plaits/dsp/engine2/phase_distortion_engine.o build/plaits/plaits/dsp/engine2/string_machine_engine.o build/plaits/plaits/dsp/speech/lpc_speech_synth.o build/plaits/plaits/dsp/speech/naive_speech_synth.o build/plaits/plaits/dsp/speech/sam_speech_synth.o build/plaits/plaits/dsp/speech/lpc_speech_synth_phonemes.o build/plaits/plaits/dsp/speech/lpc_speech_synth_words.o build/plaits/plaits/dsp/speech/lpc_speech_synth_controller.o build/plaits/plaits/dsp/engine/particle_engine.o build/plaits/plaits/dsp/engine/modal_engine.o build/plaits/plaits/dsp/engine/noise_engine.o build/plaits/plaits/dsp/engine/waveshaping_engine.o build/plaits/plaits/dsp/engine/string_engine.o build/plaits/plaits/dsp/engine/bass_drum_engine.o build/plaits/plaits/dsp/engine/swarm_engine.o build/plaits/plaits/dsp/engine/wavetable_engine.o build/plaits/plaits/dsp/engine/snare_drum_engine.o build/plaits/plaits/dsp/engine/chord_engine.o build/plaits/plaits/dsp/engine/fm_engine.o build/plaits/plaits/dsp/engine/grain_engine.o build/plaits/plaits/dsp/engine/additive_engine.o build/plaits/plaits/dsp/engine/hi_hat_engine.o build/plaits/plaits/dsp/engine/speech_engine.o build/plaits/plaits/dsp/engine/virtual_analog_engine.o build/plaits/plaits/dsp/voice.o build/plaits/plaits/dsp/fm/dx_units.o build/plaits/plaits/dsp/fm/algorithms.o build/plaits/plaits/dsp/chords/chord_bank.o build/plaits/plaits/dsp/physical_modelling/modal_voice.o build/plaits/plaits/dsp/physical_modelling/string_voice.o build/plaits/plaits/dsp/physical_modelling/resonator.o build/plaits/plaits/dsp/physical_modelling/string.o build/plaits/plaits/resources.o build/plaits/stmlib/dsp/units.o build/plaits/stmlib/utils/random.o $(RING_OBJECTS) $(UI_BASE_ARCHIVE)
	$(CXX) -std=c++17 -DGROOVE_PLAITS -DTEST -Isrc -Ithird_party/mutable -Ithird_party/daisysp/Source -Ithird_party/daisysp/Source/Utility -Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-variable -Wno-unused-local-typedefs -fsanitize=address,undefined tests/morph_sets_test.cpp build/test-core-engine.o $(HOST_SUPPORT_OBJECTS) build/plaits/plaits/dsp/engine2/chiptune_engine.o build/plaits/plaits/dsp/engine2/virtual_analog_vcf_engine.o build/plaits/plaits/dsp/engine2/wave_terrain_engine.o build/plaits/plaits/dsp/engine2/six_op_engine.o build/plaits/plaits/dsp/engine2/phase_distortion_engine.o build/plaits/plaits/dsp/engine2/string_machine_engine.o build/plaits/plaits/dsp/speech/lpc_speech_synth.o build/plaits/plaits/dsp/speech/naive_speech_synth.o build/plaits/plaits/dsp/speech/sam_speech_synth.o build/plaits/plaits/dsp/speech/lpc_speech_synth_phonemes.o build/plaits/plaits/dsp/speech/lpc_speech_synth_words.o build/plaits/plaits/dsp/speech/lpc_speech_synth_controller.o build/plaits/plaits/dsp/engine/particle_engine.o build/plaits/plaits/dsp/engine/modal_engine.o build/plaits/plaits/dsp/engine/noise_engine.o build/plaits/plaits/dsp/engine/waveshaping_engine.o build/plaits/plaits/dsp/engine/string_engine.o build/plaits/plaits/dsp/engine/bass_drum_engine.o build/plaits/plaits/dsp/engine/swarm_engine.o build/plaits/plaits/dsp/engine/wavetable_engine.o build/plaits/plaits/dsp/engine/snare_drum_engine.o build/plaits/plaits/dsp/engine/chord_engine.o build/plaits/plaits/dsp/engine/fm_engine.o build/plaits/plaits/dsp/engine/grain_engine.o build/plaits/plaits/dsp/engine/additive_engine.o build/plaits/plaits/dsp/engine/hi_hat_engine.o build/plaits/plaits/dsp/engine/speech_engine.o build/plaits/plaits/dsp/engine/virtual_analog_engine.o build/plaits/plaits/dsp/voice.o build/plaits/plaits/dsp/fm/dx_units.o build/plaits/plaits/dsp/fm/algorithms.o build/plaits/plaits/dsp/chords/chord_bank.o build/plaits/plaits/dsp/physical_modelling/modal_voice.o build/plaits/plaits/dsp/physical_modelling/string_voice.o build/plaits/plaits/dsp/physical_modelling/resonator.o build/plaits/plaits/dsp/physical_modelling/string.o build/plaits/plaits/resources.o build/plaits/stmlib/dsp/units.o build/plaits/stmlib/utils/random.o $(RING_OBJECTS) -o build/test-morph-sets $(UI_BASE_ARCHIVE)
build/test-mute-layers: tests/mute_layers_test.cpp build/test-core-engine.o $(HOST_SUPPORT_OBJECTS) build/plaits/plaits/dsp/engine2/chiptune_engine.o build/plaits/plaits/dsp/engine2/virtual_analog_vcf_engine.o build/plaits/plaits/dsp/engine2/wave_terrain_engine.o build/plaits/plaits/dsp/engine2/six_op_engine.o build/plaits/plaits/dsp/engine2/phase_distortion_engine.o build/plaits/plaits/dsp/engine2/string_machine_engine.o build/plaits/plaits/dsp/speech/lpc_speech_synth.o build/plaits/plaits/dsp/speech/naive_speech_synth.o build/plaits/plaits/dsp/speech/sam_speech_synth.o build/plaits/plaits/dsp/speech/lpc_speech_synth_phonemes.o build/plaits/plaits/dsp/speech/lpc_speech_synth_words.o build/plaits/plaits/dsp/speech/lpc_speech_synth_controller.o build/plaits/plaits/dsp/engine/particle_engine.o build/plaits/plaits/dsp/engine/modal_engine.o build/plaits/plaits/dsp/engine/noise_engine.o build/plaits/plaits/dsp/engine/waveshaping_engine.o build/plaits/plaits/dsp/engine/string_engine.o build/plaits/plaits/dsp/engine/bass_drum_engine.o build/plaits/plaits/dsp/engine/swarm_engine.o build/plaits/plaits/dsp/engine/wavetable_engine.o build/plaits/plaits/dsp/engine/snare_drum_engine.o build/plaits/plaits/dsp/engine/chord_engine.o build/plaits/plaits/dsp/engine/fm_engine.o build/plaits/plaits/dsp/engine/grain_engine.o build/plaits/plaits/dsp/engine/additive_engine.o build/plaits/plaits/dsp/engine/hi_hat_engine.o build/plaits/plaits/dsp/engine/speech_engine.o build/plaits/plaits/dsp/engine/virtual_analog_engine.o build/plaits/plaits/dsp/voice.o build/plaits/plaits/dsp/fm/dx_units.o build/plaits/plaits/dsp/fm/algorithms.o build/plaits/plaits/dsp/chords/chord_bank.o build/plaits/plaits/dsp/physical_modelling/modal_voice.o build/plaits/plaits/dsp/physical_modelling/string_voice.o build/plaits/plaits/dsp/physical_modelling/resonator.o build/plaits/plaits/dsp/physical_modelling/string.o build/plaits/plaits/resources.o build/plaits/stmlib/dsp/units.o build/plaits/stmlib/utils/random.o $(RING_OBJECTS) $(UI_BASE_ARCHIVE)
	$(CXX) -std=c++17 -DGROOVE_PLAITS -DTEST -Isrc -Ithird_party/mutable -Ithird_party/daisysp/Source -Ithird_party/daisysp/Source/Utility -Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-variable -Wno-unused-local-typedefs -fsanitize=address,undefined tests/mute_layers_test.cpp build/test-core-engine.o $(HOST_SUPPORT_OBJECTS) build/plaits/plaits/dsp/engine2/chiptune_engine.o build/plaits/plaits/dsp/engine2/virtual_analog_vcf_engine.o build/plaits/plaits/dsp/engine2/wave_terrain_engine.o build/plaits/plaits/dsp/engine2/six_op_engine.o build/plaits/plaits/dsp/engine2/phase_distortion_engine.o build/plaits/plaits/dsp/engine2/string_machine_engine.o build/plaits/plaits/dsp/speech/lpc_speech_synth.o build/plaits/plaits/dsp/speech/naive_speech_synth.o build/plaits/plaits/dsp/speech/sam_speech_synth.o build/plaits/plaits/dsp/speech/lpc_speech_synth_phonemes.o build/plaits/plaits/dsp/speech/lpc_speech_synth_words.o build/plaits/plaits/dsp/speech/lpc_speech_synth_controller.o build/plaits/plaits/dsp/engine/particle_engine.o build/plaits/plaits/dsp/engine/modal_engine.o build/plaits/plaits/dsp/engine/noise_engine.o build/plaits/plaits/dsp/engine/waveshaping_engine.o build/plaits/plaits/dsp/engine/string_engine.o build/plaits/plaits/dsp/engine/bass_drum_engine.o build/plaits/plaits/dsp/engine/swarm_engine.o build/plaits/plaits/dsp/engine/wavetable_engine.o build/plaits/plaits/dsp/engine/snare_drum_engine.o build/plaits/plaits/dsp/engine/chord_engine.o build/plaits/plaits/dsp/engine/fm_engine.o build/plaits/plaits/dsp/engine/grain_engine.o build/plaits/plaits/dsp/engine/additive_engine.o build/plaits/plaits/dsp/engine/hi_hat_engine.o build/plaits/plaits/dsp/engine/speech_engine.o build/plaits/plaits/dsp/engine/virtual_analog_engine.o build/plaits/plaits/dsp/voice.o build/plaits/plaits/dsp/fm/dx_units.o build/plaits/plaits/dsp/fm/algorithms.o build/plaits/plaits/dsp/chords/chord_bank.o build/plaits/plaits/dsp/physical_modelling/modal_voice.o build/plaits/plaits/dsp/physical_modelling/string_voice.o build/plaits/plaits/dsp/physical_modelling/resonator.o build/plaits/plaits/dsp/physical_modelling/string.o build/plaits/plaits/resources.o build/plaits/stmlib/dsp/units.o build/plaits/stmlib/utils/random.o $(RING_OBJECTS) -o build/test-mute-layers $(UI_BASE_ARCHIVE)
build/test-capture: tests/capture_test.cpp build/test-core-engine.o $(HOST_SUPPORT_OBJECTS) build/plaits/plaits/dsp/engine2/chiptune_engine.o build/plaits/plaits/dsp/engine2/virtual_analog_vcf_engine.o build/plaits/plaits/dsp/engine2/wave_terrain_engine.o build/plaits/plaits/dsp/engine2/six_op_engine.o build/plaits/plaits/dsp/engine2/phase_distortion_engine.o build/plaits/plaits/dsp/engine2/string_machine_engine.o build/plaits/plaits/dsp/speech/lpc_speech_synth.o build/plaits/plaits/dsp/speech/naive_speech_synth.o build/plaits/plaits/dsp/speech/sam_speech_synth.o build/plaits/plaits/dsp/speech/lpc_speech_synth_phonemes.o build/plaits/plaits/dsp/speech/lpc_speech_synth_words.o build/plaits/plaits/dsp/speech/lpc_speech_synth_controller.o build/plaits/plaits/dsp/engine/particle_engine.o build/plaits/plaits/dsp/engine/modal_engine.o build/plaits/plaits/dsp/engine/noise_engine.o build/plaits/plaits/dsp/engine/waveshaping_engine.o build/plaits/plaits/dsp/engine/string_engine.o build/plaits/plaits/dsp/engine/bass_drum_engine.o build/plaits/plaits/dsp/engine/swarm_engine.o build/plaits/plaits/dsp/engine/wavetable_engine.o build/plaits/plaits/dsp/engine/snare_drum_engine.o build/plaits/plaits/dsp/engine/chord_engine.o build/plaits/plaits/dsp/engine/fm_engine.o build/plaits/plaits/dsp/engine/grain_engine.o build/plaits/plaits/dsp/engine/additive_engine.o build/plaits/plaits/dsp/engine/hi_hat_engine.o build/plaits/plaits/dsp/engine/speech_engine.o build/plaits/plaits/dsp/engine/virtual_analog_engine.o build/plaits/plaits/dsp/voice.o build/plaits/plaits/dsp/fm/dx_units.o build/plaits/plaits/dsp/fm/algorithms.o build/plaits/plaits/dsp/chords/chord_bank.o build/plaits/plaits/dsp/physical_modelling/modal_voice.o build/plaits/plaits/dsp/physical_modelling/string_voice.o build/plaits/plaits/dsp/physical_modelling/resonator.o build/plaits/plaits/dsp/physical_modelling/string.o build/plaits/plaits/resources.o build/plaits/stmlib/dsp/units.o build/plaits/stmlib/utils/random.o $(RING_OBJECTS) $(UI_BASE_ARCHIVE)
	$(CXX) -std=c++17 -DGROOVE_PLAITS -DTEST -Isrc -Ithird_party/mutable -Ithird_party/daisysp/Source -Ithird_party/daisysp/Source/Utility -Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-variable -Wno-unused-local-typedefs -fsanitize=address,undefined tests/capture_test.cpp build/test-core-engine.o $(HOST_SUPPORT_OBJECTS) build/plaits/plaits/dsp/engine2/chiptune_engine.o build/plaits/plaits/dsp/engine2/virtual_analog_vcf_engine.o build/plaits/plaits/dsp/engine2/wave_terrain_engine.o build/plaits/plaits/dsp/engine2/six_op_engine.o build/plaits/plaits/dsp/engine2/phase_distortion_engine.o build/plaits/plaits/dsp/engine2/string_machine_engine.o build/plaits/plaits/dsp/speech/lpc_speech_synth.o build/plaits/plaits/dsp/speech/naive_speech_synth.o build/plaits/plaits/dsp/speech/sam_speech_synth.o build/plaits/plaits/dsp/speech/lpc_speech_synth_phonemes.o build/plaits/plaits/dsp/speech/lpc_speech_synth_words.o build/plaits/plaits/dsp/speech/lpc_speech_synth_controller.o build/plaits/plaits/dsp/engine/particle_engine.o build/plaits/plaits/dsp/engine/modal_engine.o build/plaits/plaits/dsp/engine/noise_engine.o build/plaits/plaits/dsp/engine/waveshaping_engine.o build/plaits/plaits/dsp/engine/string_engine.o build/plaits/plaits/dsp/engine/bass_drum_engine.o build/plaits/plaits/dsp/engine/swarm_engine.o build/plaits/plaits/dsp/engine/wavetable_engine.o build/plaits/plaits/dsp/engine/snare_drum_engine.o build/plaits/plaits/dsp/engine/chord_engine.o build/plaits/plaits/dsp/engine/fm_engine.o build/plaits/plaits/dsp/engine/grain_engine.o build/plaits/plaits/dsp/engine/additive_engine.o build/plaits/plaits/dsp/engine/hi_hat_engine.o build/plaits/plaits/dsp/engine/speech_engine.o build/plaits/plaits/dsp/engine/virtual_analog_engine.o build/plaits/plaits/dsp/voice.o build/plaits/plaits/dsp/fm/dx_units.o build/plaits/plaits/dsp/fm/algorithms.o build/plaits/plaits/dsp/chords/chord_bank.o build/plaits/plaits/dsp/physical_modelling/modal_voice.o build/plaits/plaits/dsp/physical_modelling/string_voice.o build/plaits/plaits/dsp/physical_modelling/resonator.o build/plaits/plaits/dsp/physical_modelling/string.o build/plaits/plaits/resources.o build/plaits/stmlib/dsp/units.o build/plaits/stmlib/utils/random.o $(RING_OBJECTS) -o build/test-capture $(UI_BASE_ARCHIVE)
build/test-plaits-gate: tests/plaits_gate_test.cpp build/plaits/plaits/dsp/engine2/chiptune_engine.o build/plaits/plaits/dsp/engine2/virtual_analog_vcf_engine.o build/plaits/plaits/dsp/engine2/wave_terrain_engine.o build/plaits/plaits/dsp/engine2/six_op_engine.o build/plaits/plaits/dsp/engine2/phase_distortion_engine.o build/plaits/plaits/dsp/engine2/string_machine_engine.o build/plaits/plaits/dsp/speech/lpc_speech_synth.o build/plaits/plaits/dsp/speech/naive_speech_synth.o build/plaits/plaits/dsp/speech/sam_speech_synth.o build/plaits/plaits/dsp/speech/lpc_speech_synth_phonemes.o build/plaits/plaits/dsp/speech/lpc_speech_synth_words.o build/plaits/plaits/dsp/speech/lpc_speech_synth_controller.o build/plaits/plaits/dsp/engine/particle_engine.o build/plaits/plaits/dsp/engine/modal_engine.o build/plaits/plaits/dsp/engine/noise_engine.o build/plaits/plaits/dsp/engine/waveshaping_engine.o build/plaits/plaits/dsp/engine/string_engine.o build/plaits/plaits/dsp/engine/bass_drum_engine.o build/plaits/plaits/dsp/engine/swarm_engine.o build/plaits/plaits/dsp/engine/wavetable_engine.o build/plaits/plaits/dsp/engine/snare_drum_engine.o build/plaits/plaits/dsp/engine/chord_engine.o build/plaits/plaits/dsp/engine/fm_engine.o build/plaits/plaits/dsp/engine/grain_engine.o build/plaits/plaits/dsp/engine/additive_engine.o build/plaits/plaits/dsp/engine/hi_hat_engine.o build/plaits/plaits/dsp/engine/speech_engine.o build/plaits/plaits/dsp/engine/virtual_analog_engine.o build/plaits/plaits/dsp/voice.o build/plaits/plaits/dsp/fm/dx_units.o build/plaits/plaits/dsp/fm/algorithms.o build/plaits/plaits/dsp/chords/chord_bank.o build/plaits/plaits/dsp/physical_modelling/modal_voice.o build/plaits/plaits/dsp/physical_modelling/string_voice.o build/plaits/plaits/dsp/physical_modelling/resonator.o build/plaits/plaits/dsp/physical_modelling/string.o build/plaits/plaits/resources.o build/plaits/stmlib/dsp/units.o build/plaits/stmlib/utils/random.o $(RING_OBJECTS) $(UI_BASE_ARCHIVE)
	$(CXX) -std=c++17 -DGROOVE_PLAITS -DTEST -Isrc -Ithird_party/mutable -Ithird_party/daisysp/Source -Ithird_party/daisysp/Source/Utility -Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-variable -Wno-unused-local-typedefs -fsanitize=address,undefined tests/plaits_gate_test.cpp build/plaits/plaits/dsp/engine2/chiptune_engine.o build/plaits/plaits/dsp/engine2/virtual_analog_vcf_engine.o build/plaits/plaits/dsp/engine2/wave_terrain_engine.o build/plaits/plaits/dsp/engine2/six_op_engine.o build/plaits/plaits/dsp/engine2/phase_distortion_engine.o build/plaits/plaits/dsp/engine2/string_machine_engine.o build/plaits/plaits/dsp/speech/lpc_speech_synth.o build/plaits/plaits/dsp/speech/naive_speech_synth.o build/plaits/plaits/dsp/speech/sam_speech_synth.o build/plaits/plaits/dsp/speech/lpc_speech_synth_phonemes.o build/plaits/plaits/dsp/speech/lpc_speech_synth_words.o build/plaits/plaits/dsp/speech/lpc_speech_synth_controller.o build/plaits/plaits/dsp/engine/particle_engine.o build/plaits/plaits/dsp/engine/modal_engine.o build/plaits/plaits/dsp/engine/noise_engine.o build/plaits/plaits/dsp/engine/waveshaping_engine.o build/plaits/plaits/dsp/engine/string_engine.o build/plaits/plaits/dsp/engine/bass_drum_engine.o build/plaits/plaits/dsp/engine/swarm_engine.o build/plaits/plaits/dsp/engine/wavetable_engine.o build/plaits/plaits/dsp/engine/snare_drum_engine.o build/plaits/plaits/dsp/engine/chord_engine.o build/plaits/plaits/dsp/engine/fm_engine.o build/plaits/plaits/dsp/engine/grain_engine.o build/plaits/plaits/dsp/engine/additive_engine.o build/plaits/plaits/dsp/engine/hi_hat_engine.o build/plaits/plaits/dsp/engine/speech_engine.o build/plaits/plaits/dsp/engine/virtual_analog_engine.o build/plaits/plaits/dsp/voice.o build/plaits/plaits/dsp/fm/dx_units.o build/plaits/plaits/dsp/fm/algorithms.o build/plaits/plaits/dsp/chords/chord_bank.o build/plaits/plaits/dsp/physical_modelling/modal_voice.o build/plaits/plaits/dsp/physical_modelling/string_voice.o build/plaits/plaits/dsp/physical_modelling/resonator.o build/plaits/plaits/dsp/physical_modelling/string.o build/plaits/plaits/resources.o build/plaits/stmlib/dsp/units.o build/plaits/stmlib/utils/random.o $(RING_OBJECTS) -o build/test-plaits-gate $(UI_BASE_ARCHIVE)
build/test-partial-redraw: tests/partial_redraw_test.cpp build/test-core-engine.o $(HOST_SUPPORT_OBJECTS) build/plaits/plaits/dsp/engine2/chiptune_engine.o build/plaits/plaits/dsp/engine2/virtual_analog_vcf_engine.o build/plaits/plaits/dsp/engine2/wave_terrain_engine.o build/plaits/plaits/dsp/engine2/six_op_engine.o build/plaits/plaits/dsp/engine2/phase_distortion_engine.o build/plaits/plaits/dsp/engine2/string_machine_engine.o build/plaits/plaits/dsp/speech/lpc_speech_synth.o build/plaits/plaits/dsp/speech/naive_speech_synth.o build/plaits/plaits/dsp/speech/sam_speech_synth.o build/plaits/plaits/dsp/speech/lpc_speech_synth_phonemes.o build/plaits/plaits/dsp/speech/lpc_speech_synth_words.o build/plaits/plaits/dsp/speech/lpc_speech_synth_controller.o build/plaits/plaits/dsp/engine/particle_engine.o build/plaits/plaits/dsp/engine/modal_engine.o build/plaits/plaits/dsp/engine/noise_engine.o build/plaits/plaits/dsp/engine/waveshaping_engine.o build/plaits/plaits/dsp/engine/string_engine.o build/plaits/plaits/dsp/engine/bass_drum_engine.o build/plaits/plaits/dsp/engine/swarm_engine.o build/plaits/plaits/dsp/engine/wavetable_engine.o build/plaits/plaits/dsp/engine/snare_drum_engine.o build/plaits/plaits/dsp/engine/chord_engine.o build/plaits/plaits/dsp/engine/fm_engine.o build/plaits/plaits/dsp/engine/grain_engine.o build/plaits/plaits/dsp/engine/additive_engine.o build/plaits/plaits/dsp/engine/hi_hat_engine.o build/plaits/plaits/dsp/engine/speech_engine.o build/plaits/plaits/dsp/engine/virtual_analog_engine.o build/plaits/plaits/dsp/voice.o build/plaits/plaits/dsp/fm/dx_units.o build/plaits/plaits/dsp/fm/algorithms.o build/plaits/plaits/dsp/chords/chord_bank.o build/plaits/plaits/dsp/physical_modelling/modal_voice.o build/plaits/plaits/dsp/physical_modelling/string_voice.o build/plaits/plaits/dsp/physical_modelling/resonator.o build/plaits/plaits/dsp/physical_modelling/string.o build/plaits/plaits/resources.o build/plaits/stmlib/dsp/units.o build/plaits/stmlib/utils/random.o $(RING_OBJECTS) $(UI_BASE_ARCHIVE)
	$(CXX) -std=c++17 -DGROOVE_PLAITS -DTEST -Isrc -Ithird_party/mutable -Ithird_party/daisysp/Source -Ithird_party/daisysp/Source/Utility -Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-variable -Wno-unused-local-typedefs -fsanitize=address,undefined tests/partial_redraw_test.cpp build/test-core-engine.o $(HOST_SUPPORT_OBJECTS) build/plaits/plaits/dsp/engine2/chiptune_engine.o build/plaits/plaits/dsp/engine2/virtual_analog_vcf_engine.o build/plaits/plaits/dsp/engine2/wave_terrain_engine.o build/plaits/plaits/dsp/engine2/six_op_engine.o build/plaits/plaits/dsp/engine2/phase_distortion_engine.o build/plaits/plaits/dsp/engine2/string_machine_engine.o build/plaits/plaits/dsp/speech/lpc_speech_synth.o build/plaits/plaits/dsp/speech/naive_speech_synth.o build/plaits/plaits/dsp/speech/sam_speech_synth.o build/plaits/plaits/dsp/speech/lpc_speech_synth_phonemes.o build/plaits/plaits/dsp/speech/lpc_speech_synth_words.o build/plaits/plaits/dsp/speech/lpc_speech_synth_controller.o build/plaits/plaits/dsp/engine/particle_engine.o build/plaits/plaits/dsp/engine/modal_engine.o build/plaits/plaits/dsp/engine/noise_engine.o build/plaits/plaits/dsp/engine/waveshaping_engine.o build/plaits/plaits/dsp/engine/string_engine.o build/plaits/plaits/dsp/engine/bass_drum_engine.o build/plaits/plaits/dsp/engine/swarm_engine.o build/plaits/plaits/dsp/engine/wavetable_engine.o build/plaits/plaits/dsp/engine/snare_drum_engine.o build/plaits/plaits/dsp/engine/chord_engine.o build/plaits/plaits/dsp/engine/fm_engine.o build/plaits/plaits/dsp/engine/grain_engine.o build/plaits/plaits/dsp/engine/additive_engine.o build/plaits/plaits/dsp/engine/hi_hat_engine.o build/plaits/plaits/dsp/engine/speech_engine.o build/plaits/plaits/dsp/engine/virtual_analog_engine.o build/plaits/plaits/dsp/voice.o build/plaits/plaits/dsp/fm/dx_units.o build/plaits/plaits/dsp/fm/algorithms.o build/plaits/plaits/dsp/chords/chord_bank.o build/plaits/plaits/dsp/physical_modelling/modal_voice.o build/plaits/plaits/dsp/physical_modelling/string_voice.o build/plaits/plaits/dsp/physical_modelling/resonator.o build/plaits/plaits/dsp/physical_modelling/string.o build/plaits/plaits/resources.o build/plaits/stmlib/dsp/units.o build/plaits/stmlib/utils/random.o $(RING_OBJECTS) -o build/test-partial-redraw $(UI_BASE_ARCHIVE)
build/test-theme: tests/theme_test.cpp $(UI_BASE_ARCHIVE)
	$(CXX) -std=c++17 -DGROOVE_PLAITS -DTEST -Isrc -Ithird_party/mutable -Ithird_party/daisysp/Source -Ithird_party/daisysp/Source/Utility -Wall -Wextra -Werror -fsanitize=address,undefined tests/theme_test.cpp -o build/test-theme $(UI_BASE_ARCHIVE)
build/test-plock-hold: tests/plock_hold_test.cpp build/test-core-engine.o $(HOST_SUPPORT_OBJECTS) build/plaits/plaits/dsp/engine2/chiptune_engine.o build/plaits/plaits/dsp/engine2/virtual_analog_vcf_engine.o build/plaits/plaits/dsp/engine2/wave_terrain_engine.o build/plaits/plaits/dsp/engine2/six_op_engine.o build/plaits/plaits/dsp/engine2/phase_distortion_engine.o build/plaits/plaits/dsp/engine2/string_machine_engine.o build/plaits/plaits/dsp/speech/lpc_speech_synth.o build/plaits/plaits/dsp/speech/naive_speech_synth.o build/plaits/plaits/dsp/speech/sam_speech_synth.o build/plaits/plaits/dsp/speech/lpc_speech_synth_phonemes.o build/plaits/plaits/dsp/speech/lpc_speech_synth_words.o build/plaits/plaits/dsp/speech/lpc_speech_synth_controller.o build/plaits/plaits/dsp/engine/particle_engine.o build/plaits/plaits/dsp/engine/modal_engine.o build/plaits/plaits/dsp/engine/noise_engine.o build/plaits/plaits/dsp/engine/waveshaping_engine.o build/plaits/plaits/dsp/engine/string_engine.o build/plaits/plaits/dsp/engine/bass_drum_engine.o build/plaits/plaits/dsp/engine/swarm_engine.o build/plaits/plaits/dsp/engine/wavetable_engine.o build/plaits/plaits/dsp/engine/snare_drum_engine.o build/plaits/plaits/dsp/engine/chord_engine.o build/plaits/plaits/dsp/engine/fm_engine.o build/plaits/plaits/dsp/engine/grain_engine.o build/plaits/plaits/dsp/engine/additive_engine.o build/plaits/plaits/dsp/engine/hi_hat_engine.o build/plaits/plaits/dsp/engine/speech_engine.o build/plaits/plaits/dsp/engine/virtual_analog_engine.o build/plaits/plaits/dsp/voice.o build/plaits/plaits/dsp/fm/dx_units.o build/plaits/plaits/dsp/fm/algorithms.o build/plaits/plaits/dsp/chords/chord_bank.o build/plaits/plaits/dsp/physical_modelling/modal_voice.o build/plaits/plaits/dsp/physical_modelling/string_voice.o build/plaits/plaits/dsp/physical_modelling/resonator.o build/plaits/plaits/dsp/physical_modelling/string.o build/plaits/plaits/resources.o build/plaits/stmlib/dsp/units.o build/plaits/stmlib/utils/random.o $(RING_OBJECTS) $(UI_BASE_ARCHIVE)
	$(CXX) -std=c++17 -DGROOVE_PLAITS -DTEST -Isrc -Ithird_party/mutable -Ithird_party/daisysp/Source -Ithird_party/daisysp/Source/Utility -Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-variable -Wno-unused-local-typedefs -fsanitize=address,undefined tests/plock_hold_test.cpp build/test-core-engine.o $(HOST_SUPPORT_OBJECTS) build/plaits/plaits/dsp/engine2/chiptune_engine.o build/plaits/plaits/dsp/engine2/virtual_analog_vcf_engine.o build/plaits/plaits/dsp/engine2/wave_terrain_engine.o build/plaits/plaits/dsp/engine2/six_op_engine.o build/plaits/plaits/dsp/engine2/phase_distortion_engine.o build/plaits/plaits/dsp/engine2/string_machine_engine.o build/plaits/plaits/dsp/speech/lpc_speech_synth.o build/plaits/plaits/dsp/speech/naive_speech_synth.o build/plaits/plaits/dsp/speech/sam_speech_synth.o build/plaits/plaits/dsp/speech/lpc_speech_synth_phonemes.o build/plaits/plaits/dsp/speech/lpc_speech_synth_words.o build/plaits/plaits/dsp/speech/lpc_speech_synth_controller.o build/plaits/plaits/dsp/engine/particle_engine.o build/plaits/plaits/dsp/engine/modal_engine.o build/plaits/plaits/dsp/engine/noise_engine.o build/plaits/plaits/dsp/engine/waveshaping_engine.o build/plaits/plaits/dsp/engine/string_engine.o build/plaits/plaits/dsp/engine/bass_drum_engine.o build/plaits/plaits/dsp/engine/swarm_engine.o build/plaits/plaits/dsp/engine/wavetable_engine.o build/plaits/plaits/dsp/engine/snare_drum_engine.o build/plaits/plaits/dsp/engine/chord_engine.o build/plaits/plaits/dsp/engine/fm_engine.o build/plaits/plaits/dsp/engine/grain_engine.o build/plaits/plaits/dsp/engine/additive_engine.o build/plaits/plaits/dsp/engine/hi_hat_engine.o build/plaits/plaits/dsp/engine/speech_engine.o build/plaits/plaits/dsp/engine/virtual_analog_engine.o build/plaits/plaits/dsp/voice.o build/plaits/plaits/dsp/fm/dx_units.o build/plaits/plaits/dsp/fm/algorithms.o build/plaits/plaits/dsp/chords/chord_bank.o build/plaits/plaits/dsp/physical_modelling/modal_voice.o build/plaits/plaits/dsp/physical_modelling/string_voice.o build/plaits/plaits/dsp/physical_modelling/resonator.o build/plaits/plaits/dsp/physical_modelling/string.o build/plaits/plaits/resources.o build/plaits/stmlib/dsp/units.o build/plaits/stmlib/utils/random.o $(RING_OBJECTS) -o build/test-plock-hold $(UI_BASE_ARCHIVE)
build/test-engine-descriptor: tests/engine_descriptor_test.cpp build/test-core-engine.o $(HOST_SUPPORT_OBJECTS) build/plaits/plaits/dsp/engine2/chiptune_engine.o build/plaits/plaits/dsp/engine2/virtual_analog_vcf_engine.o build/plaits/plaits/dsp/engine2/wave_terrain_engine.o build/plaits/plaits/dsp/engine2/six_op_engine.o build/plaits/plaits/dsp/engine2/phase_distortion_engine.o build/plaits/plaits/dsp/engine2/string_machine_engine.o build/plaits/plaits/dsp/speech/lpc_speech_synth.o build/plaits/plaits/dsp/speech/naive_speech_synth.o build/plaits/plaits/dsp/speech/sam_speech_synth.o build/plaits/plaits/dsp/speech/lpc_speech_synth_phonemes.o build/plaits/plaits/dsp/speech/lpc_speech_synth_words.o build/plaits/plaits/dsp/speech/lpc_speech_synth_controller.o build/plaits/plaits/dsp/engine/particle_engine.o build/plaits/plaits/dsp/engine/modal_engine.o build/plaits/plaits/dsp/engine/noise_engine.o build/plaits/plaits/dsp/engine/waveshaping_engine.o build/plaits/plaits/dsp/engine/string_engine.o build/plaits/plaits/dsp/engine/bass_drum_engine.o build/plaits/plaits/dsp/engine/swarm_engine.o build/plaits/plaits/dsp/engine/wavetable_engine.o build/plaits/plaits/dsp/engine/snare_drum_engine.o build/plaits/plaits/dsp/engine/chord_engine.o build/plaits/plaits/dsp/engine/fm_engine.o build/plaits/plaits/dsp/engine/grain_engine.o build/plaits/plaits/dsp/engine/additive_engine.o build/plaits/plaits/dsp/engine/hi_hat_engine.o build/plaits/plaits/dsp/engine/speech_engine.o build/plaits/plaits/dsp/engine/virtual_analog_engine.o build/plaits/plaits/dsp/voice.o build/plaits/plaits/dsp/fm/dx_units.o build/plaits/plaits/dsp/fm/algorithms.o build/plaits/plaits/dsp/chords/chord_bank.o build/plaits/plaits/dsp/physical_modelling/modal_voice.o build/plaits/plaits/dsp/physical_modelling/string_voice.o build/plaits/plaits/dsp/physical_modelling/resonator.o build/plaits/plaits/dsp/physical_modelling/string.o build/plaits/plaits/resources.o build/plaits/stmlib/dsp/units.o build/plaits/stmlib/utils/random.o $(RING_OBJECTS) $(UI_BASE_ARCHIVE)
	$(CXX) -std=c++17 -DGROOVE_PLAITS -DTEST -Isrc -Ithird_party/mutable -Ithird_party/daisysp/Source -Ithird_party/daisysp/Source/Utility -Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-variable -Wno-unused-local-typedefs -fsanitize=address,undefined tests/engine_descriptor_test.cpp build/test-core-engine.o $(HOST_SUPPORT_OBJECTS) build/plaits/plaits/dsp/engine2/chiptune_engine.o build/plaits/plaits/dsp/engine2/virtual_analog_vcf_engine.o build/plaits/plaits/dsp/engine2/wave_terrain_engine.o build/plaits/plaits/dsp/engine2/six_op_engine.o build/plaits/plaits/dsp/engine2/phase_distortion_engine.o build/plaits/plaits/dsp/engine2/string_machine_engine.o build/plaits/plaits/dsp/speech/lpc_speech_synth.o build/plaits/plaits/dsp/speech/naive_speech_synth.o build/plaits/plaits/dsp/speech/sam_speech_synth.o build/plaits/plaits/dsp/speech/lpc_speech_synth_phonemes.o build/plaits/plaits/dsp/speech/lpc_speech_synth_words.o build/plaits/plaits/dsp/speech/lpc_speech_synth_controller.o build/plaits/plaits/dsp/engine/particle_engine.o build/plaits/plaits/dsp/engine/modal_engine.o build/plaits/plaits/dsp/engine/noise_engine.o build/plaits/plaits/dsp/engine/waveshaping_engine.o build/plaits/plaits/dsp/engine/string_engine.o build/plaits/plaits/dsp/engine/bass_drum_engine.o build/plaits/plaits/dsp/engine/swarm_engine.o build/plaits/plaits/dsp/engine/wavetable_engine.o build/plaits/plaits/dsp/engine/snare_drum_engine.o build/plaits/plaits/dsp/engine/chord_engine.o build/plaits/plaits/dsp/engine/fm_engine.o build/plaits/plaits/dsp/engine/grain_engine.o build/plaits/plaits/dsp/engine/additive_engine.o build/plaits/plaits/dsp/engine/hi_hat_engine.o build/plaits/plaits/dsp/engine/speech_engine.o build/plaits/plaits/dsp/engine/virtual_analog_engine.o build/plaits/plaits/dsp/voice.o build/plaits/plaits/dsp/fm/dx_units.o build/plaits/plaits/dsp/fm/algorithms.o build/plaits/plaits/dsp/chords/chord_bank.o build/plaits/plaits/dsp/physical_modelling/modal_voice.o build/plaits/plaits/dsp/physical_modelling/string_voice.o build/plaits/plaits/dsp/physical_modelling/resonator.o build/plaits/plaits/dsp/physical_modelling/string.o build/plaits/plaits/resources.o build/plaits/stmlib/dsp/units.o build/plaits/stmlib/utils/random.o $(RING_OBJECTS) -o build/test-engine-descriptor $(UI_BASE_ARCHIVE)
build/test-sample-cache-promotion: tests/sample_cache_promotion_test.cpp $(UI_BASE_ARCHIVE)
	$(CXX) -std=c++17 -Isrc -Wall -Wextra -Werror -fsanitize=address,undefined tests/sample_cache_promotion_test.cpp -o build/test-sample-cache-promotion $(UI_BASE_ARCHIVE)
build/test-callback-gap: tests/callback_gap_test.cpp $(UI_BASE_ARCHIVE)
	$(CXX) -std=c++17 -Isrc -Wall -Wextra -Werror -fsanitize=address,undefined tests/callback_gap_test.cpp -o build/test-callback-gap $(UI_BASE_ARCHIVE)
build/test-usb-implicit: tests/usb_implicit_test.cpp $(UI_BASE_ARCHIVE)
	$(CXX) -std=c++17 -DGROOVE_PLAITS -DTEST -Isrc -Ithird_party/mutable -Ithird_party/daisysp/Source -Ithird_party/daisysp/Source/Utility -Wall -Wextra -Werror -fsanitize=address,undefined tests/usb_implicit_test.cpp -o build/test-usb-implicit $(UI_BASE_ARCHIVE)
build/test-usb-pcm-convert: tests/usb_pcm_convert_test.cpp $(UI_BASE_ARCHIVE)
	$(CXX) -std=c++17 -DGROOVE_PLAITS -DTEST -Isrc -Ithird_party/mutable -Ithird_party/daisysp/Source -Ithird_party/daisysp/Source/Utility -Wall -Wextra -Werror -fsanitize=address,undefined tests/usb_pcm_convert_test.cpp -o build/test-usb-pcm-convert $(UI_BASE_ARCHIVE)
build/test-factory-wav: tests/factory_wav_test.cpp $(UI_BASE_ARCHIVE)
	$(CXX) -std=c++17 -DGROOVE_PLAITS -DTEST -Isrc -Ithird_party/mutable -Ithird_party/daisysp/Source -Ithird_party/daisysp/Source/Utility -Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-variable -Wno-unused-local-typedefs -fsanitize=address,undefined tests/factory_wav_test.cpp -o build/test-factory-wav $(UI_BASE_ARCHIVE)
build/test-pingpong-sampler: tests/pingpong_sampler_test.cpp $(UI_BASE_ARCHIVE)
	$(CXX) -std=c++17 -DGROOVE_PLAITS -DTEST -Isrc -Ithird_party/mutable -Ithird_party/daisysp/Source -Ithird_party/daisysp/Source/Utility -Wall -Wextra -Werror -fsanitize=address,undefined tests/pingpong_sampler_test.cpp -o build/test-pingpong-sampler $(UI_BASE_ARCHIVE)
build/test-sample-zones: tests/sample_zones_test.cpp $(UI_TOOL_ARCHIVE)
	g++ -std=c++17 -DGROOVE_PLAITS -DTEST -Isrc -Ithird_party/mutable -Ithird_party/daisysp/Source -Ithird_party/daisysp/Source/Utility -O2 -Wall -Wextra -Werror tests/sample_zones_test.cpp -o build/test-sample-zones $(UI_TOOL_ARCHIVE)
build/test-audio-render-slot: tests/audio_render_slot_test.cpp $(UI_BASE_ARCHIVE)
	$(CXX) -std=c++17 -Isrc -Wall -Wextra -Werror -fsanitize=address,undefined tests/audio_render_slot_test.cpp -o build/test-audio-render-slot $(UI_BASE_ARCHIVE)
build/test-audio-stage-profile: tests/audio_stage_profile_test.cpp $(UI_BASE_ARCHIVE)
	$(CXX) -std=c++17 -Isrc -Wall -Wextra -Werror -fsanitize=address,undefined tests/audio_stage_profile_test.cpp -o build/test-audio-stage-profile $(UI_BASE_ARCHIVE)
build/test-kit-voice-mix: tests/kit_voice_mix_test.cpp $(UI_BASE_ARCHIVE)
	$(CXX) -std=c++17 -Isrc -Wall -Wextra -Werror -fsanitize=address,undefined tests/kit_voice_mix_test.cpp -o build/test-kit-voice-mix $(UI_BASE_ARCHIVE)
build/test-performance-pitch: tests/performance_pitch_test.cpp $(UI_BASE_ARCHIVE)
	$(CXX) -std=c++17 -Isrc -Wall -Wextra -Werror -fsanitize=address,undefined tests/performance_pitch_test.cpp -o build/test-performance-pitch $(UI_BASE_ARCHIVE)
build/test-outline-raster: tests/outline_raster_test.cpp $(UI_BASE_ARCHIVE)
	$(CXX) -std=c++17 -Isrc -Wall -Wextra -Werror -fsanitize=address,undefined tests/outline_raster_test.cpp -o build/test-outline-raster $(UI_BASE_ARCHIVE)
build/test-cutoff-optimization: tests/cutoff_optimization_test.cpp $(UI_BASE_ARCHIVE)
	$(CXX) -std=c++17 -Isrc -Wall -Wextra -Werror -fsanitize=address,undefined tests/cutoff_optimization_test.cpp -o build/test-cutoff-optimization $(UI_BASE_ARCHIVE)
build/test-group-parameter: tests/group_parameter_test.cpp $(UI_BASE_ARCHIVE)
	$(CXX) -std=c++17 -DGROOVE_PLAITS -DTEST -Isrc -Ithird_party/mutable -Ithird_party/daisysp/Source -Ithird_party/daisysp/Source/Utility -Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-variable -Wno-unused-local-typedefs -fsanitize=address,undefined tests/group_parameter_test.cpp -o build/test-group-parameter $(UI_BASE_ARCHIVE)
build/test-pad-fx-parameters: tests/pad_fx_parameters_test.cpp $(UI_BASE_ARCHIVE)
	$(CXX) -std=c++17 -DGROOVE_PLAITS -DTEST -Isrc -Ithird_party/mutable -Ithird_party/daisysp/Source -Ithird_party/daisysp/Source/Utility -Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-variable -Wno-unused-local-typedefs -fsanitize=address,undefined tests/pad_fx_parameters_test.cpp -o build/test-pad-fx-parameters $(UI_BASE_ARCHIVE)
build/test-fx-stress-project: tests/fx_stress_project_test.cpp $(UI_BASE_ARCHIVE)
	$(CXX) -std=c++17 -DGROOVE_PLAITS -DTEST -Isrc -Ithird_party/mutable -Ithird_party/daisysp/Source -Ithird_party/daisysp/Source/Utility -Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-variable -Wno-unused-local-typedefs -fsanitize=address,undefined tests/fx_stress_project_test.cpp -o build/test-fx-stress-project $(UI_BASE_ARCHIVE)
build/test-group-insert: tests/group_insert_test.cpp build/test-core-engine.o $(HOST_SUPPORT_OBJECTS) build/plaits/plaits/dsp/engine2/chiptune_engine.o build/plaits/plaits/dsp/engine2/virtual_analog_vcf_engine.o build/plaits/plaits/dsp/engine2/wave_terrain_engine.o build/plaits/plaits/dsp/engine2/six_op_engine.o build/plaits/plaits/dsp/engine2/phase_distortion_engine.o build/plaits/plaits/dsp/engine2/string_machine_engine.o build/plaits/plaits/dsp/speech/lpc_speech_synth.o build/plaits/plaits/dsp/speech/naive_speech_synth.o build/plaits/plaits/dsp/speech/sam_speech_synth.o build/plaits/plaits/dsp/speech/lpc_speech_synth_phonemes.o build/plaits/plaits/dsp/speech/lpc_speech_synth_words.o build/plaits/plaits/dsp/speech/lpc_speech_synth_controller.o build/plaits/plaits/dsp/engine/particle_engine.o build/plaits/plaits/dsp/engine/modal_engine.o build/plaits/plaits/dsp/engine/noise_engine.o build/plaits/plaits/dsp/engine/waveshaping_engine.o build/plaits/plaits/dsp/engine/string_engine.o build/plaits/plaits/dsp/engine/bass_drum_engine.o build/plaits/plaits/dsp/engine/swarm_engine.o build/plaits/plaits/dsp/engine/wavetable_engine.o build/plaits/plaits/dsp/engine/snare_drum_engine.o build/plaits/plaits/dsp/engine/chord_engine.o build/plaits/plaits/dsp/engine/fm_engine.o build/plaits/plaits/dsp/engine/grain_engine.o build/plaits/plaits/dsp/engine/additive_engine.o build/plaits/plaits/dsp/engine/hi_hat_engine.o build/plaits/plaits/dsp/engine/speech_engine.o build/plaits/plaits/dsp/engine/virtual_analog_engine.o build/plaits/plaits/dsp/voice.o build/plaits/plaits/dsp/fm/dx_units.o build/plaits/plaits/dsp/fm/algorithms.o build/plaits/plaits/dsp/chords/chord_bank.o build/plaits/plaits/dsp/physical_modelling/modal_voice.o build/plaits/plaits/dsp/physical_modelling/string_voice.o build/plaits/plaits/dsp/physical_modelling/resonator.o build/plaits/plaits/dsp/physical_modelling/string.o build/plaits/plaits/resources.o build/plaits/stmlib/dsp/units.o build/plaits/stmlib/utils/random.o $(RING_OBJECTS) $(UI_BASE_ARCHIVE)
	$(CXX) -std=c++17 -DGROOVE_PLAITS -DTEST -Isrc -Ithird_party/mutable -Ithird_party/daisysp/Source -Ithird_party/daisysp/Source/Utility -O1 -Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-variable -Wno-unused-local-typedefs -fsanitize=address,undefined tests/group_insert_test.cpp build/test-core-engine.o $(HOST_SUPPORT_OBJECTS) build/plaits/plaits/dsp/engine2/chiptune_engine.o build/plaits/plaits/dsp/engine2/virtual_analog_vcf_engine.o build/plaits/plaits/dsp/engine2/wave_terrain_engine.o build/plaits/plaits/dsp/engine2/six_op_engine.o build/plaits/plaits/dsp/engine2/phase_distortion_engine.o build/plaits/plaits/dsp/engine2/string_machine_engine.o build/plaits/plaits/dsp/speech/lpc_speech_synth.o build/plaits/plaits/dsp/speech/naive_speech_synth.o build/plaits/plaits/dsp/speech/sam_speech_synth.o build/plaits/plaits/dsp/speech/lpc_speech_synth_phonemes.o build/plaits/plaits/dsp/speech/lpc_speech_synth_words.o build/plaits/plaits/dsp/speech/lpc_speech_synth_controller.o build/plaits/plaits/dsp/engine/particle_engine.o build/plaits/plaits/dsp/engine/modal_engine.o build/plaits/plaits/dsp/engine/noise_engine.o build/plaits/plaits/dsp/engine/waveshaping_engine.o build/plaits/plaits/dsp/engine/string_engine.o build/plaits/plaits/dsp/engine/bass_drum_engine.o build/plaits/plaits/dsp/engine/swarm_engine.o build/plaits/plaits/dsp/engine/wavetable_engine.o build/plaits/plaits/dsp/engine/snare_drum_engine.o build/plaits/plaits/dsp/engine/chord_engine.o build/plaits/plaits/dsp/engine/fm_engine.o build/plaits/plaits/dsp/engine/grain_engine.o build/plaits/plaits/dsp/engine/additive_engine.o build/plaits/plaits/dsp/engine/hi_hat_engine.o build/plaits/plaits/dsp/engine/speech_engine.o build/plaits/plaits/dsp/engine/virtual_analog_engine.o build/plaits/plaits/dsp/voice.o build/plaits/plaits/dsp/fm/dx_units.o build/plaits/plaits/dsp/fm/algorithms.o build/plaits/plaits/dsp/chords/chord_bank.o build/plaits/plaits/dsp/physical_modelling/modal_voice.o build/plaits/plaits/dsp/physical_modelling/string_voice.o build/plaits/plaits/dsp/physical_modelling/resonator.o build/plaits/plaits/dsp/physical_modelling/string.o build/plaits/plaits/resources.o build/plaits/stmlib/dsp/units.o build/plaits/stmlib/utils/random.o $(RING_OBJECTS) -o build/test-group-insert $(UI_BASE_ARCHIVE)
build/test-group-layer-engine: tests/group_layer_engine_test.cpp build/test-core-engine.o $(HOST_SUPPORT_OBJECTS) build/plaits/plaits/dsp/engine2/chiptune_engine.o build/plaits/plaits/dsp/engine2/virtual_analog_vcf_engine.o build/plaits/plaits/dsp/engine2/wave_terrain_engine.o build/plaits/plaits/dsp/engine2/six_op_engine.o build/plaits/plaits/dsp/engine2/phase_distortion_engine.o build/plaits/plaits/dsp/engine2/string_machine_engine.o build/plaits/plaits/dsp/speech/lpc_speech_synth.o build/plaits/plaits/dsp/speech/naive_speech_synth.o build/plaits/plaits/dsp/speech/sam_speech_synth.o build/plaits/plaits/dsp/speech/lpc_speech_synth_phonemes.o build/plaits/plaits/dsp/speech/lpc_speech_synth_words.o build/plaits/plaits/dsp/speech/lpc_speech_synth_controller.o build/plaits/plaits/dsp/engine/particle_engine.o build/plaits/plaits/dsp/engine/modal_engine.o build/plaits/plaits/dsp/engine/noise_engine.o build/plaits/plaits/dsp/engine/waveshaping_engine.o build/plaits/plaits/dsp/engine/string_engine.o build/plaits/plaits/dsp/engine/bass_drum_engine.o build/plaits/plaits/dsp/engine/swarm_engine.o build/plaits/plaits/dsp/engine/wavetable_engine.o build/plaits/plaits/dsp/engine/snare_drum_engine.o build/plaits/plaits/dsp/engine/chord_engine.o build/plaits/plaits/dsp/engine/fm_engine.o build/plaits/plaits/dsp/engine/grain_engine.o build/plaits/plaits/dsp/engine/additive_engine.o build/plaits/plaits/dsp/engine/hi_hat_engine.o build/plaits/plaits/dsp/engine/speech_engine.o build/plaits/plaits/dsp/engine/virtual_analog_engine.o build/plaits/plaits/dsp/voice.o build/plaits/plaits/dsp/fm/dx_units.o build/plaits/plaits/dsp/fm/algorithms.o build/plaits/plaits/dsp/chords/chord_bank.o build/plaits/plaits/dsp/physical_modelling/modal_voice.o build/plaits/plaits/dsp/physical_modelling/string_voice.o build/plaits/plaits/dsp/physical_modelling/resonator.o build/plaits/plaits/dsp/physical_modelling/string.o build/plaits/plaits/resources.o build/plaits/stmlib/dsp/units.o build/plaits/stmlib/utils/random.o $(RING_OBJECTS) $(UI_BASE_ARCHIVE)
	$(CXX) -std=c++17 -DGROOVE_PLAITS -DTEST -Isrc -Ithird_party/mutable -Ithird_party/daisysp/Source -Ithird_party/daisysp/Source/Utility -O1 -Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-variable -Wno-unused-local-typedefs -fsanitize=address,undefined tests/group_layer_engine_test.cpp build/test-core-engine.o $(HOST_SUPPORT_OBJECTS) build/plaits/plaits/dsp/engine2/chiptune_engine.o build/plaits/plaits/dsp/engine2/virtual_analog_vcf_engine.o build/plaits/plaits/dsp/engine2/wave_terrain_engine.o build/plaits/plaits/dsp/engine2/six_op_engine.o build/plaits/plaits/dsp/engine2/phase_distortion_engine.o build/plaits/plaits/dsp/engine2/string_machine_engine.o build/plaits/plaits/dsp/speech/lpc_speech_synth.o build/plaits/plaits/dsp/speech/naive_speech_synth.o build/plaits/plaits/dsp/speech/sam_speech_synth.o build/plaits/plaits/dsp/speech/lpc_speech_synth_phonemes.o build/plaits/plaits/dsp/speech/lpc_speech_synth_words.o build/plaits/plaits/dsp/speech/lpc_speech_synth_controller.o build/plaits/plaits/dsp/engine/particle_engine.o build/plaits/plaits/dsp/engine/modal_engine.o build/plaits/plaits/dsp/engine/noise_engine.o build/plaits/plaits/dsp/engine/waveshaping_engine.o build/plaits/plaits/dsp/engine/string_engine.o build/plaits/plaits/dsp/engine/bass_drum_engine.o build/plaits/plaits/dsp/engine/swarm_engine.o build/plaits/plaits/dsp/engine/wavetable_engine.o build/plaits/plaits/dsp/engine/snare_drum_engine.o build/plaits/plaits/dsp/engine/chord_engine.o build/plaits/plaits/dsp/engine/fm_engine.o build/plaits/plaits/dsp/engine/grain_engine.o build/plaits/plaits/dsp/engine/additive_engine.o build/plaits/plaits/dsp/engine/hi_hat_engine.o build/plaits/plaits/dsp/engine/speech_engine.o build/plaits/plaits/dsp/engine/virtual_analog_engine.o build/plaits/plaits/dsp/voice.o build/plaits/plaits/dsp/fm/dx_units.o build/plaits/plaits/dsp/fm/algorithms.o build/plaits/plaits/dsp/chords/chord_bank.o build/plaits/plaits/dsp/physical_modelling/modal_voice.o build/plaits/plaits/dsp/physical_modelling/string_voice.o build/plaits/plaits/dsp/physical_modelling/resonator.o build/plaits/plaits/dsp/physical_modelling/string.o build/plaits/plaits/resources.o build/plaits/stmlib/dsp/units.o build/plaits/stmlib/utils/random.o $(RING_OBJECTS) -o build/test-group-layer-engine $(UI_BASE_ARCHIVE)
build/test-group-parallel-engine: tests/group_parallel_engine_test.cpp build/test-core-engine.o $(HOST_SUPPORT_OBJECTS) build/plaits/plaits/dsp/engine2/chiptune_engine.o build/plaits/plaits/dsp/engine2/virtual_analog_vcf_engine.o build/plaits/plaits/dsp/engine2/wave_terrain_engine.o build/plaits/plaits/dsp/engine2/six_op_engine.o build/plaits/plaits/dsp/engine2/phase_distortion_engine.o build/plaits/plaits/dsp/engine2/string_machine_engine.o build/plaits/plaits/dsp/speech/lpc_speech_synth.o build/plaits/plaits/dsp/speech/naive_speech_synth.o build/plaits/plaits/dsp/speech/sam_speech_synth.o build/plaits/plaits/dsp/speech/lpc_speech_synth_phonemes.o build/plaits/plaits/dsp/speech/lpc_speech_synth_words.o build/plaits/plaits/dsp/speech/lpc_speech_synth_controller.o build/plaits/plaits/dsp/engine/particle_engine.o build/plaits/plaits/dsp/engine/modal_engine.o build/plaits/plaits/dsp/engine/noise_engine.o build/plaits/plaits/dsp/engine/waveshaping_engine.o build/plaits/plaits/dsp/engine/string_engine.o build/plaits/plaits/dsp/engine/bass_drum_engine.o build/plaits/plaits/dsp/engine/swarm_engine.o build/plaits/plaits/dsp/engine/wavetable_engine.o build/plaits/plaits/dsp/engine/snare_drum_engine.o build/plaits/plaits/dsp/engine/chord_engine.o build/plaits/plaits/dsp/engine/fm_engine.o build/plaits/plaits/dsp/engine/grain_engine.o build/plaits/plaits/dsp/engine/additive_engine.o build/plaits/plaits/dsp/engine/hi_hat_engine.o build/plaits/plaits/dsp/engine/speech_engine.o build/plaits/plaits/dsp/engine/virtual_analog_engine.o build/plaits/plaits/dsp/voice.o build/plaits/plaits/dsp/fm/dx_units.o build/plaits/plaits/dsp/fm/algorithms.o build/plaits/plaits/dsp/chords/chord_bank.o build/plaits/plaits/dsp/physical_modelling/modal_voice.o build/plaits/plaits/dsp/physical_modelling/string_voice.o build/plaits/plaits/dsp/physical_modelling/resonator.o build/plaits/plaits/dsp/physical_modelling/string.o build/plaits/plaits/resources.o build/plaits/stmlib/dsp/units.o build/plaits/stmlib/utils/random.o $(RING_OBJECTS) $(UI_BASE_ARCHIVE)
	$(CXX) -std=c++17 -DGROOVE_PLAITS -DTEST -Isrc -Ithird_party/mutable -Ithird_party/daisysp/Source -Ithird_party/daisysp/Source/Utility -O1 -Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-variable -Wno-unused-local-typedefs -fsanitize=address,undefined tests/group_parallel_engine_test.cpp build/test-core-engine.o $(HOST_SUPPORT_OBJECTS) build/plaits/plaits/dsp/engine2/chiptune_engine.o build/plaits/plaits/dsp/engine2/virtual_analog_vcf_engine.o build/plaits/plaits/dsp/engine2/wave_terrain_engine.o build/plaits/plaits/dsp/engine2/six_op_engine.o build/plaits/plaits/dsp/engine2/phase_distortion_engine.o build/plaits/plaits/dsp/engine2/string_machine_engine.o build/plaits/plaits/dsp/speech/lpc_speech_synth.o build/plaits/plaits/dsp/speech/naive_speech_synth.o build/plaits/plaits/dsp/speech/sam_speech_synth.o build/plaits/plaits/dsp/speech/lpc_speech_synth_phonemes.o build/plaits/plaits/dsp/speech/lpc_speech_synth_words.o build/plaits/plaits/dsp/speech/lpc_speech_synth_controller.o build/plaits/plaits/dsp/engine/particle_engine.o build/plaits/plaits/dsp/engine/modal_engine.o build/plaits/plaits/dsp/engine/noise_engine.o build/plaits/plaits/dsp/engine/waveshaping_engine.o build/plaits/plaits/dsp/engine/string_engine.o build/plaits/plaits/dsp/engine/bass_drum_engine.o build/plaits/plaits/dsp/engine/swarm_engine.o build/plaits/plaits/dsp/engine/wavetable_engine.o build/plaits/plaits/dsp/engine/snare_drum_engine.o build/plaits/plaits/dsp/engine/chord_engine.o build/plaits/plaits/dsp/engine/fm_engine.o build/plaits/plaits/dsp/engine/grain_engine.o build/plaits/plaits/dsp/engine/additive_engine.o build/plaits/plaits/dsp/engine/hi_hat_engine.o build/plaits/plaits/dsp/engine/speech_engine.o build/plaits/plaits/dsp/engine/virtual_analog_engine.o build/plaits/plaits/dsp/voice.o build/plaits/plaits/dsp/fm/dx_units.o build/plaits/plaits/dsp/fm/algorithms.o build/plaits/plaits/dsp/chords/chord_bank.o build/plaits/plaits/dsp/physical_modelling/modal_voice.o build/plaits/plaits/dsp/physical_modelling/string_voice.o build/plaits/plaits/dsp/physical_modelling/resonator.o build/plaits/plaits/dsp/physical_modelling/string.o build/plaits/plaits/resources.o build/plaits/stmlib/dsp/units.o build/plaits/stmlib/utils/random.o $(RING_OBJECTS) -o build/test-group-parallel-engine $(UI_BASE_ARCHIVE)
build/test-group-send-engine: tests/group_send_engine_test.cpp build/test-core-engine.o $(HOST_SUPPORT_OBJECTS) build/plaits/plaits/dsp/engine2/chiptune_engine.o build/plaits/plaits/dsp/engine2/virtual_analog_vcf_engine.o build/plaits/plaits/dsp/engine2/wave_terrain_engine.o build/plaits/plaits/dsp/engine2/six_op_engine.o build/plaits/plaits/dsp/engine2/phase_distortion_engine.o build/plaits/plaits/dsp/engine2/string_machine_engine.o build/plaits/plaits/dsp/speech/lpc_speech_synth.o build/plaits/plaits/dsp/speech/naive_speech_synth.o build/plaits/plaits/dsp/speech/sam_speech_synth.o build/plaits/plaits/dsp/speech/lpc_speech_synth_phonemes.o build/plaits/plaits/dsp/speech/lpc_speech_synth_words.o build/plaits/plaits/dsp/speech/lpc_speech_synth_controller.o build/plaits/plaits/dsp/engine/particle_engine.o build/plaits/plaits/dsp/engine/modal_engine.o build/plaits/plaits/dsp/engine/noise_engine.o build/plaits/plaits/dsp/engine/waveshaping_engine.o build/plaits/plaits/dsp/engine/string_engine.o build/plaits/plaits/dsp/engine/bass_drum_engine.o build/plaits/plaits/dsp/engine/swarm_engine.o build/plaits/plaits/dsp/engine/wavetable_engine.o build/plaits/plaits/dsp/engine/snare_drum_engine.o build/plaits/plaits/dsp/engine/chord_engine.o build/plaits/plaits/dsp/engine/fm_engine.o build/plaits/plaits/dsp/engine/grain_engine.o build/plaits/plaits/dsp/engine/additive_engine.o build/plaits/plaits/dsp/engine/hi_hat_engine.o build/plaits/plaits/dsp/engine/speech_engine.o build/plaits/plaits/dsp/engine/virtual_analog_engine.o build/plaits/plaits/dsp/voice.o build/plaits/plaits/dsp/fm/dx_units.o build/plaits/plaits/dsp/fm/algorithms.o build/plaits/plaits/dsp/chords/chord_bank.o build/plaits/plaits/dsp/physical_modelling/modal_voice.o build/plaits/plaits/dsp/physical_modelling/string_voice.o build/plaits/plaits/dsp/physical_modelling/resonator.o build/plaits/plaits/dsp/physical_modelling/string.o build/plaits/plaits/resources.o build/plaits/stmlib/dsp/units.o build/plaits/stmlib/utils/random.o $(RING_OBJECTS) $(UI_BASE_ARCHIVE)
	$(CXX) -std=c++17 -DGROOVE_PLAITS -DTEST -Isrc -Ithird_party/mutable -Ithird_party/daisysp/Source -Ithird_party/daisysp/Source/Utility -O1 -Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-variable -Wno-unused-local-typedefs -fsanitize=address,undefined tests/group_send_engine_test.cpp build/test-core-engine.o $(HOST_SUPPORT_OBJECTS) build/plaits/plaits/dsp/engine2/chiptune_engine.o build/plaits/plaits/dsp/engine2/virtual_analog_vcf_engine.o build/plaits/plaits/dsp/engine2/wave_terrain_engine.o build/plaits/plaits/dsp/engine2/six_op_engine.o build/plaits/plaits/dsp/engine2/phase_distortion_engine.o build/plaits/plaits/dsp/engine2/string_machine_engine.o build/plaits/plaits/dsp/speech/lpc_speech_synth.o build/plaits/plaits/dsp/speech/naive_speech_synth.o build/plaits/plaits/dsp/speech/sam_speech_synth.o build/plaits/plaits/dsp/speech/lpc_speech_synth_phonemes.o build/plaits/plaits/dsp/speech/lpc_speech_synth_words.o build/plaits/plaits/dsp/speech/lpc_speech_synth_controller.o build/plaits/plaits/dsp/engine/particle_engine.o build/plaits/plaits/dsp/engine/modal_engine.o build/plaits/plaits/dsp/engine/noise_engine.o build/plaits/plaits/dsp/engine/waveshaping_engine.o build/plaits/plaits/dsp/engine/string_engine.o build/plaits/plaits/dsp/engine/bass_drum_engine.o build/plaits/plaits/dsp/engine/swarm_engine.o build/plaits/plaits/dsp/engine/wavetable_engine.o build/plaits/plaits/dsp/engine/snare_drum_engine.o build/plaits/plaits/dsp/engine/chord_engine.o build/plaits/plaits/dsp/engine/fm_engine.o build/plaits/plaits/dsp/engine/grain_engine.o build/plaits/plaits/dsp/engine/additive_engine.o build/plaits/plaits/dsp/engine/hi_hat_engine.o build/plaits/plaits/dsp/engine/speech_engine.o build/plaits/plaits/dsp/engine/virtual_analog_engine.o build/plaits/plaits/dsp/voice.o build/plaits/plaits/dsp/fm/dx_units.o build/plaits/plaits/dsp/fm/algorithms.o build/plaits/plaits/dsp/chords/chord_bank.o build/plaits/plaits/dsp/physical_modelling/modal_voice.o build/plaits/plaits/dsp/physical_modelling/string_voice.o build/plaits/plaits/dsp/physical_modelling/resonator.o build/plaits/plaits/dsp/physical_modelling/string.o build/plaits/plaits/resources.o build/plaits/stmlib/dsp/units.o build/plaits/stmlib/utils/random.o $(RING_OBJECTS) -o build/test-group-send-engine $(UI_BASE_ARCHIVE)
.PHONY: test-run-integration
test-run-integration: build/test-integration
	@mkdir -p build/test-run/integration
	@cd build/test-run/integration && $(TEST_RUN_SETUP) && UBSAN_OPTIONS=halt_on_error=1 $(abspath build/test-integration)
.PHONY: test-run-backends
test-run-backends: build/test-backends
	@mkdir -p build/test-run/backends
	@cd build/test-run/backends && $(TEST_RUN_SETUP) && UBSAN_OPTIONS=halt_on_error=1 $(abspath build/test-backends)
.PHONY: test-run-chord
test-run-chord: build/test-chord
	@mkdir -p build/test-run/chord
	@cd build/test-run/chord && $(TEST_RUN_SETUP) && UBSAN_OPTIONS=halt_on_error=1 $(abspath build/test-chord)
.PHONY: test-run-update-buttons
test-run-update-buttons: build/test-update-buttons
	@mkdir -p build/test-run/update-buttons
	@cd build/test-run/update-buttons && $(TEST_RUN_SETUP) && $(abspath build/test-update-buttons)
.PHONY: test-run-py-integration
test-run-py-integration: build/update-sim
	UBSAN_OPTIONS=halt_on_error=1 python3 tests/update/integration.py
.PHONY: test-run-py-adapters
test-run-py-adapters: build/update-sim
	python3 tests/update/adapters.py
.PHONY: test-run-py-prepare_test
test-run-py-prepare_test: build/update-sim
	python3 tests/update/prepare_test.py
.PHONY: test-run-update-serial
test-run-update-serial: build/test-update-serial
	@mkdir -p build/test-run/update-serial
	@cd build/test-run/update-serial && $(TEST_RUN_SETUP) && UBSAN_OPTIONS=halt_on_error=1 $(abspath build/test-update-serial)
.PHONY: test-run-update-usb-control
test-run-update-usb-control: build/test-update-usb-control
	@mkdir -p build/test-run/update-usb-control
	@cd build/test-run/update-usb-control && $(TEST_RUN_SETUP) && UBSAN_OPTIONS=halt_on_error=1 $(abspath build/test-update-usb-control)
.PHONY: test-run-py-uart_host_test
test-run-py-uart_host_test: build/update-sim
	python3 tests/update/uart_host_test.py
.PHONY: test-run-py-benchmark_host_test
test-run-py-benchmark_host_test: build/update-sim
	python3 tests/update/benchmark_host_test.py
.PHONY: test-run-py-remote_flow_test
test-run-py-remote_flow_test: build/update-sim
	python3 tests/update/remote_flow_test.py
.PHONY: test-run-py-usb_remote_flow_test
test-run-py-usb_remote_flow_test: build/update-sim
	python3 tests/update/usb_remote_flow_test.py
.PHONY: test-run-py-usb_remote_pty_test
test-run-py-usb_remote_pty_test: build/update-sim
	python3 tests/update/usb_remote_pty_test.py
.PHONY: test-run-loopback-record
test-run-loopback-record: build/test-loopback-record
	@mkdir -p build/test-run/loopback-record
	@cd build/test-run/loopback-record && $(TEST_RUN_SETUP) && UBSAN_OPTIONS=halt_on_error=1 $(abspath build/test-loopback-record)
.PHONY: test-run-stop-effects
test-run-stop-effects: build/test-stop-effects
	@mkdir -p build/test-run/stop-effects
	@cd build/test-run/stop-effects && $(TEST_RUN_SETUP) && UBSAN_OPTIONS=halt_on_error=1 $(abspath build/test-stop-effects)
.PHONY: test-run-track-insert
test-run-track-insert: build/test-track-insert
	@mkdir -p build/test-run/track-insert
	@cd build/test-run/track-insert && $(TEST_RUN_SETUP) && UBSAN_OPTIONS=halt_on_error=1 $(abspath build/test-track-insert)
.PHONY: test-run-kit-track
test-run-kit-track: build/test-kit-track
	@mkdir -p build/test-run/kit-track
	@cd build/test-run/kit-track && $(TEST_RUN_SETUP) && UBSAN_OPTIONS=halt_on_error=1 $(abspath build/test-kit-track)
.PHONY: test-run-kit-record-workflow
test-run-kit-record-workflow: build/test-kit-record-workflow
	@mkdir -p build/test-run/kit-record-workflow
	@cd build/test-run/kit-record-workflow && $(TEST_RUN_SETUP) && UBSAN_OPTIONS=halt_on_error=1 $(abspath build/test-kit-record-workflow)
.PHONY: test-run-wav-pcm-formats
test-run-wav-pcm-formats: build/test-wav-pcm-formats
	@mkdir -p build/test-run/wav-pcm-formats
	@cd build/test-run/wav-pcm-formats && $(TEST_RUN_SETUP) && UBSAN_OPTIONS=halt_on_error=1 $(abspath build/test-wav-pcm-formats)
.PHONY: test-run-sampler-edge-time
test-run-sampler-edge-time: build/test-sampler-edge-time
	@mkdir -p build/test-run/sampler-edge-time
	@cd build/test-run/sampler-edge-time && $(TEST_RUN_SETUP) && UBSAN_OPTIONS=halt_on_error=1 $(abspath build/test-sampler-edge-time)
.PHONY: test-run-public-overdrive
test-run-public-overdrive: build/test-public-overdrive
	@mkdir -p build/test-run/public-overdrive
	@cd build/test-run/public-overdrive && $(TEST_RUN_SETUP) && UBSAN_OPTIONS=halt_on_error=1 $(abspath build/test-public-overdrive)
.PHONY: test-run-public-fm2
test-run-public-fm2: build/test-public-fm2
	@mkdir -p build/test-run/public-fm2
	@cd build/test-run/public-fm2 && $(TEST_RUN_SETUP) && UBSAN_OPTIONS=halt_on_error=1 $(abspath build/test-public-fm2)
.PHONY: test-run-public-wavefolder
test-run-public-wavefolder: build/test-public-wavefolder
	@mkdir -p build/test-run/public-wavefolder
	@cd build/test-run/public-wavefolder && $(TEST_RUN_SETUP) && UBSAN_OPTIONS=halt_on_error=1 $(abspath build/test-public-wavefolder)
.PHONY: test-run-daisy-demo-project
test-run-daisy-demo-project: build/test-daisy-demo-project
	@mkdir -p build/test-run/daisy-demo-project
	@cd build/test-run/daisy-demo-project && $(TEST_RUN_SETUP) && UBSAN_OPTIONS=halt_on_error=1 $(abspath build/test-daisy-demo-project)
.PHONY: test-run-public-drums
test-run-public-drums: build/test-public-drums
	@mkdir -p build/test-run/public-drums
	@cd build/test-run/public-drums && $(TEST_RUN_SETUP) && UBSAN_OPTIONS=halt_on_error=1 $(abspath build/test-public-drums)
.PHONY: test-run-daisy-engine
test-run-daisy-engine: build/test-daisy-engine
	@mkdir -p build/test-run/daisy-engine
	@cd build/test-run/daisy-engine && $(TEST_RUN_SETUP) && UBSAN_OPTIONS=halt_on_error=1 $(abspath build/test-daisy-engine)
.PHONY: test-run-benchmark-requests
test-run-benchmark-requests: build/test-benchmark-requests
	@mkdir -p build/test-run/benchmark-requests
	@cd build/test-run/benchmark-requests && $(TEST_RUN_SETUP) && UBSAN_OPTIONS=halt_on_error=1 $(abspath build/test-benchmark-requests)
.PHONY: test-run-plaits-stress-project
test-run-plaits-stress-project: build/test-plaits-stress-project
	@mkdir -p build/test-run/plaits-stress-project
	@cd build/test-run/plaits-stress-project && $(TEST_RUN_SETUP) && UBSAN_OPTIONS=halt_on_error=1 $(abspath build/test-plaits-stress-project)
.PHONY: test-run-benchmark-project
test-run-benchmark-project: build/test-benchmark-project
	@mkdir -p build/test-run/benchmark-project
	@cd build/test-run/benchmark-project && $(TEST_RUN_SETUP) && UBSAN_OPTIONS=halt_on_error=1 $(abspath build/test-benchmark-project)
.PHONY: test-run-clean-save
test-run-clean-save: build/test-clean-save
	@mkdir -p build/test-run/clean-save
	@cd build/test-run/clean-save && $(TEST_RUN_SETUP) && UBSAN_OPTIONS=halt_on_error=1 $(abspath build/test-clean-save)
.PHONY: test-run-benchmark-cleanup
test-run-benchmark-cleanup: build/test-benchmark-cleanup
	@mkdir -p build/test-run/benchmark-cleanup
	@cd build/test-run/benchmark-cleanup && $(TEST_RUN_SETUP) && UBSAN_OPTIONS=halt_on_error=1 $(abspath build/test-benchmark-cleanup)
.PHONY: test-run-group-routing-cache
test-run-group-routing-cache: build/test-group-routing-cache
	@mkdir -p build/test-run/group-routing-cache
	@cd build/test-run/group-routing-cache && $(TEST_RUN_SETUP) && UBSAN_OPTIONS=halt_on_error=1 $(abspath build/test-group-routing-cache)
.PHONY: test-run-display-rectangle
test-run-display-rectangle: build/test-display-rectangle
	@mkdir -p build/test-run/display-rectangle
	@cd build/test-run/display-rectangle && $(TEST_RUN_SETUP) && UBSAN_OPTIONS=halt_on_error=1 $(abspath build/test-display-rectangle)
.PHONY: test-run-display-job-slot
test-run-display-job-slot: build/test-display-job-slot
	@mkdir -p build/test-run/display-job-slot
	@cd build/test-run/display-job-slot && $(TEST_RUN_SETUP) && UBSAN_OPTIONS=halt_on_error=1 $(abspath build/test-display-job-slot)
.PHONY: test-run-visual-snapshot
test-run-visual-snapshot: build/test-visual-snapshot
	@mkdir -p build/test-run/visual-snapshot
	@cd build/test-run/visual-snapshot && $(TEST_RUN_SETUP) && UBSAN_OPTIONS=halt_on_error=1 $(abspath build/test-visual-snapshot)
.PHONY: test-run-timing-boundary
test-run-timing-boundary: build/test-timing-boundary
	@mkdir -p build/test-run/timing-boundary
	@cd build/test-run/timing-boundary && $(TEST_RUN_SETUP) && UBSAN_OPTIONS=halt_on_error=1 $(abspath build/test-timing-boundary)
.PHONY: test-run-test
test-run-test: build/test
	@mkdir -p build/test-run/test
	@cd build/test-run/test && $(TEST_RUN_SETUP) && UBSAN_OPTIONS=halt_on_error=1 $(abspath build/test)
.PHONY: test-run-plaits
test-run-plaits: build/test-plaits
	@mkdir -p build/test-run/plaits
	@cd build/test-run/plaits && $(TEST_RUN_SETUP) && UBSAN_OPTIONS=halt_on_error=1 $(abspath build/test-plaits)
.PHONY: test-run-project-codec
test-run-project-codec: build/test-project-codec
	@mkdir -p build/test-run/project-codec
	@cd build/test-run/project-codec && $(TEST_RUN_SETUP) && UBSAN_OPTIONS=halt_on_error=1 $(abspath build/test-project-codec)
.PHONY: test-run-page-descriptors
test-run-page-descriptors: build/test-page-descriptors
	@mkdir -p build/test-run/page-descriptors
	@cd build/test-run/page-descriptors && $(TEST_RUN_SETUP) && UBSAN_OPTIONS=halt_on_error=1 $(abspath build/test-page-descriptors)
.PHONY: test-run-page-nav
test-run-page-nav: build/test-page-nav
	@mkdir -p build/test-run/page-nav
	@cd build/test-run/page-nav && $(TEST_RUN_SETUP) && UBSAN_OPTIONS=halt_on_error=1 $(abspath build/test-page-nav)
.PHONY: test-run-source-select
test-run-source-select: build/test-source-select
	@mkdir -p build/test-run/source-select
	@cd build/test-run/source-select && $(TEST_RUN_SETUP) && UBSAN_OPTIONS=halt_on_error=1 $(abspath build/test-source-select)
.PHONY: test-run-engine-pages
test-run-engine-pages: build/test-engine-pages
	@mkdir -p build/test-run/engine-pages
	@cd build/test-run/engine-pages && $(TEST_RUN_SETUP) && UBSAN_OPTIONS=halt_on_error=1 $(abspath build/test-engine-pages)
.PHONY: test-run-fx-chain
test-run-fx-chain: build/test-fx-chain
	@mkdir -p build/test-run/fx-chain
	@cd build/test-run/fx-chain && $(TEST_RUN_SETUP) && UBSAN_OPTIONS=halt_on_error=1 $(abspath build/test-fx-chain)
.PHONY: test-run-browser-open-stress
test-run-browser-open-stress: build/test-browser-open-stress
	@mkdir -p build/test-run/browser-open-stress
	@cd build/test-run/browser-open-stress && $(TEST_RUN_SETUP) && UBSAN_OPTIONS=halt_on_error=1 $(abspath build/test-browser-open-stress)
.PHONY: test-run-buttons
test-run-buttons: build/test-buttons
	@mkdir -p build/test-run/buttons
	@cd build/test-run/buttons && $(TEST_RUN_SETUP) && UBSAN_OPTIONS=halt_on_error=1 $(abspath build/test-buttons)
.PHONY: test-run-duplicate
test-run-duplicate: build/test-duplicate
	@mkdir -p build/test-run/duplicate
	@cd build/test-run/duplicate && $(TEST_RUN_SETUP) && UBSAN_OPTIONS=halt_on_error=1 $(abspath build/test-duplicate)
.PHONY: test-run-daisy-adsr
test-run-daisy-adsr: build/test-daisy-adsr
	@mkdir -p build/test-run/daisy-adsr
	@cd build/test-run/daisy-adsr && $(TEST_RUN_SETUP) && UBSAN_OPTIONS=halt_on_error=1 $(abspath build/test-daisy-adsr)
.PHONY: test-run-transition
test-run-transition: build/test-transition
	@mkdir -p build/test-run/transition
	@cd build/test-run/transition && $(TEST_RUN_SETUP) && UBSAN_OPTIONS=halt_on_error=1 $(abspath build/test-transition)
.PHONY: test-run-lab
test-run-lab: build/test-lab
	@mkdir -p build/test-run/lab
	@cd build/test-run/lab && $(TEST_RUN_SETUP) && UBSAN_OPTIONS=halt_on_error=1 $(abspath build/test-lab)
.PHONY: test-run-song
test-run-song: build/test-song
	@mkdir -p build/test-run/song
	@cd build/test-run/song && $(TEST_RUN_SETUP) && UBSAN_OPTIONS=halt_on_error=1 $(abspath build/test-song)
.PHONY: test-run-song-page
test-run-song-page: build/test-song-page
	@mkdir -p build/test-run/song-page
	@cd build/test-run/song-page && $(TEST_RUN_SETUP) && UBSAN_OPTIONS=halt_on_error=1 $(abspath build/test-song-page)
.PHONY: test-run-wave-zones
test-run-wave-zones: build/test-wave-zones
	@mkdir -p build/test-run/wave-zones
	@cd build/test-run/wave-zones && $(TEST_RUN_SETUP) && UBSAN_OPTIONS=halt_on_error=1 $(abspath build/test-wave-zones)
.PHONY: test-run-stretch
test-run-stretch: build/test-stretch
	@mkdir -p build/test-run/stretch
	@cd build/test-run/stretch && $(TEST_RUN_SETUP) && UBSAN_OPTIONS=halt_on_error=1 $(abspath build/test-stretch)
.PHONY: test-run-stretch-job
test-run-stretch-job: build/test-stretch-job
	@mkdir -p build/test-run/stretch-job
	@cd build/test-run/stretch-job && $(TEST_RUN_SETUP) && UBSAN_OPTIONS=halt_on_error=1 $(abspath build/test-stretch-job)
.PHONY: test-run-warp
test-run-warp: build/test-warp
	@mkdir -p build/test-run/warp
	@cd build/test-run/warp && $(TEST_RUN_SETUP) && UBSAN_OPTIONS=halt_on_error=1 $(abspath build/test-warp)
.PHONY: test-run-stretch-playback
test-run-stretch-playback: build/test-stretch-playback
	@mkdir -p build/test-run/stretch-playback
	@cd build/test-run/stretch-playback && $(TEST_RUN_SETUP) && UBSAN_OPTIONS=halt_on_error=1 $(abspath build/test-stretch-playback)
.PHONY: test-run-mod-matrix
test-run-mod-matrix: build/test-mod-matrix
	@mkdir -p build/test-run/mod-matrix
	@cd build/test-run/mod-matrix && $(TEST_RUN_SETUP) && UBSAN_OPTIONS=halt_on_error=1 $(abspath build/test-mod-matrix)
test-run-fx-type-lock: build/test-fx-type-lock
	@mkdir -p build/test-run/fx-type-lock && cd build/test-run/fx-type-lock && UBSAN_OPTIONS=halt_on_error=1 ../../../build/test-fx-type-lock
test-run-freeze: build/test-freeze
	@mkdir -p build/test-run/freeze && cd build/test-run/freeze && UBSAN_OPTIONS=halt_on_error=1 ../../../build/test-freeze
.PHONY: test-run-sound-state
test-run-sound-state: build/test-sound-state
	@mkdir -p build/test-run/sound-state
	@cd build/test-run/sound-state && $(TEST_RUN_SETUP) && UBSAN_OPTIONS=halt_on_error=1 $(abspath build/test-sound-state)
.PHONY: test-run-reload
test-run-reload: build/test-reload
	@mkdir -p build/test-run/reload
	@cd build/test-run/reload && $(TEST_RUN_SETUP) && UBSAN_OPTIONS=halt_on_error=1 $(abspath build/test-reload)
.PHONY: test-run-morph-sets
test-run-morph-sets: build/test-morph-sets
	@mkdir -p build/test-run/morph-sets
	@cd build/test-run/morph-sets && $(TEST_RUN_SETUP) && UBSAN_OPTIONS=halt_on_error=1 $(abspath build/test-morph-sets)
.PHONY: test-run-mute-layers
test-run-mute-layers: build/test-mute-layers
	@mkdir -p build/test-run/mute-layers
	@cd build/test-run/mute-layers && $(TEST_RUN_SETUP) && UBSAN_OPTIONS=halt_on_error=1 $(abspath build/test-mute-layers)
.PHONY: test-run-capture
test-run-capture: build/test-capture
	@mkdir -p build/test-run/capture
	@cd build/test-run/capture && $(TEST_RUN_SETUP) && UBSAN_OPTIONS=halt_on_error=1 $(abspath build/test-capture)
.PHONY: test-run-plaits-gate
test-run-plaits-gate: build/test-plaits-gate
	@mkdir -p build/test-run/plaits-gate
	@cd build/test-run/plaits-gate && $(TEST_RUN_SETUP) && UBSAN_OPTIONS=halt_on_error=1 $(abspath build/test-plaits-gate)
.PHONY: test-run-partial-redraw
test-run-partial-redraw: build/test-partial-redraw
	@mkdir -p build/test-run/partial-redraw
	@cd build/test-run/partial-redraw && $(TEST_RUN_SETUP) && UBSAN_OPTIONS=halt_on_error=1 $(abspath build/test-partial-redraw)
.PHONY: test-run-theme
test-run-theme: build/test-theme
	python3 tools/ui/themes.py --check
	@mkdir -p build/test-run/theme
	@cd build/test-run/theme && $(TEST_RUN_SETUP) && UBSAN_OPTIONS=halt_on_error=1 $(abspath build/test-theme)
.PHONY: test-run-plock-hold
test-run-plock-hold: build/test-plock-hold
	@mkdir -p build/test-run/plock-hold
	@cd build/test-run/plock-hold && $(TEST_RUN_SETUP) && UBSAN_OPTIONS=halt_on_error=1 $(abspath build/test-plock-hold)
.PHONY: test-run-engine-descriptor
test-run-engine-descriptor: build/test-engine-descriptor
	@mkdir -p build/test-run/engine-descriptor
	@cd build/test-run/engine-descriptor && $(TEST_RUN_SETUP) && UBSAN_OPTIONS=halt_on_error=1 $(abspath build/test-engine-descriptor)
.PHONY: test-run-sample-cache-promotion
test-run-sample-cache-promotion: build/test-sample-cache-promotion
	@mkdir -p build/test-run/sample-cache-promotion
	@cd build/test-run/sample-cache-promotion && $(TEST_RUN_SETUP) && UBSAN_OPTIONS=halt_on_error=1 $(abspath build/test-sample-cache-promotion)
.PHONY: test-run-callback-gap
test-run-callback-gap: build/test-callback-gap
	@mkdir -p build/test-run/callback-gap
	@cd build/test-run/callback-gap && $(TEST_RUN_SETUP) && UBSAN_OPTIONS=halt_on_error=1 $(abspath build/test-callback-gap)
.PHONY: test-run-usb-implicit
test-run-usb-implicit: build/test-usb-implicit
	@mkdir -p build/test-run/usb-implicit
	@cd build/test-run/usb-implicit && $(TEST_RUN_SETUP) && UBSAN_OPTIONS=halt_on_error=1 $(abspath build/test-usb-implicit)
.PHONY: test-run-usb-pcm-convert
test-run-usb-pcm-convert: build/test-usb-pcm-convert
	@mkdir -p build/test-run/usb-pcm-convert
	@cd build/test-run/usb-pcm-convert && $(TEST_RUN_SETUP) && UBSAN_OPTIONS=halt_on_error=1 $(abspath build/test-usb-pcm-convert)
.PHONY: test-run-py-kernel_bounds_test
test-run-py-kernel_bounds_test: build/update-sim
	python3 tests/kernel_bounds_test.py
.PHONY: test-run-py-circle_hook_test
test-run-py-circle_hook_test: build/update-sim
	python3 tests/circle_hook_test.py
.PHONY: test-run-py-mk3_observe_test
test-run-py-mk3_observe_test: build/update-sim
	python3 tests/mk3_observe_test.py
.PHONY: test-run-py-boot_seed_test
test-run-py-boot_seed_test: build/update-sim
	python3 tests/boot_seed_test.py
.PHONY: test-run-py-update_identity_test
test-run-py-update_identity_test: build/update-sim
	python3 tests/update_identity_test.py
.PHONY: test-run-py-circle_dhcp_patch_test
test-run-py-circle_dhcp_patch_test: build/update-sim
	python3 tests/circle_dhcp_patch_test.py
.PHONY: test-run-py-hardware_capture_retry_test
test-run-py-hardware_capture_retry_test: build/update-sim
	python3 tests/hardware_capture_retry_test.py
.PHONY: test-run-factory-wav
test-run-factory-wav: build/test-factory-wav
	@mkdir -p build/test-run/factory-wav
	@cd build/test-run/factory-wav && $(TEST_RUN_SETUP) && UBSAN_OPTIONS=halt_on_error=1 $(abspath build/test-factory-wav)
.PHONY: test-run-pingpong-sampler
test-run-pingpong-sampler: build/test-pingpong-sampler
	@mkdir -p build/test-run/pingpong-sampler
	@cd build/test-run/pingpong-sampler && $(TEST_RUN_SETUP) && UBSAN_OPTIONS=halt_on_error=1 $(abspath build/test-pingpong-sampler)
.PHONY: test-run-sample-zones
test-run-sample-zones: build/test-sample-zones
	@mkdir -p build/test-run/sample-zones
	@cd build/test-run/sample-zones && $(TEST_RUN_SETUP) && UBSAN_OPTIONS=halt_on_error=1 $(abspath build/test-sample-zones)
.PHONY: test-run-audio-render-slot
test-run-audio-render-slot: build/test-audio-render-slot
	@mkdir -p build/test-run/audio-render-slot
	@cd build/test-run/audio-render-slot && $(TEST_RUN_SETUP) && UBSAN_OPTIONS=halt_on_error=1 $(abspath build/test-audio-render-slot)
.PHONY: test-run-audio-stage-profile
test-run-audio-stage-profile: build/test-audio-stage-profile
	@mkdir -p build/test-run/audio-stage-profile
	@cd build/test-run/audio-stage-profile && $(TEST_RUN_SETUP) && UBSAN_OPTIONS=halt_on_error=1 $(abspath build/test-audio-stage-profile)
.PHONY: test-run-kit-voice-mix
test-run-kit-voice-mix: build/test-kit-voice-mix
	@mkdir -p build/test-run/kit-voice-mix
	@cd build/test-run/kit-voice-mix && $(TEST_RUN_SETUP) && UBSAN_OPTIONS=halt_on_error=1 $(abspath build/test-kit-voice-mix)
.PHONY: test-run-performance-pitch
test-run-performance-pitch: build/test-performance-pitch
	@mkdir -p build/test-run/performance-pitch
	@cd build/test-run/performance-pitch && $(TEST_RUN_SETUP) && UBSAN_OPTIONS=halt_on_error=1 $(abspath build/test-performance-pitch)
.PHONY: test-run-outline-raster
test-run-outline-raster: build/test-outline-raster
	@mkdir -p build/test-run/outline-raster
	@cd build/test-run/outline-raster && $(TEST_RUN_SETUP) && UBSAN_OPTIONS=halt_on_error=1 $(abspath build/test-outline-raster)
.PHONY: test-run-cutoff-optimization
test-run-cutoff-optimization: build/test-cutoff-optimization
	@mkdir -p build/test-run/cutoff-optimization
	@cd build/test-run/cutoff-optimization && $(TEST_RUN_SETUP) && UBSAN_OPTIONS=halt_on_error=1 $(abspath build/test-cutoff-optimization)
.PHONY: test-run-group-parameter
test-run-group-parameter: build/test-group-parameter
	@mkdir -p build/test-run/group-parameter
	@cd build/test-run/group-parameter && $(TEST_RUN_SETUP) && UBSAN_OPTIONS=halt_on_error=1 $(abspath build/test-group-parameter)
.PHONY: test-run-pad-fx-parameters
test-run-pad-fx-parameters: build/test-pad-fx-parameters
	@mkdir -p build/test-run/pad-fx-parameters
	@cd build/test-run/pad-fx-parameters && $(TEST_RUN_SETUP) && UBSAN_OPTIONS=halt_on_error=1 $(abspath build/test-pad-fx-parameters)
.PHONY: test-run-fx-stress-project
test-run-fx-stress-project: build/test-fx-stress-project
	@mkdir -p build/test-run/fx-stress-project
	@cd build/test-run/fx-stress-project && $(TEST_RUN_SETUP) && UBSAN_OPTIONS=halt_on_error=1 $(abspath build/test-fx-stress-project)
.PHONY: test-run-group-insert
test-run-group-insert: build/test-group-insert
	@mkdir -p build/test-run/group-insert
	@cd build/test-run/group-insert && $(TEST_RUN_SETUP) && UBSAN_OPTIONS=halt_on_error=1 $(abspath build/test-group-insert)
.PHONY: test-run-group-layer-engine
test-run-group-layer-engine: build/test-group-layer-engine
	@mkdir -p build/test-run/group-layer-engine
	@cd build/test-run/group-layer-engine && $(TEST_RUN_SETUP) && UBSAN_OPTIONS=halt_on_error=1 $(abspath build/test-group-layer-engine)
.PHONY: test-run-group-parallel-engine
test-run-group-parallel-engine: build/test-group-parallel-engine
	@mkdir -p build/test-run/group-parallel-engine
	@cd build/test-run/group-parallel-engine && $(TEST_RUN_SETUP) && UBSAN_OPTIONS=halt_on_error=1 $(abspath build/test-group-parallel-engine)
.PHONY: test-run-group-send-engine
test-run-group-send-engine: build/test-group-send-engine
	@mkdir -p build/test-run/group-send-engine
	@cd build/test-run/group-send-engine && $(TEST_RUN_SETUP) && UBSAN_OPTIONS=halt_on_error=1 $(abspath build/test-group-send-engine)

.PHONY: test-all test
test-all: test-run-integration test-run-backends test-run-chord test-run-update-buttons test-run-py-integration test-run-py-adapters test-run-py-prepare_test test-run-update-serial test-run-update-usb-control test-run-py-uart_host_test test-run-py-benchmark_host_test test-run-py-remote_flow_test test-run-py-usb_remote_flow_test test-run-py-usb_remote_pty_test test-run-loopback-record test-run-stop-effects test-run-track-insert test-run-kit-track test-run-kit-record-workflow test-run-wav-pcm-formats test-run-sampler-edge-time test-run-public-overdrive test-run-public-fm2 test-run-public-wavefolder test-run-daisy-demo-project test-run-public-drums test-run-daisy-engine test-run-benchmark-requests test-run-plaits-stress-project test-run-benchmark-project test-run-clean-save test-run-benchmark-cleanup test-run-group-routing-cache test-run-display-rectangle test-run-display-job-slot test-run-visual-snapshot test-run-timing-boundary test-run-test test-run-plaits test-run-project-codec test-run-page-descriptors test-run-page-nav test-run-source-select test-run-engine-pages test-run-fx-chain test-run-browser-open-stress test-run-buttons test-run-duplicate test-run-daisy-adsr test-run-transition test-run-lab test-run-song test-run-song-page test-run-wave-zones test-run-stretch test-run-stretch-job test-run-warp test-run-stretch-playback test-run-mod-matrix test-run-fx-type-lock test-run-freeze test-run-sound-state test-run-reload test-run-morph-sets test-run-mute-layers test-run-capture test-run-plaits-gate test-run-partial-redraw test-run-theme test-run-plock-hold test-run-engine-descriptor test-run-sample-cache-promotion test-run-callback-gap test-run-usb-implicit test-run-usb-pcm-convert test-run-py-kernel_bounds_test test-run-py-circle_hook_test test-run-py-mk3_observe_test test-run-py-boot_seed_test test-run-py-update_identity_test test-run-py-circle_dhcp_patch_test test-run-py-hardware_capture_retry_test test-run-factory-wav test-run-pingpong-sampler test-run-sample-zones test-run-audio-render-slot test-run-audio-stage-profile test-run-kit-voice-mix test-run-performance-pitch test-run-outline-raster test-run-cutoff-optimization test-run-group-parameter test-run-pad-fx-parameters test-run-fx-stress-project test-run-group-insert test-run-group-layer-engine test-run-group-parallel-engine test-run-group-send-engine
TEST_TARGETS ?= test-all
test:
	ulimit -s $(TEST_STACK_KB) && $(MAKE) --no-print-directory -j$(TEST_JOBS) -k $(TEST_TARGETS)


# Freeze buffer ownership crosses the task/audio boundary; run the real engine
# concurrently rather than simulating cancellation between render calls.
.PHONY: freeze-handoff-test test-run-freeze-handoff freeze-handoff-tsan
build/test-freeze-handoff: tests/freeze_handoff_test.cpp build/test-core-engine.o $(HOST_SUPPORT_OBJECTS) $(PL_OBJECTS) $(UI_BASE_ARCHIVE)
	$(CXX) $(HOST_FLAGS) $(SAN) -pthread -Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-variable -Wno-unused-local-typedefs $< build/test-core-engine.o $(HOST_SUPPORT_OBJECTS) $(PL_OBJECTS) -o $@ $(UI_BASE_ARCHIVE)
freeze-handoff-test: build/test-freeze-handoff
	UBSAN_OPTIONS=halt_on_error=1 ./build/test-freeze-handoff
test-run-freeze-handoff: build/test-freeze-handoff
	UBSAN_OPTIONS=halt_on_error=1 ./build/test-freeze-handoff
test-all: test-run-freeze-handoff
TSAN_PL_OBJECTS := $(patsubst third_party/mutable/%.cc,build/tsan-plaits/%.o,$(PL_SOURCES))
build/tsan-plaits/%.o: third_party/mutable/%.cc
	@mkdir -p $(dir $@)
	$(CXX) $(HOST_FLAGS) -fsanitize=thread -g1 -O1 -c $< -o $@
TSAN_MSFA_OBJECTS := $(patsubst third_party/msfa/%.cc,build/tsan-msfa/%.o,$(MSFA_SOURCES))
build/tsan-msfa/%.o: third_party/msfa/%.cc $(wildcard third_party/msfa/*.h)
	@mkdir -p $(dir $@)
	$(CXX) -std=c++17 -O1 -fwrapv -fsanitize=thread -D__CONTROLLERS_H -Isrc/msfa_adapter/msfa -c $< -o $@
TSAN_DSP_SOURCES = $(filter-out build/%,$(DAISY)) src/resonator_dsp.cpp src/texture_dsp.cpp
build/test-freeze-handoff-tsan: tests/freeze_handoff_test.cpp $(ENGINE_SOURCES) $(TSAN_DSP_SOURCES) $(TSAN_PL_OBJECTS) $(TSAN_MSFA_OBJECTS) $(UI_TOOL_ARCHIVE)
	$(CXX) $(HOST_FLAGS) -fsanitize=thread -g1 -O1 -pthread $< $(ENGINE_SOURCES) $(TSAN_DSP_SOURCES) $(TSAN_PL_OBJECTS) $(TSAN_MSFA_OBJECTS) -o $@ $(UI_TOOL_ARCHIVE)
freeze-handoff-tsan: build/test-freeze-handoff-tsan
	TSAN_OPTIONS=halt_on_error=1 ./build/test-freeze-handoff-tsan

# The save failure regression is part of the normal gate as well.
test-all: async-project-save-test

.PHONY: project-codec-stack-test
build/test-project-codec-stack: tests/project_codec_stack_test.cpp $(DEMO_HEADERS) $(UI_BASE_ARCHIVE)
	$(CXX) $(HOST_FLAGS) $(SAN) -pthread -Wall -Wextra -Werror $< -o $@ $(UI_BASE_ARCHIVE)
project-codec-stack-test: test-run-project-codec-stack
test-all: project-codec-stack-test

build/test-plaits-poly: tests/plaits_poly_test.cpp build/test-core-engine.o $(HOST_SUPPORT_OBJECTS) $(PL_OBJECTS) $(UI_BASE_ARCHIVE)
	$(CXX) $(HOST_FLAGS) $(SAN) -O1 -pthread -Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-variable -Wno-unused-local-typedefs $< build/test-core-engine.o $(HOST_SUPPORT_OBJECTS) $(PL_OBJECTS) -o $@ $(UI_BASE_ARCHIVE)
.PHONY: test-run-plaits-poly
test-run-plaits-poly: build/test-plaits-poly
	UBSAN_OPTIONS=halt_on_error=1 ./build/test-plaits-poly
test-all: test-run-plaits-poly

build/test-browser-file-ops.o: tests/browser_file_ops_test.cpp $(wildcard src/*.h src/*.inc) $(wildcard platform/desktop/*.h)
	$(CXX) $(HOST_FLAGS) $(SAN) -O1 -Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-variable -Wno-unused-local-typedefs -c $< -o $@
build/test-browser-file-ops: build/test-browser-file-ops.o build/test-core-engine.o $(HOST_SUPPORT_OBJECTS) $(PL_OBJECTS) $(UI_BASE_ARCHIVE)
	$(CXX) $(HOST_FLAGS) $(SAN) -O1 $^ -o $@ $(UI_BASE_ARCHIVE)
.PHONY: test-run-browser-file-ops
test-run-browser-file-ops: build/test-browser-file-ops
	UBSAN_OPTIONS=halt_on_error=1 ./build/test-browser-file-ops
test-all: test-run-browser-file-ops

build/test-scene-take: tests/scene_take_test.cpp src/scene_take.h $(UI_BASE_ARCHIVE)
	$(CXX) $(HOST_FLAGS) $(SAN) -O1 -Wall -Wextra -Werror $< -o $@ $(UI_BASE_ARCHIVE)
build/test-scene-take-engine.o: tests/scene_take_engine_test.cpp $(wildcard src/*.h src/*.inc) $(wildcard platform/desktop/*.h)
	$(CXX) $(HOST_FLAGS) $(SAN) -O1 -Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-variable -Wno-unused-local-typedefs -c $< -o $@
build/test-scene-take-engine: build/test-scene-take-engine.o build/test-core-engine.o $(HOST_SUPPORT_OBJECTS) $(PL_OBJECTS) $(UI_BASE_ARCHIVE)
	$(CXX) $(HOST_FLAGS) $(SAN) -O1 $^ -o $@ $(UI_BASE_ARCHIVE)
.PHONY: test-run-scene-take
test-run-scene-take: build/test-scene-take build/test-scene-take-engine
	UBSAN_OPTIONS=halt_on_error=1 ./build/test-scene-take
	UBSAN_OPTIONS=halt_on_error=1 ./build/test-scene-take-engine
test-all: test-run-scene-take

build/test-clip-follow: tests/clip_follow_test.cpp src/clip_follow.h $(UI_BASE_ARCHIVE)
	$(CXX) $(HOST_FLAGS) $(SAN) -O1 -Wall -Wextra -Werror $< -o $@ $(UI_BASE_ARCHIVE)
build/test-clip-follow-engine.o: tests/clip_follow_engine_test.cpp $(wildcard src/*.h src/*.inc) src/ui/control_descriptor.h
	$(CXX) $(HOST_FLAGS) $(SAN) -O1 -Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-variable -Wno-unused-local-typedefs -c $< -o $@
build/test-clip-follow-engine: build/test-clip-follow-engine.o build/test-core-engine.o $(HOST_SUPPORT_OBJECTS) $(PL_OBJECTS) $(UI_BASE_ARCHIVE)
	$(CXX) $(HOST_FLAGS) $(SAN) -O1 $^ -o $@ $(UI_BASE_ARCHIVE)
.PHONY: test-run-clip-follow
test-run-clip-follow: build/test-clip-follow build/test-clip-follow-engine
	UBSAN_OPTIONS=halt_on_error=1 ./build/test-clip-follow
	UBSAN_OPTIONS=halt_on_error=1 ./build/test-clip-follow-engine
test-all: test-run-clip-follow

ENGINE_TEST_OBJECTS := $(addprefix build/engine-core/,$(addsuffix .o,$(ENGINE_NAMES)))
build/engine-core/%.o: src/%.cpp Makefile src/engine_sources.mk
	@mkdir -p $(@D)
	$(CXX) $(HOST_FLAGS) $(SAN) -O1 -MMD -MP -Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-variable -Wno-unused-local-typedefs -c $< -o $@
-include $(wildcard build/engine-core/*.d)
build/test-core-engine.o: $(ENGINE_TEST_OBJECTS)
	$(LD) -r $(ENGINE_TEST_OBJECTS) -o $@
ENGINE_TOOL_OBJECTS := $(addprefix build/engine-tool/,$(addsuffix .o,$(ENGINE_NAMES)))
build/engine-tool/%.o: src/%.cpp Makefile src/engine_sources.mk
	@mkdir -p $(@D)
	$(CXX) $(HOST_FLAGS) -O2 -MMD -MP -Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-variable -Wno-unused-local-typedefs -c $< -o $@
-include $(wildcard build/engine-tool/*.d)


build/test-resample.o: tests/resample_test.cpp $(wildcard src/*.h src/*.inc) $(wildcard platform/desktop/*.h)
	$(CXX) $(HOST_FLAGS) $(SAN) -O1 -Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-variable -Wno-unused-local-typedefs -c $< -o $@
build/test-resample: build/test-resample.o build/test-core-engine.o $(HOST_SUPPORT_OBJECTS) $(PL_OBJECTS) $(UI_BASE_ARCHIVE)
	$(CXX) $(HOST_FLAGS) $(SAN) -O1 $^ -o $@ $(UI_BASE_ARCHIVE)
.PHONY: test-run-resample
test-run-resample: build/test-resample
	UBSAN_OPTIONS=halt_on_error=1 ./build/test-resample
test-all: test-run-resample

build/test-arrangement.o: tests/arrangement_test.cpp $(wildcard src/*.h src/*.inc)
	$(CXX) $(HOST_FLAGS) $(SAN) -O1 -Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-variable -Wno-unused-local-typedefs -c $< -o $@
build/test-arrangement: build/test-arrangement.o build/test-core-engine.o $(HOST_SUPPORT_OBJECTS) $(PL_OBJECTS) $(UI_BASE_ARCHIVE)
	$(CXX) $(HOST_FLAGS) $(SAN) -O1 $^ -o $@ $(UI_BASE_ARCHIVE)
.PHONY: test-run-arrangement
test-run-arrangement: build/test-arrangement
	UBSAN_OPTIONS=halt_on_error=1 ./build/test-arrangement
test-all: test-run-arrangement

# Sidecar failure paths are independent of DSP compilation.
build/test-preserved-payload: tests/preserved_payload_test.cpp src/preserved_payload.h $(UI_BASE_ARCHIVE)
	$(CXX) $(HOST_FLAGS) $(SAN) -O1 -Wall -Wextra -Werror $< -o $@ $(UI_BASE_ARCHIVE)
.PHONY: test-run-preserved-payload
test-run-preserved-payload: build/test-preserved-payload
	UBSAN_OPTIONS=halt_on_error=1 ./build/test-preserved-payload
test-all: test-run-preserved-payload

build/test-recording-controls: tests/recording_controls_test.cpp src/recording_controls.h src/ui/control_descriptor.h $(UI_BASE_ARCHIVE)
	$(CXX) $(HOST_FLAGS) $(SAN) -O1 -Wall -Wextra -Werror $< -o $@ $(UI_BASE_ARCHIVE)
.PHONY: test-run-recording-controls
test-run-recording-controls: build/test-recording-controls
	UBSAN_OPTIONS=halt_on_error=1 ./build/test-recording-controls
test-all: test-run-recording-controls

build/test-sampler-record-request.o: tests/sampler_record_request_test.cpp $(DEMO_HEADERS)
	$(CXX) $(HOST_FLAGS) $(SAN) -O1 -Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-variable -Wno-unused-local-typedefs -c $< -o $@
build/test-sampler-record-request: build/test-sampler-record-request.o build/test-core-engine.o $(HOST_SUPPORT_OBJECTS) $(PL_OBJECTS) $(UI_BASE_ARCHIVE)
	$(CXX) $(HOST_FLAGS) $(SAN) -O1 $^ -o $@ $(UI_BASE_ARCHIVE)
.PHONY: test-run-sampler-record-request
test-run-sampler-record-request: build/test-sampler-record-request
	UBSAN_OPTIONS=halt_on_error=1 ./build/test-sampler-record-request
test-all: test-run-sampler-record-request

build/test-ui390-workflow.o: tests/ui390_workflow_test.cpp $(wildcard src/*.h src/*.inc) $(wildcard src/ui/*.h)
	$(CXX) $(HOST_FLAGS) $(SAN) -O0 -Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-variable -Wno-unused-local-typedefs -c $< -o $@
build/test-ui390-workflow: build/test-ui390-workflow.o build/test-core-engine.o $(HOST_SUPPORT_OBJECTS) $(PL_OBJECTS) $(UI_BASE_ARCHIVE)
	$(CXX) $(HOST_FLAGS) $(SAN) -O1 $^ -o $@ $(UI_BASE_ARCHIVE)
.PHONY: test-run-ui390-workflow
test-run-ui390-workflow: build/test-ui390-workflow
	UBSAN_OPTIONS=halt_on_error=1 ./build/test-ui390-workflow
test-all: test-run-ui390-workflow

.PHONY: docs-check
docs-check:
	python3 tools/check_docs.py

build/test-midi-loopback-probe: tests/midi_loopback_probe_test.cpp src/midi_loopback_probe.h src/backends.h $(UI_BASE_ARCHIVE)
	$(CXX) $(HOST_FLAGS) $(SAN) -O1 -Wall -Wextra -Werror $< -o $@ $(UI_BASE_ARCHIVE)
.PHONY: test-run-midi-loopback-probe
test-run-midi-loopback-probe: build/test-midi-loopback-probe
	UBSAN_OPTIONS=halt_on_error=1 ./build/test-midi-loopback-probe
test-all: test-run-midi-loopback-probe
.PHONY: test-run-midi-loopback-host
test-run-midi-loopback-host:
	python3 tests/update/midi_loopback_host_test.py
test-all: test-run-midi-loopback-host

# The wrapper isolates native resource macros from other upstream cores.
build/resonator-dsp.o: src/resonator_dsp.cpp src/resonator_dsp.h
	@mkdir -p $(@D)
	$(CXX) $(HOST_FLAGS) $(SAN) -g -O1 -MMD -MP -c $< -o $@

# Avoid make 4.3 snap_deps failure combining the legacy engine .d and native
# resource dependencies. The engine tracks its headers explicitly instead.
-include $(wildcard $(PL_OBJECTS:.o=.d))
build/test-core-engine.o: $(wildcard src/*.h src/ui/*.h) $(shell find third_party/mutable third_party/daisysp/Source -name '*.h')

build/test-resonator-engine: tests/resonator_engine_test.cpp build/test-core-engine.o $(wildcard src/*.h src/*.inc) $(PL_OBJECTS) $(HOST_SUPPORT_OBJECTS) $(UI_BASE_ARCHIVE)
	$(CXX) $(HOST_FLAGS) -O1 -Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-variable -Wno-unused-local-typedefs $(SAN) $< build/test-core-engine.o $(HOST_SUPPORT_OBJECTS) $(PL_OBJECTS) -o $@ $(UI_BASE_ARCHIVE)
.PHONY: test-run-resonator-engine
test-run-resonator-engine: build/test-resonator-engine
	UBSAN_OPTIONS=halt_on_error=1 ./build/test-resonator-engine
test-all: test-run-resonator-engine

build/texture-dsp.o: src/texture_dsp.cpp src/texture_dsp.h
	@mkdir -p $(@D)
	$(CXX) $(HOST_FLAGS) -O1 $(SAN) -MMD -MP -c $< -o $@

build/test-texture-engine: tests/texture_engine_test.cpp build/test-core-engine.o $(wildcard src/*.h src/*.inc) $(PL_OBJECTS) $(HOST_SUPPORT_OBJECTS) $(UI_BASE_ARCHIVE)
	$(CXX) $(HOST_FLAGS) -O1 -Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-variable -Wno-unused-local-typedefs $(SAN) $< build/test-core-engine.o $(HOST_SUPPORT_OBJECTS) $(PL_OBJECTS) -o $@ $(UI_BASE_ARCHIVE)
.PHONY: test-run-texture-engine
test-run-texture-engine: build/test-texture-engine
	UBSAN_OPTIONS=halt_on_error=1 ./build/test-texture-engine
test-all: test-run-texture-engine

build/mutable-port/correlator.cc: third_party/mutable/clouds/dsp/correlator.cc tools/mutable/portable_correlator.py
	python3 tools/mutable/portable_correlator.py $< $@
build/plaits/clouds/dsp/correlator.o: build/mutable-port/correlator.cc
	@mkdir -p $(dir $@)
	$(CXX) $(HOST_FLAGS) $(SAN) -g -O1 -MMD -MP -c $< -o $@

build/mutable-port/clouds/dsp/looping_sample_player.h: third_party/mutable/clouds/dsp/looping_sample_player.h tools/mutable/portable_loop.py
	python3 tools/mutable/portable_loop.py $< $@
$(TEXTURE_OBJECTS): build/mutable-port/clouds/dsp/looping_sample_player.h

build/mutable-port/granular_processor.cc: third_party/mutable/clouds/dsp/granular_processor.cc tools/mutable/portable_processor.py
	python3 tools/mutable/portable_processor.py $< $@
build/plaits/clouds/dsp/granular_processor.o: build/mutable-port/granular_processor.cc
	@mkdir -p $(dir $@)
	$(CXX) $(HOST_FLAGS) $(SAN) -g -O1 -MMD -MP -c $< -o $@


# Use production optimization so the scalar baseline has Circle's fused MAC semantics.
build/test-texture-rate: tests/texture_rate_test.cpp src/texture_voice.h src/texture_rate_coefficients.h $(UI_BASE_ARCHIVE)
	$(CXX) $(HOST_FLAGS) -O2 $(SAN) $< -o $@ $(UI_BASE_ARCHIVE)
build/test-native-parameter-range: tests/native_parameter_range_test.cpp $(wildcard src/*.h src/*.inc) $(UI_BASE_ARCHIVE)
	$(CXX) $(HOST_FLAGS) -O1 $(SAN) $< -o $@ $(UI_BASE_ARCHIVE)
.PHONY: test-run-texture-rate test-run-native-parameter-range
test-run-texture-rate: build/test-texture-rate
	UBSAN_OPTIONS=halt_on_error=1 ./build/test-texture-rate
test-run-native-parameter-range: build/test-native-parameter-range
	UBSAN_OPTIONS=halt_on_error=1 ./build/test-native-parameter-range
test-all: test-run-texture-rate test-run-native-parameter-range


build/test-texture-extremes: tests/texture_extremes_test.cpp $(TEXTURE_OBJECTS) build/plaits/stmlib/dsp/units.o build/plaits/stmlib/utils/random.o $(UI_BASE_ARCHIVE)
	$(CXX) $(HOST_FLAGS) -O1 $(SAN) $^ -o $@ $(UI_BASE_ARCHIVE)
.PHONY: test-run-texture-extremes
test-run-texture-extremes: build/test-texture-extremes
	UBSAN_OPTIONS=halt_on_error=1 ./build/test-texture-extremes
test-all: test-run-texture-extremes

build/test-audio-lane.o: tests/audio_lane_test.cpp $(DEMO_HEADERS)
	$(CXX) $(HOST_FLAGS) $(SAN) -O1 -Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-variable -Wno-unused-local-typedefs -c $< -o $@
build/test-audio-lane: build/test-audio-lane.o build/test-core-engine.o $(HOST_SUPPORT_OBJECTS) $(PL_OBJECTS) $(UI_BASE_ARCHIVE)
	$(CXX) $(HOST_FLAGS) $(SAN) -O1 $^ -o $@ $(UI_BASE_ARCHIVE)
.PHONY: test-run-audio-lane
test-run-audio-lane: build/test-audio-lane
	UBSAN_OPTIONS=halt_on_error=1 ./build/test-audio-lane
test-all: test-run-audio-lane

build/test-midi-transport-echo.o: tests/midi_transport_echo_test.cpp $(DEMO_HEADERS)
	$(CXX) $(HOST_FLAGS) $(SAN) -O1 -Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-variable -Wno-unused-local-typedefs -c $< -o $@
build/test-midi-transport-echo: build/test-midi-transport-echo.o build/test-core-engine.o $(HOST_SUPPORT_OBJECTS) $(PL_OBJECTS) $(UI_BASE_ARCHIVE)
	$(CXX) $(HOST_FLAGS) $(SAN) -O1 $^ -o $@ $(UI_BASE_ARCHIVE)
.PHONY: test-run-midi-transport-echo
test-run-midi-transport-echo: build/test-midi-transport-echo
	UBSAN_OPTIONS=halt_on_error=1 ./build/test-midi-transport-echo
test-all: test-run-midi-transport-echo

build/test-ui-navigation-led.o: tests/ui_navigation_led_test.cpp $(DEMO_HEADERS)
	$(CXX) $(HOST_FLAGS) $(SAN) -O0 -Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-variable -Wno-unused-local-typedefs -c $< -o $@
build/test-ui-navigation-led: build/test-ui-navigation-led.o build/test-core-engine.o $(HOST_SUPPORT_OBJECTS) $(PL_OBJECTS) $(UI_BASE_ARCHIVE)
	$(CXX) $(HOST_FLAGS) $(SAN) -O1 $^ -o $@ $(UI_BASE_ARCHIVE)
.PHONY: test-run-ui-navigation-led
test-run-ui-navigation-led: build/test-ui-navigation-led
	UBSAN_OPTIONS=halt_on_error=1 ./build/test-ui-navigation-led
test-all: test-run-ui-navigation-led

build/led-legend: examples/led_legend.cpp src/ui/pad_feedback.h src/ui/led_state.h src/ui/theme.h src/mk3_surface.h $(UI_TOOL_ARCHIVE)
	@mkdir -p build
	$(CXX) -std=c++17 -Isrc -Wall -Wextra -Werror $< -o $@ $(UI_TOOL_ARCHIVE)
.PHONY: led-preview
led-preview: build/led-legend
	./build/led-legend > build/led-legend.svg

.PHONY: factory-catalog test-run-factory-content
build/factory-catalog: tools/content/catalog.cpp Makefile tools/content/catalog_environment.lock.json $(wildcard src/*.h src/*.inc) $(UI_TOOL_ARCHIVE) | factory-generation-environment
	@mkdir -p build
	$(CXX) -std=c++17 -O2 -Isrc -Wall -Wextra -Werror -MMD -MP -MF $@.d -MT $@ $< -o $@ $(UI_TOOL_ARCHIVE)
.PHONY: factory-generation-environment
factory-generation-environment:
	python3 tools/content/catalog_environment.py --compiler '$(CXX)'
factory-catalog: build/factory-catalog factory-generation-environment
	@mkdir -p build/factory-data
	./build/factory-catalog assets/samples/demo-slices.wav build/factory-data
build/test-factory-content: tests/factory_content_test.cpp $(wildcard src/*.h src/*.inc) $(UI_BASE_ARCHIVE)
	@mkdir -p build
	$(CXX) -std=c++17 -O1 -Isrc -Wall -Wextra -Werror $(SAN) $< -o $@ $(UI_BASE_ARCHIVE)
test-run-factory-content: build/test-factory-content
	UBSAN_OPTIONS=halt_on_error=1 ./build/test-factory-content $(abspath assets/samples/demo-slices.wav)
test-all: test-run-factory-content

build/test-factory-workflow.o: tests/factory_workflow_test.cpp $(DEMO_HEADERS)
	$(CXX) $(HOST_FLAGS) $(SAN) -O1 -Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-variable -Wno-unused-local-typedefs -c $< -o $@
build/test-factory-workflow: build/test-factory-workflow.o build/test-core-engine.o $(HOST_SUPPORT_OBJECTS) $(PL_OBJECTS) $(UI_BASE_ARCHIVE)
	$(CXX) $(HOST_FLAGS) $(SAN) -O1 $^ -o $@ $(UI_BASE_ARCHIVE)
.PHONY: test-run-factory-workflow
test-run-factory-workflow: factory-catalog build/test-factory-workflow
	UBSAN_OPTIONS=halt_on_error=1 ./build/test-factory-workflow $(abspath build/factory-data)
test-all: test-run-factory-workflow

build/test-name-scroll: tests/name_scroll_test.cpp src/software_canvas.h src/update/view.h $(UI_BASE_ARCHIVE)
	$(CXX) $(HOST_FLAGS) $(SAN) -Ithird_party/monocypher -O1 -Wall -Wextra -Werror $< -o $@ $(UI_BASE_ARCHIVE)
test-run-name-scroll: build/test-name-scroll
	UBSAN_OPTIONS=halt_on_error=1 ./build/test-name-scroll
test-all: test-run-name-scroll

build/test-sampler-instance: tests/sampler_instance_test.cpp build/test-core-engine.o $(DEMO_HEADERS) $(PL_OBJECTS) $(HOST_SUPPORT_OBJECTS) $(UI_BASE_ARCHIVE)
	$(CXX) $(HOST_FLAGS) -O1 -Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-variable -Wno-unused-local-typedefs $(SAN) $< build/test-core-engine.o $(HOST_SUPPORT_OBJECTS) $(PL_OBJECTS) -o $@ $(UI_BASE_ARCHIVE)
.PHONY: test-run-sampler-instance
test-run-sampler-instance: build/test-sampler-instance
	UBSAN_OPTIONS=halt_on_error=1 ./build/test-sampler-instance
test-all: test-run-sampler-instance

build/test-persistent-track: tests/persistent_track_test.cpp build/test-core-engine.o $(DEMO_HEADERS) $(PL_OBJECTS) $(HOST_SUPPORT_OBJECTS) $(UI_BASE_ARCHIVE)
	$(CXX) $(HOST_FLAGS) -O0 -Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-variable -Wno-unused-local-typedefs $(SAN) $< build/test-core-engine.o $(HOST_SUPPORT_OBJECTS) $(PL_OBJECTS) -o $@ $(UI_BASE_ARCHIVE)
test-run-persistent-track: build/test-persistent-track
	UBSAN_OPTIONS=halt_on_error=1 ./build/test-persistent-track
test-all: test-run-persistent-track

build/test-named-preset: tests/named_preset_test.cpp build/test-core-engine.o $(HOST_SUPPORT_OBJECTS) $(PL_OBJECTS) $(DEMO_HEADERS) $(UI_BASE_ARCHIVE)
	$(CXX) $(HOST_FLAGS) $(SAN) -O0 -Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-variable -Wno-unused-local-typedefs $< build/test-core-engine.o $(HOST_SUPPORT_OBJECTS) $(PL_OBJECTS) -o $@ $(UI_BASE_ARCHIVE)
.PHONY: test-run-named-preset
test-run-named-preset: build/test-named-preset
	UBSAN_OPTIONS=halt_on_error=1 ./build/test-named-preset
test-all: test-run-named-preset

build/test-preset-audition: tests/preset_audition_test.cpp build/test-core-engine.o $(HOST_SUPPORT_OBJECTS) $(PL_OBJECTS) $(DEMO_HEADERS) $(UI_BASE_ARCHIVE)
	$(CXX) $(HOST_FLAGS) $(SAN) -O0 -Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-variable -Wno-unused-local-typedefs $< build/test-core-engine.o $(HOST_SUPPORT_OBJECTS) $(PL_OBJECTS) -o $@ $(UI_BASE_ARCHIVE)
.PHONY: test-run-preset-audition
test-run-preset-audition: build/test-preset-audition
	UBSAN_OPTIONS=halt_on_error=1 ./build/test-preset-audition
test-all: test-run-preset-audition

build/test-engine-morph: tests/engine_morph_test.cpp build/test-core-engine.o $(HOST_SUPPORT_OBJECTS) $(PL_OBJECTS) $(DEMO_HEADERS) $(UI_BASE_ARCHIVE)
	$(CXX) $(HOST_FLAGS) $(SAN) -O1 -Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-variable -Wno-unused-local-typedefs $< build/test-core-engine.o $(HOST_SUPPORT_OBJECTS) $(PL_OBJECTS) -o $@ $(UI_BASE_ARCHIVE)
test-run-engine-morph: build/test-engine-morph
	UBSAN_OPTIONS=halt_on_error=1 ./build/test-engine-morph
test-all: test-run-engine-morph

build/test-scoped-target: tests/scoped_target_test.cpp build/test-core-engine.o $(HOST_SUPPORT_OBJECTS) $(PL_OBJECTS) $(DEMO_HEADERS) $(UI_BASE_ARCHIVE)
	$(CXX) $(HOST_FLAGS) $(SAN) -O0 -Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-variable -Wno-unused-local-typedefs $< build/test-core-engine.o $(HOST_SUPPORT_OBJECTS) $(PL_OBJECTS) -o $@ $(UI_BASE_ARCHIVE)
test-run-scoped-target: build/test-scoped-target
	UBSAN_OPTIONS=halt_on_error=1 ./build/test-scoped-target
test-all: test-run-scoped-target

build/test-unavailable-target: tests/unavailable_target_test.cpp $(DEMO_HEADERS) $(UI_BASE_ARCHIVE)
	$(CXX) $(HOST_FLAGS) $(SAN) -O1 -Wall -Wextra -Werror $< -o $@ $(UI_BASE_ARCHIVE)
test-run-unavailable-target: build/test-unavailable-target
	UBSAN_OPTIONS=halt_on_error=1 ./build/test-unavailable-target
test-all: test-run-unavailable-target

build/engine-schema: tools/engine-schema.cpp $(wildcard src/*.h) $(UI_TOOL_ARCHIVE)
	@mkdir -p $(@D)
	$(CXX) $(HOST_FLAGS) -O1 -Wall -Wextra -Werror $< -o $@ $(UI_TOOL_ARCHIVE)
.PHONY: engine-schema test-run-engine-schema
engine-schema: build/engine-schema
	./build/engine-schema > build/engine-schema.json
build/test-engine-schema: tests/engine_schema_test.cpp $(wildcard src/*.h) $(UI_BASE_ARCHIVE)
	@mkdir -p $(@D)
	$(CXX) $(HOST_FLAGS) $(SAN) -O1 -Wall -Wextra -Werror $< -o $@ $(UI_BASE_ARCHIVE)
test-run-engine-schema: build/test-engine-schema
	UBSAN_OPTIONS=halt_on_error=1 ./build/test-engine-schema
test-all: test-run-engine-schema

build/test-wavetable-engine: tests/wavetable_engine_test.cpp build/test-core-engine.o $(DEMO_HEADERS) $(HOST_SUPPORT_OBJECTS) $(PL_OBJECTS) $(UI_BASE_ARCHIVE)
	$(CXX) $(HOST_FLAGS) $(SAN) -O1 -Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-variable -Wno-unused-local-typedefs $< build/test-core-engine.o $(HOST_SUPPORT_OBJECTS) $(PL_OBJECTS) -o $@ $(UI_BASE_ARCHIVE)
.PHONY: test-run-wavetable-engine wavetable-bank
test-run-wavetable-engine: build/test-wavetable-engine
	UBSAN_OPTIONS=halt_on_error=1 ./build/test-wavetable-engine
wavetable-bank:
	python3 tools/content/generate-wavetable.py
test-all: test-run-wavetable-engine

build/test-virtual-analog-engine: tests/virtual_analog_engine_test.cpp build/test-core-engine.o $(DEMO_HEADERS) $(HOST_SUPPORT_OBJECTS) $(PL_OBJECTS) $(UI_BASE_ARCHIVE)
	$(CXX) $(HOST_FLAGS) $(SAN) -O1 -Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-variable -Wno-unused-local-typedefs $< build/test-core-engine.o $(HOST_SUPPORT_OBJECTS) $(PL_OBJECTS) -o $@ $(UI_BASE_ARCHIVE)
.PHONY: test-run-virtual-analog-engine
test-run-virtual-analog-engine: build/test-virtual-analog-engine
	UBSAN_OPTIONS=halt_on_error=1 ./build/test-virtual-analog-engine
test-all: test-run-virtual-analog-engine

build/test-bake-preset: tests/bake_preset_test.cpp build/test-core-engine.o $(HOST_SUPPORT_OBJECTS) $(PL_OBJECTS) $(DEMO_HEADERS) $(UI_BASE_ARCHIVE)
	$(CXX) $(HOST_FLAGS) $(SAN) -O0 -Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-variable -Wno-unused-local-typedefs $< build/test-core-engine.o $(HOST_SUPPORT_OBJECTS) $(PL_OBJECTS) -o $@ $(UI_BASE_ARCHIVE)
.PHONY: test-run-bake-preset
test-run-bake-preset: build/test-bake-preset
	UBSAN_OPTIONS=halt_on_error=1 ./build/test-bake-preset
test-all: test-run-bake-preset

# Unmodified fixed-point vendor code uses wrapping and signed shifts.
build/msfa/%.o: third_party/msfa/%.cc $(wildcard third_party/msfa/*.h)
	@mkdir -p $(dir $@)
	$(CXX) -std=c++17 -O2 -fwrapv $(SAN) -fno-sanitize=shift -D__CONTROLLERS_H -Isrc/msfa_adapter/msfa -c $< -o $@

build/test-fm6-voice: tests/fm6_voice_test.cpp src/fm6_voice.cpp src/fm6_voice.h src/fm6_state.h $(MSFA_OBJECTS) build/plaits/plaits/resources.o $(UI_BASE_ARCHIVE)
	$(CXX) $(HOST_FLAGS) $(SAN) -O1 tests/fm6_voice_test.cpp src/fm6_voice.cpp $(MSFA_OBJECTS) build/plaits/plaits/resources.o -o $@ $(UI_BASE_ARCHIVE)
.PHONY: test-run-fm6-voice
test-run-fm6-voice: build/test-fm6-voice
	UBSAN_OPTIONS=halt_on_error=1 ./build/test-fm6-voice
test-all: test-run-fm6-voice
test simulator: $(MSFA_OBJECTS)

build/test-fm6-engine: tests/fm6_engine_test.cpp build/test-core-engine.o $(DEMO_HEADERS) $(HOST_SUPPORT_OBJECTS) $(PL_OBJECTS) $(UI_BASE_ARCHIVE)
	$(CXX) $(HOST_FLAGS) $(SAN) -O1 -Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-variable -Wno-unused-local-typedefs $< build/test-core-engine.o $(HOST_SUPPORT_OBJECTS) $(PL_OBJECTS) -o $@ $(UI_BASE_ARCHIVE)
test-run-fm6-engine: build/test-fm6-engine
	UBSAN_OPTIONS=halt_on_error=1 ./build/test-fm6-engine
test-all: test-run-fm6-engine

build/test-soundfont: tests/soundfont_test.cpp build/test-core-engine.o $(DEMO_HEADERS) $(HOST_SUPPORT_OBJECTS) $(PL_OBJECTS) $(UI_BASE_ARCHIVE)
	$(CXX) $(HOST_FLAGS) $(SAN) -O1 -Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-variable -Wno-unused-local-typedefs $< build/test-core-engine.o $(HOST_SUPPORT_OBJECTS) $(PL_OBJECTS) -o $@ $(UI_BASE_ARCHIVE)
test-run-soundfont: build/test-soundfont
	UBSAN_OPTIONS=halt_on_error=1 ./build/test-soundfont
test-all: test-run-soundfont

build/test-vocoder: tests/vocoder_test.cpp $(DEMO_HEADERS) $(HOST_SUPPORT_OBJECTS) $(PL_OBJECTS) $(UI_BASE_ARCHIVE)
	$(CXX) $(HOST_FLAGS) $(SAN) -O1 -Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-variable -Wno-unused-local-typedefs $< $(HOST_SUPPORT_OBJECTS) $(PL_OBJECTS) -o $@ $(UI_BASE_ARCHIVE)
test-run-vocoder: build/test-vocoder
	UBSAN_OPTIONS=halt_on_error=1 ./build/test-vocoder
test-all: test-run-vocoder

build/test-tracker: tests/tracker_test.cpp build/test-core-engine.o $(DEMO_HEADERS) $(HOST_SUPPORT_OBJECTS) $(PL_OBJECTS) $(UI_BASE_ARCHIVE)
	$(CXX) $(HOST_FLAGS) $(SAN) -O1 -Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-variable -Wno-unused-local-typedefs $< build/test-core-engine.o $(HOST_SUPPORT_OBJECTS) $(PL_OBJECTS) -o $@ $(UI_BASE_ARCHIVE)
test-run-tracker: build/test-tracker
	UBSAN_OPTIONS=halt_on_error=1 ./build/test-tracker
test-all: test-run-tracker

build/test-spectral-engines: tests/spectral_engines_test.cpp build/test-core-engine.o $(DEMO_HEADERS) $(HOST_SUPPORT_OBJECTS) $(PL_OBJECTS) $(UI_BASE_ARCHIVE)
	$(CXX) $(HOST_FLAGS) $(SAN) -O1 -Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-variable -Wno-unused-local-typedefs $< build/test-core-engine.o $(HOST_SUPPORT_OBJECTS) $(PL_OBJECTS) -o $@ $(UI_BASE_ARCHIVE)
test-run-spectral-engines: build/test-spectral-engines
	UBSAN_OPTIONS=halt_on_error=1 ./build/test-spectral-engines
test-all: test-run-spectral-engines

build/test-layer-preset: tests/layer_preset_test.cpp build/test-core-engine.o $(DEMO_HEADERS) $(HOST_SUPPORT_OBJECTS) $(PL_OBJECTS) $(UI_BASE_ARCHIVE)
	$(CXX) $(HOST_FLAGS) $(SAN) -O1 -Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-variable -Wno-unused-local-typedefs $< build/test-core-engine.o $(HOST_SUPPORT_OBJECTS) $(PL_OBJECTS) -o $@ $(UI_BASE_ARCHIVE)
test-run-layer-preset: build/test-layer-preset
	UBSAN_OPTIONS=halt_on_error=1 ./build/test-layer-preset
test-all: test-run-layer-preset

build/test-layer-files: tests/layer_files_test.cpp build/test-core-engine.o $(DEMO_HEADERS) $(HOST_SUPPORT_OBJECTS) $(PL_OBJECTS) $(UI_BASE_ARCHIVE)
	$(CXX) $(HOST_FLAGS) $(SAN) -O1 -Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-variable -Wno-unused-local-typedefs $< build/test-core-engine.o $(HOST_SUPPORT_OBJECTS) $(PL_OBJECTS) -o $@ $(UI_BASE_ARCHIVE)
test-run-layer-files: build/test-layer-files
	UBSAN_OPTIONS=halt_on_error=1 ./build/test-layer-files
test-all: test-run-layer-files

build/test-multitimbral: tests/multitimbral_test.cpp build/test-core-engine.o $(DEMO_HEADERS) $(HOST_SUPPORT_OBJECTS) $(PL_OBJECTS) $(UI_BASE_ARCHIVE)
	$(CXX) $(HOST_FLAGS) $(SAN) -O1 -Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-variable -Wno-unused-local-typedefs $< build/test-core-engine.o $(HOST_SUPPORT_OBJECTS) $(PL_OBJECTS) -o $@ $(UI_BASE_ARCHIVE)
test-run-multitimbral: build/test-multitimbral
	UBSAN_OPTIONS=halt_on_error=1 ./build/test-multitimbral
test-all: test-run-multitimbral

build/test-wave4-ui: tests/wave4_ui_test.cpp build/test-core-engine.o $(DEMO_HEADERS) $(HOST_SUPPORT_OBJECTS) $(PL_OBJECTS) $(UI_BASE_ARCHIVE)
	$(CXX) $(HOST_FLAGS) $(SAN) -O1 -Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-variable -Wno-unused-local-typedefs $< build/test-core-engine.o $(HOST_SUPPORT_OBJECTS) $(PL_OBJECTS) -o $@ $(UI_BASE_ARCHIVE)
test-run-wave4-ui: build/test-wave4-ui
	UBSAN_OPTIONS=halt_on_error=1 ./build/test-wave4-ui

build/test-device-settings: tests/device_settings_test.cpp src/device_settings.h src/input_tuner.h $(UI_BASE_ARCHIVE)
	$(CXX) -std=c++17 -Isrc $(SAN) -O1 -Wall -Wextra -Werror $< -o $@ $(UI_BASE_ARCHIVE)
test-run-device-settings: build/test-device-settings
	UBSAN_OPTIONS=halt_on_error=1 ./build/test-device-settings

build/test-device-workflow: tests/device_workflow_test.cpp build/test-core-engine.o $(DEMO_HEADERS) $(HOST_SUPPORT_OBJECTS) $(PL_OBJECTS) $(UI_BASE_ARCHIVE)
	$(CXX) $(HOST_FLAGS) $(SAN) -O1 -Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-variable -Wno-unused-local-typedefs $< build/test-core-engine.o $(HOST_SUPPORT_OBJECTS) $(PL_OBJECTS) -o $@ $(UI_BASE_ARCHIVE)
test-run-device-workflow: build/test-device-workflow
	UBSAN_OPTIONS=halt_on_error=1 ./build/test-device-workflow

build/test-wave4-workload: tests/wave4_workload_test.cpp src/wave4_workload.h $(DEMO_HEADERS) $(UI_BASE_ARCHIVE)
	$(CXX) $(HOST_FLAGS) $(SAN) -O1 $< -o $@ $(UI_BASE_ARCHIVE)
test-run-wave4-workload: build/test-wave4-workload
	UBSAN_OPTIONS=halt_on_error=1 ./build/test-wave4-workload

# Run the independent audio tail even when an earlier backend case fails.
.PHONY: backend-audio-test
build/test-backend-audio: tests/integration/backends_test.cpp build/test-core-engine.o $(DEMO_HEADERS) $(HOST_SUPPORT_OBJECTS) $(PL_OBJECTS) $(UI_BASE_ARCHIVE)
	$(CXX) $(HOST_FLAGS) -O1 -Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-variable -Wno-unused-local-typedefs $(SAN) -DGROOVE_BACKEND_AUDIO_ONLY $< build/test-core-engine.o $(HOST_SUPPORT_OBJECTS) $(PL_OBJECTS) -o $@ $(UI_BASE_ARCHIVE)
backend-audio-test: build/test-backend-audio
	UBSAN_OPTIONS=halt_on_error=1 ./build/test-backend-audio

# Memoized Rings coefficients: header and implementation must be paired.
build/mutable-port/rings/dsp/string.h build/mutable-port/rings/dsp/string.cc &: tools/mutable/cached_string.py third_party/mutable/rings/dsp/string.h third_party/mutable/rings/dsp/string.cc
	python3 tools/mutable/cached_string.py third_party/mutable build/mutable-port
build/resonator-dsp.o $(filter build/plaits/rings/%,$(PL_OBJECTS)): build/mutable-port/rings/dsp/string.h
build/plaits/rings/dsp/string.o: build/mutable-port/rings/dsp/string.cc
	$(CXX) $(HOST_FLAGS) $(SAN) -g -O1 -MMD -MP -c $< -o $@
build/reference-string.o: third_party/mutable/rings/dsp/string.cc
	$(CXX) -std=c++17 -O1 $(SAN) -Ithird_party/mutable -DString=ReferenceString -DDampingFilter=ReferenceDampingFilter -DkDelayLineSize=ReferenceDelayLineSize -c $< -o $@
build/test-native-string-cache: tests/native_string_cache_test.cpp build/mutable-port/rings/dsp/string.cc build/mutable-port/rings/dsp/string.h build/reference-string.o $(UI_BASE_ARCHIVE)
	$(CXX) $(HOST_FLAGS) -O1 $(SAN) $< build/mutable-port/rings/dsp/string.cc build/reference-string.o third_party/mutable/rings/resources.cc third_party/mutable/stmlib/utils/random.cc third_party/mutable/stmlib/dsp/units.cc -o $@ $(UI_BASE_ARCHIVE)
.PHONY: test-run-native-string-cache test-run-native-budget
test-run-native-string-cache: build/test-native-string-cache
	UBSAN_OPTIONS=halt_on_error=1 ./build/test-native-string-cache
build/test-native-budget: tests/native_budget_test.cpp src/native_budget.h src/engine_registry.h $(UI_BASE_ARCHIVE)
	$(CXX) $(HOST_FLAGS) -O1 $(SAN) $< -o $@ $(UI_BASE_ARCHIVE)
test-run-native-budget: build/test-native-budget
	UBSAN_OPTIONS=halt_on_error=1 ./build/test-native-budget
test-all: test-run-native-string-cache test-run-native-budget

build/mutable-port/stft.cc: tools/mutable/windowed_stft.py third_party/mutable/clouds/dsp/pvoc/stft.cc src/native_window.h
	python3 tools/mutable/windowed_stft.py third_party/mutable $@
build/plaits/clouds/dsp/pvoc/stft.o: build/mutable-port/stft.cc
	$(CXX) $(HOST_FLAGS) $(SAN) -g -O1 -MMD -MP -c $< -o $@
build/test-native-window: tests/native_window_test.cpp src/native_window.h $(UI_BASE_ARCHIVE)
	$(CXX) $(HOST_FLAGS) $(SAN) -O2 $< -o $@ $(UI_BASE_ARCHIVE)
.PHONY: test-run-native-window
test-run-native-window: build/test-native-window
	UBSAN_OPTIONS=halt_on_error=1 ./build/test-native-window
test-all: test-run-native-window

build/test-stream-cache: tests/stream_cache_test.cpp $(DEMO_HEADERS) $(UI_BASE_ARCHIVE)
	$(CXX) $(HOST_FLAGS) $(SAN) -O1 -Wall -Wextra -Werror $< -o $@ $(UI_BASE_ARCHIVE)
test-run-stream-cache: build/test-stream-cache
	UBSAN_OPTIONS=halt_on_error=1 ./build/test-stream-cache
test-all: test-run-stream-cache

build/test-wave5-import-ui.o: tests/wave5_import_ui_test.cpp $(DEMO_HEADERS)
	$(CXX) $(HOST_FLAGS) -O0 -Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-variable -Wno-unused-local-typedefs $(SAN) -c $< -o $@
build/test-wave5-import-ui: build/test-wave5-import-ui.o build/test-core-engine.o $(HOST_SUPPORT_OBJECTS) $(PL_OBJECTS) $(UI_BASE_ARCHIVE)
	$(CXX) $(HOST_FLAGS) $(SAN) $^ -o $@ $(UI_BASE_ARCHIVE)
.PHONY: test-run-wave5-import-ui
test-run-wave5-import-ui: build/test-wave5-import-ui
	@mkdir -p build/test-run/wave5-import-ui
	@cd build/test-run/wave5-import-ui && $(TEST_RUN_SETUP) && UBSAN_OPTIONS=halt_on_error=1 $(abspath build/test-wave5-import-ui)
test-all: test-run-wave5-import-ui

build/test-wave5-review.o: tests/wave5_review_test.cpp $(DEMO_HEADERS)
	$(CXX) $(HOST_FLAGS) -O0 -Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-variable -Wno-unused-local-typedefs $(SAN) -c $< -o $@
build/test-wave5-review: build/test-wave5-review.o build/test-core-engine.o $(HOST_SUPPORT_OBJECTS) $(PL_OBJECTS) $(UI_BASE_ARCHIVE)
	$(CXX) $(HOST_FLAGS) $(SAN) $^ -o $@ $(UI_BASE_ARCHIVE)
.PHONY: test-run-wave5-review
test-run-wave5-review: build/test-wave5-review
	@mkdir -p build/test-run/wave5-review
	@cd build/test-run/wave5-review && $(TEST_RUN_SETUP) && UBSAN_OPTIONS=halt_on_error=1 $(abspath build/test-wave5-review)
test-all: test-run-wave5-review

# These UI regressions must use the current shared declarations, not a cached
# executable from before a controls/modal-only edit.
build/test-backends.o build/test-page-nav build/test-source-select build/test-browser-file-ops.o: $(DEMO_HEADERS)

build/test-midi-import-job.o: tests/midi_import_job_test.cpp $(DEMO_HEADERS)
	$(CXX) $(HOST_FLAGS) -O0 -Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-variable -Wno-unused-local-typedefs $(SAN) -c $< -o $@
build/test-midi-import-job: build/test-midi-import-job.o build/test-core-engine.o $(HOST_SUPPORT_OBJECTS) $(PL_OBJECTS) $(UI_BASE_ARCHIVE)
	$(CXX) $(HOST_FLAGS) $(SAN) $^ -o $@ $(UI_BASE_ARCHIVE)
.PHONY: test-run-midi-import-job
test-run-midi-import-job: build/test-midi-import-job
	@mkdir -p build/test-run/midi-import-job
	@cd build/test-run/midi-import-job && $(TEST_RUN_SETUP) && UBSAN_OPTIONS=halt_on_error=1 $(abspath build/test-midi-import-job)
test-all: test-run-midi-import-job

build/test-ui-type.o: tests/ui_type_test.cpp $(DEMO_HEADERS)
	$(CXX) $(HOST_FLAGS) -O0 -Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-variable -Wno-unused-local-typedefs $(SAN) -c $< -o $@
build/test-ui-type: build/test-ui-type.o build/test-core-engine.o $(HOST_SUPPORT_OBJECTS) $(PL_OBJECTS) $(UI_BASE_ARCHIVE)
	$(CXX) $(HOST_FLAGS) $(SAN) $^ -o $@ $(UI_BASE_ARCHIVE)
test-run-ui-type: build/test-ui-type
	UBSAN_OPTIONS=halt_on_error=1 ./build/test-ui-type
test-all: test-run-ui-type
# Keep frontend acceptance binaries tied to the new shared atlas and layout.
build/test-theme build/test-page-descriptors build/test-partial-redraw: $(DEMO_HEADERS)

build/test-ui-icons.o: tests/ui_icons_test.cpp $(DEMO_HEADERS)
	$(CXX) $(HOST_FLAGS) -O0 -Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-variable -Wno-unused-local-typedefs $(SAN) -c $< -o $@
build/test-ui-icons: build/test-ui-icons.o build/test-core-engine.o $(HOST_SUPPORT_OBJECTS) $(PL_OBJECTS) $(UI_BASE_ARCHIVE)
	$(CXX) $(HOST_FLAGS) $(SAN) $^ -o $@ $(UI_BASE_ARCHIVE)
test-run-ui-icons: build/test-ui-icons
	UBSAN_OPTIONS=halt_on_error=1 ./build/test-ui-icons
test-all: test-run-ui-icons

# Wave 8 memory-only live analysis and shared visual/content acceptance.
build/test-visual-analysis.o: tests/visual_analysis_test.cpp $(DEMO_HEADERS)
	$(CXX) $(HOST_FLAGS) -O0 -Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-variable -Wno-unused-local-typedefs $(SAN) -c $< -o $@
build/test-visual-analysis: build/test-visual-analysis.o build/test-core-engine.o $(HOST_SUPPORT_OBJECTS) $(PL_OBJECTS) $(UI_BASE_ARCHIVE)
	$(CXX) $(HOST_FLAGS) $(SAN) $^ -o $@ $(UI_BASE_ARCHIVE)
test-run-visual-analysis: build/test-visual-analysis
	UBSAN_OPTIONS=halt_on_error=1 ./build/test-visual-analysis
build/test-ui-visuals.o: tests/ui_visuals_test.cpp $(DEMO_HEADERS)
	$(CXX) $(HOST_FLAGS) -O0 -Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-variable -Wno-unused-local-typedefs $(SAN) -c $< -o $@
build/test-ui-visuals: build/test-ui-visuals.o build/test-core-engine.o $(HOST_SUPPORT_OBJECTS) $(PL_OBJECTS) $(UI_BASE_ARCHIVE)
	$(CXX) $(HOST_FLAGS) $(SAN) $^ -o $@ $(UI_BASE_ARCHIVE)
test-run-ui-visuals: build/test-ui-visuals
	UBSAN_OPTIONS=halt_on_error=1 ./build/test-ui-visuals
test-all: test-run-visual-analysis test-run-ui-visuals

.PHONY: test-run-py-circle_request_reuse_test
test-run-py-circle_request_reuse_test: build/update-sim
	python3 tests/circle_request_reuse_test.py

test-all: test-run-py-circle_request_reuse_test


build/test-dsp-profile.o: tests/dsp_profile_test.cpp $(DEMO_HEADERS)
	$(CXX) $(HOST_FLAGS) -O1 -g $(SAN) -Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-variable -Wno-unused-local-typedefs -MMD -MP -c $< -o $@
build/test-dsp-profile: build/test-dsp-profile.o build/test-core-engine.o $(HOST_SUPPORT_OBJECTS) $(PL_OBJECTS) $(UI_BASE_ARCHIVE)
	$(CXX) $(HOST_FLAGS) -O1 -g $(SAN) $^ -o $@ $(UI_BASE_ARCHIVE)
.PHONY: test-run-dsp-profile
test-run-dsp-profile: build/test-dsp-profile
	@mkdir -p build/test-run/dsp-profile
	@cd build/test-run/dsp-profile && UBSAN_OPTIONS=halt_on_error=1 $(abspath build/test-dsp-profile)
test-all: test-run-dsp-profile

# Current-generation recovery and deterministic storage fault injection.
build/test-current-save-recovery: tests/current_save_recovery_test.cpp $(wildcard src/*.h) $(UI_BASE_ARCHIVE)
	$(CXX) -std=c++17 -Isrc -O1 $(SAN) -Wall -Wextra -Werror $< -o $@ $(UI_BASE_ARCHIVE)
test-run-current-save-recovery: build/test-current-save-recovery
	UBSAN_OPTIONS=halt_on_error=1 ./build/test-current-save-recovery
test-all: test-run-current-save-recovery

build/test-project-ownership: tests/project_ownership_test.cpp $(wildcard src/*.h) $(UI_BASE_ARCHIVE)
	$(CXX) -std=c++17 -Isrc -O1 $(SAN) -Wall -Wextra -Werror $< -o $@ $(UI_BASE_ARCHIVE)
test-run-project-ownership: build/test-project-ownership
	UBSAN_OPTIONS=halt_on_error=1 ./build/test-project-ownership
test-all: test-run-project-ownership

.PHONY: test-run-current-save-recovery test-run-project-ownership test-run-async-project-save test-run-async-project-read test-run-py-host_build_graph_test
test-run-py-host_build_graph_test:
	python3 tests/update/host_build_graph_test.py
test-all: test-run-py-host_build_graph_test

build/test-content-boundaries: tests/content_boundaries_test.cpp $(wildcard src/*.h) build/host-support/src/soundfont.o $(UI_BASE_ARCHIVE)
	$(CXX) $(HOST_FLAGS) -O1 $(SAN) -Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-variable $< build/host-support/src/soundfont.o -o $@ $(UI_BASE_ARCHIVE)
test-run-content-boundaries: build/test-content-boundaries
	UBSAN_OPTIONS=halt_on_error=1 ./build/test-content-boundaries
VOICE_BOUNDARY_OBJECTS := build/host-support/src/wavetable_bank.o build/host-support/third_party/daisysp/Source/Filters/ladder.o build/host-support/third_party/daisysp/Source/Control/adsr.o build/host-support/third_party/daisysp/Source/Synthesis/oscillator.o build/host-support/third_party/daisysp/Source/Filters/svf.o
build/test-voice-boundaries: tests/voice_boundaries_test.cpp $(wildcard src/*.h) $(VOICE_BOUNDARY_OBJECTS) $(UI_BASE_ARCHIVE)
	$(CXX) $(HOST_FLAGS) -O1 $(SAN) -Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-variable $< $(VOICE_BOUNDARY_OBJECTS) -o $@ $(UI_BASE_ARCHIVE)
test-run-voice-boundaries: build/test-voice-boundaries
	UBSAN_OPTIONS=halt_on_error=1 ./build/test-voice-boundaries
build/test-workflow-interruptions: tests/workflow_interruptions_test.cpp build/test-core-engine.o $(HOST_SUPPORT_OBJECTS) $(PL_OBJECTS) $(DEMO_HEADERS) $(UI_BASE_ARCHIVE)
	$(CXX) $(HOST_FLAGS) -O1 $(SAN) -Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-variable -Wno-unused-local-typedefs $< build/test-core-engine.o $(HOST_SUPPORT_OBJECTS) $(PL_OBJECTS) -o $@ $(UI_BASE_ARCHIVE)
test-run-workflow-interruptions: build/test-workflow-interruptions
	UBSAN_OPTIONS=halt_on_error=1 ./build/test-workflow-interruptions
.PHONY: test-run-content-boundaries test-run-voice-boundaries test-run-workflow-interruptions
test-all: test-run-content-boundaries test-run-voice-boundaries test-run-workflow-interruptions

# All maintained C++ fixtures participate in the same inventory.
build/test-sample-analysis-slot: tests/sample_analysis_slot_test.cpp $(DEMO_HEADERS) $(UI_BASE_ARCHIVE)
	$(CXX) -std=c++17 -O2 -Wall -Wextra -Werror $(SAN) -pthread -Isrc tests/sample_analysis_slot_test.cpp -o build/test-sample-analysis-slot $(UI_BASE_ARCHIVE)
.PHONY: test-run-sample-analysis-slot
test-run-sample-analysis-slot: build/test-sample-analysis-slot
	UBSAN_OPTIONS=halt_on_error=1 ./build/test-sample-analysis-slot
test-all: test-run-sample-analysis-slot

build/test-live-fx-lock: tests/live_fx_lock_test.cpp $(DEMO_HEADERS) build/test-core-engine.o $(HOST_SUPPORT_OBJECTS) $(PL_OBJECTS) $(UI_BASE_ARCHIVE)
	$(CXX) $(HOST_FLAGS) -O1 -Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-variable -Wno-unused-local-typedefs $(SAN) tests/live_fx_lock_test.cpp build/test-core-engine.o $(HOST_SUPPORT_OBJECTS) $(PL_OBJECTS) -o build/test-live-fx-lock $(UI_BASE_ARCHIVE)
.PHONY: test-run-live-fx-lock
test-run-live-fx-lock: build/test-live-fx-lock
	UBSAN_OPTIONS=halt_on_error=1 ./build/test-live-fx-lock
test-all: test-run-live-fx-lock

build/test-live-fx-codec: tests/live_fx_codec_test.cpp $(DEMO_HEADERS) $(UI_BASE_ARCHIVE)
	$(CXX) $(SAN) -std=c++17 -O2 -Isrc -Wall -Wextra -Werror tests/live_fx_codec_test.cpp -o build/test-live-fx-codec $(UI_BASE_ARCHIVE)
.PHONY: test-run-live-fx-codec
test-run-live-fx-codec: build/test-live-fx-codec
	./build/test-live-fx-codec
test-all: test-run-live-fx-codec

build/test-macro-curves: tests/macro_curves_test.cpp $(DEMO_HEADERS) $(UI_BASE_ARCHIVE)
	$(CXX) $(SAN) -std=c++17 -O2 -Isrc -Wall -Wextra -Werror tests/macro_curves_test.cpp -o build/test-macro-curves $(UI_BASE_ARCHIVE)
.PHONY: test-run-macro-curves
test-run-macro-curves: build/test-macro-curves
	./build/test-macro-curves
test-all: test-run-macro-curves

build/test-project-io-buffer: tests/project_io_buffer_test.cpp $(DEMO_HEADERS) $(UI_BASE_ARCHIVE)
	$(CXX) $(HOST_FLAGS) -O2 -Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-variable -Wno-unused-local-typedefs $(SAN) tests/project_io_buffer_test.cpp -o build/test-project-io-buffer $(UI_BASE_ARCHIVE)
.PHONY: test-run-project-io-buffer
test-run-project-io-buffer: build/test-project-io-buffer
	UBSAN_OPTIONS=halt_on_error=1 ./build/test-project-io-buffer
test-all: test-run-project-io-buffer

build/test-kit-asset-verifier: tests/kit_asset_verifier_test.cpp $(DEMO_HEADERS) $(UI_BASE_ARCHIVE)
	$(CXX) -std=c++17 -O2 -Isrc -Wall -Wextra -Werror $(SAN) tests/kit_asset_verifier_test.cpp -o build/test-kit-asset-verifier $(UI_BASE_ARCHIVE)
.PHONY: test-run-kit-asset-verifier
test-run-kit-asset-verifier: build/test-kit-asset-verifier
	UBSAN_OPTIONS=halt_on_error=1 ./build/test-kit-asset-verifier
test-all: test-run-kit-asset-verifier

build/test-chunked-sample-read: tests/chunked_sample_read_test.cpp $(DEMO_HEADERS) $(UI_BASE_ARCHIVE)
	$(CXX) -std=c++17 -O2 -Isrc -Wall -Wextra -Werror $(SAN) tests/chunked_sample_read_test.cpp -o build/test-chunked-sample-read $(UI_BASE_ARCHIVE)
.PHONY: test-run-chunked-sample-read
test-run-chunked-sample-read: build/test-chunked-sample-read
	UBSAN_OPTIONS=halt_on_error=1 ./build/test-chunked-sample-read
test-all: test-run-chunked-sample-read

.PHONY: test-run-project-codec-stack
test-run-project-codec-stack: build/test-project-codec-stack
	UBSAN_OPTIONS=halt_on_error=1 ./build/test-project-codec-stack
test-all: test-run-project-codec-stack


# Screen semantics and physical control grammar share the live controller.
build/test-ui-grammar: tests/ui_grammar_test.cpp $(DEMO_HEADERS) build/test-core-engine.o $(HOST_SUPPORT_OBJECTS) $(PL_OBJECTS) $(UI_BASE_ARCHIVE)
	$(CXX) $(HOST_FLAGS) -O1 -Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-variable -Wno-unused-local-typedefs $(SAN) $< build/test-core-engine.o $(HOST_SUPPORT_OBJECTS) $(PL_OBJECTS) -o $@ $(UI_BASE_ARCHIVE)
.PHONY: test-run-ui-grammar
test-run-ui-grammar: build/test-ui-grammar
	UBSAN_OPTIONS=halt_on_error=1 ./build/test-ui-grammar
test-all: test-run-ui-grammar

build/test-wave12-input.o: tests/wave12_input_test.cpp $(DEMO_HEADERS)
	$(CXX) $(HOST_FLAGS) $(SAN) -O1 -Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-variable -Wno-unused-local-typedefs -c $< -o $@
build/test-wave12-input: build/test-wave12-input.o build/test-core-engine.o $(HOST_SUPPORT_OBJECTS) $(PL_OBJECTS) $(UI_BASE_ARCHIVE)
	$(CXX) $(HOST_FLAGS) $(SAN) $^ -o $@ $(UI_BASE_ARCHIVE)
.PHONY: test-run-wave12-input
test-run-wave12-input: build/test-wave12-input
	UBSAN_OPTIONS=halt_on_error=1 ./build/test-wave12-input
test-all: test-run-wave12-input

.PHONY: canonical-sample
canonical-sample:
	python3 tools/content/canonical_sample.py
simulator session-demo workflow-demo readiness-demo creative-demo factory-content: canonical-sample
build/canonical-catalog: tools/content/canonical_catalog.cpp $(wildcard src/*.h src/*.inc) $(UI_TOOL_ARCHIVE)
	@mkdir -p build
	$(CXX) -std=c++17 -O2 -Isrc -Wall -Wextra -Werror $< -o $@ $(UI_TOOL_ARCHIVE)
.PHONY: canonical-catalog
canonical-catalog: build/canonical-catalog factory-catalog
	./build/canonical-catalog build/factory-data
	python3 tools/content/factory_library_assets.py build/factory-data

# The same file-service core used by the firmware, including HTTP requests.
build/test-user-files: tests/user_files_test.cpp tests/file_transfer_fixtures.h $(DEMO_HEADERS) $(UI_BASE_ARCHIVE)
	$(CXX) -std=c++17 -O1 -Isrc -Wall -Wextra -Werror $(SAN) $< -o $@ $(UI_BASE_ARCHIVE)
.PHONY: test-run-user-files
test-run-user-files: build/test-user-files
	UBSAN_OPTIONS=halt_on_error=1 ./build/test-user-files
test-all: test-run-user-files

build/file-web-server: tools/testing/file_web_server.cpp tests/file_transfer_fixtures.h $(DEMO_HEADERS) $(UI_BASE_ARCHIVE)
	$(CXX) -std=c++17 -O1 -Isrc -Wall -Wextra -Werror $(SAN) $< -o $@ $(UI_BASE_ARCHIVE)
.PHONY: test-run-file-web-http
test-run-file-web-http: build/file-web-server
	python3 $(abspath tests/file_web_http_test.py)
test-all: test-run-file-web-http

build/test-file-bulk: tests/file_bulk_test.cpp tests/file_transfer_fixtures.h $(DEMO_HEADERS) $(UI_BASE_ARCHIVE)
	$(CXX) -std=c++17 -O1 -Isrc -Wall -Wextra -Werror $(SAN) $< -o $@ $(UI_BASE_ARCHIVE)
.PHONY: test-run-file-bulk test-run-usb-files-cli
test-run-file-bulk: build/test-file-bulk
	UBSAN_OPTIONS=halt_on_error=1 ./build/test-file-bulk
test-run-usb-files-cli: build/test-file-bulk
	python3 tests/usb_files_cli_test.py
test-all: test-run-file-bulk test-run-usb-files-cli

.PHONY: test-run-py-circle-gadget-cache
test-run-py-circle-gadget-cache:
	python3 tests/circle_gadget_cache_test.py
test-all: test-run-py-circle-gadget-cache

.PHONY: factory-boot-assets-check test-run-factory-boot-gate test-run-boot-splash
factory-boot-assets-check: factory-catalog
	python3 tools/content/factory_boot_manifest.py
	python3 tools/ui/boot_splash.py --check
build/test-factory-boot-gate: tests/factory_boot_gate_test.cpp src/factory_boot_gate.h src/generated/factory_catalog.inc $(wildcard src/*.h src/*.inc) $(UI_BASE_ARCHIVE)
	$(CXX) -std=c++17 -O1 -Isrc -Wall -Wextra -Werror $(SAN) $< -o $@ $(UI_BASE_ARCHIVE)
test-run-factory-boot-gate: factory-boot-assets-check build/test-factory-boot-gate
	UBSAN_OPTIONS=halt_on_error=1 ./build/test-factory-boot-gate $(abspath assets/samples/demo-slices.wav)
test-run-boot-splash:
	python3 tests/boot_splash_test.py
test-all: test-run-factory-boot-gate test-run-boot-splash

# Catalog fingerprints must follow nested codec/state headers as well.
-include build/factory-catalog.d

.PHONY: test-run-project-reset-stack
build/test-project-reset-stack: tests/project_reset_stack_test.cpp src/project.h $(wildcard src/*.h src/*.inc) $(UI_TOOL_ARCHIVE)
	$(CXX) -std=c++17 -O2 -g -pthread -Wall -Wextra -Werror -Wno-unused-parameter -Isrc $< -o $@ $(UI_TOOL_ARCHIVE)
test-run-project-reset-stack: build/test-project-reset-stack
	./build/test-project-reset-stack
test-all: test-run-project-reset-stack

build/test-insights-rework.o: tests/insights_rework_test.cpp $(DEMO_HEADERS)
	$(CXX) $(HOST_FLAGS) $(SAN) -O1 -MMD -MP -Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-variable -Wno-unused-local-typedefs -c $< -o $@
build/test-insights-rework: build/test-insights-rework.o build/test-core-engine.o $(HOST_SUPPORT_OBJECTS) $(PL_OBJECTS) $(UI_BASE_ARCHIVE)
	$(CXX) $(SAN) $^ -o $@ $(UI_BASE_ARCHIVE)
.PHONY: test-run-insights-rework
test-run-insights-rework: build/test-insights-rework
	UBSAN_OPTIONS=halt_on_error=1 ./build/test-insights-rework $(abspath build/insights-led-palette.json)
test-all: test-run-insights-rework

build/test-formant-engine: tests/formant_engine_test.cpp build/test-core-engine.o $(DEMO_HEADERS) $(HOST_SUPPORT_OBJECTS) $(PL_OBJECTS) $(UI_BASE_ARCHIVE)
	$(CXX) $(HOST_FLAGS) $(SAN) -O1 -Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-variable -Wno-unused-local-typedefs $< build/test-core-engine.o $(HOST_SUPPORT_OBJECTS) $(PL_OBJECTS) -o $@ $(UI_BASE_ARCHIVE)
test-run-formant-engine: build/test-formant-engine
	UBSAN_OPTIONS=halt_on_error=1 ./build/test-formant-engine
test-all: test-run-formant-engine

build/test-pitch-shift: tests/pitch_shift_test.cpp $(DEMO_HEADERS) $(HOST_SUPPORT_OBJECTS) $(PL_OBJECTS) $(UI_BASE_ARCHIVE)
	$(CXX) $(HOST_FLAGS) $(SAN) -O1 -Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-variable -Wno-unused-local-typedefs $< $(HOST_SUPPORT_OBJECTS) $(PL_OBJECTS) -o $@ $(UI_BASE_ARCHIVE)
test-run-pitch-shift: build/test-pitch-shift
	UBSAN_OPTIONS=halt_on_error=1 ./build/test-pitch-shift
test-all: test-run-pitch-shift

build/test-preset-fx: tests/preset_fx_test.cpp build/test-core-engine.o $(HOST_SUPPORT_OBJECTS) $(PL_OBJECTS) $(DEMO_HEADERS) $(UI_BASE_ARCHIVE)
	$(CXX) $(HOST_FLAGS) $(SAN) -O0 -Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-variable -Wno-unused-local-typedefs $< build/test-core-engine.o $(HOST_SUPPORT_OBJECTS) $(PL_OBJECTS) -o $@ $(UI_BASE_ARCHIVE)
.PHONY: test-run-preset-fx
test-run-preset-fx: build/test-preset-fx
	UBSAN_OPTIONS=halt_on_error=1 ./build/test-preset-fx
test-all: test-run-preset-fx

build/test-va-extensions: tests/va_extensions_test.cpp $(DEMO_HEADERS) $(HOST_SUPPORT_OBJECTS) $(PL_OBJECTS) $(UI_BASE_ARCHIVE)
	$(CXX) $(HOST_FLAGS) $(SAN) -O1 -Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-variable -Wno-unused-local-typedefs $< $(HOST_SUPPORT_OBJECTS) $(PL_OBJECTS) -o $@ $(UI_BASE_ARCHIVE)
test-run-va-extensions: build/test-va-extensions
	UBSAN_OPTIONS=halt_on_error=1 ./build/test-va-extensions
test-all: test-run-va-extensions

build/test-rotary: tests/rotary_test.cpp $(DEMO_HEADERS) $(HOST_SUPPORT_OBJECTS) $(PL_OBJECTS) $(UI_BASE_ARCHIVE)
	$(CXX) $(HOST_FLAGS) $(SAN) -O1 -Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-variable -Wno-unused-local-typedefs $< $(HOST_SUPPORT_OBJECTS) $(PL_OBJECTS) -o $@ $(UI_BASE_ARCHIVE)
test-run-rotary: build/test-rotary
	UBSAN_OPTIONS=halt_on_error=1 ./build/test-rotary
test-all: test-run-rotary

build/test-granular-engine: tests/granular_engine_test.cpp build/test-core-engine.o $(HOST_SUPPORT_OBJECTS) $(PL_OBJECTS) $(DEMO_HEADERS) $(UI_BASE_ARCHIVE)
	$(CXX) $(HOST_FLAGS) $(SAN) -O0 -Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-variable -Wno-unused-local-typedefs $< build/test-core-engine.o $(HOST_SUPPORT_OBJECTS) $(PL_OBJECTS) -o $@ $(UI_BASE_ARCHIVE)
.PHONY: test-run-granular-engine
test-run-granular-engine: build/test-granular-engine
	UBSAN_OPTIONS=halt_on_error=1 ./build/test-granular-engine
test-all: test-run-granular-engine

build/test-step-attributes: tests/step_attributes_test.cpp build/test-core-engine.o $(DEMO_HEADERS) $(HOST_SUPPORT_OBJECTS) $(PL_OBJECTS) $(UI_BASE_ARCHIVE)
	$(CXX) $(HOST_FLAGS) $(SAN) -O1 -Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-variable -Wno-unused-local-typedefs $< build/test-core-engine.o $(HOST_SUPPORT_OBJECTS) $(PL_OBJECTS) -o $@ $(UI_BASE_ARCHIVE)
test-run-step-attributes: build/test-step-attributes
	UBSAN_OPTIONS=halt_on_error=1 ./build/test-step-attributes
test-all: test-run-step-attributes

build/test-optimization-contract: tests/optimization_contract_test.cpp $(DEMO_HEADERS) $(UI_BASE_ARCHIVE)
	$(CXX) $(HOST_FLAGS) $(SAN) -O1 -Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-variable -Wno-unused-local-typedefs $< -o $@ $(UI_BASE_ARCHIVE)
.PHONY: test-run-optimization-contract
test-run-optimization-contract: build/test-optimization-contract
	UBSAN_OPTIONS=halt_on_error=1 ./build/test-optimization-contract
test-all: test-run-optimization-contract

build/ui-base.a: build/engine-core/software_canvas.o build/engine-core/software_widgets.o build/engine-core/state_defaults.o
	$(AR) rcs $@ $^

# Optimized offline tools must not inherit ASan/UBSan runtime dependencies.
build/ui-tool/%.o: src/%.cpp Makefile
	@mkdir -p $(@D)
	$(CXX) $(HOST_FLAGS) -O2 -MMD -MP -Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-variable -Wno-unused-local-typedefs -c $< -o $@
build/ui-tool.a: build/ui-tool/software_canvas.o build/ui-tool/software_widgets.o build/ui-tool/state_defaults.o
	$(AR) rcs $@ $^
-include $(wildcard build/ui-tool/*.d)

-include build/test-display-rectangle.d build/test-display-job-slot.d

# One explicitly authorized endpoint fix; preserve vendor files and provenance.
build/mutable-port/rings/dsp/resonator.cc: tools/mutable/bounded_resonator.py third_party/mutable/rings/dsp/resonator.cc third_party/mutable/resonator-texture-sources.json
	python3 tools/mutable/bounded_resonator.py third_party/mutable $@
build/plaits/rings/dsp/resonator.o: build/mutable-port/rings/dsp/resonator.cc src/resonator_boundary.h
	$(CXX) $(HOST_FLAGS) $(SAN) -g -O1 -MMD -MP -c $< -o $@
build/fx-profile/rings/dsp/resonator.o: build/mutable-port/rings/dsp/resonator.cc src/resonator_boundary.h
	$(CXX) $(HOST_FLAGS) -O2 -g -pg -c $< -o $@
build/reference-resonator.o: third_party/mutable/rings/dsp/resonator.cc Makefile
	$(CXX) $(HOST_FLAGS) $(SAN) -O1 -DResonator=ReferenceResonator -DkMaxModes=kReferenceMaxModes -c $< -o $@
build/test-resonator-boundary: tests/resonator_boundary_test.cpp build/plaits/rings/dsp/resonator.o build/reference-resonator.o build/plaits/rings/resources.o
	$(CXX) $(HOST_FLAGS) $(SAN) -O1 -Wall -Wextra -Werror $^ -o $@
.PHONY: test-run-resonator-boundary
test-run-resonator-boundary: build/test-resonator-boundary
	UBSAN_OPTIONS=halt_on_error=1 ./build/test-resonator-boundary
test-all: test-run-resonator-boundary

build/test-display-coalesce: tests/display_coalesce_test.cpp src/display_job_slot.h src/display_rectangle.h src/display_rle.h src/display_wire_facts.h src/display_probe_packet.h
	$(CXX) $(HOST_FLAGS) $(SAN) -O1 -Wall -Wextra -Werror $< -o $@
.PHONY: test-run-display-coalesce
test-run-display-coalesce: build/test-display-coalesce
	UBSAN_OPTIONS=halt_on_error=1 ./build/test-display-coalesce
test-all: test-run-display-coalesce

build/test-va-trigger-bank: tests/va_trigger_bank_test.cpp src/virtual_analog_voice.h $(HOST_SUPPORT_OBJECTS)
	$(CXX) $(HOST_FLAGS) $(SAN) -O1 -Wall -Wextra -Werror $< $(filter %/ladder.o %/svf.o %/adsr.o %/oscillator.o,$(HOST_SUPPORT_OBJECTS)) -o $@
.PHONY: test-run-va-trigger-bank
test-run-va-trigger-bank: build/test-va-trigger-bank
	UBSAN_OPTIONS=halt_on_error=1 ./build/test-va-trigger-bank
test-all: test-run-va-trigger-bank

build/test-visual-plain-equivalence: tests/visual_plain_equivalence_test.cpp build/test-core-engine.o $(HOST_SUPPORT_OBJECTS) $(PL_OBJECTS) $(UI_BASE_ARCHIVE)
	$(CXX) $(HOST_FLAGS) $(SAN) -O1 -Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-variable -Wno-unused-local-typedefs $< build/test-core-engine.o $(HOST_SUPPORT_OBJECTS) $(PL_OBJECTS) -o $@ $(UI_BASE_ARCHIVE)
.PHONY: test-run-visual-plain-equivalence
test-run-visual-plain-equivalence: build/test-visual-plain-equivalence
	UBSAN_OPTIONS=halt_on_error=1 ./build/test-visual-plain-equivalence
test-all: test-run-visual-plain-equivalence

# Causal timing ledgers: bounded control reservation, audio-owned publication.
build/test-runtime-attribution: tests/runtime_attribution_test.cpp src/render_audit.h src/audio_slow_trace.h src/display_attribution.h src/usb_progress.h src/display_frame_gate.h
	$(CXX) $(HOST_FLAGS) $(SAN) -O1 -Wall -Wextra -Werror $< -o $@
test-run-runtime-attribution: build/test-runtime-attribution
	UBSAN_OPTIONS=halt_on_error=1 ./build/test-runtime-attribution
test-all: test-run-runtime-attribution

# User-authored factory masters use the current codec; GBL1 is ROM-only packing.
.PHONY: factory-library-pack-check test-run-factory-library
factory-library-pack-check:
	python3 tools/content/factory_library_pack.py
	python3 tools/content/factory_library_bank.py
factory-catalog: factory-library-pack-check
build/factory-catalog build/test-factory-content build/test-factory-boot-gate build/test-factory-library-audit build/test-bake-preset: src/generated/factory_library.inc src/generated/factory_library_bank.inc
build/import-factory-library: tools/content/import_factory_library.cpp Makefile $(UI_TOOL_ARCHIVE)
	$(CXX) -std=c++17 -O2 -Isrc -Wall -Wextra -Werror -MMD -MP $< -o $@ $(UI_TOOL_ARCHIVE)
-include build/import-factory-library.d
build/test-factory-library: tests/factory_library_test.cpp src/factory_library.h src/generated/factory_library.inc src/generated/factory_library_bank.inc $(UI_BASE_ARCHIVE)
	$(CXX) -std=c++17 -O1 -Isrc -Wall -Wextra -Werror $(SAN) $< -o $@ $(UI_BASE_ARCHIVE)
test-run-factory-library: build/test-factory-library
	UBSAN_OPTIONS=halt_on_error=1 ./build/test-factory-library $(abspath assets/factory-library)
test-all: test-run-factory-library

build/test-factory-library-audit: tests/factory_library_audit_test.cpp build/test-core-engine.o $(HOST_SUPPORT_OBJECTS) $(PL_OBJECTS) $(DEMO_HEADERS) $(UI_BASE_ARCHIVE)
	$(CXX) $(HOST_FLAGS) $(SAN) -O0 -Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-variable -Wno-unused-local-typedefs $< build/test-core-engine.o $(HOST_SUPPORT_OBJECTS) $(PL_OBJECTS) -o $@ $(UI_BASE_ARCHIVE)
.PHONY: test-run-factory-library-audit
test-run-factory-library-audit: build/test-factory-library-audit
	UBSAN_OPTIONS=halt_on_error=1 ./build/test-factory-library-audit $(abspath assets/samples/demo-slices.wav)
test-all: test-run-factory-library-audit

build/test-track-kind-colour: tests/track_kind_colour_test.cpp build/test-core-engine.o $(HOST_SUPPORT_OBJECTS) $(PL_OBJECTS) $(DEMO_HEADERS) $(UI_BASE_ARCHIVE)
	$(CXX) $(HOST_FLAGS) $(SAN) -O1 -Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-variable -Wno-unused-local-typedefs $< build/test-core-engine.o $(HOST_SUPPORT_OBJECTS) $(PL_OBJECTS) -o $@ $(UI_BASE_ARCHIVE)
test-run-track-kind-colour: build/test-track-kind-colour
	UBSAN_OPTIONS=halt_on_error=1 ./build/test-track-kind-colour
test-all: test-run-track-kind-colour

build/test-preset-v12-codec: tests/preset_v12_codec_test.cpp src/project_codec.h src/preset_identity.h $(UI_BASE_ARCHIVE)
	$(CXX) $(HOST_FLAGS) $(SAN) -O1 -Wall -Wextra -Werror $< -o $@ $(UI_BASE_ARCHIVE)
test-run-preset-v12-codec: build/test-preset-v12-codec
	UBSAN_OPTIONS=halt_on_error=1 ./build/test-preset-v12-codec
test-all: test-run-preset-v12-codec

build/test-content-shelf: tests/content_shelf_test.cpp src/content_shelf.h $(UI_BASE_ARCHIVE)
	$(CXX) $(HOST_FLAGS) $(SAN) -O1 -Wall -Wextra -Werror $< -o $@ $(UI_BASE_ARCHIVE)
test-run-content-shelf: build/test-content-shelf
	UBSAN_OPTIONS=halt_on_error=1 ./build/test-content-shelf
test-all: test-run-content-shelf

build/test-preset-lifecycle: tests/preset_lifecycle_test.cpp build/test-core-engine.o $(HOST_SUPPORT_OBJECTS) $(PL_OBJECTS) $(DEMO_HEADERS) $(UI_BASE_ARCHIVE)
	$(CXX) $(HOST_FLAGS) $(SAN) -O0 -Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-variable -Wno-unused-local-typedefs $< build/test-core-engine.o $(HOST_SUPPORT_OBJECTS) $(PL_OBJECTS) -o $@ $(UI_BASE_ARCHIVE)
test-run-preset-lifecycle: build/test-preset-lifecycle
	UBSAN_OPTIONS=halt_on_error=1 ./build/test-preset-lifecycle
test-all: test-run-preset-lifecycle

# Offline-authored synthesis percussion. Runtime uses the existing boot-only
# lossless pack restore; no synth rendering during factory installation.
build/synth-drums: tools/content/synth_drums.cpp src/synth_drum_recipes.h build/test-core-engine.o $(HOST_SUPPORT_OBJECTS) $(PL_OBJECTS) $(UI_BASE_ARCHIVE)
	$(CXX) $(HOST_FLAGS) $(SAN) -O1 -Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-variable -Wno-unused-local-typedefs $< build/test-core-engine.o $(HOST_SUPPORT_OBJECTS) $(PL_OBJECTS) -o $@ $(UI_BASE_ARCHIVE)

build/test-retired-engines: tests/retired_engines_test.cpp build/test-core-engine.o $(HOST_SUPPORT_OBJECTS) $(PL_OBJECTS) $(DEMO_HEADERS) $(UI_BASE_ARCHIVE)
	$(CXX) $(HOST_FLAGS) $(SAN) -O0 -Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-variable -Wno-unused-local-typedefs $< build/test-core-engine.o $(HOST_SUPPORT_OBJECTS) $(PL_OBJECTS) -o $@ $(UI_BASE_ARCHIVE)
.PHONY: test-run-retired-engines
test-run-retired-engines: build/test-retired-engines
	UBSAN_OPTIONS=halt_on_error=1 ./build/test-retired-engines
test-all: test-run-retired-engines

.PHONY: factory-synth-drums-pack-check
factory-synth-drums-pack-check:
	python3 tools/content/factory_synth_drums_pack.py
factory-catalog: factory-synth-drums-pack-check
build/factory-catalog build/test-factory-content build/test-factory-boot-gate: src/generated/factory_synth_drums.inc

# Authored T1-T4 comparison data, independent of the synthetic DSP probe suite.
build/efficiency-tracks: tools/dsp/efficiency_tracks.cpp $(wildcard src/*.h src/*.inc) $(UI_TOOL_ARCHIVE)
	$(CXX) -std=c++17 -O1 -Isrc -Wall -Wextra -Werror $< -o $@ $(UI_TOOL_ARCHIVE)
build/render-efficiency: tools/dsp/render_efficiency.cpp $(ENGINE_TOOL_OBJECTS) $(HOST_SUPPORT_OBJECTS) $(PL_OBJECTS) $(UI_BASE_ARCHIVE)
	$(CXX) $(HOST_FLAGS) -O2 -Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-variable -Wno-unused-local-typedefs $(SAN) -pthread $< $(ENGINE_TOOL_OBJECTS) $(HOST_SUPPORT_OBJECTS) $(PL_OBJECTS) -o $@ $(UI_BASE_ARCHIVE)
build/render-efficiency-sanitized: tools/dsp/render_efficiency.cpp build/test-core-engine.o $(HOST_SUPPORT_OBJECTS) $(PL_OBJECTS) $(UI_BASE_ARCHIVE)
	$(CXX) $(HOST_FLAGS) -O1 -Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-variable -Wno-unused-local-typedefs $(SAN) -pthread $< build/test-core-engine.o $(HOST_SUPPORT_OBJECTS) $(PL_OBJECTS) -o $@ $(UI_BASE_ARCHIVE)
build/test-core-work-meter: tests/core_work_meter_test.cpp src/core_work_meter.h
	$(CXX) -std=c++17 -O1 -Isrc -Wall -Wextra -Werror $(SAN) $< -o $@
test-run-core-work-meter: build/test-core-work-meter
	UBSAN_OPTIONS=halt_on_error=1 ./build/test-core-work-meter
test-all: test-run-core-work-meter
build/test-efficiency-restore: tests/efficiency_restore_test.cpp build/test-core-engine.o $(HOST_SUPPORT_OBJECTS) $(PL_OBJECTS) $(DEMO_HEADERS) $(UI_BASE_ARCHIVE)
	$(CXX) $(HOST_FLAGS) $(SAN) -O0 -Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-variable -Wno-unused-local-typedefs $< build/test-core-engine.o $(HOST_SUPPORT_OBJECTS) $(PL_OBJECTS) -o $@ $(UI_BASE_ARCHIVE)
test-run-efficiency-restore: build/test-efficiency-restore
	UBSAN_OPTIONS=halt_on_error=1 ./build/test-efficiency-restore
test-all: test-run-efficiency-restore

# Deterministic independent-state DSP helper, including real concurrent fallback.
build/test-va-sample-slot: tests/va_sample_slot_test.cpp src/va_sample_slot.h src/virtual_analog_voice.h src/dsp_assist_counts.h $(HOST_SUPPORT_OBJECTS) $(PL_OBJECTS) $(UI_BASE_ARCHIVE)
	$(CXX) $(HOST_FLAGS) -O2 $(SAN) -pthread -Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-variable $< $(HOST_SUPPORT_OBJECTS) $(PL_OBJECTS) -o $@ $(UI_BASE_ARCHIVE)
test-run-va-sample-slot: build/test-va-sample-slot
	UBSAN_OPTIONS=halt_on_error=1 ./build/test-va-sample-slot
test-all: test-run-va-sample-slot
va-sample-slot-race-test:
	$(CXX) -std=c++17 -O2 -g -fsanitize=thread -pthread -Isrc -Ithird_party/daisysp/Source -Ithird_party/daisysp/Source/Utility tests/va_sample_slot_test.cpp third_party/daisysp/Source/Synthesis/oscillator.cpp third_party/daisysp/Source/Control/adsr.cpp third_party/daisysp/Source/Filters/ladder.cpp third_party/daisysp/Source/Filters/svf.cpp -o build/test-va-sample-slot-tsan
	TSAN_OPTIONS=halt_on_error=1 ./build/test-va-sample-slot-tsan

build/test-va-engine-assist: tests/va_engine_assist_test.cpp build/test-core-engine.o $(HOST_SUPPORT_OBJECTS) $(PL_OBJECTS) $(UI_BASE_ARCHIVE)
	$(CXX) $(HOST_FLAGS) -O2 $(SAN) -pthread -Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-variable -Wno-unused-local-typedefs $< build/test-core-engine.o $(HOST_SUPPORT_OBJECTS) $(PL_OBJECTS) -o $@ $(UI_BASE_ARCHIVE)
test-run-va-engine-assist: build/test-va-engine-assist
	UBSAN_OPTIONS=halt_on_error=1 ./build/test-va-engine-assist
test-all: test-run-va-engine-assist

.PHONY: test-run-va-sample-slot va-sample-slot-race-test test-run-va-engine-assist

test-run-cpu-heap-patch:
	python3 tests/cpu_heap_patch_test.py
test-all: test-run-cpu-heap-patch
.PHONY: test-run-cpu-heap-patch


# Immutable attachment snapshots are prepared before their sample-timed event.
build/test-prepared-sound: tests/prepared_sound_queue_test.cpp src/prepared_sound_queue.h src/normal_copy.h src/spsc_queue.h $(DEMO_HEADERS)
	$(CXX) -std=c++17 -O2 -g $(SAN) -pthread -Wall -Wextra -Werror -Isrc $< -o $@
test-run-prepared-sound: build/test-prepared-sound
	UBSAN_OPTIONS=halt_on_error=1 ./build/test-prepared-sound
test-all: test-run-prepared-sound
prepared-sound-race-test:
	$(CXX) -std=c++17 -O2 -g -fsanitize=thread -pthread -Wall -Wextra -Werror -Isrc tests/prepared_sound_queue_test.cpp -o build/test-prepared-sound-tsan
	TSAN_OPTIONS=halt_on_error=1 ./build/test-prepared-sound-tsan
.PHONY: test-run-prepared-sound prepared-sound-race-test

build/test-lfo-sync: tests/lfo_sync_test.cpp build/test-core-engine.o $(DEMO_HEADERS) $(HOST_SUPPORT_OBJECTS) $(PL_OBJECTS) $(UI_BASE_ARCHIVE)
	$(CXX) $(HOST_FLAGS) $(SAN) -O1 -Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-variable -Wno-unused-local-typedefs $< build/test-core-engine.o $(HOST_SUPPORT_OBJECTS) $(PL_OBJECTS) -o $@ $(UI_BASE_ARCHIVE)
test-run-lfo-sync: build/test-lfo-sync
	UBSAN_OPTIONS=halt_on_error=1 ./build/test-lfo-sync
test-all: test-run-lfo-sync

build/test-layer-components: tests/layer_components_test.cpp build/test-core-engine.o $(DEMO_HEADERS) $(HOST_SUPPORT_OBJECTS) $(PL_OBJECTS) $(UI_BASE_ARCHIVE)
	$(CXX) $(HOST_FLAGS) $(SAN) -O1 -Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-variable -Wno-unused-local-typedefs $< build/test-core-engine.o $(HOST_SUPPORT_OBJECTS) $(PL_OBJECTS) -o $@ $(UI_BASE_ARCHIVE)
test-run-layer-components: build/test-layer-components
	UBSAN_OPTIONS=halt_on_error=1 ./build/test-layer-components
test-all: test-run-layer-components

build/test-obsolete-content: tests/obsolete_content_test.cpp $(UI_BASE_ARCHIVE)
	$(CXX) -std=c++17 -O1 -Isrc -Wall -Wextra -Werror $(SAN) $< -o $@ $(UI_BASE_ARCHIVE)
test-run-obsolete-content: build/test-obsolete-content
	UBSAN_OPTIONS=halt_on_error=1 ./build/test-obsolete-content
test-all: test-run-obsolete-content

build/render-project: tools/content/render_project.cpp build/test-core-engine.o $(HOST_SUPPORT_OBJECTS) $(PL_OBJECTS) $(UI_BASE_ARCHIVE)
	$(CXX) $(HOST_FLAGS) -O1 -Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-variable -Wno-unused-local-typedefs $(SAN) $< build/test-core-engine.o $(HOST_SUPPORT_OBJECTS) $(PL_OBJECTS) -o $@ $(UI_BASE_ARCHIVE)

# External review: every maintained runner is registered and checked as a set.
test-all: test-run-device-settings test-run-device-workflow test-run-wave4-ui test-run-wave4-workload test-run-review-dsp test-run-review-workflow test-run-review-recovery-http test-run-py-test-inventory test-run-py-catalog-environment
build/test-review-dsp: tests/review_dsp_test.cpp $(wildcard src/*.h) $(PL_OBJECTS) build/host-support/third_party/daisysp/Source/Effects/wavefolder.o
	$(CXX) $(HOST_FLAGS) $(SAN) -O1 -Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-local-typedefs $< $(PL_OBJECTS) build/host-support/third_party/daisysp/Source/Effects/wavefolder.o -o $@
build/test-review-workflow: tests/review_workflow_test.cpp $(DEMO_HEADERS) build/test-core-engine.o $(HOST_SUPPORT_OBJECTS) $(PL_OBJECTS) $(UI_BASE_ARCHIVE)
	$(CXX) $(HOST_FLAGS) $(SAN) -O1 -Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-variable -Wno-unused-local-typedefs $< build/test-core-engine.o $(HOST_SUPPORT_OBJECTS) $(PL_OBJECTS) -o $@ $(UI_BASE_ARCHIVE)
build/test-review-recovery-http: tests/review_recovery_http_test.cpp src/file_http_deadline.h src/display_frame_gate.h src/recovery_frame.h
	$(CXX) -std=c++17 -Isrc $(SAN) -O1 -Wall -Wextra -Werror $< -o $@
.PHONY: test-run-review-dsp test-run-review-workflow test-run-review-recovery-http test-run-py-test-inventory test-run-py-catalog-environment
test-run-review-dsp: build/test-review-dsp
	UBSAN_OPTIONS=halt_on_error=1 ./build/test-review-dsp
test-run-review-workflow: build/test-review-workflow
	UBSAN_OPTIONS=halt_on_error=1 ./build/test-review-workflow
test-run-review-recovery-http: build/test-review-recovery-http
	UBSAN_OPTIONS=halt_on_error=1 ./build/test-review-recovery-http
test-run-py-test-inventory:
	python3 tests/update/test_inventory_test.py
test-run-py-catalog-environment:
	python3 tests/update/catalog_environment_test.py

# Full-sweep immediate fixes: opaque container scopes and boundary invariants.
build/test-review-wave2-codec: tests/review_wave2_codec_test.cpp tests/review_wave2_writer_fixtures.h $(DEMO_HEADERS) $(UI_BASE_ARCHIVE)
	$(CXX) $(HOST_FLAGS) $(SAN) -O1 -Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-variable -Wno-unused-local-typedefs $< -o $@ $(UI_BASE_ARCHIVE)
build/test-review-wave2-edges: tests/review_wave2_edges_test.cpp src/file_dhcp.h platform/desktop/file_access.h $(DEMO_HEADERS) $(UI_BASE_ARCHIVE)
	$(CXX) $(HOST_FLAGS) $(SAN) -O1 -Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-variable -Wno-unused-local-typedefs $< -o $@ $(UI_BASE_ARCHIVE)
build/test-review-wave2-workflow: tests/review_wave2_workflow_test.cpp build/test-core-engine.o $(DEMO_HEADERS) $(HOST_SUPPORT_OBJECTS) $(PL_OBJECTS) $(UI_BASE_ARCHIVE)
	$(CXX) $(HOST_FLAGS) $(SAN) -O1 -Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-variable -Wno-unused-local-typedefs $< build/test-core-engine.o $(HOST_SUPPORT_OBJECTS) $(PL_OBJECTS) -o $@ $(UI_BASE_ARCHIVE)
.PHONY: test-run-review-wave2-codec test-run-review-wave2-edges test-run-review-wave2-workflow test-run-py-review-wave2-tools
test-run-review-wave2-codec: build/test-review-wave2-codec
	UBSAN_OPTIONS=halt_on_error=1 ./build/test-review-wave2-codec
test-run-review-wave2-edges: build/test-review-wave2-edges
	UBSAN_OPTIONS=halt_on_error=1 ./build/test-review-wave2-edges
test-run-review-wave2-workflow: build/test-review-wave2-workflow
	UBSAN_OPTIONS=halt_on_error=1 ./build/test-review-wave2-workflow
test-run-py-review-wave2-tools:
	python3 tests/update/review_wave2_tools_test.py
test-all: test-run-review-wave2-codec test-run-review-wave2-edges test-run-review-wave2-workflow test-run-py-review-wave2-tools

build/test-meter-plan-a: tests/meter_plan_a_test.cpp build/test-core-engine.o $(DEMO_HEADERS) $(HOST_SUPPORT_OBJECTS) $(PL_OBJECTS) $(UI_BASE_ARCHIVE)
	$(CXX) $(HOST_FLAGS) $(SAN) -O1 -Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-variable -Wno-unused-local-typedefs $< build/test-core-engine.o $(HOST_SUPPORT_OBJECTS) $(PL_OBJECTS) -o $@ $(UI_BASE_ARCHIVE)
.PHONY: test-run-meter-plan-a
test-run-meter-plan-a: build/test-meter-plan-a
	UBSAN_OPTIONS=halt_on_error=1 ./build/test-meter-plan-a $(METER_FRAME_DIR)
test-all: test-run-meter-plan-a

.PHONY: test-run-py-display-pipeline
test-run-py-display-pipeline:
	UBSAN_OPTIONS=halt_on_error=1 python3 tests/usb_hot_paths_test.py
	UBSAN_OPTIONS=halt_on_error=1 python3 tests/display_pipeline_test.py
test-all: test-run-py-display-pipeline

# Optional MGXC13 preset root gain and deterministic Factory PCM regression.
build/preset-trim: tools/content/preset_trim.cpp $(DEMO_HEADERS) $(UI_TOOL_ARCHIVE)
	$(CXX) -std=c++17 -O2 -Isrc -Wall -Wextra -Werror $< -o $@ $(UI_TOOL_ARCHIVE)
build/render-preset: tools/content/render_preset.cpp build/test-core-engine.o $(HOST_SUPPORT_OBJECTS) $(PL_OBJECTS) $(UI_BASE_ARCHIVE)
	$(CXX) $(HOST_FLAGS) -O1 $(SAN) -Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-variable -Wno-unused-local-typedefs $< build/test-core-engine.o $(HOST_SUPPORT_OBJECTS) $(PL_OBJECTS) -o $@ $(UI_BASE_ARCHIVE)
build/test-preset-output: tests/preset_output_test.cpp build/test-core-engine.o $(HOST_SUPPORT_OBJECTS) $(PL_OBJECTS) $(DEMO_HEADERS) $(UI_BASE_ARCHIVE)
	$(CXX) $(HOST_FLAGS) $(SAN) -O1 -Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-variable -Wno-unused-local-typedefs $< build/test-core-engine.o $(HOST_SUPPORT_OBJECTS) $(PL_OBJECTS) -o $@ $(UI_BASE_ARCHIVE)
test-run-preset-output: build/test-preset-output
	UBSAN_OPTIONS=halt_on_error=1 ./build/test-preset-output
.PHONY: test-run-preset-output
test-all: test-run-preset-output

# Insights 9–14: current-song edits, browser windows and global mode/refusal UI.
build/test-insights-9-14: tests/insights_9_14_test.cpp build/test-core-engine.o $(DEMO_HEADERS) $(HOST_SUPPORT_OBJECTS) $(PL_OBJECTS) $(UI_BASE_ARCHIVE)
	$(CXX) $(HOST_FLAGS) $(SAN) -O1 -Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-variable -Wno-unused-local-typedefs $< build/test-core-engine.o $(HOST_SUPPORT_OBJECTS) $(PL_OBJECTS) -o $@ $(UI_BASE_ARCHIVE)
.PHONY: test-run-insights-9-14 test-run-py-hid-recovery
test-run-insights-9-14: build/test-insights-9-14
	UBSAN_OPTIONS=halt_on_error=1 ./build/test-insights-9-14
test-run-py-hid-recovery:
	python3 tests/hid_recovery_test.py
test-all: test-run-insights-9-14 test-run-py-hid-recovery

build/test-twin: tests/twin_test.cpp build/test-core-engine.o $(DEMO_HEADERS) $(HOST_SUPPORT_OBJECTS) $(PL_OBJECTS) $(UI_BASE_ARCHIVE)
	$(CXX) $(HOST_FLAGS) $(SAN) -O0 -Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-variable -Wno-unused-local-typedefs $< build/test-core-engine.o $(HOST_SUPPORT_OBJECTS) $(PL_OBJECTS) -o $@ $(UI_BASE_ARCHIVE)
.PHONY: test-run-twin
test-run-twin: build/test-twin
	UBSAN_OPTIONS=halt_on_error=1 ./build/test-twin
test-all: test-run-twin

# Atomic composition operations use the actual Engine/Performance host path.
build/test-composition: tests/composition_test.cpp build/test-core-engine.o $(DEMO_HEADERS) $(HOST_SUPPORT_OBJECTS) $(PL_OBJECTS) $(UI_BASE_ARCHIVE)
	$(CXX) $(HOST_FLAGS) $(SAN) -O0 -Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-variable -Wno-unused-local-typedefs $< build/test-core-engine.o $(HOST_SUPPORT_OBJECTS) $(PL_OBJECTS) -o $@ $(UI_BASE_ARCHIVE)
.PHONY: test-run-composition
test-run-composition: build/test-composition
	UBSAN_OPTIONS=halt_on_error=1 ./build/test-composition
test-all: test-run-composition
build/test-composition-templates: tests/composition_templates_test.cpp build/test-core-engine.o $(DEMO_HEADERS) $(HOST_SUPPORT_OBJECTS) $(PL_OBJECTS) $(UI_BASE_ARCHIVE)
	$(CXX) $(HOST_FLAGS) $(SAN) -O0 -Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-variable -Wno-unused-local-typedefs $< build/test-core-engine.o $(HOST_SUPPORT_OBJECTS) $(PL_OBJECTS) -o $@ $(UI_BASE_ARCHIVE)
.PHONY: test-run-composition-templates
test-run-composition-templates: build/test-composition-templates
	UBSAN_OPTIONS=halt_on_error=1 ./build/test-composition-templates
test-all: test-run-composition-templates
# Read-only acceptance of externally authored template songs.
build/check-templates: tools/content/check_templates.cpp build/test-core-engine.o $(DEMO_HEADERS) $(HOST_SUPPORT_OBJECTS) $(PL_OBJECTS) $(UI_BASE_ARCHIVE)
	$(CXX) $(HOST_FLAGS) $(SAN) -O1 -Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-variable -Wno-unused-local-typedefs $< build/test-core-engine.o $(HOST_SUPPORT_OBJECTS) $(PL_OBJECTS) -o $@ $(UI_BASE_ARCHIVE)
build/composition-template-fixture: tests/composition_template_fixture.cpp $(UI_BASE_ARCHIVE)
	$(CXX) -std=c++17 -Isrc -O1 $(SAN) -Wall -Wextra -Werror $< -o $@ $(UI_BASE_ARCHIVE)
.PHONY: test-run-composition-acceptance
test-run-composition-acceptance: build/composition-template-fixture build/check-templates
	./build/composition-template-fixture build/composition-template-acceptance
	./build/check-templates build/composition-template-acceptance FACTORY/DEMOS/fixture-0.mgxs FACTORY/DEMOS/fixture-1.mgxs FACTORY/DEMOS/fixture-2.mgxs
	! ./build/check-templates --one build/composition-template-acceptance FACTORY/DEMOS/invalid-structure.mgxs
	! ./build/check-templates --one build/composition-template-acceptance FACTORY/DEMOS/invalid-silent.mgxs
test-all: test-run-composition-acceptance

# Bounded real-owner qualification for the three-wave deployment.
build/test-deployment-check: tests/deployment_check_test.cpp firmware/deployment_check.h src/update/project_snapshot.h build/test-core-engine.o $(DEMO_HEADERS) $(HOST_SUPPORT_OBJECTS) $(PL_OBJECTS) $(UI_BASE_ARCHIVE)
	$(CXX) $(HOST_FLAGS) $(SAN) -O1 -Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-variable -Wno-unused-local-typedefs $< build/test-core-engine.o $(HOST_SUPPORT_OBJECTS) $(PL_OBJECTS) -o $@ $(UI_BASE_ARCHIVE)
.PHONY: test-run-deployment-check
test-run-deployment-check: build/test-deployment-check
	UBSAN_OPTIONS=halt_on_error=1 ./build/test-deployment-check
test-all: test-run-deployment-check
.PHONY: test-run-py-deployment-runner
test-run-py-deployment-runner:
	python3 tests/deployment_runner_test.py
	python3 tests/diagnostic_owner_test.py
test-all: test-run-py-deployment-runner
