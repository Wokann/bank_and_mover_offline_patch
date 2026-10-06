#!/usr/bin/env python3
"""Statically verify the combined Poke Mover patch.

静态校验 Poke Mover 合并补丁。
"""

from __future__ import annotations

import argparse
import hashlib
import importlib.util
import sys
import zlib
from pathlib import Path


IMAGE_BASE = 0x00100000
EXPECTED_BASE_SHA256 = "001C20ADA74016507C969BB44A0A50F8EDF803EC06A3FC46834263BA8DF0FD2F"
INITIALIZER_START = 0x00261C74
INITIALIZER_END = 0x00262C10
TEXT_ACTUAL_END = 0x0028D1AC
FOLLOWING_STOCK_CODE_START = INITIALIZER_END
FOLLOWING_STOCK_CODE_END = 0x002638D8
TEXT_MAPPED_END = 0x0028E000
LAYEREDFS_RESERVED_SIZE = 0x130
VERSION_STORAGE_SIZE = 0x40
VERSION_STORAGE_START = TEXT_MAPPED_END - VERSION_STORAGE_SIZE
VERSION_IDENTIFIER = b"offline_patch_v1.0.0\0"
PAYLOAD_START = 0x0028D2E0
EXPANDED_PAYLOAD_START = 0x00365000
EXPANDED_PAYLOAD_SIZE = 0x2000
TICKET_INTERFACES = (
    (0x00249754, "initialize", "ticketjob_initialize", 0x0023D3C0),
    (0x00249774, "poll", "ticketjob_poll", 0x0023E230),
    (0x00249818, "campaignrequest", "movercampaign_request", 0x0023BE4C),
    (0x00249CEC, "unbind", "ticketjob_unbind", 0x0023E63C),
)
DATA_ACTUAL_END = 0x003293FC
BSS_SIZE = 0x0003A4A8
BSS_ACTUAL_END = DATA_ACTUAL_END + BSS_SIZE
READ_WRITE_MAPPED_END = (BSS_ACTUAL_END + 0xFFF) & ~0xFFF
RUNTIME_STORAGE_START = READ_WRITE_MAPPED_END - 0x10
NDS_SCANNER_CONTEXT_SLOT = RUNTIME_STORAGE_START
MODE_STORAGE = RUNTIME_STORAGE_START + 4


def symbols(path: Path) -> dict[str, int]:
    result: dict[str, int] = {}
    for line in path.read_text(encoding="ascii").splitlines():
        fields = line.split()
        if len(fields) == 2:
            result[fields[1].lower()] = int(fields[0], 16)
    return result


def word(image: bytes, address: int) -> int:
    offset = address - IMAGE_BASE
    return int.from_bytes(image[offset:offset + 4], "little")


def verify_constructor_table(base: bytes, patched: bytes) -> None:
    # Native startup calls each PREL32 entry as entry address + stored offset.
    # Direct branches and absolute pointers alone do not cover these calls.
    # 原版启动以“表项地址 + 相对偏移”调用 PREL32 构造表，不能只检查直接分支和绝对指针。
    table_start = 0x00102AE4 + word(base, 0x00102AF8)
    table_end = 0x00102AE8 + word(base, 0x00102AFC)
    if not (
        IMAGE_BASE <= table_start < table_end <= IMAGE_BASE + len(base)
        and (table_end - table_start) % 4 == 0
    ):
        raise ValueError("invalid native startup constructor table")
    if ((0x002EB7BC + word(base, 0x002EB7BC)) & 0xFFFFFFFF) != INITIALIZER_START:
        raise ValueError("native constructor entry no longer targets its initializer")
    if patched[table_start - IMAGE_BASE:table_end - IMAGE_BASE] != \
            base[table_start - IMAGE_BASE:table_end - IMAGE_BASE]:
        raise ValueError("native constructor table changed")
    if patched[INITIALIZER_START - IMAGE_BASE:INITIALIZER_END - IMAGE_BASE] != \
            base[INITIALIZER_START - IMAGE_BASE:INITIALIZER_END - IMAGE_BASE]:
        raise ValueError("native startup initializer changed")


def branch_target(image: bytes, address: int) -> tuple[int, bool, int]:
    instruction = word(image, address)
    if ((instruction >> 25) & 7) != 5:
        raise ValueError(f"expected branch at {address:08X}, found {instruction:08X}")
    displacement = (instruction & 0xFFFFFF) << 2
    if displacement & 0x02000000:
        displacement -= 0x04000000
    return address + 8 + displacement, bool(instruction & 0x01000000), instruction >> 28


def arm_literal_value(
    image: bytes, address: int, register: int, condition: int = 0xE
) -> int:
    instruction = word(image, address)
    expected = (condition << 28) | 0x059F0000 | (register << 12)
    if instruction & 0xFFFFF000 != expected:
        raise ValueError(f"expected ARM literal load at {address:08X}")
    return word(image, address + 8 + (instruction & 0xFFF))


