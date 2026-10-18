"""Shared title-resource codec for independent, native-style single-line hints.

只改标题布局及提示位置；保留原图片、字号、字体、动画及其他资源。
The LZ11 layout follows the format described by DSDecmp's original reader:
https://github.com/svn2github/3DS-Explorer/blob/master/3DSExplorer/DSDecmp/Formats/Nitro/LZ11.cs
Pane fields/alpha flags are independently documented in Switch-Toolbox's
File_Format_Library/FileFormats/Layout/CTR/Panes/PAN1.cs and TXT1.cs.
"""

from __future__ import annotations

import struct
from pathlib import Path

from tools.message_archive import read_garc, write_garc, u16, u32

MODE_TEXT_CAPACITY = 256  # Bytes, including the UTF-16 terminator.
NATIVE_HOME_CENTER_Y = -78.0
HOME_CENTER_Y = -88.0
NATIVE_LANGUAGE_CENTER_Y = -45.0
LANGUAGE_CENTER_Y = -62.0
LANGUAGE_PANE_Y = LANGUAGE_CENTER_Y - HOME_CENTER_Y
SWITCH_CENTER_Y = -36.0
SWITCH_PANE_Y = SWITCH_CENTER_Y - HOME_CENTER_Y
MODE_CENTER_Y = 32.0
MODE_FONT_HEIGHT = 19.0
MODE_LINE_SPACING = 0.0
MODE_SCREEN_MARGIN = 15.0
MODE_PANE_X = 0.0
MODE_PANE_HEIGHT = 20.0
MODE_PANE_TOP = MODE_CENTER_Y + MODE_PANE_HEIGHT / 2
MODE_PANE_Y = MODE_CENTER_Y - HOME_CENTER_Y
MODE_PANE_WIDTH = 320.0 - 2 * MODE_SCREEN_MARGIN
USER_TEXT_COLOR = bytes((96, 96, 96, 255))
USER_PANE_WIDTH = 300.0
USER_STATUS_CENTER_Y, USER_ID_CENTER_Y = 106.0, 82.0
# Screen centers: mode 88, START 120, R 156, L 182, HOME 208.
# 单行原生文字属性、整像素坐标；START／HOME／版权保持当前位置，R/L/HOME 等距。
# Native credit alpha starts at screen y=220; HOME pane ends at 218 (tested).


def decompress_lz11(data: bytes) -> bytes:
    if len(data) < 4 or data[0] != 0x11:
        raise ValueError("expected an LZ11 title package")
    size, pos = int.from_bytes(data[1:4], "little"), 4
    if not size:
        if len(data) < 8:
            raise ValueError("truncated extended LZ11 size")
        size, pos = u32(data, 4), 8
    if size > 0xFFFFFF:
        raise ValueError("unexpected title package size")
    out = bytearray()

    def byte() -> int:
        nonlocal pos
        if pos == len(data):
            raise ValueError("truncated LZ11 token")
        result = data[pos]
        pos += 1
        return result

    while len(out) < size:
        flags = byte()
        for bit in range(7, -1, -1):
            if len(out) == size:
                break
            first = byte()
            if not flags & (1 << bit):
                out.append(first)
                continue
            kind, second = first >> 4, byte()
            if kind == 0:
                length = ((first & 15) << 4 | second >> 4) + 0x11
                distance = ((second & 15) << 8 | byte()) + 1
            elif kind == 1:
                third = byte()
                length = ((first & 15) << 12 | second << 4 | third >> 4) + 0x111
                distance = ((third & 15) << 8 | byte()) + 1
            else:
                length = kind + 1
                distance = ((first & 15) << 8 | second) + 1
            if distance > len(out) or length > size - len(out):
                raise ValueError("invalid LZ11 back-reference")
            for _ in range(length):
                out.append(out[-distance])
    if len(data) - pos > 3:
        raise ValueError("unexpected trailing LZ11 data")
    return bytes(out)


