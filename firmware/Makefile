include ../src/engine_sources.mk
MSFA_OBJECTS := $(patsubst ../third_party/msfa/%.cc,msfa/%.o,$(wildcard ../third_party/msfa/*.cc))
CIRCLEHOME ?= ../.deps/circle
PL_SOURCES := ../third_party/mutable/clouds/dsp/correlator.cc ../third_party/mutable/clouds/dsp/granular_processor.cc ../third_party/mutable/clouds/dsp/mu_law.cc ../third_party/mutable/clouds/dsp/pvoc/frame_transformation.cc ../third_party/mutable/clouds/dsp/pvoc/phase_vocoder.cc ../third_party/mutable/clouds/dsp/pvoc/stft.cc ../third_party/mutable/clouds/resources.cc ../third_party/mutable/stmlib/dsp/atan.cc ../third_party/mutable/rings/dsp/part.cc ../third_party/mutable/rings/dsp/resonator.cc ../third_party/mutable/rings/dsp/string.cc ../third_party/mutable/rings/dsp/fm_voice.cc ../third_party/mutable/rings/resources.cc $(shell find ../third_party/mutable/plaits/dsp -name '*.cc') ../third_party/mutable/plaits/resources.cc ../third_party/mutable/stmlib/dsp/units.cc ../third_party/mutable/stmlib/utils/random.cc
PL_OBJECTS := $(patsubst ../third_party/mutable/%.cc,plaits/%.o,$(PL_SOURCES))
OBJS = granular_dsp.o daisy_phasor.o daisy_formantosc.o daisy_vosim.o file_transfer.o soundfont.o fm6_voice.o $(MSFA_OBJECTS) daisy_ladder.o wavetable_bank.o texture_dsp.o resonator_dsp.o update_runtime.o update_mono.o update_ed25519.o main.o kernel.o ui_probe.o mk3_usb.o mk3_audio.o mk3_midi.o project_storage.o $(addsuffix .o,$(ENGINE_NAMES)) daisy_adsr.o daisy_oscillator.o daisy_fm2.o daisy_svf.o daisy_wavefolder.o daisy_overdrive.o daisy_analogbassdrum.o daisy_analogsnaredrum.o daisy_hihat.o daisy_synthbassdrum.o daisy_synthsnaredrum.o $(PL_OBJECTS)
CFLAGS += -I$(CURDIR)/../src -I$(CURDIR)/../build/mutable-port -I$(CURDIR)/../third_party/monocypher -I$(CIRCLEHOME)/addon/fatfs -DGROOVE_PLAITS -DM_PI=3.14159265358979323846 -I$(CURDIR)/../src/freestanding -I$(CURDIR)/../third_party/mutable -I$(CURDIR)/../third_party/daisysp/Source -I$(CURDIR)/../third_party/daisysp/Source/Utility
# The detailed per-sample ledger is an explicit diagnostic build, not the product path.
DETAILED_RENDER_ATTRIBUTION ?= 0
CFLAGS += -DGROOVE_DETAILED_RENDER_ATTRIBUTION=$(DETAILED_RENDER_ATTRIBUTION)
include update-libs.mk
LIBS = $(UPDATE_LIBS) $(CIRCLEHOME)/addon/fatfs/libfatfs.a $(CIRCLEHOME)/addon/SDCard/libsdcard.a $(CIRCLEHOME)/lib/sound/libsound.a $(CIRCLEHOME)/lib/usb/libusb.a $(CIRCLEHOME)/lib/input/libinput.a $(CIRCLEHOME)/lib/fs/libfs.a $(CIRCLEHOME)/lib/libcircle.a
include $(CIRCLEHOME)/Rules.mk
# Retain compiler-measured frames for the boot/reset stack budget.
kernel.o file_transfer.o project_storage.o: CFLAGS += -fstack-usage
# Engine and kernel units use the shared Circle target profile.
# The third-party DSP units below are the one deliberate exception:
# they are pinned to -O2, the profile the shipped firmware was
# qualified with on hardware. The shared profile's aggressive
# inline limits (tuned for the control path's code-cache budget)
# make these units grow in aggregate -- a clean build with the
# exception lost overflows the kernel size margin -- and -O2 is
# also the upstream projects' own build profile. A later -O flag
# wins, so appending -O2 overrides the profile's -O3 for these
# objects only. The explicit Makefile prerequisite matters: these
# objects once kept a stale profile for months because their
# pattern rules did not depend on this file.
DSP_O2_OBJECTS := $(PL_OBJECTS) $(MSFA_OBJECTS) fm6_voice.o \
    daisy_analogbassdrum.o daisy_analogsnaredrum.o daisy_hihat.o \
    daisy_synthbassdrum.o daisy_synthsnaredrum.o
$(DSP_O2_OBJECTS): CPPFLAGS += -O2
$(DSP_O2_OBJECTS): Makefile
$(addsuffix .o,$(ENGINE_NAMES)): %.o: ../src/%.cpp ../src/engine.h Makefile ../src/engine_sources.mk
	$(CPP) $(CPPFLAGS) -MMD -MP -c -o $@ $<
daisy_adsr.o: ../third_party/daisysp/Source/Control/adsr.cpp
	$(CPP) $(CPPFLAGS) -MMD -MP -c -o $@ $<
daisy_oscillator.o: ../third_party/daisysp/Source/Synthesis/oscillator.cpp
	$(CPP) $(CPPFLAGS) -MMD -MP -c -o $@ $<
daisy_wavefolder.o: ../third_party/daisysp/Source/Effects/wavefolder.cpp
	$(CPP) $(CPPFLAGS) -MMD -MP -c -o $@ $<
daisy_overdrive.o: ../third_party/daisysp/Source/Effects/overdrive.cpp
	$(CPP) $(CPPFLAGS) -MMD -MP -c -o $@ $<
daisy_fm2.o: ../third_party/daisysp/Source/Synthesis/fm2.cpp
	$(CPP) $(CPPFLAGS) -MMD -MP -c -o $@ $<
daisy_svf.o: ../third_party/daisysp/Source/Filters/svf.cpp
	$(CPP) $(CPPFLAGS) -MMD -MP -c -o $@ $<
daisy_%.o: ../third_party/daisysp/Source/Drums/%.cpp
	$(CPP) $(CPPFLAGS) -MMD -MP -c -o $@ $<
plaits/%.o: ../third_party/mutable/%.cc
	@mkdir -p $(dir $@)
	$(CPP) $(CPPFLAGS) -MMD -MP -c -o $@ $<
-include $(DEPS) $(PL_OBJECTS:.o=.d)

update_mono.o: ../third_party/monocypher/monocypher.c
	$(CC) $(CFLAGS) -MMD -MP -c -o $@ $<
update_ed25519.o: ../third_party/monocypher/monocypher-ed25519.c
	$(CC) $(CFLAGS) -MMD -MP -c -o $@ $<

resonator_dsp.o: ../src/resonator_dsp.cpp ../src/resonator_dsp.h
	$(CPP) $(CPPFLAGS) -MMD -MP -c $< -o $@

texture_dsp.o: ../src/texture_dsp.cpp ../src/texture_dsp.h
	$(CPP) $(CPPFLAGS) -DTEST -MMD -MP -c $< -o $@

# Upstream TEST disables only the STM32 timing-pin driver for these DSP files.
plaits/clouds/dsp/granular_processor.o: CFLAGS += -DTEST

../build/mutable-port/correlator.cc: ../third_party/mutable/clouds/dsp/correlator.cc ../tools/mutable/portable_correlator.py
	python3 ../tools/mutable/portable_correlator.py $< $@
plaits/clouds/dsp/correlator.o: ../build/mutable-port/correlator.cc
	@mkdir -p $(dir $@)
	$(CPP) $(CPPFLAGS) -MMD -MP -c $< -o $@

../build/mutable-port/clouds/dsp/looping_sample_player.h: ../third_party/mutable/clouds/dsp/looping_sample_player.h ../tools/mutable/portable_loop.py
	python3 ../tools/mutable/portable_loop.py $< $@
texture_dsp.o $(filter plaits/clouds/%,$(PL_OBJECTS)): ../build/mutable-port/clouds/dsp/looping_sample_player.h

../build/mutable-port/granular_processor.cc: ../third_party/mutable/clouds/dsp/granular_processor.cc ../tools/mutable/portable_processor.py
	python3 ../tools/mutable/portable_processor.py $< $@
plaits/clouds/dsp/granular_processor.o: ../build/mutable-port/granular_processor.cc
	@mkdir -p $(dir $@)
	$(CPP) $(CPPFLAGS) -DTEST -MMD -MP -c $< -o $@

../build/embedded_demo_sample.h: ../assets/samples/demo-slices.wav ../tools/content/embed_sample.py
	python3 ../tools/content/embed_sample.py $< $@
project_storage.o: ../build/embedded_demo_sample.h

wavetable_bank.o: ../src/wavetable_bank.cpp ../src/generated/akwf_tables.inc ../src/wavetable_bank.h
	$(CPP) $(CPPFLAGS) -MMD -MP -c $< -o $@

daisy_ladder.o: ../third_party/daisysp/Source/Filters/ladder.cpp ../third_party/daisysp/Source/Filters/ladder.h
	$(CPP) $(CPPFLAGS) -MMD -MP -c $< -o $@

soundfont.o: ../src/soundfont.cpp ../src/soundfont.h ../third_party/tinysoundfont/tsf.h
	$(CPP) $(CPPFLAGS) -MMD -MP -c -o $@ $<

fm6_voice.o: ../src/fm6_voice.cpp ../src/fm6_voice.h ../src/fm6_state.h
	$(CPP) $(CPPFLAGS) -MMD -MP -c -o $@ $<
msfa/%.o: ../third_party/msfa/%.cc
	@mkdir -p $(dir $@)
	$(CPP) $(CPPFLAGS) -fwrapv -D__CONTROLLERS_H -I../src/msfa_adapter/msfa -MMD -MP -c -o $@ $<
-include $(MSFA_OBJECTS:.o=.d)

../build/mutable-port/rings/dsp/string.h ../build/mutable-port/rings/dsp/string.cc &: ../tools/mutable/cached_string.py ../third_party/mutable/rings/dsp/string.h ../third_party/mutable/rings/dsp/string.cc
	python3 ../tools/mutable/cached_string.py ../third_party/mutable ../build/mutable-port
resonator_dsp.o $(filter plaits/rings/%,$(PL_OBJECTS)): ../build/mutable-port/rings/dsp/string.h
plaits/rings/dsp/string.o: ../build/mutable-port/rings/dsp/string.cc
	$(CPP) $(CPPFLAGS) -MMD -MP -c $< -o $@

../build/mutable-port/stft.cc: ../tools/mutable/windowed_stft.py ../third_party/mutable/clouds/dsp/pvoc/stft.cc ../src/native_window.h
	python3 ../tools/mutable/windowed_stft.py ../third_party/mutable $@
plaits/clouds/dsp/pvoc/stft.o: ../build/mutable-port/stft.cc
	$(CPP) $(CPPFLAGS) -DTEST -MMD -MP -c $< -o $@

daisy_formantosc.o: ../third_party/daisysp/Source/Synthesis/formantosc.cpp
	$(CPP) $(CPPFLAGS) -MMD -MP -c $< -o $@
daisy_vosim.o: ../third_party/daisysp/Source/Synthesis/vosim.cpp
	$(CPP) $(CPPFLAGS) -MMD -MP -c $< -o $@

daisy_phasor.o: ../third_party/daisysp/Source/Control/phasor.cpp
	$(CPP) $(CPPFLAGS) -MMD -MP -c $< -o $@

granular_dsp.o: ../src/granular_dsp.cpp
	$(CPP) $(CPPFLAGS) -MMD -MP -c $< -o $@

../build/mutable-port/rings/dsp/resonator.cc: ../tools/mutable/bounded_resonator.py ../third_party/mutable/rings/dsp/resonator.cc ../third_party/mutable/resonator-texture-sources.json
	python3 ../tools/mutable/bounded_resonator.py ../third_party/mutable $@
plaits/rings/dsp/resonator.o: ../build/mutable-port/rings/dsp/resonator.cc ../src/resonator_boundary.h
	$(CPP) $(CPPFLAGS) -MMD -MP -c $< -o $@
