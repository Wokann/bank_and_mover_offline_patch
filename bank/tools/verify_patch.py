"""Statically verify the built combined Bank patch and its LayeredFS resources.

静态校验已构建的 Bank 合并补丁及其 LayeredFS 资源。
"""

from __future__ import annotations

import argparse
import hashlib
import importlib.util
import sys
import zlib
from pathlib import Path


IMAGE_BASE = 0x00100000
EXPECTED_BASE_SHA256 = "2DCE4796F54807CF8A67F1CE6297BF472D969B30ED7A7E8E25C2A6C2BDC40ABF"
OPTIONAL_REWARD_STATE = 0x002B0270
OPTIONAL_REWARD_BODY_END = 0x002B1AD0
TICKET_JOB_CALLS = (
    (0x002B0444, "combinepatch_ticketinitialize", "ticketjob_initialize"),
    (0x002B0464, "combinepatch_ticketpoll", "ticketjob_poll"),
    (0x002B1994, "combinepatch_ticketunbind", "ticketjob_unbind"),
)
RESTORED_HOME_BOX_START = 0x00285BA8
RESTORED_HOME_BOX_END = 0x0028711C
RESTORED_HOME_START = 0x002A7BF0
RESTORED_HOME_END = 0x002A8760
TEXT_ACTUAL_END = 0x00313910
EXPANDED_PAYLOAD_START = 0x003FC000
EXPANDED_PAYLOAD_SIZE = 0x3000
DATA_ACTUAL_END = 0x003ABACC
BSS_ACTUAL_END = 0x003FA904
READ_WRITE_MAPPED_END = 0x003FB000
RUNTIME_STORAGE_START = READ_WRITE_MAPPED_END - 0x10
TAIL_CAVE_START = 0x00313A40
TEXT_MAPPED_END = 0x00314000
VERSION_STORAGE_SIZE = 0x40
VERSION_STORAGE_START = TEXT_MAPPED_END - VERSION_STORAGE_SIZE
TAIL_CAVE_END = VERSION_STORAGE_START
VERSION_IDENTIFIER = b"offline_patch_v1.0.0\0"
ARM_COND_EQ = 0x0
ARM_COND_NE = 0x1
ARM_COND_AL = 0xE
ARM_NOP = 0xE1A00000
BANK_ROOT_POINTER_SLOT = 0x003AB938


def load_module(name: str, path: Path):
    spec = importlib.util.spec_from_file_location(name, path)
    if spec is None or spec.loader is None:
        raise RuntimeError(f"cannot load module: {path}")
    module = importlib.util.module_from_spec(spec)
    sys.modules[name] = module
    spec.loader.exec_module(module)
    return module


def read_symbols(path: Path) -> dict[str, int]:
    symbols: dict[str, int] = {}
    for line in path.read_text(encoding="ascii").splitlines():
        fields = line.split()
        if len(fields) != 2:
            continue
        symbols[fields[1].lower()] = int(fields[0], 16)
    return symbols


def image_offset(address: int) -> int:
    return address - IMAGE_BASE


def read_word(image: bytes, address: int) -> int:
    offset = image_offset(address)
    if offset < 0 or offset + 4 > len(image):
        raise ValueError(f"address outside image: {address:08X}")
    return int.from_bytes(image[offset : offset + 4], "little")


def decode_arm_branch(image: bytes, address: int) -> tuple[int, bool, int]:
    """Return an ARM branch's target, link bit, and condition code.

    返回 ARM 分支的目标、链接位和条件码。
    """
    word = read_word(image, address)
    if ((word >> 25) & 0x7) != 0x5:
        raise ValueError(f"expected ARM B/BL at {address:08X}, found {word:08X}")
    displacement = (word & 0x00FFFFFF) << 2
    if displacement & 0x02000000:
        displacement -= 0x04000000
    return address + 8 + displacement, bool(word & 0x01000000), word >> 28


def expect_branch(
    image: bytes,
    address: int,
    target: int,
    link: bool,
    condition: int,
    name: str,
) -> None:
    """Require one exact ARM B/BL instruction at a patched hook site.

    要求补丁钩子位置存在一条精确的 ARM B/BL 指令。
    """
    actual_target, actual_link, actual_condition = decode_arm_branch(image, address)
    if actual_target != target or actual_link != link or actual_condition != condition:
        expected_kind = "BL" if link else "B"
        actual_kind = "BL" if actual_link else "B"
        raise ValueError(
            f"{name}: expected cond={condition:X} {expected_kind} {target:08X}, "
            f"found cond={actual_condition:X} {actual_kind} {actual_target:08X}"
        )


def expect_word(image: bytes, address: int, value: int, name: str) -> None:
    """Require one exact overwritten ARM word.

    要求一个被覆盖的 ARM 指令字保持精确值。
    """
    actual = read_word(image, address)
    if actual != value:
        raise ValueError(f"{name}: expected {value:08X}, found {actual:08X}")


def expect_bytes(image: bytes, address: int, value: bytes, name: str) -> None:
    """Require one exact byte sequence at a mapped image address.

    要求映射镜像地址处存在一段精确字节序列。
    """
    offset = image_offset(address)
    if offset < 0 or offset + len(value) > len(image):
        raise ValueError(f"{name}: address outside image")
    if image[offset : offset + len(value)] != value:
        raise ValueError(f"{name}: byte sequence mismatch")


def expect_ldr_r0_literal(image: bytes, address: int, value: int, name: str) -> None:
    """Require an ARM ``ldr r0, [pc, #imm]`` that resolves to one fixed value.

    要求 ARM 的 ``ldr r0, [pc, #imm]`` 解析为一个固定值。
    """
    word = read_word(image, address)
    if (word & 0x0FFF0000) != 0x059F0000 or ((word >> 12) & 0xF) != 0:
        raise ValueError(f"{name}: expected ARM ldr r0 literal, found {word:08X}")
    literal_address = address + 8 + (word & 0xFFF)
    expect_word(image, literal_address, value, name)


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


def verify_expansion(base: bytes, image: bytes, symbols: dict[str, int],
                     base_exheader: Path, exheader: Path) -> None:
    """Check the loader, fixed native addresses and restored function bodies."""
    from tools.prepare_expanded_code import prepare_images

    _, expected_header, _ = prepare_images(
        base, base_exheader.read_bytes(), EXPANDED_PAYLOAD_START, EXPANDED_PAYLOAD_SIZE
    )
    if exheader.read_bytes() != expected_header:
        raise ValueError("release exheader differs from the exact expanded layout/capabilities")
    payload_end = EXPANDED_PAYLOAD_START + EXPANDED_PAYLOAD_SIZE
    used_end = symbols["combinepatch_codeusedend"]
    if not EXPANDED_PAYLOAD_START < used_end <= payload_end:
        raise ValueError("payload exceeds the added executable pages")
    if (symbols["codeexpansion_payloadstart"] != EXPANDED_PAYLOAD_START or
            symbols["code_expansion_payload_size"] != EXPANDED_PAYLOAD_SIZE):
        raise ValueError("assembly and exheader payload reservations differ")
    if image[len(base):image_offset(EXPANDED_PAYLOAD_START)] != bytes(
            EXPANDED_PAYLOAD_START - IMAGE_BASE - len(base)):
        raise ValueError("original BSS and pre-payload padding are not zero-filled")
    if any(image[image_offset(used_end):image_offset(payload_end)]):
        raise ValueError("unused payload padding is not zero-filled")
    if (symbols["offlinepatch_runtimestoragestart"] != RUNTIME_STORAGE_START or
            symbols["offlinepatch_runtimestorageend"] != READ_WRITE_MAPPED_END or
            symbols["combinepatch_modestorage"] != RUNTIME_STORAGE_START or
            not BSS_ACTUAL_END <= RUNTIME_STORAGE_START < READ_WRITE_MAPPED_END):
        raise ValueError("patch runtime storage overlaps native logical BSS")
    if image[image_offset(TEXT_MAPPED_END):len(base)] != base[image_offset(TEXT_MAPPED_END):]:
        raise ValueError("native rodata/data contents changed")
    for start, end, name in (
        (RESTORED_HOME_BOX_START, RESTORED_HOME_BOX_END, "HOME box-selection UI"),
        (RESTORED_HOME_START, RESTORED_HOME_END, "HOME/eShop states"),
        (0x00100024, 0x00100048, "native BSS clear"),
        (0x00102BA4, 0x00102BC8, "native constructor walker"),
    ):
        if image[image_offset(start):image_offset(end)] != base[image_offset(start):image_offset(end)]:
            raise ValueError(f"{name} no longer matches the original image")
    expect_word(base, 0x00100040, DATA_ACTUAL_END, "native BSS start")
    expect_word(base, 0x00100044, BSS_ACTUAL_END, "native BSS end")
    table_start = 0x00102BAC + read_word(base, 0x00102BC0)
    table_end = 0x00102BB0 + read_word(base, 0x00102BC4)
    if (table_start, table_end) != (0x00369050, 0x00369370):
        raise ValueError("native PREL32 constructor table moved")
    if image[image_offset(table_start):image_offset(table_end)] != base[
            image_offset(table_start):image_offset(table_end)]:
        raise ValueError("native PREL32 constructor entries changed")
    for current in (base, image):
        if any(current[image_offset(TEXT_ACTUAL_END):image_offset(TAIL_CAVE_START)]):
            raise ValueError("Luma LayeredFS reservation changed")

    startup = symbols["codeexpansion_startup"]
    loader_begin = symbols["codeexpansion_loaderbegin"]
    loader_end = symbols["codeexpansion_loaderend"]
    if not (startup == TAIL_CAVE_START < loader_begin <=
            symbols["codeexpansion_enable"] < loader_end <= TAIL_CAVE_END):
        raise ValueError("startup loader exceeds original executable padding")
    expect_word(base, 0x00100010, 0xFA000AE3, "original constructor call")
    if (symbols["bankstartup_constructorcall"] != 0x00100010 or
            symbols["bankstartup_runconstructors"] != 0x00102BA4):
        raise ValueError("startup hook/resume addresses differ")
    expect_branch(image, 0x00100010, startup, True, ARM_COND_AL, "expansion startup hook")
    expect_word(image, startup, 0xE92D5FFF, "startup register preservation")
    expect_word(image, startup + 4, 0xE3A009FF, "added-page address")
    expect_word(image, startup + 8, 0xE3A01A03, "added-page size")
    expect_branch(image, startup + 12, symbols["codeexpansion_enable"],
                  True, ARM_COND_AL, "enable added pages")
    expect_word(image, startup + 16, 0xE3500000, "loader result check")
    expect_branch(image, startup + 20, startup + 32, False, ARM_COND_NE, "loader failure")
    expect_word(image, startup + 24, 0xE8BD5FFF, "startup register restoration")
    instruction = read_word(image, startup + 28)
    if instruction & 0xFFFFF000 != 0xE59FF000:
        raise ValueError("startup does not tail-resume the native Thumb constructor walker")
    expect_word(image, startup + 36 + (instruction & 0xFFF), 0x00102BA5,
                "Thumb constructor resume target")
    expect_word(image, startup + 36, 0xEF00003C, "fail-closed SVC Break")
    expect_branch(image, startup + 40, startup + 32, False, ARM_COND_AL, "fail-closed loop")
    for svc in (0x23, 0x27, 0x2A, 0x70):
        if (0xEF000000 | svc).to_bytes(4, "little") not in image[
                image_offset(loader_begin):image_offset(loader_end)]:
            raise ValueError(f"startup loader lacks SVC 0x{svc:02X}")
    if any(image[image_offset(loader_end):image_offset(TAIL_CAVE_END)]):
        raise ValueError("unused executable tail padding changed")


