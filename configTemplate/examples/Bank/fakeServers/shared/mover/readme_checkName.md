# Mover word filtering

[简体中文](readme_checkName.zh-cn.md) · [Pokémon checks](readme_checkPokemon.md) · [Server configuration](../readme_server.md)

NDS and VC share this syntax reference, not their files, switches, exceptions or
source-specific character handling.

| Source | Word-list file, relative to this directory |
| --- | --- |
| Generation V NDS | [nds/checkName.conf](nds/checkName.conf) |
| Virtual Console | [vc/checkName.conf](vc/checkName.conf) |

These paths are under `sd:/3ds/Bank/fakeServers/shared/mover/`. Guides and the
shared [license](LICENSE.badword.txt) are external documentation, not loaded by
the game. Moving them does not combine the two runtime word lists.

## File layout

```ini
[global]
enableCheckName=true;

[ngword]
^example$

[allow_nickname]
ExactNickname

[allow_ot]
ExactOT
```

Use UTF-8, optionally with BOM, and one entry per line. The example is illustrative,
not an additional preset. `enableCheckName=false` closes normal loading early without
reading the remaining list. A missing optional file adds no name restrictions.
Blank/whitespace-only lines and whole-line `#` or `;` comments are ignored.
Headers/comments may be indented, but entry text retains leading/trailing literal
spaces. Do not indent expressions for decoration. LF and CRLF are accepted.
Escape an initial literal `#` or `;`; write a space-only pattern as `[ ]` or `\x20`.

`[ngword]` is the only blocked-pattern format. `[allow_nickname]` and `[allow_ot]`
are case-sensitive, exact whole-field exceptions: no normalization, trimming,
regex or substring matching. A nickname exception does not exempt OT or legality.
Exception order does not matter. Actual native names hold at most twelve UTF-16
units; a longer exception cannot match that field.

Each of the three sections may have `.JPN/.USA/.EUR/.AUS/.CHN/.KOR/.TWN`, for
example `[allow_nickname.USA]`. This means the CTR console region, not language,
country or currency. Common and matching regional entries are combined; unknown
regions do not activate regional entries. Unrecognized sections are isolated until
the next recognized header. A missing/invalid global switch does not enable a list.

## Han characters and fan translations

