#!/usr/bin/env python3
"""Prepare fixed-address Mover .code/.exheader pairs for expanded layouts.

The original .text/.rodata/.data addresses and .text's *actual* size stay fixed.
Original BSS becomes explicit zero-filled .data; the extra code is assembled into
a page-aligned reservation after it.  This command does not insert the payload
or make the pages executable: the small bootstrap does MEMOP_PROT at runtime.
The matching BPS declares the expanded image length, allowing an unmodified
Azahar loader to resize its code buffer without a separate ExHeader layout.

Public implementation references (not extracted game source):
  https://github.com/RicBent/Magikoopa/blob/master/MagikoopaUI/patchmaker.cpp
  https://github.com/LumaTeam/Luma3DS/blob/master/sysmodules/loader/source/loader.c
  https://github.com/LumaTeam/Luma3DS/blob/master/sysmodules/loader/source/patcher.c
  https://github.com/devkitPro/libctru/blob/master/libctru/include/3ds/svc.h
  https://github.com/LumaTeam/Luma3DS/blob/master/sysmodules/loader/source/bps_patcher.cpp
  https://github.com/azahar-emu/azahar/blob/9e6f523a57fac9564ac0bf8286db3c3702d301ec/src/core/file_sys/patch.cpp

Unlike Magikoopa's text-size rounding, retaining the actual text size preserves
Luma's 0x130-byte LayeredFS injection reservation.  The BPS target length requires
this matching exheader: Luma checks against its new allocation.
The service ACL adds ps:ps for random local transaction identifiers. The
permitted-save whitelist adds only Bank (0xC9B) for language synchronization.
Only the real ARM11 capabilities at 0x370 are changed; the signed access-descriptor
copy (in a full 0x800-byte header) is preserved.  This is a CFW override, not a
newly signed retail NCCH.
"""

from __future__ import annotations

import argparse
import subprocess
import hashlib
from pathlib import Path
import struct
import sys


PAGE_SIZE = 0x1000
IMAGE_BASE = 0x00100000
TITLE_ID = 0x00040000000C9C00
BASE_SHA256 = "001c20ada74016507c969bb44a0a50f8edf803ec06a3fc46834263ba8df0fd2f"
TEXT_INFO = (0x00100000, 0x18E, 0x18D1AC)
RO_INFO = (0x0028E000, 0x5E, 0x5DA58)
DATA_INFO = (0x002EC000, 0x3E, 0x3D3FC)
BSS_SIZE = 0x3A4A8
DATA_ACTUAL_END = DATA_INFO[0] + DATA_INFO[2]
BSS_ACTUAL_END = DATA_ACTUAL_END + BSS_SIZE
OLD_RW_MAPPED_END = (BSS_ACTUAL_END + PAGE_SIZE - 1) & -PAGE_SIZE
DEFAULT_PAYLOAD_START = 0x00365000
DEFAULT_PAYLOAD_SIZE = 0x2BF000
KERNEL_CAPS_OFFSET = 0x370
KERNEL_CAPS_COUNT = 28
REQUIRED_SVCS = frozenset((0x23, 0x27, 0x2A, 0x3C, 0x70))


def require(condition: bool, message: str) -> None:
    if not condition:
        raise ValueError(message)


def svc_groups(header: bytes | bytearray) -> dict[int, tuple[int, int]]:
    groups: dict[int, tuple[int, int]] = {}
    for index, value in enumerate(struct.unpack_from("<28I", header, KERNEL_CAPS_OFFSET)):
        if value & 0xF8000000 == 0xF0000000:
            group = (value >> 24) & 7
            require(group not in groups, f"duplicate SVC mask group {group}: ambiguous input")
            groups[group] = (index, value)
    return groups


def allowed_svcs(header: bytes | bytearray) -> set[int]:
    return {
        group * 24 + bit
        for group, (_, value) in svc_groups(header).items()
        for bit in range(24)
        if value & (1 << bit)
    }


def grant_required_svcs(header: bytearray) -> set[int]:
    """Set only required bits, without reordering non-SVC descriptors."""
    previous = allowed_svcs(header)
    groups = svc_groups(header)
    free_slots = [
        index for index, value in
        enumerate(struct.unpack_from("<28I", header, KERNEL_CAPS_OFFSET))
        if value == 0xFFFFFFFF
    ]
    for svc in sorted(REQUIRED_SVCS - previous):
        group, bit = divmod(svc, 24)
        if group in groups:
            index, value = groups[group]
        else:
            require(bool(free_slots), "no unused kernel capability slot for required SVC mask")
            index, value = free_slots.pop(0), 0xF0000000 | (group << 24)
        value |= 1 << bit
        struct.pack_into("<I", header, KERNEL_CAPS_OFFSET + index * 4, value)
        groups[group] = (index, value)
    require(allowed_svcs(header) == previous | REQUIRED_SVCS,
            "SVC mask update changed unrelated permissions")
    return REQUIRED_SVCS - previous


