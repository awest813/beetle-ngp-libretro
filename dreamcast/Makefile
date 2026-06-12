# Beetle NGP Dreamcast launcher (KallistiOS / DreamSDK)
#
# Prerequisites:
#   source $KOS_BASE/environ.sh
#
# Build:
#   make
#
# Upload (Broadband Adapter):
#   dc-tool-ip -x beetlengp.elf

TARGET = beetlengp.elf
OBJS   = main.o menu.o dc_audio.o dc_settings.o dc_video.o \
         dc_input.o dc_saves.o dc_notify.o dc_vmu.o dc_pvr.o dc_ui.o
CORE   = ../mednafen_ngp_libretro_dreamcast.a

KOS_CFLAGS += -I../libretro-common/include

all: rm-elf $(TARGET)

include $(KOS_BASE)/Makefile.rules

$(CORE):
	$(MAKE) -C .. platform=dreamcast

$(TARGET): $(OBJS) $(CORE)
	kos-c++ -o $(TARGET) $(OBJS) -Wl,--whole-archive $(CORE) -Wl,--no-whole-archive -lstdc++ $(KOS_LIBS)

clean: clean_launcher clean_core

clean_launcher:
	-rm -f $(TARGET) $(OBJS)

clean_core:
	$(MAKE) -C .. platform=dreamcast clean

.PHONY: all clean clean_launcher clean_core
