.PHONY: all clean verify FORCE layeredfs
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
BUILD := build
TITLE_ID := 00040000000C9C00
BASE := rom/exefs/00040000000C9C00.dec.code
PATCHED := $(BUILD)/00040000000C9C00.dec.code
RELEASE_ROOT := ../release
RELEASE_TITLE := $(RELEASE_ROOT)/$(TITLE_ID)
RELEASE_IPS := $(RELEASE_TITLE)/code.ips
RELEASE_ROMFS := $(RELEASE_TITLE)/romfs
ROMFS_SOURCE ?= rom/romfs
OBJECTS := $(patsubst $(SRC)/%.c,$(BUILD)/%.o,$(wildcard $(SRC)/*.c))
SYMBOLS := $(BUILD)/armips-symbols.txt

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
$(BUILD)/offline_flow.o $(BUILD)/nds_fs.o $(BUILD)/nds_sources.o: CFLAGS := $(THUMB_CFLAGS)

-include $(OBJECTS:.o=.d)

all: $(RELEASE_IPS) layeredfs verify

$(BUILD):
	mkdir -p "$@"

$(BUILD)/%.o: $(SRC)/%.c | $(BUILD)
	"$(CC)" $(CFLAGS) -c "$<" -o "$@"
	"$(OBJDUMP)" -dr -z "$@" > "$(@:.o=.s)"

$(PATCHED): FORCE $(SRC)/main.s $(OBJECTS) $(BASE) $(TOOLS)/verify_patch.py
	"$(PYTHON)" -B -c "from pathlib import Path; from tools.verify_patch import verify_code_cave; verify_code_cave(Path('$(BASE)').read_bytes())"
	cd "$(SRC)" && "$(ARMIPS)" -erroronwarning -stat -sym "../$(SYMBOLS)" main.s

$(RELEASE_IPS): $(PATCHED)
	mkdir -p "$(dir $@)"
	rm -f "$@"
	"$(IPS_TOOL)" --create --ips "$(BASE)" "$(PATCHED)" "$@"

layeredfs: $(SRC)/patch_messages.py $(TOOLS)/message_archive.py
	rm -rf "$(RELEASE_ROMFS)"
	"$(PYTHON)" -B -m src.patch_messages --source-romfs "$(ROMFS_SOURCE)" --output-romfs "$(RELEASE_ROMFS)"

verify: $(PATCHED) $(RELEASE_IPS) layeredfs $(TOOLS)/verify_patch.py
	"$(PYTHON)" -B -m tools.verify_patch --base "$(BASE)" --patched "$(PATCHED)" --symbols "$(SYMBOLS)" --ips "$(RELEASE_IPS)" --source-romfs "$(ROMFS_SOURCE)" --output-romfs "$(RELEASE_ROMFS)"

clean:
	rm -rf "$(RELEASE_TITLE)" "$(BUILD)"

FORCE:
