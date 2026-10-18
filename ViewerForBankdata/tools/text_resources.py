"""Load fixed local text resources used by the viewer."""

from __future__ import annotations

import json
from dataclasses import dataclass
from pathlib import Path
from typing import Mapping, Sequence
from file_safety import require_plain_file

PRIVATE_USE_FIRST = 0xE000
PRIVATE_USE_LAST = 0xF8FF
COMMON_PRIVATE_GLYPHS: dict[int, str] = {
    0xE07F: " ",
    0xE08D: "…",
    0xE08E: "♂",
    0xE08F: "♀",
}


@dataclass(frozen=True)
class GameNameCatalog:
    source: Path | None
    names: dict[int, str]
    glyph_map: dict[int, str]
    note: str | None = None


def load_species_name_catalog(resource_dir: Path, language: str) -> GameNameCatalog:
    path = resource_dir / f"species_{language}.json"
    try:
        document = json.loads(require_plain_file(path).read_text(encoding="utf-8"))
        if document.get("format") != 1 or document.get("language") != language:
            raise ValueError("resource metadata does not match its file name")
        rows = document["names"]
        if not isinstance(rows, list):
            raise ValueError("resource names field is not a list")
        names = {number: value for number, value in enumerate(rows) if isinstance(value, str) and value}
        glyph_map = dict(COMMON_PRIVATE_GLYPHS)
        glyph_map.update({int(code, 16): value for code, value in document.get("glyph_map", {}).items()})
        return GameNameCatalog(path, names, glyph_map)
    except (OSError, KeyError, TypeError, ValueError, json.JSONDecodeError) as exc:
        return GameNameCatalog(None, {}, dict(COMMON_PRIVATE_GLYPHS), str(exc))


def render_stored_name(words: Sequence[int], glyph_map: Mapping[int, str] | None = None) -> str:
    characters: list[str] = []
    for value in words:
        if value in (0, 0xFFFF):
            break
        if glyph_map and value in glyph_map:
            characters.append(glyph_map[value])
        elif PRIVATE_USE_FIRST <= value <= PRIVATE_USE_LAST:
            characters.append("�")
        else:
            characters.append(chr(value))
    return "".join(characters).rstrip()