SERVICE_ACL_OFFSET = 0x250
SERVICE_ACL_SIZE = 0x100
SAVE_ACCESS_OFFSET = 0x240
BANK_SAVE_UNIQUE_ID = 0xC9B


def grant_bank_save(header: bytearray) -> None:
    """Add one permitted title; retain variation flags, FS rights and other IDs."""
    offsets = [SAVE_ACCESS_OFFSET]
    if header[0x24F] & 2:
        offsets.append(0x230)  # Extended six-ID layout; otherwise extdata is not a whitelist.
    slots = [(offset, shift) for offset in offsets for shift in (0, 20, 40)]
    if any((struct.unpack_from('<Q',header,offset)[0] >> shift) & 0xFFFFF == BANK_SAVE_UNIQUE_ID
           for offset,shift in slots):
        return
    for offset,shift in slots:
        value=struct.unpack_from('<Q',header,offset)[0]
        if not ((value >> shift) & 0xFFFFF):
            struct.pack_into('<Q',header,offset,value | (BANK_SAVE_UNIQUE_ID << shift))
            return
    raise ValueError('no unused permitted-save whitelist slot for Bank')


def grant_random_service(header: bytearray) -> None:
    """Add only PS randomness access, preserving all existing service entries."""
    name = b"ps:ps\0\0\0"
    slots = range(SERVICE_ACL_OFFSET, SERVICE_ACL_OFFSET + SERVICE_ACL_SIZE, 8)
    if any(header[offset:offset + 8] == name for offset in slots):
        return
    for offset in slots:
        if not any(header[offset:offset + 8]):
            header[offset:offset + 8] = name
            return
    raise ValueError("no unused service ACL slot for PS randomness")


def verify_layout(header: bytes, base_header: bytes | None = None,
                  payload_start: int = DEFAULT_PAYLOAD_START,
                  payload_size: int = DEFAULT_PAYLOAD_SIZE) -> None:
    """Validate an expanded exheader, optionally requiring the exact safe delta.

    The release verifier can import this without ROM files.  Supplying the
    original header also checks every untouched byte and every other capability.
    """
    require(len(header) in (0x400, 0x800), "expanded exheader must be 0x400 or 0x800 bytes")
    require(struct.unpack_from("<Q", header, 0x200)[0] == TITLE_ID,
            "expanded exheader title ID differs")
    require(struct.unpack_from("<III", header, 0x10) == TEXT_INFO,
            "expanded exheader changes the original text mapping/actual-size")
    require(struct.unpack_from("<III", header, 0x20) == RO_INFO,
            "expanded exheader changes the original rodata mapping/actual-size")
    data_size = payload_start + payload_size - DATA_INFO[0]
    require(payload_start >= OLD_RW_MAPPED_END and payload_start % PAGE_SIZE == 0 and
            payload_size > 0 and payload_size % PAGE_SIZE == 0 and
            payload_start + payload_size <= 0x14000000,
            "invalid expanded payload reservation")
    expected_data = (DATA_INFO[0], data_size // PAGE_SIZE, data_size)
    require(struct.unpack_from("<III", header, 0x30) == expected_data,
            "expanded exheader data address/pages/size differs")
    require(struct.unpack_from("<I", header, 0x3C)[0] == 0,
            "expanded exheader BSS size differs")
    require(REQUIRED_SVCS <= allowed_svcs(header), "expanded exheader lacks bootstrap SVC rights")
    if base_header is not None:
        require(len(base_header) == len(header), "original/expanded exheader lengths differ")
        expected = bytearray(base_header)
        struct.pack_into("<III", expected, 0x34, data_size // PAGE_SIZE, data_size, 0)
        grant_required_svcs(expected)
        grant_random_service(expected)
        grant_bank_save(expected)
        require(header == expected, "expanded exheader contains changes outside the exact safe delta")


def prepare_images(base_code: bytes, base_header: bytes, payload_start: int,
                   payload_size: int, *, check_hash: bool = True) -> tuple[bytes, bytes, set[int]]:
    require(len(base_header) in (0x400, 0x800), "exheader must be 0x400 or 0x800 bytes")
    if check_hash:
        require(hashlib.sha256(base_code).hexdigest() == BASE_SHA256,
                "base .code SHA-256 does not match the supported unmodified Mover image")
    require(struct.unpack_from("<Q", base_header, 0x200)[0] == TITLE_ID,
            "exheader title ID is not the supported Mover title")
    for offset, expected, name in ((0x10, TEXT_INFO, ".text"),
                                    (0x20, RO_INFO, ".rodata"),
                                    (0x30, DATA_INFO, ".data")):
        require(struct.unpack_from("<III", base_header, offset) == expected,
                f"unsupported {name} address/pages/actual-size profile")
    require(struct.unpack_from("<I", base_header, 0x3C)[0] == BSS_SIZE,
            "unsupported original BSS size")
    expected_length = sum(info[1] for info in (TEXT_INFO, RO_INFO, DATA_INFO)) * PAGE_SIZE
    require(len(base_code) == expected_length, "base .code length differs from its segment pages")
    require(TEXT_INFO[0] + TEXT_INFO[1] * PAGE_SIZE == RO_INFO[0] and
            RO_INFO[0] + RO_INFO[1] * PAGE_SIZE == DATA_INFO[0],
            "this tool requires the fixed contiguous original segment layout")
    require(0 <= payload_start < 0x14000000 and payload_start % PAGE_SIZE == 0,
            "payload start must be page-aligned in the process code address range")
    require(payload_start >= OLD_RW_MAPPED_END,
            "payload reservation overlaps original data/BSS/runtime storage")
    require(payload_size > 0 and payload_size % PAGE_SIZE == 0,
            "payload size must be a positive whole number of pages")
    payload_end = payload_start + payload_size
    require(payload_end <= 0x14000000, "payload end exceeds the code address range")
    # This profile's existing last .data page also contains the first BSS bytes.
    # They must already be zero; retaining the entire input prefix is intentional.
    require(not any(base_code[DATA_ACTUAL_END - IMAGE_BASE:]),
            "original data-page padding is nonzero; cannot safely materialize BSS")

    code = base_code + bytes(payload_end - IMAGE_BASE - len(base_code))
    header = bytearray(base_header)
    data_size = payload_end - DATA_INFO[0]
    struct.pack_into("<III", header, 0x34, data_size // PAGE_SIZE, data_size, 0)
    added = grant_required_svcs(header)
    grant_random_service(header)
    grant_bank_save(header)

    mutable_offsets = set(range(0x34, 0x40)) | set(range(0x230,0x238)) | set(range(0x240,0x248)) | set(range(SERVICE_ACL_OFFSET,
        SERVICE_ACL_OFFSET + SERVICE_ACL_SIZE)) | set(
        range(KERNEL_CAPS_OFFSET, KERNEL_CAPS_OFFSET + KERNEL_CAPS_COUNT * 4))
    require(all(old == new or index in mutable_offsets
                for index, (old, new) in enumerate(zip(base_header, header))),
            "unexpected exheader field modification")
    require(code[:len(base_code)] == base_code, "original .code prefix changed")
    require(not any(code[DATA_ACTUAL_END - IMAGE_BASE:]),
            "prepared original BSS/reservation is not zero-initialized")
    verify_layout(bytes(header), base_header, payload_start, payload_size)
    return code, bytes(header), set(added)


def prepare_command(args: argparse.Namespace) -> None:
    inputs = (args.base_code.resolve(), args.base_exheader.resolve())
    outputs = (args.output_code.resolve(), args.output_exheader.resolve())
    require(len(set(inputs + outputs)) == 4, "input/output paths must be distinct")
    for output in outputs:
        if output.exists():
            require(not any(output.samefile(source) for source in inputs),
                    "output aliases an original input")
    code, header, added = prepare_images(args.base_code.read_bytes(),
                                        args.base_exheader.read_bytes(),
                                        args.payload_start, args.payload_size)
    for output in outputs:
        output.parent.mkdir(parents=True, exist_ok=True)
    args.output_code.write_bytes(code)
    args.output_exheader.write_bytes(header)
    print(f"Prepared fixed-address code: 0x{len(code):X} bytes; "
          f"payload 0x{args.payload_start:08X}-0x{args.payload_start + args.payload_size:08X}")
    print(f".data pages: 0x{(args.payload_start + args.payload_size - DATA_INFO[0]) // PAGE_SIZE:X}; "
          "original BSS materialized, exheader BSS=0")
    print("Added SVC capabilities: " + (", ".join(f"0x{x:02X}" for x in sorted(added)) or "none"))


def parse_integer(value: str) -> int:
    try:
        return int(value, 0)
    except ValueError as exc:
        raise argparse.ArgumentTypeError(f"not an integer: {value}") from exc


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest="command", required=True)
    prepare = commands.add_parser("prepare", help="reserve extra .data pages and patch exheader")
    prepare.add_argument("--base-code", type=Path, required=True)
    prepare.add_argument("--base-exheader", type=Path, required=True)
    prepare.add_argument("--output-code", type=Path, required=True)
    prepare.add_argument("--output-exheader", type=Path, required=True)
    prepare.add_argument("--payload-start", type=parse_integer, default=DEFAULT_PAYLOAD_START)
    prepare.add_argument("--payload-size", type=parse_integer, default=DEFAULT_PAYLOAD_SIZE)
    args = parser.parse_args()
    try:
        subprocess.run([sys.executable, "-B", str(Path(__file__).absolute().parents[2] /
                        "common/tools/check_file_links.py"), "--recursive", "--allow-missing",
                        *(str(path) for path in vars(args).values() if isinstance(path, Path))], check=True)
        prepare_command(args)
        return 0
    except (OSError, ValueError, subprocess.CalledProcessError) as exc:
        print(f"ERROR: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
