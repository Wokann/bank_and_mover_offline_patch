"""Statically verify the built combined Bank patch and its LayeredFS resources.

静态校验已构建的 Bank 合并补丁及其 LayeredFS 资源。
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
from patch_version import add_version_arguments, label_from_arguments, label_storage, patch_label as build_patch_label

IMAGE_BASE = 0x00100000
EXPECTED_BASE_SHA256 = "2DCE4796F54807CF8A67F1CE6297BF472D969B30ED7A7E8E25C2A6C2BDC40ABF"
OPTIONAL_REWARD_STATE = 0x002B0270
OPTIONAL_REWARD_BODY_END = 0x002B1AD0
TICKET_JOB_CALLS = (
    (0x002B0444, "combinepatch_ticketinitialize", "ticketjob_initialize"),
    (0x002B110C, "combinepatch_ticketpurchasepoll", "ticketjob_pollpurchase"),
    (0x002B1248, "combinepatch_ticketpurchasepoll", "ticketjob_pollpurchase"),
)
TICKET_SEND_CALLS = (0x00169B00,0x00169DE8,0x001698E8,0x00169C6C,0x001699F8)
TICKET_UI_HOOKS = (
    (0x002B0374, "combinepatch_ticketuiprepare", None, False),
    (0x002B0404, "combinepatch_ticketuifade", "ui_setappletfade", True),
    (0x002B0438, "combinepatch_ticketuifade", "ui_setappletfade", True),
    (0x002B19D0, "combinepatch_ticketuicleanup", "messageui_animate", True),
)
TICKET_SYSTEM_HOOKS = (
    (0x002A1EE8,"shoprequest",0x0022DD84,True),
    (0x002A2184,"purchaserequest",0x0022DD84,True),
    (0x002A1C20,"eccatalog",0x00162658,True),
    (0x002A4604,"ecbalance",0x00162E78,True),
    (0x002A214C,"ecformat",0x00162F00,True),
    (0x002A4660,"ecformat",0x00162F00,True),
)
TICKET_EXPIRY_CALLS=(0x002A1E00,0x002A1F2C)
TICKET_LOCAL_JOB_HOOKS=(0x002A0518,0x002A0560,0x002A20E8,0x002A1CAC,0x002A05F8,*TICKET_EXPIRY_CALLS,
    0x002A0494,0x002A2018,*(entry[0] for entry in TICKET_SYSTEM_HOOKS))
TICKET_EC_STACK_SIZE = 0x10000
RESTORED_HOME_BOX_START = 0x00285BA8
RESTORED_HOME_BOX_END = 0x0028711C
RESTORED_HOME_START = 0x002A7BF0
RESTORED_HOME_END = 0x002A8760
TEXT_ACTUAL_END = 0x00313910
EXPANDED_PAYLOAD_START = 0x003FC000
EXPANDED_PAYLOAD_SIZE = 0x23A000
SERVER_CONTEXT_SIZE = 0x104000
SERVER_RUNTIME_SIZE = 0x216000
DATA_ACTUAL_END = 0x003ABACC
BSS_ACTUAL_END = 0x003FA904
READ_WRITE_MAPPED_END = 0x003FB000
RUNTIME_STORAGE_START = READ_WRITE_MAPPED_END - 0x10
TAIL_CAVE_START = 0x00313A40
TEXT_MAPPED_END = 0x00314000
VERSION_STORAGE_SIZE = 0x40
VERSION_STORAGE_START = TEXT_MAPPED_END - VERSION_STORAGE_SIZE
TAIL_CAVE_END = VERSION_STORAGE_START
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
    context_start = payload_end - SERVER_RUNTIME_SIZE
    if (symbols["fakeserver_contextsize"] != SERVER_CONTEXT_SIZE or
            symbols["fakeserver_runtimesize"] != SERVER_RUNTIME_SIZE or
            symbols["codeexpansion_executablesize"] != EXPANDED_PAYLOAD_SIZE - SERVER_RUNTIME_SIZE):
        raise ValueError("service storage or executable-only protection size differs")
    if (symbols["fakeserver_contextstart"] != context_start or
            symbols["fakeserver_contextend"] != context_start+SERVER_CONTEXT_SIZE or
            symbols["combinepatch_codeend"] != context_start):
        raise ValueError("fakeServer context overlaps code or original memory")
    worker=context_start+SERVER_CONTEXT_SIZE
    if (symbols["fakeserver_workerstart"] != worker or
            symbols["fakeserver_taskbodystart"] != worker+0x1000 or
            symbols["fakeserver_workerstackstart"] != worker+0x101000 or
            symbols["fakeserver_workerstackend"] != worker+0x111000 or
            symbols["fakeserver_reservedtailstart"] != worker+0x111000 or
            symbols["fakeserver_runtimeend"] != payload_end):
        raise ValueError("process-owned worker/control/body/stack layout differs")
    client = image[image_offset(symbols["fakeserverclient_get"]):
                   image_offset(symbols["fakeserverclient_endsession"])]
    # The aligned context may be loaded by MOV rather than a literal pool.
    # 对齐后的上下文地址可能由 MOV 直接装入，而非放在字面量池中。
    context_load = read_word(image, symbols["fakeserverclient_get"] + 4)
    rotation = ((context_load >> 8) & 15) * 2
    immediate = context_load & 255
    context_value = ((immediate >> rotation) | (immediate << (32 - rotation))) & 0xFFFFFFFF
    thumb_context = False
    for offset in range(0, len(client) - 3, 2):
        move = int.from_bytes(client[offset:offset + 2], "little")
        shift = int.from_bytes(client[offset + 2:offset + 4], "little")
        register = (move >> 8) & 7
        if (move & 0xF800 == 0x2000 and shift & 0xF800 == 0 and
                shift & 7 == register and (shift >> 3) & 7 == register and
                (move & 255) << ((shift >> 6) & 31) == context_start):
            thumb_context = True
            break
    if (context_start.to_bytes(4, "little") not in client and
            not thumb_context and
            not (context_load & 0xFFFFF000 == 0xE3A00000 and
                 context_value == context_start)):
        raise ValueError("fakeServer client does not bind the reserved context")
    used_end = symbols["combinepatch_codeusedend"]
    if not EXPANDED_PAYLOAD_START < used_end <= context_start:
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
    expect_word(image, startup + 8, 0xE3A01909, "executable-only page size")
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
                base_exheader: Path, exheader: Path, *, patch_label: str | None = None) -> None:
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
    retired={"deletefile","renamefile","getfilesize","readcompletefile","writecompletefile","writefilerangeutf16"}
    if retired.intersection(symbols):
        raise ValueError("retired unreferenced filesystem wrappers are still imported")
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
        "combinepatch_connectionconstruct", "combinepatch_connectionpoll",
        "combinepatch_connectionunbind",
        "combinepatch_titlemodetextinitialize",
        "combinepatch_titlescreenupdate",
        "combinepatch_titleprocessmodetoggle",
        "combinepatch_titleexit", "combinepatch_titledestruct",
        "combinepatch_startupinitializegate", "combinepatch_startupinitialize",
        "combinepatch_startupsynchronizelanguage", "combinepatch_startupdestruct",
        "languagesettings_startupsync", "languagesettings_syncpending",
        "languagesettings_bindrecord", "languagesettings_save", "languagesettings_cancel",
        "combinepatch_settingsready", "combinepatch_settingsmissing", "combinepatch_settingsfailed",
        "combinepatch_titlebindsession",
        "combinepatch_titleresult",
        "combinepatch_turtleload",
        "combinepatch_turtlesave",
        "combinepatch_initialturtlecheck", "combinepatch_initialturtleload",
        "combinepatch_initialturtledestruct",
        "combinepatch_turtleformat",
        "combinepatch_turtleformatpoll",
        "combinepatch_nogamechoice",
        "combinepatch_selectnogamemessage",
        "combinepatch_selectmismatchmessage",
        "combinepatch_selectnogamedownloadbutton",
        "turtleredirect_preparesessionpoll", "turtleredirect_preparesessionpending",
        "turtleredirect_cancelpreparesession",
        "combinepatch_payloadbegin",
        "combinepatch_payloadend",
        "fshelpers_payloadbegin",
        "fshelpers_payloadend",
        "bankdataredirect_payloadbegin",
        "bankdataredirect_payloadend",
        "fakeserver_payloadbegin",
        "fakeserver_payloadend",
        "fakeserver_contextstart", "fakeserver_contextend", "fakeserver_contextsize",
        "codeexpansion_executablesize", "combinepatch_codeend",
        "fakeserverclient_get", "fakeserverclient_endsession", "fsserver_open", "fsserver_stage", "fsserver_ticket",
        "combinepatch_savestateupdate",
        "bankdataredirect_saveupdate",
        "bankdataredirect_originalsaveupdate",
        "systemtime_payloadbegin",
        "systemtime_payloadend",
        "serverticket_payloadbegin",
        "serverticket_payloadend",
        "serverticket_pollpurchase", "combinepatch_ticketpurchasepoll",
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
        "combinepatch_initialremoterecordupdate",
        "combinepatch_ticketinitialize",
        "serverticket_initializeselected",
        "serverticket_backendstorage",
        "srv_getservicehandle",
        "runtimeenvironment_get",
        "runtimeenvironment_initializeticket",
        "runtimeenvironment_useserverticket",
        "runtimeenvironment_storagestart",
        "runtimeenvironment_storageend",
        "fakeserverclient_ticket",
        "combinepatch_ticketsend",
        "combinepatch_ticketproviderbind",
        "combinepatch_ticketproviderunbind",
        "combinepatch_ticketwrappersend",
        "ticketjob_initialize",
        "ticketjob_poll",
        "ticketjob_unbind",
        "online_fake_server_ticket_fallback",
        "combinepatch_onlineticketfallback",
        "combinepatch_giftupdate", "servergifts_update", "servergifts_request",
        "servergifts_originalupdate", "fsgifts_reply",
        "combinepatch_bankdatasyncdispatch",
        "combinepatch_bankdatasyncdestruct",
        "bankdataredirect_exportdownloadupdate", "bankdataredirect_canceldownloaded",
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
        "combinepatch_nosaverollbackupdate",
        "combinepatch_nosaverollbackupdateofficial",
        "banknosaverollbackstate_update",
        "combinepatch_rewardresult",
        "combinepatch_homeresult",
        "combinepatch_selectusebankstate",
        "combinepatch_homecleanupresult",
        "combinepatch_result21",
        "combinepatch_result5",
        "combinepatch_result6or12",
        "combinepatch_remotebind",
        "combinepatch_stageprovider", "combinepatch_commitprovider",
        "combinepatch_rollbackprovider", "combinepatch_metadataprovider",
        "bankdataredirect_transactionprovider", "bankdataredirect_nosaveupdate",
        "nativeresponse_transactionreply", "nativeresponse_pollaccepted",
        "nativeresponse_acceptmetadata", "nativeresponse_pollmetadata",
        "combinepatch_menuselectioncallback",
        "combinepatch_menuselectiondisabled",
        "combinepatch_downloadcapturetrampoline",
        "combinepatch_showmodegreeting",
        "combinepatch_selectgameselectionmessage",
        "combinepatch_selectmismatchmessage",
        "combinepatch_showunlockprompt",
        "combinepatch_selectusebankmenutext",
        "combinepatch_selectinstalledmovermenutext",
        "combinepatch_selectdownloadmovermenutext",
        "combinepatch_selecthomemenutextr6",
        "combinepatch_selecthomemenutextr5",
        "combinepatch_selectdisconnectmessage",
        "combinepatch_disconnectwithlanguagesave",
        "bankdataredirect_createupdate", "bankdataredirect_originalcreateupdate",
        "combinepatch_bankcreateupdate", "combinepatch_accountinitialize",
        "combinepatch_accountunbind", "combinepatch_accountsend", "combinepatch_accountrequestready",
        "combinepatch_createprovider",
        "bankdataredirect_downloadupdate",
        "bankdataredirect_originaldownloadupdate",
        "nativeresponse_createconnection",
        "nativeresponse_pollconnection",
        "bankdataredirect_initialrecordupdate",
        "bankdataredirect_originalinitialrecordupdate",
        "combinepatch_recordprovider", "combinepatch_queryprovider",
        "combinepatch_initialcampaignrequest", "nativeresponse_pollrecord",
        "nativeresponse_pollquery",
        "serverticket_send",
        "systemtime_getcurrentdate",
        "bankdataredirect_capturedownloaded",
        "combinepatch_selectbankslotdate",
        "bankslot_getcurrentdate",
        "bankslot_writedatecall",
        "bankslot_cleardatecall",
        "bankslots_movedatecall",
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
        "turtleredirect_initialcheckpoll", "turtleredirect_initialloadpoll",
        "turtleredirect_formatbackend",
        "turtleredirect_formatpoll",
        "turtleredirect_statesavebegin",
        "turtleredirect_statesavepoll",
        "turtleredirect_originalstatesavebegin",
        "turtleredirect_originalstatesavepoll",
        "combinepatch_pathdatabegin",
        "combinepatch_pathdataend",
        "openarchive",
        "closearchive",
        "pathcommand",
        "renamepath",
        "setsize",
        "openfile",
        "resultisnotfound",
        "emptypath",
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
    expect_branch(image, slot_date + 20, symbols["systemtime_getcurrentdate"],
                  False, ARM_COND_AL, "shared console date for Offline slots")
    expect_word(image, slot_date + 24, symbols["combinepatch_modestorage"],
                "slot-date session-mode pointer")

    # State 19 keeps its native body, initializer, callbacks and timed finalizer.
    # Only its request transport and message call differ; callback waiting is native.
    # 仅替换请求传输和消息调用，完成回调与等待仍走原版。
    rollback_state = symbols["banknosaverollbackstate_update"]
    first, last = image_offset(rollback_state + 4), image_offset(0x002AED78)
    body = bytearray(image[first:last])
    for call in (0x002AED3C, 0x002AEC3C):
        position = image_offset(call) - first
        body[position:position + 4] = base_image[image_offset(call):image_offset(call) + 4]
    if body != base_image[first:last]:
        raise ValueError("native no-save rollback lifecycle changed outside its hooks")
    rollback_dispatch = symbols["bankdataredirect_originalnosaveupdate"]
    expect_word(image, rollback_dispatch, read_word(base_image, rollback_state),
                "native rollback prologue replay")
    expect_branch(image, rollback_dispatch + 4, rollback_state + 4,
                  False, ARM_COND_AL, "native rollback body resume")

    # Keep the marker at a stable address and reserve the complete zero-padded
    # block for future compatibility metadata.
    # 将标识固定在稳定地址，并为未来兼容性元数据保留完整的零填充区域。
    if symbols["offlinepatch_versionidentifier"] != VERSION_STORAGE_START:
        raise ValueError("offline patch version identifier moved")
    version_offset = image_offset(VERSION_STORAGE_START)
    expected_version_storage = label_storage(build_patch_label() if patch_label is None else patch_label)
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
    # Only job calls and the Offline-only applet UI edges may differ. Native
    # allocation, results, state transitions and destruction remain intact.
    # 仅允许作业调用和离线小程序 UI 边界变化；保留原版分配、结果、状态推进和析构。
    cursor = image_offset(OPTIONAL_REWARD_STATE)
    ticket_hooks = [(address, wrapper, native, True)
                    for address, wrapper, native in TICKET_JOB_CALLS] + list(TICKET_UI_HOOKS)
    for address, wrapper, native, link in sorted(ticket_hooks):
        end = image_offset(address)
        if image[cursor:end] != base_image[cursor:end]:
            raise ValueError("native entitlement flow changed outside a job call")
        if native is not None:
            expect_branch(base_image, address, symbols[native], True, ARM_COND_AL,
                          f"base {native} call")
        expect_branch(image, address, symbols[wrapper], link, ARM_COND_AL,
                      f"redirected {native} call")
        cursor = end + 4
    end = image_offset(OPTIONAL_REWARD_BODY_END)
    if image[cursor:end] != base_image[cursor:end]:
        raise ValueError("native entitlement companion functions were modified")
    for wrapper in ("combinepatch_ticketuiprepare", "combinepatch_ticketuifade",
                    "combinepatch_ticketuicleanup"):
        address = symbols[wrapper]
        expect_word(image, address + 4, 0xE5DCC000, f"{wrapper} session mode")
        expect_word(image, address + 8, 0xE35C0000, f"{wrapper} Offline-only test")
    prepare = symbols["combinepatch_ticketuiprepare"]
    expect_branch(image, prepare + 12, 0x002B03F0, False, ARM_COND_EQ,
                  "Offline ticket keeps the existing connection pane")
    expect_word(image, prepare + 16, read_word(base_image, 0x002B0374),
                "native ticket UI prologue replay")
    expect_branch(image, prepare + 20, 0x002B0378, False, ARM_COND_AL,
                  "native ticket UI resumes unchanged")
    fade = symbols["combinepatch_ticketuifade"]
    expect_word(image, fade + 12, 0x03A00001, "Offline applet fade completion")
    expect_word(image, fade + 16, 0x012FFF1E, "Offline applet fade return")
    expect_branch(image, fade + 20, symbols["ui_setappletfade"], False, ARM_COND_AL,
                  "native applet fade")
    cleanup = symbols["combinepatch_ticketuicleanup"]
    expect_branch(image, cleanup + 12, symbols["messageui_animate"], False, ARM_COND_NE,
                  "native ticket message cleanup")
    expect_word(image, cleanup + 16, 0xE5D4C030, "local ticket result phase")
    expect_word(image, cleanup + 20, 0xE35C0004, "successful local ticket only")
    expect_word(image, cleanup + 24, 0x03A00001, "preserve local connection pane")
    expect_word(image, cleanup + 28, 0x012FFF1E, "local ticket cleanup return")
    expect_branch(image, cleanup + 32, symbols["messageui_animate"], False, ARM_COND_AL,
                  "errors retain native ticket UI cleanup")
    job_start, job_end = image_offset(0x002A04C0), image_offset(0x002A2504)
    expected_job=bytearray(base_image[job_start:job_end])
    for address in TICKET_LOCAL_JOB_HOOKS:
        if not 0x002A04C0<=address<0x002A2504:
            continue
        offset=image_offset(address)
        expected_job[offset-job_start:offset-job_start+4]=image[offset:offset+4]
    if image[job_start:job_end] != expected_job:
        raise ValueError("native ticket job implementation was modified")
    # Native introduction, both gift passes, decoding, capacity checks, miles,
    # recipient writes, BOSS observer binding and finalizers remain intact.
    # 保留原介绍、两轮查询、解码、容量检查、里程、写入、BOSS 观察者绑定和收尾。
    gift_hooks = (
        (0x002A9750, "combinepatch_giftupdate", False),
        (0x002A9824, "combinepatch_selectpostselectionconnectionmessage", True),
        (0x002A9934, "combinepatch_giftresourcerequest", True),
        (0x002AAF50, "combinepatch_giftresourcerequest", True),
    )
    first, last = image_offset(0x002A9750), image_offset(0x002ABA70)
    body = bytearray(image[first:last])
    for address, wrapper, link in gift_hooks:
        expect_branch(image, address, symbols[wrapper], link, ARM_COND_AL,
                      f"gift dispatch {wrapper}")
        offset = image_offset(address)
        body[offset-first:offset-first+4] = base_image[offset:offset+4]
    if body != base_image[first:last]:
        raise ValueError("native gift lifecycle changed outside its update/transport/message hooks")
    for name, register in (("combinepatch_selectpostselectionconnectionmessage", 1),):
        message_selector = symbols[name]
        expect_word(image, message_selector + 4, 0xE5DCC000,
                    "connection message uses the latched session mode")
        expect_word(image, message_selector + 8, 0xE35C0000,
                    "connection message is Offline-only")
        expect_word(image, message_selector + 12, 0x03A00065 | (register << 12),
                    "Offline local-data connection message")
        expect_word(image, message_selector + 16, 0x13A0000E | (register << 12),
                    "native connection message in network modes")
        expect_word(image, message_selector + 20, 0xE12FFF1E,
                    "connection message selector returns to the native caller")
        expect_word(image, message_selector + 24, symbols["combinepatch_modestorage"],
                    "connection message session-mode pointer")
    expect_word(base_image, 0x002A9824, 0xE3A0100E, "base gift connection message")
    # r1 selects the table, r2 the line. Both mileage callers use table 37,
    # whose native line 14 includes number/plural tags. Generic connection
    # line 101 belongs to table 39 and must never replace either call.
    # r1 选择文本表、r2 选择条目。里程用表 37 的第 14 条（含数字／复数标签），
    # 连接提示 101 属于表 39，不得将编号相同当作可互换的文字。
    for address in (0x002AA990, 0x002AAEC8):
        for offset, word in ((-8,0xE3A03001),(-4,0xE5940040),
                             (0,0xE3A0200E),(4,0xE1A01003)):
            expect_word(base_image,address+offset,word,"native mileage view/table/line/style")
            expect_word(image,address+offset,word,"preserved mileage view/table/line/style")
        expect_branch(image,address+8,0x001D6650,True,ARM_COND_AL,"native mileage text consumer")
    expect_word(image,0x002B2B20,0xE3A02025,"portrait table 37 resource selection")
    expect_word(image,0x002B2B2C,0xE5850090,"portrait table bound at view+0x90")
    expect_word(image,0x002B2A68,0xE3A00027,"generic table 39 resource selection")
    for start,end in ((0x001D6650,0x001D6758),(0x002B2A50,0x002B2B64),
                      (0x002299F8,0x00229C00)):
        expect_bytes(image,start,base_image[image_offset(start):image_offset(end)],
                     "native text table consumer/constructor/bounds reader unchanged")
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
        == symbols["systemtime_payloadbegin"]
        < symbols["systemtime_payloadend"]
        == symbols["bankdataredirect_payloadbegin"]
        < symbols["bankdataredirect_payloadend"]
        == symbols["fakeserver_payloadbegin"]
        < symbols["fakeserver_payloadend"]
        == symbols["servergifts_payloadbegin"]
        < symbols["servergifts_payloadend"]
        == symbols["serverticket_payloadbegin"]
        < symbols["serverticket_payloadend"]
        == symbols["unlockmode_payloadbegin"]
        < symbols["unlockmode_payloadend"]
        == payload_end
    ):
        raise ValueError("imported local-file payload objects are not contiguous or ordered")

    business_wrapper_symbols = (
        "combinepatch_nosaverollbackupdate",
        "combinepatch_nosaverollbackupdateofficial",
        "combinepatch_selectbankslotdate",
        "combinepatch_networkavailability",
        "combinepatch_titlemodetextinitialize",
        "combinepatch_titlescreenupdate",
        "combinepatch_titleprocessmodetoggle",
        "combinepatch_initialremoterecordupdate",
        "combinepatch_giftupdate", "combinepatch_giftresourcerequest",
        "combinepatch_bankdatasyncdispatch",
        "combinepatch_bankdatasyncdestruct",
        "bankdataredirect_exportdownloadupdate", "bankdataredirect_canceldownloaded",
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
        "combinepatch_rewardresult",
        "combinepatch_homeresult",
        "combinepatch_selectusebankstate",
        "combinepatch_homecleanupresult",
        "combinepatch_result21",
        "combinepatch_result5",
        "combinepatch_result6or12",
        "combinepatch_remotebind",
        "combinepatch_stageprovider", "combinepatch_commitprovider",
        "combinepatch_rollbackprovider", "combinepatch_metadataprovider",
        "combinepatch_menuselectioncallback",
        "combinepatch_menuselectiondisabled",
        "combinepatch_downloadcapturetrampoline",
        "turtleredirect_loadbackend",
        "turtleredirect_savebackend",
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
        "combinepatch_showmodegreeting",
        "combinepatch_selectgameselectionmessage",
        "combinepatch_showunlockprompt",
        "combinepatch_selectusebankmenutext",
        "combinepatch_selectdisconnectmessage",
        "combinepatch_selectnogamedownloadbutton",
        "combinepatch_disconnectwithlanguagesave",
        "combinepatch_pathdatabegin",
        "combinepatch_pathdataend",
        "emptypath",
    )
    for name in metadata_wrapper_symbols:
        address = symbols[name]
        if not EXPANDED_PAYLOAD_START <= address < symbols["combinepatch_codeusedend"]:
            raise ValueError(f"{name} is outside the added executable pages")

    for name in (
        "nativeresponse_createconnection",
        "nativeresponse_pollconnection",
        "bankdataredirect_initialrecordupdate",
        "serverticket_send",
        "systemtime_getcurrentdate",
        "bankdataredirect_capturedownloaded",
        "openarchive",
        "closearchive",
        "pathcommand",
        "renamepath",
        "setsize",
        "openfile",
        "resultisnotfound",
        "unlockmode_trygetfirstcandidatecode",
    ):
        address = symbols[name]
        if not payload_begin <= address < payload_end:
            raise ValueError(f"{name} is outside the imported local-file payload")

    service_image = image[image_offset(symbols["fakeserver_payloadbegin"]):
                          image_offset(symbols["fakeserver_payloadend"])]
    # Bank names occur in both migration and live adapters; journals only in the adapter.
    for value, count in ((b"bankdata.bin\0",2),(b"bankdata.tmp\0",2),(b"bankdata.bak\0",2),
                         (b"transaction.bin\0",1),(b"transaction.tmp\0",1),(b"transaction.bak\0",1)):
        if service_image.count(value) != count:
            raise ValueError(f"missing or duplicated fakeServer path: {value!r}")
    for value in (b"/3ds/Bank/client/client.conf\0",b"/3ds/Bank/client/users\0",
                  b"/3ds/Bank/fakeServers/users\0",b"/3ds/Bank/fakeServers/shared/\0"):
        if value not in service_image:
            raise ValueError(f"missing scoped profile path: {value!r}")
    expect_word(image,symbols['fssdstorage']+36,1,'selected-user storage capability')
    for name in ('fsuser_select','fssd_selectuser','fssd_resetuser','fsaccountconfiguration_load',
                 'fsaccountconfiguration_purchase','consoleidentity_read'):
        if not payload_begin<=symbols[name]<payload_end:
            raise ValueError(f"profile entry outside payload: {name}")
    for obsolete in (b"/3ds/Bank/gen5.tmp\0",b"/3ds/Bank/gen5.bak\0",
                     b"/3ds/Bank/sav.bin\0",b"/3ds/Bank/fakeServers/transaction.bin\0",
                     b"bank/bankdata.bin\0",b"bank/transaction.bin\0"):
        if obsolete in service_image:
            raise ValueError(f"retired global NDS work path remains: {obsolete!r}")
    if not symbols["fakeserver_payloadbegin"] <= symbols["fstransaction_sourcepath"] < symbols["fakeserver_payloadend"]:
        raise ValueError("sibling NDS path resolver is outside the service payload")
    for name, value in {
        "emptypath": b"\0",
    }.items():
        expect_bytes(image, symbols[name], value, name)

    hook_branches = (
        (0x002B1AD0, "combinepatch_titlescreenupdate", False, ARM_COND_AL, "title input and session latch"),
        (0x002B4974, "combinepatch_titlemodetextinitialize", True, ARM_COND_AL, "title mode text"),
        (0x002B1B5C, "combinepatch_titleexit", False, ARM_COND_AL, "title rebind wait without repeated UI release"),
        (0x002B1CD4, "combinepatch_titledestruct", False, ARM_COND_AL, "cancel pending title rebind before destruction"),
        (0x002A46E0, "combinepatch_startupinitializegate", False, ARM_COND_AL, "startup helper waits for settings preparation"),
        (0x002A47F4, "combinepatch_startupinitialize", False, ARM_COND_AL, "startup heap allocated once across preparation waits"),
        (0x002A489C, "combinepatch_startupsynchronizelanguage", False, ARM_COND_AL, "sync SD after native language application"),
        (0x002A4958, "combinepatch_startupdestruct", False, ARM_COND_AL, "cancel startup delivery before releasing its heap"),
        (0x002B1B98, "combinepatch_titlereleasewaitingview", False, ARM_COND_AL, "only title L releases prompt before language change"),
        (0x002B1BDC, "combinepatch_titlebindsession", False, ARM_COND_AL, "session record reload after title teardown"),
        (0x002A5680, "combinepatch_titleresult", False, ARM_COND_EQ, "title L or storage binding result"),
        (0x002ABDEC, "combinepatch_languagereturndestruct", False, ARM_COND_AL, "cancel pending language sync before native destruction"),
        (0x002AF37C, "combinepatch_networkavailability", True, ARM_COND_AL, "mode latch"),
        (0x002AE568, "combinepatch_bankcreateupdate", False, ARM_COND_AL, "native create response polling"),
        (0x0011491C, "combinepatch_accountinitialize", False, ARM_COND_AL, "local account provider"),
        (0x0022DDCC, "combinepatch_accountunbind", False, ARM_COND_AL, "local account provider cleanup"),
        (0x00114F88, "combinepatch_accountrequestready", True, ARM_COND_AL, "local initial request readiness without network state mutation"),
        (0x0012259C, "combinepatch_accountsend", True, ARM_COND_AL, "initial-value request transport"),
        (0x002A3868, "combinepatch_createprovider", False, ARM_COND_AL, "creation request transport"),
        (0x002AF3A0, "combinepatch_connectionconstruct", True, ARM_COND_NE, "connection carrier"),
        (0x002ABDA8, "combinepatch_connectionconstruct", True, ARM_COND_NE, "disconnect carrier"),
        (0x002A43CC, "combinepatch_connectionpoll", False, ARM_COND_AL, "connection result transport"),
        (0x002A44A0, "combinepatch_connectionunbind", False, ARM_COND_AL, "connection unbind"),
        (0x0010D67C, "combinepatch_networkmonitorupdate", False, ARM_COND_AL, "system connection monitor mode gate"),
        (0x0029D944, "combinepatch_networkmonitorworkeraccount", True, ARM_COND_AL, "background connection monitor mode gate"),
        (0x002AF3B4, "combinepatch_selectinitialconnectionmessage", True, ARM_COND_AL, "initial connection message"),
        (0x002ACBDC, "combinepatch_initialremoterecordupdate", False, ARM_COND_AL, "initial remote record update"),
        (0x002A2FB0, "combinepatch_recordprovider", False, ARM_COND_AL, "record query transport"),
        (0x002A3198, "combinepatch_queryprovider", False, ARM_COND_AL, "transaction query transport"),
        (0x002A3710, "combinepatch_remoteunbind", False, ARM_COND_AL, "cancel local delivery before remote cleanup"),
        (0x002A3954, "combinepatch_downloadresponse", False, ARM_COND_AL, "local canceled download container cleanup"),
        (0x002ACEB4, "combinepatch_initialcampaignrequest", True, ARM_COND_AL, "initial campaign transport"),
        (0x002ACE14, "combinepatch_selectpostselectionconnectionmessage", True, ARM_COND_AL, "initial-record reconnect message"),
        (0x002AF460, "combinepatch_bankdatasyncdispatch", False, ARM_COND_AL, "Bank data sync"),
        (0x002AFE70, "combinepatch_bankdatasyncdestruct", False, ARM_COND_AL, "cancel owned export before download-state destruction"),
        (0x002AFE50, "combinepatch_bankdatasyncinitialize", False, ARM_COND_AL, "Bank data-sync initializer"),
        (0x002A93F4, "combinepatch_postselectionconnectionupdate", False, ARM_COND_AL, "post-selection connection"),
        (0x002A8760, "combinepatch_transactionrecoveryupdate", False, ARM_COND_AL, "mode-specific mismatch notice completion"),
        (0x002A8C98, "combinepatch_selectmismatchmessage", True, ARM_COND_AL, "Offline-only extra mismatch page"),
        (0x002AD7BC, "combinepatch_localforcerollbackupdate", False, ARM_COND_AL, "offline forced rollback dispatch"),
        (0x002A5860, "combinepatch_recoveryresult", False, ARM_COND_AL, "mismatch retry through native game-selection creation"),
        (0x002A588C, "combinepatch_forcerollbacksuccess", False, ARM_COND_EQ, "official forced rollback completion latch"),
        (0x002A970C, "combinepatch_selectpostselectionconnectionmessage", True, ARM_COND_AL, "post-selection initializer message"),
        (0x002A9750, "combinepatch_giftupdate", False, ARM_COND_AL, "native gift wait response"),
        (0x002A9824, "combinepatch_selectpostselectionconnectionmessage", True, ARM_COND_AL, "gift connection message"),
        (0x002A9934, "combinepatch_giftresourcerequest", True, ARM_COND_AL, "first gift resource request"),
        (0x002AAF50, "combinepatch_giftresourcerequest", True, ARM_COND_AL, "second gift resource request"),
        (0x002AED3C, "combinepatch_selectpostselectionconnectionmessage", True, ARM_COND_AL, "first-use connection message"),
        (0x002AF1D0, "combinepatch_selectpostselectionconnectionmessage", True, ARM_COND_AL, "network preparation message"),
        (0x002ABC24, "combinepatch_storagenoticedisconnectupdate", False, ARM_COND_AL, "storage error acknowledgement before disconnect"),
        (0x002AEB9C, "combinepatch_nosaverollbackupdate", False, ARM_COND_AL, "mode-specific no-save remote rollback"),
        (0x002A57C8, "combinepatch_rewardresult", False, ARM_COND_EQ, "mode-specific reward result"),
        (0x002A56F0, "combinepatch_homeresult", False, ARM_COND_EQ, "HOME redirect"),
        (0x002A56DC, "combinepatch_selectusebankstate", False, ARM_COND_EQ, "first menu item download route"),
        (0x002A5920, "combinepatch_homecleanupresult", False, ARM_COND_EQ, "download exit after native HOME cleanup"),
        (0x002AC958, "combinepatch_nogamechoice", False, ARM_COND_AL, "mode-specific no-game HOME entry"),
        (0x002AC79C, "combinepatch_selectnogamemessage", True, ARM_COND_AL, "mode-specific no-game prompt"),
        (0x002AC95C, "combinepatch_selectnogamedownloadbutton", True, ARM_COND_AL, "Download confirmation button label"),
        (0x002A58CC, "combinepatch_result21", False, ARM_COND_EQ, "native no-save rollback route"),
        (0x002A57E0, "combinepatch_result5", False, ARM_COND_EQ, "no-save result"),
        (0x002A57F0, "combinepatch_result6or12", False, ARM_COND_EQ, "result six or twelve"),
        (0x002A6C8C, "combinepatch_menuselectioncallback", False, ARM_COND_AL, "feature-menu callback"),
        (0x002A6824, "combinepatch_showmodegreeting", True, ARM_COND_AL, "first menu greeting"),
        (0x002A6978, "combinepatch_showmodegreeting", True, ARM_COND_AL, "return menu greeting"),
        (0x002ADD0C, "combinepatch_selectgameselectionmessage", True, ARM_COND_AL, "Unlock game-selection prompt"),
        (0x002AD92C, "combinepatch_showunlockprompt", True, ARM_COND_AL, "official unlock-code prompt"),
        (0x001D6308, "combinepatch_selectusebankmenutext", True, ARM_COND_AL, "first menu label"),
        (0x001D6318, "combinepatch_selectinstalledmovermenutext", True, ARM_COND_AL, "installed Mover menu label"),
        (0x001D6320, "combinepatch_selecthomemenutextr6", True, ARM_COND_AL, "installed HOME menu label"),
        (0x001D63A8, "combinepatch_selectdownloadmovermenutext", True, ARM_COND_AL, "download Mover menu label"),
        (0x001D63B0, "combinepatch_selecthomemenutextr5", True, ARM_COND_AL, "download HOME menu label"),
        (0x002D1248, "combinepatch_downloadcapturetrampoline", True, ARM_COND_AL, "capture after native current/legacy loading"),
        (0x002ABD8C, "combinepatch_selectdisconnectmessage", True, ARM_COND_AL, "disconnect message"),
        (0x002B1CF8, "combinepatch_savestateupdate", False, ARM_COND_AL, "offline source cleanup completion"),
        (0x001DF3E0, "combinepatch_remotebind", False, ARM_COND_AL, "remote container binding"),
        (0x002A26F8, "combinepatch_metadataprovider", False, ARM_COND_AL, "metadata query provider"),
        (0x002A25C4, "combinepatch_stageprovider", False, ARM_COND_AL, "stage provider"),
        (0x001D5DF0, "combinepatch_commitprovider", False, ARM_COND_AL, "commit provider"),
        (0x001D5CA4, "combinepatch_rollbackprovider", False, ARM_COND_AL, "rollback provider"),
        (0x002A2838, "combinepatch_recoverytokenprovider", False, ARM_COND_AL, "recovery token provider"),
        (0x002B2548, "combinepatch_selectsavemessage", True, ARM_COND_AL, "save message"),
        (0x0015DC00, "combinepatch_turtleload", True, ARM_COND_AL, "Turtle load backend"),
        (0x0015DC40, "combinepatch_turtlesave", True, ARM_COND_AL, "Turtle save backend"),
        (0x002AC578, "combinepatch_initialturtlecheck", False, ARM_COND_AL, "initial Turtle check background wait"),
        (0x002AC5A0, "combinepatch_initialturtleload", False, ARM_COND_AL, "initial Turtle decoder/import wait"),
        (0x002ACB10, "combinepatch_initialturtledestruct", False, ARM_COND_AL, "cancel initial settings delivery before destruction"),
        (0x002AC678, "combinepatch_turtleformat", True, ARM_COND_AL, "Turtle format start"),
        (0x002AC68C, "combinepatch_turtleformatpoll", True, ARM_COND_AL, "Turtle format poll"),
        (0x002B4AB4, "combinepatch_localsavebegin", False, ARM_COND_AL, "native settings-save begin"),
        (0x002B4A20, "combinepatch_localsavepoll", False, ARM_COND_AL, "native settings-save poll"),
    )
    for address, target_symbol, link, condition, name in hook_branches:
        expect_branch(image, address, symbols[target_symbol], link, condition, name)
    for name, register, native, disabled in (
        ("combinepatch_selectdownloadmovermenutext", 6, 44, 124),
        ("combinepatch_selectinstalledmovermenutext", 5, 45, 125),
        ("combinepatch_selecthomemenutextr6", 6, 86, 126),
        ("combinepatch_selecthomemenutextr5", 5, 86, 126),
    ):
        wrapper = symbols[name]
        expect_word(image, wrapper+20, 0x03A00000 | register << 12 | native,
                    "Original Mode retains full native label")
        expect_word(image, wrapper+24, 0x13A00000 | register << 12 | disabled,
                    "disabled action uses its own full label with suffix")
        expect_word(image, wrapper+32, 0xE128F00C, "menu selector restores caller flags")
    expect_word(base_image,0x001D6310,0xE3A0802B,"native support menu label hook")
    expect_branch(image,0x001D6310,symbols["combinepatch_selectsupportmenutext"],True,ARM_COND_AL,
                  "support identity label selector")
    for address,old,new in ((0x002A6F78,0x00229D48,"supportcode_readfriendprincipal"),
                            (0x002A704C,0x0021E5BC,"supportcode_readactprincipal"),
                            (0x002A6F98,0x002A4078,"combinepatch_formatsupportcode")):
        expect_branch(base_image,address,old,True,ARM_COND_AL,"native support source/formatter")
        expect_branch(image,address,symbols[new],True,ARM_COND_AL,"support response observer")
    expect_bytes(image,0x002A4078,base_image[image_offset(0x002A4078):image_offset(0x002A4204)],
                 "native support code encoder unchanged")
    for address in (0x002A4838,0x002A484C,0x002B1C2C,0x002A5638,0x002A5648):
        expect_word(image,address,read_word(base_image,address),
                    "native startup language authority; no SD title preview")

    for address, result, name, operation, job in (
            (0x002A2FB0, 0x002A2FCC, "combinepatch_recordprovider",5,5),
            (0x002A3198, 0x002A31B4, "combinepatch_queryprovider",4,5),
            (0x002A3624, 0x002A363C, "combinepatch_downloadprovider",6,6),
            (0x002A2B24, 0x002A2B3C, "combinepatch_metadataupdateprovider",10,5),
            (0x002A3868, 0x002A3884, "combinepatch_createprovider",7,5),
            (0x002A25C4, 0x002A25DC, "combinepatch_stageprovider",8,6)):
        bridge=symbols[name]
        expect_word(base_image,address,0xE5940010,"native provider load")
        expect_word(image,bridge+16,0xE92D4010,"preserve native provider return")
        expect_word(image,bridge+20,0xE1A00004,"register native remote owner")
        expect_word(image,bridge+24,0xE1A01000|job,"register native job lifetime")
        expect_word(image,bridge+28,0xE3A02000|operation,"register service operation")
        expect_branch(image,bridge+32,symbols["nativeresponse_acceptoperation"],True,ARM_COND_AL,
                      "register local request without storage I/O")
        expect_word(image,bridge+36,0xE8BD4010,"restore native provider return")
        expect_branch(image,bridge+40,result,False,ARM_COND_AL,"resume native job registration")
        expect_word(image,bridge+44,0xE5940010,"online provider load replay")
        expect_branch(image,bridge+48,address+4,False,ARM_COND_AL,"online provider dispatch")
    unbind=symbols["combinepatch_remoteunbind"]
    expect_word(base_image,0x002A3710,0xE92D4070,"native remote cleanup prologue")
    expect_word(image,unbind,0xE92D500F,"preserve remote cleanup arguments")
    expect_branch(image,unbind+4,symbols["nativeresponse_cancelremote"],True,ARM_COND_AL,"cancel local delivery")
    expect_word(image,unbind+8,0xE8BD500F,"restore remote cleanup arguments")
    expect_word(image,unbind+12,0xE92D4070,"replay native remote cleanup prologue")
    expect_branch(image,unbind+16,0x002A3714,False,ARM_COND_AL,"native remote cleanup continuation")
    response=symbols["combinepatch_downloadresponse"]
    original=symbols["bankdataredirect_originaldownloadresponse"]
    expect_word(base_image,0x002A3954,0xE92D4070,"native download response prologue")
    expect_branch(image,response+12,original,False,ARM_COND_NE,"nonlocal download response stays native")
    expect_branch(image,response+20,symbols["nativeresponse_downloadresponse"],False,ARM_COND_AL,"local download cancel ownership")
    expect_word(image,original,0xE92D4070,"download response native prologue replay")
    expect_branch(image,original+4,0x002A3958,False,ARM_COND_AL,"unchanged native download response")
    if image[image_offset(0x002A3958):image_offset(0x002A3A68)] != base_image[image_offset(0x002A3958):image_offset(0x002A3A68)]:
        raise ValueError("download observer/result/consumer/destructor body was modified")
    campaign=symbols["combinepatch_initialcampaignrequest"]
    expect_branch(base_image,0x002ACEB4,0x001D5A14,True,ARM_COND_AL,"native campaign request")
    expect_word(image,campaign,0xE92D401F,"preserve native campaign request arguments")
    expect_word(image,campaign+16,0x03A00001,"select configured offline tickets")
    expect_word(image,campaign+20,0x13A00000,"select online ticket scope")
    expect_branch(image,campaign+32,symbols["fakeserverclient_selectticket"],True,ARM_COND_AL,"select current ticket scope")
    expect_word(image,campaign+40,0xE8BD401F,"restore native campaign request arguments")
    expect_word(image,campaign+48,0x13A00000,"accept local resource request")
    expect_word(image,campaign+52,0x112FFF1E,"return to native resource wait")
    expect_branch(image,campaign+56,0x001D5A14,False,ARM_COND_AL,"native BOSS request")
    for start,end in ((0x002A2F68,0x002A3424),(0x002ACBDC,0x002ACF10)):
        expected=bytearray(base_image[image_offset(start):image_offset(end)])
        for address,*_ in hook_branches:
            if start<=address<end:
                expected[address-start:address-start+4]=image[image_offset(address):image_offset(address+4)]
        if image[image_offset(start):image_offset(end)]!=expected:
            raise ValueError("native initial record/query body changed outside transport hooks")
    for start,end in ((0x002ACF10,0x002AD190),(0x002D0E04,0x002D0F20)):
        if image[image_offset(start):image_offset(end)]!=base_image[image_offset(start):image_offset(end)]:
            raise ValueError("native initial record consumers/campaign parser/cleanup changed")

    save_wrapper = symbols["combinepatch_savestateupdate"]
    mode_load = read_word(image, save_wrapper)
    if mode_load & 0xFFFFF000 != 0xE59FC000:
        raise ValueError("source-save completion wrapper lacks mode selection")
    expect_word(image, save_wrapper + 8 + (mode_load & 0xFFF),
                symbols["combinepatch_modestorage"], "source-save completion mode")
    expect_word(image, save_wrapper + 4, 0xE5DCC000, "source-save mode byte")
    expect_word(image, save_wrapper + 8, 0xE35C0000, "offline-only completion guard")
    expect_branch(image, save_wrapper + 12, symbols["bankdataredirect_saveupdate"],
                  False, ARM_COND_EQ, "offline source completion accounting")
    native_save = symbols["bankdataredirect_originalsaveupdate"]
    if native_save != save_wrapper + 16:
        raise ValueError("online modes do not fall through to the native save trampoline")
    expect_word(base_image, 0x002B1CF8, 0xE92D4070, "native save prologue")
    expect_word(image, native_save, 0xE92D4070, "native save prologue replay")
    expect_branch(image, native_save + 4, 0x002B1CFC,
                  False, ARM_COND_AL, "native save continuation")
    save_start, save_end = 0x002B1CF8, 0x002B24B4
    expected_save = bytearray(base_image[image_offset(save_start):image_offset(save_end)])
    save_hooks = [address for address, *_ in hook_branches if save_start <= address < save_end]
    for address in save_hooks:
        expected_save[address - save_start:address - save_start + 4] = \
            image[image_offset(address):image_offset(address + 4)]
    if image[image_offset(save_start):image_offset(save_end)] != expected_save:
        raise ValueError("native save/source-cleanup body changed outside checked hooks")

    for start, end, bridge, local, prologue in (
        (0x001DF3E0, 0x001DF5D0, "combinepatch_remotebind", "bankdataredirect_bindremote", 0xE92D4070),
    ):
        if image[image_offset(start + 4):image_offset(end)] != base_image[image_offset(start + 4):image_offset(end)]:
            raise ValueError(f"native remote body changed beyond entry at {start:08X}")
        wrapper = symbols[bridge]
        expect_branch(image, wrapper + 12, symbols[local], False, ARM_COND_EQ, "offline request dispatch")
        expect_word(base_image, start, prologue, "original remote prologue")
        expect_word(image, wrapper + 16, prologue, "native remote prologue replay")
        expect_branch(image, wrapper + 20, start + 4, False, ARM_COND_AL, "native remote continuation")

    for address, target_symbol, name in (
        (0x0015DC00, "turtlestorage_loadatonce", "base Turtle load backend"),
        (0x0015DC40, "turtlestorage_saveatonce", "base Turtle save backend"),
        (0x002A484C, "turtlestorage_checkarchivestatus", "base title Turtle check"),
        (0x002AC578, "turtlestorage_checkarchivestatus", "base initial Turtle check"),
        (0x002AC5A0, "gamedata_loadturtle", "base initial Turtle load"),
        (0x002AC678, "turtlestorage_formatstart", "base Turtle format start"),
        (0x002AC68C, "turtlestorage_formatpoll", "base Turtle format poll"),
    ):
        expect_branch(base_image, address, symbols[target_symbol], True, ARM_COND_AL, name)

    for name, local_target, native_target, caller in (
        ("check", "turtleredirect_initialcheckpoll", "turtlestorage_checkarchivestatus", 0x002AC578),
        ("load", "turtleredirect_initialloadpoll", "gamedata_loadturtle", 0x002AC5A0),
    ):
        wrapper=symbols[f"combinepatch_initialturtle{name}"]
        if not EXPANDED_PAYLOAD_START <= wrapper < symbols["combinepatch_codeusedend"]:
            raise ValueError(f"initial settings {name} outside executable pages")
        word=read_word(image,wrapper)
        if word & 0xFFFFF000 != 0xE59FC000:
            raise ValueError(f"initial settings {name} lacks mode literal")
        expect_word(image,wrapper+8+(word&0xFFF),RUNTIME_STORAGE_START,f"initial settings {name} mode storage")
        for offset,expected in ((4,0xE5DCC000),(8,0xE35C0000),(16,0xE1A02001),
                                (20,0xE1A01000),(24,0xE1A00004),(32,0xE3700001)):
            expect_word(image,wrapper+offset,expected,f"initial settings {name} argument/wait test")
        expect_branch(image,wrapper+12,wrapper+44,False,ARM_COND_NE,f"initial settings {name} native mode")
        expect_branch(image,wrapper+28,symbols[local_target],True,ARM_COND_AL,f"initial settings {name} local poll")
        expect_branch(image,wrapper+36,0x002ACA98,False,ARM_COND_EQ,f"initial settings {name} native frame wait")
        expect_branch(image,wrapper+40,caller+4,False,ARM_COND_AL,f"initial settings {name} local result")
        expect_branch(image,wrapper+44,symbols[native_target],True,ARM_COND_AL,f"initial settings {name} original call")
        expect_branch(image,wrapper+48,caller+4,False,ARM_COND_AL,f"initial settings {name} native result")
    initial_destruct=symbols["combinepatch_initialturtledestruct"]
    expect_word(base_image,0x002ACB10,0xE59F1018,"native initial destructor vtable load")
    expect_word(image,initial_destruct,0xE92D4001,"initial destructor saves owner/LR")
    expect_branch(image,initial_destruct+4,symbols["turtleredirect_cancelpreparesession"],
                  True,ARM_COND_AL,"cancel initial preview/import delivery")
    expect_word(image,initial_destruct+8,0xE8BD4001,"initial destructor restores owner/LR")
    word=read_word(image,initial_destruct+12)
    if word & 0xFFFFF000 != 0xE59F1000:
        raise ValueError("initial destructor lacks original vtable literal")
    expect_word(image,initial_destruct+20+(word&0xFFF),0x00361AB8,"initial destructor original vtable")
    expect_branch(image,initial_destruct+16,0x002ACB14,False,ARM_COND_AL,"initial destructor native continuation")
    for start,end in ((0x002ABE08,0x002ABED0),(0x002AC57C,0x002AC598),
                       (0x002AC5A4,0x002AC670),(0x002ACA98,0x002ACAA4),
                       (0x002ACB14,0x002ACB30),(0x0015DBF0,0x0015DC00),
                       (0x0015DC04,0x0015DC34)):
        expect_bytes(image,start,base_image[image_offset(start):image_offset(end)],
                     "initial settings native dispatch/decoder/flags/results preserved")

    if symbols["combinepatch_modecount"] != 4 or symbols["combinepatch_modeoriginal"] != 3:
        raise ValueError("title selector must include Original after Unlock")
    if symbols["combinepatch_initiallanguagependingoffset"] != 5:
        raise ValueError("backend language latch overlaps mode or ticket flags")
    for name,value in (("ready",0),("missing",1),("failed",2)):
        if symbols[f"combinepatch_settings{name}"]!=value:
            raise ValueError("settings outcome must distinguish absence from failure")
    expect_word(base_image,0x002A5638,0xE5D0001B,"native bootstrap settings flag read")
    expect_word(image, symbols["combinepatch_titlemodetextinitialize"] + 32,
                0xE5DC3003, "restore retained title selection")
    if symbols["combinepatch_titlelanguageoffset"] != 7:
        raise ValueError("title language flag overlaps existing runtime storage")
    expect_bytes(image,0x002D139C,
                 base_image[image_offset(0x002D139C):image_offset(0x002D1448)],
                 "native title-confirm animation/result callback unchanged")
    for operation, local_target, native_target in (
        ("load", "turtleredirect_loadbackend", "turtlestorage_loadatonce"),
        ("save", "turtleredirect_savebackend", "turtlestorage_saveatonce"),
        ("format", "turtleredirect_formatbackend", "turtlestorage_formatstart"),
        ("formatpoll", "turtleredirect_formatpoll", "turtlestorage_formatpoll"),
        ("statesavebegin", "turtleredirect_statesavebegin", "turtleredirect_originalstatesavebegin"),
        ("statesavepoll", "turtleredirect_statesavepoll", "turtleredirect_originalstatesavepoll"),
    ):
        wrapper = symbols[f"combinepatch_local{operation[5:]}"] if operation.startswith("state") else symbols[f"combinepatch_turtle{operation}"]
        if not EXPANDED_PAYLOAD_START <= wrapper < symbols["combinepatch_codeusedend"]:
            raise ValueError(f"Turtle {operation} wrapper outside added pages")
        word = read_word(image, wrapper)
        if word & 0xFFFFF000 != 0xE59FC000:
            raise ValueError(f"Turtle {operation} lacks session-mode literal")
        expect_word(image, wrapper + 8 + (word & 0xFFF), RUNTIME_STORAGE_START,
                    f"Turtle {operation} session storage")
        expect_word(image, wrapper + 4, 0xE5DCC000, f"Turtle {operation} mode load")
        expect_word(image, wrapper + 8, 0xE35C0000, f"Turtle {operation} offline test")
        if operation in ("load","save"):
            expect_branch(image,wrapper+12,symbols[native_target],False,ARM_COND_NE,
                          f"Turtle {operation} native session")
            expect_word(image,wrapper+20,0xE5DCC007,"global language operation byte")
            expect_word(image,wrapper+24,0xE35C0001,"global language action test")
            expect_branch(image,wrapper+28,symbols[native_target],False,ARM_COND_EQ,
                          f"Turtle {operation} language always uses native settings")
            expect_branch(image,wrapper+32,symbols[local_target],False,ARM_COND_AL,
                          f"Turtle {operation} ordinary offline route")
        else:
            expect_branch(image, wrapper + 12, symbols[local_target], False, ARM_COND_EQ,
                          f"Turtle {operation} offline route")
            expect_branch(image, wrapper + 16, symbols[native_target], False, ARM_COND_AL,
                          f"Turtle {operation} native route")
    destructor=symbols["combinepatch_languagereturndestruct"]
    expect_branch(image,0x002ABDEC,destructor,False,ARM_COND_AL,"cancel language synchronization on destruction")
    expect_branch(image,destructor+4,symbols["languagesettings_cancel"],True,ARM_COND_AL,"cancel owned language request")
    expect_branch(image,destructor+16,0x002ABDF0,False,ARM_COND_AL,"native return-state destructor")
    expect_bytes(image,0x002ABDF0,base_image[image_offset(0x002ABDF0):image_offset(0x002ABE08)],
                 "native return-state destructor body unchanged")
    for start, end, name in ((0x002B4A20, 0x002B4AB4, "poll"),
                             (0x002B4AB4, 0x002B4B00, "begin")):
        trampoline = symbols[f"turtleredirect_originalstatesave{name}"]
        expect_word(base_image, start, 0xE92D4070, f"native settings {name} prologue")
        expect_word(image, trampoline, 0xE92D4070, f"settings {name} prologue replay")
        expect_branch(image, trampoline + 4, start + 4, False, ARM_COND_AL,
                      f"settings {name} native continuation")
        expect_bytes(image, start + 4, base_image[image_offset(start + 4):image_offset(end)],
                     f"native settings {name} body preserved")
    release = symbols["combinepatch_titlereleasewaitingview"]
    expect_word(base_image, 0x002B1B98, 0xE595402C, "native title exit UI root load")
    expect_word(image, release, 0xE595402C, "title exit UI root load")
    expect_word(image, release + 8, 0xE5DCC007, "title language flag")
    expect_word(image, release + 12, 0xE35C0001, "only L changes language")
    expect_branch(image, release + 16, 0x002B1B9C, False, ARM_COND_NE,
                  "ordinary START retains cached prompt")
    expect_word(image, release + 20, 0xE1A00004, "cached prompt release argument")
    expect_branch(image, release + 24, symbols["bankui_releasewaitingview"],
                  True, ARM_COND_AL, "native asynchronous prompt release")
    expect_word(image, release + 28, 0xE3500000, "wait for cached prompt release")
    expect_branch(image, release + 32, 0x002B1BE4, False, ARM_COND_EQ,
                  "pending release keeps title view alive")
    expect_branch(image, release + 36, 0x002B1B9C, False, ARM_COND_AL,
                  "released prompt resumes native title destruction")
    for begin, end, name in (
        (0x002A58B4, 0x002A58BC, "native post-selection state return"),
        (0x001D6A34, 0x001D6AAC, "native prompt release lifecycle"),
        (0x002B1B60, 0x002B1B98, "native title exit animation wait"),
        (0x002B1B9C, 0x002B1BDC, "native title view destruction"),
        (0x002B1CD8, 0x002B1CF8, "native title destructor body"),
    ):
        expect_bytes(image, begin, base_image[image_offset(begin):image_offset(end)], name)
    for address in (0x002B1BD8, 0x002B1BE0, 0x002B1BF0, 0x002B1C28):
        expect_word(image, address, read_word(base_image, address), "stock title lifecycle")
    exit_entry=symbols["combinepatch_titleexit"]
    expect_word(base_image,0x002B1B5C,0xE92D4070,"native title Exit prologue")
    expect_word(image,exit_entry,0xE92D4070,"replayed title Exit prologue")
    expect_word(image,exit_entry+4,0xE1A05000,"original title Exit state register")
    expect_branch(image,exit_entry+8,symbols["turtleredirect_preparesessionpending"],
                  True,ARM_COND_AL,"check already released title wait")
    expect_branch(image,exit_entry+16,symbols["combinepatch_titlebindsession"],
                  False,ARM_COND_NE,"pending rebind skips freed title UI")
    expect_branch(image,exit_entry+24,0x002B1B64,False,ARM_COND_AL,
                  "first Exit keeps original animation/release body")
    destructor=symbols["combinepatch_titledestruct"]
    expect_word(base_image,0x002B1CD4,0xE59F1018,"native title destructor vtable load")
    expect_word(image,destructor,0xE92D4001,"preserve destructor argument and return address")
    expect_word(image,destructor+8,0xE8BD4001,"restore destructor argument and return address")
    expect_word(image,destructor+12,0xE59F1000,"replay original destructor vtable load")
    expect_word(image,destructor+20,0x00361FF0,"original title vtable pointer")
    expect_branch(image,destructor+4,symbols["turtleredirect_cancelpreparesession"],
                  True,ARM_COND_AL,"cancel prepared settings before title destruction")
    expect_branch(image,destructor+16,0x002B1CD8,False,ARM_COND_AL,
                  "resume original title destructor")
    for start, end, hooks, name in (
        (0x002A46A4, 0x002A47F4, (0x002A46E0,), "startup helper phase/fade lifecycle"),
        (0x002A47F4, 0x002A4958, (0x002A47F4,0x002A489C), "startup heap/UI constructor lifecycle"),
        (0x002A4958, 0x002A4974, (0x002A4958,), "startup heap destructor lifecycle"),
    ):
        expected=bytearray(base_image[image_offset(start):image_offset(end)])
        for address in hooks:
            expected[address-start:address-start+4]=image[image_offset(address):image_offset(address+4)]
        expect_bytes(image,start,bytes(expected),name)
    initializer=symbols["combinepatch_startupinitialize"]
    expect_word(base_image,0x002A47F4,0xE92D41F0,"native startup initializer stack")
    for offset,word in ((0,0xE92D41F0),(4,0xE1A04000),(8,0xE24DD010)):
        expect_word(image,initializer+offset,word,"startup initializer stack replay")
    startup_destructor=symbols["combinepatch_startupdestruct"]
    expect_word(image,startup_destructor,0xE92D4001,"preserve startup destructor argument")
    expect_branch(image,startup_destructor+4,symbols["languagesettings_cancel"],
                  True,ARM_COND_AL,"cancel startup synchronization before heap destruction")
    expect_word(image,startup_destructor+8,0xE8BD4001,"restore startup destructor argument")
    expect_word(image,startup_destructor+20,0x003616E8,"native startup helper vtable")
    expect_branch(image,startup_destructor+16,0x002A495C,False,ARM_COND_AL,
                  "resume native startup heap destructor")
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
    expect_word(image, capture + 52, 0xE5C01001, "capture status before snapshot allocation")
    expect_word(image, capture + 56, 0xE5941008, "capture download flow")
    expect_word(image, capture + 60, 0xE59110CC, "capture natively loaded bank object")
    expect_word(image, capture + 64, 0xE1A00004, "capture lifetime owner is original controller")
    expect_branch(image, capture + 68, symbols["bankdataredirect_capturedownloaded"],
                  True, ARM_COND_AL, "capture current-format object")
    expect_word(image, capture + 84, 0xE3A00003, "snapshot is pending, not a successful publication")
    expect_word(image, capture + 88, 0xE5C10001, "capture publishes only pending status")
    expect_word(image, capture_return + 16, 0xE3560000, "replay native post-load flag comparison")
    callback_start, callback_end = image_offset(0x002D11B0), image_offset(0x002D1318)
    callback = bytearray(image[callback_start:callback_end])
    hook_offset = image_offset(0x002D1248) - callback_start
    callback[hook_offset:hook_offset + 4] = base_image[
        image_offset(0x002D1248):image_offset(0x002D124C)
    ]
    if callback != base_image[callback_start:callback_end]:
        raise ValueError("native download loading, metadata or completion processing changed")
    dispatch=symbols["combinepatch_bankdatasyncdispatch"]
    expect_branch(image,dispatch+12,symbols["bankdataredirect_downloadupdate"],
                  False,ARM_COND_EQ,"offline download uses local response")
    expect_word(image,dispatch+16,0xE35C0001,"online export belongs only to Download Mode")
    expect_branch(image,dispatch+20,symbols["bankdataredirect_exportdownloadupdate"],
                  False,ARM_COND_EQ,"download export acknowledgement waits before native update")
    sync_destructor=symbols["combinepatch_bankdatasyncdestruct"]
    expect_word(image,sync_destructor,0xE92D4001,"preserve download destructor argument")
    expect_branch(image,sync_destructor+4,symbols["bankdataredirect_canceldownloaded"],
                  True,ARM_COND_AL,"cancel snapshot/task before state destruction")
    expect_word(image,sync_destructor+8,0xE8BD4001,"restore download destructor argument")
    expect_word(image,sync_destructor+20,0x00361EC8,"native download-state vtable")
    expect_branch(image,sync_destructor+16,0x002AFE74,False,ARM_COND_AL,
                  "native download destructor resumes unchanged")
    expect_bytes(image,0x002AFE74,base_image[image_offset(0x002AFE74):image_offset(0x002AFE94)],
                 "native download-state destructor and original literal preserved")
    for start, end in ((0x002D1084, 0x002D10FC), (0x002AFE94, 0x002B0074)):
        if image[image_offset(start):image_offset(end)] != \
                base_image[image_offset(start):image_offset(end)]:
            raise ValueError("native first-create callbacks or HOME cleanup changed")

    # These gates isolate system sessions, not native SDK jobs or error results.
    # 分流的是系统会话监控，不是原 SDK 作业或错误结果。
    request_ready = symbols["combinepatch_accountrequestready"]
    for offset, word in ((0,0xE59FC010),(4,0xE5DCC000),(8,0xE35C0000),
                         (12,0x03A00001),(16,0x15D40000),(20,0xE12FFF1E)):
        expect_word(image,request_ready+offset,word,"initial request readiness without native-state writes")
    expect_word(image,request_ready+24,symbols["combinepatch_modestorage"],"initial request session mode")
    expect_bytes(image,0x00114658,base_image[image_offset(0x00114658):image_offset(0x001147A0)],
                 "real network initialization and its initialized flag remain native")
    monitor = symbols["combinepatch_networkmonitorupdate"]
    worker = symbols["combinepatch_networkmonitorworkeraccount"]
    for entry in (monitor, worker):
        expect_word(image, entry, 0xE59FC010, "monitor mode literal load")
        expect_word(image, entry + 4, 0xE5DCC000, "latched session mode")
        expect_word(image, entry + 8, 0xE35C0000, "Offline-only monitor gate")
        expect_word(image, entry + 24, symbols["combinepatch_modestorage"], "monitor mode storage")
    expect_word(image, monitor + 12, 0x012FFF1E, "Offline monitor returns without system calls")
    expect_word(image, monitor + 16, 0xE92D4070, "native monitor prologue")
    expect_branch(image, monitor + 20, 0x0010D680, False, ARM_COND_AL, "native monitor continuation")
    expect_word(image, worker + 12, 0x03A04000, "Offline worker omits only the network account")
    expect_word(image, worker + 16, 0x15964028, "online worker retains the native account")
    expect_word(image, worker + 20, 0xE12FFF1E, "worker returns to its native loop")
    for start, end, site in ((0x0010D67C, 0x0010D810, 0x0010D67C),
                            (0x0029D908, 0x0029DB88, 0x0029D944)):
        expected = bytearray(base_image[image_offset(start):image_offset(end)])
        expected[site-start:site-start+4] = image[image_offset(site):image_offset(site+4)]
        if image[image_offset(start):image_offset(end)] != expected:
            raise ValueError("native connection monitoring/lifecycle changed outside its mode gate")

    for start, end in ((0x002A2504,0x002A26AC),(0x002A26AC,0x002A27C8),
                       (0x001D5D74,0x001D5EC0),(0x001D5C28,0x001D5D74)):
        expected = bytearray(base_image[image_offset(start):image_offset(end)])
        for address, *_ in hook_branches:
            if start <= address < end:
                expected[address-start:address-start+4] = image[image_offset(address):image_offset(address+4)]
        if image[image_offset(start):image_offset(end)] != expected:
            raise ValueError(f"native transaction SDK construction/observer changed at {start:08X}")

    force_entry = symbols["bankdataredirect_originalforcerollbackupdate"]
    expect_word(image, force_entry, 0xE92D4030, "native force rollback prologue replay")
    expect_branch(image, force_entry + 4, 0x002AD7C0, False, ARM_COND_AL,
                  "native force rollback continuation")
    expected_force = bytearray(base_image[image_offset(0x002AD7C0):image_offset(0x002ADBD4)])
    expected_force[0x16C:0x170] = image[image_offset(0x002AD92C):image_offset(0x002AD930)]
    if image[image_offset(0x002AD7C0):image_offset(0x002ADBD4)] != expected_force:
        raise ValueError("native force rollback, keyboard or finalizer changed")

    # Native trampolines preserve matching, source saves and token cleanup.
    # 原版跳板保留事务匹配、来源保存与令牌清理，不加入额外来源准入检查。
    for entry, trampoline, prologue, end in (
        (0x002A8760, "combinepatch_transactionrecoveryupdateofficial", 0xE92D40F0, 0x002A9118),
        (0x002A93F4, "bankdataredirect_originalsourcerecoveryupdate", 0xE92D4070, 0x002A9700),
        (0x002AF460, "bankdataredirect_originaldownloadupdate", 0xE92D4070, 0x002AF808),
    ):
        expect_word(base_image, entry, prologue, f"base {trampoline} prologue")
        expect_word(image, symbols[trampoline], prologue, f"{trampoline} prologue")
        expect_branch(image, symbols[trampoline] + 4, entry + 4, False, ARM_COND_AL,
                      f"{trampoline} native continuation")
        expected = bytearray(base_image[image_offset(entry + 4):image_offset(end)])
        if entry == 0x002A8760:
            # Only the selected message ID changes. Keep both confirmation
            # waits, native state transitions and destructor byte-identical.
            # 仅允许消息编号分流；原版等待、状态转移及析构仍逐字节一致。
            site = 0x002A8C98
            expected[site-entry-4:site-entry] = image[image_offset(site):image_offset(site+4)]
        if image[image_offset(entry + 4):image_offset(end)] != expected:
            raise ValueError(f"native recovery body or cleanup changed at {entry:#x}")
    expect_word(base_image, 0x002A8C98, 0xE3A0200A, "base mismatch message ID")
    selector = symbols["combinepatch_selectmismatchmessage"]
    expect_word(image, selector + 4, 0xE5DCC000, "mismatch text uses the latched session")
    expect_word(image, selector + 8, 0xE35C0000, "mismatch appendix is Offline only")
    expect_word(image, selector + 12, 0x03A02083, "Offline mismatch appendix ID")
    expect_word(image, selector + 16, 0x13A0200A, "online mismatch keeps stock ID")
    expect_word(image, selector + 20, 0xE12FFF1E, "mismatch selector returns to stock")
    expect_word(image, selector + 24, symbols["combinepatch_modestorage"], "mismatch session pointer")
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

    # First-use still executes the original setup, messages, response and wait.
    for start, end in ((0x002AE5C0, 0x002AE5C8), (0x002AE74C, 0x002AE754),
                       (0x002AE85C, 0x002AE86C)):
        expect_bytes(image, start, base_image[start - IMAGE_BASE:end - IMAGE_BASE],
                     "native first-use body")
    expect_word(base_image, 0x002B2548, 0xE3A01008, "base save message")

    expect_word(image,symbols["combinepatch_homeresult"]+16,0xE3A00014,
                "disabled HOME cannot enter language state")
    expect_branch(image, symbols["combinepatch_homeresult"] + 12,
                  0x002A5778, False, ARM_COND_EQ, "Original HOME dispatch")
    menu_callback = symbols["combinepatch_menuselectioncallback"]
    expect_word(image, menu_callback + 8, 0xE3520003, "Original menu mode check")
    expect_branch(image, menu_callback + 12, menu_callback + 44, False,
                  ARM_COND_EQ, "Original menu native callback")
    expect_word(image, menu_callback + 16, 0xE5902010, "menu state load")
    expect_word(image, menu_callback + 20, 0xE3520001, "menu state check")
    expect_branch(image,menu_callback+24,menu_callback+44,False,ARM_COND_NE,
                  "nonselection substates use native callback")
    expect_word(image, menu_callback + 28, 0xE3510003, "Mover entry check; support is not blocked")
    expect_word(image, menu_callback + 36, 0xE3510005, "HOME entry check")
    expect_branch(image,menu_callback+40,symbols["combinepatch_menuselectiondisabled"],
                  False,ARM_COND_EQ,"HOME disabled without entering language selection")
    expect_branch(
        image,
        menu_callback + 32,
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

    for address in (0x002AF390, 0x002AF394, 0x002ABD98, 0x002ABD9C,
                    0x002AE5C4, 0x002AE750):
        expect_word(image, address, read_word(base_image, address),
                    "native job construction remains intact")

    # Initialization supplies job/shared/mode; the C selector keeps the
    # native one-argument initialization ABI. Remaining interfaces use its flag.
    # 初始化传入作业／共享视图／模式；C 选择器保持原版单参数初始化 ABI。
    # 后续接口只读取该作业已经选定的后端。
    ticket_start = symbols["combinepatch_ticketinitialize"]
    ticket_end = symbols["combinepatch_ticketwrappersend"]
    if not payload_end <= ticket_start < ticket_end <= symbols["combinepatch_codeusedend"]:
        raise ValueError("ticket job wrappers exceed the added executable pages")
    policy = symbols["online_fake_server_ticket_fallback"]
    policy_address = symbols["combinepatch_onlineticketfallback"]
    if policy not in (0, 1) or policy_address != ticket_end or \
            image[image_offset(policy_address)] != policy:
        raise ValueError("ticket fallback policy must be one byte: 0 native, 1 auto-server")
    environment_start = symbols["runtimeenvironment_storagestart"]
    if environment_start != RUNTIME_STORAGE_START - 0x10 or \
            symbols["runtimeenvironment_storageend"] != RUNTIME_STORAGE_START or \
            environment_start < BSS_ACTUAL_END:
        raise ValueError("runtime capability cache overlaps native BSS or mode storage")
    for name in ("runtimeenvironment_get", "runtimeenvironment_initializeticket",
                 "runtimeenvironment_useserverticket"):
        if not symbols["codeexpansion_loaderbegin"] <= symbols[name] < symbols["codeexpansion_loaderend"]:
            raise ValueError("runtime capability code must remain in executable loader padding")
    backend = symbols["serverticket_backendstorage"]
    if backend != RUNTIME_STORAGE_START + 4:
        raise ValueError("ticket backend flag overlaps native or mode storage")
    if symbols["srv_getservicehandle"] != 0x0023514C:
        raise ValueError("Bank SRV entry point does not match its native image")
    selector = symbols["serverticket_initializeselected"]
    if not symbols["serverticket_payloadbegin"] <= selector < symbols["serverticket_payloadend"]:
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
    for name in ("send","providerbind","providerunbind"):
        wrapper=symbols[f"combinepatch_ticket{name}"]
        instruction=read_word(image,wrapper)
        if instruction & 0xFFFFF000 != 0xE59FC000:
            raise ValueError("ticket transport lacks backend pointer")
        expect_word(image,wrapper+8+(instruction&0xFFF),backend,"ticket backend literal")
        expect_word(image,wrapper+4,0xE5DCC000,"ticket backend byte")
        expect_word(image,wrapper+8,0xE35C0000,"ticket backend test")
        if name=="send":
            expect_branch(image,wrapper+12,symbols["serverticket_send"],False,ARM_COND_NE,"service ticket request")
            expect_branch(image,wrapper+16,0x0021EB10,False,ARM_COND_AL,"native ticket transport")
        elif name=="providerunbind":
            expect_branch(image,wrapper+12,symbols["serverticket_cancelprovider"],False,ARM_COND_NE,"cancel local ticket requests")
            expect_word(image,wrapper+16,0xE12FFF11,"native SDK unbind virtual call")
        else:
            expect_word(image,wrapper+12,0x13A00001,"local SDK provider acknowledgement")
            expect_word(image,wrapper+16,0x112FFF1E,"local SDK provider return")
            expect_word(image,wrapper+20,0xE12FFF12 if name=="providerbind" else 0xE12FFF11,"native SDK provider call")
    for address in TICKET_SEND_CALLS:
        expect_branch(base_image,address,0x0021EB10,True,ARM_COND_AL,"original EC send")
        expect_branch(image,address,symbols["combinepatch_ticketsend"],True,ARM_COND_AL,"local EC send hook")
    for address,name in ((0x002A0518,"providerbind"),(0x002A20E8,"providerunbind")):
        expect_branch(image,address,symbols[f"combinepatch_ticket{name}"],True,ARM_COND_AL,"SDK provider binding")
    for address,label,local,original,instruction in (
        (0x002A1CAC,"jobpoll","serverticket_poll","serverticket_originalpoll",0xE92D4070),
        (0x002A05F8,"creditpoll","serverticket_pollcredit","serverticket_originalcredit",0xE5900004)):
        wrapper=symbols["combinepatch_ticket"+label]
        expect_branch(image,address,wrapper,False,ARM_COND_AL,"ticket wait adapter")
        expect_word(base_image,address,instruction,"native ticket wait prologue")
        expect_word(image,wrapper+4,0xE5DCC000,"ticket wait backend byte")
        expect_word(image,wrapper+8,0xE35C0000,"ticket wait backend test")
        expect_branch(image,wrapper+12,symbols[local],False,ARM_COND_NE,"local ticket wait")
        if symbols[original]!=wrapper+16:
            raise ValueError("native ticket wait trampoline moved")
        expect_word(image,wrapper+16,instruction,"native ticket wait prologue replay")
        expect_branch(image,wrapper+20,address+4,False,ARM_COND_AL,"native ticket wait continuation")
    wrapper=symbols["combinepatch_ticketexpiryrequest"]
    expect_word(image,wrapper+4,0xE5DCC000,"expiry backend byte")
    expect_word(image,wrapper+8,0xE35C0000,"expiry backend test")
    expect_word(image,wrapper+12,0x13A03000,"local expiry selects existing async mode")
    expect_branch(image,wrapper+16,0x001D6FB8,False,ARM_COND_AL,"native expiry helper")
    for address in TICKET_EXPIRY_CALLS:
        expect_branch(base_image,address,0x001D6FB8,True,ARM_COND_AL,"original expiry send")
        expect_branch(image,address,wrapper,True,ARM_COND_AL,"backend-aware expiry send")
    for start,end in ((0x001D6FB8,0x001D70B4),(0x002A1260,0x002A1744),(0x002A0ADC,0x002A0B78)):
        if image[image_offset(start):image_offset(end)]!=base_image[image_offset(start):image_offset(end)]:
            raise ValueError("original synchronous helpers changed instead of adapting their callers")
    for address,name,native,link in TICKET_SYSTEM_HOOKS:
        wrapper=symbols[f"combinepatch_ticket{name}"]
        expect_branch(base_image,address,native,link,ARM_COND_AL,"native system shop call")
        expect_branch(image,address,wrapper,link,ARM_COND_AL,"local system shop adapter")
        expect_word(image,wrapper+4,0xE5DCC000,"shop backend byte")
        expect_word(image,wrapper+8,0xE35C0000,"shop backend test")
        expect_branch(image,wrapper+12,native,False,ARM_COND_EQ,"native system shop continuation")
        entry=wrapper+16
        if name=="shoprequest":
            expect_word(image,entry,0xE1A00004,"initial shop native job")
            entry+=4
        elif name=="purchaserequest":
            expect_word(image,entry,0xE2440F52,"purchase native job from saved product block")
            entry+=4
        target="serverticket_requestapplet" if name.endswith("request") else "serverticket_"+name
        instruction=read_word(image,entry)
        if instruction & 0xFFFFF000 != 0xE59FC000:
            raise ValueError("local shop adapter lacks a Thumb entry pointer")
        literal=entry+8+(instruction&0xFFF)
        expect_word(image,literal,symbols[target]+1,"local shop ARM/Thumb interworking")
        expect_word(image,entry+4,0xE12FFF1C,"local shop adapter interworking branch")
    worker=symbols["combinepatch_ticketecworkerconstruct"]
    for address in (0x002A0494,0x002A2018):
        expect_word(base_image,address-4,0xE3A02A01,"native EC worker requests 4 KiB")
        expect_word(image,address-4,0xE3A02A01,"native EC worker size argument retained")
        expect_branch(base_image,address,0x0022DFD0,True,ARM_COND_AL,"native EC worker constructor")
        expect_branch(image,address,worker,True,ARM_COND_AL,"local EC worker stack adapter")
    instruction=read_word(image,worker)
    if instruction & 0xFFFFF000 != 0xE59FC000:
        raise ValueError("EC worker stack adapter lacks its backend pointer")
    expect_word(image,worker+8+(instruction&0xFFF),backend,"EC worker backend literal")
    expect_word(image,worker+4,0xE5DCC000,"EC worker backend byte")
    expect_word(image,worker+8,0xE35C0000,"EC worker backend test")
    expect_word(image,worker+12,0x13A02801,"local EC worker requests 64 KiB only")
    expect_branch(image,worker+16,0x0022DFD0,False,ARM_COND_AL,"original SDK thread construction and ownership")
    for start,end in ((0x002A0468,0x002A04C0),(0x0022DFD0,0x0022E178),
                      (0x002A2048,0x002A20B0),(0x002A4694,0x002A46B0)):
        expected=bytearray(base_image[image_offset(start):image_offset(end)])
        for address in (0x002A0494,):
            if start<=address<end:
                expected[address-start:address-start+4]=image[image_offset(address):image_offset(address)+4]
        if image[image_offset(start):image_offset(end)]!=expected:
            raise ValueError("native EC worker construction/wait/destruction changed outside its stack adapter")
    credit=symbols["combinepatch_ticketcreditrequest"]
    purchase_poll=symbols["combinepatch_ticketpurchasepoll"]
    instruction=read_word(image,purchase_poll)
    if instruction & 0xFFFFF000 != 0xE59FC000:
        raise ValueError("purchase poll adapter lacks its backend pointer")
    expect_word(image,purchase_poll+8+(instruction&0xFFF),backend,"purchase poll backend literal")
    expect_word(image,purchase_poll+4,0xE5DCC000,"purchase poll backend byte")
    expect_word(image,purchase_poll+8,0xE35C0000,"purchase poll backend test")
    expect_branch(image,purchase_poll+12,0x002A0ADC,False,ARM_COND_EQ,"native purchase polling unchanged")
    expect_word(image,purchase_poll+16,0xE1A02004,"local purchase controller argument")
    instruction=read_word(image,purchase_poll+20)
    if instruction & 0xFFFFF000 != 0xE59FC000:
        raise ValueError("purchase poll adapter lacks its Thumb entry pointer")
    expect_word(image,purchase_poll+28+(instruction&0xFFF),symbols["serverticket_pollpurchase"]+1,"local purchase prompt interworking")
    expect_word(image,purchase_poll+24,0xE12FFF1C,"local purchase poll branch")
    expect_branch(image,0x002A0560,credit,False,ARM_COND_AL,"credit applet boundary")
    expect_word(image,credit+12,0x03A01000,"native credit prologue replay")
    expect_branch(image,credit+16,0x002A0564,False,ARM_COND_EQ,"native credit request continuation")
    for start,end in ((0x002A04C0,0x002A22D0),(0x001697F4,0x00169E3C),(0x002A4594,0x002A4694)):
        expected=bytearray(base_image[image_offset(start):image_offset(end)])
        for address in (*TICKET_SEND_CALLS,*TICKET_LOCAL_JOB_HOOKS):
            if start<=address<end:
                expected[address-start:address-start+4]=image[image_offset(address):image_offset(address)+4]
        if image[image_offset(start):image_offset(end)]!=expected:
            raise ValueError("native ticket construction, polling, parsers or destruction changed")

    if apply_bps(base_image, bps.read_bytes()) != image:
        raise ValueError("release code.bps does not reproduce the patched code image")


def verify_messages(source_romfs: Path, output_romfs: Path, *, patch_label: str | None = None) -> None:
    from tools.title_layout import TITLE_LAYOUT_ARCHIVE, patch_title_archive
    relative = Path("a") / TITLE_LAYOUT_ARCHIVE
    if (output_romfs / relative).read_bytes() != patch_title_archive((source_romfs / relative).read_bytes(), patch_label=patch_label):
        raise ValueError("title language layout differs from its verified builder")
    from title_font import patch_title_fonts
    relative=Path("a/0/2/3")
    if (output_romfs/relative).read_bytes()!=patch_title_fonts((source_romfs/relative).read_bytes()):
        raise ValueError("cross-language name/arrow fonts differ from their verified builder")
    module = load_module("patch_messages_verify", Path(__file__).resolve().parents[1] / "src" / "patch_messages.py")
    module.verify_force_unlock_layout(source_romfs)
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
        expected_mismatch = module.build_offline_mismatch_values(
            module.message_codec.read_message_line_values(source_message_file, module.MISMATCH_SOURCE_LINE),
            module.OFFLINE_MISMATCH_MESSAGES[archive],
        )
        if module.message_codec.read_message_line_values(output_message_file, module.OFFLINE_MISMATCH_LINE) != expected_mismatch:
            raise ValueError(f"offline mismatch appendix differs in archive {archive}")
        no_game_end = len(no_game_values)
        while no_game_end and no_game_values[no_game_end - 1] == 0:
            no_game_end -= 1
        if no_game_values[no_game_end - 3:no_game_end] != [0x0010, 0x0001, 0xBE01]:
            raise ValueError(f"no-game final confirmation wait missing in archive {archive}")
        expected_title_mode_offline = module.TITLE_MODE_OFFLINE_MESSAGES[archive]
        expected_title_mode_download = module.TITLE_MODE_DOWNLOAD_MESSAGES[archive]
        expected_title_mode_unlock = module.TITLE_MODE_UNLOCK_MESSAGES[archive]
        expected_title_mode_original = module.TITLE_MODE_ORIGINAL_MESSAGES[archive]
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
            module.TITLE_SWITCH_LINE: module.TITLE_SWITCH_MESSAGES[archive],
            module.TITLE_USER_LABEL_LINE: module.USER_LABEL_MESSAGES[archive],
            module.TITLE_USER_HINT_LINE: module.USER_SWITCH_HINTS[archive],
            module.RESERVED_DISABLED_LINE: "",
            module.RESERVED_SUPPORT_DISABLED_LINE: "",
            module.RESERVED_LANGUAGE_MENU_LINE: "",
            module.UNLOCK_USE_BANK_LINE: module.UNLOCK_MENU_MESSAGES[archive],
            module.UNLOCK_GAME_SELECTION_LINE: module.UNLOCK_GAME_SELECTION_MESSAGES[archive],
            module.UNLOCK_NO_RECOVERY_LINE: module.UNLOCK_NO_RECOVERY_MESSAGES[archive],
            module.UNLOCK_RECOVERED_LINE: module.UNLOCK_RECOVERED_MESSAGES[archive],
            module.UNLOCK_FORCED_RECOVERED_LINE: module.UNLOCK_FORCED_RECOVERED_MESSAGES[archive],
            module.TICKET_INSUFFICIENT_FUNDS_LINE: module.TICKET_INSUFFICIENT_FUNDS_MESSAGES[archive],
        }
        expected_lines.update(module.build_disabled_menu_labels(archive, source_lines))
        expected_lines[module.INVALID_SUPPORT_LINE]=module.build_invalid_support_label(archive, source_lines)
        for line, expected in expected_lines.items():
            if output_lines[line] != expected:
                raise ValueError(f"message line {line} mismatch in archive {archive}")
        source_portrait=source_entries[module.MENU_MESSAGE_FILE_INDEX].files[0]
        output_portrait=output_entries[module.MENU_MESSAGE_FILE_INDEX].files[0]
        source_portrait_lines=module.message_codec.read_message_lines(source_portrait)
        source_greeting=source_portrait_lines[module.MENU_GREETING_LINE]
        output_greetings = module.message_codec.read_message_lines(
            output_portrait
        )
        for line in range(len(source_portrait_lines)):
            if module.message_codec.read_message_line_values(source_portrait,line)!= \
                    module.message_codec.read_message_line_values(output_portrait,line):
                raise ValueError(f"native portrait/mileage table37 line {line} changed in {archive}")
        if len(source_portrait_lines)!=27 or len(output_greetings)!=30 or not output_greetings[14]:
            raise ValueError(f"portrait/mileage table37 bounds/line14 differ in {archive}")
        if output_lines[module.TITLE_LANGUAGE_LINE] != module.TITLE_LANGUAGE_MESSAGES[archive]:
            raise ValueError(f"title language hint mismatch in {archive}")
        if tuple(output_lines[132:135])!=module.storage_notice_lines(archive):
            raise ValueError(f"SD notice translations differ in {archive}")
        if len(output_lines)!=135 or module.OFFLINE_MISMATCH_LINE!=131 or module.TICKET_INSUFFICIENT_FUNDS_LINE!=121 or \
                module.TITLE_LANGUAGE_LINE!=122 or module.TITLE_SWITCH_LINE!=127 or \
                hasattr(module,"TRANSACTION_MESSAGES") or hasattr(module,"TRANSACTION_MESSAGE_BASE"):
            raise ValueError(f"generic table39 bounds differ in {archive}")
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
    add_version_arguments(parser)
    args = parser.parse_args()
    subprocess.run([sys.executable, "-B", str(Path(__file__).absolute().parents[2] /
                    "common/tools/check_file_links.py"), "--recursive",
                    *(str(path) for path in vars(args).values() if isinstance(path, Path))], check=True)
    patch_label = label_from_arguments(args)
    verify_code(args.base, args.patched, args.symbols, args.bps,
                args.base_exheader, args.exheader, patch_label=patch_label)
    verify_messages(args.source_romfs, args.output_romfs, patch_label=patch_label)
    print("combined patch static verification passed")


if __name__ == "__main__":
    main()
