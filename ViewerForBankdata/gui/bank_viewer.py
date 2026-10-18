# -*- coding: utf-8 -*-
"""Read-only bankdata viewer using the Python standard library."""

from __future__ import annotations

import argparse
import datetime as dt
import struct
import sys
import tkinter as tk
from collections import Counter
from dataclasses import dataclass
from pathlib import Path
from tkinter import filedialog, messagebox, ttk

ROOT = Path(__file__).absolute().parents[1]
TOOLS = ROOT / "tools"
if str(TOOLS) not in sys.path:
    sys.path.insert(0, str(TOOLS))

from text_resources import (  # noqa: E402
    COMMON_PRIVATE_GLYPHS,
    GameNameCatalog,
    load_species_name_catalog,
    render_stored_name,
)
from parse_bank_file import BOX_LENGTH, decrypt_ek6, fmt_u16  # noqa: E402
from file_safety import require_plain_file  # noqa: E402
from viewer_i18n import (  # noqa: E402
    LANGUAGE_OPTIONS,
    STAT_HEADERS,
    field_text,
    game_name,
    source_title,
    ui,
)


CORE_SIZE = 0xBB518
OBJECT_SIZE = CORE_SIZE + 8
BOX_BASE = 0x17C
BOX_STRIDE = 0x1B56
BOX_COUNT = 100
SLOTS_PER_BOX = 30
TRANSFER_BASE = 0xAAF14
FORMAT_BASE = 0xACA44
TRANSFER_FORMAT_BASE = 0xAD5FC
SOURCE_RECORD_BASE = 0xAD61C
SOURCE_RECORD_SIZE = 68
INDEX_BASE = 0xAD83C
INDEX_SIZE = 29280
SOURCE_CODE_BASE = 0xB4AA0
TIME_BASE = 0xB5658
NICKNAME_OFFSET = 0x40
NICKNAME_WORD_COUNT = 13
STORED_NAME_LANGUAGE_OFFSET = 0xE3

# The slot's language byte selects one of the two Chinese glyph tables when
# its default nickname uses private-use words.  Other languages need only the
# small common typography map.
CHINESE_STORED_NAME_LANGUAGES = {9: "zh-Hans", 10: "zh-Hant"}


@dataclass(frozen=True)
class SlotDetails:
    species: int
    nickname_words: tuple[int, ...]
    language_code: int


def decode_utf16(raw: bytes) -> str:
    return raw.decode("utf-16-le", errors="replace").split("\x00", 1)[0].rstrip()


def service_time(value: int) -> str:
    if not value:
        return "—"
    try:
        return (dt.datetime(2000, 1, 1) + dt.timedelta(seconds=value)).strftime("%Y-%m-%d %H:%M:%S UTC")
    except (OverflowError, ValueError):
        return f"0x{value:016X}"