def compress_lz11(data: bytes) -> bytes:
    if len(data) > 0xFFFFFF:
        raise ValueError("title package is too large for LZ11")
    out = bytearray(b"\x11" + len(data).to_bytes(3, "little"))
    if not data:
        return bytes(out + bytes(4))
    pos = 0
    while pos < len(data):
        flag_pos = len(out)
        out.append(0)
        for bit in range(7, -1, -1):
            if pos == len(data):
                break
            previous = data.rfind(data[pos:pos + 3], max(0, pos - 4096), pos)
            length = 0
            if previous >= 0 and len(data) - pos >= 3:
                distance = pos - previous
                limit = min(len(data) - pos, 0x10110)
                while length < limit and data[pos + length] == data[pos + length - distance]:
                    length += 1
            if length < 3:
                out.append(data[pos])
                pos += 1
                continue
            out[flag_pos] |= 1 << bit
            displacement = distance - 1
            if length <= 16:
                out.extend(((length - 1) << 4 | displacement >> 8, displacement & 255))
            elif length <= 0x110:
                count = length - 0x11
                out.extend((count >> 4, (count & 15) << 4 | displacement >> 8, displacement & 255))
            else:
                count = length - 0x111
                out.extend((0x10 | count >> 12, count >> 4 & 255,
                            (count & 15) << 4 | displacement >> 8, displacement & 255))
            pos += length
    return bytes(out)


def sections(layout: bytes) -> list[tuple[bytes, int, int]]:
    if len(layout) < 20 or layout[:6] != b"CLYT\xff\xfe" or u32(layout, 12) != len(layout):
        raise ValueError("unexpected CLYT header")
    result = []
    pos = u16(layout, 6)
    for _ in range(u16(layout, 16)):
        if pos + 8 > len(layout):
            raise ValueError("truncated CLYT section")
        size = u32(layout, pos + 4)
        if size < 8 or pos + size > len(layout):
            raise ValueError("invalid CLYT section size")
        result.append((layout[pos:pos + 4], pos, size))
        pos += size
    if pos != len(layout):
        raise ValueError("CLYT section count/size mismatch")
    return result


def reposition_title_hints(layout: bytes, *, native_language_pane: bytes | None) -> bytes:
    """Lower HOME and Mover's existing L hint; never move START or credit.

    HOME only enters transparent padding above the visible copyright text.
    HOME 仅借用版权图片上方的透明区，原 START 图片和版权图片逐字节保留。
    """
    changes = {(b"txt1", b"g"): (NATIVE_HOME_CENTER_Y, HOME_CENTER_Y)}
    if native_language_pane is not None:
        changes[b"txt1", native_language_pane] = (NATIVE_LANGUAGE_CENTER_Y, LANGUAGE_CENTER_Y)
    result = bytearray(layout)
    seen = set()
    for tag, pos, _ in sections(layout):
        if tag not in (b"pic1", b"txt1"):
            continue
        name = layout[pos+12:pos+36].split(b"\0")[0]
        key = (tag, name)
        if key not in changes:
            continue
        if key in seen:
            raise ValueError("duplicate native title hint pane")
        seen.add(key)
        y = struct.unpack_from("<f", layout, pos+40)[0]
        old_y, new_y = changes[key]
        if y != old_y:
            raise ValueError("unsupported native title hint position")
        struct.pack_into("<f", result, pos+40, new_y)
    if seen != changes.keys():
        raise ValueError("missing native title hint pane")
    return bytes(result)