def verify_code(base: Path, patched: Path, symbols_path: Path, bps: Path,
                base_exheader: Path, exheader: Path) -> None:
    image = patched.read_bytes()
    base_image = base.read_bytes()
    base_hash = hashlib.sha256(base_image).hexdigest().upper()
    if base_hash != EXPECTED_BASE_SHA256:
        raise ValueError(
            "base code SHA-256 does not match the supported Pokemon Bank v1.5 image: "
            f"{base_hash}"
        )
    if len(image) != EXPANDED_PAYLOAD_START + EXPANDED_PAYLOAD_SIZE - IMAGE_BASE:
        raise ValueError("patched code image length differs from the expanded mapping")
    symbols = read_symbols(symbols_path)
    required = {
        "combinepatch_codeusedend",
        "combinepatch_modestorage",
        "offlinepatch_runtimestoragestart",
        "offlinepatch_runtimestorageend",
        "codeexpansion_payloadstart",
        "code_expansion_payload_size",
        "codeexpansion_startup",
        "codeexpansion_enable",
        "codeexpansion_loaderbegin",
        "codeexpansion_loaderend",
        "bankstartup_constructorcall",
        "bankstartup_runconstructors",
        "combinepatch_networkavailability",
        "combinepatch_titlemodetextinitialize",
        "combinepatch_titlescreenupdate",
        "combinepatch_titleprocessmodetoggle",
        "combinepatch_titlepreview",
        "combinepatch_titlebindsession",
        "combinepatch_titleresult",
        "combinepatch_titleorresume",
        "combinepatch_turtleload",
        "combinepatch_turtlesave",
        "combinepatch_turtlecheck",
        "combinepatch_turtleformat",
        "combinepatch_turtleformatpoll",
        "combinepatch_nogamechoice",
        "combinepatch_selectnogamemessage",
        "combinepatch_selectnogamedownloadbutton",
        "turtleredirect_preparesession",
        "combinepatch_networkskipremotejob",
        "combinepatch_networkskipremotejobofficial",
        "combinepatch_payloadbegin",
        "combinepatch_payloadend",
        "fshelpers_payloadbegin",
        "fshelpers_payloadend",
        "bankdataredirect_payloadbegin",
        "bankdataredirect_payloadend",
        "offlineflow_payloadbegin",
        "offlineflow_payloadend",
        "localmileage_payloadbegin",
        "localmileage_payloadend",
        "localticket_payloadbegin",
        "localticket_payloadend",
        "unlockmode_payloadbegin",
        "unlockmode_payloadend",
        "unlockmode_trygetfirstcandidatecode",
        "unlockmode_postselectionupdate",
        "unlockmode_recoveryupdate",
        "unlockmode_recoverystorage",
        "bankui_showmessageline",
        "ui_setpanevisible",
        "ui_applyanimation",
        "messageview_getresult",
        "combinepatch_networkupdate",
        "combinepatch_initialremoterecordupdate",
        "combinepatch_ticketinitialize",
        "localticket_initializeselected",
        "localticket_backendstorage",
        "srv_getservicehandle",
        "codeexpansion_isazahar",
        "combinepatch_ticketpoll",
        "combinepatch_ticketpolllocal",
        "combinepatch_ticketunbind",
        "combinepatch_ticketunbindlocal",
        "combinepatch_ticketwrappersend",
        "ticketjob_initialize",
        "ticketjob_poll",
        "ticketjob_unbind",
        "online_ticket_check_bypass",
        "combinepatch_onlineticketfallback",
        "combinepatch_firstpresentdispatch",
        "combinepatch_bankdatasyncdispatch",
        "combinepatch_bankdatasyncturtlestate0",
        "combinepatch_bankdatasyncinitialize",
        "combinepatch_selectinitialconnectionmessage",
        "combinepatch_selectsavemessage",
        "combinepatch_selectpostselectionconnectionmessage",
        "combinepatch_postselectionconnectionupdate",
        "combinepatch_postselectionconnectionupdateofficial",
        "combinepatch_transactionrecoveryupdate",
        "combinepatch_transactionrecoveryupdateofficial",
        "combinepatch_recoveryresult",
        "combinepatch_forcerollbacksuccess",
        "combinepatch_disconnectupdate",
        "combinepatch_disconnectskipremotejob",
        "combinepatch_disconnectskipremotejobofficial",
        "combinepatch_bankcreatestate0",
        "combinepatch_bankcreatestate0official",
        "combinepatch_bankcreateaftermessages",
        "combinepatch_bankcreateaftermessagesofficial",
        "combinepatch_createinitial",
        "combinepatch_bankcreatesuccess",
        "combinepatch_selectpostselectionstate",
        "combinepatch_rewardresult",
        "combinepatch_homeresult",
        "combinepatch_selectusebankstate",
        "combinepatch_homecleanupresult",
        "combinepatch_result21",
        "combinepatch_result5",
        "combinepatch_result6or12",
        "combinepatch_savebegin",
        "combinepatch_savedisplaywait",
        "combinepatch_saveturtlestate2",
        "combinepatch_saveturtlestate1",
        "combinepatch_savestage",
        "combinepatch_savecommit",
        "combinepatch_savecommitwait",
        "combinepatch_saverollback",
        "combinepatch_saverollbackwait",
        "combinepatch_menuselectioncallback",
        "combinepatch_menuselectiondisabled",
        "combinepatch_downloadcapturetrampoline",
        "combinepatch_redirecthometolanguage",
        "combinepatch_showmodegreeting",
        "combinepatch_selectgameselectionmessage",
        "combinepatch_showunlockprompt",
        "combinepatch_selectusebankmenutext",
        "combinepatch_selectsupportmenutext",
        "combinepatch_selectinstalledmovermenutext",
        "combinepatch_selectdownloadmovermenutext",
        "combinepatch_selecthomemenutextr6",
        "combinepatch_selecthomemenutextr5",
        "combinepatch_selectdisconnectmessage",
        "combinepatch_disconnectwithlanguagesave",
        "combinepatch_bankdatasyncentry",
        "offlinepatch_networkupdate",
        "offlinepatch_postselectionconnectionupdate",
        "offlinepatch_disconnectupdate",
        "offlinepatch_initialremoterecordupdate",
        "localticket_initialize",
        "localticket_poll",
        "localmileage_getcurrentdate",
        "offlinepatch_loadbankdata",
        "offlinepatch_savedisplaydelayupdate",
        "offlinepatch_createinitial",
        "bankdataredirect_capturedownloaded",
        "combinepatch_selectbankslotdate",
        "bankslot_getcurrentdate",
        "bankslot_writedatecall",
        "bankslot_cleardatecall",
        "bankslots_movedatecall",
        "offlinepatch_stage",
        "offlinepatch_commit",
        "offlinepatch_rollback",
        "allocator_allocate",
        "ui_setlayervisible",
        "ui_setmessageline",
        "bankui_setmessageline",
        "waitingsound_stop",
        "offlinepatch_versionidentifier",
        "turtleredirect_payloadbegin",
        "turtleredirect_payloadend",
        "turtleredirect_loadbackend",
        "turtleredirect_savebackend",
        "turtleredirect_checkbackend",
        "turtleredirect_formatbackend",
        "turtleredirect_formatpoll",
        "turtlesavepath",
        "turtletemppath",
        "combinepatch_pathdatabegin",
        "combinepatch_pathdataend",
        "openarchive",
        "closearchive",
        "pathcommand",
        "renamepath",
        "setsize",
        "openfile",
        "resultisnotfound",
        "deletefile",
        "renamefile",
        "getfilesize",
        "readcompletefile",
        "writecompletefile",
        "emptypath",
        "directory3ds",
        "directorybank",
        "bankpath",
        "temppath",
        "backuppath",
        "brokenbankpath",
        "brokenbackuppath",
        "turtlestorage_formatpoll",
        "turtlestorage_loadatonce",
        "turtlestorage_saveatonce",
        "turtlestorage_formatstart",
        "turtlestorage_checkarchivestatus",
    }
    missing = sorted(required - symbols.keys())
    if missing:
        raise ValueError(f"missing armips symbols: {', '.join(missing)}")

    verify_expansion(base_image, image, symbols, base_exheader, exheader)

    # Replace only the three slot-date calls. Native slot data, metadata,
    # timestamp conversion, network clock and mutex helpers remain unchanged.
    # 仅替换三处槽日期调用；保留槽数据、元数据、时间戳转换及联网时钟与锁函数。
    slot_date = symbols["combinepatch_selectbankslotdate"]
    date_calls = (
        (0x001D86E0, 0x001D88CC, "bankslot_writedatecall"),
        (0x001D95C8, 0x001D9730, "bankslot_cleardatecall"),
        (0x002B9C94, 0x002BA124, "bankslots_movedatecall"),
    )
    for start, end, name in date_calls:
        call = symbols[name]
        expect_branch(base_image, call, symbols["bankslot_getcurrentdate"],
                      True, ARM_COND_AL, f"native {name}")
        expect_branch(image, call, slot_date, True, ARM_COND_AL, name)
        offset, limit = image_offset(start), image_offset(end)
        body = bytearray(image[offset:limit])
        position = image_offset(call) - offset
        body[position:position + 4] = base_image[
            image_offset(call):image_offset(call) + 4]
        if body != base_image[offset:limit]:
            raise ValueError(f"{name}: slot operation changed outside its date call")
    for start, end in ((0x001D3BF4, 0x001D3D24), (0x002295E4, 0x002296B4),
                       (0x0022E3E0, 0x0022E4E4), (0x002BA148, 0x002BA30C)):
        first, last = image_offset(start), image_offset(end)
        if image[first:last] != base_image[first:last]:
            raise ValueError(f"native date/context/slot-swap body changed at {start:08X}")
    expect_word(image, slot_date, 0xE59FC010, "slot-date mode-pointer load")
    expect_word(image, slot_date + 4, 0xE5DCC000, "slot-date session-mode load")
    expect_word(image, slot_date + 8, 0xE35C0000, "slot-date Offline-only test")
    expect_branch(image, slot_date + 12, symbols["bankslot_getcurrentdate"],
                  False, ARM_COND_NE, "native slot date in other modes")
    expect_word(image, slot_date + 16, 0xE1A00001, "local slot-date output argument")
    expect_branch(image, slot_date + 20, symbols["localmileage_getcurrentdate"],
                  False, ARM_COND_AL, "shared console date for Offline slots")
    expect_word(image, slot_date + 24, symbols["combinepatch_modestorage"],
                "slot-date session-mode pointer")

    # Keep the marker at a stable address and reserve the complete zero-padded
    # block for future compatibility metadata.
    # 将标识固定在稳定地址，并为未来兼容性元数据保留完整的零填充区域。
    if symbols["offlinepatch_versionidentifier"] != VERSION_STORAGE_START:
        raise ValueError("offline patch version identifier moved")
    version_offset = image_offset(VERSION_STORAGE_START)
    expected_version_storage = VERSION_IDENTIFIER.ljust(VERSION_STORAGE_SIZE, b"\0")
    if image[version_offset : version_offset + VERSION_STORAGE_SIZE] != expected_version_storage:
        raise ValueError("offline patch version storage is malformed")
    if base_image[version_offset : version_offset + VERSION_STORAGE_SIZE] != bytes(VERSION_STORAGE_SIZE):
        raise ValueError("offline patch version storage no longer uses original padding")
    if VERSION_STORAGE_START + VERSION_STORAGE_SIZE != TEXT_MAPPED_END:
        raise ValueError("offline patch version storage no longer ends with mapped text")

    payload_begin = symbols["combinepatch_payloadbegin"]
    payload_end = symbols["combinepatch_payloadend"]
    if not EXPANDED_PAYLOAD_START <= payload_begin < payload_end <= symbols["combinepatch_codeusedend"]:
        raise ValueError("local payload exceeds the added executable pages")
    # Only the three job calls may differ; native allocation, construction,
    # result handling, state transitions and destruction must remain intact.
    # 仅允许三个作业调用点变化；保留原版分配、构造、结果处理、状态推进与析构。
    cursor = image_offset(OPTIONAL_REWARD_STATE)
    for address, wrapper, native in TICKET_JOB_CALLS:
        end = image_offset(address)
        if image[cursor:end] != base_image[cursor:end]:
            raise ValueError("native entitlement flow changed outside a job call")
        expect_branch(base_image, address, symbols[native], True, ARM_COND_AL,
                      f"base {native} call")
        expect_branch(image, address, symbols[wrapper], True, ARM_COND_AL,
                      f"redirected {native} call")
        cursor = end + 4
    end = image_offset(OPTIONAL_REWARD_BODY_END)
    if image[cursor:end] != base_image[cursor:end]:
        raise ValueError("native entitlement companion functions were modified")
    job_start, job_end = image_offset(0x002A04C0), image_offset(0x002A2504)
    if image[job_start:job_end] != base_image[job_start:job_end]:
        raise ValueError("native ticket job implementation was modified")
    expect_word(
        image,
        RESTORED_HOME_BOX_END,
        read_word(base_image, RESTORED_HOME_BOX_END),
        "UI function after the restored HOME box-selection region",
    )
    if not (
        payload_begin
        == symbols["fshelpers_payloadbegin"]
        < symbols["fshelpers_payloadend"]
        == symbols["bankdataredirect_payloadbegin"]
        < symbols["bankdataredirect_payloadend"]
        == symbols["offlineflow_payloadbegin"]
        < symbols["offlineflow_payloadend"]
        == symbols["localmileage_payloadbegin"]
        < symbols["localmileage_payloadend"]
        == symbols["localticket_payloadbegin"]
        < symbols["localticket_payloadend"]
        == symbols["unlockmode_payloadbegin"]
        < symbols["unlockmode_payloadend"]
        == payload_end
    ):
        raise ValueError("imported local-file payload objects are not contiguous or ordered")

    business_wrapper_symbols = (
        "combinepatch_selectbankslotdate",
        "combinepatch_networkavailability",
        "combinepatch_titlemodetextinitialize",
        "combinepatch_titlescreenupdate",
        "combinepatch_titleprocessmodetoggle",
        "combinepatch_networkskipremotejob",
        "combinepatch_networkskipremotejobofficial",
        "combinepatch_networkupdate",
        "combinepatch_initialremoterecordupdate",
        "combinepatch_firstpresentdispatch",
        "combinepatch_bankdatasyncdispatch",
        "combinepatch_bankdatasyncinitialize",
        "combinepatch_selectinitialconnectionmessage",
        "combinepatch_selectsavemessage",
        "combinepatch_selectpostselectionconnectionmessage",
        "combinepatch_postselectionconnectionupdate",
        "combinepatch_postselectionconnectionupdateofficial",
        "combinepatch_transactionrecoveryupdate",
        "combinepatch_transactionrecoveryupdateofficial",
        "combinepatch_recoveryresult",
        "combinepatch_forcerollbacksuccess",
        "unlockmode_postselectionupdate",
        "unlockmode_recoveryupdate",
        "combinepatch_disconnectupdate",
        "combinepatch_disconnectskipremotejob",
        "combinepatch_disconnectskipremotejobofficial",
        "combinepatch_bankcreatestate0",
        "combinepatch_bankcreatestate0official",
        "combinepatch_bankcreateaftermessages",
        "combinepatch_bankcreateaftermessagesofficial",
        "combinepatch_createinitial",
        "combinepatch_bankcreatesuccess",
        "combinepatch_selectpostselectionstate",
        "combinepatch_rewardresult",
        "combinepatch_homeresult",
        "combinepatch_selectusebankstate",
        "combinepatch_homecleanupresult",
        "combinepatch_result21",
        "combinepatch_result5",
        "combinepatch_result6or12",
        "combinepatch_savebegin",
        "combinepatch_savedisplaywait",
        "combinepatch_savestage",
        "combinepatch_savecommit",
        "combinepatch_savecommitwait",
        "combinepatch_saverollback",
        "combinepatch_saverollbackwait",
        "combinepatch_menuselectioncallback",
        "combinepatch_menuselectiondisabled",
        "combinepatch_downloadcapturetrampoline",
        "turtleredirect_loadbackend",
        "turtleredirect_savebackend",
        "turtleredirect_checkbackend",
        "turtleredirect_formatbackend",
        "turtleredirect_formatpoll",
    )
    for name in business_wrapper_symbols:
        address = symbols[name]
        if not EXPANDED_PAYLOAD_START <= address < symbols["combinepatch_codeusedend"]:
            raise ValueError(f"{name} is outside the added executable pages")
    if not (
        EXPANDED_PAYLOAD_START
        <= symbols["turtleredirect_payloadbegin"]
        < symbols["turtleredirect_payloadend"]
        <= symbols["combinepatch_codeusedend"]
    ):
        raise ValueError("Turtle redirect payload exceeds the added executable pages")

    metadata_wrapper_symbols = (
        "combinepatch_titlereleasewaitingview",
        "combinepatch_redirecthometolanguage",
        "combinepatch_showmodegreeting",
        "combinepatch_selectgameselectionmessage",
        "combinepatch_showunlockprompt",
        "combinepatch_selectusebankmenutext",
        "combinepatch_selectdisconnectmessage",
        "combinepatch_selectnogamedownloadbutton",
        "combinepatch_disconnectwithlanguagesave",
        "combinepatch_bankdatasyncentry",
        "combinepatch_pathdatabegin",
        "combinepatch_pathdataend",
        "emptypath",
        "directory3ds",
        "directorybank",
        "bankpath",
        "temppath",
        "backuppath",
        "brokenbankpath",
        "brokenbackuppath",
        "turtlesavepath",
        "turtletemppath",
    )
    for name in metadata_wrapper_symbols:
        address = symbols[name]
        if not EXPANDED_PAYLOAD_START <= address < symbols["combinepatch_codeusedend"]:
            raise ValueError(f"{name} is outside the added executable pages")

    for name in (
        "offlinepatch_networkupdate",
        "offlinepatch_postselectionconnectionupdate",
        "offlinepatch_disconnectupdate",
        "offlinepatch_initialremoterecordupdate",
        "localticket_initialize",
        "localticket_poll",
        "localmileage_getcurrentdate",
        "offlinepatch_loadbankdata",
        "offlinepatch_savedisplaydelayupdate",
        "offlinepatch_createinitial",
        "bankdataredirect_capturedownloaded",
        "offlinepatch_stage",
        "offlinepatch_commit",
        "offlinepatch_rollback",
        "openarchive",
        "closearchive",
        "pathcommand",
        "renamepath",
        "setsize",
        "openfile",
        "resultisnotfound",
        "deletefile",
        "renamefile",
        "getfilesize",
        "readcompletefile",
        "writecompletefile",
        "unlockmode_trygetfirstcandidatecode",
    ):
        address = symbols[name]
        if not payload_begin <= address < payload_end:
            raise ValueError(f"{name} is outside the imported local-file payload")

    for name, value in {
        "turtlesavepath": b"/3ds/Bank/sav.bin\0",
        "turtletemppath": b"/3ds/Bank/sav.tmp\0",
        "emptypath": b"\0",
        "directory3ds": b"/3ds\0",
        "directorybank": b"/3ds/Bank\0",
        "bankpath": b"/3ds/Bank/bankdata.bin\0",
        "temppath": b"/3ds/Bank/bankdata.tmp\0",
        "backuppath": b"/3ds/Bank/bankdata.bak\0",
        "brokenbankpath": b"/3ds/Bank/bankdata.bin.break\0",
        "brokenbackuppath": b"/3ds/Bank/bankdata.bak.break\0",
    }.items():
        expect_bytes(image, symbols[name], value, name)

    hook_branches = (
        (0x002B1AD0, "combinepatch_titlescreenupdate", False, ARM_COND_AL, "title input and session latch"),
        (0x002B4974, "combinepatch_titlemodetextinitialize", True, ARM_COND_AL, "title mode text"),
        (0x002B1C2C, "combinepatch_titlepreview", False, ARM_COND_AL, "title SD preview"),
        (0x002B1B98, "combinepatch_titlereleasewaitingview", False, ARM_COND_AL, "release cached prompt UI before language reload"),
        (0x002B1BDC, "combinepatch_titlebindsession", False, ARM_COND_AL, "session record reload after title teardown"),
        (0x002A5680, "combinepatch_titleresult", False, ARM_COND_EQ, "selected backend language check"),
        (0x002A5648, "combinepatch_titleorresume", False, ARM_COND_AL, "resume selected mode after first language save"),
        (0x002AF1FC, "combinepatch_networkupdate", False, ARM_COND_AL, "network update"),
        (0x002AF37C, "combinepatch_networkavailability", True, ARM_COND_AL, "mode latch"),
        (0x002AF38C, "combinepatch_networkskipremotejob", False, ARM_COND_AL, "network remote-job bypass"),
        (0x002AF3B4, "combinepatch_selectinitialconnectionmessage", True, ARM_COND_AL, "initial connection message"),
        (0x002ACBDC, "combinepatch_initialremoterecordupdate", False, ARM_COND_AL, "initial remote record update"),
        (0x002ACE14, "combinepatch_selectpostselectionconnectionmessage", True, ARM_COND_AL, "initial-record reconnect message"),
        (0x002AF460, "combinepatch_bankdatasyncdispatch", False, ARM_COND_AL, "Bank data sync"),
        (0x002AF720, "combinepatch_bankdatasyncturtlestate0", False, ARM_COND_AL, "offline Turtle descriptor preservation after Bankdata load"),
        (0x002AFE50, "combinepatch_bankdatasyncinitialize", False, ARM_COND_AL, "Bank data-sync initializer"),
        (0x002A58B4, "combinepatch_selectpostselectionstate", False, ARM_COND_AL, "post-selection state"),
        (0x002A93F4, "combinepatch_postselectionconnectionupdate", False, ARM_COND_AL, "post-selection connection"),
        (0x002A8760, "combinepatch_transactionrecoveryupdate", False, ARM_COND_AL, "mode-specific mismatch notice completion"),
        (0x002A5860, "combinepatch_recoveryresult", False, ARM_COND_AL, "mismatch retry through native game-selection creation"),
        (0x002A588C, "combinepatch_forcerollbacksuccess", False, ARM_COND_EQ, "official forced rollback completion latch"),
        (0x002A970C, "combinepatch_selectpostselectionconnectionmessage", True, ARM_COND_AL, "post-selection initializer message"),
        (0x002A981C, "combinepatch_firstpresentdispatch", False, ARM_COND_AL, "first-present and local-mileage dispatch"),
        (0x002AED3C, "combinepatch_selectpostselectionconnectionmessage", True, ARM_COND_AL, "first-use connection message"),
        (0x002AF1D0, "combinepatch_selectpostselectionconnectionmessage", True, ARM_COND_AL, "network preparation message"),
        (0x002ABC24, "combinepatch_disconnectupdate", False, ARM_COND_AL, "disconnect"),
        (0x002ABD94, "combinepatch_disconnectskipremotejob", False, ARM_COND_AL, "disconnect remote-job bypass"),
        (0x002AE5C0, "combinepatch_bankcreatestate0", False, ARM_COND_AL, "first-use setup"),
        (0x002AE74C, "combinepatch_bankcreateaftermessages", False, ARM_COND_AL, "first-use messages"),
        (0x002AE85C, "combinepatch_createinitial", True, ARM_COND_AL, "first-use data creation"),
        (0x002AE864, "combinepatch_bankcreatesuccess", False, ARM_COND_NE, "first-use success"),
        (0x002A57C8, "combinepatch_rewardresult", False, ARM_COND_EQ, "mode-specific reward result"),
        (0x002A56F0, "combinepatch_homeresult", False, ARM_COND_EQ, "HOME redirect"),
        (0x002A56DC, "combinepatch_selectusebankstate", False, ARM_COND_EQ, "first menu item download route"),
        (0x002A5920, "combinepatch_homecleanupresult", False, ARM_COND_EQ, "download exit after native HOME cleanup"),
        (0x002AC958, "combinepatch_nogamechoice", False, ARM_COND_AL, "mode-specific no-game HOME entry"),
        (0x002AC79C, "combinepatch_selectnogamemessage", True, ARM_COND_AL, "mode-specific no-game prompt"),
        (0x002AC95C, "combinepatch_selectnogamedownloadbutton", True, ARM_COND_AL, "Download confirmation button label"),
        (0x002A58CC, "combinepatch_result21", False, ARM_COND_EQ, "language result"),
        (0x002A57E0, "combinepatch_result5", False, ARM_COND_EQ, "no-save result"),
        (0x002A57F0, "combinepatch_result6or12", False, ARM_COND_EQ, "result six or twelve"),
        (0x002A6C8C, "combinepatch_menuselectioncallback", False, ARM_COND_AL, "feature-menu callback"),
        (0x002A6824, "combinepatch_showmodegreeting", True, ARM_COND_AL, "first menu greeting"),
        (0x002A6978, "combinepatch_showmodegreeting", True, ARM_COND_AL, "return menu greeting"),
        (0x002ADD0C, "combinepatch_selectgameselectionmessage", True, ARM_COND_AL, "Unlock game-selection prompt"),
        (0x002AD92C, "combinepatch_showunlockprompt", True, ARM_COND_AL, "official unlock-code prompt"),
        (0x001D6308, "combinepatch_selectusebankmenutext", True, ARM_COND_AL, "first menu label"),
        (0x001D6310, "combinepatch_selectsupportmenutext", True, ARM_COND_AL, "support menu label"),
        (0x001D6318, "combinepatch_selectinstalledmovermenutext", True, ARM_COND_AL, "installed Mover menu label"),
        (0x001D6320, "combinepatch_selecthomemenutextr6", True, ARM_COND_AL, "installed HOME menu label"),
        (0x001D63A8, "combinepatch_selectdownloadmovermenutext", True, ARM_COND_AL, "download Mover menu label"),
        (0x001D63B0, "combinepatch_selecthomemenutextr5", True, ARM_COND_AL, "download HOME menu label"),
        (0x002D1248, "combinepatch_downloadcapturetrampoline", True, ARM_COND_AL, "capture after native current/legacy loading"),
        (0x002ABD8C, "combinepatch_selectdisconnectmessage", True, ARM_COND_AL, "disconnect message"),
        (0x002B1D7C, "combinepatch_savebegin", False, ARM_COND_AL, "save begin"),
        (0x002B1DFC, "combinepatch_savedisplaywait", False, ARM_COND_AL, "save display wait"),
        (0x002B1F4C, "combinepatch_saveturtlestate2", False, ARM_COND_AL, "offline Turtle descriptor preservation before commit"),
        (0x002B1FF8, "combinepatch_saveturtlestate1", False, ARM_COND_AL, "offline Turtle descriptor preservation before rollback"),
        (0x002B24A0, "combinepatch_savestage", True, ARM_COND_AL, "save stage"),
        (0x002B20AC, "combinepatch_savecommit", True, ARM_COND_AL, "save commit"),
        (0x002B20C0, "combinepatch_savecommitwait", False, ARM_COND_AL, "save commit wait"),
        (0x002B20E8, "combinepatch_saverollback", True, ARM_COND_AL, "save rollback"),
        (0x002B2100, "combinepatch_saverollbackwait", False, ARM_COND_AL, "save rollback wait"),
        (0x002B2548, "combinepatch_selectsavemessage", True, ARM_COND_AL, "save message"),
        (0x0015DC00, "combinepatch_turtleload", True, ARM_COND_AL, "Turtle load backend"),
        (0x0015DC40, "combinepatch_turtlesave", True, ARM_COND_AL, "Turtle save backend"),
        (0x002A484C, "turtleredirect_checkbackend", True, ARM_COND_AL, "title Turtle check"),
        (0x002AC578, "combinepatch_turtlecheck", True, ARM_COND_AL, "initial Turtle check"),
        (0x002AC678, "combinepatch_turtleformat", True, ARM_COND_AL, "Turtle format start"),
        (0x002AC68C, "combinepatch_turtleformatpoll", True, ARM_COND_AL, "Turtle format poll"),
    )
    for address, target_symbol, link, condition, name in hook_branches:
        expect_branch(image, address, symbols[target_symbol], link, condition, name)

    for address, target_symbol, name in (
        (0x0015DC00, "turtlestorage_loadatonce", "base Turtle load backend"),
        (0x0015DC40, "turtlestorage_saveatonce", "base Turtle save backend"),
        (0x002A484C, "turtlestorage_checkarchivestatus", "base title Turtle check"),
        (0x002AC578, "turtlestorage_checkarchivestatus", "base initial Turtle check"),
        (0x002AC678, "turtlestorage_formatstart", "base Turtle format start"),
        (0x002AC68C, "turtlestorage_formatpoll", "base Turtle format poll"),
    ):
        expect_branch(base_image, address, symbols[target_symbol], True, ARM_COND_AL, name)

    if symbols["combinepatch_modecount"] != 4 or symbols["combinepatch_modeoriginal"] != 3:
        raise ValueError("title selector must include Original after Unlock")
    if symbols["combinepatch_initiallanguagependingoffset"] != 5:
        raise ValueError("backend language latch overlaps mode or ticket flags")
    expect_word(image, symbols["combinepatch_titlemodetextinitialize"] + 24,
                0xE5DC3003, "restore retained title selection")
    for operation, local_target, native_target in (
        ("load", "turtleredirect_loadbackend", "turtlestorage_loadatonce"),
        ("save", "turtleredirect_savebackend", "turtlestorage_saveatonce"),
        ("check", "turtleredirect_checkbackend", "turtlestorage_checkarchivestatus"),
        ("format", "turtleredirect_formatbackend", "turtlestorage_formatstart"),
        ("formatpoll", "turtleredirect_formatpoll", "turtlestorage_formatpoll"),
    ):
        wrapper = symbols[f"combinepatch_turtle{operation}"]
        if not EXPANDED_PAYLOAD_START <= wrapper < symbols["combinepatch_codeusedend"]:
            raise ValueError(f"Turtle {operation} wrapper outside added pages")
        word = read_word(image, wrapper)
        if word & 0xFFFFF000 != 0xE59FC000:
            raise ValueError(f"Turtle {operation} lacks session-mode literal")
        expect_word(image, wrapper + 8 + (word & 0xFFF), RUNTIME_STORAGE_START,
                    f"Turtle {operation} session storage")
        expect_word(image, wrapper + 4, 0xE5DCC000, f"Turtle {operation} mode load")
        expect_word(image, wrapper + 8, 0xE35C0000, f"Turtle {operation} offline test")
        expect_branch(image, wrapper + 12, symbols[local_target], False, ARM_COND_EQ,
                      f"Turtle {operation} offline route")
        expect_branch(image, wrapper + 16, symbols[native_target], False, ARM_COND_AL,
                      f"Turtle {operation} native route")
    release = symbols["combinepatch_titlereleasewaitingview"]
    expect_word(base_image, 0x002B1B98, 0xE595402C, "native title exit UI root load")
    expect_word(image, release, 0xE595402C, "title exit UI root load")
    expect_word(image, release + 4, 0xE1A00004, "cached prompt release argument")
    expect_branch(image, release + 8, symbols["bankui_releasewaitingview"],
                  True, ARM_COND_AL, "native asynchronous prompt release")
    expect_word(image, release + 12, 0xE3500000, "wait for cached prompt release")
    expect_branch(image, release + 16, 0x002B1BE4, False, ARM_COND_EQ,
                  "pending release keeps title view alive")
    expect_branch(image, release + 20, 0x002B1B9C, False, ARM_COND_AL,
                  "released prompt resumes native title destruction")
    for begin, end, name in (
        (0x001D6A34, 0x001D6AAC, "native prompt release lifecycle"),
        (0x002B1B5C, 0x002B1B98, "native title exit animation wait"),
        (0x002B1B9C, 0x002B1BDC, "native title view destruction"),
    ):
        expect_bytes(image, begin, base_image[image_offset(begin):image_offset(end)], name)
    for address in (0x002B1BD8, 0x002B1BE0, 0x002B1BF0, 0x002B1C28):
        expect_word(image, address, read_word(base_image, address), "stock title lifecycle")
    expect_word(base_image, 0x002AC958, 0xE5945040, "native no-game choice view")
    no_game = symbols["combinepatch_nogamechoice"]
    expect_branch(image, no_game + 12, no_game + 24, False, ARM_COND_EQ,
                  "Original no-game HOME choice")
    expect_word(image, no_game + 16, 0xE35C0001, "Download no-game choice check")
    expect_branch(image, no_game + 20, symbols["initialgamecheck_failuretransition"],
                  False, ARM_COND_NE, "Offline and Unlock block HOME after confirmation")
    expect_word(image, no_game + 24, 0xE5945040, "native no-game choice view")
    expect_branch(image, no_game + 28, 0x002AC95C, False, ARM_COND_AL,
                  "Download and Original no-game choice continuation")
    no_game_message = symbols["combinepatch_selectnogamemessage"]
    expect_word(image, no_game_message + 16, 0x012FFF1E, "Original no-game message return")
    expect_word(image, no_game_message + 20, 0xE35C0001, "Download no-game prompt mode")
    expect_word(image, no_game_message + 24, 0x03A02073, "Download no-game prompt ID")
    expect_word(image, no_game_message + 28, 0x13A02070, "blocked no-game prompt ID")
    game_selection = symbols["combinepatch_selectgameselectionmessage"]
    expect_word(image, game_selection + 8, 0xE35C0002, "Unlock-only game-selection guidance")
    expect_word(image, game_selection + 12, 0x03A0106E, "Unlock game-selection prompt ID")
    expect_word(image, game_selection + 16, 0x13A01003, "native game-selection prompt in other modes")
    expect_word(base_image, 0x002AC95C, 0xE3A03059, "native HOME confirmation label")
    download_button = symbols["combinepatch_selectnogamedownloadbutton"]
    for offset, word, description in (
        (0, 0xE10F3000, "save caller flags"),
        (8, 0xE5DCC000, "read session mode"),
        (12, 0xE35C0001, "check Download Mode"),
        (16, 0x03A0C075, "Download confirmation label"),
        (20, 0x13A0C059, "native HOME confirmation in other modes"),
        (24, 0xE128F003, "restore caller flags"),
        (28, 0xE1A0300C, "return selected label in r3"),
        (32, 0xE12FFF1E, "return to native button setup"),
    ):
        expect_word(image, download_button + offset, word, description)
    if image[image_offset(0x002AC960):image_offset(0x002AC9B4)] != \
            base_image[image_offset(0x002AC960):image_offset(0x002AC9B4)]:
        raise ValueError("native cancel label or no-game button setup changed")
    for name, result in (("combinepatch_selectusebankstate", 28),
                         ("combinepatch_homecleanupresult", 20)):
        wrapper = symbols[name]
        expect_word(image, wrapper + 4, 0xE5DCC000, "download route session mode")
        expect_word(image, wrapper + 8, 0xE35C0001, "download route mode check")
        expect_word(image, wrapper + 12, 0x03A00000 | result, "download route state")
        expect_word(image, wrapper + 16, 0xE8BD8010, "download route native return")
    capture = symbols["combinepatch_downloadcapturetrampoline"]
    expect_word(image, 0x002D11C4, read_word(base_image, 0x002D11C4),
                "native download callback metadata-offset literal load")
    expect_word(base_image, 0x002D1248, 0xE3560000, "native post-load flag comparison")
    expect_word(image, capture + 32, 0xE5D40041, "capture complete-file route flag")
    capture_return, link, condition = decode_arm_branch(image, capture + 28)
    if link or condition != ARM_COND_NE:
        raise ValueError("download capture must reject other session modes")
    expect_branch(image, capture + 40, capture_return, False, ARM_COND_EQ,
                  "download capture excludes ordinary game-dependent route")
    expect_word(image, capture + 48, 0xE3A01002, "capture failure status")
    expect_word(image, capture + 52, 0xE5C01001, "capture status before SD write")
    expect_word(image, capture + 56, 0xE5940008, "capture download flow")
    expect_word(image, capture + 60, 0xE59000CC, "capture natively loaded bank object")
    expect_branch(image, capture + 64, symbols["bankdataredirect_capturedownloaded"],
                  True, ARM_COND_AL, "capture current-format object")
    expect_word(image, capture + 80, 0xE3A00001, "capture success status")
    expect_word(image, capture + 84, 0xE5C10001, "capture success after checked SD commit")
    expect_word(image, capture_return + 16, 0xE3560000, "replay native post-load flag comparison")
    callback_start, callback_end = image_offset(0x002D11B0), image_offset(0x002D1318)
    callback = bytearray(image[callback_start:callback_end])
    hook_offset = image_offset(0x002D1248) - callback_start
    callback[hook_offset:hook_offset + 4] = base_image[
        image_offset(0x002D1248):image_offset(0x002D124C)
    ]
    if callback != base_image[callback_start:callback_end]:
        raise ValueError("native download loading, metadata or completion processing changed")
    for start, end in ((0x002D1084, 0x002D10FC), (0x002AFE94, 0x002B0074)):
        if image[image_offset(start):image_offset(end)] != \
                base_image[image_offset(start):image_offset(end)]:
            raise ValueError("native first-create callbacks or HOME cleanup changed")

    # Unlock wrappers replay only the native prologues. Recovery, networking,
    # selected-game saving, callback cleanup and state creation stay native.
    # 解锁包装仅重放原版入口；恢复、联网、所选游戏保存、回调清理与状态创建保持原版。
    for entry, trampoline, prologue, end in (
        (0x002A8760, "combinepatch_transactionrecoveryupdateofficial", 0xE92D40F0, 0x002A9118),
        (0x002A93F4, "combinepatch_postselectionconnectionupdateofficial", 0xE92D4070, 0x002A9700),
    ):
        expect_word(base_image, entry, prologue, f"base {trampoline} prologue")
        expect_word(image, symbols[trampoline], prologue, f"{trampoline} prologue")
        expect_branch(image, symbols[trampoline] + 4, entry + 4, False, ARM_COND_AL,
                      f"{trampoline} native continuation")
        if image[image_offset(entry + 4):image_offset(end)] != \
                base_image[image_offset(entry + 4):image_offset(end)]:
            raise ValueError(f"native recovery body or cleanup changed at {entry:#x}")
    for start, end in ((0x002A5864, 0x002A588C), (0x002A5890, 0x002A58A4),
                       (0x002A593C, 0x002A6750)):
        if image[image_offset(start):image_offset(end)] != \
                base_image[image_offset(start):image_offset(end)]:
            raise ValueError("native recovery routing, controller or state creation changed")
    expect_branch(image, symbols["combinepatch_forcerollbacksuccess"] + 20,
                  0x002A58A4, False, ARM_COND_AL, "forced rollback resumes native state 17")
    if symbols["unlockmode_recoverystorage"] != 0x003FAFF6:
        raise ValueError("unlock recovery status moved outside its reserved runtime byte")

    expect_word(base_image, 0x002B1AD0, 0xE92D40F8, "base title-state prologue")
    expect_word(base_image, 0x002AC958, 0xE5945040, "base no-game HOME choice setup")
    # Keep message completion/cleanup and the existing failure transition intact.
    # 保持提示完成／清理以及现有失败返回路径不变。
    for start, end in ((0x002AC924, 0x002AC958), (0x002ACA90, 0x002ACAA4)):
        start_offset, end_offset = image_offset(start), image_offset(end)
        if image[start_offset:end_offset] != base_image[start_offset:end_offset]:
            raise ValueError("native no-game message or failure path was modified")
    expect_word(base_image, 0x002A9810, 0xEB00435E, "base first-present flag getter")
    expect_word(image, 0x002A9810, 0xEB00435E, "native first-present flag getter")
    expect_branch(
        base_image,
        0x002B4974,
        symbols["bankui_setmessageline"],
        True,
        ARM_COND_AL,
        "base title HOME-help binding",
    )
    expect_word(base_image, 0x002AF38C, 0xE594100C, "base connection allocator context")
    expect_word(base_image, 0x002AF3B4, 0xE3A0100C, "base initial connection message")
    expect_word(base_image, 0x002AF390, 0xE3A00E2B, "base connection allocator size")
    expect_branch(
        base_image,
        0x002AF394,
        symbols["allocator_allocate"],
        True,
        ARM_COND_AL,
        "base connection allocator call",
    )
    expect_word(
        image,
        symbols["combinepatch_networkskipremotejobofficial"],
        0xE594100C,
        "download connection allocator context",
    )
    expect_word(
        image,
        symbols["combinepatch_networkskipremotejobofficial"] + 4,
        0xE3A00E2B,
        "download connection allocator size",
    )
    expect_branch(
        image,
        symbols["combinepatch_networkskipremotejobofficial"] + 8,
        symbols["allocator_allocate"],
        True,
        ARM_COND_AL,
        "download connection allocator call",
    )
    expect_branch(
        image,
        symbols["combinepatch_networkskipremotejobofficial"] + 12,
        0x002AF398,
        False,
        ARM_COND_AL,
        "download connection native resume",
    )

    expect_word(base_image, 0x002ABD94, 0xE594100C, "base disconnect allocator context")
    expect_word(base_image, 0x002ABD98, 0xE3A00E2B, "base disconnect allocator size")
    expect_branch(
        base_image,
        0x002ABD9C,
        symbols["allocator_allocate"],
        True,
        ARM_COND_AL,
        "base disconnect allocator call",
    )
    expect_word(
        image,
        symbols["combinepatch_disconnectskipremotejobofficial"],
        0xE594100C,
        "download disconnect allocator context",
    )
    expect_word(
        image,
        symbols["combinepatch_disconnectskipremotejobofficial"] + 4,
        0xE3A00E2B,
        "download disconnect allocator size",
    )
    expect_branch(
        image,
        symbols["combinepatch_disconnectskipremotejobofficial"] + 8,
        symbols["allocator_allocate"],
        True,
        ARM_COND_AL,
        "download disconnect allocator call",
    )
    expect_branch(
        image,
        symbols["combinepatch_disconnectskipremotejobofficial"] + 12,
        0x002ABDA0,
        False,
        ARM_COND_AL,
        "download disconnect native resume",
    )

    expect_word(base_image, 0x002AE5C0, 0xE59F02D0, "base first-use root-slot literal load")
    expect_word(base_image, 0x002AE5C4, 0xE594100C, "base first-use allocator context")
    expect_ldr_r0_literal(
        image,
        symbols["combinepatch_bankcreatestate0official"],
        BANK_ROOT_POINTER_SLOT,
        "download first-use root-slot literal",
    )
    expect_word(
        image,
        symbols["combinepatch_bankcreatestate0official"] + 4,
        0xE594100C,
        "download first-use allocator context",
    )
    expect_branch(
        image,
        symbols["combinepatch_bankcreatestate0official"] + 8,
        0x002AE5C8,
        False,
        ARM_COND_AL,
        "download first-use native resume",
    )
    expect_word(base_image, 0x002AE74C, 0xE59F0144, "base first-use post-message root-slot literal load")
    expect_word(base_image, 0x002AE750, 0xE5905000, "base first-use post-message root load")
    expect_ldr_r0_literal(
        image,
        symbols["combinepatch_bankcreateaftermessagesofficial"],
        BANK_ROOT_POINTER_SLOT,
        "download post-message root-slot literal",
    )
    expect_word(
        image,
        symbols["combinepatch_bankcreateaftermessagesofficial"] + 4,
        0xE5905000,
        "download first-use post-message root load",
    )
    expect_branch(
        image,
        symbols["combinepatch_bankcreateaftermessagesofficial"] + 8,
        0x002AE754,
        False,
        ARM_COND_AL,
        "download first-use post-message native resume",
    )
    expect_word(base_image, 0x002AE864, 0x13A00007, "base first-use success state")
    expect_word(base_image, 0x002B2548, 0xE3A01008, "base save message")
    expect_word(
        image,
        symbols["combinepatch_bankcreatesuccess"] + 16,
        0x03A00007,
        "Download and Original wait for native first-use callback",
    )
    expect_word(image, symbols["combinepatch_bankcreatesuccess"] + 12,
                0x135C0001, "Download first-use callback wait check")
    expect_word(image, symbols["combinepatch_bankcreatesuccess"] + 20,
                0x13A00008, "Offline and Unlock first-use completion state")

    expect_branch(
        image,
        symbols["combinepatch_homeresult"] + 16,
        symbols["combinepatch_redirecthometolanguage"],
        False,
        ARM_COND_AL,
        "HOME language redirect",
    )
    expect_branch(image, symbols["combinepatch_homeresult"] + 12,
                  0x002A5778, False, ARM_COND_EQ, "Original HOME dispatch")
    menu_callback = symbols["combinepatch_menuselectioncallback"]
    expect_word(image, menu_callback + 8, 0xE3520003, "Original menu mode check")
    expect_branch(image, menu_callback + 12, menu_callback + 44, False,
                  ARM_COND_EQ, "Original menu native callback")
    expect_word(image, menu_callback + 16, 0xE5902010, "menu state load")
    expect_word(image, menu_callback + 20, 0xE3520001, "menu state check")
    expect_word(image, menu_callback + 28, 0xE3510002, "support entry check")
    expect_branch(
        image,
        menu_callback + 32,
        symbols["combinepatch_menuselectiondisabled"],
        False,
        ARM_COND_EQ,
        "support entry return",
    )
    expect_word(image, menu_callback + 36, 0xE3510003, "Mover entry check")
    expect_branch(
        image,
        menu_callback + 40,
        symbols["combinepatch_menuselectiondisabled"],
        False,
        ARM_COND_EQ,
        "Mover entry return",
    )
    expect_word(
        image,
        symbols["combinepatch_menuselectiondisabled"],
        0xE92D4070,
        "menu immediate-restore prologue",
    )
    expect_word(
        image,
        symbols["combinepatch_menuselectiondisabled"] + 4,
        0xE1A04000,
        "menu immediate-restore state register",
    )
    expect_word(
        image,
        symbols["combinepatch_menuselectiondisabled"] + 8,
        0xE5945038,
        "menu preserved-selection manager load",
    )
    expect_branch(
        image,
        symbols["combinepatch_menuselectiondisabled"] + 40,
        symbols["ui_setselectionfocus"],
        True,
        ARM_COND_AL,
        "menu controller-focus restore",
    )
    expect_word(
        image,
        symbols["combinepatch_menuselectiondisabled"] + 56,
        0xE5840010,
        "menu remains in interactive substate",
    )

    for address, name in (
        (0x002AF390, "network remote-job bypass padding 0"),
        (0x002AF394, "network remote-job bypass padding 1"),
        (0x002ABD98, "disconnect remote-job bypass padding 0"),
        (0x002ABD9C, "disconnect remote-job bypass padding 1"),
        (0x002AE5C4, "first-use setup padding"),
        (0x002AE750, "first-use messages padding"),
        (0x002B1D80, "save begin padding"),
        (0x002B1E00, "save display wait padding 0"),
        (0x002B1E04, "save display wait padding 1"),
        (0x002B20C4, "save commit wait padding 0"),
        (0x002B20C8, "save commit wait padding 1"),
        (0x002B2104, "save rollback wait padding 0"),
        (0x002B2108, "save rollback wait padding 1"),
    ):
        expect_word(image, address, ARM_NOP, name)

    # Initialization supplies job/shared/mode; the C selector keeps the
    # native one-argument initialization ABI. Remaining interfaces use its flag.
    # 初始化传入作业／共享视图／模式；C 选择器保持原版单参数初始化 ABI。
    # 后续接口只读取该作业已经选定的后端。
    ticket_start = symbols["combinepatch_ticketinitialize"]
    ticket_end = symbols["combinepatch_ticketwrappersend"]
    if not payload_end <= ticket_start < ticket_end <= symbols["combinepatch_codeusedend"]:
        raise ValueError("ticket job wrappers exceed the added executable pages")
    policy = symbols["online_ticket_check_bypass"]
    policy_address = symbols["combinepatch_onlineticketfallback"]
    if policy not in (0, 1) or policy_address != ticket_end or \
            image[image_offset(policy_address)] != policy:
        raise ValueError("ticket fallback policy must be one byte: 0 native, 1 auto-test")
    backend = symbols["localticket_backendstorage"]
    if backend != RUNTIME_STORAGE_START + 4:
        raise ValueError("ticket backend flag overlaps native or mode storage")
    if symbols["srv_getservicehandle"] != 0x0023514C:
        raise ValueError("Bank SRV entry point does not match its native image")
    selector = symbols["localticket_initializeselected"]
    if not symbols["localticket_payloadbegin"] <= selector < symbols["localticket_payloadend"]:
        raise ValueError("ticket selector is outside its module")
    initialize = symbols["combinepatch_ticketinitialize"]
    expect_word(image, initialize, 0xE5941028, "ticket shared-data argument")
    expect_word(image, initialize + 8, 0xE5DC2000, "ticket mode argument")
    expect_word(image, initialize + 16, 0xE5DC3000, "ticket fallback policy argument")
    expect_branch(image, initialize + 20, selector, False, ARM_COND_AL,
                  "ticket backend selector")
    for offset, target in ((4, symbols["combinepatch_modestorage"]),
                           (12, policy_address)):
        address = initialize + offset
        instruction = read_word(image, address)
        if instruction & 0xFFFFF000 != 0xE59FC000:
            raise ValueError("ticket initialization lacks its argument pointer")
        literal = address + 8 + (instruction & 0xFFF)
        if not ticket_start <= literal < ticket_end:
            raise ValueError("ticket initialization literal exceeds its wrappers")
        expect_word(image, literal, target, "ticket initialization argument pointer")
    for operation in ("poll", "unbind"):
        wrapper = symbols[f"combinepatch_ticket{operation}"]
        local = symbols[f"combinepatch_ticket{operation}local"]
        if local != wrapper + 20:
            raise ValueError(f"ticket {operation} wrapper layout changed")
        for offset, target, name in ((0, backend, "ticket selected-backend pointer"),):
            address = wrapper + offset
            word = read_word(image, address)
            if word & 0xFFFFF000 != 0xE59FC000:
                raise ValueError(f"{name}: expected ARM ldr r12 literal")
            literal = address + 8 + (word & 0xFFF)
            if not ticket_start <= literal < ticket_end:
                raise ValueError("ticket wrapper literal is outside its payload")
            expect_word(image, literal, target, name)
            expect_word(image, address + 4, 0xE5DCC000, f"{name} byte load")
            expect_word(image, address + 8, 0xE35C0000, f"{name} zero test")
        expect_branch(image, wrapper + 12, local, False, ARM_COND_NE,
                      f"local ticket {operation}")
        expect_branch(image, wrapper + 16, symbols[f"ticketjob_{operation}"],
                      False, ARM_COND_AL, f"native ticket {operation}")
        if operation == "poll":
            expect_branch(image, local, symbols["localticket_poll"],
                          False, ARM_COND_AL, "local ticket result")
        else:
            expect_word(image, local, 0xE3A00001, "unbound local ticket success")
            expect_word(image, local + 4, 0xE12FFF1E, "local unbind return")

    if apply_bps(base_image, bps.read_bytes()) != image:
        raise ValueError("release code.bps does not reproduce the patched code image")


