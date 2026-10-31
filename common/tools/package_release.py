"""Package verified patches and configuration; include project guides in release builds.

使用 7-Zip 打包两端补丁与空配置模板；正式构建附带根目录说明和更新日志。
"""
from __future__ import annotations

import argparse
from collections import Counter
from datetime import datetime, timedelta, timezone
import hashlib
import os
from pathlib import Path, PurePosixPath
import shutil
import subprocess
import tempfile
import zipfile
import zlib

from check_file_links import inspect_paths
from patch_version import ROOT, add_version_arguments, label_from_arguments, label_storage

TITLES = {"bank": "00040000000C9B00", "mover": "00040000000C9C00"}
FOLDERS = (*TITLES.values(), "Bank")
DOCUMENTS = ("README.md", "README.zh-cn.md", "CHANGELOG.md", "CHANGELOG.zh-cn.md")


def digest(stream) -> str:
    result = hashlib.sha256()
    for block in iter(lambda: stream.read(1024 * 1024), b""):
        result.update(block)
    return result.hexdigest()


def manifest(directory: Path) -> dict[str, str]:
    result = {}
    for path in sorted(directory.rglob("*")):
        if path.is_file():
            with path.open("rb") as stream:
                result[path.relative_to(directory).as_posix()] = digest(stream)
    return result


def sevenzip_tool(requested: str = "") -> str:
    if requested:
        result = shutil.which(requested)
        if not result and Path(requested).is_file():
            result = str(Path(requested).absolute())
        if result:
            return result
        raise ValueError("7-Zip not found; set SEVENZIP to its executable")
    if os.name == "nt":
        for variable in ("ProgramFiles", "ProgramW6432", "ProgramFiles(x86)"):
            if os.environ.get(variable):
                path = Path(os.environ[variable]) / "7-Zip" / "7z.exe"
                if path.is_file():
                    return str(path)
    for name in ("7z", "7zz", "7za"):
        result = shutil.which(name)
        if result:
            return result
    raise ValueError("Install 7-Zip or set SEVENZIP to its executable")


def collect_inputs(root: Path, label: str, *, include_documents: bool = False) -> dict[str, str]:
    release = root / "release"
    template = root / "configTemplate" / "templateForCopy" / "Bank"
    paths = [release / folder for folder in FOLDERS] + [template]
    if include_documents:
        paths.extend(root / name for name in DOCUMENTS)
    for title, title_id in TITLES.items():
        paths.extend((root / title / "build" / "patch-version.bin",
                      root / title / "build" / (title_id + ".dec.code"),
                      root / title / "build" / "exheader.bin"))
    report = inspect_paths(paths, recursive=True)
    if report["issues"]:
        raise ValueError("Package inputs contain missing, linked or unreadable paths")
    if manifest(release / "Bank") != manifest(template):
        raise ValueError("release/Bank must exactly match the complete empty-user template")
    for group in ("client", "fakeServers"):
        users = release / "Bank" / group / "users"
        if users.is_dir() and any(path.is_dir() for path in users.iterdir()):
            raise ValueError("The package must not contain account directories")
    expected = {}
    for folder in FOLDERS:
        for name, checksum in manifest(release / folder).items():
            if PurePosixPath(name).name in ("sav.bin", "sav.bak", "sav.tmp", "bankdata.bin",
                                           "bankdata.bak", "bankdata.tmp", "transaction.bin",
                                           "transaction.bak", "transaction.tmp"):
                raise ValueError("The package must not contain player data")
            if folder != "Bank" and name not in ("code.bps", "exheader.bin") and not name.startswith("romfs/"):
                raise ValueError(f"Unexpected file in patch output: {folder}/{name}")
            expected[folder + "/" + name] = checksum
    if include_documents:
        for name in DOCUMENTS:
            with (root / name).open("rb") as stream:
                expected[name] = digest(stream)
    for title, title_id in TITLES.items():
        build = root / title / "build"
        if (build / "patch-version.bin").read_bytes() != label_storage(label):
            raise ValueError(f"{title} has a different build version; rebuild both titles")
        image = (build / (title_id + ".dec.code")).read_bytes()
        if label_storage(label) not in image:
            raise ValueError(f"{title} code does not embed the expected build version")
        patch = (release / title_id / "code.bps").read_bytes()
        if (len(patch) < 19 or not patch.startswith(b"BPS1") or
                zlib.crc32(patch[:-4]) != int.from_bytes(patch[-4:], "little") or
                zlib.crc32(image) != int.from_bytes(patch[-8:-4], "little")):
            raise ValueError(f"{title} BPS does not match the verified build")
        if (release / title_id / "exheader.bin").read_bytes() != (build / "exheader.bin").read_bytes():
            raise ValueError(f"{title} ExHeader does not match the verified build")
    return expected


