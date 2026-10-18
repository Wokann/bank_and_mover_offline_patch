# -*- coding: utf-8 -*-
"""
Pokemon Bank (00040000000C9B00) BankFileData dump parser.

Decrypts EK6 (Gen 6 box Pokemon, 232 bytes) slots inside a BankFileData core dump,
using the algorithm from FlagBrew/PKSM-Core (PK6.cpp + crypto.hpp):
  - encryptionConstant = u32 LE at slot+0
  - crypt: LCRNG seed stream XOR over slot+8 .. slot+232
  - blockShuffle: 4 blocks of 56 bytes rearranged by sv = (enc >> 13) & 31

Usage:
  python parse_bank_file.py <bank_file.bin>
"""

import argparse
import struct
from file_safety import require_plain_file


# ---- constants from PKSM-Core ----
BOX_LENGTH = 232
BLOCK_LENGTH = 56
ENCRYPTION_START = 8
SLOTS_PER_BOX = 30
BANK_BOX_COUNT = 100
CORE_SIZE = 0xBB518
OBJECT_HEADER_SIZE = 8
SLOT_BASE = OBJECT_HEADER_SIZE + 0x17C
# BankBoxRecord: 30*232 + boxLabel[17]*2 (Utf16CodeUnit=u16) + backgroundId(1) + group(1) + order(2)
TRAY_STRIDE = SLOTS_PER_BOX * BOX_LENGTH + 17 * 2 + 4  # 0x1B56

BLOCK_POSITIONS = [
    0, 1, 2, 3, 0, 1, 3, 2, 0, 2, 1, 3, 0, 3, 1, 2, 0, 2, 3, 1, 0, 3, 2, 1,
    1, 0, 2, 3, 1, 0, 3, 2, 2, 0, 1, 3, 3, 0, 1, 2, 2, 0, 3, 1, 3, 0, 2, 1,
    1, 2, 0, 3, 1, 3, 0, 2, 2, 1, 0, 3, 3, 1, 0, 2, 2, 3, 0, 1, 3, 2, 0, 1,
    1, 2, 3, 0, 1, 3, 2, 0, 2, 1, 3, 0, 3, 1, 2, 0, 2, 3, 1, 0, 3, 2, 1, 0,
    0, 1, 2, 3, 0, 1, 3, 2, 0, 2, 1, 3, 0, 3, 1, 2, 0, 2, 3, 1, 0, 3, 2, 1,
    1, 0, 2, 3, 1, 0, 3, 2,
]


def seed_step(seed):
    return (seed * 0x41C64E6D + 0x6073) & 0xFFFFFFFF


def pkm_crypt(data, key):
    """XOR data[8:] with LCRNG stream (PK6::decrypt step 1)."""
    out = bytearray(data)
    seed = key
    for i in range(ENCRYPTION_START, BOX_LENGTH, 2):
        seed = seed_step(seed)
        out[i] ^= (seed >> 16) & 0xFF
        out[i + 1] ^= (seed >> 24) & 0xFF
    return bytes(out)


def block_shuffle(data, sv):
    """Rearrange 4x56-byte blocks (PK6::decrypt step 2)."""
    out = bytearray(data)
    temp = bytearray(data[ENCRYPTION_START:ENCRYPTION_START + BLOCK_LENGTH * 4])
    index = sv * 4
    for block in range(4):
        ofs = BLOCK_POSITIONS[index + block]
        src = temp[ofs * BLOCK_LENGTH:(ofs + 1) * BLOCK_LENGTH]
        out[ENCRYPTION_START + block * BLOCK_LENGTH:ENCRYPTION_START + (block + 1) * BLOCK_LENGTH] = src
    return bytes(out)


def decrypt_ek6(slot):
    """Return decrypted 232-byte EK6, or None if slot is empty/unencrypted."""
    if len(slot) != BOX_LENGTH or slot[:4] == b"\xff\xff\xff\xff":
        return None
    enc = struct.unpack_from("<I", slot, 0)[0]
    sv = (enc >> 13) & 31
    dec = pkm_crypt(slot, enc)
    dec = block_shuffle(dec, sv)
    return dec


def fmt_u16(dec, off):
    return struct.unpack_from("<H", dec, off)[0]


def load_bank_file(path):
    """Normalize the server body or eight-byte-prefixed memory object."""
    with require_plain_file(path).open("rb") as stream:
        data = stream.read(CORE_SIZE + OBJECT_HEADER_SIZE + 1)
    if len(data) == CORE_SIZE:
        data = bytes(OBJECT_HEADER_SIZE) + data
    elif len(data) != CORE_SIZE + OBJECT_HEADER_SIZE:
        raise ValueError("expected a 0xBB518-byte Bank body or 0xBB520-byte memory object")
    version, boxes = struct.unpack_from("<HH", data, OBJECT_HEADER_SIZE + 0x15C)
    if (version, boxes) != (2, BANK_BOX_COUNT):
        raise ValueError("unsupported Bank format or box count")
    return data


def main():
    parser = argparse.ArgumentParser(description="Validate and summarize a bankdata file")
    parser.add_argument("input", help="bankdata file selected by the user")
    args = parser.parse_args()
    try:
        data = load_bank_file(args.input)
    except (OSError, ValueError) as error:
        parser.error(str(error))

    # BankFileData object layout: vptr at +0, core at +8
    core_off = 8
    version, tray_max = struct.unpack_from("<HH", data, core_off + 0x15C)
    point, ticket_count = struct.unpack_from("<II", data, core_off + 0x170)
    print(f"core version={version} bankBoxCount={tray_max} point={point} ticketCount={ticket_count}")

    slot_base = core_off + 0x17C
    n_slots = BANK_BOX_COUNT * SLOTS_PER_BOX
    empty = 0
    found = []
    for k in range(n_slots):
        tray, pos = divmod(k, SLOTS_PER_BOX)
        off = slot_base + tray * TRAY_STRIDE + pos * BOX_LENGTH
        if off + BOX_LENGTH > len(data):
            break
        slot = data[off:off + BOX_LENGTH]
        if all(b == 0xFF for b in slot):
            empty += 1
            continue
        dec = decrypt_ek6(slot)
        if dec is None:
            continue
        species = fmt_u16(dec, 0x08)
        chk = fmt_u16(dec, 0x06)
        # verify checksum (sum of u16 from 0x08..232)
        calc = sum(struct.unpack_from("<H", dec, i)[0] for i in range(0x08, BOX_LENGTH, 2)) & 0xFFFF
        mark = "CHK_OK" if chk == calc else f"CHK_MISMATCH({calc})"
        found.append((k, species, chk, mark))

    print(f"slots checked={k+1} empty={empty} nonempty={len(found)}")
    for k, sp, chk, mark in found[:60]:
        tray, pos = divmod(k, SLOTS_PER_BOX)
        print(f"tray {tray+1:3d} box {pos+1:2d}: species={sp:4d} checksum={chk:04x} {mark}")
    if len(found) > 60:
        print(f"... and {len(found)-60} more")


if __name__ == "__main__":
    main()