class BankModel:
    def __init__(self, path: Path):
        path = require_plain_file(path)
        raw = path.read_bytes()
        if len(raw) == OBJECT_SIZE:
            raw = raw[8:]
        if len(raw) != CORE_SIZE:
            raise ValueError(f"expected 0x{CORE_SIZE:X} or 0x{OBJECT_SIZE:X} bytes, got 0x{len(raw):X}")
        self.path = path
        self.core = raw
        self.group_names = [decode_utf16(raw[0x008 + index * 34:0x008 + (index + 1) * 34]) for index in range(10)]
        self.boxes = [self._read_box(index) for index in range(BOX_COUNT)]
        self.transfer_slots = [self._read_slot(TRANSFER_BASE + index * BOX_LENGTH) for index in range(SLOTS_PER_BOX)]
        self.source_records = [self._read_source(index) for index in range(8)]

    def _decoded_slot(self, offset: int) -> bytes | None:
        slot = self.core[offset:offset + BOX_LENGTH]
        if len(slot) != BOX_LENGTH or slot[:4] == b"\xff\xff\xff\xff":
            return None
        return decrypt_ek6(slot)

    def _read_slot(self, offset: int) -> int:
        decoded = self._decoded_slot(offset)
        return fmt_u16(decoded, 0x08) if decoded else 0

    def slot_details(self, box_index: int | None, slot_index: int) -> SlotDetails | None:
        if not 0 <= slot_index < SLOTS_PER_BOX:
            return None
        offset = (
            TRANSFER_BASE + slot_index * BOX_LENGTH
            if box_index is None
            else BOX_BASE + box_index * BOX_STRIDE + slot_index * BOX_LENGTH
        )
        decoded = self._decoded_slot(offset)
        if not decoded:
            return None
        return SlotDetails(
            fmt_u16(decoded, 0x08),
            struct.unpack_from(f"<{NICKNAME_WORD_COUNT}H", decoded, NICKNAME_OFFSET),
            decoded[STORED_NAME_LANGUAGE_OFFSET],
        )

    def _read_box(self, index: int) -> dict[str, object]:
        base = BOX_BASE + index * BOX_STRIDE
        return {
            "index": index,
            "name": decode_utf16(self.core[base + 30 * BOX_LENGTH:base + 30 * BOX_LENGTH + 34]),
            "background": self.core[base + 0x1B52],
            "group": self.core[base + 0x1B53],
            "order": struct.unpack_from("<H", self.core, base + 0x1B54)[0],
            "slots": [self._read_slot(base + slot * BOX_LENGTH) for slot in range(SLOTS_PER_BOX)],
        }

    def _read_source(self, index: int) -> dict[str, object]:
        base = SOURCE_RECORD_BASE + index * SOURCE_RECORD_SIZE
        return {
            "index": index,
            "name": decode_utf16(self.core[base:base + 26]),
            "sex": struct.unpack_from("<H", self.core, base + 26)[0],
            "trainer_id": struct.unpack_from("<I", self.core, base + 28)[0],
            "stats": struct.unpack_from("<9I", self.core, base + 32),
        }

    def header_rows(self) -> list[tuple[str, str]]:
        year = struct.unpack_from("<H", self.core, 0x160)[0]
        month, day, hour, minute, second, padding = self.core[0x162:0x168]
        update_gift, timed_gift, points, passes = struct.unpack_from("<IIII", self.core, 0x168)
        flags = self.core[0x178:0x17C]
        deposited, withdrawn = struct.unpack_from("<HH", self.core, 0xB4A9C)
        index_magic = self.core[INDEX_BASE:INDEX_BASE + 4].decode("ascii", errors="replace")
        index_version = struct.unpack_from("<I", self.core, INDEX_BASE + 4)[0]
        reserved = self.core[0xBB419:0xBB518]
        return [
            ("object_id", f"0x{struct.unpack_from('<Q', self.core, 0)[0]:016X}"),
            ("version", str(struct.unpack_from("<H", self.core, 0x15C)[0])),
            ("box_count", str(struct.unpack_from("<H", self.core, 0x15E)[0])),
            ("edit_time", f"{year:04d}-{month:02d}-{day:02d} {hour:02d}:{minute:02d}:{second:02d}  pad=0x{padding:02X}"),
            ("rewards", f"update={update_gift}, timed={timed_gift}"),
            ("points", f"points={points}, passes={passes}"),
            ("flags", " ".join(str(value) for value in flags)),
            ("format", f"Bank Box={dict(Counter(self.core[FORMAT_BASE:FORMAT_BASE + 3000]))}; Transfer={dict(Counter(self.core[TRANSFER_FORMAT_BASE:TRANSFER_FORMAT_BASE + 30]))}"),
            ("index", f"offset=0x{INDEX_BASE:X}, size={INDEX_SIZE}, magic={index_magic!r}, version={index_version}"),
            ("operations", f"deposit={deposited}, withdraw={withdrawn}"),
            ("tail", f"extended={self.core[0xBB418]}, reserved_nonzero={sum(value != 0 for value in reserved)}"),
        ]

    def box_metadata(self, index: int) -> dict[str, object]:
        start = index * SLOTS_PER_BOX
        formats = Counter(self.core[FORMAT_BASE + start:FORMAT_BASE + start + SLOTS_PER_BOX])
        codes = Counter(self.core[SOURCE_CODE_BASE + start:SOURCE_CODE_BASE + start + SLOTS_PER_BOX])
        times = [struct.unpack_from("<Q", self.core, TIME_BASE + (start + slot) * 8)[0] for slot in range(SLOTS_PER_BOX)]
        return {"formats": formats, "codes": codes, "times": times}