def verify_no_immediate_thumb_blx(image: bytes, start: int, end: int) -> None:
    """Reject importer-generated Thumb-to-ARM immediate calls.

    armips resolves R_ARM_THM_CALL as an immediate Thumb BLX without applying
    the interworking relocation correctly. All calls from these Thumb objects
    into ARM code must therefore use a literal address followed by BLX Rm.
    """
    for address in range((start + 1) & ~1, end - 2, 2):
        offset = address - IMAGE_BASE
        first = int.from_bytes(image[offset:offset + 2], "little")
        second = int.from_bytes(image[offset + 2:offset + 4], "little")
        if first & 0xF800 == 0xF000 and second & 0xF800 == 0xE800:
            raise ValueError(
                f"unsafe immediate Thumb-to-ARM call at {address:08X}"
            )

def apply_bps(base: bytes, patch: bytes) -> bytes:
    """Decode the release BPS, checking sizes, copy bounds and all three CRCs."""
    if len(patch) < 19 or not patch.startswith(b"BPS1"):
        raise ValueError("invalid BPS stream")
    cursor, footer = 4, len(patch) - 12

    def number() -> int:
        nonlocal cursor
        value, shift = 0, 1
        while cursor < footer:
            byte = patch[cursor]
            cursor += 1
            value += (byte & 0x7F) * shift
            if byte & 0x80:
                return value
            shift <<= 7
            value += shift
            if shift > 1 << 35:
                break
        raise ValueError("invalid BPS number")

    source_size, target_size, metadata_size = number(), number(), number()
    if source_size != len(base) or metadata_size != 0:
        raise ValueError("BPS source length or metadata is incompatible")
    if target_size != EXPANDED_PAYLOAD_START + EXPANDED_PAYLOAD_SIZE - IMAGE_BASE:
        raise ValueError("BPS target length differs from the mapped image")
    source_crc, target_crc, patch_crc = (
        int.from_bytes(patch[index:index + 4], "little")
        for index in range(footer, len(patch), 4)
    )
    if zlib.crc32(base) != source_crc or zlib.crc32(patch[:-4]) != patch_crc:
        raise ValueError("BPS source or patch CRC mismatch")

    output = bytearray()
    source_relative = target_relative = 0
    while cursor < footer:
        command = number()
        operation, length = command & 3, (command >> 2) + 1
        if length > target_size - len(output):
            raise ValueError("BPS command exceeds target size")
        if operation == 0:
            offset = len(output)
            if offset + length > source_size:
                raise ValueError("BPS source read exceeds source size")
            output.extend(base[offset:offset + length])
        elif operation == 1:
            if cursor + length > footer:
                raise ValueError("truncated BPS literal")
            output.extend(patch[cursor:cursor + length])
            cursor += length
        else:
            encoded = number()
            distance = -(encoded >> 1) if encoded & 1 else encoded >> 1
            if operation == 2:
                source_relative += distance
                if source_relative < 0 or source_relative + length > source_size:
                    raise ValueError("BPS source copy exceeds source size")
                output.extend(base[source_relative:source_relative + length])
                source_relative += length
            else:
                target_relative += distance
                if target_relative < 0 or target_relative >= len(output):
                    raise ValueError("BPS target copy does not reference existing output")
                for _ in range(length):
                    output.append(output[target_relative])
                    target_relative += 1
    if cursor != footer or len(output) != target_size or zlib.crc32(output) != target_crc:
        raise ValueError("BPS target size or CRC mismatch")
    return bytes(output)


def load_helper(path: Path):
    spec = importlib.util.spec_from_file_location("combined_messages", path)
    if spec is None or spec.loader is None:
        raise RuntimeError(path)
    module = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = module
    spec.loader.exec_module(module)
    return module


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--base", required=True, type=Path)
    parser.add_argument("--patched", required=True, type=Path)
    parser.add_argument("--symbols", required=True, type=Path)
    parser.add_argument("--bps", required=True, type=Path)
    parser.add_argument("--source-romfs", required=True, type=Path)
    parser.add_argument("--output-romfs", required=True, type=Path)
    parser.add_argument("--exheader", required=True, type=Path)
    parser.add_argument("--base-exheader", required=True, type=Path)
    args = parser.parse_args()

    base = args.base.read_bytes()
    patched = args.patched.read_bytes()
    digest = hashlib.sha256(base).hexdigest().upper()
    if digest != EXPECTED_BASE_SHA256:
        raise ValueError(f"unsupported base code SHA-256: {digest}")
    sym = symbols(args.symbols)
    verify_constructor_table(base, patched)
    from tools.prepare_expanded_code import prepare_images
    _, expected_header, _ = prepare_images(
        base, args.base_exheader.read_bytes(), EXPANDED_PAYLOAD_START, EXPANDED_PAYLOAD_SIZE)
    if args.exheader.read_bytes() != expected_header:
        raise ValueError("released exheader differs from the required expanded layout/capabilities")
    if len(patched) != EXPANDED_PAYLOAD_START + EXPANDED_PAYLOAD_SIZE - IMAGE_BASE:
        raise ValueError("expanded code does not end at its mapped data-page boundary")
    if patched[len(base):EXPANDED_PAYLOAD_START - IMAGE_BASE] != bytes(
            EXPANDED_PAYLOAD_START - IMAGE_BASE - len(base)):
        raise ValueError("expanded BSS and pre-payload padding are not zero-filled")
    if apply_bps(base, args.bps.read_bytes()) != patched:
        raise ValueError("release BPS does not reproduce the patched image")
    registration_offset = FOLLOWING_STOCK_CODE_START - IMAGE_BASE
    registration_size = FOLLOWING_STOCK_CODE_END - FOLLOWING_STOCK_CODE_START
    if patched[
        registration_offset:registration_offset + registration_size
    ] != base[registration_offset:registration_offset + registration_size]:
        raise ValueError("native startup registration code changed")

    required = {
        "combinepatch_codeusedend", "combinepatch_offlinepayloadusedend",
        "combinepatch_offlinepayloadstart", "textactualend", "textmappedend",
        "fshelpers_payloadbegin", "fshelpers_payloadend",
        "ndssources_payloadbegin", "ndssources_payloadend",
        "ndsfs_payloadbegin", "ndsfs_payloadend",
        "bankdataredirect_payloadbegin", "bankdataredirect_payloadend",
        "localticket_payloadbegin", "localticket_payloadend",
        "localvalidation_payloadbegin", "localvalidation_payloadend",
        "offlineflow_payloadbegin", "offlineflow_payloadend",
        "patchpaths_payloadbegin", "patchpaths_payloadend",
        "combinepatch_titletextinitialize", "combinepatch_titlestateupdate",
        "combinepatch_titleprocessmodetoggle", "combinepatch_networkupdate",
        "combinepatch_networkavailability",
        "combinepatch_onlineticketbypass", "online_ticket_check_bypass",
        "combinepatch_modestorage", "combinepatch_ticketwrappersend",
        "localticket_initialize", "localticket_poll", "localticket_campaignresult",
        "combinepatch_eligibilityupdate", "combinepatch_getpokemonupdate",
        "combinepatch_saveskipremotejob",
        "offlinepatch_networkupdate", "offlinepatch_stage", "offlinepatch_commit",
        "offlinepatch_rollback", "offlinepatch_preparegen5validation",
        "offlinepatch_preparegen12validation",
        "ndssources_listupdate", "ndssources_selectlistid",
        "ndssources_readsave", "ndssources_readgamecode", "ndssources_writesave",
        "ndssources_originalreadsave", "ndssources_originalreadgamecode",
        "ndssources_originalwritesave", "combinepatch_selectndssource",
        "combinepatch_vcsourceadded", "combinepatch_ndsreadsaveentry",
        "combinepatch_ndsreadgamecodeentry", "combinepatch_ndswritesaveentry",
        "offlinepatch_versionidentifier", "offlinepatch_runtimestoragestart",
        "nds_scanner_context_slot", "offlinepatch_runtimestorageend",
        "combinepatch_modestorage",
        "codeexpansion_startup", "codeexpansion_enable",
        "codeexpansion_loaderbegin", "codeexpansion_loaderend",
    }
    for _, operation, native_name, _ in TICKET_INTERFACES:
        required.update((f"combinepatch_ticket{operation}",
                         f"combinepatch_ticket{operation}local", native_name))
    missing = sorted(required - sym.keys())
    if missing:
        raise ValueError(f"missing armips symbols: {', '.join(missing)}")
    code_start = EXPANDED_PAYLOAD_START
    code_end = EXPANDED_PAYLOAD_START + EXPANDED_PAYLOAD_SIZE
    if not code_start < sym["combinepatch_codeusedend"] <= code_end:
        raise ValueError("payload exceeds the added executable pages")
    if not (
        sym["textactualend"] == TEXT_ACTUAL_END
        and sym["textmappedend"] == TEXT_MAPPED_END
        and PAYLOAD_START % 0x10 == 0
        and PAYLOAD_START - TEXT_ACTUAL_END >= LAYEREDFS_RESERVED_SIZE
    ):
        raise ValueError("invalid LayeredFS reservation or payload alignment")
    for image in (base, patched):
        if image[TEXT_ACTUAL_END - IMAGE_BASE:PAYLOAD_START - IMAGE_BASE] != bytes(
            PAYLOAD_START - TEXT_ACTUAL_END
        ):
            raise ValueError("Luma LayeredFS reservation is not untouched padding")
    tail_start = sym["combinepatch_codeusedend"]
    tail_end = code_end
    if sym["combinepatch_offlinepayloadstart"] != tail_start:
        raise ValueError("incorrect offline payload start for selected layout")
    if not tail_start < sym["combinepatch_offlinepayloadusedend"] <= tail_end:
        raise ValueError("offline payload exceeds the executable tail")
    payload_objects = (
        ("fshelpers_payloadbegin", "fshelpers_payloadend"),
        ("ndssources_payloadbegin", "ndssources_payloadend"),
        ("localticket_payloadbegin", "localticket_payloadend"),
        ("bankdataredirect_payloadbegin", "bankdataredirect_payloadend"),
        ("offlineflow_payloadbegin", "offlineflow_payloadend"),
        ("patchpaths_payloadbegin", "patchpaths_payloadend"),
        ("localvalidation_payloadbegin", "localvalidation_payloadend"),
        ("ndsfs_payloadbegin", "ndsfs_payloadend"),
    )
    for begin, end in payload_objects:
        if sym[begin] >= sym[end]:
            raise ValueError(f"empty or reversed payload object: {begin}")
    head_objects = payload_objects[:2]
    if not code_start < sym[head_objects[0][0]]:
        raise ValueError("first feature object precedes the added pages")
    for (_, previous_end), (next_begin, _) in zip(head_objects, head_objects[1:]):
        if sym[previous_end] != sym[next_begin]:
            raise ValueError("feature payload objects are not consecutive")
    if sym[head_objects[-1][1]] != sym["combinepatch_codeusedend"]:
        raise ValueError("feature payload end does not follow the final object")
    if sym[head_objects[-1][1]] > code_end:
        raise ValueError("feature payload exceeds the added pages")
    tail_objects = payload_objects[2:]
    if sym[tail_objects[0][0]] < tail_start:
        raise ValueError("first tail payload object precedes the executable tail")
    for (_, previous_end), (next_begin, _) in zip(tail_objects, tail_objects[1:]):
        if sym[previous_end] != sym[next_begin]:
            raise ValueError("tail payload objects are not consecutive")
    if sym[tail_objects[-1][1]] != sym["combinepatch_offlinepayloadusedend"]:
        raise ValueError("offline payload end does not follow the final object")
    for module in ("fshelpers", "ndssources", "bankdataredirect",
                   "offlineflow", "localvalidation", "ndsfs"):
        verify_no_immediate_thumb_blx(
            patched, sym[f"{module}_payloadbegin"], sym[f"{module}_payloadend"]
        )
    thumb_tail_routes = (
        ("combinepatch_networkupdate", "offlinepatch_networkupdate"),
        ("combinepatch_remotecheckupdate", "offlinepatch_remotecheckupdate"),
        ("combinepatch_notransferupdate", "offlinepatch_notransferupdate"),
        ("combinepatch_disconnectupdate", "offlinepatch_disconnectupdate"),
        ("combinepatch_stage", "offlinepatch_stage"),
        ("combinepatch_commit", "offlinepatch_commit"),
        ("combinepatch_rollback", "offlinepatch_rollback"),
    )
    for wrapper_name, helper_name in thumb_tail_routes:
        wrapper = sym[wrapper_name]
        if arm_literal_value(patched, wrapper + 12, 12, 0) != sym[helper_name] + 1 or \
                word(patched, wrapper + 16) != 0x012FFF1C:
            raise ValueError(f"incorrect conditional Thumb dispatch: {wrapper_name}")
    save_wait = sym["combinepatch_savewait"]
    if arm_literal_value(patched, save_wait + 20, 12) != \
            sym["offlinepatch_savedisplaydelayupdate"] + 1 or \
            word(patched, save_wait + 24) != 0xE12FFF3C:
        raise ValueError("save-delay call does not interwork to Thumb")
    eligibility_entry = sym["offlinepatch_eligibilityentry"]
    if arm_literal_value(patched, eligibility_entry + 12, 12) != \
            sym["offlinepatch_eligibilityupdate"] + 1 or \
            word(patched, eligibility_entry + 16) != 0xE12FFF1C:
        raise ValueError("eligibility entry does not interwork to Thumb")
    # Keep the marker in the final mapped text bytes. The remaining bytes stay
    # zero for a longer future identifier or metadata.
    # 将标识固定在已映射 text 段的最后部分。其余字节保持为零，供未来更长的标识
    # 或元数据使用。
    if sym["offlinepatch_versionidentifier"] != VERSION_STORAGE_START:
        raise ValueError("offline patch version identifier moved")
    version_offset = VERSION_STORAGE_START - IMAGE_BASE
    expected_version_storage = VERSION_IDENTIFIER.ljust(VERSION_STORAGE_SIZE, b"\0")
    if patched[version_offset:version_offset + VERSION_STORAGE_SIZE] != expected_version_storage:
        raise ValueError("offline patch version storage is malformed")
    if base[version_offset:version_offset + VERSION_STORAGE_SIZE] != bytes(VERSION_STORAGE_SIZE):
        raise ValueError("offline patch version storage no longer uses original padding")
    if VERSION_STORAGE_START + VERSION_STORAGE_SIZE != TEXT_MAPPED_END:
        raise ValueError("offline patch version storage no longer ends with mapped text")

    expected_base_words = {
        0x0024BA84: 0xEBFD4261,
        0x00249F20: 0xE5D0103C,
        0x00248A08: 0xE92D4070,
        0x00248B88: 0xEBFB1ACF,
        0x00248B98: 0xE594100C,
        0x00248BC0: 0xE3A0100D,
        0x00249600: 0xE92D43F8,
        0x00248814: 0xE92D41F0,
        0x00248C68: 0xE92D4070,
        0x002455D0: 0xE92D4FF0,
        0x00246A5C: 0xE3A0100F,
        0x00245728: 0xE59F0C74,
        0x00246368: 0xE28DD0FC,
        0x002460B8: 0xE59F02E4,
        0x00248360: 0xE92D4070,
        0x00248500: 0xE3A0100F,
        0x0024709C: 0xE92D4010,
        0x00247288: 0xE3A0100E,
        0x00247290: 0xE594100C,
        0x00242D4C: 0x03A0000E,
        0x0024A150: 0xE59F04F4,
        0x0024A240: 0xEBFFD1E9,
        0x0024A258: 0xE5D41044,
        0x0024A3E4: 0xEBFD454C,
        0x0024A3EC: 0x13A0000B,
        0x0024A420: 0xEBFD44EA,
        0x0024A428: 0x13A0000D,
        0x0024A6F0: 0xE3A01009,
        0x00244870: 0xE3500005,
        0x0024149C: 0xE1C605B8,
        0x0021A7E0: 0xE92D4008,
        0x0021AA0C: 0xE92D40F0,
        0x0021AB50: 0xE92D4FFF,
        0x002E6620: 0x00244D2C,
    }
    for address, expected in expected_base_words.items():
        actual = word(base, address)
        if actual != expected:
            raise ValueError(
                f"unexpected base instruction at {address:08X}: "
                f"expected {expected:08X}, found {actual:08X}"
            )

    hooks = {
        0x0024BA84: ("combinepatch_titletextinitialize", True, 0xE),
        0x00249F20: ("combinepatch_titlestateupdate", False, 0xE),
        0x00248A08: ("combinepatch_networkupdate", False, 0xE),
        0x00248B88: ("combinepatch_networkavailability", True, 0xE),
        0x00248B98: ("combinepatch_networkskipremotejob", False, 0xE),
        0x00248BC0: ("combinepatch_selectinitialconnectmessage", True, 0xE),
        0x00248814: ("combinepatch_remotecheckupdate", False, 0xE),
        0x00248C68: ("combinepatch_eligibilityupdate", False, 0xE),
        0x002455D0: ("combinepatch_getpokemonupdate", False, 0xE),
        0x00246A5C: ("combinepatch_selectbankconnectmessage", True, 0xE),
        0x00245728: ("combinepatch_gen5validation", False, 0xE),
        0x002460B8: ("combinepatch_gen12validation", False, 0xE),
        0x00248360: ("combinepatch_notransferupdate", False, 0xE),
        0x00248500: ("combinepatch_selectbankconnectmessage", True, 0xE),
        0x0024709C: ("combinepatch_disconnectupdate", False, 0xE),
        0x00247288: ("combinepatch_selectdisconnectmessage", True, 0xE),
        0x00247290: ("combinepatch_disconnectskipremotejob", False, 0xE),
        0x00242D4C: ("combinepatch_remotechecksuccessstate", False, 0x0),
        0x0024A150: ("combinepatch_saveskipremotejob", False, 0xE),
        0x0024A240: ("combinepatch_stage", True, 0xE),
        0x0024A258: ("combinepatch_savewait", False, 0xE),
        0x0024A3E4: ("combinepatch_commit", True, 0xE),
        0x0024A3EC: ("combinepatch_aftercommit", False, 0x1),
        0x0024A420: ("combinepatch_rollback", True, 0xE),
        0x0024A428: ("combinepatch_afterrollback", False, 0x1),
        0x0024A6F0: ("combinepatch_selectsavemessage", True, 0xE),
        0x00244870: ("combinepatch_selectndssource", True, 0xE),
        0x0024149C: ("combinepatch_vcsourceadded", True, 0xE),
        0x0021A7E0: ("combinepatch_ndsreadsaveentry", False, 0xE),
        0x0021AA0C: ("combinepatch_ndsreadgamecodeentry", False, 0xE),
        0x0021AB50: ("combinepatch_ndswritesaveentry", False, 0xE),
    }
    if word(base, 0x00100010) != 0xFA000AB1:
        raise ValueError("unexpected original constructor-walker call")
    hooks[0x00100010] = ("codeexpansion_startup", True, 0xE)
    loader_start = sym["codeexpansion_startup"]
    loader_end = sym["codeexpansion_loaderend"]
    if not PAYLOAD_START <= loader_start < loader_end <= VERSION_STORAGE_START:
        raise ValueError("expansion bootstrap is outside executable padding")
    if word(patched, loader_start) != 0xE92D5FFF or \
            word(patched, loader_start + 24) != 0xE8BD5FFF:
        raise ValueError("expansion bootstrap does not preserve startup registers")
    if arm_literal_value(patched, loader_start + 4, 0) != EXPANDED_PAYLOAD_START or \
            word(patched, loader_start + 8) != 0xE3A01A02:
        raise ValueError("expansion bootstrap protects the wrong pages")
    if branch_target(patched, loader_start + 12) != \
            (sym["codeexpansion_enable"], True, 0xE):
        raise ValueError("expansion bootstrap does not call its native ARM helper")
    if arm_literal_value(patched, loader_start + 28, 15) != 0x00102ADD:
        raise ValueError("expansion bootstrap does not resume native constructors")
    loader = patched[sym["codeexpansion_loaderbegin"] - IMAGE_BASE:loader_end - IMAGE_BASE]
    for service in (0x27, 0x2A, 0x70, 0x23):
        if (0xEF000000 | service).to_bytes(4, "little") not in loader:
            raise ValueError(f"missing expansion SVC {service:02X}")
    for address, operation, native_name, native_address in TICKET_INTERFACES:
        if branch_target(base, address) != (native_address, True, 0xE):
            raise ValueError(f"unexpected native ticket call at {address:08X}")
        if sym[native_name] != native_address:
            raise ValueError(f"incorrect Mover ticket address: {native_name}")
        hooks[address] = (f"combinepatch_ticket{operation}", True, 0xE)
    for address, (name, link, condition) in hooks.items():
        target, actual_link, actual_condition = branch_target(patched, address)
        if (target, actual_link, actual_condition) != (sym[name], link, condition):
            raise ValueError(f"incorrect hook at {address:08X}")

    # Keep the native state, callback and cleanup intact except for the four
    # interface calls; no whole-state ticket bypass remains.
    # 除四个接口调用外，原版状态、回调与清理不变；不再从整状态外部跳过票据流程。
    state_start, state_end = 0x00249600, 0x00249D38
    expected_state = bytearray(base[state_start - IMAGE_BASE:state_end - IMAGE_BASE])
    for address, _, _, _ in TICKET_INTERFACES:
        expected_state[address - state_start:address - state_start + 4] = \
            patched[address - IMAGE_BASE:address - IMAGE_BASE + 4]
    if patched[state_start - IMAGE_BASE:state_end - IMAGE_BASE] != expected_state:
        raise ValueError("native ticket state/callback/cleanup changed outside its interfaces")
    for start, end in ((0x0023D3C0, 0x0023E910), (0x0023BC50, 0x0023BF24),
                       (0x00199720, 0x0019983C)):
        if patched[start - IMAGE_BASE:end - IMAGE_BASE] != base[start - IMAGE_BASE:end - IMAGE_BASE]:
            raise ValueError(f"native ticket implementation changed: {start:08X}-{end:08X}")

    policy = sym["online_ticket_check_bypass"]
    policy_address = sym["combinepatch_onlineticketbypass"]
    wrapper_start = sym["combinepatch_ticketinitialize"]
    wrapper_end = sym["combinepatch_ticketwrappersend"]
    if policy not in (0, 1) or patched[policy_address - IMAGE_BASE] != policy:
        raise ValueError("ONLINE_TICKET_CHECK_BYPASS must be a matching 0/1 byte")
    if not code_start <= policy_address < wrapper_start < wrapper_end <= sym["fshelpers_payloadbegin"]:
        raise ValueError("ticket policy/wrappers exceed the added pages")
    for _, operation, native_name, _ in TICKET_INTERFACES:
        wrapper = sym[f"combinepatch_ticket{operation}"]
        local = sym[f"combinepatch_ticket{operation}local"]
        if local != wrapper + 36:
            raise ValueError(f"incorrect ticket {operation} wrapper layout")
        for offset, pointer in ((0, sym["combinepatch_modestorage"]),
                                (16, policy_address)):
            address = wrapper + offset
            instruction = word(patched, address)
            if instruction & 0xFFFFF000 != 0xE59FC000:
                raise ValueError(f"ticket {operation} does not load its policy pointer")
            literal = address + 8 + (instruction & 0xFFF)
            if not wrapper_start <= literal < wrapper_end or word(patched, literal) != pointer:
                raise ValueError(f"incorrect ticket {operation} policy literal")
            if word(patched, address + 4) != 0xE5DCC000 or word(patched, address + 8) != 0xE35C0000:
                raise ValueError(f"ticket {operation} does not test the session/policy byte")
        for offset, target, condition in ((12, local, 0x0), (28, local, 0x1),
                                           (32, sym[native_name], 0xE)):
            if branch_target(patched, wrapper + offset) != (target, False, condition):
                raise ValueError(f"incorrect ticket {operation} route")
        if operation == "initialize":
            if word(patched, local) != 0xE5941028 or \
                    branch_target(patched, local + 4) != (sym["localticket_initialize"], False, 0xE):
                raise ValueError("local ticket initialization loses the shared-data argument")
        elif operation == "poll":
            if branch_target(patched, local) != (sym["localticket_poll"], False, 0xE):
                raise ValueError("local ticket poll is not a tail call")
        elif operation == "campaignrequest":
            if word(patched, local) != 0xE1A00004 or \
                    branch_target(patched, local + 4) != (sym["localticket_campaignresult"], False, 0xE):
                raise ValueError("local campaign completion loses the state argument")
        elif word(patched, local) != 0xE3A00001 or word(patched, local + 4) != 0xE12FFF1E:
            raise ValueError("local ticket unbind does not return to native destruction")

    validation_routes = (
        (
            "combinepatch_gen5validation",
            "offlinepatch_preparegen5validation",
            0x00245800,
            0x0024572C,
        ),
        (
            "combinepatch_gen12validation",
            "offlinepatch_preparegen12validation",
            0x002461D0,
            0x002460BC,
        ),
    )
    for wrapper_name, helper_name, local_continuation, original_continuation in validation_routes:
        wrapper = sym[wrapper_name]
        expected_branches = (
            (wrapper + 0x0C, wrapper + (0x28 if wrapper_name ==
                "combinepatch_gen5validation" else 0x20), False, 0x1),
            (wrapper + (0x24 if wrapper_name == "combinepatch_gen5validation"
                else 0x1C), local_continuation, False, 0xE),
            (wrapper + (0x2C if wrapper_name == "combinepatch_gen5validation"
                else 0x24), original_continuation, False, 0xE),
        )
        for address, target, link, condition in expected_branches:
            actual_target, actual_link, actual_condition = branch_target(patched, address)
            if (actual_target, actual_link, actual_condition) != (target, link, condition):
                raise ValueError(f"incorrect validation route at {address:08X}")
        if arm_literal_value(patched, wrapper + 0x14, 12) != sym[helper_name] + 1 or \
                word(patched, wrapper + 0x18) != 0xE12FFF3C:
            raise ValueError("validation preparation does not interwork to Thumb")
        if wrapper_name == "combinepatch_gen5validation":
            if word(patched, wrapper + 0x1C) != 0xE3500000:
                raise ValueError("Gen 5 validation does not test preparation failure")
            if branch_target(patched, wrapper + 0x20) != (0x00246368, False, 0x0):
                raise ValueError("Gen 5 source failure does not return through stock cleanup")

    # Keep the native local checks, conversion, and result consumer unchanged.
    # 保持原版本地检查、转换器和逐槽结果消费代码不变。
    for start, end in ((0x00241574, 0x0024212C), (0x00242F8C, 0x00243448),
                       (0x00245800, 0x002460B8), (0x002461C8, 0x00246A2C)):
        if patched[start - IMAGE_BASE:end - IMAGE_BASE] != base[start - IMAGE_BASE:end - IMAGE_BASE]:
            raise ValueError(f"native validation/conversion range changed: {start:08X}-{end:08X}")

    if word(patched, 0x002E6620) != sym["ndssources_listupdate"] + 1:
        raise ValueError("NDS source-list vtable slot does not target the scanner")

    thumb_entries = (
        ("combinepatch_ndsreadsaveentry", "ndssources_readsave"),
        ("combinepatch_ndsreadgamecodeentry", "ndssources_readgamecode"),
        ("combinepatch_ndswritesaveentry", "ndssources_writesave"),
    )
    for entry_name, thumb_name in thumb_entries:
        entry = sym[entry_name]
        if arm_literal_value(patched, entry, 12) != sym[thumb_name] + 1:
            raise ValueError(f"{entry_name} does not switch to its Thumb target")
        if word(patched, entry + 4) != 0xE12FFF1C:
            raise ValueError(f"{entry_name} does not end in bx r12")
    select_wrapper = sym["combinepatch_selectndssource"]
    if arm_literal_value(patched, select_wrapper + 4, 12) != (
        sym["ndssources_selectlistid"] + 1
    ):
        raise ValueError("NDS source-selection wrapper does not call its Thumb target")

    if word(patched, sym["combinepatch_vcsourceadded"]) != 0xE1C605B8:
        raise ValueError("VC source-cap wrapper does not replay the count store")

    # Runtime words live after the logical .bss end but inside its final mapped
    # page. They are therefore patch-owned and zero-filled by process creation;
    # unlike the former .data-tail addresses, they cannot alias stock objects.
    # 运行时字位于逻辑 .bss 末尾之后、最终映射页以内，因此属于补丁且由进程创建时
    # 清零；它们不会再像旧的 .data 尾地址那样与原版对象重叠。
    if BSS_ACTUAL_END != 0x003638A4 or READ_WRITE_MAPPED_END != 0x00364000:
        raise ValueError("unexpected read/write layout calculation")
    if word(base, 0x00100040) != DATA_ACTUAL_END:
        raise ValueError("stock BSS-clear start no longer matches the audited data end")
    if word(base, 0x00100044) != BSS_ACTUAL_END:
        raise ValueError("stock BSS-clear end no longer matches the audited BSS end")
    if not BSS_ACTUAL_END <= RUNTIME_STORAGE_START < READ_WRITE_MAPPED_END:
        raise ValueError("runtime storage is outside mapped .bss padding")
    for offset in range(0, len(base) - 3, 4):
        value = int.from_bytes(base[offset:offset + 4], "little")
        if RUNTIME_STORAGE_START <= value < READ_WRITE_MAPPED_END:
            raise ValueError(
                f"stock aligned pointer enters runtime storage at "
                f"{IMAGE_BASE + offset:08X}"
            )
    expected_runtime_symbols = {
        "offlinepatch_runtimestoragestart": RUNTIME_STORAGE_START,
        "nds_scanner_context_slot": NDS_SCANNER_CONTEXT_SLOT,
        "combinepatch_modestorage": MODE_STORAGE,
        "offlinepatch_runtimestorageend": READ_WRITE_MAPPED_END,
    }
    for name, expected in expected_runtime_symbols.items():
        if sym[name] != expected:
            raise ValueError(f"incorrect runtime storage symbol: {name}")
    if NDS_SCANNER_CONTEXT_SLOT < IMAGE_BASE + len(base):
        raise ValueError("scanner context slot unexpectedly lies in the code image")
    nds_payload = patched[
        sym["ndssources_payloadbegin"] - IMAGE_BASE:
        sym["ndssources_payloadend"] - IMAGE_BASE
    ]
    if NDS_SCANNER_CONTEXT_SLOT.to_bytes(4, "little") not in nds_payload:
        raise ValueError("NDS scanner payload does not reference its runtime slot")
    if (0x00329FF8).to_bytes(4, "little") in nds_payload:
        raise ValueError("NDS scanner payload still references the stock resource table")

    trampoline_routes = (
        ("ndssources_originalreadsave", 0xE92D4008, 0x0021A7E4),
        ("ndssources_originalreadgamecode", 0xE92D40F0, 0x0021AA10),
        ("ndssources_originalwritesave", 0xE92D4FFF, 0x0021AB54),
    )
    for name, prologue, continuation in trampoline_routes:
        address = sym[name]
        if word(patched, address) != prologue:
            raise ValueError(f"incorrect stock prologue replay in {name}")
        target, link, condition = branch_target(patched, address + 4)
        if (target, link, condition) != (continuation, False, 0xE):
            raise ValueError(f"incorrect stock continuation in {name}")

    # Preserve discovery, the asynchronous worker, save validation, and card
    # I/O bodies. Only the three entry instructions are redirected.
    # 保留原版来源发现、异步作业、存档校验及卡带 I/O 函数体；只转接三个入口指令。
    for start, end in ((0x0019A94C, 0x0019AF04),
                       (0x0021A7E4, 0x0021AA0C), (0x0021AA10, 0x0021AB50),
                       (0x0021AB54, 0x0021ADCC), (0x00243510, 0x00243AC0),
                       (0x00244D2C, 0x002450F4)):
        if patched[start - IMAGE_BASE:end - IMAGE_BASE] != base[start - IMAGE_BASE:end - IMAGE_BASE]:
            raise ValueError(f"native source/save implementation changed: {start:08X}-{end:08X}")

    messages = load_helper(Path(__file__).resolve().parents[1] / "src" / "patch_messages.py")
    for archive in messages.ARCHIVES:
        source_entries = messages.message_codec.read_garc(
            (args.source_romfs / "a" / Path(archive)).read_bytes()
        )[2]
        output_entries = messages.message_codec.read_garc(
            (args.output_romfs / "a" / Path(archive)).read_bytes()
        )[2]
        source_lines = messages.message_codec.read_message_lines(
            source_entries[messages.MESSAGE_FILE_INDEX].files[0]
        )
        output_lines = messages.message_codec.read_message_lines(
            output_entries[messages.MESSAGE_FILE_INDEX].files[0]
        )
        if output_lines[:len(source_lines)] != source_lines:
            raise ValueError(f"stock messages changed in {archive}")
        if len(output_lines) != messages.DISCONNECT_LINE + 1:
            raise ValueError(f"unexpected appended line count in {archive}")

    print("combined Mover patch verification passed")


if __name__ == "__main__":
    main()
