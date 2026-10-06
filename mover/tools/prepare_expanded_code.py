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
Only the real ARM11 capabilities at 0x370 are changed; the signed access-descriptor
copy (in a full 0x800-byte header) is preserved.  This is a CFW override, not a
newly signed retail NCCH.
"""

from __future__ import annotations

import argparse
import hashlib
from pathlib import Path
import struct
import sys
import unittest


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
DEFAULT_PAYLOAD_SIZE = 0x2000
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

    mutable_offsets = set(range(0x34, 0x40)) | set(
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


class PreparationTests(unittest.TestCase):
    def setUp(self) -> None:
        self.code = bytes(sum(info[1] for info in (TEXT_INFO, RO_INFO, DATA_INFO)) * PAGE_SIZE)
        self.header = bytearray(0x800)
        for offset, value in ((0x10, TEXT_INFO), (0x20, RO_INFO), (0x30, DATA_INFO)):
            struct.pack_into("<III", self.header, offset, *value)
        struct.pack_into("<I", self.header, 0x3C, BSS_SIZE)
        struct.pack_into("<Q", self.header, 0x200, TITLE_ID)
        self.header[0xD] = 3
        struct.pack_into("<28I", self.header, KERNEL_CAPS_OFFSET, *([0xFFFFFFFF] * 28))

    def prepare(self, header: bytes | bytearray | None = None,
                start: int = DEFAULT_PAYLOAD_START, size: int = DEFAULT_PAYLOAD_SIZE):
        return prepare_images(self.code, bytes(self.header if header is None else header),
                              start, size, check_hash=False)

    def test_layout_and_exact_header_fields(self) -> None:
        code, header, added = self.prepare()
        self.assertEqual(len(code), 0x267000)
        self.assertEqual(code[:len(self.code)], self.code)
        self.assertEqual(struct.unpack_from("<III", header, 0x34), (0x7B, 0x7B000, 0))
        self.assertEqual(header[:0x34], self.header[:0x34])
        self.assertEqual(header[0x400:], self.header[0x400:])
        self.assertEqual(added, REQUIRED_SVCS)
        self.assertEqual(allowed_svcs(header), REQUIRED_SVCS)

    def test_existing_caps_only_add_missing_bits(self) -> None:
        struct.pack_into("<III", self.header, KERNEL_CAPS_OFFSET,
                         0xF0FA9F4E, 0xF1FFBFFF, 0xF2003FE7)
        _, header, added = self.prepare()
        self.assertEqual(added, {0x70})
        self.assertEqual(header[KERNEL_CAPS_OFFSET:KERNEL_CAPS_OFFSET + 12],
                         self.header[KERNEL_CAPS_OFFSET:KERNEL_CAPS_OFFSET + 12])
        self.assertEqual(struct.unpack_from("<I", header, KERNEL_CAPS_OFFSET + 12)[0], 0xF4010000)

    def test_missing_bit_in_existing_group(self) -> None:
        struct.pack_into("<I", self.header, KERNEL_CAPS_OFFSET, 0xF1000001)
        _, header, _ = self.prepare()
        self.assertEqual(struct.unpack_from("<I", header, KERNEL_CAPS_OFFSET)[0], 0xF1048801)

    def test_header_info_only(self) -> None:
        _, header, _ = self.prepare(self.header[:0x400])
        self.assertEqual(len(header), 0x400)

    def test_verify_expanded_header(self) -> None:
        _, header, _ = self.prepare()
        verify_layout(header, bytes(self.header))
        for offset in (0x18, 0x30, 0x34, 0x38, 0x3C, 0x400):
            dirty = bytearray(header)
            dirty[offset] ^= 1
            with self.subTest(offset=offset), self.assertRaises(ValueError):
                verify_layout(bytes(dirty), bytes(self.header))

    def test_reject_invalid_inputs(self) -> None:
        for start, size in ((0x363000, 0x2000), (0x365001, 0x2000),
                            (0x365000, 0), (0x365000, 1), (0x14000000, 0x2000)):
            with self.subTest(start=start, size=size), self.assertRaises(ValueError):
                self.prepare(start=start, size=size)
        with self.assertRaises(ValueError):
            prepare_images(self.code, bytes(self.header), DEFAULT_PAYLOAD_START, DEFAULT_PAYLOAD_SIZE)
        dirty = bytearray(self.code)
        dirty[DATA_ACTUAL_END - IMAGE_BASE] = 1
        with self.assertRaises(ValueError):
            prepare_images(bytes(dirty), bytes(self.header), DEFAULT_PAYLOAD_START,
                           DEFAULT_PAYLOAD_SIZE, check_hash=False)

    def test_reject_cap_exhaustion_and_duplicates(self) -> None:
        full = bytearray(self.header)
        struct.pack_into("<28I", full, KERNEL_CAPS_OFFSET, *([0xFC000227] * 28))
        with self.assertRaises(ValueError):
            self.prepare(full)
        struct.pack_into("<II", self.header, KERNEL_CAPS_OFFSET, 0xF1000001, 0xF1000002)
        with self.assertRaises(ValueError):
            self.prepare()


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
    commands.add_parser("self-test", help="run synthetic format/invariant tests without ROM files")
    args = parser.parse_args()
    try:
        if args.command == "self-test":
            suite = unittest.defaultTestLoader.loadTestsFromTestCase(PreparationTests)
            return 0 if unittest.TextTestRunner(verbosity=2).run(suite).wasSuccessful() else 1
        prepare_command(args)
        return 0
    except (OSError, ValueError) as exc:
        print(f"ERROR: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
