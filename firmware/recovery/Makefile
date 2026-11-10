CIRCLEHOME ?= ../../.deps/circle
OBJS = software_canvas.o software_widgets.o main.o kernel.o update_runtime.o mk3_usb.o update_mono.o update_ed25519.o
CFLAGS += -DGROOVE_RECOVERY -I$(CURDIR)/../../src/freestanding -I$(CURDIR)/../../third_party/monocypher -I$(CIRCLEHOME)/addon/fatfs
include ../update-libs.mk
LIBS = $(UPDATE_LIBS) $(CIRCLEHOME)/addon/fatfs/libfatfs.a $(CIRCLEHOME)/addon/SDCard/libsdcard.a $(CIRCLEHOME)/lib/usb/libusb.a $(CIRCLEHOME)/lib/input/libinput.a $(CIRCLEHOME)/lib/fs/libfs.a $(CIRCLEHOME)/lib/libcircle.a
include $(CIRCLEHOME)/Rules.mk
update_runtime.o: ../update_runtime.cpp
	$(CPP) $(CPPFLAGS) -MMD -MP -c -o $@ $<
mk3_usb.o: ../mk3_usb.cpp
	$(CPP) $(CPPFLAGS) -MMD -MP -c -o $@ $<
update_mono.o: ../../third_party/monocypher/monocypher.c
	$(CC) $(CFLAGS) -MMD -MP -c -o $@ $<
update_ed25519.o: ../../third_party/monocypher/monocypher-ed25519.c
	$(CC) $(CFLAGS) -MMD -MP -c -o $@ $<
-include $(DEPS)

# Shared primitive implementations; recovery does not link Performance/Engine.
software_canvas.o: ../../src/software_canvas.cpp
	$(CPP) $(CPPFLAGS) -MMD -MP -c $< -o $@
software_widgets.o: ../../src/software_widgets.cpp
	$(CPP) $(CPPFLAGS) -MMD -MP -c $< -o $@
