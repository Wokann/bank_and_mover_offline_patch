#!/usr/bin/env python3
"""Statically verify the combined Poke Mover patch.

静态校验 Poke Mover 合并补丁。
"""

from __future__ import annotations

import argparse
import hashlib
import importlib.util
import sys
from pathlib import Path


IMAGE_BASE = 0x00100000
EXPECTED_BASE_SHA256 = "001C20ADA74016507C969BB44A0A50F8EDF803EC06A3FC46834263BA8DF0FD2F"
CAVE_START = 0x00261C74
CAVE_END = 0x00262C10
TEXT_ACTUAL_END = 0x0028D1AC
TEXT_MAPPED_END = 0x0028E000
LAYEREDFS_RESERVED_SIZE = 0x130
VERSION_STORAGE_SIZE = 0x40
VERSION_STORAGE_START = TEXT_MAPPED_END - VERSION_STORAGE_SIZE
VERSION_IDENTIFIER = b"offline_patch_v1.0.0\0"
PAYLOAD_START = 0x0028D2E0
TRANSPORTER_REDIRECT_AREA_START = 0x0028DD00
TRANSPORTER_REDIRECT_AREA_SIZE = 0xE0
LOCAL_VALIDATION_AREA_START = (
    TRANSPORTER_REDIRECT_AREA_START + TRANSPORTER_REDIRECT_AREA_SIZE
)
PAYLOAD_END = VERSION_STORAGE_START
TRANSPORTER_REDIRECT_HOOK_RANGES = (
    (0x0021A7E0, 0x0021A7E4),
    (0x0021AA0C, 0x0021AA24),
    (0x0021AB50, 0x0021AB68),
)
TICKET_INTERFACES = (
    (0x00249754, "initialize", "ticketjob_initialize", 0x0023D3C0),
    (0x00249774, "poll", "ticketjob_poll", 0x0023E230),
    (0x00249818, "campaignrequest", "movercampaign_request", 0x0023BE4C),
    (0x00249CEC, "unbind", "ticketjob_unbind", 0x0023E63C),
)


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


def branch_target(image: bytes, address: int) -> tuple[int, bool, int]:
    instruction = word(image, address)
    if ((instruction >> 25) & 7) != 5:
        raise ValueError(f"expected branch at {address:08X}, found {instruction:08X}")
    displacement = (instruction & 0xFFFFFF) << 2
    if displacement & 0x02000000:
        displacement -= 0x04000000
    return address + 8 + displacement, bool(instruction & 0x01000000), instruction >> 28


