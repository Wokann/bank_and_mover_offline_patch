# Cross-language font supplementation

[简体中文](fonts.zh-cn.md)

## Coverage

Bank and Mover bundle three main fonts in `romfs/a/0/2/3`: the common font
(including Japanese, Korean and Western scripts), Simplified Chinese, and
Traditional Chinese. Every main font gains characters required for:

- Default Bank, group and box names, including 银行/銀行, 群组/群組 and 盒子.
- Species 1–807 in the project's ten language-name resources.
- Existing non-CJK characters from all three original main fonts, including
  kana, Latin, Korean, punctuation and native private-use characters.

All language subfiles of the native keyboard/conversion archive `a/0/0/3` are
checked separately: its 471 non-null/non-terminator characters are covered by
every supplemented main font. Keyboard/input rules do not change; this adds
neither a Chinese input method nor new selectable input characters. The smaller
auxiliary font retains its up/down-arrow supplement. The numeric font is unchanged.

This is not a complete Unicode or CJK font. Arbitrary translations, external
usernames, nicknames or group names may require characters outside this scope.
A missing character is never replaced by a merely similar-looking character.

## Build and memory

Supplementation runs only on the PC during resource construction. All glyphs
come from the user's original fonts. Exact pixels, filtering borders and width
metrics permit an alias; otherwise native pixels are copied to a new cell,
without scaling or redrawing. Every original character retains its glyph index,
width metrics and complete sampling border. Save-text encoding does not change.
Species JSON supplies required characters, never an OCR/private-use glyph map.

If existing cells are insufficient, atlas repacking uses supported power-of-two
3DS texture dimensions without increasing texture bytes or draw-sheet count.
Holes in an existing TABLE map are filled there, rather than hidden behind a
later map. All FINF/TGLP/CWDH/CMAP offsets and linked lists are revalidated.

Runtime keeps the native whole-font loading/release path, not per-character SD
reads. This adds no worker jobs, configuration files, save writes or font heaps.
The common font gains a small amount of map/metric data; the Chinese fonts use
less texture memory after denser packing. Screens retaining multiple fonts,
such as the Pokédex, still require the combined resources: the currently active
main font alone is not a complete budget for every screen.

## Integrity requirements

Requirements cover every original cell and filtering border, preserved metrics/indices,
species-name and keyboard coverage, native lookup chains, TABLE holes,
idempotence, texture limits and safe build failure for unavailable donors.
These do not replace language-switch and screen-rendering tests on an emulator
or hardware.

## Format references

The original font assets, native font-switching/Pokédex code and keyboard
table layouts are the primary evidence. Independent
implementations cross-check the format, but supply no additional glyph artwork:

- [libctru: CFNT, width, texture and character-map structures](https://github.com/devkitPro/libctru/blob/master/libctru/include/3ds/font.h).
- [libctru: character lookup, linked maps and sheet coordinates](https://github.com/devkitPro/libctru/blob/master/libctru/source/font.c).
- [citro3d: texture dimension checks and A4 format](https://github.com/devkitPro/citro3d/blob/master/source/texture.c).

Format cross-checks alone do not demonstrate every game screen. Repacking also
requires pixel-level comparison with the local original resources; final visual
behavior must be verified in the corresponding application.