def verify_messages(source_romfs: Path, output_romfs: Path) -> None:
    module = load_module("patch_messages_verify", Path(__file__).resolve().parents[1] / "src" / "patch_messages.py")
    for archive in module.ARCHIVES:
        source = source_romfs / "a" / Path(archive)
        output = output_romfs / "a" / Path(archive)
        if not output.is_file():
            raise ValueError(f"missing LayeredFS message archive: {archive}")
        _, _, source_entries = module.message_codec.read_garc(source.read_bytes())
        _, _, output_entries = module.message_codec.read_garc(output.read_bytes())
        source_message_file = source_entries[module.MESSAGE_FILE_INDEX].files[0]
        output_message_file = output_entries[module.MESSAGE_FILE_INDEX].files[0]
        source_lines = module.message_codec.read_message_lines(source_message_file)
        output_lines = module.message_codec.read_message_lines(output_message_file)
        expected_no_game_values = module.build_no_game_prompt_values(
            module.message_codec.read_message_line_values(
                source_message_file, module.NO_GAME_RECORD_LINE
            )
        )
        no_game_values = module.message_codec.read_message_line_values(
            output_message_file, module.NO_GAME_BLOCKED_LINE
        )
        if no_game_values != expected_no_game_values:
            raise ValueError(f"no-game prompt mismatch in archive {archive}")
        no_game_end = len(no_game_values)
        while no_game_end and no_game_values[no_game_end - 1] == 0:
            no_game_end -= 1
        if no_game_values[no_game_end - 3:no_game_end] != [0x0010, 0x0001, 0xBE01]:
            raise ValueError(f"no-game final confirmation wait missing in archive {archive}")
        title_home = module.SHORT_TITLE_HOME_MESSAGES.get(
            archive, source_lines[module.TITLE_HOME_LINE]
        )
        expected_title_mode_offline = (
            f"{title_home}\n"
            f"{module.TITLE_MODE_OFFLINE_MESSAGES[archive]}"
        )
        expected_title_mode_download = (
            f"{title_home}\n"
            f"{module.TITLE_MODE_DOWNLOAD_MESSAGES[archive]}"
        )
        expected_title_mode_unlock = (
            f"{title_home}\n"
            f"{module.TITLE_MODE_UNLOCK_MESSAGES[archive]}"
        )
        expected_title_mode_original = (
            f"{title_home}\n"
            f"{module.TITLE_MODE_ORIGINAL_MESSAGES[archive]}"
        )
        for line in range(len(source_lines)):
            if module.message_codec.read_message_line_values(output_message_file, line) != \
                    module.message_codec.read_message_line_values(source_message_file, line):
                raise ValueError(f"stock message {line} changed in archive {archive}")
        expected_lines = {
            module.INTERNET_CONNECTION_LINE: source_lines[module.INTERNET_CONNECTION_LINE],
            module.BANK_CONNECTION_LINE: source_lines[module.BANK_CONNECTION_LINE],
            module.SAVE_LINE: source_lines[module.SAVE_LINE],
            module.DISCONNECT_LINE: source_lines[module.DISCONNECT_LINE],
            module.TITLE_VERSION_LINE: source_lines[module.TITLE_VERSION_LINE],
            module.SUPPORT_LINE: source_lines[module.SUPPORT_LINE],
            module.MOVER_DOWNLOAD_LINE: source_lines[module.MOVER_DOWNLOAD_LINE],
            module.MOVER_INSTALLED_LINE: source_lines[module.MOVER_INSTALLED_LINE],
            module.HOME_LINE: source_lines[module.HOME_LINE],
            module.BLANK_LINE: "",
            module.DOWNLOAD_PROGRESS_LINE: module.DOWNLOAD_PROGRESS_MESSAGES[archive],
            module.DOWNLOAD_SUCCESS_LINE: module.DOWNLOAD_SUCCESS_MESSAGES[archive],
            module.DOWNLOAD_USE_BANK_LINE: module.DOWNLOAD_MENU_MESSAGES[archive],
            module.DOWNLOAD_NO_GAME_LINE: module.DOWNLOAD_NO_GAME_MESSAGES[archive],
            module.DOWNLOAD_FAILED_LINE: module.DOWNLOAD_FAILED_MESSAGES[archive],
            module.DOWNLOAD_START_LINE: module.DOWNLOAD_START_MESSAGES[archive],
            module.OFFLINE_INITIAL_CONNECT_LINE: module.OFFLINE_INITIAL_CONNECT_MESSAGES[archive],
            module.OFFLINE_BANK_CONNECTION_LINE: module.OFFLINE_BANK_CONNECTION_MESSAGES[archive],
            module.OFFLINE_SAVE_LINE: module.OFFLINE_SAVE_MESSAGES[archive],
            module.OFFLINE_DISCONNECT_LINE: module.OFFLINE_DISCONNECT_MESSAGES[archive],
            module.TITLE_MODE_OFFLINE_LINE: expected_title_mode_offline,
            module.TITLE_MODE_DOWNLOAD_LINE: expected_title_mode_download,
            module.TITLE_MODE_UNLOCK_LINE: expected_title_mode_unlock,
            module.TITLE_MODE_ORIGINAL_LINE: expected_title_mode_original,
            module.DISABLED_LINE: module.DISABLED_MESSAGES[archive],
            module.LANGUAGE_MENU_LINE: module.LANGUAGE_MENU_MESSAGES[archive],
            module.UNLOCK_USE_BANK_LINE: module.UNLOCK_MENU_MESSAGES[archive],
            module.UNLOCK_GAME_SELECTION_LINE: module.UNLOCK_GAME_SELECTION_MESSAGES[archive],
            module.UNLOCK_NO_RECOVERY_LINE: module.UNLOCK_NO_RECOVERY_MESSAGES[archive],
            module.UNLOCK_RECOVERED_LINE: module.UNLOCK_RECOVERED_MESSAGES[archive],
            module.UNLOCK_FORCED_RECOVERED_LINE: module.UNLOCK_FORCED_RECOVERED_MESSAGES[archive],
        }
        for line, expected in expected_lines.items():
            if output_lines[line] != expected:
                raise ValueError(f"message line {line} mismatch in archive {archive}")
        source_greeting = module.message_codec.read_message_lines(
            source_entries[module.MENU_MESSAGE_FILE_INDEX].files[0]
        )[module.MENU_GREETING_LINE]
        output_greetings = module.message_codec.read_message_lines(
            output_entries[module.MENU_MESSAGE_FILE_INDEX].files[0]
        )
        menu_greeting = module.SHORT_MENU_GREETINGS.get(archive, source_greeting)
        expected_offline = f"{menu_greeting}\n{module.OFFLINE_MENU_GREETINGS[archive]}"
        expected_download = f"{menu_greeting}\n{module.DOWNLOAD_MENU_GREETINGS[archive]}"
        expected_unlock = f"{menu_greeting}\n{module.UNLOCK_MENU_GREETINGS[archive]}"
        if output_greetings[module.MENU_GREETING_LINE] != source_greeting:
            raise ValueError(f"stock greeting mismatch in archive {archive}")
        if output_greetings[module.OFFLINE_MENU_GREETING_LINE] != expected_offline:
            raise ValueError(f"offline greeting mismatch in archive {archive}")
        if output_greetings[module.DOWNLOAD_MENU_GREETING_LINE] != expected_download:
            raise ValueError(f"download greeting mismatch in archive {archive}")
        if output_greetings[module.UNLOCK_MENU_GREETING_LINE] != expected_unlock:
            raise ValueError(f"unlock greeting mismatch in archive {archive}")

        source_challenge_values = module.message_codec.read_message_line_values(
            source_message_file, module.UNLOCK_CHALLENGE_SOURCE_LINE
        )
        output_stock_challenge_values = module.message_codec.read_message_line_values(
            output_message_file, module.UNLOCK_SUPPORT_REFERENCE_LINE
        )
        expected_support_values = module.build_support_reference_values(
            source_challenge_values,
            module.UNLOCK_SUPPORT_REFERENCE_LABELS[archive],
        )
        if output_stock_challenge_values != expected_support_values:
            raise ValueError(f"support-reference prompt mismatch in archive {archive}")
        expected_unlock_values = module.build_unlock_prompt_values(
            source_challenge_values,
            module.UNLOCK_SUPPORT_REFERENCE_LABELS[archive],
            module.UNLOCK_CODE_LABELS[archive],
        )
        output_unlock_values = module.message_codec.read_message_line_values(
            output_message_file, module.UNLOCK_CHALLENGE_LINE
        )
        if output_unlock_values != expected_unlock_values:
            raise ValueError(f"unlock-code prompt mismatch in archive {archive}")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--base", required=True, type=Path)
    parser.add_argument("--patched", required=True, type=Path)
    parser.add_argument("--symbols", required=True, type=Path)
    parser.add_argument("--bps", required=True, type=Path)
    parser.add_argument("--base-exheader", required=True, type=Path)
    parser.add_argument("--exheader", required=True, type=Path)
    parser.add_argument("--source-romfs", required=True, type=Path)
    parser.add_argument("--output-romfs", required=True, type=Path)
    args = parser.parse_args()
    verify_code(args.base, args.patched, args.symbols, args.bps,
                args.base_exheader, args.exheader)
    verify_messages(args.source_romfs, args.output_romfs)
    print("combined patch static verification passed")


if __name__ == "__main__":
    main()
