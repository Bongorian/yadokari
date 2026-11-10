# Default off for public builds: Circle's WLAN addon carries
# Plan 9-derived code under the GPL v2 only, and its combination
# with this GPL v3 firmware has not been cleared. Set
# ENABLE_UPDATE_WIFI=1 only for private builds.
ENABLE_UPDATE_WIFI ?= 0
UPDATE_LIBS = $(CIRCLEHOME)/lib/net/libnet.a $(CIRCLEHOME)/lib/sched/libsched.a $(CIRCLEHOME)/lib/usb/gadget/libusbgadget.a
ifeq ($(ENABLE_UPDATE_WIFI),1)
CFLAGS += -DGROOVE_UPDATE_WIFI
UPDATE_LIBS := $(CIRCLEHOME)/addon/wlan/hostap/wpa_supplicant/libwpa_supplicant.a $(CIRCLEHOME)/addon/wlan/libwlan.a $(UPDATE_LIBS)
endif

# Bare-metal GCC does not support retain; keep the signed identity as an
# explicit linker root in both application and recovery section-GC builds.
LDFLAGS += --undefined=GrooveUpdateIdentity
