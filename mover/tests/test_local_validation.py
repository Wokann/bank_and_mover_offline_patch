#!/usr/bin/env python3
"""Run the production C classifier against synthetic, read-only records.

使用合成只读记录回归测试生产 C 分类器；可选存档仅在内存中读取，不会写回。
"""

from __future__ import annotations

import argparse
import ctypes
import struct
import subprocess
import sys
import tempfile
from contextlib import ExitStack
from pathlib import Path


IMAGE_BASE = 0x00100000
BLOCK_ORDER_ADDRESS = 0x002B42E4
EMPTY_TEMPLATE_ADDRESS = 0x002B3FD4
RECORD_SIZE = 136
SLOT_COUNT = 30


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--base", type=Path, required=True)
    parser.add_argument("--cc", default="gcc")
    parser.add_argument("--save", type=Path)
    parser.add_argument("--save-stride", type=lambda value: int(value, 0),
                        choices=(0x24000, 0x26000), default=0x24000)
    args = parser.parse_args()
    base = args.base.read_bytes()
    order_offset = BLOCK_ORDER_ADDRESS - IMAGE_BASE
    order = base[order_offset:order_offset + 128]
    if len(order) != 128 or any(
        sorted(order[index:index + 4]) != [0, 32, 64, 96]
        for index in range(0, 128, 4)
    ):
        raise ValueError("unsupported native block-order table")
    template_offset = EMPTY_TEMPLATE_ADDRESS - IMAGE_BASE
    template = base[template_offset:template_offset + RECORD_SIZE]
    test_root = Path(__file__).resolve().parent
    checks = 0

    def encode(body: bytes, pid: int = 0, control: int = 0) -> bytes:
        physical = bytearray(128)
        row = order[((pid >> 13) & 31) * 4:((pid >> 13) & 31) * 4 + 4]
        for block, offset in enumerate(row):
            physical[offset:offset + 32] = body[block * 32:block * 32 + 32]
        words = list(struct.unpack("<64H", physical))
        checksum = sum(words) & 0xFFFF
        seed = checksum
        if not control & 2:
            for index, value in enumerate(words):
                seed = (seed * 0x41C64E6D + 0x6073) & 0xFFFFFFFF
                words[index] = value ^ (seed >> 16)
        return struct.pack("<IHH64H", pid, control, checksum, *words)

    with tempfile.TemporaryDirectory(prefix="mover-validation-") as temporary, \
            ExitStack() as cleanup:
        library_path = Path(temporary) / (
            "local_validation_test.dll" if sys.platform == "win32" else
            "local_validation_test.so"
        )
        subprocess.run([
            args.cc, "-std=c11", "-O2", "-Wall", "-Wextra", "-Werror",
            "-shared", "-fPIC", "-I", str(test_root.parent / "include"),
            str(test_root / "local_validation_test.c"), "-o", str(library_path),
        ], check=True)
        library = ctypes.CDLL(str(library_path))
        import _ctypes
        cleanup.callback(_ctypes.FreeLibrary if sys.platform == "win32" else
                         _ctypes.dlclose, library._handle)
        library.testSetBlockOrder.argtypes = [ctypes.c_void_p]
        library.testSetBlockOrder.restype = None
        library.testRecord.argtypes = [ctypes.c_void_p]
        library.testRecord.restype = ctypes.c_int
        library.testBox.argtypes = [ctypes.c_void_p, ctypes.c_int,
                                   ctypes.POINTER(ctypes.c_int32),
                                   ctypes.POINTER(ctypes.c_uint32)]
        library.testBox.restype = ctypes.c_int
        library.testVc.argtypes = [ctypes.POINTER(ctypes.c_int32)]
        library.testVc.restype = None
        table = ctypes.create_string_buffer(order)
        library.testSetBlockOrder(table)
        if library.testRecordSize() != RECORD_SIZE:
            raise AssertionError("production record size differs from native size")

        def check_record(record: bytes | bytearray, expected: int) -> None:
            nonlocal checks
            buffer = ctypes.create_string_buffer(bytes(record))
            actual = library.testRecord(buffer)
            if actual != expected:
                raise AssertionError(f"record classification: {actual} != {expected}")
            if buffer.raw[:RECORD_SIZE] != record:
                raise AssertionError("classifier changed source record")
            checks += 1

        check_record(template, 20)
        for shuffle in range(32):
            for pid_seed in (0, 0xDAAD1234):
                pid = (pid_seed & ~0x3E000) | (shuffle << 13)
                for decoded in (0, 2):
                    for variant in range(3):
                        body = bytearray(128)
                        if variant == 1:
                            body[0x38] = 4
                        elif variant == 2:
                            body[0x38] = 4
                            body[0x5E] = 0xA5
                            body[0x7E] = 0x5A
                        empty = encode(body, pid, decoded)
                        check_record(empty, 20)
                        damaged = bytearray(empty)
                        damaged[8] ^= 1
                        check_record(damaged, 1)
                        check_record(encode(body, pid, decoded | 4), 1)
                        for species in (1, 198, 649, 650, 809, 65535):
                            struct.pack_into("<H", body, 0, species)
                            check_record(encode(body, pid, decoded),
                                         0 if species <= 649 else 1)

        body = bytearray(128)
        struct.pack_into("<H", body, 0, 198)
        struct.pack_into("<I", body, 0x30, 0x40000000)
        check_record(encode(body), 0)
        struct.pack_into("<H", body, 2, 1)
        check_record(encode(body), 0)
        body = bytearray(b"\xFF" * 128)
        body[:2] = b"\0\0"
        check_record(encode(body), 20)
        check_record(bytes(RECORD_SIZE), 1)
        check_record(b"\xFF" * RECORD_SIZE, 1)

        def check_box(records: bytes, expected: list[int], missing: int = 0) -> None:
            nonlocal checks
            buffer = ctypes.create_string_buffer(records)
            results = (ctypes.c_int32 * SLOT_COUNT)()
            substate = ctypes.c_uint32()
            status = library.testBox(buffer, missing, results, ctypes.byref(substate))
            if status != (0 if missing else 1) or list(results) != expected:
                raise AssertionError("whole-box preparation or stale-result clearing failed")
            if substate.value != (13 if missing else 2):
                raise AssertionError("source failure did not select native read-error substate")
            if buffer.raw[:len(records)] != records:
                raise AssertionError("whole-box preparation changed source records")
            checks += 1

        records = template * SLOT_COUNT
        check_box(records, [20] * SLOT_COUNT)
        occupied = bytearray(128)
        struct.pack_into("<H", occupied, 0, 198)
        valid = encode(occupied)
        residual = bytearray(128)
        residual[0x38] = 4
        damaged = bytearray(valid)
        damaged[8] ^= 1
        unsupported = bytearray(occupied)
        struct.pack_into("<H", unsupported, 0, 650)
        fixtures = (template, valid, encode(residual), bytes(damaged),
                    encode(occupied, control=4), encode(unsupported))
        expected = (20, 0, 20, 1, 1, 1)
        check_box(b"".join(fixtures[index % len(fixtures)]
                           for index in range(SLOT_COUNT)),
                  [expected[index % len(expected)] for index in range(SLOT_COUNT)])
        for missing in (1, 2, 3):
            check_box(valid * SLOT_COUNT, [1] * SLOT_COUNT, missing)
        for missing in (4, 5):
            results = (ctypes.c_int32 * SLOT_COUNT)()
            substate = ctypes.c_uint32()
            if library.testBox(None, missing, results, ctypes.byref(substate)) != 0:
                raise AssertionError("missing state/shared data was reported successful")
            if substate.value != (13 if missing == 4 else 2):
                raise AssertionError("missing-state error handling differs")
            checks += 1
        vc_results = (ctypes.c_int32 * SLOT_COUNT)()
        library.testVc(vc_results)
        if list(vc_results) != [0] * SLOT_COUNT:
            raise AssertionError("VC results changed or inherited stale values")
        checks += 1

        if args.save:
            save = args.save.read_bytes()
            if len(save) < 2 * args.save_stride:
                raise ValueError("source save does not contain both redundant sides")
            for face in range(2):
                start = face * args.save_stride + 0x400
                records = save[start:start + SLOT_COUNT * RECORD_SIZE]
                results = (ctypes.c_int32 * SLOT_COUNT)()
                substate = ctypes.c_uint32()
                buffer = ctypes.create_string_buffer(records)
                if library.testBox(buffer, 0, results, ctypes.byref(substate)) != 1:
                    raise AssertionError("sample source preparation failed")
                if buffer.raw[:len(records)] != records:
                    raise AssertionError("sample source was modified")
                values = list(results)
                print(f"sample face {face}: continue={values.count(0)}, "
                      f"empty={values.count(20)}, reject={values.count(1)}")
                checks += 1

    print(f"production C classifier regression passed: {checks} cases")


if __name__ == "__main__":
    main()
