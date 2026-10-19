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
TITLE_ID := 00040000000C9B00
BASE := rom/exefs/00040000000C9B00.dec.code
PATCHED := $(BUILD)/00040000000C9B00.dec.code
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
# Bank does not use the UTF-16 directory and ranged-file extension.
# Bank 不使用 UTF-16 目录和文件分段操作扩展，不注入未调用的对象。
FS_OBJECTS := $(patsubst $(COMMON)/src/%.c,$(BUILD)/common/%.o,$(filter-out $(FS_SRC)/fs_utf16.c,$(wildcard $(FS_SRC)/*.c)))
COMMON_OBJECTS := $(SERVER_OBJECTS) $(FS_OBJECTS)
OBJECTS := $(patsubst $(SRC)/%.c,$(BUILD)/%.o,$(wildcard $(SRC)/*.c)) $(COMMON_OBJECTS)
SYMBOLS := $(BUILD)/armips-symbols.txt
EXHEADER ?= rom/exheader.bin
PREPARED_BASE := $(BUILD)/expanded-base.code
EXPANDED_EXHEADER := $(BUILD)/exheader.bin
RELEASE_EXHEADER := $(RELEASE_TITLE)/exheader.bin

CC := $(DEVKITARM)/bin/arm-none-eabi-gcc
OBJDUMP := $(DEVKITARM)/bin/arm-none-eabi-objdump
CFLAGS := -std=c11 -Os -march=armv6k -mtune=mpcore -marm -mfloat-abi=soft \
	-ffreestanding -fno-builtin -fno-stack-protector -fno-unwind-tables \
	-fno-asynchronous-unwind-tables -fomit-frame-pointer -fno-common \
	-ffunction-sections -fdata-sections -MMD -MP -Wall -Wextra -Werror \
	-mlong-calls -Iinclude -I$(COMMON)/include

-include $(OBJECTS:.o=.d)

# Compact C-only helpers use Thumb; assembly entry points explicitly stay ARM.
# 纯 C 调用的辅助函数使用 Thumb；汇编直达入口显式保留 ARM。
$(BUILD)/server_ticket.o $(BUILD)/common/fakeServer/ticket.o \
$(BUILD)/common/fakeServer/fake_server_client.o \
$(BUILD)/common/fakeServer/user_profiles.o \
$(BUILD)/common/fakeServer/console_profile.o \
$(BUILD)/common/fs_system/protected_file.o \
$(BUILD)/common/fs_system/storage_notice.o \
$(BUILD)/common/fs_system/diagnostic_log.o \
$(BUILD)/common/fs_system/fs_helpers.o \
$(BUILD)/common/fs_system/utf_path.o \
$(BUILD)/common/fs_system/settings_store.o \
$(BUILD)/common/fs_system/console_settings.o \
$(BUILD)/common/fakeServer/numbers.o \
$(BUILD)/common/fakeServer/gift_files.o \
$(BUILD)/common/fakeServer/custom_settings.o \
$(BUILD)/common/fakeServer/source.o \
$(BUILD)/common/fakeServer/gifts.o \
$(BUILD)/common/fakeServer/request_broker.o \
$(BUILD)/common/fakeServer/account_configuration.o \
$(BUILD)/common/fakeServer/sd_adapter.o \
$(BUILD)/common/fs_system/console_identity.o \
$(BUILD)/common/fakeServer/configuration.o \
$(BUILD)/common/fakeServer/configuration_edit.o \
$(BUILD)/common/fakeServer/ticket_settings.o \
$(BUILD)/common/fakeServer/mover_checks.o \
$(BUILD)/common/fakeServer/mover_expression.o \
$(BUILD)/common/fakeServer/word_pattern.o \
$(BUILD)/common/fakeServer/memory.o \
$(BUILD)/common/fakeServer/service_request.o \
$(BUILD)/common/fakeServer/fake_servers.o \
$(BUILD)/common/fakeServer/transaction.o: CFLAGS += -mthumb -mtp=soft -fno-jump-tables

# Standalone payloads have no Thumb jump-table runtime helper.
# 独立补丁不链接 Thumb 跳转表运行库，由编译器使用直接分支。

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
	"$(PYTHON)" -B -m tools.prepare_expanded_code prepare --base-code "$(BASE)" --base-exheader "$(EXHEADER)" --output-code "$@" --output-exheader "$(EXPANDED_EXHEADER)" --payload-start 0x003FC000 --payload-size 0x23A000

$(VERSION_DATA): FORCE ../VERSION $(COMMON)/tools/patch_version.py | $(BUILD) check-files
	"$(PYTHON)" -B $(COMMON)/tools/patch_version.py $(PATCH_VERSION_ARGS) --output "$@"

$(PATCHED): FORCE $(SRC)/main.s $(OBJECTS) $(PREPARED_BASE) $(VERSION_DATA)
	cd "$(SRC)" && "$(ARMIPS)" -erroronwarning -stat -sym "../$(SYMBOLS)" -strequ INPUT_CODE "../$(PREPARED_BASE)" -strequ OUTPUT_CODE "../$(PATCHED)" -strequ BUILD_DIRECTORY "../$(BUILD)" main.s

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
	"$(PYTHON)" -B -m tools.verify_patch --base "$(BASE)" --patched "$(PATCHED)" --symbols "$(SYMBOLS)" --bps "$(RELEASE_BPS)" --base-exheader "$(EXHEADER)" --exheader "$(RELEASE_EXHEADER)" --source-romfs "$(ROMFS_SOURCE)" --output-romfs "$(RELEASE_ROMFS)" $(PATCH_VERSION_ARGS)

# Ship the complete empty-user template, never the example account data.
# 分发完整空用户模板，不采用带演示账号的数据。
config: | check-files
	"$(PYTHON)" -B -c "import shutil, sys; shutil.copytree(sys.argv[1], sys.argv[2], dirs_exist_ok=True)" "$(CONFIG_TEMPLATE)" "$(RELEASE_CONFIG)"

clean: | check-files
	rm -rf "$(RELEASE_TITLE)" "$(BUILD)"

FORCE:
