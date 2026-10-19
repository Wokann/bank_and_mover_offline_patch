.PHONY: all clean verify FORCE layeredfs exheader config check-files
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
CONFIG_TEMPLATE := ../configTemplate/templateForCopy/Bank
RELEASE_CONFIG := $(RELEASE_ROOT)/Bank
ROMFS_SOURCE ?= rom/romfs
COMMON := ../common
PATCH_VERSION ?=
PATCH_RELEASE ?= 0
PATCH_REVISION ?= $(shell git rev-parse --short=6 HEAD 2>/dev/null)
PATCH_REVISION := $(strip $(PATCH_REVISION))
PATCH_VERSION_ARGS := --patch-version "$(PATCH_VERSION)" --patch-release "$(PATCH_RELEASE)" --patch-revision "$(PATCH_REVISION)"
VERSION_DATA := $(BUILD)/patch-version.bin
FONT_NAMES := $(wildcard ../ViewerForBankdata/resources/species_*.json)
SERVER_SRC := $(COMMON)/src/fakeServer
FS_SRC := $(COMMON)/src/fs_system
SERVER_OBJECTS := $(patsubst $(COMMON)/src/%.c,$(BUILD)/common/%.o,$(wildcard $(SERVER_SRC)/*.c))
FS_OBJECTS := $(patsubst $(COMMON)/src/%.c,$(BUILD)/common/%.o,$(wildcard $(FS_SRC)/*.c))
COMMON_OBJECTS := $(SERVER_OBJECTS) $(FS_OBJECTS)
OBJECTS := $(patsubst $(SRC)/%.c,$(BUILD)/%.o,$(wildcard $(SRC)/*.c)) $(COMMON_OBJECTS)
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
	-mlong-calls -Iinclude -I$(COMMON)/include
THUMB_CFLAGS := $(filter-out -marm,$(ARM_CFLAGS)) -mthumb -fno-jump-tables
CFLAGS := $(ARM_CFLAGS)

$(BUILD)/bankdata_redirect.o $(BUILD)/server_validation.o \
$(BUILD)/nds_sources.o \
$(BUILD)/language_settings.o $(BUILD)/bank_native_settings.o $(COMMON_OBJECTS): CFLAGS := $(THUMB_CFLAGS)

# Time observation and shared-buffer arbitration use ARM-only barriers/atomics.
# 时间读取与共享缓冲仲裁使用 ARM 模式的内存屏障／原子指令。
$(BUILD)/common/fs_system/system_time.o $(BUILD)/common/fs_system/runtime_environment.o \
$(BUILD)/common/fs_system/native_worker.o $(BUILD)/common/fakeServer/async_task.o \
$(BUILD)/common/fs_system/diagnostic_log.o \
$(BUILD)/common/fs_system/service_worker.o \
$(BUILD)/common/fakeServer/native_transport.o \
$(BUILD)/common/fs_system/title_users.o \
$(BUILD)/common/fakeServer/transaction_view.o: CFLAGS := $(ARM_CFLAGS)

-include $(OBJECTS:.o=.d)

all: check-files $(RELEASE_BPS) exheader layeredfs config verify

check-files:
	"$(PYTHON)" -B ../common/tools/check_file_links.py --recursive --allow-missing .. "$(BUILD)" "$(RELEASE_ROOT)" "$(ROMFS_SOURCE)" "$(BASE)" "$(EXHEADER)"

$(BUILD): | check-files
	mkdir -p "$@"

$(BUILD)/common/%.o: $(COMMON)/src/%.c | $(BUILD) check-files
	mkdir -p "$(dir $@)"
	"$(CC)" $(CFLAGS) -c "$<" -o "$@"
	"$(OBJDUMP)" -dr -z "$@" > "$(@:.o=.s)"

$(BUILD)/%.o: $(SRC)/%.c | $(BUILD) check-files
	"$(CC)" $(CFLAGS) -c "$<" -o "$@"
	"$(OBJDUMP)" -dr -z "$@" > "$(@:.o=.s)"

$(PREPARED_BASE): FORCE $(BASE) $(EXHEADER) $(TOOLS)/prepare_expanded_code.py | $(BUILD) check-files
	"$(PYTHON)" -B -m tools.prepare_expanded_code prepare --base-code "$(BASE)" --base-exheader "$(EXHEADER)" --output-code "$@" --output-exheader "$(EXPANDED_EXHEADER)" --payload-start 0x00365000 --payload-size 0x2BF000

$(VERSION_DATA): FORCE ../VERSION $(COMMON)/tools/patch_version.py | $(BUILD) check-files
	"$(PYTHON)" -B $(COMMON)/tools/patch_version.py $(PATCH_VERSION_ARGS) --output "$@"

$(PATCHED): FORCE $(SRC)/main.s $(OBJECTS) $(ASSEMBLY_BASE) $(TOOLS)/verify_patch.py $(VERSION_DATA)
	cd "$(SRC)" && "$(ARMIPS)" -erroronwarning -stat -sym "../$(SYMBOLS)" -strequ INPUT_CODE "../$(ASSEMBLY_BASE)" -strequ OUTPUT_CODE "../$(PATCHED)" -strequ BUILD_DIRECTORY "../$(BUILD)" main.s

$(RELEASE_EXHEADER): $(PATCHED) | check-files
	mkdir -p "$(dir $@)"
	cp "$(EXPANDED_EXHEADER)" "$@"

exheader: $(RELEASE_EXHEADER)

$(RELEASE_BPS): $(PATCHED) | check-files
	mkdir -p "$(dir $@)"
	rm -f "$@"
	"$(IPS_TOOL)" --create --bps --exact "$(BASE)" "$(PATCHED)" "$@"
	rm -f "$(RELEASE_TITLE)/code.ips"

layeredfs: $(SRC)/patch_messages.py $(TOOLS)/message_archive.py $(TOOLS)/title_layout.py $(COMMON)/tools/title_resources.py $(COMMON)/tools/title_messages.py $(COMMON)/tools/storage_messages.py $(COMMON)/tools/title_font.py $(COMMON)/tools/patch_version.py ../VERSION $(FONT_NAMES) | check-files
	rm -rf "$(RELEASE_ROMFS)"
	"$(PYTHON)" -B -m src.patch_messages --source-romfs "$(ROMFS_SOURCE)" --output-romfs "$(RELEASE_ROMFS)" $(PATCH_VERSION_ARGS)

verify: $(PATCHED) $(RELEASE_BPS) exheader layeredfs $(TOOLS)/verify_patch.py
	"$(PYTHON)" -B -m tools.verify_patch --base "$(BASE)" --patched "$(PATCHED)" --symbols "$(SYMBOLS)" --bps "$(RELEASE_BPS)" --source-romfs "$(ROMFS_SOURCE)" --output-romfs "$(RELEASE_ROMFS)" $(VERIFY_EXHEADER) $(PATCH_VERSION_ARGS)

# Ship the complete empty-user template, never the example account data.
# 分发完整空用户模板，不采用带演示账号的数据。
config: | check-files
	"$(PYTHON)" -B -c "import shutil, sys; shutil.copytree(sys.argv[1], sys.argv[2], dirs_exist_ok=True)" "$(CONFIG_TEMPLATE)" "$(RELEASE_CONFIG)"

clean: | check-files
	rm -rf "$(RELEASE_TITLE)" "$(BUILD)"

FORCE:
