#!/usr/bin/env python3
"""Statically verify the combined Poke Mover patch.

静态校验 Poke Mover 合并补丁。
"""

from __future__ import annotations

import argparse
import hashlib
import importlib.util
import subprocess
import sys
import zlib
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "common/tools"))
from patch_version import add_version_arguments, label_from_arguments, label_storage

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
PAYLOAD_START = 0x0028D2E0
EXPANDED_PAYLOAD_START = 0x00365000
EXPANDED_PAYLOAD_SIZE = 0x2BF000
SERVER_CONTEXT_SIZE = 0x104000
SERVER_RUNTIME_SIZE = 0x296000
TICKET_INTERFACES = (
    (0x00249754, "initialize", "ticketjob_initialize", 0x0023D3C0),
    (0x00249818, "campaignrequest", "movercampaign_request", 0x0023BE4C),
)
TICKET_SEND_CALLS = (0x0015B89C,0x0015BB84,0x0015B684,0x0015BA08,0x0015B794)
TICKET_EXPIRY_CALLS = (0x0023E384,0x0023E4B0)
TICKET_POLL_HOOKS = (
    (0x0023E230,"jobpoll","serverticket_poll","serverticket_originalpoll",0xE92D4070),
    (0x00249600,"statepoll","serverticket_updatestate","serverticket_originalstate",0xE92D43F8),
)
TICKET_UI_HOOKS = (
    (0x00249650, "combinepatch_ticketuiprepare", False, 0x002496B8),
    (0x002496CC, "combinepatch_ticketuifade", True, 0x0019983C),
    (0x00249704, "combinepatch_ticketjobuiprepare", False, 0x0024974C),
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


def verify_no_immediate_thumb_blx(image: bytes, start: int, end: int,
                                  constants: tuple[tuple[int, int], ...] = ()) -> None:
    """Reject importer-generated Thumb-to-ARM immediate calls.

    armips resolves R_ARM_THM_CALL as an immediate Thumb BLX without applying
    the interworking relocation correctly. All calls from these Thumb objects
    into ARM code must therefore use a literal address followed by BLX Rm.
    """
    for address in range((start + 1) & ~1, end - 2, 2):
        if any(address < upper and address + 4 > lower for lower, upper in constants):
            continue
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
    add_version_arguments(parser)
    args = parser.parse_args()
    subprocess.run([sys.executable, "-B", str(Path(__file__).absolute().parents[2] /
                    "common/tools/check_file_links.py"), "--recursive",
                    *(str(path) for path in vars(args).values() if isinstance(path, Path))], check=True)
    patch_label = label_from_arguments(args)

    base = args.base.read_bytes()
    patched = args.patched.read_bytes()
    digest = hashlib.sha256(base).hexdigest().upper()
    if digest != EXPECTED_BASE_SHA256:
        raise ValueError(f"unsupported base code SHA-256: {digest}")
    sym = symbols(args.symbols)
    retired={"deletefile","renamefile","getfilesize","readcompletefile","writecompletefile","writefilerangeutf16"}
    if retired.intersection(sym):
        raise ValueError("retired unreferenced filesystem wrappers are still imported")
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
        "online_fake_server_ticket_fallback", "combinepatch_onlineticketfallback",
        "combinepatch_codeusedend", "combinepatch_offlinepayloadusedend",
        "combinepatch_offlinepayloadstart", "textactualend", "textmappedend",
        "fshelpers_payloadbegin", "fshelpers_payloadend",
        "ndssources_payloadbegin", "ndssources_payloadend",
        "ndsfs_payloadbegin", "ndsfs_payloadend",
        "bankdataredirect_payloadbegin", "bankdataredirect_payloadend",
        "serverticket_payloadbegin", "serverticket_payloadend",
        "systemtime_payloadbegin", "systemtime_payloadend", "systemtime_getcurrentdate",
        "servervalidation_payloadbegin", "servervalidation_payloadend",
        "patchpaths_payloadbegin", "patchpaths_payloadend",
        "languagesettings_payloadbegin", "languagesettings_payloadend",
        "fakeserver_payloadbegin", "fakeserver_payloadend",
        "fakeserver_contextstart", "fakeserver_contextend", "fakeserver_contextsize",
        "codeexpansion_executablesize", "combinepatch_codeend",
        "fakeserverclient_get", "fakeserverclient_endsession", "fsserver_open", "fsserver_stage", "fsserver_ticket",
        "languagesettings_preparestartup", "languagesettings_cancelstartup",
        "languagesettings_storeselection",
        "languagesettings_pollsynchronization", "languagesettings_cancelsynchronization",
        "fsbanknative_readsettings", "fsbanknative_updatelanguage",
        "combinepatch_startuplanguagegate", "combinepatch_startuplanguagedestroy", "combinepatch_savebanklanguage",
        "combinepatch_languagereturnexit", "combinepatch_languagereturndestroy",
        "languageselection_setsettings",
        "combinepatch_titletextinitialize", "combinepatch_titlestateupdate",
        "combinepatch_titleprocessmodetoggle", "combinepatch_connectionconstruct",
        "combinepatch_connectionpoll", "combinepatch_connectionunbind",
        "combinepatch_networkavailability",
        "combinepatch_modestorage", "combinepatch_ticketwrappersend",
        "serverticket_send", "serverticket_campaignresult",
        "serverticket_initializeselected", "serverticket_backendstorage",
        "moverticketstate_campaigncallback", "crc16_calculate",
        "srv_getservicehandle", "runtimeenvironment_get",
        "runtimeenvironment_initializeticket", "runtimeenvironment_useserverticket",
        "runtimeenvironment_storagestart", "runtimeenvironment_storageend",
        "fakeserverclient_ticket",
        "combinepatch_eligibilityupdate", "combinepatch_getpokemonupdate",
        "combinepatch_sourcerecoveryupdate", "combinepatch_transactionrecoveryupdate",
        "bankdataredirect_sourcerecoveryupdate", "bankdataredirect_transactionrecoveryupdate",
        "bankdataredirect_originalsourcerecoveryupdate",
        "bankdataredirect_originaltransactionrecoveryupdate",
        "bankdataredirect_remotecheckupdate", "bankdataredirect_originalremotecheckupdate",
        "combinepatch_queryprovider", "nativeresponse_pollquery",
        "combinepatch_remotebind", "combinepatch_metadataprovider", "combinepatch_stageprovider",
        "combinepatch_commitprovider", "combinepatch_rollbackprovider",
        "bankdataredirect_transactionprovider", "bankdataredirect_nosaveupdate",
        "nativeresponse_transactionreply", "nativeresponse_pollaccepted",
        "nativeresponse_acceptmetadata", "nativeresponse_pollmetadata",
        "nativeresponse_createconnection", "nativeresponse_pollconnection",
        "servervalidation_start", "servervalidation_poll",
        "ndssources_listupdate", "ndssources_selectlistid",
        "ndssources_titlemessage", "combinepatch_selectndsgametitle",
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
        required.update((f"combinepatch_ticket{operation}", native_name))
        if operation != "initialize":
            required.add(f"combinepatch_ticket{operation}local")
    missing = sorted(required - sym.keys())
    if missing:
        raise ValueError(f"missing armips symbols: {', '.join(missing)}")
    if word(patched, sym["combinepatch_titletextinitialize"] + 16) != 0xE5DC3001:
        raise ValueError("title initializer must restore the retained mode selection")
    code_start = EXPANDED_PAYLOAD_START
    payload_end = EXPANDED_PAYLOAD_START + EXPANDED_PAYLOAD_SIZE
    code_end = payload_end - SERVER_RUNTIME_SIZE
    if (sym["fakeserver_contextsize"] != SERVER_CONTEXT_SIZE or
            sym["fakeserver_runtimesize"] != SERVER_RUNTIME_SIZE or
            sym["codeexpansion_executablesize"] != code_end - code_start):
        raise ValueError("service storage or executable-only protection size differs")
    if (sym["fakeserver_contextstart"] != code_end or
            sym["fakeserver_contextend"] != code_end+SERVER_CONTEXT_SIZE or sym["combinepatch_codeend"] != code_end):
        raise ValueError("fakeServer context overlaps code or original memory")
    worker=code_end+SERVER_CONTEXT_SIZE
    if (sym["fakeserver_workerstart"] != worker or
            sym["fakeserver_taskbodystart"] != worker+0x1000 or
            sym["fakeserver_workerstackstart"] != worker+0x101000 or
            sym["fakeserver_workerstackend"] != worker+0x111000 or
            sym["fakeserver_reservedtailstart"] != worker+0x111000 or
            sym["fakeserver_runtimeend"] != payload_end):
        raise ValueError("process-owned worker/control/body/stack layout differs")
    if (sym.get("localsource_contextstart") != worker+0x111000 or
            sym.get("localsource_imagestart") != worker+0x112000 or
            sym.get("localsource_imageend") != payload_end or
            payload_end-sym["localsource_imagestart"] != 0x80000):
        raise ValueError("independent local Gen5 image overlaps worker, Bank context or code")
    client = patched[sym["fakeserverclient_get"] - IMAGE_BASE:
                     sym["fakeserverclient_endsession"] - IMAGE_BASE]
    # The aligned address can be synthesized rather than stored in a literal pool.
    # 对齐地址也可由立即数与移位合成，不一定使用字面量池。
    context_load = word(patched, sym["fakeserverclient_get"] + 4)
    rotation = ((context_load >> 8) & 15) * 2
    immediate = context_load & 255
    context_value = ((immediate >> rotation) | (immediate << (32 - rotation))) & 0xFFFFFFFF
    thumb_move = int.from_bytes(client[:2], "little")
    thumb_shift = int.from_bytes(client[4:6], "little")
    thumb_context = (thumb_move & 0xFF00 == 0x2000 and thumb_shift & 0xF83F == 0 and
                     (thumb_move & 255) << ((thumb_shift >> 6) & 31) == code_end)
    # New aligned constants can use any low register, not necessarily the
    # entry's r0. Accept an exact adjacent MOVS/LSLS synthesis with matching
    # source/destination registers; do not treat arbitrary immediates as binds.
    for position in range(0,len(client)-3,2):
        move=int.from_bytes(client[position:position+2],'little')
        shift=int.from_bytes(client[position+2:position+4],'little')
        register=(move>>8)&7
        if (move&0xf800==0x2000 and shift&0xf800==0 and
                (shift&7)==register and ((shift>>3)&7)==register and
                (move&255)<<((shift>>6)&31)==code_end):
            thumb_context=True
    if (code_end.to_bytes(4, "little") not in client and not thumb_context and
            not (context_load & 0xFFFFF000 == 0xE3A00000 and context_value == code_end)):
        raise ValueError("fakeServer client does not bind the reserved context")
    if any(patched[code_end - IMAGE_BASE:payload_end - IMAGE_BASE]):
        raise ValueError("fakeServer context is not initially zero-filled")
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
        ("serverticket_payloadbegin", "serverticket_payloadend"),
        ("systemtime_payloadbegin", "systemtime_payloadend"),
        ("consolesettings_payloadbegin", "consolesettings_payloadend"),
        ("bankdataredirect_payloadbegin", "bankdataredirect_payloadend"),
        ("patchpaths_payloadbegin", "patchpaths_payloadend"),
        ("servervalidation_payloadbegin", "servervalidation_payloadend"),
        ("ndsfs_payloadbegin", "ndsfs_payloadend"),
        ("languagesettings_payloadbegin", "languagesettings_payloadend"),
        ("fakeserver_payloadbegin", "fakeserver_payloadend"),
    )
    for begin, end in payload_objects:
        if sym[begin] >= sym[end]:
            raise ValueError(f"empty or reversed payload object: {begin}")
    service_image=patched[sym["fakeserver_payloadbegin"]-IMAGE_BASE:sym["fakeserver_payloadend"]-IMAGE_BASE]
    for obsolete in (b"/3ds/Bank/gen5.tmp\0",b"/3ds/Bank/gen5.bak\0"):
        if obsolete in service_image:
            raise ValueError(f"retired global NDS work path remains: {obsolete!r}")
    if not sym["fakeserver_payloadbegin"] <= sym["fstransaction_sourcepath"] < sym["fakeserver_payloadend"]:
        raise ValueError("sibling NDS path resolver is outside the service payload")
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
    # A byte pattern inside a CRC table is data, not an interworking call.
    # Exempt only the exact polynomial-verified constant table, never code.
    crc_start = sym["fshash_crctable"]
    crc_end = crc_start + 256 * 4
    if crc_start & 3 or not sym["fakeserver_payloadbegin"] <= crc_start < crc_end <= sym["fakeserver_payloadend"]:
        raise ValueError("CRC table is outside the checked fake-server object")
    for index in range(256):
        value = index
        for _ in range(8):
            value = (value >> 1) ^ (0xEDB88320 if value & 1 else 0)
        if word(patched, crc_start + index * 4) != value:
            raise ValueError("CRC-32 constant table changed")
    for module in ("fshelpers", "ndssources", "bankdataredirect",
                   "servervalidation", "ndsfs", "languagesettings", "fakeserver"):
        verify_no_immediate_thumb_blx(
            patched, sym[f"{module}_payloadbegin"], sym[f"{module}_payloadend"],
            ((crc_start, crc_end),) if module == "fakeserver" else ()
        )
    thumb_tail_routes = (
        ("combinepatch_remotecheckupdate", "bankdataredirect_remotecheckupdate"),
        ("combinepatch_remotebind", "bankdataredirect_bindremote"),
        ("combinepatch_eligibilityupdate", "bankdataredirect_eligibilityupdate"),
        ("combinepatch_notransferupdate", "bankdataredirect_nosaveupdate"),
        ("combinepatch_transactionrecoveryupdate", "bankdataredirect_transactionrecoveryupdate"),
    )
    for wrapper_name, helper_name in thumb_tail_routes:
        wrapper = sym[wrapper_name]
        if arm_literal_value(patched, wrapper + 12, 12, 0) != sym[helper_name] + 1 or \
                word(patched, wrapper + 16) != 0x012FFF1C:
            raise ValueError(f"incorrect conditional Thumb dispatch: {wrapper_name}")
    # Keep the marker in the final mapped text bytes. The remaining bytes stay
    # zero for a longer future identifier or metadata.
    # 将标识固定在已映射 text 段的最后部分。其余字节保持为零，供未来更长的标识
    # 或元数据使用。
    if sym["offlinepatch_versionidentifier"] != VERSION_STORAGE_START:
        raise ValueError("offline patch version identifier moved")
    version_offset = VERSION_STORAGE_START - IMAGE_BASE
    expected_version_storage = label_storage(patch_label)
    if patched[version_offset:version_offset + VERSION_STORAGE_SIZE] != expected_version_storage:
        raise ValueError("offline patch version storage is malformed")
    if base[version_offset:version_offset + VERSION_STORAGE_SIZE] != bytes(VERSION_STORAGE_SIZE):
        raise ValueError("offline patch version storage no longer uses original padding")
    if VERSION_STORAGE_START + VERSION_STORAGE_SIZE != TEXT_MAPPED_END:
        raise ValueError("offline patch version storage no longer ends with mapped text")

    expected_base_words = {
        0x0024A0C4: 0xE92D4070,
        0x0024BA84: 0xEBFD4261,
        0x00249F20: 0xE5D0103C,
        0x00248A08: 0xE92D4070,
        0x00248B88: 0xEBFB1ACF,
        0x00248B98: 0xE594100C,
        0x00248BC0: 0xE3A0100D,
        0x00249600: 0xE92D43F8,
        0x00248814: 0xE92D41F0,
        0x00246B68: 0xE92D4070,
        0x00246EC8: 0xE3A0100F,
        0x00243AF4: 0xE92D40F0,
        0x00248C68: 0xE92D4070,
        0x002455D0: 0xE92D4FF0,
        0x00246A5C: 0xE3A0100F,
        0x00245728: 0xE59F0C74,
        0x00246368: 0xE28DD0FC,
        0x002460B8: 0xE59F02E4,
        0x00248360: 0xE92D4070,
        0x00248500: 0xE3A0100F,
        0x0024709C: 0xE92D4010,
        0x002470F4: 0xE92D41F0,
        0x00247100: 0xEBFD5194,
        0x002472E8: 0xE59F1010,
        0x00247288: 0xE3A0100E,
        0x002472C4: 0xE3A01021,
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
        0x002445A4: 0xE92D41F0,
        0x0024483C: 0xEBFD59F5,
        0x0019B240: 0xEB000472,
        0x0024149C: 0xE1C605B8,
        0x0021A7E0: 0xE92D4008,
        0x0021AA0C: 0xE92D40F0,
        0x0021AB50: 0xE92D4FFF,
        0x002E6620: 0x00244D2C,
        0x0024538C: 0xE5D01038,
        0x00245450: 0xE92D4010,
    }
    for address, expected in expected_base_words.items():
        actual = word(base, address)
        if actual != expected:
            raise ValueError(
                f"unexpected base instruction at {address:08X}: "
                f"expected {expected:08X}, found {actual:08X}"
            )

    hooks = {
        0x00240F38: ("combinepatch_startuplanguagegate", False, 0xE),
        0x00241024: ("combinepatch_startuplanguagedestroy", False, 0xE),
        0x002470F4: ("combinepatch_languagereturnexit", False, 0xE),
        0x00247100: ("combinepatch_disconnectminimumwait", True, 0xE),
        0x002472E8: ("combinepatch_languagereturndestroy", False, 0xE),
        0x0024B92C: ("combinepatch_sourcesavebegin", False, 0xE),
        0x0024B868: ("combinepatch_sourcesavepoll", False, 0xE),
        0x0024A0C4: ("combinepatch_savestateupdate", False, 0xE),
        0x0024BA84: ("combinepatch_titletextinitialize", True, 0xE),
        0x00249F20: ("combinepatch_titlestateupdate", False, 0xE),
        0x00248B88: ("combinepatch_networkavailability", True, 0xE),
        0x00248BAC: ("combinepatch_connectionconstruct", True, 0x1),
        0x002472A4: ("combinepatch_connectionconstruct", True, 0x1),
        0x0019A068: ("combinepatch_connectionpoll", False, 0xE),
        0x00240B24: ("combinepatch_connectionunbind", False, 0xE),
        0x0010C89C: ("combinepatch_networkmonitorupdate", False, 0xE),
        0x0023A05C: ("combinepatch_networkmonitorworkeraccount", True, 0xE),
        0x00248BC0: ("combinepatch_selectinitialconnectmessage", True, 0xE),
        0x00248814: ("combinepatch_remotecheckupdate", False, 0xE),
        0x0023F2FC: ("combinepatch_queryprovider", False, 0xE),
        0x0023F874: ("combinepatch_remoteunbind", False, 0xE),
        0x0023F944: ("combinepatch_downloadresponse", False, 0xE),
        0x00246B68: ("combinepatch_sourcerecoveryupdate", False, 0xE),
        0x00246EC8: ("combinepatch_selectbankconnectmessage", True, 0xE),
        0x00243AF4: ("combinepatch_transactionrecoveryupdate", False, 0xE),
        0x00248C68: ("combinepatch_eligibilityupdate", False, 0xE),
        0x002455D0: ("combinepatch_getpokemonupdate", False, 0xE),
        0x00246A5C: ("combinepatch_selectbankconnectmessage", True, 0xE),
        0x0019E7E8: ("combinepatch_validationhttplaunch", True, 0xE),
        0x00248360: ("combinepatch_notransferupdate", False, 0xE),
        0x00248500: ("combinepatch_selectbankconnectmessage", True, 0xE),
        0x00247288: ("combinepatch_selectdisconnectmessage", True, 0xE),
        0x002472C4: ("combinepatch_selectclosingmessage", True, 0xE),
        0x0019BB34: ("combinepatch_remotebind", False, 0xE),
        0x0023EBE0: ("combinepatch_metadataprovider", False, 0xE),
        0x0023EAAC: ("combinepatch_stageprovider", False, 0xE),
        0x0019B998: ("combinepatch_commitprovider", False, 0xE),
        0x0019B84C: ("combinepatch_rollbackprovider", False, 0xE),
        0x0024A6F0: ("combinepatch_selectsavemessage", True, 0xE),
        0x00244870: ("combinepatch_selectndssource", True, 0xE),
        0x002445A4: ("combinepatch_ndsselectionupdateentry", False, 0xE),
        0x0024483C: ("combinepatch_selectionconfirmfade", True, 0xE),
        0x0019B240: ("combinepatch_selectndsgametitle", True, 0xE),
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
    size_load = word(patched, loader_start + 8)
    rotation = ((size_load >> 8) & 15) * 2
    immediate = size_load & 255
    size_value = ((immediate >> rotation) | (immediate << (32 - rotation))) & 0xFFFFFFFF
    if (arm_literal_value(patched, loader_start + 4, 0) != EXPANDED_PAYLOAD_START or
            size_load & 0xFFFFF000 != 0xE3A01000 or size_value != EXPANDED_PAYLOAD_SIZE - SERVER_RUNTIME_SIZE):
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
    for address in TICKET_EXPIRY_CALLS:
        if branch_target(base, address) != (0x0019D8C8, True, 0xE):
            raise ValueError("unexpected native expiry request")
        if branch_target(patched,address)!=(sym["combinepatch_ticketexpiryrequest"],True,0xE):
            raise ValueError("local expiry must use the original helper's async mode")
    for start,end in ((0x0019D8C8,0x0019DF70),(0x0019DF70,0x0019E454)):
        if patched[start-IMAGE_BASE:end-IMAGE_BASE] != base[start-IMAGE_BASE:end-IMAGE_BASE]:
            raise ValueError("native ticket helper/wait must not be changed for background delivery")
    hooks[0x002403B8] = ("combinepatch_validationunbind", False, 0xE)
    for address, operation, native_name, native_address in TICKET_INTERFACES:
        if branch_target(base, address) != (native_address, True, 0xE):
            raise ValueError(f"unexpected native ticket call at {address:08X}")
        if sym[native_name] != native_address:
            raise ValueError(f"incorrect Mover ticket address: {native_name}")
        hooks[address] = (f"combinepatch_ticket{operation}", True, 0xE)
    for address, wrapper, link, native in TICKET_UI_HOOKS:
        hooks[address] = (wrapper, link, 0xE)
        if link and branch_target(base, address) != (native, True, 0xE):
            raise ValueError(f"unexpected native applet fade at {address:08X}")
        start = sym[wrapper]
        if word(patched, start + 4) != 0xE5DCC000 or \
                word(patched, start + 8) != 0xE35C0000:
            raise ValueError(f"{wrapper} must use an Offline-only session test")
        if link:
            if word(patched, start + 12) != 0x03A00001 or \
                    word(patched, start + 16) != 0x012FFF1E or \
                    branch_target(patched, start + 20) != (native, False, 0xE):
                raise ValueError("native applet fade must remain available outside Offline")
        elif branch_target(patched, start + 12) != (native, False, 0) or \
                word(patched, start + 16) != word(base, address) or \
                branch_target(patched, start + 20) != (address + 4, False, 0xE):
            raise ValueError(f"{wrapper} must replay and resume native preparation")
    hooks[0x0024709C] = ("combinepatch_storagenoticedisconnectupdate", False, 0xE)
    for address, (name, link, condition) in hooks.items():
        target, actual_link, actual_condition = branch_target(patched, address)
        if (target, actual_link, actual_condition) != (sym[name], link, condition):
            raise ValueError(f"incorrect hook at {address:08X}")

    # Text IDs are meaningful only in their bound table. Scalar connection/
    # save calls consume r1 in generic table23; the game-title pane uses table24.
    # 编号必须对应已绑定的文本表：连接／保存的 r1 使用通用表 23，游戏名用表 24。
    for name,local_id,native_id in (
        ("selectinitialconnectmessage",69,13),("selectbankconnectmessage",70,15),
        ("selectsavemessage",71,9)):
        start=sym["combinepatch_"+name]
        expected=(0xE5DCC000,0xE35C0000,0x03A01000|local_id,
                  0x13A01000|native_id,0xE12FFF1E)
        if arm_literal_value(patched,start,12)!=sym["combinepatch_modestorage"] or \
                tuple(word(patched,start+4+i*4) for i in range(5))!=expected:
            raise ValueError(f"text selector changes table/argument/native mode: {name}")
    for name,local_id,native_id in (("disconnect",72,14),("closing",33,33)):
        start=sym["combinepatch_select"+name+"message"]
        if arm_literal_value(patched,start,12)!=sym["combinepatch_modestorage"] or \
                tuple(word(patched,start+4+i*4) for i in range(5))!=(
                    0xE5DCC000,0xE35C0000,0x13A01000|native_id,0x112FFF1E,0xE92D400D) or \
                arm_literal_value(patched,start+24,12)!=sym["nativeresponse_takemissingbanknotice"]+1 or \
                tuple(word(patched,start+28+i*4) for i in range(5))!=(
                    0xE12FFF3C,0xE3500000,0x13A0104A,0x03A01000|local_id,0xE8BD800D):
            raise ValueError("missing-Bank notice changes native mode, message IDs or preserved UI arguments")
    minimum_wait=sym["combinepatch_disconnectminimumwait"]
    if arm_literal_value(patched,minimum_wait,12)!=sym["combinepatch_modestorage"] or \
            tuple(word(patched,minimum_wait+offset) for offset in (4,8,16,24,28,32))!=(
                0xE5DCC000,0xE35C0000,0xE92D400D,0xE12FFF3C,0xE1A01000,0xE8BD400D) or \
            arm_literal_value(patched,minimum_wait+20,12)!=sym["nativeresponse_disconnectminimumdelay"]+1 or \
            branch_target(patched,minimum_wait+12)!=(0x0019B758,False,0x1) or \
            branch_target(patched,minimum_wait+36)!=(0x0019B758,False,0xE):
        raise ValueError("disconnect minimum wait changes native mode, timing consumer or preserved arguments")
    for start,end in ((0x0019C700,0x0019C7CC),(0x001DBEB8,0x001DC0C0),
                      (0x0024B5D8,0x0024B61C),(0x002426D4,0x00242718)):
        if patched[start-IMAGE_BASE:end-IMAGE_BASE]!=base[start-IMAGE_BASE:end-IMAGE_BASE]:
            raise ValueError("native text table consumer/bounds/constructor changed")
    if word(patched,0x0024B5DC)!=0xE3A03017 or word(patched,0x002426FC)!=0xE3A0C018:
        raise ValueError("generic table23 or game-title table24 resource selection changed")

    # The local response adapter must not reintroduce a fixed display delay.
    # 本地响应入口不再添加固定显示延时；保存参数后直接轮询并续接原状态机。
    entry = sym["offlinepatch_getpokemonentry"]
    if word(patched, entry) != 0xE92D4001 or \
            arm_literal_value(patched, entry + 4, 12) != sym["servervalidation_poll"] + 1 or \
            word(patched, entry + 8) != 0xE12FFF3C or \
            word(patched, entry + 12) != 0xE8BD4001 or \
            word(patched, entry + 16) != 0xE92D4FF0 or \
            branch_target(patched, entry + 20) != (0x002455D4, False, 0xE):
        raise ValueError("local response entry adds a delay or changes the native continuation")

    # Keep SDK job delivery and worker stop/lock/sleep/exit code native.
    # 保留原 SDK 作业交付及后台线程停止／锁／休眠／退出。
    monitor = sym["combinepatch_networkmonitorupdate"]
    worker = sym["combinepatch_networkmonitorworkeraccount"]
    for entry in (monitor, worker):
        if [word(patched, entry + offset) for offset in (0, 4, 8, 24)] != \
                [0xE59FC010, 0xE5DCC000, 0xE35C0000, sym["combinepatch_modestorage"]]:
            raise ValueError("connection monitor must gate only the latched Offline mode")
    if word(patched, monitor + 12) != 0x012FFF1E or \
            word(patched, monitor + 16) != 0xE92D4070 or \
            branch_target(patched, monitor + 20) != (0x0010C8A0, False, 0xE):
        raise ValueError("native connection monitor prologue/continuation changed")
    if [word(patched, worker + offset) for offset in (12, 16, 20)] != \
            [0x03A04000, 0x15964028, 0xE12FFF1E]:
        raise ValueError("background monitor must retain native account loading outside Offline")
    for start, end, site in ((0x0010C89C, 0x0010CA30, 0x0010C89C),
                            (0x0023A020, 0x0023A2A0, 0x0023A05C)):
        expected = bytearray(base[start - IMAGE_BASE:end - IMAGE_BASE])
        expected[site-start:site-start+4] = patched[site-IMAGE_BASE:site+4-IMAGE_BASE]
        if patched[start - IMAGE_BASE:end - IMAGE_BASE] != expected:
            raise ValueError("native connection monitor lifecycle changed outside its mode gate")

    save_wrapper = sym["combinepatch_savestateupdate"]
    native_save = sym["bankdataredirect_originalsaveupdate"]
    if arm_literal_value(patched, save_wrapper, 12) != sym["bankdataredirect_saveupdate"] + 1 or \
            word(patched, save_wrapper + 4) != 0xE12FFF1C:
        raise ValueError("save accounting must interwork to Thumb in both modes")
    if native_save != save_wrapper + 8 or word(patched, native_save) != 0xE92D4070 or \
            branch_target(patched, native_save + 4) != (0x0024A0C8, False, 0xE):
        raise ValueError("online source save does not replay the native prologue and continuation")
    save_start, save_end = 0x0024A0C4, 0x0024A660
    expected_save = bytearray(base[save_start - IMAGE_BASE:save_end - IMAGE_BASE])
    for address in hooks:
        if save_start <= address < save_end:
            expected_save[address - save_start:address - save_start + 4] = \
                patched[address - IMAGE_BASE:address - IMAGE_BASE + 4]
    if patched[save_start - IMAGE_BASE:save_end - IMAGE_BASE] != expected_save:
        raise ValueError("native save/source-cleanup body changed outside checked hooks")

    for start, end, bridge, prologue in (
        (0x0019BB34, 0x0019BD24, "combinepatch_remotebind", 0xE92D4070),
    ):
        if patched[start + 4 - IMAGE_BASE:end - IMAGE_BASE] != base[start + 4 - IMAGE_BASE:end - IMAGE_BASE]:
            raise ValueError(f"native remote body changed beyond entry at {start:08X}")
        wrapper = sym[bridge]
        if word(base, start) != prologue or word(patched, wrapper + 20) != prologue or \
                branch_target(patched, wrapper + 24) != (start + 4, False, 0xE):
            raise ValueError(f"native remote prologue/continuation changed at {start:08X}")
    for start, end in ((0x0023E9EC,0x0023EB94),(0x0023EB94,0x0023ECB0),
                       (0x0019B91C,0x0019BA68),(0x0019B7D0,0x0019B91C)):
        expected = bytearray(base[start-IMAGE_BASE:end-IMAGE_BASE])
        for address in hooks:
            if start <= address < end:
                expected[address-start:address-start+4] = patched[address-IMAGE_BASE:address-IMAGE_BASE+4]
        if patched[start-IMAGE_BASE:end-IMAGE_BASE] != expected:
            raise ValueError(f"native transaction SDK construction/observer changed at {start:08X}")
    no_save_start, no_save_end = 0x00248360, 0x0024853C
    expected_exit = bytearray(base[no_save_start - IMAGE_BASE:no_save_end - IMAGE_BASE])
    for address in hooks:
        if no_save_start <= address < no_save_end:
            expected_exit[address - no_save_start:address - no_save_start + 4] = patched[address - IMAGE_BASE:address - IMAGE_BASE + 4]
    if patched[no_save_start - IMAGE_BASE:no_save_end - IMAGE_BASE] != expected_exit:
        raise ValueError("native no-transfer request/wait/destruction changed outside checked hooks")

    # Keep native state, callback and cleanup except for interface calls and
    # Offline applet UI edges; no whole-state ticket bypass remains.
    # 除接口调用和离线小程序 UI 边界外，状态、回调与清理不变；不整段跳过票务流程。
    state_start, state_end = 0x00249600, 0x00249D38
    expected_state = bytearray(base[state_start - IMAGE_BASE:state_end - IMAGE_BASE])
    for address, _, _, _ in TICKET_INTERFACES:
        expected_state[address - state_start:address - state_start + 4] = \
            patched[address - IMAGE_BASE:address - IMAGE_BASE + 4]
    for address, _, _, _ in TICKET_UI_HOOKS:
        expected_state[address - state_start:address - state_start + 4] = \
            patched[address - IMAGE_BASE:address - IMAGE_BASE + 4]
    expected_state[:4]=patched[state_start-IMAGE_BASE:state_start-IMAGE_BASE+4]
    if patched[state_start - IMAGE_BASE:state_end - IMAGE_BASE] != expected_state:
        raise ValueError("native ticket state/callback/cleanup changed outside its interfaces")
    for start, end in ((0x0023D3C0, 0x0023E910), (0x0015B590,0x0015BBD8), (0x0023BC50, 0x0023BF24),
                       (0x00199720, 0x0019983C), (0x0019BD88, 0x0019BE68)):
        expected=bytearray(base[start-IMAGE_BASE:end-IMAGE_BASE])
        for address in (*TICKET_SEND_CALLS,*TICKET_EXPIRY_CALLS,0x0023E230,0x0023D418,0x0023E66C):
            if start<=address<end:
                expected[address-start:address-start+4]=patched[address-IMAGE_BASE:address-IMAGE_BASE+4]
        if patched[start - IMAGE_BASE:end - IMAGE_BASE] != expected:
            raise ValueError(f"native ticket implementation changed: {start:08X}-{end:08X}")
    if sym["moverticketstate_campaigncallback"] != 0x0024991C or \
            sym["crc16_calculate"] != 0x0019BD88:
        raise ValueError("local campaign callback/CRC addresses differ from Mover")

    wrapper_start = sym["combinepatch_ticketinitialize"]
    wrapper_end = sym["combinepatch_ticketwrappersend"]
    if not code_start <= wrapper_start < wrapper_end <= sym["fshelpers_payloadbegin"]:
        raise ValueError("ticket wrappers exceed the added pages")
    policy = sym["online_fake_server_ticket_fallback"]
    policy_address = sym["combinepatch_onlineticketfallback"]
    if policy not in (0, 1) or policy_address != wrapper_end or \
            patched[policy_address - IMAGE_BASE] != policy:
        raise ValueError("ticket fallback policy must be one byte: 0 native, 1 auto-server")
    environment_start = sym["runtimeenvironment_storagestart"]
    if environment_start != RUNTIME_STORAGE_START - 0x10 or \
            sym["runtimeenvironment_storageend"] != RUNTIME_STORAGE_START or \
            environment_start < BSS_ACTUAL_END:
        raise ValueError("runtime capability cache overlaps native BSS or mode storage")
    for name in ("runtimeenvironment_get", "runtimeenvironment_initializeticket",
                 "runtimeenvironment_useserverticket"):
        if not sym["codeexpansion_loaderbegin"] <= sym[name] < sym["codeexpansion_loaderend"]:
            raise ValueError("runtime capability code must remain in executable loader padding")
    backend = sym["serverticket_backendstorage"]
    if backend != sym["offlinepatch_runtimestoragestart"] + 8:
        raise ValueError("ticket backend flag overlaps native or scanner/mode storage")
    if sym["srv_getservicehandle"] != 0x001E596C:
        raise ValueError("Mover SRV entry point does not match its native image")
    selector = sym["serverticket_initializeselected"]
    if not sym["serverticket_payloadbegin"] <= selector < sym["serverticket_payloadend"]:
        raise ValueError("ticket selector is outside its module")
    if word(patched, wrapper_start) != 0xE5941028 or \
            word(patched, wrapper_start + 8) != 0xE5DC2000 or \
            word(patched, wrapper_start + 16) != 0xE5DC3000:
        raise ValueError("ticket selector arguments differ from job/shared/mode/policy ABI")
    for offset, pointer in ((4, sym["combinepatch_modestorage"]),
                            (12, policy_address)):
        if arm_literal_value(patched, wrapper_start + offset, 12) != pointer:
            raise ValueError("ticket initialization argument pointer differs")
    if branch_target(patched, wrapper_start + 20) != (selector, False, 0xE):
        raise ValueError("ticket initialization does not tail-call its backend selector")
    for _, operation, native_name, _ in TICKET_INTERFACES:
        if operation == "initialize":
            continue
        wrapper = sym[f"combinepatch_ticket{operation}"]
        local = sym[f"combinepatch_ticket{operation}local"]
        if local != wrapper + 20:
            raise ValueError(f"incorrect ticket {operation} wrapper layout")
        for offset, pointer in ((0, backend),):
            address = wrapper + offset
            instruction = word(patched, address)
            if instruction & 0xFFFFF000 != 0xE59FC000:
                raise ValueError(f"ticket {operation} does not load its selected-backend pointer")
            literal = address + 8 + (instruction & 0xFFF)
            if not wrapper_start <= literal < wrapper_end or word(patched, literal) != pointer:
                raise ValueError(f"incorrect ticket {operation} backend literal")
            if word(patched, address + 4) != 0xE5DCC000 or word(patched, address + 8) != 0xE35C0000:
                raise ValueError(f"ticket {operation} does not test its backend byte")
        for offset, target, condition in ((12, local, 0x1),
                                           (16, sym[native_name], 0xE)):
            if branch_target(patched, wrapper + offset) != (target, False, condition):
                raise ValueError(f"incorrect ticket {operation} route")
        if operation == "campaignrequest":
            if word(patched, local) != 0xE1A00004 or \
                    branch_target(patched, local + 4) != (sym["serverticket_campaignresult"], False, 0xE):
                raise ValueError("local campaign completion loses the state argument")

    for name in ("send","providerbind","providerunbind"):
        wrapper=sym[f"combinepatch_ticket{name}"]
        if arm_literal_value(patched,wrapper,12)!=backend or \
                word(patched,wrapper+4)!=0xE5DCC000 or word(patched,wrapper+8)!=0xE35C0000:
            raise ValueError("ticket transport backend selection differs")
        if name=="send":
            if branch_target(patched,wrapper+12)!=(sym["serverticket_send"],False,1) or \
                    branch_target(patched,wrapper+16)!=(0x001D17E0,False,0xE):
                raise ValueError("EC send must retain native transport and local parser adapter")
        elif name=="providerunbind":
            if branch_target(patched,wrapper+12)!=(sym["serverticket_cancelprovider"],False,1) or \
                    word(patched,wrapper+16)!=0xE12FFF11:
                raise ValueError("ticket unbind must cancel local work and preserve the native virtual call")
        elif word(patched,wrapper+12)!=0x13A00001 or word(patched,wrapper+16)!=0x112FFF1E or \
                word(patched,wrapper+20)!=0xE12FFF12:
            raise ValueError("SDK provider binding does not retain its native virtual call")
    for address in TICKET_SEND_CALLS:
        if branch_target(base,address)!=(0x001D17E0,True,0xE) or \
                branch_target(patched,address)!=(sym["combinepatch_ticketsend"],True,0xE):
            raise ValueError("incorrect local EC request hook")
    for address,name in ((0x0023D418,"providerbind"),(0x0023E66C,"providerunbind")):
        if branch_target(patched,address)!=(sym[f"combinepatch_ticket{name}"],True,0xE):
            raise ValueError("incorrect local SDK provider hook")
    for address,name,local,original,instruction in TICKET_POLL_HOOKS:
        wrapper=sym["combinepatch_ticket"+name]
        trampoline=sym[original]
        if branch_target(patched,address)!=(wrapper,False,0xE) or \
                arm_literal_value(patched,wrapper,12)!=backend or \
                word(patched,wrapper+4)!=0xE5DCC000 or word(patched,wrapper+8)!=0xE35C0000 or \
                branch_target(patched,wrapper+12)!=(sym[local],False,1) or trampoline!=wrapper+16 or \
                word(base,address)!=instruction or word(patched,trampoline)!=instruction or \
                branch_target(patched,trampoline+4)!=(address+4,False,0xE):
            raise ValueError("ticket poll must preserve native prologue, registers and online continuation")
    wrapper=sym["combinepatch_ticketexpiryrequest"]
    if arm_literal_value(patched,wrapper,12)!=backend or word(patched,wrapper+4)!=0xE5DCC000 or \
            word(patched,wrapper+8)!=0xE35C0000 or word(patched,wrapper+12)!=0x13A03000 or \
            branch_target(patched,wrapper+16)!=(0x0019D8C8,False,0xE):
        raise ValueError("expiry adapter must change only the local async argument")

    validation_launch = sym["combinepatch_validationhttplaunch"]
    unbind=sym["combinepatch_validationunbind"]
    original=sym["servervalidation_originalunbindclient"]
    if arm_literal_value(patched,unbind,12)!=MODE_STORAGE or \
            word(patched,unbind+4)!=0xE5DCC000 or word(patched,unbind+8)!=0xE35C0000 or \
            arm_literal_value(patched,unbind+12,12,0)!=sym["servervalidation_unbindclient"]+1 or \
            word(patched,unbind+16)!=0x012FFF1C or original!=unbind+20 or \
            word(patched,original)!=word(base,0x002403B8) or \
            branch_target(patched,original+4)!=(0x002403BC,False,0xE):
        raise ValueError("validation cancellation must preserve original unbind outside Offline")
    if arm_literal_value(patched, validation_launch + 0x0C, 12, 0) != sym["servervalidation_start"] + 1 or \
            word(patched, validation_launch + 0x10) != 0x012FFF1C or \
            branch_target(patched, validation_launch + 0x14) != (0x0018A1A0, False, 0xE):
        raise ValueError("validation HTTP launch loses native dispatch/interworking")

    request_start, request_end = 0x0019E5A0, 0x0019E90C
    original_request = bytearray(patched[request_start - IMAGE_BASE:request_end - IMAGE_BASE])
    original_request[0x0019E7E8 - request_start:0x0019E7EC - request_start] = \
        base[0x0019E7E8 - IMAGE_BASE:0x0019E7EC - IMAGE_BASE]
    if original_request != base[request_start - IMAGE_BASE:request_end - IMAGE_BASE]:
        raise ValueError("native validation request construction/registration changed")

    for address,result,name,operation,job in (
            (0x0023F2FC,0x0023F318,"combinepatch_queryprovider",4,5),
            (0x0023F788,0x0023F7A0,"combinepatch_downloadprovider",6,6),
            (0x0023EEC8,0x0023EEE0,"combinepatch_metadataupdateprovider",10,5),
            (0x0023EAAC,0x0023EAC4,"combinepatch_stageprovider",8,6)):
        bridge=sym[name]
        call=word(patched,bridge+32)
        if (word(base,address)!=0xE5940010 or
                word(patched,bridge+16)!=0xE92D4010 or
                word(patched,bridge+20)!=0xE1A00004 or
                word(patched,bridge+24)!=0xE1A01000|job or
                word(patched,bridge+28)!=0xE3A02000|operation or
                call&0xFFFFF000!=0xE59FC000 or
                word(patched,bridge+40+(call&0xFFF))!=sym["nativeresponse_acceptoperation"]+1 or
                word(patched,bridge+36)!=0xE12FFF3C or
                word(patched,bridge+40)!=0xE8BD4010 or
                branch_target(patched,bridge+44)!=(result,False,0xE) or
                word(patched,bridge+48)!=0xE5940010 or
                branch_target(patched,bridge+52)!=(address+4,False,0xE)):
            raise ValueError("asynchronous transport bridge does not preserve native provider dispatch")
    unbind=sym["combinepatch_remoteunbind"]
    cancel=word(patched,unbind+4)
    if (word(base,0x0023F874)!=0xE92D4070 or
            word(patched,unbind)!=0xE92D500F or cancel&0xFFFFF000!=0xE59FC000 or
            word(patched,unbind+12+(cancel&0xFFF))!=sym["nativeresponse_cancelremote"]+1 or
            word(patched,unbind+8)!=0xE12FFF3C or word(patched,unbind+12)!=0xE8BD500F or
            word(patched,unbind+16)!=0xE92D4070 or
            branch_target(patched,unbind+20)!=(0x0023F878,False,0xE)):
        raise ValueError("remote cleanup must cancel delivery then retain its native body")
    response=sym["combinepatch_downloadresponse"]
    original=sym["bankdataredirect_originaldownloadresponse"]
    if (word(base,0x0023F944)!=0xE92D4070 or
            branch_target(patched,response+12)!=(original,False,0x1) or
            arm_literal_value(patched,response+20,12,0xE)!=sym["nativeresponse_downloadresponse"]+1 or
            word(patched,response+24)!=0xE12FFF1C or word(patched,original)!=0xE92D4070 or
            branch_target(patched,original+4)!=(0x0023F948,False,0xE) or
            patched[0x0023F948-IMAGE_BASE:0x0023FA58-IMAGE_BASE] != base[0x0023F948-IMAGE_BASE:0x0023FA58-IMAGE_BASE]):
        raise ValueError("download completion must preserve native callbacks and nonlocal dispatch")
    for start, end, entry in ((0x0023F2A8, 0x0023F3CC, 0x0023F2FC),
                               (0x00248814, 0x00248950, 0x00248814)):
        expected = bytearray(base[start - IMAGE_BASE:end - IMAGE_BASE])
        expected[entry - start:entry - start + 4] = patched[entry - IMAGE_BASE:entry - IMAGE_BASE + 4]
        if patched[start - IMAGE_BASE:end - IMAGE_BASE] != expected:
            raise ValueError("native query construction/wait body changed outside transport hooks")
    for start, end in ((0x0025C8AC, 0x0025C978), (0x00248950, 0x002489B0)):
        if patched[start - IMAGE_BASE:end - IMAGE_BASE] != base[start - IMAGE_BASE:end - IMAGE_BASE]:
            raise ValueError("native remote query consumer/destructor changed")

    # Conversion bodies may change only at checked locale/name callsites.
    # The Gen5 entry replays its displaced prologue; VC entries stay native.
    # 转换体只允许核对的地区／命名接点变化，Gen5 重放序言，VC 入口保留原版。
    name_hooks={
        0x00241574:("gen5",0xE92D4FF3),
    }
    for address,(kind,prologue) in name_hooks.items():
        target=sym["movernames_convert"+kind]
        original=sym["movernames_original"+kind]
        if (word(base,address)!=prologue or branch_target(patched,address)!=(target,False,0xE) or
                word(patched,original)!=prologue or branch_target(patched,original+4)!=(address+4,False,0xE)):
            raise ValueError(f"invalid native name conversion wrapper: {address:08X}")
    for address in (0x0024E48C,0x0024F428,0x002508B4):
        if word(patched,address)!=word(base,address):
            raise ValueError("VC conversion must not use post-conversion name wrappers")
    name_choice_hooks={0x00241FF4:"movernames_gen5defaultentry"}
    characters=sym["movergen5_namecharacters"]
    if (word(base,0x00242128)!=characters or
            patched[characters-IMAGE_BASE:characters-IMAGE_BASE+164]!=
            base[characters-IMAGE_BASE:characters-IMAGE_BASE+164]):
        raise ValueError("Gen5 source-name selection must retain the native glyph conversion table")
    choice=sym["movernames_gen5defaultentry"]
    if (word(base,0x00241FF4)!=0xE5D6000F or
            branch_target(patched,0x00241FF4)!=(choice,False,0xE) or
            [word(patched,choice+offset) for offset in (0,4,8,12,20,24,32)]!=
            [0xE92D500F,0xE28D0068,0xE1A0100B,0xE59D21B0,0xE3500000,0xE8BD500F,0xE5D6000F] or
            branch_target(patched,choice+16)!=(sym["movernames_gen5default"],True,0xE) or
            branch_target(patched,choice+28)!=(0x00242020,False,0x1) or
            branch_target(patched,choice+36)!=(0x00241FF8,False,0xE)):
        raise ValueError("Gen5 must choose the name before lookup/write and preserve its per-call decision")
    vc_name_hooks={
        **{address:(0x0019C8D0,"movernames_setvcnicknameentry") for address in
            (0x0024ECC4,0x0024ED80,0x0024EDC0,0x002510D0,0x00251190,0x002511D0)},
        **{address:(0x00235FF0,"movernames_setvctrainerentry") for address in
            (0x0024ED48,0x00251148)},
        **{address:(0x0019C8D0,"movernames_setvcbnicknameentry") for address in
            (0x0024FA74,0x0024FB24,0x0024FB64)},
        0x0024FAF0:(0x00235FF0,"movernames_setvcbtrainerentry"),
    }
    for address,(original,target) in vc_name_hooks.items():
        if (branch_target(base,address)!=(original,True,0xE) or
                branch_target(patched,address)!=(sym[target],True,0xE)):
            raise ValueError(f"invalid pre-write VC name hook: {address:08X}")
    for entry,offset,target in (
            ("movernames_setvcnicknameentry",0x114,"movernames_setvcnickname"),
            ("movernames_setvctrainerentry",0x110,"movernames_setvctrainer"),
            ("movernames_setvcbnicknameentry",0x110,"movernames_setvcnickname"),
            ("movernames_setvcbtrainerentry",0x10C,"movernames_setvctrainer")):
        address=sym[entry]
        if (word(patched,address)!=(0xE59D2000|offset) or
                word(patched,address+4)!=0xE1A03006 or
                branch_target(patched,address+8)!=(sym[target],False,0xE)):
            raise ValueError("VC name selection must use this slot's frame, not global candidate state")
    legacy_hooks={
        0x0024F6B0:(0x0019CD3C,"moverlegacy_createvcb"),
        **{address:(0x00198D60,"moverlegacy_decode") for address in (0x0024F470,0x0024F968,0x0024FAE4)},
        **{address:(0x00198CD0,"moverlegacy_setgender") for address in
            (0x0024F8F4,0x0024F90C)}
    }
    for address,(original,target) in legacy_hooks.items():
        if (branch_target(base,address)!=(original,True,0xE) or
                branch_target(patched,address)!=(sym[target],True,0xE)):
            raise ValueError(f"invalid VC legacy wrapper: {address:08X}")
    checked_name_hooks={0x00245F14:(0x00241574,"movernames_convertgen5checkedentry")}
    for address,(original,target) in checked_name_hooks.items():
        if (branch_target(base,address)!=(original,True,0xE) or
                branch_target(patched,address)!=(sym[target],True,0xE)):
            raise ValueError("accepted Gen5 names must preserve the slot's screening result")
    checked=sym["movernames_convertgen5checkedentry"]
    if ([word(patched,checked+offset) for offset in (0,4,8)]!=
            [0xE5952028,0xE2822048,0xE7922108] or
            branch_target(patched,checked+12)!=(sym["movernames_convertgen5checked"],False,0xE)):
        raise ValueError("Gen5 name handoff must read only this slot's decision and retain the ABI")
    locale_hooks = {
        0x00241B0C: (0x001E2594,"moverconsole_location"),
        0x00241B3C: (0x00109DBC,"moverconsole_region"),
        0x0024E9A8: (0x001E2594,"moverconsole_location"),
        0x0024E9D8: (0x00109DBC,"moverconsole_region"),
        0x0024F794: (0x001E2594,"moverconsole_location"),
        0x0024F7C4: (0x00109DBC,"moverconsole_region"),
        0x00250DC4: (0x001E2594,"moverconsole_location"),
        0x00250DF4: (0x00109DBC,"moverconsole_region"),
    }
    for address,(native,target) in locale_hooks.items():
        if (branch_target(base,address)!=(native,True,0xE) or
                branch_target(patched,address)!=(sym[target],True,0xE)):
            raise ValueError(f"invalid v1.5 conversion locale call: {address:08X}")
    for start,end in ((0x00109DBC,0x00109DDC),(0x001E2594,0x001E25B8),
                      (0x00109D20,0x00109D4C)):
        if patched[start-IMAGE_BASE:end-IMAGE_BASE]!=base[start-IMAGE_BASE:end-IMAGE_BASE]:
            raise ValueError("native CFG wrappers/UI language must remain unchanged")
    for start, end in ((0x00241574, 0x0024212C), (0x0024E48C,0x0024EE90),
                       (0x0024F428,0x0024FC2C),(0x002508B4,0x002512A8),
                       (0x00242F8C, 0x00243448),
                       (0x00245728, 0x00246A2C), (0x0023CBC0, 0x0023D298),
                       (0x002401F0, 0x00240300), (0x0025C62C, 0x0025C734)):
        expected=bytearray(base[start-IMAGE_BASE:end-IMAGE_BASE])
        for address in (*locale_hooks,*name_hooks,*legacy_hooks,*checked_name_hooks,
                        *name_choice_hooks,*vc_name_hooks):
            if start<=address<end:
                expected[address-start:address-start+4]=patched[address-IMAGE_BASE:address-IMAGE_BASE+4]
        if patched[start - IMAGE_BASE:end - IMAGE_BASE] != expected:
            raise ValueError(f"native validation/conversion range changed: {start:08X}-{end:08X}")

    if word(patched, 0x002E6620) != sym["ndssources_listupdate"] + 1:
        raise ValueError("NDS source-list vtable slot does not target the scanner")

    thumb_entries = (
        ("combinepatch_ndsselectionupdateentry", "ndssources_selectionupdate"),
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
        ("ndssources_originalsavebegin", 0xE92D4070, 0x0024B930),
        ("ndssources_originalsavepoll", 0xE92D4038, 0x0024B86C),
        ("ndssources_originalselectionupdate", 0xE92D41F0, 0x002445A8),
    )
    for name, prologue, continuation in trampoline_routes:
        address = sym[name]
        if word(patched, address) != prologue:
            raise ValueError(f"incorrect stock prologue replay in {name}")
        target, link, condition = branch_target(patched, address + 4)
        if (target, link, condition) != (continuation, False, 0xE):
            raise ValueError(f"incorrect stock continuation in {name}")
    for operation in ("begin", "poll"):
        address = sym[f"combinepatch_sourcesave{operation}"]
        if arm_literal_value(patched, address, 12) != sym[f"ndssources_save{operation}"] + 1 or \
                word(patched, address + 4) != 0xE12FFF1C:
            raise ValueError(f"source save {operation} loses ARM/Thumb ABI")

    # Preserve source discovery, save/card-I/O bodies, trainer display, and
    # the native title table outside the checked I/O, game-title and text hooks.
    # 除已核对的卡带 I/O 及游戏名钩子外，保留来源发现、存档／卡带读写函数体、
    # 训练家显示与原版游戏名表；连接文本的四字节钩子单独核对。
    for start, end in ((0x0019A94C, 0x0019AF04),
                       (0x0019B1F0, 0x0019B240), (0x0019B244, 0x0019B2E8),
                       (0x002B47E0, 0x002B488C),
                       (0x0021A7E4, 0x0021AA0C), (0x0021AA10, 0x0021AB50),
                       (0x0021AB54, 0x0021ADCC), (0x00243510, 0x00243AC0),
                       (0x00244D2C, 0x002450F4),
                       (0x00246B6C, 0x00246EC8), (0x00246ECC, 0x00246F04),
                       (0x00243AF8, 0x002444BC),
                       (0x00242D48, 0x00242D58)):
        if patched[start - IMAGE_BASE:end - IMAGE_BASE] != base[start - IMAGE_BASE:end - IMAGE_BASE]:
            raise ValueError(f"native source/save implementation changed: {start:08X}-{end:08X}")
    # Preserve native selection except the entry, fade-completion gate and
    # existing selected-ID hook; no menu/result/state-table rewrite is allowed.
    restored_selection=bytearray(patched[0x002445A4-IMAGE_BASE:0x002449D0-IMAGE_BASE])
    for address in (0x002445A4,0x0024483C,0x00244870):
        restored_selection[address-0x002445A4:address-0x002445A4+4]=base[address-IMAGE_BASE:address-IMAGE_BASE+4]
    if restored_selection!=base[0x002445A4-IMAGE_BASE:0x002449D0-IMAGE_BASE]:
        raise ValueError("native source selection changed outside its checked entry/fade/ID hooks")
    for entry, trampoline, prologue in (
        (0x00246B68, "bankdataredirect_originalsourcerecoveryupdate", 0xE92D4070),
        (0x00243AF4, "bankdataredirect_originaltransactionrecoveryupdate", 0xE92D40F0),
    ):
        address = sym[trampoline]
        if word(patched, address) != prologue or \
                branch_target(patched, address + 4) != (entry + 4, False, 0xE):
            raise ValueError(f"native recovery trampoline changed: {trampoline}")
    for start, end in ((0x0024B86C, 0x0024B92C), (0x0024B930, 0x0024BA6C)):
        if patched[start - IMAGE_BASE:end - IMAGE_BASE] != base[start - IMAGE_BASE:end - IMAGE_BASE]:
            raise ValueError(f"native source save dispatcher changed: {start:08X}-{end:08X}")

    for address, helper in ((0x0025C618, "combinepatch_savebanklanguage"),):
        if branch_target(patched, address) != (sym[helper], True, 0xE):
            raise ValueError("shared language hook does not call its wrapper")
    load_language = sym["combinepatch_startuplanguagegate"]
    for offset, expected in ((0,0xE92D4001),(8,0xE5911000),(12,0xE591101C),
                             (20,0xE12FFF3C),(24,0xE3500000),(28,0xE8BD4001)):
        if word(patched,load_language+offset)!=expected:
            raise ValueError("startup language gate does not preserve native controller/stack")
    if (arm_literal_value(patched,load_language+4,1)!=0x00329380 or
            arm_literal_value(patched,load_language+16,12)!=sym["languagesettings_preparestartup"]+1 or
            branch_target(patched,load_language+32)!=(0x00240FFC,False,0) or
            branch_target(patched,load_language+36)!=(0x00240D28,True,0xE) or
            branch_target(patched,load_language+40)!=(0x00240F3C,False,0xE)):
        raise ValueError("startup language gate does not wait before native initialization/fade")
    startup_destroy=sym["combinepatch_startuplanguagedestroy"]
    if (word(patched,startup_destroy)!=0xE92D4001 or
            arm_literal_value(patched,startup_destroy+4,12)!=sym["languagesettings_cancelstartup"]+1 or
            word(patched,startup_destroy+8)!=0xE12FFF3C or
            word(patched,startup_destroy+12)!=0xE8BD4001 or
            arm_literal_value(patched,startup_destroy+16,1)!=0x002E6438 or
            branch_target(patched,startup_destroy+20)!=(0x00241028,False,0xE)):
        raise ValueError("startup language cancellation does not preserve original destructor")
    store_language = sym["combinepatch_savebanklanguage"]
    if arm_literal_value(patched, store_language, 12) != \
            sym["languagesettings_storeselection"] + 1 or \
            word(patched, store_language + 4) != 0xE12FFF1C:
        raise ValueError("confirmed language selection does not interwork to Thumb")
    language_wait = sym["combinepatch_languagereturnexit"]
    for offset, expected in ((0,0xE92D4010),(4,0xE1A04000),(12,0xE12FFF3C),
                             (16,0xE3500000),(24,0xE1A00004),(32,0xE3500000),
                             (36,0xE1A00004),(40,0xE8BD4010),
                             (44,0x03A00000),(48,0x012FFF1E),(52,0xE92D41F0)):
        if word(patched,language_wait+offset)!=expected:
            raise ValueError("language return wait does not preserve the native exit/stack")
    if arm_literal_value(patched,language_wait+8,12)!=sym["languagesettings_pollsynchronization"]+1 or \
            branch_target(patched,language_wait+20)!=(language_wait+36,False,0) or \
            branch_target(patched,language_wait+28)!=(sym["moverstoragenotice_poll"],True,0xE) or \
            branch_target(patched,language_wait+56)!=(0x002470F8,False,0xE):
        raise ValueError("language synchronization does not wait in the animated return exit")
    language_destroy = sym["combinepatch_languagereturndestroy"]
    if (word(patched,language_destroy)!=0xE92D4001 or
            arm_literal_value(patched,language_destroy+4,12)!=sym["languagesettings_cancelsynchronization"]+1 or
            word(patched,language_destroy+8)!=0xE12FFF3C or
            arm_literal_value(patched,language_destroy+12,12)!=sym["nativeresponse_clearmissingbanknotice"]+1 or
            word(patched,language_destroy+16)!=0xE12FFF3C or
            word(patched,language_destroy+20)!=0xE8BD4001 or
            arm_literal_value(patched,language_destroy+24,1)!=0x002E679C or
            branch_target(patched,language_destroy+28)!=(0x002472EC,False,0xE)):
        raise ValueError("language return cancellation does not preserve the native destructor")
    if "languagesettings_takesyncresult" in sym:
        raise ValueError("synthetic language-save title notification was reintroduced")
    for start, end, hook in ((0x00240EF8, 0x00241024, 0x00240F38),
                             (0x00241024, 0x00241044, 0x00241024),
                             (0x0025C5BC, 0x0025C62C, 0x0025C618),
                             (0x002472E8, 0x00247304, 0x002472E8)):
        if patched[start - IMAGE_BASE:hook - IMAGE_BASE] != base[start - IMAGE_BASE:hook - IMAGE_BASE] or \
                patched[hook + 4 - IMAGE_BASE:end - IMAGE_BASE] != base[hook + 4 - IMAGE_BASE:end - IMAGE_BASE]:
            raise ValueError("native language startup/confirmation changed outside the hook")
    for start,end in ((0x002470F8,0x00247100),(0x00247104,0x00247234)):
        if patched[start-IMAGE_BASE:end-IMAGE_BASE]!=base[start-IMAGE_BASE:end-IMAGE_BASE]:
            raise ValueError("native disconnect exit changed outside the language/timing hooks")
    for start, end in ((0x00240D28,0x00240E50),(0x0022AF28, 0x0022AFD8), (0x0022C06C, 0x0022C078),
                       (0x0024538C,0x002453A8),(0x00245450,0x00245460),(0x002470A0,0x002470F4)):
        if patched[start - IMAGE_BASE:end - IMAGE_BASE] != base[start - IMAGE_BASE:end - IMAGE_BASE]:
            raise ValueError("native language resource binding or selection setter changed")
    notice=sym["combinepatch_storagenoticedisconnectupdate"]
    if branch_target(patched,notice+4)!=(sym["moverstoragenotice_poll"],True,0xE) or \
            branch_target(patched,notice+28)!=(0x002470A0,False,0xE):
        raise ValueError("SD notice does not resume the native disconnect update")
    for offset,value in ((0,0xE92D4001),(8,0xE3500000),(12,0xE8BD4001),
                         (16,0x03A00000),(20,0x012FFF1E),(24,0xE92D4010)):
        if word(patched,notice+offset)!=value:
            raise ValueError("SD notice does not preserve the disconnect stack/acknowledgement gate")

    from tools.title_layout import TITLE_LAYOUT_ARCHIVE, patch_title_archive
    relative = Path("a") / TITLE_LAYOUT_ARCHIVE
    if (args.output_romfs / relative).read_bytes() != patch_title_archive((args.source_romfs / relative).read_bytes(), patch_label=patch_label):
        raise ValueError("Mover title mode layout differs from its verified builder")
    from title_font import patch_title_fonts
    relative=Path("a/0/2/3")
    if (args.output_romfs/relative).read_bytes()!=patch_title_fonts((args.source_romfs/relative).read_bytes()):
        raise ValueError("Mover cross-language name/arrow fonts differ from their verified builder")
    messages = load_helper(Path(__file__).resolve().parents[1] / "src" / "patch_messages.py")
    game_titles = messages.read_game_titles(args.source_romfs)
    for archive in messages.ARCHIVES:
        source_entries = messages.message_codec.read_garc(
            (args.source_romfs / "a" / Path(archive)).read_bytes()
        )[2]
        output_entries = messages.message_codec.read_garc(
            (args.output_romfs / "a" / Path(archive)).read_bytes()
        )[2]
        source_lines = messages.message_codec.read_message_values(
            source_entries[messages.MESSAGE_FILE_INDEX].files[0]
        )
        output_lines = messages.message_codec.read_message_values(
            output_entries[messages.MESSAGE_FILE_INDEX].files[0]
        )
        if messages.MESSAGE_FILE_INDEX!=23 or len(source_lines)!=67 or len(output_lines)!=80 or \
                messages.GAME_TITLE_FILE_INDEX!=24:
            raise ValueError(f"text table identity/bounds changed in {archive}")
        expected_native = list(source_lines)
        language_units, language_flags = source_lines[messages.TITLE_LANGUAGE_LINE]
        if language_units.count(ord("L")) != 1:
            raise ValueError(f"unexpected native L label in {archive}")
        expected_native[messages.TITLE_LANGUAGE_LINE] = (
            [ord(messages.L_BUTTON) if unit == ord("L") else unit for unit in language_units],
            language_flags)
        if output_lines[:len(source_lines)] != expected_native:
            raise ValueError(f"stock messages changed beyond the L-button glyph in {archive}")
        if len(output_lines) != messages.TITLE_USER_HINT_LINE + 4:
            raise ValueError(f"unexpected appended line count in {archive}")
        output_text = messages.message_codec.read_message_lines(
            output_entries[messages.MESSAGE_FILE_INDEX].files[0])
        if tuple(output_text[77:80])!=messages.storage_notice_lines(archive):
            raise ValueError(f"SD notice translations differ in {archive}")
        for line, expected in ((messages.TITLE_OFFLINE_LINE, messages.TITLE_OFFLINE[archive]),
                               (messages.TITLE_ONLINE_LINE, messages.TITLE_ONLINE[archive]),
                               (messages.TITLE_SWITCH_LINE, messages.TITLE_SWITCH_MESSAGES[archive]),
                               (messages.TITLE_USER_LABEL_LINE, messages.USER_LABEL_MESSAGES[archive]),
                               (messages.TITLE_USER_HINT_LINE, messages.USER_SWITCH_HINTS[archive])):
            if output_text[line] != expected or len(expected.splitlines()) != 1:
                raise ValueError(f"independent single-line title hint mismatch in {archive}")
        if messages.TITLE_SWITCH_LINE!=73 or hasattr(messages,"TRANSACTION_MESSAGES") or \
                hasattr(messages,"TRANSACTION_MESSAGE_BASE"):
            raise ValueError(f"unused recovery messages or stale R-hint slot in {archive}")
        if messages.MISSING_BANK_DISCONNECT_LINE!=74 or \
                output_text[74]!=messages.MISSING_BANK_DISCONNECT_MESSAGES[archive] or \
                len(output_text[74].splitlines())!=3 or \
                output_text[74].splitlines()[-1]!=messages.DISCONNECT_MESSAGES[archive] or \
                max(len(line.encode("utf-16le"))//2 for line in output_text[74].splitlines())>48 or \
                output_lines[74][1]!=source_lines[messages.STOCK_DISCONNECT_LINE][1]:
            raise ValueError(f"missing-Bank disconnect text/flags invalid in {archive}")
        source_titles = messages.message_codec.read_message_values(
            source_entries[messages.GAME_TITLE_FILE_INDEX].files[0]
        )
        output_titles = messages.message_codec.read_message_values(
            output_entries[messages.GAME_TITLE_FILE_INDEX].files[0]
        )
        if len(source_titles) != messages.GAME_TITLE_MESSAGE_BASE or \
                output_titles != source_titles + list(game_titles):
            raise ValueError(f"ROM-language game titles differ from stock resources in {archive}")
        if len(source_entries) != len(output_entries):
            raise ValueError(f"archive entry count changed in {archive}")
        for index, (source_entry, output_entry) in enumerate(zip(source_entries, output_entries)):
            if source_entry.flags != output_entry.flags or \
                    source_entry.files.keys() != output_entry.files.keys():
                raise ValueError(f"archive entry layout changed in {archive}, entry {index}")
            for subindex, source_file in source_entry.files.items():
                if subindex == 0 and index in (messages.MESSAGE_FILE_INDEX,
                                               messages.GAME_TITLE_FILE_INDEX):
                    continue
                if output_entry.files[subindex] != source_file:
                    raise ValueError(f"unrelated resource changed in {archive}, entry {index}")

    print("combined Mover patch verification passed")


if __name__ == "__main__":
    main()
