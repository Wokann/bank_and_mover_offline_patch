.PHONY: all package
.DEFAULT_GOAL := all

export MSYS2_ARG_CONV_EXCL := *

PYTHON ?= python3
SEVENZIP ?=
PATCH_VERSION ?=
PATCH_RELEASE ?= 0
PATCH_REVISION ?= $(shell git rev-parse --short=6 HEAD 2>/dev/null)
PATCH_REVISION := $(strip $(PATCH_REVISION))
export PYTHON PATCH_VERSION PATCH_RELEASE PATCH_REVISION

# Both titles publish the same default Bank directory; build them in order.
# 两端共用默认 Bank 输出目录，按顺序构建，避免并行覆盖。
all:
	+$(MAKE) -C bank all
	+$(MAKE) -C mover all

package: all
	"$(PYTHON)" -B common/tools/package_release.py --sevenzip "$(SEVENZIP)" --patch-version "$(PATCH_VERSION)" --patch-release "$(PATCH_RELEASE)" --patch-revision "$(PATCH_REVISION)"