def add_title_panes(layout: bytes, extra_panes: tuple[tuple[bytes, str], ...],
                    *, mode_text_color: bytes) -> bytes:
    if len(mode_text_color) != 4:
        raise ValueError("mode text color must contain four RGBA bytes")
    chunks = sections(layout)
    matches = [i for i, (tag, pos, _) in enumerate(chunks)
               if tag == b"txt1" and layout[pos + 12:pos + 36].split(b"\0")[0] == b"g"]
    if len(matches) != 1:
        raise ValueError("title must contain exactly one HOME text pane")
    index = matches[0]
    _, pos, size = chunks[index]
    tag, user_pos, user_size = chunks[index + 1]
    if tag != b"usd1" or struct.unpack_from("<f", layout, pos + 40)[0] != HOME_CENTER_Y:
        raise ValueError("unsupported HOME pane layout")
    if u16(layout, pos + 76) != 110:
        raise ValueError("unexpected native title text capacity")
    user = layout[user_pos:user_pos + user_size]
    if user_size != 36 or user[28:] != b"g_fw\0\0\0\0":
        raise ValueError("unsupported title font-width property")
    # g_fw is a pane-local native property name, not a pane-name prefix.
    # g_fw 是原版按窗格查询的属性名，不随窗格名称改变。
    children = bytearray()
    centers = {"language": LANGUAGE_PANE_Y, "mode": MODE_PANE_Y, "switch": SWITCH_PANE_Y,
               "user_status": USER_STATUS_CENTER_Y-HOME_CENTER_Y,
               "user_id": USER_ID_CENTER_Y-HOME_CENTER_Y}
    for name, kind in extra_panes:
        if kind not in centers:
            raise ValueError("unsupported title hint pane kind")
        pane = bytearray(layout[pos:pos + size])
        pane[12:36] = name.ljust(24, b"\0")
        pane[8] |= 2  # InfluenceAlpha: inherit the animated parent HOME alpha.
        # Keep the original single-line size/alignment/font properties exactly.
        # 不再使用双行居中；保持原单行宽高、对齐和字体字段，偏移均为整数。
        struct.pack_into("<3f", pane, 36, MODE_PANE_X, centers[kind], 0.0)
        if kind == "mode":
            struct.pack_into("<H", pane, 76, MODE_TEXT_CAPACITY)
            # Set both text-gradient endpoints; leave HOME/L and pane alpha intact.
            # 两端文字颜色一致；不改变 HOME/L 颜色及窗格的渐变透明度。
            pane[92:100] = mode_text_color * 2
        elif kind in ("user_status", "user_id"):
            # Left aligned, integral pixel translation, native font/size and fade.
            # Hidden by default: only offline + multiple real users enables it.
            pane[8] &= ~1
            pane[9] = pane[84] = 3
            struct.pack_into("<f", pane, 36, -150.0)
            struct.pack_into("<f", pane, 68, USER_PANE_WIDTH)
            struct.pack_into("<H", pane, 76, MODE_TEXT_CAPACITY)
            pane[92:100] = USER_TEXT_COLOR * 2
        children.extend(pane + user)
    # TextBox is also a Pane. Its children follow native g's visibility,
    # title-in/title-out animation and transforms without editing animations.
    # 文本框同属 Pane；作为 g 的子窗格继承可见性、标题进出动画及变换。
    insert = (b"pas1" + struct.pack("<I", 8) + children +
              b"pae1" + struct.pack("<I", 8))
    result = bytearray(layout[:user_pos + user_size] + insert + layout[user_pos + user_size:])
    struct.pack_into("<I", result, 12, len(result))
    struct.pack_into("<H", result, 16, len(chunks) + 2 + 2 * len(extra_panes))
    sections(result)
    return bytes(result)


def package_files(data: bytes) -> tuple[int, int, list[tuple[int, str, int, int]]]:
    if len(data) < 8:
        raise ValueError("truncated title resource package")
    names = 4 + u32(data, 0) * 64
    if names + 4 > len(data):
        raise ValueError("invalid title resource names")
    names_end = names + 4 + u32(data, names) * 32
    darc = (names_end + 127) & ~127
    if darc + 28 > len(data) or data[darc:darc + 8] != b"darc\xff\xfe\x1c\0":
        raise ValueError("missing title DARC")
    if u32(data, darc + 12) != len(data) - darc:
        raise ValueError("DARC length mismatch")
    table = darc + u32(data, darc + 16)
    if table + 12 > len(data):
        raise ValueError("invalid DARC root")
    count = u32(data, table + 8)
    strings = table + count * 12
    body = darc + u32(data, darc + 24)
    if strings > body or body > len(data):
        raise ValueError("invalid DARC table bounds")
    files = []
    for i in range(count):
        entry = table + i * 12
        flags, offset, size = struct.unpack_from("<III", data, entry)
        if flags >> 24:
            continue
        name_start = strings + (flags & 0xFFFFFF)
        name_end = name_start
        while name_end + 2 <= body and data[name_end:name_end + 2] != b"\0\0":
            name_end += 2
        if name_start < strings or name_end + 2 > body:
            raise ValueError("invalid DARC name")
        name = data[name_start:name_end].decode("utf-16le")
        start = darc + offset
        if start < body or start + size > len(data):
            raise ValueError("invalid DARC file bounds")
        files.append((entry, name, start, size))
    return names, darc, files