class BankViewer(tk.Tk):
    def __init__(
        self,
        initial: Path | None = None,
        initial_language: str = "en",
    ):
        super().__init__()
        self.geometry("1380x850")
        self.minsize(1050, 680)
        self.model: BankModel | None = None
        self.language = tk.StringVar(value=initial_language)
        self.language_choice = tk.StringVar(value=dict(LANGUAGE_OPTIONS)[initial_language])
        self.resource_dir = ROOT / "resources"
        self.catalog = GameNameCatalog(None, {}, dict(COMMON_PRIVATE_GLYPHS), None)
        self.species_names: dict[int, str] = {}
        self._stored_name_maps: dict[int, dict[int, str]] = {}
        self.selected_box: int | None = None
        self.selected_slot = 0
        self.active_slots: list[int] = []
        self._build()
        self.apply_language()
        if initial:
            self.load_path(initial)

    def tr(self, key: str) -> str:
        return ui(self.language.get(), key)

    def _build(self) -> None:
        top = ttk.Frame(self, padding=8)
        top.pack(fill="x")
        self.open_button = ttk.Button(top, command=self.open_file)
        self.open_button.pack(side="left")
        self.lang_label = ttk.Label(top)
        self.lang_label.pack(side="left", padx=(20, 6))
        self.language_box = ttk.Combobox(
            top,
            textvariable=self.language_choice,
            values=tuple(label for _code, label in LANGUAGE_OPTIONS),
            state="readonly",
            width=18,
        )
        self.language_box.pack(side="left")
        self.language_box.bind("<<ComboboxSelected>>", self._choose_language)
        self.path_label = ttk.Label(top, anchor="e")
        self.path_label.pack(side="right", fill="x", expand=True)

        self.privacy_label = ttk.Label(self, padding=(10, 4), foreground="#9b4d00")
        self.privacy_label.pack(fill="x")

        pane = ttk.Panedwindow(self, orient="horizontal")
        pane.pack(fill="both", expand=True, padx=8, pady=8)
        left = ttk.Frame(pane, width=300)
        right = ttk.Frame(pane)
        pane.add(left, weight=1)
        pane.add(right, weight=4)

        self.tree_title = ttk.Label(left, font=("Segoe UI", 11, "bold"))
        self.tree_title.pack(anchor="w", pady=(0, 5))
        self.tree = ttk.Treeview(left, show="tree")
        tree_scroll = ttk.Scrollbar(left, orient="vertical", command=self.tree.yview)
        self.tree.configure(yscrollcommand=tree_scroll.set)
        self.tree.pack(side="left", fill="both", expand=True)
        tree_scroll.pack(side="right", fill="y")
        self.tree.bind("<<TreeviewSelect>>", self.select_tree)

        self.notebook = ttk.Notebook(right)
        self.notebook.pack(fill="both", expand=True)
        self.box_tab = ttk.Frame(self.notebook, padding=10)
        self.creature_tab = ttk.Frame(self.notebook, padding=10)
        self.overview_tab = ttk.Frame(self.notebook, padding=10)
        self.sources_tab = ttk.Frame(self.notebook, padding=10)
        self.boxmeta_tab = ttk.Frame(self.notebook, padding=10)
        self.slotmeta_tab = ttk.Frame(self.notebook, padding=10)
        self.structure_tab = ttk.Frame(self.notebook, padding=10)
        for tab in (self.box_tab, self.creature_tab, self.overview_tab, self.sources_tab, self.boxmeta_tab, self.slotmeta_tab, self.structure_tab):
            self.notebook.add(tab, text="")

        self.box_heading = ttk.Label(self.box_tab, font=("Segoe UI", 14, "bold"))
        self.box_heading.pack(anchor="w", pady=(0, 10))
        grid = ttk.Frame(self.box_tab)
        grid.pack(fill="both", expand=True)
        self.slot_labels: list[ttk.Label] = []
        for slot in range(SLOTS_PER_BOX):
            row, column = divmod(slot, 6)
            grid.rowconfigure(row, weight=1)
            grid.columnconfigure(column, weight=1)
            label = ttk.Label(grid, anchor="center", relief="ridge", padding=8, wraplength=140)
            label.grid(row=row, column=column, sticky="nsew", padx=3, pady=3)
            label.bind("<Button-1>", lambda _event, selected=slot: self.select_slot(selected))
            self.slot_labels.append(label)

        self.creature_text = tk.Text(self.creature_tab, wrap="word", padx=10, pady=10, state="disabled")
        creature_scroll = ttk.Scrollbar(self.creature_tab, orient="vertical", command=self.creature_text.yview)
        self.creature_text.configure(yscrollcommand=creature_scroll.set)
        self.creature_text.pack(side="left", fill="both", expand=True)
        creature_scroll.pack(side="right", fill="y")

        self.overview = ttk.Treeview(self.overview_tab, columns=("field", "value"), show="headings", height=13)
        self.overview.column("field", width=220, stretch=False)
        self.overview.column("value", width=720)
        self.overview.pack(fill="x")
        self.overview.bind("<<TreeviewSelect>>", self.show_description)
        self.description = tk.Text(self.overview_tab, height=8, wrap="word", state="disabled", padx=8, pady=8)
        self.description.pack(fill="both", expand=True, pady=(10, 0))

        self.source_columns = ("game", "player", "sex", "trainer", "caught", "fished", "hatched", "evolved", "fossils", "wild", "traded", "daily_caught", "daily_evolved")
        self.sources = ttk.Treeview(self.sources_tab, columns=self.source_columns, show="headings")
        for column, width in (("game", 150), ("player", 170), ("sex", 70), ("trainer", 130), ("caught", 80), ("fished", 80), ("hatched", 80), ("evolved", 80), ("fossils", 80), ("wild", 120), ("traded", 80), ("daily_caught", 130), ("daily_evolved", 130)):
            self.sources.column(column, width=width, anchor="center")
        source_y_scroll = ttk.Scrollbar(self.sources_tab, orient="vertical", command=self.sources.yview)
        source_x_scroll = ttk.Scrollbar(self.sources_tab, orient="horizontal", command=self.sources.xview)
        self.sources.configure(yscrollcommand=source_y_scroll.set, xscrollcommand=source_x_scroll.set)
        self.sources.grid(row=0, column=0, sticky="nsew")
        source_y_scroll.grid(row=0, column=1, sticky="ns")
        source_x_scroll.grid(row=1, column=0, sticky="ew")
        self.sources_tab.rowconfigure(0, weight=1)
        self.sources_tab.columnconfigure(0, weight=1)

        self.boxmeta = ttk.Treeview(self.boxmeta_tab, columns=("box", "f6", "f7", "origin", "latest"), show="headings")
        self.boxmeta.column("box", width=210)
        self.boxmeta.column("f6", width=90, anchor="center")
        self.boxmeta.column("f7", width=90, anchor="center")
        self.boxmeta.column("origin", width=500)
        self.boxmeta.column("latest", width=210)
        boxmeta_scroll = ttk.Scrollbar(self.boxmeta_tab, orient="vertical", command=self.boxmeta.yview)
        self.boxmeta.configure(yscrollcommand=boxmeta_scroll.set)
        self.boxmeta.pack(side="left", fill="both", expand=True)
        boxmeta_scroll.pack(side="right", fill="y")

        self.slotmeta = ttk.Treeview(self.slotmeta_tab, columns=("slot", "species", "format", "origin", "code", "time", "raw_time"), show="headings")
        for column, width in (("slot", 70), ("species", 190), ("format", 120), ("origin", 200), ("code", 70), ("time", 230), ("raw_time", 160)):
            self.slotmeta.column(column, width=width, anchor="center")
        slot_y_scroll = ttk.Scrollbar(self.slotmeta_tab, orient="vertical", command=self.slotmeta.yview)
        slot_x_scroll = ttk.Scrollbar(self.slotmeta_tab, orient="horizontal", command=self.slotmeta.xview)
        self.slotmeta.configure(yscrollcommand=slot_y_scroll.set, xscrollcommand=slot_x_scroll.set)
        self.slotmeta.grid(row=0, column=0, sticky="nsew")
        slot_y_scroll.grid(row=0, column=1, sticky="ns")
        slot_x_scroll.grid(row=1, column=0, sticky="ew")
        self.slotmeta_tab.rowconfigure(0, weight=1)
        self.slotmeta_tab.columnconfigure(0, weight=1)

        self.structure = tk.Text(self.structure_tab, wrap="none", padx=10, pady=10, state="disabled")
        structure_y_scroll = ttk.Scrollbar(self.structure_tab, orient="vertical", command=self.structure.yview)
        structure_x_scroll = ttk.Scrollbar(self.structure_tab, orient="horizontal", command=self.structure.xview)
        self.structure.configure(yscrollcommand=structure_y_scroll.set, xscrollcommand=structure_x_scroll.set)
        self.structure.grid(row=0, column=0, sticky="nsew")
        structure_y_scroll.grid(row=0, column=1, sticky="ns")
        structure_x_scroll.grid(row=1, column=0, sticky="ew")
        self.structure_tab.rowconfigure(0, weight=1)
        self.structure_tab.columnconfigure(0, weight=1)
        self.status = ttk.Label(self, relief="sunken", anchor="w", padding=4)
        self.status.pack(fill="x", side="bottom")

    def _choose_language(self, _event: object) -> None:
        selected = self.language_choice.get()
        for code, label in LANGUAGE_OPTIONS:
            if label == selected:
                self.language.set(code)
                break
        self.apply_language()

    def apply_language(self) -> None:
        language = self.language.get()
        self.catalog = load_species_name_catalog(self.resource_dir, language)
        self.species_names = self.catalog.names
        self._stored_name_maps.clear()
        for code, stored_language in CHINESE_STORED_NAME_LANGUAGES.items():
            if stored_language == language:
                self._stored_name_maps[code] = self.catalog.glyph_map
        self.title(self.tr("title"))
        self.open_button.configure(text=self.tr("open"))
        self.lang_label.configure(text=self.tr("language") + ":")
        self.tree_title.configure(text=self.tr("groups"))
        self.privacy_label.configure(text="⚠ " + self.tr("privacy"))
        for tab, key in zip(
            (self.box_tab, self.creature_tab, self.overview_tab, self.sources_tab, self.boxmeta_tab, self.slotmeta_tab, self.structure_tab),
            ("box", "creature", "overview", "sources", "boxmeta", "slotmeta", "structure"),
        ):
            self.notebook.tab(tab, text=self.tr(key))
        for column, key in (("field", "field"), ("value", "value")):
            self.overview.heading(column, text=self.tr(key))
        source_headers = {
            "game": self.tr("game"), "player": self.tr("player"), "sex": self.tr("sex"), "trainer": self.tr("trainer"),
            "caught": STAT_HEADERS[language][0], "fished": STAT_HEADERS[language][1], "hatched": STAT_HEADERS[language][2],
            "evolved": STAT_HEADERS[language][3], "fossils": STAT_HEADERS[language][4], "wild": STAT_HEADERS[language][5],
            "traded": STAT_HEADERS[language][6], "daily_caught": STAT_HEADERS[language][7], "daily_evolved": STAT_HEADERS[language][8],
        }
        for column, label in source_headers.items():
            self.sources.heading(column, text=label)
        for column, key in (("box", "box"), ("f6", "format6"), ("f7", "format7"), ("origin", "origin"), ("latest", "latest")):
            self.boxmeta.heading(column, text=self.tr(key))
        for column, label in (("slot", self.tr("slots")), ("species", self.tr("number")), ("format", self.tr("format")), ("origin", self.tr("origin")), ("code", self.tr("code")), ("time", self.tr("latest")), ("raw_time", self.tr("raw_time"))):
            self.slotmeta.heading(column, text=label)
        if self.model:
            self.refresh_all()
        else:
            self.status.configure(text=self.tr("no_file"))

    def open_file(self) -> None:
        path = filedialog.askopenfilename(title=self.tr("open"), filetypes=(("bankdata", "*.bin *.dat"), (self.tr("all"), "*.*")))
        if path:
            self.load_path(Path(path))

    def load_path(self, path: Path) -> None:
        try:
            self.model = BankModel(path)
        except Exception as exc:
            messagebox.showerror(self.tr("invalid"), str(exc))
            return
        self.path_label.configure(text=str(path))
        self.refresh_all()
        self.status.configure(text=f"{self.tr('loaded')}: {path.name}  |  0x{CORE_SIZE:X} bytes")

    def refresh_all(self) -> None:
        self.refresh_tree()
        self.refresh_overview()
        self.refresh_sources()
        self.refresh_boxmeta()
        self.refresh_structure()
        self.show_box(self.selected_box if self.selected_box is not None else 0)

    def refresh_tree(self) -> None:
        self.tree.delete(*self.tree.get_children())
        groups: dict[int, str] = {}
        for index in range(10):
            name = self.model.group_names[index] or f"{self.tr('groups')} {index + 1}"
            groups[index] = self.tree.insert("", "end", iid=f"g{index}", text=name, open=index == 0)
        for box in sorted(self.model.boxes, key=lambda item: (item["group"], item["order"], item["index"])):
            parent = groups.get(int(box["group"]), "")
            slots = box["slots"]
            count = sum(value != 0 for value in slots)
            name = str(box["name"]) or f"{self.tr('box')} {int(box['index']) + 1}"
            self.tree.insert(parent, "end", iid=f"b{box['index']}", text=f"{int(box['index']) + 1:03d}  {name}  [{count}/30]")
        transfer_count = sum(value != 0 for value in self.model.transfer_slots)
        self.tree.insert("", "end", iid="transfer", text=f"{self.tr('transfer')} [{transfer_count}/30]")

    def select_tree(self, _event: object) -> None:
        selection = self.tree.selection()
        if not selection:
            return
        item = selection[0]
        if item.startswith("b"):
            self.show_box(int(item[1:]))
            self.notebook.select(self.box_tab)
        elif item == "transfer":
            self.show_box(None)
            self.notebook.select(self.box_tab)

    def species_label(self, number: int) -> str:
        if not number:
            return self.tr("empty")
        name = self.species_names.get(number)
        return f"No.{number:04d}\n{name}" if name else f"No.{number:04d}"

    def show_box(self, index: int | None) -> None:
        self.selected_box = index
        if index is None:
            slots = self.model.transfer_slots
            heading = self.tr("transfer")
        else:
            box = self.model.boxes[index]
            slots = box["slots"]
            name = str(box["name"])
            heading = f"{self.tr('box')} {index + 1:03d}" + (f" — {name}" if name else "")
        self.active_slots = list(slots)
        if not 0 <= self.selected_slot < len(self.active_slots):
            self.selected_slot = 0
        self.box_heading.configure(text=heading)
        for position, (label, species) in enumerate(zip(self.slot_labels, self.active_slots), start=1):
            label.configure(text=f"{position:02d}\n{self.species_label(species)}", relief="sunken" if position - 1 == self.selected_slot else "ridge")
        self.refresh_slotmeta(index, self.active_slots)
        self.refresh_creature()

    def select_slot(self, slot: int) -> None:
        if not self.model or not 0 <= slot < len(self.active_slots):
            return
        self.selected_slot = slot
        self.show_box(self.selected_box)
        self.notebook.select(self.creature_tab)

    def _slot_extra(self, slot: int) -> tuple[int, int | None, int | None]:
        if self.selected_box is None:
            return self.model.core[TRANSFER_FORMAT_BASE + slot], None, None
        offset = self.selected_box * SLOTS_PER_BOX + slot
        return (
            self.model.core[FORMAT_BASE + offset],
            self.model.core[SOURCE_CODE_BASE + offset],
            struct.unpack_from("<Q", self.model.core, TIME_BASE + offset * 8)[0],
        )

    def _format_text(self, value: int) -> str:
        if value == 0:
            return self.tr("format6")
        if value == 1:
            return self.tr("format7")
        return f"{self.tr('unknown')}({value})"

    def _source_name(self, code: int) -> str:
        title = source_title(self.language.get(), code)
        return title if title else f"{self.tr('unknown')}({code})"

    def _glyph_map_for_stored_name(self, language_code: int) -> dict[int, str]:
        stored_language = CHINESE_STORED_NAME_LANGUAGES.get(language_code)
        if stored_language is None:
            return COMMON_PRIVATE_GLYPHS
        glyph_map = self._stored_name_maps.get(language_code)
        if glyph_map is None:
            glyph_map = load_species_name_catalog(self.resource_dir, stored_language).glyph_map
            self._stored_name_maps[language_code] = glyph_map
        return glyph_map

    def refresh_creature(self) -> None:
        self.creature_text.configure(state="normal")
        self.creature_text.delete("1.0", "end")
        if not self.active_slots:
            self.creature_text.insert("1.0", self.tr("select_slot"))
            self.creature_text.configure(state="disabled")
            return
        species = self.active_slots[self.selected_slot]
        if not species:
            self.creature_text.insert("1.0", f"{self.tr('empty')}\n\n{self.tr('select_slot')}")
            self.creature_text.configure(state="disabled")
            return
        format_value, source_code, timestamp = self._slot_extra(self.selected_slot)
        lines = [
            self.species_label(species).replace("\n", " "),
            "",
            f"{self.tr('slots')}: {self.selected_slot + 1}",
            f"{self.tr('format')}: {self._format_text(format_value)}",
        ]
        if source_code is not None:
            lines.append(f"{self.tr('origin')}: {self._source_name(source_code)}")
        if timestamp is not None:
            lines.append(f"{self.tr('latest')}: {service_time(timestamp)}")
        details = self.model.slot_details(self.selected_box, self.selected_slot)
        if details:
            nickname = render_stored_name(
                details.nickname_words,
                self._glyph_map_for_stored_name(details.language_code),
            )
            if nickname:
                lines.append(f"{self.tr('nickname')}: {nickname}")
        self.creature_text.insert("1.0", "\n".join(lines))
        self.creature_text.configure(state="disabled")

    def refresh_overview(self) -> None:
        self.overview.delete(*self.overview.get_children())
        for key, value in self.model.header_rows():
            label, _description = field_text(self.language.get(), key)
            self.overview.insert("", "end", iid=key, values=(label, value))
        self.show_description(None)

    def show_description(self, _event: object | None) -> None:
        selected = self.overview.selection()
        text = field_text(self.language.get(), selected[0])[1] if selected else self.tr("description")
        self.description.configure(state="normal")
        self.description.delete("1.0", "end")
        self.description.insert("1.0", text)
        self.description.configure(state="disabled")

    def refresh_sources(self) -> None:
        self.sources.delete(*self.sources.get_children())
        for row in self.model.source_records:
            sex_value = int(row["sex"])
            sex = self.tr("male") if sex_value == 0 else self.tr("female") if sex_value == 1 else f"{self.tr('unknown')}({sex_value})"
            self.sources.insert("", "end", values=(game_name(self.language.get(), int(row["index"])), row["name"], sex, row["trainer_id"], *row["stats"]))

    def refresh_boxmeta(self) -> None:
        self.boxmeta.delete(*self.boxmeta.get_children())
        for box in self.model.boxes:
            meta = self.model.box_metadata(int(box["index"]))
            origins = ", ".join(f"{self._source_name(code)}×{count}" for code, count in sorted(meta["codes"].items()) if code)
            valid_times = [value for value in meta["times"] if value]
            self.boxmeta.insert(
                "",
                "end",
                values=(
                    f"{int(box['index']) + 1:03d}  {box['name']}",
                    meta["formats"].get(0, 0),
                    meta["formats"].get(1, 0),
                    origins or "—",
                    service_time(max(valid_times)) if valid_times else "—",
                ),
            )

    def refresh_slotmeta(self, index: int | None, slots: list[int]) -> None:
        self.slotmeta.delete(*self.slotmeta.get_children())
        if index is None:
            formats = self.model.core[TRANSFER_FORMAT_BASE:TRANSFER_FORMAT_BASE + SLOTS_PER_BOX]
            source_codes: list[int | None] = [None] * SLOTS_PER_BOX
            times: list[int | None] = [None] * SLOTS_PER_BOX
        else:
            start = index * SLOTS_PER_BOX
            formats = self.model.core[FORMAT_BASE + start:FORMAT_BASE + start + SLOTS_PER_BOX]
            source_codes = list(self.model.core[SOURCE_CODE_BASE + start:SOURCE_CODE_BASE + start + SLOTS_PER_BOX])
            times = [struct.unpack_from("<Q", self.model.core, TIME_BASE + (start + slot) * 8)[0] for slot in range(SLOTS_PER_BOX)]
        for position, species in enumerate(slots):
            code = source_codes[position]
            timestamp = times[position]
            self.slotmeta.insert(
                "",
                "end",
                values=(
                    position + 1,
                    self.species_label(species).replace("\n", " "),
                    self._format_text(formats[position]),
                    "—" if code is None else self._source_name(code),
                    "—" if code is None else code,
                    "—" if timestamp is None else service_time(timestamp),
                    "—" if timestamp is None else (f"0x{timestamp:016X}" if timestamp else "0"),
                ),
            )

    def refresh_structure(self) -> None:
        # The detailed report is independent prose about observed storage
        # roles; no game prose is loaded for this page.
        lines = []
        for key, value in self.model.header_rows():
            label, description = field_text(self.language.get(), key)
            lines.extend((f"{label}: {value}", f"  {description}", ""))
        self.structure.configure(state="normal")
        self.structure.delete("1.0", "end")
        self.structure.insert("1.0", "\n".join(lines))
        self.structure.configure(state="disabled")


def self_test(path: Path) -> int:
    model = BankModel(path)
    assert len(model.boxes) == 100
    assert len(model.group_names) == 10
    assert len(model.transfer_slots) == 30
    assert len(model.source_records) == 8
    assert model.header_rows()[-1][0] == "tail"
    print(f"OK: {path}")
    print("boxes=100 groups=10 transfer_slots=30 source_records=8")
    print(f"nonempty_bank_slots={sum(value != 0 for box in model.boxes for value in box['slots'])}")
    print(f"nonempty_transfer_slots={sum(value != 0 for value in model.transfer_slots)}")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description="Read-only bankdata GUI viewer")
    parser.add_argument("path", nargs="?", type=Path)
    parser.add_argument("--language", choices=tuple(dict(LANGUAGE_OPTIONS)), default="en")
    parser.add_argument("--self-test", action="store_true")
    args = parser.parse_args()
    path = args.path
    if args.self_test:
        if path is None:
            parser.error("self-test requires a bankdata path")
        return self_test(path)
    app = BankViewer(path, args.language)
    app.mainloop()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
