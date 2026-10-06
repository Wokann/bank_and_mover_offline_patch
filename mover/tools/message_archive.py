"""Self-contained GARC and encrypted message-file codec.

自包含的 GARC 与加密消息文件编解码器。
"""

from __future__ import annotations

import struct
from dataclasses import dataclass


@dataclass
class GarcEntry:
    flags: int
    files: dict[int, bytes]


def u16(data: bytes, offset: int) -> int:
    return struct.unpack_from("<H", data, offset)[0]


def u32(data: bytes, offset: int) -> int:
    return struct.unpack_from("<I", data, offset)[0]


def _decrypt_line(data: bytes, line_key: int) -> list[int]:
    result: list[int] = []
    key = line_key
    for (encrypted,) in struct.iter_unpack("<H", data):
        result.append(encrypted ^ key)
        key = ((key << 3) | (key >> 13)) & 0xFFFF
    return result


def _encrypt_line(values: list[int], line_key: int) -> bytes:
    output = bytearray()
    key = line_key
    for value in values:
        output += struct.pack("<H", value ^ key)
        key = ((key << 3) | (key >> 13)) & 0xFFFF
    return bytes(output)


def _text_values(text: str) -> list[int]:
    encoded = text.encode("utf-16le")
    values = list(struct.unpack(f"<{len(encoded) // 2}H", encoded))
    values.append(0)
    if len(values) & 1:
        values.append(0)
    return values


def patch_message_file(
    data: bytes,
    replacements: dict[int, str],
    appended_lines: tuple[tuple[str, int], ...] = (),
    appended_value_lines: tuple[tuple[list[int], int], ...] = (),
) -> bytes:
    if len(data) < 0x18 or u16(data, 0) != 1 or u32(data, 12) != 0x10:
        raise ValueError("unsupported message-file header")
    line_count = u16(data, 2)
    section_offset = u32(data, 12)
    lines: list[tuple[list[int], int]] = []
    line_key = 0x7C89
    for index in range(line_count):
        entry_offset = section_offset + 4 + index * 8
        text_offset, length, flags = struct.unpack_from("<IHH", data, entry_offset)
        start = section_offset + text_offset
        encrypted = data[start : start + length * 2]
        if len(encrypted) != length * 2:
            raise ValueError(f"message line {index} extends past the file")
        values = _decrypt_line(encrypted, line_key)
        if index in replacements:
            values = _text_values(replacements[index])
        lines.append((values, flags))
        line_key = (line_key + 0x2983) & 0xFFFF
    lines.extend((_text_values(text), flags) for text, flags in appended_lines)
    lines.extend((list(values), flags) for values, flags in appended_value_lines)
    section = bytearray(4 + len(lines) * 8)
    line_key = 0x7C89
    for index, (values, flags) in enumerate(lines):
        text_offset = len(section)
        encrypted = _encrypt_line(values, line_key)
        struct.pack_into("<IHH", section, 4 + index * 8, text_offset, len(values), flags)
        section += encrypted
        while len(section) & 3:
            section.append(0)
        line_key = (line_key + 0x2983) & 0xFFFF
    struct.pack_into("<I", section, 0, len(section))
    header = bytearray(data[:section_offset])
    struct.pack_into("<H", header, 2, len(lines))
    struct.pack_into("<I", header, 4, len(section))
    return bytes(header + section)


def read_message_lines(data: bytes) -> list[str]:
    line_count = u16(data, 2)
    section_offset = u32(data, 12)
    lines: list[str] = []
    line_key = 0x7C89
    for index in range(line_count):
        entry_offset = section_offset + 4 + index * 8
        text_offset, length, _flags = struct.unpack_from("<IHH", data, entry_offset)
        start = section_offset + text_offset
        values = _decrypt_line(data[start : start + length * 2], line_key)
        if 0 in values:
            values = values[: values.index(0)]
        encoded = struct.pack(f"<{len(values)}H", *values) if values else b""
        lines.append(encoded.decode("utf-16le"))
        line_key = (line_key + 0x2983) & 0xFFFF
    return lines


def read_garc(data: bytes) -> tuple[int, int, list[GarcEntry]]:
    if data[:4] != b"CRAG":
        raise ValueError("not a little-endian GARC archive")
    header_length = u32(data, 4)
    version = u16(data, 10)
    alignment = u32(data, 32) if version == 0x0600 else 4
    fato = header_length
    if data[fato : fato + 4] != b"OTAF":
        raise ValueError("missing FATO section")
    fato_length = u32(data, fato + 4)
    entry_count = u16(data, fato + 8)
    fat_offsets = struct.unpack_from(f"<{entry_count}I", data, fato + 12)
    fatb = fato + fato_length
    if data[fatb : fatb + 4] != b"BTAF":
        raise ValueError("missing FATB section")
    fatb_length = u32(data, fatb + 4)
    fat_table = fatb + 12
    fimb = fatb + fatb_length
    if data[fimb : fimb + 4] != b"BMIF":
        raise ValueError("missing FIMB section")
    data_offset = fimb + u32(data, fimb + 4)
    entries: list[GarcEntry] = []
    for fat_offset in fat_offsets:
        cursor = fat_table + fat_offset
        flags = u32(data, cursor)
        cursor += 4
        files: dict[int, bytes] = {}
        for subindex in range(32):
            if flags & (1 << subindex):
                start, _end, length = struct.unpack_from("<III", data, cursor)
                cursor += 12
                files[subindex] = data[data_offset + start : data_offset + start + length]
        entries.append(GarcEntry(flags, files))
    return version, alignment, entries


def write_garc(version: int, alignment: int, entries: list[GarcEntry]) -> bytes:
    if version not in (0x0400, 0x0600):
        raise ValueError(f"unsupported GARC version 0x{version:04X}")
    header_length = 36 if version == 0x0600 else 28
    fato_length = 12 + 4 * len(entries)
    fat_records = bytearray()
    fat_offsets: list[int] = []
    file_data = bytearray()
    largest_unpadded = 0
    largest_padded = 0
    subentry_count = 0
    for entry in entries:
        fat_offsets.append(len(fat_records))
        fat_records += struct.pack("<I", entry.flags)
        for subindex in range(32):
            if not (entry.flags & (1 << subindex)):
                continue
            payload = entry.files[subindex]
            start = len(file_data)
            file_data += payload
            length = len(payload)
            while len(file_data) % alignment:
                file_data.append(0)
            end = len(file_data)
            fat_records += struct.pack("<III", start, end, length)
            largest_unpadded = max(largest_unpadded, length)
            largest_padded = max(largest_padded, end - start)
            subentry_count += 1
    fatb_length = 12 + len(fat_records)
    data_offset = header_length + fato_length + fatb_length + 12
    file_length = data_offset + len(file_data)
    header = bytearray()
    header += struct.pack("<4sIHHIII", b"CRAG", header_length, 0xFEFF, version, 4, data_offset, file_length)
    if version == 0x0600:
        header += struct.pack("<III", largest_padded, largest_unpadded, alignment)
    else:
        header += struct.pack("<I", largest_unpadded)
    fato = struct.pack("<4sIHH", b"OTAF", fato_length, len(entries), 0xFFFF)
    fato += struct.pack(f"<{len(fat_offsets)}I", *fat_offsets)
    fatb = struct.pack("<4sII", b"BTAF", fatb_length, subentry_count) + fat_records
    fimb = struct.pack("<4sII", b"BMIF", 12, len(file_data))
    return bytes(header + fato + fatb + fimb + file_data)
