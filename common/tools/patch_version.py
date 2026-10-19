"""One build label for title resources and the embedded patch identifier.

标题资源与补丁内嵌标识共用构建标签；仅明确的正式构建省略 Git 简写。
"""

from __future__ import annotations

import argparse
import re
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
STORAGE_SIZE = 64


def patch_label(version: str = "", *, release: bool = False, revision: str | None = None) -> str:
    version = version or (ROOT / "VERSION").read_text(encoding="ascii").strip()
    if not re.fullmatch(r"(?:0|[1-9][0-9]*)\.(?:0|[1-9][0-9]*)\.(?:0|[1-9][0-9]*)", version):
        raise ValueError("PATCH_VERSION must be a numeric major.minor.patch version")
    label = f"Offline Patch v{version}"
    if not release:
        if revision is None:
            try:
                revision = subprocess.check_output(
                    ["git", "-C", str(ROOT), "rev-parse", "--short=6", "HEAD"],
                    text=True, stderr=subprocess.DEVNULL,
                ).strip()
            except (OSError, subprocess.CalledProcessError) as error:
                raise ValueError("Development builds require Git or an explicit PATCH_REVISION") from error
        if not re.fullmatch(r"[0-9a-fA-F]{6,40}", revision):
            raise ValueError("PATCH_REVISION must be the Git hexadecimal revision (at least six digits)")
        label += "-" + revision.lower()
    if len(label.encode("ascii")) + 1 > STORAGE_SIZE:
        raise ValueError("patch label exceeds its reserved 64-byte identifier")
    return label


def label_storage(label: str) -> bytes:
    raw = label.encode("ascii")
    if not raw or b"\0" in raw or len(raw) >= STORAGE_SIZE:
        raise ValueError("invalid patch identifier")
    return (raw + b"\0").ljust(STORAGE_SIZE, b"\0")


def add_version_arguments(parser: argparse.ArgumentParser) -> None:
    parser.add_argument("--patch-version", default="", help="base version; defaults to repository VERSION")
    parser.add_argument("--patch-release", choices=("0", "1"), default="0", help="1 explicitly omits the Git suffix")
    parser.add_argument("--patch-revision", default=None, help="Git short revision for development builds")


def label_from_arguments(args: argparse.Namespace) -> str:
    return patch_label(args.patch_version, release=args.patch_release == "1", revision=args.patch_revision)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    add_version_arguments(parser)
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()
    label = label_from_arguments(args)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_bytes(label_storage(label))
    print(label)


if __name__ == "__main__":
    main()