def patch_package(data: bytes, *, native_pane_count: int, lower_layout: str,
                  extra_panes: tuple[tuple[bytes, str], ...], mode_text_color: bytes,
                  native_language_pane: bytes | None = None) -> bytes:
    names, darc, files = package_files(data)
    if u32(data, names) != native_pane_count:
        raise ValueError("unsupported title pane-name table")
    name_start = names + 4 + native_pane_count * 32
    if any(data[name_start:darc]):
        raise ValueError("nonempty title name-table padding")
    existing_names = {data[names+4+32*i:names+36+32*i].split(b"\0")[0]
                      for i in range(native_pane_count)}
    if not extra_panes or len({name for name, _ in extra_panes}) != len(extra_panes) or any(
            not name or len(name) > 23 or name in existing_names for name, _ in extra_panes):
        raise ValueError("invalid or duplicate new title pane names")
    target = [item for item in files if item[1] == lower_layout]
    if len(target) != 1:
        raise ValueError("missing unique lower title layout")
    entry, _, start, size = target[0]
    layout = reposition_title_hints(data[start:start + size], native_language_pane=native_language_pane)
    layout = add_title_panes(layout, extra_panes, mode_text_color=mode_text_color)
    delta = len(layout) - size
    # Mover has only 24 padding bytes, so one 32-byte name moves its DARC by
    # 128 bytes. DARC offsets are relative to DARC, not the outer package.
    # Mover 的名称表余量仅 24 字节，追加名称后按 128 字节重新对齐 DARC。
    prefix = bytearray(data[:name_start])
    for name, _ in extra_panes:
        prefix.extend(name.ljust(32, b"\0"))
    prefix.extend(bytes((-len(prefix)) % 128))
    struct.pack_into("<I", prefix, names, native_pane_count + len(extra_panes))
    body = bytearray(data[darc:start] + layout + data[start + size:])
    struct.pack_into("<I", body, 12, len(body))
    for position, _, file_start, _ in files:
        if position == entry:
            struct.pack_into("<I", body, position - darc + 8, len(layout))
        elif file_start >= start + size:
            struct.pack_into("<I", body, position - darc + 4, file_start - darc + delta)
    result = prefix + body
    package_files(result)
    return bytes(result)


def patch_title_archive(data: bytes, *, native_pane_count: int, lower_layout: str,
                        extra_panes: tuple[tuple[bytes, str], ...], mode_text_color: bytes,
                        native_language_pane: bytes | None = None) -> bytes:
    version, alignment, entries = read_garc(data)
    if len(entries) != 1 or set(entries[0].files) != {0}:
        raise ValueError("unexpected title GARC")
    package = patch_package(decompress_lz11(entries[0].files[0]),
                            native_pane_count=native_pane_count, lower_layout=lower_layout,
                            extra_panes=extra_panes, mode_text_color=mode_text_color,
                            native_language_pane=native_language_pane)
    entries[0].files[0] = compress_lz11(package)
    rebuilt = write_garc(version, alignment, entries)
    if decompress_lz11(read_garc(rebuilt)[2][0].files[0]) != package:
        raise ValueError("title archive round-trip failed")
    return rebuilt


def build_title_archive(source_romfs: Path, output_romfs: Path, *, archive: str,
                        native_pane_count: int, lower_layout: str,
                        extra_panes: tuple[tuple[bytes, str], ...], mode_text_color: bytes,
                        native_language_pane: bytes | None = None) -> None:
    relative = Path("a") / archive
    source, output = source_romfs / relative, output_romfs / relative
    patched = patch_title_archive(source.read_bytes(), native_pane_count=native_pane_count,
                                  lower_layout=lower_layout, extra_panes=extra_panes,
                                  mode_text_color=mode_text_color, native_language_pane=native_language_pane)
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_bytes(patched)
    print(f"patched independent title panes -> {output}")