def apply_ips(base: bytes, patch: bytes) -> bytes:
    if not patch.startswith(b"PATCH") or not patch.endswith(b"EOF"):
        raise ValueError("invalid IPS stream")
    output = bytearray(base)
    cursor = 5
    while patch[cursor:cursor + 3] != b"EOF":
        offset = int.from_bytes(patch[cursor:cursor + 3], "big")
        length = int.from_bytes(patch[cursor + 3:cursor + 5], "big")
        cursor += 5
        if length:
            output[offset:offset + length] = patch[cursor:cursor + length]
            cursor += length
        else:
            count = int.from_bytes(patch[cursor:cursor + 2], "big")
            output[offset:offset + count] = bytes([patch[cursor + 2]]) * count
            cursor += 3
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
    parser.add_argument("--ips", required=True, type=Path)
    parser.add_argument("--source-romfs", required=True, type=Path)
    parser.add_argument("--output-romfs", required=True, type=Path)
    args = parser.parse_args()

    base = args.base.read_bytes()
    patched = args.patched.read_bytes()
    digest = hashlib.sha256(base).hexdigest().upper()
    if digest != EXPECTED_BASE_SHA256:
        raise ValueError(f"unsupported base code SHA-256: {digest}")
    if len(base) != len(patched):
        raise ValueError("patched image length differs from base")
    if apply_ips(base, args.ips.read_bytes()) != patched:
        raise ValueError("release IPS does not reproduce the patched image")

    sym = symbols(args.symbols)
    required = {
        "combinepatch_codeusedend", "combinepatch_offlinepayloadusedend",
        "combinepatch_offlinepayloadstart", "textactualend", "textmappedend",
        "fshelpers_payloadbegin", "fshelpers_payloadend",
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
        "offlinepatch_versionidentifier",
    }
    for _, operation, native_name, _ in TICKET_INTERFACES:
        required.update((f"combinepatch_ticket{operation}",
                         f"combinepatch_ticket{operation}local", native_name))
    missing = sorted(required - sym.keys())
    if missing:
        raise ValueError(f"missing armips symbols: {', '.join(missing)}")
    if not CAVE_START < sym["combinepatch_codeusedend"] <= CAVE_END:
        raise ValueError("code-cave payload exceeds the audited range")
    if not (
        sym["textactualend"] == TEXT_ACTUAL_END
        and sym["textmappedend"] == TEXT_MAPPED_END
        and sym["combinepatch_offlinepayloadstart"] == PAYLOAD_START
        and PAYLOAD_START % 0x10 == 0
        and PAYLOAD_START - TEXT_ACTUAL_END >= LAYEREDFS_RESERVED_SIZE
    ):
        raise ValueError("invalid LayeredFS reservation or payload alignment")
    for image in (base, patched):
        if image[TEXT_ACTUAL_END - IMAGE_BASE:PAYLOAD_START - IMAGE_BASE] != bytes(
            PAYLOAD_START - TEXT_ACTUAL_END
        ):
            raise ValueError("Luma LayeredFS reservation is not untouched padding")
    if not (
        PAYLOAD_START < sym["combinepatch_offlinepayloadusedend"]
        <= TRANSPORTER_REDIRECT_AREA_START
    ):
        raise ValueError("offline payload exceeds the executable tail")
    payload_objects = (
        ("fshelpers_payloadbegin", "fshelpers_payloadend"),
        ("localticket_payloadbegin", "localticket_payloadend"),
        ("bankdataredirect_payloadbegin", "bankdataredirect_payloadend"),
        ("offlineflow_payloadbegin", "offlineflow_payloadend"),
        ("patchpaths_payloadbegin", "patchpaths_payloadend"),
    )
    for begin, end in payload_objects:
        if sym[begin] >= sym[end]:
            raise ValueError(f"empty or reversed payload object: {begin}")
    if not (
        CAVE_START < sym["fshelpers_payloadbegin"]
        < sym["fshelpers_payloadend"] == sym["localticket_payloadbegin"]
        < sym["localticket_payloadend"] == sym["combinepatch_codeusedend"]
        <= CAVE_END
    ):
        raise ValueError("filesystem/ticket objects exceed the audited code cave")
    tail_objects = payload_objects[2:]
    if sym[tail_objects[0][0]] < PAYLOAD_START:
        raise ValueError("first tail payload object precedes the executable tail")
    for (_, previous_end), (next_begin, _) in zip(tail_objects, tail_objects[1:]):
        if sym[previous_end] != sym[next_begin]:
            raise ValueError("tail payload objects are not consecutive")
    if sym[tail_objects[-1][1]] != sym["combinepatch_offlinepayloadusedend"]:
        raise ValueError("offline payload end does not follow the final object")
    if not (
        LOCAL_VALIDATION_AREA_START
        <= sym["localvalidation_payloadbegin"]
        < sym["localvalidation_payloadend"]
        <= VERSION_STORAGE_START
    ):
        raise ValueError("local-validation object exceeds its audited tail area")

    # The external Transporter Redirect Patch replaces these three stock hook
    # ranges and injects its SD-save payload at 0x0028DD00. Keep every byte
    # untouched so independently generated IPS patches can be merged.
    # 外部 Transporter Redirect Patch 会覆盖这三处原版钩子区域，并从
    # 0x0028DD00 注入 SD 存档载荷。所有相关字节都必须保持不变，以便
    # 独立生成的 IPS 补丁可以合并。
    redirect_ranges = TRANSPORTER_REDIRECT_HOOK_RANGES + (
        (
            TRANSPORTER_REDIRECT_AREA_START,
            TRANSPORTER_REDIRECT_AREA_START + TRANSPORTER_REDIRECT_AREA_SIZE,
        ),
    )
    for start, end in redirect_ranges:
        start_offset = start - IMAGE_BASE
        end_offset = end - IMAGE_BASE
        if patched[start_offset:end_offset] != base[start_offset:end_offset]:
            raise ValueError(
                f"Transporter Redirect Patch range was modified: {start:08X}-{end:08X}"
            )
    redirect_payload_offset = TRANSPORTER_REDIRECT_AREA_START - IMAGE_BASE
    if base[
        redirect_payload_offset:
        redirect_payload_offset + TRANSPORTER_REDIRECT_AREA_SIZE
    ] != bytes(TRANSPORTER_REDIRECT_AREA_SIZE):
        raise ValueError("Transporter Redirect Patch reservation is not stock padding")

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
    }
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
    if not CAVE_START <= policy_address < wrapper_start < wrapper_end <= sym["fshelpers_payloadbegin"]:
        raise ValueError("ticket policy/wrappers exceed the audited code cave")
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
            (wrapper + 0x0C, wrapper + (0x24 if wrapper_name ==
                "combinepatch_gen5validation" else 0x1C), False, 0x1),
            (wrapper + 0x14, sym[helper_name], True, 0xE),
            (wrapper + (0x20 if wrapper_name == "combinepatch_gen5validation"
                else 0x18), local_continuation, False, 0xE),
            (wrapper + (0x28 if wrapper_name == "combinepatch_gen5validation"
                else 0x20), original_continuation, False, 0xE),
        )
        for address, target, link, condition in expected_branches:
            actual_target, actual_link, actual_condition = branch_target(patched, address)
            if (actual_target, actual_link, actual_condition) != (target, link, condition):
                raise ValueError(f"incorrect validation route at {address:08X}")
        if wrapper_name == "combinepatch_gen5validation":
            if word(patched, wrapper + 0x18) != 0xE3500000:
                raise ValueError("Gen 5 validation does not test preparation failure")
            if branch_target(patched, wrapper + 0x1C) != (0x00246368, False, 0x0):
                raise ValueError("Gen 5 source failure does not return through stock cleanup")

    # Keep the native local checks, conversion, and result consumer unchanged.
    # 保持原版本地检查、转换器和逐槽结果消费代码不变。
    for start, end in ((0x00241574, 0x0024212C), (0x00242F8C, 0x00243448),
                       (0x00245800, 0x002460B8), (0x002461C8, 0x00246A2C)):
        if patched[start - IMAGE_BASE:end - IMAGE_BASE] != base[start - IMAGE_BASE:end - IMAGE_BASE]:
            raise ValueError(f"native validation/conversion range changed: {start:08X}-{end:08X}")

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
