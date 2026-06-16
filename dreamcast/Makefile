# Beetle NGP Dreamcast launcher (KallistiOS / DreamSDK)
#
# Prerequisites:
#   source $KOS_BASE/environ.sh
#
# Targets:
#   make              Build beetlengp.elf
#   make cdi          Build .elf + generate self-boot .cdi image
#   make sdiso        Build .elf + generate SD-card .iso image
#   make iso          Same as sdiso
#   make dist         Build .elf + .cdi + .sdiso
#   make upload       Upload .elf via dc-tool (BBA)
#
# CDI prerequisites (auto-detected, install if missing):
#   scramble  - part of KOS ($KOS_BASE/utils/scramble/)
#   mkisofs   - system package (cdrkit or cdrtools)
#   cdi4dc    - download from DreamSDK or build from source

TARGET     := beetlengp
TARGET_ELF := $(TARGET).elf
TARGET_CDI := $(TARGET).cdi
TARGET_ISO := $(TARGET).iso
OBJS       := main.o menu.o dc_audio.o dc_settings.o dc_video.o \
               dc_input.o dc_saves.o dc_notify.o dc_vmu.o dc_pvr.o dc_ui.o
CORE       := ../mednafen_ngp_libretro_dreamcast.a

KOS_CFLAGS += -I../libretro-common/include

# ---- Detect available tools ----
SCRAMBLE   := $(KOS_BASE)/utils/scramble/scramble
MAKEIP     := $(KOS_BASE)/utils/makeip/makeip
CDI4DC     := $(shell command -v cdi4dc 2>/dev/null || echo cdi4dc)
MKISOFS    := $(shell command -v mkisofs 2>/dev/null || command -v genisoimage 2>/dev/null || echo mkisofs)
DC_TOOL_IP := $(shell command -v dc-tool-ip 2>/dev/null || echo dc-tool-ip)

# Disc image variables
CD_ROOT_DIR  := cd_root
IP_BIN       := IP.BIN
DISC_LABEL   := BeetleNGP
ROMS_DIR     ?=

all: rm-elf $(TARGET_ELF)

include $(KOS_BASE)/Makefile.rules

$(CORE):
	$(MAKE) -C .. platform=dreamcast

$(TARGET_ELF): $(OBJS) $(CORE)
	kos-c++ -o $(TARGET_ELF) $(OBJS) -Wl,--whole-archive $(CORE) -Wl,--no-whole-archive -lstdc++ $(KOS_LIBS)

# ---- CDI (self-boot CD-R) ----
$(IP_BIN):
	@echo "  GEN     $@"
	@OUT=$(IP_BIN) bash gen_ip.sh
	@test -f $(IP_BIN) || { echo "ERROR: Failed to generate $(IP_BIN)"; exit 1; }

$(TARGET_CDI): $(TARGET_ELF) $(IP_BIN)
	@echo "  BUILD   $@"
	@if [ ! -x "$(SCRAMBLE)" ]; then \
		echo "ERROR: scramble not found at $(SCRAMBLE)"; \
		echo "  Is KOS_BASE set? source $$KOS_BASE/environ.sh"; \
		exit 1; \
	fi
	@rm -rf $(CD_ROOT_DIR)
	@mkdir -p $(CD_ROOT_DIR)
	@kos-objcopy -O binary $(TARGET_ELF) $(CD_ROOT_DIR)/1ST_READ.BIN
	@$(SCRAMBLE) $(CD_ROOT_DIR)/1ST_READ.BIN $(CD_ROOT_DIR)/1ST_READ.BIN
	cp $(IP_BIN) $(CD_ROOT_DIR)/
	$(if $(ROMS_DIR),cp -r $(ROMS_DIR)/*.ngp $(ROMS_DIR)/*.ngc $(ROMS_DIR)/*.npc $(CD_ROOT_DIR)/ 2>/dev/null || true,)
	@if [ ! -x "$(CDI4DC)" ] && ! command -v cdi4dc >/dev/null 2>&1; then \
		echo "  cdi4dc not found - generating ISO only"; \
		echo "  Install cdi4dc to produce .cdi: https://dreamsdk.org"; \
		$(MAKE) $(TARGET_ISO); \
		echo "  Created $(TARGET_ISO) (use with GDEMU or emulator)"; \
	else \
		$(CDI4DC) $(TARGET_CDI) -b $(IP_BIN) -d $(CD_ROOT_DIR) >/dev/null 2>&1 \
			|| $(CDI4DC) $(TARGET_CDI) $(CD_ROOT_DIR) $(DISC_LABEL) 0 0 >/dev/null 2>&1 \
			|| { echo "  cdi4dc failed; checking for mkdcdisc..."; \
			     mkdcdisc -n "$(DISC_LABEL)" -d $(CD_ROOT_DIR) -o $(TARGET_CDI) 2>/dev/null \
			     || { echo "ERROR: Could not create CDI. Install cdi4dc or mkdcdisc."; exit 1; }; }; \
		rm -rf $(CD_ROOT_DIR); \
		echo "  Created $(TARGET_CDI)"; \
	fi

cdi: $(TARGET_CDI)
.PHONY: cdi

# ---- SD ISO (GDEMU / SD card adapter) ----
$(TARGET_ISO): $(TARGET_ELF) $(IP_BIN)
	@echo "  BUILD   $@"
	@rm -rf $(CD_ROOT_DIR)
	@mkdir -p $(CD_ROOT_DIR)
	@kos-objcopy -O binary $(TARGET_ELF) $(CD_ROOT_DIR)/1ST_READ.BIN
	@$(SCRAMBLE) $(CD_ROOT_DIR)/1ST_READ.BIN $(CD_ROOT_DIR)/1ST_READ.BIN
	cp $(IP_BIN) $(CD_ROOT_DIR)/
	$(if $(ROMS_DIR),cp -r $(ROMS_DIR)/*.ngp $(ROMS_DIR)/*.ngc $(ROMS_DIR)/*.npc $(CD_ROOT_DIR)/ 2>/dev/null || true,)
	@$(MKISOFS) -V "$(DISC_LABEL)" -l -o $(TARGET_ISO) $(CD_ROOT_DIR)
	@rm -rf $(CD_ROOT_DIR)
	@echo "  Created $(TARGET_ISO)"

sdiso: $(TARGET_ISO)
iso: $(TARGET_ISO)
.PHONY: sdiso iso

# ---- Distribution (all image formats) ----
dist: $(TARGET_ELF) $(TARGET_ISO) $(TARGET_CDI)
	@echo "  Distribution complete:"
	@ls -lh $(TARGET_ELF) $(TARGET_ISO) $(patsubst %.cdi,%.cdi,$(wildcard $(TARGET_CDI)))
.PHONY: dist

# ---- Network upload (BBA) ----
upload: $(TARGET_ELF)
	$(DC_TOOL_IP) -x $(TARGET_ELF)
.PHONY: upload

# ---- Clean ----
clean: clean_launcher clean_core clean_disc
.PHONY: clean

clean_launcher:
	-rm -f $(TARGET_ELF) $(OBJS)

clean_core:
	$(MAKE) -C .. platform=dreamcast clean

clean_disc:
	-rm -rf $(CD_ROOT_DIR)
	-rm -f $(IP_BIN) $(TARGET_CDI) $(TARGET_ISO) 1ST_READ.BIN
.PHONY: clean_disc
