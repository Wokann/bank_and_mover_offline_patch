"""Bank title: mode above START, independent R/L/HOME rows below it."""
from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "common/tools"))
import title_resources as shared
from patch_version import patch_label as build_patch_label
from title_resources import (
    MODE_TEXT_CAPACITY, MODE_PANE_X, MODE_PANE_Y, MODE_PANE_TOP, MODE_PANE_WIDTH,
    MODE_PANE_HEIGHT, MODE_LINE_SPACING, MODE_SCREEN_MARGIN,
    compress_lz11, decompress_lz11, package_files, sections,
)

TITLE_LAYOUT_ARCHIVE = "0/4/2"
LANGUAGE_PANE_ID, LANGUAGE_PANE_NAME = 41, b"15"
MODE_PANE_ID, MODE_PANE_NAME = 42, b"16"
SWITCH_PANE_ID, SWITCH_PANE_NAME = 43, b"17"
LOWER_LAYOUT = "Turtle_lower6.bclyt"
UPPER_LAYOUT, NATIVE_VERSION_PANE_NAME = "Turtle_upper6.bclyt", b"10"
PATCH_VERSION_PANE_ID, PATCH_VERSION_PANE_NAME = 46, b"PatchVersion"
USER_STATUS_PANE_ID, USER_STATUS_PANE_NAME = 44, b"18"
USER_ID_PANE_ID, USER_ID_PANE_NAME = 45, b"19"
EXTRA_PANES = ((LANGUAGE_PANE_NAME, "language"), (MODE_PANE_NAME, "mode"), (SWITCH_PANE_NAME, "switch"),
               (USER_STATUS_PANE_NAME, "user_status"), (USER_ID_PANE_NAME, "user_id"),
               (PATCH_VERSION_PANE_NAME, "patch_version"))
MODE_TEXT_COLOR = b"\xff" * 4  # Keep Bank's native white; 保留 Bank 原版白色。


def patch_title_archive(data: bytes, *, patch_label: str | None = None) -> bytes:
    return shared.patch_title_archive(data, native_pane_count=LANGUAGE_PANE_ID,
                                      lower_layout=LOWER_LAYOUT, extra_panes=EXTRA_PANES,
                                      mode_text_color=MODE_TEXT_COLOR, upper_layout=UPPER_LAYOUT,
                                      native_version_pane=NATIVE_VERSION_PANE_NAME,
                                      patch_label=build_patch_label() if patch_label is None else patch_label)


def build_title_archive(source_romfs: Path, output_romfs: Path, *, patch_label: str | None = None) -> None:
    shared.build_title_archive(source_romfs, output_romfs, archive=TITLE_LAYOUT_ARCHIVE,
                              native_pane_count=LANGUAGE_PANE_ID, lower_layout=LOWER_LAYOUT,
                              extra_panes=EXTRA_PANES, mode_text_color=MODE_TEXT_COLOR,
                              upper_layout=UPPER_LAYOUT, native_version_pane=NATIVE_VERSION_PANE_NAME,
                              patch_label=build_patch_label() if patch_label is None else patch_label)
