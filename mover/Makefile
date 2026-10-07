.PHONY: all clean verify FORCE layeredfs exheader
.DEFAULT_GOAL := all

export MSYS2_ARG_CONV_EXCL := *

ifeq ($(strip $(DEVKITARM)),)
$(error DEVKITARM is not set; export DEVKITARM=/path/to/devkitARM)
endif

SRC := src
TOOLS := tools
ARMIPS ?= ../$(TOOLS)/armips/armips
IPS_TOOL ?= $(TOOLS)/flips/flips
PYTHON ?= python3
BUILD ?= build
TITLE_ID := 00040000000C9C00
BASE := rom/exefs/00040000000C9C00.dec.code
PATCHED := $(BUILD)/00040000000C9C00.dec.code
RELEASE_ROOT ?= ../release
RELEASE_TITLE := $(RELEASE_ROOT)/$(TITLE_ID)
RELEASE_BPS := $(RELEASE_TITLE)/code.bps
RELEASE_ROMFS := $(RELEASE_TITLE)/romfs
ROMFS_SOURCE ?= rom/romfs
OBJECTS := $(patsubst $(SRC)/%.c,$(BUILD)/%.o,$(wildcard $(SRC)/*.c))
SYMBOLS := $(BUILD)/armips-symbols.txt
EXHEADER ?= rom/exheader.bin
PREPARED_BASE := $(BUILD)/expanded-base.code
EXPANDED_EXHEADER := $(BUILD)/exheader.bin
RELEASE_EXHEADER := $(RELEASE_TITLE)/exheader.bin
ASSEMBLY_BASE := $(PREPARED_BASE)
VERIFY_EXHEADER := --base-exheader "$(EXHEADER)" --exheader "$(RELEASE_EXHEADER)"

CC := $(DEVKITARM)/bin/arm-none-eabi-gcc
OBJDUMP := $(DEVKITARM)/bin/arm-none-eabi-objdump
ARM_CFLAGS := -std=c11 -Os -march=armv6k -mtune=mpcore -marm -mfloat-abi=soft \
	-ffreestanding -fno-builtin -fno-stack-protector -fno-unwind-tables \
	-fno-asynchronous-unwind-tables -fomit-frame-pointer -fno-common \
	-ffunction-sections -fdata-sections -MMD -MP -Wall -Wextra -Werror \
	-Iinclude
THUMB_CFLAGS := $(filter-out -marm,$(ARM_CFLAGS)) -mthumb -fno-jump-tables
CFLAGS := $(ARM_CFLAGS)

$(BUILD)/bankdata_redirect.o $(BUILD)/fs_helpers.o $(BUILD)/local_validation.o \
$(BUILD)/offline_flow.o $(BUILD)/nds_fs.o $(BUILD)/nds_sources.o \
$(BUILD)/language_settings.o: CFLAGS := $(THUMB_CFLAGS)

-include $(OBJECTS:.o=.d)

all: $(RELEASE_BPS) exheader layeredfs verify

$(BUILD):
	mkdir -p "$@"

$(BUILD)/%.o: $(SRC)/%.c | $(BUILD)
	"$(CC)" $(CFLAGS) -c "$<" -o "$@"
	"$(OBJDUMP)" -dr -z "$@" > "$(@:.o=.s)"

$(PREPARED_BASE): FORCE $(BASE) $(EXHEADER) $(TOOLS)/prepare_expanded_code.py | $(BUILD)
	"$(PYTHON)" -B -m tools.prepare_expanded_code prepare --base-code "$(BASE)" --base-exheader "$(EXHEADER)" --output-code "$@" --output-exheader "$(EXPANDED_EXHEADER)" --payload-start 0x00365000 --payload-size 0x3000

$(PATCHED): FORCE $(SRC)/main.s $(OBJECTS) $(ASSEMBLY_BASE) $(TOOLS)/verify_patch.py
	cd "$(SRC)" && "$(ARMIPS)" -erroronwarning -stat -sym "../$(SYMBOLS)" -strequ INPUT_CODE "../$(ASSEMBLY_BASE)" -strequ OUTPUT_CODE "../$(PATCHED)" -strequ BUILD_DIRECTORY "../$(BUILD)" main.s

$(RELEASE_EXHEADER): $(PATCHED)
	mkdir -p "$(dir $@)"
	cp "$(EXPANDED_EXHEADER)" "$@"

exheader: $(RELEASE_EXHEADER)

$(RELEASE_BPS): $(PATCHED)
	mkdir -p "$(dir $@)"
	rm -f "$@"
	"$(IPS_TOOL)" --create --bps --exact "$(BASE)" "$(PATCHED)" "$@"
	rm -f "$(RELEASE_TITLE)/code.ips"

layeredfs: $(SRC)/patch_messages.py $(TOOLS)/message_archive.py
	rm -rf "$(RELEASE_ROMFS)"
	"$(PYTHON)" -B -m src.patch_messages --source-romfs "$(ROMFS_SOURCE)" --output-romfs "$(RELEASE_ROMFS)"

verify: $(PATCHED) $(RELEASE_BPS) exheader layeredfs $(TOOLS)/verify_patch.py
	"$(PYTHON)" -B -m tools.verify_patch --base "$(BASE)" --patched "$(PATCHED)" --symbols "$(SYMBOLS)" --bps "$(RELEASE_BPS)" --source-romfs "$(ROMFS_SOURCE)" --output-romfs "$(RELEASE_ROMFS)" $(VERIFY_EXHEADER)

clean:
	rm -rf "$(RELEASE_TITLE)" "$(BUILD)"

FORCE:
