# -*- coding: utf-8 -*-
"""
Brute-force scan of a BankFileData dump for slots that decrypt to a given species
with a valid EK6 checksum. Used to locate tray boundaries when the layout is
uncertain.

Usage:
  python find_species.py <species_id> --input <dump.bin>
"""

import argparse
import struct

from parse_bank_file import (
    BOX_LENGTH,
    decrypt_ek6,
    load_bank_file,
    SLOT_BASE,
)

def checksum_ok(dec):
    chk = struct.unpack_from("<H", dec, 0x06)[0]
    calc = sum(struct.unpack_from("<H", dec, i)[0] for i in range(0x08, BOX_LENGTH, 2)) & 0xFFFF
    return chk == calc


def find_offsets(data, target):
    # The native tray stride is 0x1B56, so alternate trays are only halfword
    # aligned. Include the final possible record rather than dropping it.
    for offset in range(SLOT_BASE, len(data) - BOX_LENGTH + 1, 2):
        decoded = decrypt_ek6(data[offset:offset + BOX_LENGTH])
        if decoded and struct.unpack_from("<H", decoded, 0x08)[0] == target and checksum_ok(decoded):
            yield offset


def main():
    parser = argparse.ArgumentParser(description="Scan BankFileData dump for a species")
    parser.add_argument("species", type=lambda x: int(x, 0), help="species id (decimal or 0x hex)")
    parser.add_argument("--input", required=True, help="bankdata file selected by the user")
    args = parser.parse_args()
    path = args.input
    target = args.species
    try:
        data = load_bank_file(path)
    except (OSError, ValueError) as error:
        parser.error(str(error))
    if not 1 <= target <= 807:
        parser.error("species must be between 1 and 807")

    core_off = 8
    slot_base = core_off + 0x17C  # tray0 slot0 (file offset 0x184)

    print(f"target species={target} (0x{target:04x})")
    hits = []
    for off in find_offsets(data, target):
        hits.append(off)
        print(f"  HIT off=0x{off:06x} (tray0-relative 0x{off-slot_base:06x}, +0x{off-0x184:06x})")

    print(f"total hits: {len(hits)}")
    if hits:
        # derive tray stride candidates from hit spacing
        if len(hits) >= 2:
            print("spacings:", [hex(hits[i+1] - hits[i]) for i in range(len(hits) - 1)])


if __name__ == "__main__":
    main()