Whole-Pokémon Han rejection belongs to [checkPokemon](readme_checkPokemon.md#unicodechinese-and-the-default-han-rule). Account fan permission and VC transcoding tables are described under [user.conf](../../users/readme_user.md#custom-options).
This file only provides NGWord and name exceptions, not a custom `[rule]` options section.

## NGWord syntax

| Construct | Supported form |
| --- | --- |
| Text / wildcard | Literals, escaped punctuation, `.` (not line breaks) |
| Groups / alternatives | `(...)`, `(?:...)`, `a\|b` |
| Character sets | `[abc]`, `[a-z]`, `[^abc]`; ranges require single-character ends |
| Repetition | `?`, `*`, `+`, `{m}`, `{m,}`, `{m,n}`, optional trailing lazy `?` |
| Character escapes | `\xHH`, `\uHHHH`, `\t`, `\n`, `\v`, `\f`, `\r` |
| Character classes | `\d/\D/\w/\W/\s/\S`, inside or outside sets |
| Anchors | `^`, `$`, with the NGWord outer-edge behavior below |

`\d` is ASCII digits; `\w` is ASCII letters/digits/underscore; `\s` follows
ECMAScript whitespace. `[^...]` negates a set. `[\dA]` is valid, `[a-\d]` is not.
Repetition counts are non-negative uint32 values. Only a Boolean match is returned,
not captures or greedy/lazy positions. Backreferences, lookarounds, `\b`, Unicode
properties, POSIX named classes, regex flags and capture output are unsupported.

Names are checked in original, normalized and space-removed views. The normalization
covers letter/digit width, BMP lowercase, kana width/script/small kana and halfwidth
voicing combinations, not full NFKC or Switch-specific lookalike tables. BMP lowercase
uses [Unicode data][unicode] and [unconditional special casing][casing]; context- or
language-dependent special casing is not applied. Surrogate units remain unchanged.

Unescaped outer `^`/`$` constrain the entire rule, including all top-level alternatives,
using original-name ordinary/fullwidth-space word edges. `^ab|cd$` matches whole words
ab or cd, not abx/xcd; prefer `^(ab|cd)$`. Anchors inside groups refer to the current
whole view's start/end. `^bad$` is a whole-word example; `.*bad.*` is a substring example.

An expression matching the empty string (`.*`, `()`, `a|`) is skipped. Nullable
subexpressions within a nonempty whole rule are valid: `a()b`, `(?:a|)b`.
Malformed UTF-8, embedded NUL and unsupported syntax skip only that entry; valid
entries before/after it remain active. Real I/O or allocation failures remain errors.
There is no fixed entry count, line length or nesting limit; practical limits are
available heap, 32-bit representation and checked arithmetic, not unlimited memory.

## Default content and exact-name exceptions

Both source lists import all seventeen `romfs/NgWord/18/{0..16}.txt` files from
[abcboy101/badword at a pinned revision][badword]. Its [version selector][version]
maps 18 to 3DS 11.1.0-34. UTF-16 was converted to UTF-8, exactly deduplicated and
combined with configuration headers. All 4016 unique expressions retain case and
literal spaces; all languages are used together, not selected by interface language.
No v19+ Switch word/allow/lookalike tables are included.

The editable species-name exceptions are not an extracted official server whitelist.
Names are ordered Japanese, English, French, Italian, German, Spanish, Latin American Spanish,
Korean, Simplified Chinese and Traditional Chinese, then
National Dex within each group. Shared exact values occur only in the first group;
Italian/Spanish/Latin American Spanish therefore retain headings without duplicate English values.
Pokémon names in rule explanations do not automatically become word exceptions.
Later-generation official names are included as nickname exceptions, not new native
language IDs; Han names still require the character-policy opt-in above.

Name data comes from [kwsch/PKHeX's pinned resources][names], cross-checked against
[PokéAPI's pinned name table][pokeapi] for nine distinct language sets. The Latin American
Spanish resource matches Spanish exactly. The CSV's mixed-script `負电拍拍` is excluded;
[the official Minun entry][official-minun] confirms `负电拍拍`. Source-range
selection, exact deduplication, generation case/punctuation, historical names and
verified stored Nidoran glyphs are applied using [generation-name][generation] and
[NDS string encoding][encoding-nds] and [VC string encoding][encoding-vc] references. These are community format references,
not proof of the original server's exact exception policy. The original source-language
references are Nintendo's [language page][languages] and [Korean manual][korean]. Later
additions are supported by the [Chinese-language announcement][chinese] and
[official Latin American Spanish announcement][latin-spanish].

Only official game spellings and documented official changes are included, not regional
old translations or community aliases. Within each list's Dex range, the
[March 2020 notice][official-change-march] retains `多边兽Ⅱ` / `多边兽２型` and,
where applicable, `多边兽Ｚ` / `多边兽乙型`; the [October notice][official-change-october]
retains `死神棺` / `迭失棺`. Generation-specific French/Korean spellings and verified stored
glyphs remain. Changes to species outside the source's 251/649 scope are not imported.

### NDS

The preset adds 5225 exact species-name/glyph values for Dex 001–649. Gen5 reads native PK5 16-bit name fields; stored gender glyphs can differ from ordinary Unicode. Generation-specific uppercase and punctuation variants, including the straight apostrophe in Farfetch'd, are explicit entries rather than automatic exemptions.

NDS additions: exact nickname exceptions `omfg`, `bbwtzh`, `bbwten`; exact OT exception `せいしゃ`; blocked pattern `^adhd$`. These values do not exempt longer strings, other capitalization or the opposite field. Name-correction result codes are 250/251/252 for nickname/OT/both.

### VC

The preset adds 2134 exact species-name/glyph values for Dex 001–251. Korean covers Gold/Silver, not Generation I or Crystal. Mover first converts VC strings to its PK7 candidate; the service reads those UTF-16 fields, not zero-extended raw VC bytes. Use the converted spelling/glyphs in exact exceptions. MR.MIME / M.MIME, old French TETARTE and Korean 수륙챙이 / 덩구리 are retained alongside current spellings. The v1.2-only MR-MIME / FARFETCH D bugs are not preset. See the [name/transcoding notes][censored] and [VC converter][encoding-vc].

VC USA-console nickname exceptions: `bbwjjp`, `bbwefr`, `bbwees`, `bbwpen`, `bbwpfr`, `bbwpes`, `bbwkko`, `bbwtzh`, `bbwten`, `adhd`. `bbween` and `con` still undergo blocking; no global-region exemption is inferred. The added pattern `[ㅘㅙㅚㅝㅞㅟㅢ괻읆]` covers the nine documented unsupported Hangul characters. Name-correction result codes are 253/254/255 for nickname/OT/both.

Compatibility additions are limited community observations, principally one
[word-filter test record][word-tests], not a complete official v1.5 list. The
[Transporter name notes][censored] provide version/character context. Do not infer
unrestricted kana allowances or global exceptions from one console region.

## Reload and results

The current file is loaded once per relevant policy request; slot/name matching
uses RAM, not another SD read per name. A later request sees edits; an already
returned result is not recomputed. Invalid individual entries are skipped. The patch never writes a word list or its siblings. Rules are not mirrored into another configuration bin.

Ordinary NGWord nickname checks apply to nicknamed records. The Han restriction has
no nickname-flag exemption; custom mode also checks retained unnicknamed Han names
against NGWord. OT is always checked. Ordinary word hits return
the original name-correction result; the disabled-Han Gen5 character check instead
returns slot rejection `1` before that stage. The original receiver
performs the actual rename, prompts and save. Its initial explanatory name notice is
not proof that the word list matched. Name corrections take priority over fan-name
restoration. Set enableCheckName=false; in this file's [global] to disable name
screening without disabling fan conversion or checkPokemon filtering.
Exact-name exceptions do not change nickname
flags, rename a Pokémon or verify that the nickname equals its actual species.

## Attribution and licensing

NGWord source: abcboy101/badword, revision
`1a76c29d53f2f6e98c23b6849215e99801c4224b`, GPL-3.0; source paths and transformations
are above. Species resources/format references: kwsch and PKHeX contributors,
revision `641c26ba100225f3605e5a44b61b3b16e15a3774`, [GPL version 3][pkhex-license].
The complete, unmodified GPL text is [LICENSE.badword.txt](LICENSE.badword.txt).
The combined NDS/VC presets contain sixteen compatibility records in addition to
the unchanged NGWord corpus and species-name exceptions. Source and editable data
remain available to recipients through the links and supplied text files.
Matching is independently implemented; no upstream TypeScript/Python, PKHeX or
Nintendo native filtering implementation is copied into the C patch. The public
reference is not proof of identical official-server behavior.

PokéAPI supplies the secondary species-name cross-check; no executable PokéAPI
code is included. Names and data remain
subject to their owners' rights. Its required BSD-3-Clause notice follows.

```text
Copyright (c) © 2013–2023 Paul Hallett and PokéAPI contributors
(https://github.com/PokeAPI/pokeapi#contributing).
Pokémon and Pokémon character names are trademarks of Nintendo.
All rights reserved.

Redistribution and use in source and binary forms, with or without modification,
are permitted provided that the following conditions are met:
* Redistributions of source code must retain the above copyright notice,
  this list of conditions and the following disclaimer.
* Redistributions in binary form must reproduce the above copyright notice,
  this list of conditions and the following disclaimer in the documentation
  and/or other materials provided with the distribution.
* Neither the name of PokéAPI nor the names of its contributors may be used
  to endorse or promote products derived from this software without specific
  prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
```

[badword]: https://github.com/abcboy101/badword/tree/1a76c29d53f2f6e98c23b6849215e99801c4224b
[version]: https://github.com/abcboy101/badword/blob/1a76c29d53f2f6e98c23b6849215e99801c4224b/web/index.html
[names]: https://github.com/kwsch/PKHeX/tree/641c26ba100225f3605e5a44b61b3b16e15a3774/PKHeX.Core/Resources/text/other
[pokeapi]: https://github.com/PokeAPI/pokeapi/blob/81d20919845b6a445969b9265aab5cec6b04cece/data/v2/csv/pokemon_species_names.csv
[generation]: https://github.com/kwsch/PKHeX/blob/641c26ba100225f3605e5a44b61b3b16e15a3774/PKHeX.Core/PKM/Util/SpeciesName.cs
[encoding-nds]: https://github.com/kwsch/PKHeX/blob/641c26ba100225f3605e5a44b61b3b16e15a3774/PKHeX.Core/PKM/Strings/StringConverter5.cs
[languages]: https://www.nintendo.com/nl-nl/Games/Nintendo-3DS-games/Pokemon-Sun-1092368.html
[korean]: https://www.nintendo.com/kr/pdf/pkm_mov.pdf
[chinese]: https://www.nintendo.com/en-za/News/2016/February/New-Pokemon-games-announced-via-Pokemon-Direct-1088288.html
[word-tests]: https://bulbapedia.bulbagarden.net/wiki/Talk:Pok%C3%A9_Transporter#Word_filter_tests
[censored]: https://bulbapedia.bulbagarden.net/wiki/Pok%C3%A9_Transporter#Censored_words
[unicode]: https://www.unicode.org/Public/17.0.0/ucd/UnicodeData.txt
[casing]: https://www.unicode.org/Public/17.0.0/ucd/SpecialCasing.txt
[pkhex-license]: https://github.com/kwsch/PKHeX/blob/641c26ba100225f3605e5a44b61b3b16e15a3774/LICENSE
[official-change-march]: https://web.archive.org/web/20200505203356/https://cn.portal-pokemon.com/topics/event/200323190120_post_19.html
[official-change-october]: https://web.archive.org/web/20201020125159/https://cn.portal-pokemon.com/topics/event/201020170000_post_21.html
[official-minun]: https://dex.pokemon.cn/play/pokedex/0312
[latin-spanish]: https://www.pokemon.com/el/videojuegos-pokemon/leyendas-pokemon-z-a
[encoding-vc]: https://github.com/kwsch/PKHeX/blob/641c26ba100225f3605e5a44b61b3b16e15a3774/PKHeX.Core/PKM/Strings/StringConverter12Transporter.cs
