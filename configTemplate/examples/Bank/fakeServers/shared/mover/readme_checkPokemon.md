# Mover Pokémon filtering

[简体中文](readme_checkPokemon.zh-cn.md) · [Word filtering and attribution](readme_checkName.md) · [Server configuration](../readme_server.md)

NDS and VC use the same syntax described here but keep separate files and global
switches. One source never loads the other source's rules.

| Source | Configuration file, relative to this directory |
| --- | --- |
| Generation V NDS | [nds/checkPokemon.conf](nds/checkPokemon.conf) |
| Virtual Console | [vc/checkPokemon.conf](vc/checkPokemon.conf) |

These paths are under `sd:/3ds/Bank/fakeServers/shared/mover/`. Missing/malformed
optional data discards only that source's additional rules, not the other source's
policy or the original local checks. This is an editable compatibility preset,
not a complete extracted official v1.5 server legality table. Guides and licenses
are external documentation, not loaded by the game.

## Layout and editing

Start with `[global]` and `enableCheckPokemon=true;` or `enableCheckPokemon=false;`.
False skips the remaining rules during loading. Only shared fan permission may automatically disable BanUnicodeChinese; other rules are not edited. Each rule runs from `[rule.name]` to the next rule header or end of file.
Names accept 1–31 uppercase/lowercase ASCII letters, digits or underscores, are local
labels only, and are not written into game/bank saves.

Each rule requires exactly three unique fields: `enabled=0/1;`,
`action=reject/require;`, `condition=expression;`. Headers have no semicolon;
fields end at an unquoted `;`. Conditions may span lines and contain whitespace.
`#` comments run to the end of the physical line; quoted `#`/`;` are literal text.
Block comments, backslash continuations and literal newlines inside strings are unsupported.

```ini
[global]
enableCheckPokemon=true;

[rule.025_custom_example]
enabled=0;
action=require;
condition=
    species == 25 &&
    level >= 5 &&
    (ot == "Example" || ot == "Other");
```

This disabled custom example illustrates syntax, not an official restriction.
Remove a rule or set `enabled=0;` to disable it. The directory determines its source;
there is no `source`, `group`, `blocked_species` or `action=allow` field.

## Expressions

`&&` means AND, `||` means OR; AND binds more tightly, with nested parentheses
available. Write each comparison completely: `species == 25 || species == 26`,
not `species == 25 || 26` or `species=25,26`. Comparisons are `== != < <= > >=`.
No arithmetic, assignments, calls, variables, single `&/|` or unary `!` are supported.
Numbers are non-negative decimal uint32 values; overflow is rejected.

| Field | Meaning |
| --- | --- |
| `species` | National Dex number. Use complete OR comparisons for multiple species. |
| `form` | Native observed form number. |
| `level` / `met_level` | Current level / recorded encounter or transfer level. |
| `shiny` / `fateful` | Shiny / fateful-encounter flag, `true/false` or `1/0`. |
| `tid` / `sid` | Native Trainer ID / secret ID. |
| `language` / `origin` | Pokémon's native language / original game, not UI language or the currently selected input game. |
| `move` | `==` matches any of four slots; `!=` requires absence from all four. Other comparisons are unsupported. |
| `duplicate_moves` | Whether a nonzero move occurs twice; repeated empty slots do not count. |
| `UnicodeChinese` | Whether nickname or OT contains CJK Han; true/false or 1/0, independent of name exceptions. |
| `ot` | Quoted UTF-8, decoded to exact native UTF-16; only `==/!=`, case/space/glyph sensitive. Escape quotes/backslashes with a backslash. |

OT has the native twelve-UTF-16-unit capacity. NDS reads PK5 fields; VC reads the
stock-converted PK7 candidate. Language IDs are shared; source-specific origin and
form mappings follow below.

### Rule decisions

`reject` denies when the whole expression is true. `require` derives its applicable
species from this same expression, preserving AND/OR/parentheses but treating other
comparisons as unrestricted scope, then requires the complete condition for that scope.
An OR branch without a species restriction makes that branch's scope all species;
a require without any species comparison applies globally.

Rules are independent: any matching reject, or failed applicable require, denies.
They do not combine conditions across rules or override structural/checksum,
valid-empty handling or the original fourteen local checks. Name correction
belongs to the separate [word policy](readme_checkName.md) policy, not species rejection.

### UnicodeChinese and the default Han rule

`UnicodeChinese` is true when nickname or OT contains a listed CJK block, regardless
of language ID, nickname flag or name whitelist. The default ordinary rule is:

```ini
[rule.BanUnicodeChinese]
enabled=1;
action=reject;
condition=
    UnicodeChinese == true;
```

A hit returns native per-slot rejection 1, excluding the entire Pokémon without a
rename. NGWord independently uses NDS 250–252 / VC 253–255. Gen5 default rejection is
supported by the user's hardware observation; the same VC rejection is a custom local
policy, not a recovered exhaustive official server rule. Shared [fan permission](../readme_server.md#allowfantranslationchn)
actually writes this rule's enabled=0 with protected publication, without disabling
other rules. Turning permission off does not re-enable it.

The [Unicode 17 blocks](https://www.unicode.org/Public/17.0.0/ucd/Blocks.txt) include
reserved positions. Kana, Hangul, PUA, punctuation and radical blocks are not blanket
restricted; supplementary code points use bounded UTF-16 surrogate pairs.

| Range | Content |
| --- | --- |
| U+3400–4DBF, U+4E00–9FFF | Extension A, basic unified ideographs |
| U+F900–FAFF, U+2F800–2FA1F | Compatibility ideographs and supplement |
| U+20000–2A6DF, U+2A700–2B73F | Extensions B, C |
| U+2B740–2B81F, U+2B820–2CEAF | Extensions D, E |
| U+2CEB0–2EBEF, U+2EBF0–2EE5F | Extensions F, I |
| U+30000–3134F, U+31350–323AF, U+323B0–3347F | Extensions G, H, J |

### language values

These are decimal PKM language IDs, not the Bank/Mover UI language, country,
CTR region or the section suffix in checkName.conf. Write a numeric comparison such
as `language == 2`; symbolic names such as `language == ENG` are not supported.

| Value | Language | Relevance to these inputs |
| --- | --- | --- |
| 0 | Unset | Not an automatic rejection: some Japanese Gen5 in-game trades retain 0. |
| 1 | Japanese | Native source-game language. |
| 2 | English | Includes US/UK/Australian English; not a region selector. |
| 3 | French | Native source-game language. |
| 4 | Italian | Native source-game language. |
| 5 | German | Native source-game language. |
| 6 | Unused/reserved | Not Korean. |
| 7 | Spanish | Native source-game language. |
| 8 | Korean | VC Korean games are Gold/Silver, not Gen1/Crystal. |
| 9 | Simplified Chinese | Introduced later; not an original Gen5/VC source language. |
| 10 | Traditional Chinese | Introduced later; not an original Gen5/VC source language. |
| 11 | Latin American Spanish | Later than the v1.5 source games. |

The later-language nickname whitelist does not assign IDs 9–11 to a Pokémon or
make a non-native language record legal. Values are observed, not repaired.
Mapping: [PKHeX LanguageID][language-ids]; the patch reads the PK5 field or the
stock-converted PK7 field without remapping it.

### origin values

#### NDS

These are the original game IDs stored in PKM data, not the currently selected
NDS game or this project's client game IDs. A Pokémon read from Black can retain
`origin == 11` (Pearl). In particular, White is 20 and Black is 21.

| Value | Original game |
| --- | --- |
| 0 | Unset/unknown, not a wildcard |
| 1 | Sapphire |
| 2 | Ruby |
| 3 | Emerald |
| 4 | FireRed |
| 5 | LeafGreen |
| 7 | HeartGold |
| 8 | SoulSilver |
| 10 | Diamond |
| 11 | Pearl |
| 12 | Platinum |
| 15 | Colosseum / XD (shared ID) |
| 16 | Battle Revolution (stored game ID; not a promise of accepted origin) |
| 20 | White |
| 21 | Black |
| 22 | White 2 |
| 23 | Black 2 |

Unused gaps are not aliases for another game. Values for later games are not
normal Gen5 origins. See the stored-value section of [GameVersion][game-ids],
not its later editor-only version-group enum members.

#### VC

VC checks inspect the original Mover's converted PK7 candidate, not raw GB/GBC
header bytes. Its VC origin IDs are separate from NDS/3DS selection-menu IDs.

| Value | VC original game |
| --- | --- |
| 35 | Red |
| 36 | Green (Japan) / Blue (international) |
| 37 | Blue (Japan) |
| 38 | Yellow |
| 39 | Gold |
| 40 | Silver |
| 41 | Crystal |

For example, `origin == 41` selects a Crystal-origin candidate, not all Pokémon
currently read from a Crystal save. Do not use 1/2 to mean VC Red/Blue, or 30/31
(Sun/Moon) just because the candidate is in PK7 format. Unset 0 is not a wildcard.
Mapping: [GameVersion VC stored IDs][game-ids].

### form values

#### NDS

`form` is species-specific, not a global form enum. Always combine it with
`species`. Gender differences, shiny colors and Spinda spots are not alternative
values of this field. Single-form species normally use 0.

Common Gen5 mappings follow. This is a value reference, not a transfer allowlist;
battle-only, item-dependent and fused forms still undergo the original checks.

| Species | Form values |
| --- | --- |
| 201 Unown | 0–25 = A–Z; 26 = !; 27 = ? |
| 351 Castform | 0 Normal; 1 Sunny; 2 Rainy; 3 Snowy |
| 386 Deoxys | 0 Normal; 1 Attack; 2 Defense; 3 Speed |
| 412 Burmy / 413 Wormadam | 0 Plant Cloak; 1 Sandy Cloak; 2 Trash Cloak |
| 421 Cherrim | 0 Overcast; 1 Sunshine |
| 422 Shellos / 423 Gastrodon | 0 West Sea; 1 East Sea |
| 479 Rotom | 0 Normal; 1 Heat; 2 Wash; 3 Frost; 4 Fan; 5 Mow |
| 487 Giratina | 0 Altered; 1 Origin |
| 492 Shaymin | 0 Land; 1 Sky |
| 493 Arceus | 0 Normal; 1 Fighting; 2 Flying; 3 Poison; 4 Ground; 5 Rock; 6 Bug; 7 Ghost; 8 Steel; 9 Fire; 10 Water; 11 Grass; 12 Electric; 13 Psychic; 14 Ice; 15 Dragon; 16 Dark |
| 550 Basculin | 0 Red-Striped; 1 Blue-Striped |
| 555 Darmanitan | 0 Standard; 1 Zen |
| 585 Deerling / 586 Sawsbuck | 0 Spring; 1 Summer; 2 Autumn; 3 Winter |
| 641 Tornadus / 642 Thundurus / 645 Landorus | 0 Incarnate; 1 Therian |
| 646 Kyurem | 0 Normal; 1 White; 2 Black (fused forms are not transferable) |
| 647 Keldeo | 0 Ordinary; 1 Resolute |
| 648 Meloetta | 0 Aria; 1 Pirouette |
| 649 Genesect | 0 No Drive; 1 Douse; 2 Shock; 3 Burn; 4 Chill |

For example, `species == 479 && form == 2` selects Wash Rotom, not every
Pokémon whose form is 2. Use the Gen5 context in [FormConverter][form-ids];
later Fairy Arceus, White-Striped Basculin, regional/Mega forms are not Gen5 IDs.

#### VC

This is the form already assigned by the stock VC-to-PK7 conversion, not an
arbitrary raw GB/GBC byte. Most VC species have form 0. Unown (201) uses
0–25 for A–Z; ! / ? (26/27) do not exist in Generation II.

Use `species == 201 && form == 1` for Unown B. Gender differences, shiny colors
and Spinda spots are not form selectors. Later Alolan/regional, Mega and Pichu's
HGSS Spiky-eared form are not VC input forms. Known native conversion quirks
are still handled by the original program, not a new form allowlist here.
Mapping: [FormConverter's Unown/Gen2 handling][form-ids].

## Loading and failure handling

One relevant request loads/compiles once; slot checks use RAM, not more SD reads.
Later requests read current text, without recomputing an already delivered decision.
Rule counts, expression length and parser depth grow dynamically within available
heap and checked 32-bit representation; configuration is not executed as C code.
No configuration binary mirror is used.

Missing semicolons/fields, duplicate fields, invalid parentheses or unsupported
syntax invalidate this source's additional table as a whole, not a partial policy.
This differs from word lists, where an invalid entry is skipped individually.
Real I/O/allocation failures remain resource errors. The original local checks
continue even with no enabled additional rules.

## Default rules and evidence

These conditions are independently arranged for this patch. Community rejection
reports and editor legality results are not interchangeable; neither proves a
complete official server algorithm. [Nintendo's version page][mover-version]
dates v1.5 to 2020-02-11 and requires updating for online use. An older experiment
is historical evidence; a later report supports the v1.5 era but not an immutable
server policy. Where evidence is single-source, inferred or not a controlled
one-variable test, the table says so.

### NDS rules

Four rules are enabled. Number prefixes describe the minimum/maximum IDs in a combination, not every species between them.

| Rule | Condition | Evidence and limits |
| --- | --- | --- |
| `BanUnicodeChinese` | Enabled: reject the whole Pokémon when nickname or OT contains Unicode Han. | User Gen5 hardware observation; equivalent VC behavior is a custom policy. See the ranges and fan permission above. |
| `251_648_shiny_locked` | Reject shiny Celebi, Arceus, Victini, Keldeo or Meloetta. | [Historical summary][pp-bans], [2016 samples][pp-sample], [2021 Arceus discussion][arceus-case], [Celebi feedback][nds-celebi]. Community evidence includes confounding variables; not a controlled full v1.5 table. |
| `491_darkrai_pearl_fateful0` | Reject Darkrai with `origin == 11 && fateful == false`. | [Pearl rejection report][gcl-void] and [2023 discussion][pp-pearl]. This field combination is a local inference, not a proven isolated server predicate. |
| `492_shaymin_dp_fateful0` | Reject Shaymin with `(origin == 10 \|\| origin == 11) && fateful == false`. | [2023 discussion][pp-pearl] changes more than one field; the exact tuple is inferred, without isolated-field reproduction. |

The Project Pokémon threads have previously required readable indexed text when direct access returned 403; forum advice is not an official algorithm. The [Glitch City archive][gcl-void] reports Pearl Darkrai still failing after raising it to 50. These rules do not detect a particular exploit or prove that a fateful flag alone guarantees acceptance.

No default blanket ban is added for shiny Reshiram/Zekrom, non-shiny void Arceus, language-only Mew cases, all event Pokémon or all HM moves. Conflicting/successful reports include [Arceus][arceus-conflict], [English shiny Mew][english-mew] and [HM-stage distinctions][hm-distinction]. VC restrictions are not applied to NDS.

Other reported conditions are not silently enforced: [EV totals][ev-research] need a confirmed observation stage, and [Farfetch'd name formatting][farfetch-name] is a source-name consistency report, not a blanket nickname ban. [Rage-glitch moves][rage-moves], a [Rayquaza rejection question][nobunaga-case] and [editor PP warnings][pp-overflow] do not establish a complete per-species move table. HOME trading/OT restrictions concern a different service, not this preset. Successful counterexamples and uncertain reports remain reasons to narrow rules, not to invent extra client checks.

### VC rules

Seven rules are supplied: five enabled, two explicitly disabled. Changing a disabled candidate to 1 opts into an older observation, not a verified current v1.5 restriction.

| Rule | Condition / default | Evidence and limits |
| --- | --- | --- |
| `BanUnicodeChinese` | Enabled: reject the whole Pokémon when nickname or OT contains Unicode Han. | User Gen5 hardware observation; equivalent VC behavior is a custom policy. See the ranges and fan permission above. |
| `025_240_dizzy_punch` | Enabled: reject the nineteen listed Odd Egg family members when any move is Dizzy Punch (146). | [Legality summary][bp-legality], [Crystal Odd Egg data][odd-eggs], [evolution family data][odd-family], [Hitmonchan report][odd-case], [Elekid report][odd-recent] and [move research][odd-research]. The nineteen-member closure is inferred, not nineteen controlled rejection tests. |
| `115_duplicate_moves` | Disabled: reject Kangaskhan with duplicate nonzero moves. | [2017 paired Water Gun experiment][vc-duplicate]. A single historical experiment; other duplicate moves are inferred and no v1.5 controlled retest is established. |
| `144_146_min_level50` | Disabled: require Articuno/Zapdos/Moltres level at least 50. | [2017 research][vc-floor-history] includes Zapdos passing after leveling. Lacks separate modern boundary tests for all three. |
| `150_mewtwo_min_level70` | Enabled: require current level at least 70. | [2020 level-five report and level-seventy response][mewtwo-2020], [2022 low-level report][mewtwo-2022], plus [earlier research][vc-floor-history]. Multiple community sources, not a one-variable server-code proof. |
| `151_mew_ot_tid22796` | Enabled: require `tid == 22796 && (ot == "GF" \|\| ot == "ゲーフリ")`; no additional shiny ban. | [Legality summary][bp-legality] and [distribution/transfer research][mew-report]. |
| `251_celebi_min_level30` | Enabled: require current level at least 30, not met level; no blanket shiny ban. | [Legality summary][bp-legality] and [community answer][celebi-level]. No controlled 29/30 boundary pair; secondary answer has previously been available only through indexed text. |

Dizzy Punch applies to IDs 25,26,35,36,39,40,106,107,124,125,126,172,173,174,236,237,238,239,240. Seven babies and their Generation I–II evolution closure establish family membership, not server rejection by themselves. Kangaskhan/Smeargle are not in that rule. The public Crystal data is a format/data reference, not Nintendo server source.

No extra ban is added for every shiny Mew/Celebi, levels above 100, or VC IDs 252–255; original conversion/local checks remain. [The Pay Day Mewtwo correction][payday-fixed] is a counterexample to older advice, not a new ban. No independent event-origin proof is required beyond the configured Mew fields. HM restrictions from a different transfer stage and HOME trading restrictions are not imported here.

[arceus-case]: https://www.reddit.com/r/ShinyPokemon/comments/o435ib/
[arceus-conflict]: https://projectpokemon.org/home/forums/topic/39815-unreleased-prototype-pokemon/page/30/
[english-mew]: https://www.reddit.com/r/pokemonrng/comments/1atxe96/
[ev-research]: https://bnossu.hatenablog.com/entry/2025/07/20/191222
[farfetch-name]: https://github.com/kwsch/PKHeX/issues/4066
[gcl-void]: https://archives.glitchcity.info/forums/board-110/thread-7845/page-1.html
[hm-distinction]: https://www.reddit.com/r/pokemon/comments/r8z7ox/
[mover-version]: https://en-americas-support.nintendo.com/app/answers/detail/a_id/25447
[language-ids]: https://raw.githubusercontent.com/kwsch/PKHeX/641c26ba100225f3605e5a44b61b3b16e15a3774/PKHeX.Core/Game/Enums/LanguageID.cs
[game-ids]: https://raw.githubusercontent.com/kwsch/PKHeX/641c26ba100225f3605e5a44b61b3b16e15a3774/PKHeX.Core/Game/Enums/GameVersion.cs
[form-ids]: https://raw.githubusercontent.com/kwsch/PKHeX/641c26ba100225f3605e5a44b61b3b16e15a3774/PKHeX.Core/PKM/Util/Conversion/FormConverter.cs
[nds-celebi]: https://www.reddit.com/r/pokemonrng/comments/18rjkwp/
[nobunaga-case]: https://detail.chiebukuro.yahoo.co.jp/qa/question_detail/q13293642568
[pp-bans]: https://projectpokemon.org/home/forums/topic/29147-pokebank-banned-pokemon/
[pp-overflow]: https://projectpokemon.org/home/forums/topic/55864-pp-show-up-incorrectly-in-pkhex-but-not-in-the-game-on-one-pokemon/
[pp-pearl]: https://projectpokemon.org/home/forums/topic/63625-unable-to-match-an-encounter-from-origin-game-darkrai-shaymin/
[pp-sample]: https://projectpokemon.org/home/forums/topic/38312-whats-wrong-with-these-pkms-rejected-by-poke-transporter/
[rage-moves]: https://bulbapedia.bulbagarden.net/wiki/Rage_glitch
[bp-legality]: https://bulbapedia.bulbagarden.net/wiki/Pok%C3%A9_Transporter#Legality_checking
[celebi-level]: https://www.reddit.com/r/pokemoncrystal/comments/1i7jvmr/
[mew-report]: https://projectpokemon.org/home/forums/topic/39235-vc-rby-virtual-console-mew-uk/page/5/
[mewtwo-2020]: https://www.reddit.com/r/pokemon/comments/fp34py/
[mewtwo-2022]: https://medaka.5ch.io/test/read.cgi/poke/1646476301
[odd-case]: https://www.reddit.com/r/pokemon/comments/f9akpf/
[odd-eggs]: https://raw.githubusercontent.com/pret/pokecrystal/master/data/events/odd_eggs.asm
[odd-family]: https://raw.githubusercontent.com/PokeAPI/pokeapi/81d20919845b6a445969b9265aab5cec6b04cece/data/v2/csv/pokemon_species.csv
[odd-recent]: https://www.reddit.com/r/pokemoncrystal/comments/1vu7i6k/poke_transporter/
[odd-research]: https://www.smogon.com/forums/threads/comprehensive-list-of-moves-exclusive-to-the-virtual-console-games.3577468/post-10611983
[payday-fixed]: https://www.smogon.com/forums/threads/bug-reports-v2-0-read-op-before-posting.3469932/page-401
[vc-duplicate]: https://archives.glitchcity.info/forums/board-107/thread-7799/page-1.html
[vc-floor-history]: https://archives.glitchcity.info/forums/board-107/thread-7799/page-3.html
