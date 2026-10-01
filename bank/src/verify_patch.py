"""Statically verify the built combined Bank patch and its LayeredFS resources.

静态校验已构建的 Bank 合并补丁及其 LayeredFS 资源。
"""

from __future__ import annotations

import argparse
import hashlib
import importlib.util
import sys
from pathlib import Path


IMAGE_BASE = 0x00100000
EXPECTED_BASE_SHA256 = "2DCE4796F54807CF8A67F1CE6297BF472D969B30ED7A7E8E25C2A6C2BDC40ABF"
OPTIONAL_REWARD_STATE = 0x002B0270
OPTIONAL_REWARD_BODY_END = 0x002B14E0
HOME_CAVE_START = 0x002A7BF0
HOME_CAVE_END = 0x002A8760
TAIL_CAVE_START = 0x00313A40
TEXT_MAPPED_END = 0x00314000
VERSION_STORAGE_SIZE = 0x40
VERSION_STORAGE_START = TEXT_MAPPED_END - VERSION_STORAGE_SIZE
TAIL_CAVE_END = VERSION_STORAGE_START
VERSION_IDENTIFIER = b"offline_patch_v0.9.0\0"
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


def apply_ips(base: bytes, ips_bytes: bytes) -> bytes:
    """Apply a complete IPS stream to a same-size code image.

    将完整 IPS 流应用于等长度代码镜像。
    """
    if not ips_bytes.startswith(b"PATCH") or not ips_bytes.endswith(b"EOF"):
        raise ValueError("release code.ips is not a complete IPS patch")
    output = bytearray(base)
    cursor = len(b"PATCH")
    while True:
        if cursor + 3 > len(ips_bytes):
            raise ValueError("IPS stream ends before EOF")
        if ips_bytes[cursor : cursor + 3] == b"EOF":
            cursor += 3
            if cursor != len(ips_bytes):
                raise ValueError("unexpected IPS truncate record for a same-size code image")
            return bytes(output)
        offset = int.from_bytes(ips_bytes[cursor : cursor + 3], "big")
        cursor += 3
        if cursor + 2 > len(ips_bytes):
            raise ValueError("IPS stream ends in record length")
        length = int.from_bytes(ips_bytes[cursor : cursor + 2], "big")
        cursor += 2
        if length == 0:
            if cursor + 3 > len(ips_bytes):
                raise ValueError("IPS stream ends in RLE record")
            length = int.from_bytes(ips_bytes[cursor : cursor + 2], "big")
            value = ips_bytes[cursor + 2]
            cursor += 3
            data = bytes([value]) * length
        else:
            if cursor + length > len(ips_bytes):
                raise ValueError("IPS stream ends in data record")
            data = ips_bytes[cursor : cursor + length]
            cursor += length
        if offset + len(data) > len(output):
            raise ValueError("IPS record writes outside the base code image")
        output[offset : offset + len(data)] = data