def verify_archive(path: Path, expected: dict[str, str]) -> Counter:
    methods = Counter()
    with zipfile.ZipFile(path) as archive:
        entries = archive.infolist()
        names = [entry.filename for entry in entries]
        if len(names) != len(set(names)):
            raise ValueError("Duplicate archive entries")
        for name in names:
            parts = PurePosixPath(name).parts
            if (not parts or (parts[0] not in FOLDERS and name not in DOCUMENTS) or name.startswith("/") or
                    ".." in parts or ":" in name or "\\" in name):
                raise ValueError("Unexpected archive path")
        files = {entry.filename: entry for entry in entries if not entry.is_dir()}
        if files.keys() != expected.keys():
            raise ValueError("Archive contains missing or extra files")
        for name, entry in files.items():
            # 7-Zip may store tiny/incompressible files instead of enlarging them.
            # 极小或不可压缩的文件允许由 7-Zip 自动选择原样存储。
            if entry.flag_bits & 1 or entry.compress_type not in (zipfile.ZIP_LZMA, zipfile.ZIP_STORED):
                raise ValueError("Archive must use ZIP LZMA without encryption")
            with archive.open(entry) as stream:
                if digest(stream) != expected[name]:
                    raise ValueError(f"Archive content differs: {name}")
            methods[entry.compress_type] += 1
    return methods


def package(root: Path, label: str, tool: str, *, include_documents: bool = False) -> Path:
    expected = collect_inputs(root, label, include_documents=include_documents)
    release = root / "release"
    stamp = datetime.now(timezone(timedelta(hours=8))).strftime("%Y%m%d%H%M")
    output = release / (label + "-" + stamp + ".zip")
    if output.exists():
        raise FileExistsError(f"Archive already exists: {output.name}; it will not be overwritten")
    with tempfile.TemporaryDirectory(prefix=".package-", dir=release) as temporary:
        archive = Path(temporary) / output.name
        inputs = [(release, FOLDERS)]
        if include_documents:
            inputs.append((root, DOCUMENTS))
        for directory, names in inputs:
            subprocess.run([tool, "a", "-tzip", "-mm=LZMA", "-mx=9", "-mmt=on", "-sse", "-bb0", "-bsp0",
                            str(archive), *names], cwd=directory, check=True)
        subprocess.run([tool, "t", "-bsp0", str(archive)], cwd=release, check=True)
        methods = verify_archive(archive, expected)
        if collect_inputs(root, label, include_documents=include_documents) != expected:
            raise ValueError("Build outputs changed during packaging")
        if output.exists():
            raise FileExistsError(f"Archive already exists: {output.name}; it will not be overwritten")
        # Reserve a new name exclusively, then replace only our own empty file.
        # 独占创建新文件名，再以完整压缩包替换自己的占位，不覆盖已有文件。
        with output.open("xb"):
            pass
        try:
            archive.replace(output)
        except BaseException:
            output.unlink()
            raise
    with output.open("rb") as stream:
        checksum = digest(stream)
    print(f"Created release/{output.name} ({output.stat().st_size / 1000000:.2f} MB)")
    print(f"Verified {len(expected)} files; LZMA: {methods[zipfile.ZIP_LZMA]}; stored: {methods[zipfile.ZIP_STORED]}")
    print(f"SHA-256: {checksum}")
    return output


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    add_version_arguments(parser)
    parser.add_argument("--sevenzip", default="", help="7-Zip executable; auto-detected when empty")
    args = parser.parse_args()
    try:
        package(ROOT, label_from_arguments(args), sevenzip_tool(args.sevenzip),
                include_documents=args.patch_release == "1")
    except (OSError, ValueError, subprocess.CalledProcessError, zipfile.BadZipFile) as error:
        parser.exit(1, f"Package failed: {error}\n")


if __name__ == "__main__":
    main()