def verify_code(base: Path, patched: Path, symbols_path: Path, ips: Path) -> None:
    image = patched.read_bytes()
    base_image = base.read_bytes()
    base_hash = hashlib.sha256(base_image).hexdigest().upper()
    if base_hash != EXPECTED_BASE_SHA256:
        raise ValueError(
            "base code SHA-256 does not match the supported Pokemon Bank v1.5 image: "
            f"{base_hash}"
        )
    if len(image) != len(base_image):
        raise ValueError("patched code image length differs from the base image")
    symbols = read_symbols(symbols_path)
    required = {
        "combinepatch_networkavailability",
        "combinepatch_titlemodetextinitialize",
        "combinepatch_titlescreenupdate",
        "combinepatch_titleprocessmodetoggle",
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
        "combinepatch_networkupdate",
        "combinepatch_initialremoterecordupdate",
        "combinepatch_optionalrewardbypassupdate",
        "combinepatch_firstpresentdispatch",
        "combinepatch_bankdatasyncdispatch",
        "combinepatch_bankdatasyncturtlestate0",
        "combinepatch_bankdatasyncinitialize",
        "combinepatch_selectinitialconnectionmessage",
        "combinepatch_selectsavemessage",
        "combinepatch_selectpostselectionconnectionmessage",
        "combinepatch_postselectionconnectionupdate",
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
        "offlinepatch_optionalrewardbypassupdate",
        "localmileage_getcurrentdate",
        "offlinepatch_loadbankdata",
        "offlinepatch_savedisplaydelayupdate",
        "offlinepatch_createinitial",
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
    if payload_begin != OPTIONAL_REWARD_STATE + 4:
        raise ValueError("local payload no longer starts after the optional-reward entry branch")
    if not payload_begin < payload_end <= OPTIONAL_REWARD_BODY_END:
        raise ValueError("local payload exceeds the optional-reward state body")
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

    home_cave_symbols = (
        "combinepatch_networkavailability",
        "combinepatch_titlemodetextinitialize",
        "combinepatch_titlescreenupdate",
        "combinepatch_titleprocessmodetoggle",
        "combinepatch_networkskipremotejob",
        "combinepatch_networkskipremotejobofficial",
        "combinepatch_networkupdate",
        "combinepatch_initialremoterecordupdate",
        "combinepatch_optionalrewardbypassupdate",
        "combinepatch_firstpresentdispatch",
        "combinepatch_bankdatasyncdispatch",
        "combinepatch_bankdatasyncinitialize",
        "combinepatch_selectinitialconnectionmessage",
        "combinepatch_selectsavemessage",
        "combinepatch_selectpostselectionconnectionmessage",
        "combinepatch_postselectionconnectionupdate",
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
    for name in home_cave_symbols:
        address = symbols[name]
        if not HOME_CAVE_START <= address < HOME_CAVE_END:
            raise ValueError(f"{name} is outside the reclaimed HOME/Mover code cave")
    if not (
        HOME_CAVE_START
        <= symbols["turtleredirect_payloadbegin"]
        < symbols["turtleredirect_payloadend"]
        <= HOME_CAVE_END
    ):
        raise ValueError("Turtle redirect payload exceeds the reclaimed HOME/Mover code cave")

    tail_cave_symbols = (
        "combinepatch_redirecthometolanguage",
        "combinepatch_showmodegreeting",
        "combinepatch_selectgameselectionmessage",
        "combinepatch_showunlockprompt",
        "combinepatch_selectusebankmenutext",
        "combinepatch_selectdisconnectmessage",
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
    for name in tail_cave_symbols:
        address = symbols[name]
        if not TAIL_CAVE_START <= address < TAIL_CAVE_END:
            raise ValueError(f"{name} is outside the executable tail cave")

    for name in (
        "offlinepatch_networkupdate",
        "offlinepatch_postselectionconnectionupdate",
        "offlinepatch_disconnectupdate",
        "offlinepatch_initialremoterecordupdate",
        "offlinepatch_optionalrewardbypassupdate",
        "localmileage_getcurrentdate",
        "offlinepatch_loadbankdata",
        "offlinepatch_savedisplaydelayupdate",
        "offlinepatch_createinitial",
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
        (0x002AF1FC, "combinepatch_networkupdate", False, ARM_COND_AL, "network update"),
        (0x002AF37C, "combinepatch_networkavailability", True, ARM_COND_AL, "mode latch"),
        (0x002AF38C, "combinepatch_networkskipremotejob", False, ARM_COND_AL, "network remote-job bypass"),
        (0x002AF3B4, "combinepatch_selectinitialconnectionmessage", True, ARM_COND_AL, "initial connection message"),
        (0x002ACBDC, "combinepatch_initialremoterecordupdate", False, ARM_COND_AL, "initial remote record update"),
        (0x002ACE14, "combinepatch_selectpostselectionconnectionmessage", True, ARM_COND_AL, "initial-record reconnect message"),
        (OPTIONAL_REWARD_STATE, "combinepatch_optionalrewardbypassupdate", False, ARM_COND_AL, "optional reward bypass"),
        (0x002AF460, "combinepatch_bankdatasyncdispatch", False, ARM_COND_AL, "Bank data sync"),
        (0x002AF720, "combinepatch_bankdatasyncturtlestate0", False, ARM_COND_AL, "offline Turtle descriptor preservation after Bankdata load"),
        (0x002AFE50, "combinepatch_bankdatasyncinitialize", False, ARM_COND_AL, "Bank data-sync initializer"),
        (0x002A58B4, "combinepatch_selectpostselectionstate", False, ARM_COND_AL, "post-selection state"),
        (0x002A93F4, "combinepatch_postselectionconnectionupdate", False, ARM_COND_AL, "post-selection connection"),
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
        (0x002A58CC, "combinepatch_result21", False, ARM_COND_EQ, "language result"),
        (0x002A57E0, "combinepatch_result5", False, ARM_COND_EQ, "no-save result"),
        (0x002A57F0, "combinepatch_result6or12", False, ARM_COND_EQ, "result six or twelve"),
        (0x002A6C8C, "combinepatch_menuselectioncallback", False, ARM_COND_AL, "feature-menu callback"),
        (0x002A6824, "combinepatch_showmodegreeting", True, ARM_COND_AL, "first menu greeting"),
        (0x002A6978, "combinepatch_showmodegreeting", True, ARM_COND_AL, "return menu greeting"),
        (0x002ADD0C, "combinepatch_selectgameselectionmessage", True, ARM_COND_AL, "mode-specific game-selection prompt"),
        (0x002AD92C, "combinepatch_showunlockprompt", True, ARM_COND_AL, "official unlock-code prompt"),
        (0x001D6308, "combinepatch_selectusebankmenutext", True, ARM_COND_AL, "first menu label"),
        (0x001D6310, "combinepatch_selectsupportmenutext", True, ARM_COND_AL, "support menu label"),
        (0x001D6318, "combinepatch_selectinstalledmovermenutext", True, ARM_COND_AL, "installed Mover menu label"),
        (0x001D6320, "combinepatch_selecthomemenutextr6", True, ARM_COND_AL, "installed HOME menu label"),
        (0x001D63A8, "combinepatch_selectdownloadmovermenutext", True, ARM_COND_AL, "download Mover menu label"),
        (0x001D63B0, "combinepatch_selecthomemenutextr5", True, ARM_COND_AL, "download HOME menu label"),
        (0x002D11C4, "combinepatch_downloadcapturetrampoline", True, ARM_COND_AL, "ordinary Bank download callback"),
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
        (0x0015DC00, "turtleredirect_loadbackend", True, ARM_COND_AL, "Turtle load backend"),
        (0x0015DC40, "turtleredirect_savebackend", True, ARM_COND_AL, "Turtle save backend"),
        (0x002A484C, "turtleredirect_checkbackend", True, ARM_COND_AL, "title Turtle check"),
        (0x002AC578, "turtleredirect_checkbackend", True, ARM_COND_AL, "initial Turtle check"),
        (0x002AC678, "turtleredirect_formatbackend", True, ARM_COND_AL, "Turtle format start"),
        (0x002AC68C, "turtleredirect_formatpoll", True, ARM_COND_AL, "Turtle format poll"),
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

    # The following state begins the stock transaction-recovery implementation;
    # the redirect must end before it and leave its entry untouched.
    # 下一个状态是原版事务恢复实现；重定向必须在此之前结束并保持其入口不变。
    expect_word(
        image,
        HOME_CAVE_END,
        read_word(base_image, HOME_CAVE_END),
        "transaction-recovery state entry",
    )

    expect_word(base_image, 0x002B1AD0, 0xE92D40F8, "base title-state prologue")
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
        symbols["combinepatch_bankcreatesuccess"],
        0xE3A00008,
        "combined first-use success state",
    )

    expect_branch(
        image,
        symbols["combinepatch_homeresult"],
        symbols["combinepatch_redirecthometolanguage"],
        False,
        ARM_COND_AL,
        "HOME language redirect",
    )
    expect_word(image, symbols["combinepatch_menuselectioncallback"], 0xE5902010, "menu state load")
    expect_word(image, symbols["combinepatch_menuselectioncallback"] + 4, 0xE3520001, "menu state check")
    expect_word(image, symbols["combinepatch_menuselectioncallback"] + 12, 0xE3510002, "support entry check")
    expect_branch(
        image,
        symbols["combinepatch_menuselectioncallback"] + 16,
        symbols["combinepatch_menuselectiondisabled"],
        False,
        ARM_COND_EQ,
        "support entry return",
    )
    expect_word(image, symbols["combinepatch_menuselectioncallback"] + 20, 0xE3510003, "Mover entry check")
    expect_branch(
        image,
        symbols["combinepatch_menuselectioncallback"] + 24,
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

    local_target, local_link, local_condition = decode_arm_branch(
        image, symbols["combinepatch_optionalrewardbypassupdate"]
    )
    if (
        local_link
        or local_condition != ARM_COND_AL
        or local_target != symbols["offlinepatch_optionalrewardbypassupdate"]
    ):
        raise ValueError("optional-reward bypass does not enter the local implementation")

    ips_bytes = ips.read_bytes()
    if apply_ips(base_image, ips_bytes) != image:
        raise ValueError("release code.ips does not reproduce the patched code image")


def verify_messages(source_romfs: Path, output_romfs: Path) -> None:
    module = load_module("patch_messages_verify", Path(__file__).with_name("patch_messages.py"))
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
            module.OFFLINE_INITIAL_CONNECT_LINE: module.OFFLINE_INITIAL_CONNECT_MESSAGES[archive],
            module.OFFLINE_BANK_CONNECTION_LINE: module.OFFLINE_BANK_CONNECTION_MESSAGES[archive],
            module.OFFLINE_SAVE_LINE: module.OFFLINE_SAVE_MESSAGES[archive],
            module.OFFLINE_DISCONNECT_LINE: module.OFFLINE_DISCONNECT_MESSAGES[archive],
            module.TITLE_MODE_OFFLINE_LINE: expected_title_mode_offline,
            module.TITLE_MODE_DOWNLOAD_LINE: expected_title_mode_download,
            module.TITLE_MODE_UNLOCK_LINE: expected_title_mode_unlock,
            module.DISABLED_LINE: module.DISABLED_MESSAGES[archive],
            module.LANGUAGE_MENU_LINE: module.LANGUAGE_MENU_MESSAGES[archive],
            module.DOWNLOAD_GAME_SELECTION_LINE: module.DOWNLOAD_GAME_SELECTION_MESSAGES[archive],
            module.UNLOCK_USE_BANK_LINE: module.UNLOCK_MENU_MESSAGES[archive],
            module.UNLOCK_GAME_SELECTION_LINE: module.UNLOCK_GAME_SELECTION_MESSAGES[archive],
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
            output_message_file, module.UNLOCK_CHALLENGE_SOURCE_LINE
        )
        if output_stock_challenge_values != source_challenge_values:
            raise ValueError(f"stock unlock prompt changed in archive {archive}")
        expected_unlock_values = module.build_unlock_prompt_values(
            source_challenge_values, module.UNLOCK_CODE_LABELS[archive]
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
    parser.add_argument("--ips", required=True, type=Path)
    parser.add_argument("--source-romfs", required=True, type=Path)
    parser.add_argument("--output-romfs", required=True, type=Path)
    args = parser.parse_args()
    verify_code(args.base, args.patched, args.symbols, args.ips)
    verify_messages(args.source_romfs, args.output_romfs)
    print("combined patch static verification passed")


if __name__ == "__main__":
    main()
