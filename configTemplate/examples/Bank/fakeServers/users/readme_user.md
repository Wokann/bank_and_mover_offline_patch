# Personal user configuration

[简体中文](readme_user.zh-cn.md) · [Shared server configuration](../shared/readme_server.md) · [Client and user selection](../../client/readme_client.md) · [Layout and protection](../../readme_fakeServer.md)

This guide is shared by all accounts. The actual configuration is
`sd:/3ds/Bank/fakeServers/users/<userID>/user.conf`. These guide files are not user
IDs and do not participate in account selection. Do not duplicate them per account
or put personal balances in shared server.conf.

## Format and fields

Generated for the selected user on first use. Locale numbers below illustrate a Japanese console; actual creation captures the current host. Pass records and `balance.$currency` share `[ticket]`, separated only by a blank line, without a separate balance section:

```ini
[console]
autoRegion=false;
region=0;
language=0;
country=1;
state=2;

[ticket]
purchased=false;
paid_periods=0000-00-00..0000-00-00;
trial_claimed=false;
annual_count=0;

autoCurrency=true;
currentCurrency=$JPY;
balance.$NONE=0;
balance.$JPY=0;
balance.$USD=0;
balance.$GBP=0;
balance.$EUR=0;
balance.$KRW=0;
balance.$TWD=0;
balance.$HKD=0;
balance.$AUD=0;
balance.$NZD=0;
balance.$CAD=0;
balance.$CHF=0;
balance.$ZAR=0;
```

| Key | Purpose |
| --- | --- |
| purchased | False ignores paid_periods for entitlement even if its dates remain valid; other records are retained. |
| paid_periods | Pass validity with the same multi-range/regional syntax. True still requires a matching date. Invalid data does not invent an expiry. |
| trial_claimed | Whether the trial was claimed; false alone does not force an offer. |
| annual_count | Annual purchase counter, not a duration or first-bank flag. |
| balance.$currency | This user's balance; missing/invalid resets to zero, not another user's funds. |

Trials also require the stock client's first-bank-creation state, enabled trial, a matching claim window, no current valid pass, no current free access and trial_claimed=false. An expired annual pass or nonzero annual_count does not alone disqualify a trial. Neither purchased nor annual_count establishes first creation.

Annual passes normally add 365 days, extending current validity or starting a new interval when expired; the original flow queries validity again. Free activities, paid validity and trial records coexist. Free access takes precedence while personal pass records remain stored.

## Custom options

Place these project-specific options at the end of the selected account's `user.conf`. Both default off when created or missing, affect only that account, and are not read from shared `server.conf`:

```ini
[custom]
enableMoverv1.2=false;
allowFanTranslationCHN=false;
```

### enableMoverv1.2

Offline RGBY only: retain constructor-generated random gender using the species ratio,
rather than overwriting it from Attack DV. Shiny requires Attack DV 10, Defense DV in
2/3/6/7/10/11/14/15, and Speed/Special DV both 10. The old Japanese/Western character
maps also apply, including Western 0xE8 period becoming a hyphen, 0xE0 apostrophe becoming
a space, and unsupported digits/diacritics. Fixed-gender and genderless species remain native.

Off preserves v1.5. Gen2, Gen5 and online paths never enter these wrappers. This does not
roll back server screening, Bank format, transactions, locale or the whole application,
and cannot recreate a historical server. See the [conversion reference](https://bulbapedia.bulbagarden.net/wiki/Pok%C3%A9_Transporter#From_Generation_I_and_II), a community source rather than a complete official algorithm specification.

### allowFanTranslationCHN

For this account, the loaded `BanUnicodeChinese` rule is always disabled in RAM, whether the shared file says `enabled=0;` or `enabled=1;`. Shared `checkPokemon.conf` is never changed; no `.bak`/`.tmp`, missing rule or missing file is created. Other accounts retain their own settings and shared-rule behavior. Leave the shared rule at `enabled=1;` to control this exception per account; a shared `enabled=0;` disables that rule for every account.

- NDS already uses UTF-16: retain Chinese nickname/OT, including unnicknamed Han names
  otherwise replaced by a default species name. The nickname flag itself is not fabricated.
- VC uses the English Crystal table for raw alphabet 1, Korean Gold/Silver for alphabet 6.
  Source means string encoding, not UI/console/wrapper region. Korean-based international
  fan patches still use the Korean table. Ordinary Korean bytes can conflict: leave off
  for untranslated Korean saves.
- Only generated candidate names change, not source saves. Unknown/truncated pairs fall
  back for the whole name, never publishing a partial conversion.
- NGWord, other screening, native checksums/encryption, callbacks and transactions remain.
  This permission is independent of checkName being enabled/present. If both custom
  options are on, the explicit fan table restores Chinese after legacy conversion.
  A requested nickname/OT correction takes priority: restoration never resurrects that
  field's rejected name. To retain Chinese without name screening, set enableCheckName=false;
  in the selected source's checkName.conf [global]. Pokemon screening remains independent.

Conversion options accompany the background Bank download; later policy requests read
current rule/name files. Mid-operation edits do not retroactively transcode candidates;
return to the title and reconnect after changing conversion options.

Each table has one project-specific source, not an official encoding specification;
other hacks, projects and revised tables are not covered:

- [TomJinW/pokecrystal_cn](https://github.com/TomJinW/pokecrystal_cn/tree/f487d44c0d6f75a48a4a4b975ca04bc3914b3f52):
  pinned [charmap_cn.asm](https://github.com/TomJinW/pokecrystal_cn/blob/f487d44c0d6f75a48a4a4b975ca04bc3914b3f52/constants/charmap_cn.asm), 7615 two-byte mappings.
- [TomJinW/PKMN_GSCHS](https://github.com/TomJinW/PKMN_GSCHS/tree/c3947b83c6c66a015103e1bc08bbca7f2a862d3b):
  pinned final [fontTBL.txt](https://github.com/TomJinW/PKMN_GSCHS/blob/c3947b83c6c66a015103e1bc08bbca7f2a862d3b/05_Font/fontTBL.txt), 2350 mappings.

No ROMs, upstream patches or font images are distributed. The NGWord
GPL text does not automatically license these separate references under the same terms.


## Console locale and currency

The selected user's `[console]` is private. `autoRegion=false` is the default: missing fields are initially captured from application-visible CFG as fixed numbers. Existing valid values do not refresh merely because files move to another console or Luma locale.

| Field | Meaning |
| --- | --- |
| autoRegion | `false` retains the four manual numbers below. `true` refreshes and saves them from the console startup snapshot when that account is first confirmed after each application launch, not on every read. Failed host fields retain existing valid values. |
| region | CFG console-region ID 0–6, not game version, UI language or origin region. AUS=3 is reserved; see combinations below. |
| language | CFG system-language ID 0–11, not a Pokemon language ID. Recorded as environment metadata; these v1.5 conversions use the three geographic fields, not this field to override Pokemon language or the title's L-button UI-language flow. |
| country | CFG country ID 0–255, not a two-letter ISO string. |
| state | Subdivision ID 0–255 under that country; not a globally interchangeable ID. |

The region switch and four numbers belong to `[console]`. The two currency lines belong to `[ticket]`, immediately above the balances:

| Field | Meaning |
| --- | --- |
| autoCurrency | Defaults to `true`: selects a currency from this user's effective region/country and updates currentCurrency. `false` uses the next line manually, even if it conflicts with the region. |
| currentCurrency | `$NONE`, `$JPY`, `$USD`, `$GBP`, `$EUR`, `$KRW`, `$TWD`, `$HKD`, `$AUD`, `$NZD`, `$CAD`, `$CHF` or `$ZAR`. Displays the selected currency in automatic mode; supplies the actual choice in manual mode. An unmapped locale uses the independent $NONE shop/wallet; funds are never exchanged or merged. |

Title up/down selection only changes the display and activeUser, not the accounts' user.conf files. Automatic settings apply after touch/START confirms entry. Revisiting an already-used account during that process does not refresh its region again. Enabling autoRegion mid-run takes effect next launch. Automatic currency follows the user's effective locale rather than bypassing fixed values to query the live host. Later reads use current configuration; delivered client results are not recalculated.

Numbers accept decimal or 0x-prefixed hexadecimal: country=1 and Tokyo state=2; state=0x0A is decimal 10. Bare Luma hexadecimal 10 is not hexadecimal here. autoRegion controls automatic behavior; individual numeric fields do not accept auto. `none` means unavailable/no override, not an automatic switch. Invalid/out-of-range entries repair only that field; an invalid duplicate cannot erase an earlier valid value. Unlisted raw byte values can be retained manually but are not guaranteed accepted by stock software or other tools.

Bank's offline free/trial/paid windows, gift region conditions and currency policy use the user's effective locale. Mover's offline native conversion assigns the same user locale to newly converted Pokemon, ahead of Luma fixed settings. This feature does not rewrite console-region fields of existing Bank Pokemon, override source Pokemon language, or alter native Online/Unlock/Download session locale. File changes apply on subsequent actual reads, not retrospectively to already delivered conversion results.

## Backup and recovery

Locale, currency, pass records and balances publish in one user.conf image, not two independently
committed files. The pre-write primary remains as user.bak; user.tmp is staging.
If the primary is missing, backup values are read and republished. Only absence of
both uses unpurchased/unclaimed/zero-counter/zero-balance defaults. Valid manual
values survive per-entry repair rather than a blanket rollback. Defaults cannot
recreate lost balances or purchase history without a recoverable source.

Stop the software before editing or resetting. Deleting user.conf while user.bak
remains will recover the backup. See the [complete inventory and boundaries](../../readme_fakeServer.md#files-written-and-protection-policies).

## Complete console-locale reference

These are 3DS CFG IDs, not Pokemon language IDs. Subdivisions are listed under their country. Country 0 and state 0 mean unset. Historical system-era labels are retained; administrative changes do not renumber the records.

### Console region

| Decimal | Code | Meaning |
| --- | --- | --- |
| 0 | JPN | Japan |
| 1 | USA | Americas |
| 2 | EUR | Europe |
| 3 | AUS | Australia region ID |
| 4 | CHN | China |
| 5 | KOR | Korea |
| 6 | TWN | Taiwan / Hong Kong |

### System language

| Decimal | Code | Meaning |
| --- | --- | --- |
| 0 | JP | Japanese |
| 1 | EN | English |
| 2 | FR | French |
| 3 | DE | German |
| 4 | IT | Italian |
| 5 | ES | Spanish |
| 6 | ZH | Simplified Chinese |
| 7 | KO | Korean |
| 8 | NL | Dutch |
| 9 | PT | Portuguese |
| 10 | RU | Russian |
| 11 | TW | Traditional Chinese |

### Console-region, system-language and country combinations

Languages are conventional system-language choices (CFG IDs). Countries follow PKHeX's 3DS Pokemon region/country restrictions, not an exhaustive hardware Settings-menu test. These are not Pokemon language IDs. Sources and known differences follow the table.

| region | Conventional system languages | country set |
| --- | --- | --- |
| 0 JPN | 0 Japanese | 1 |
| 1 USA | 1 English, 2 French, 5 Spanish, 9 Portuguese | 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 153, 156, 168, 174, 186 |
| 2 EUR | 1 English, 2 French, 3 German, 4 Italian, 5 Spanish, 8 Dutch, 9 Portuguese, 10 Russian | 64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93, 94, 95, 96, 97, 98, 99, 100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111, 112, 113, 114, 115, 116, 117, 118, 119, 120, 121, 122, 123, 124, 125, 126, 127, 169, 184, 185 |
| 3 AUS | Reserved; Australian consoles normally use EUR=2; not recommended for Pokemon | No separate valid set |
| 4 CHN | 6 Simplified Chinese | 144, 160 |
| 5 KOR | 7 Korean | 136 |
| 6 TWN | 11 Traditional Chinese | 128, 144 |

Country names are expanded below. Look up state IDs under that same country; subdivision IDs are not interchangeable between countries.

<details>
<summary>region=0 JPN</summary>

- country=1 / JP — Japan

</details>

<details>
<summary>region=1 USA</summary>

- country=8 / AI — Anguilla
- country=9 / AG — Antigua and Barbuda
- country=10 / AR — Argentina
- country=11 / AW — Aruba
- country=12 / BS — Bahamas
- country=13 / BB — Barbados
- country=14 / BZ — Belize
- country=15 / BO — Bolivia
- country=16 / BR — Brazil
- country=17 / VG — British Virgin Islands
- country=18 / CA — Canada
- country=19 / KY — Cayman Islands
- country=20 / CL — Chile
- country=21 / CO — Colombia
- country=22 / CR — Costa Rica
- country=23 / DM — Dominica
- country=24 / DO — Dominican Republic
- country=25 / EC — Ecuador
- country=26 / SV — El Salvador
- country=27 / GF — French Guiana
- country=28 / GD — Grenada
- country=29 / GP — Guadeloupe
- country=30 / GT — Guatemala
- country=31 / GY — Guyana
- country=32 / HT — Haiti
- country=33 / HN — Honduras
- country=34 / JM — Jamaica
- country=35 / MQ — Martinique
- country=36 / MX — Mexico
- country=37 / MS — Montserrat
- country=38 / AN — Netherlands Antilles
- country=39 / NI — Nicaragua
- country=40 / PA — Panama
- country=41 / PY — Paraguay
- country=42 / PE — Peru
- country=43 / KN — St. Kitts and Nevis
- country=44 / LC — St. Lucia
- country=45 / VC — St. Vincent and the Grenadines
- country=46 / SR — Suriname
- country=47 / TT — Trinidad and Tobago
- country=48 / TC — Turks and Caicos Islands
- country=49 / US — United States
- country=50 / UY — Uruguay
- country=51 / VI — US Virgin Islands
- country=52 / VE — Venezuela
- country=153 / SG — Singapore
- country=156 / MY — Malaysia
- country=168 / AE — U.A.E.
- country=174 / SA — Saudi Arabia
- country=186 / BM — Bermuda

</details>

<details>
<summary>region=2 EUR</summary>

- country=64 / AL — Albania
- country=65 / AU — Australia
- country=66 / AT — Austria
- country=67 / BE — Belgium
- country=68 / BA — Bosnia and Herzegovina
- country=69 / BW — Botswana
- country=70 / BG — Bulgaria
- country=71 / HR — Croatia
- country=72 / CY — Cyprus
- country=73 / CZ — Czech Republic
- country=74 / DK — Denmark (Kingdom of)
- country=75 / EE — Estonia
- country=76 / FI — Finland
- country=77 / FR — France
- country=78 / DE — Germany
- country=79 / GR — Greece
- country=80 / HU — Hungary
- country=81 / IS — Iceland
- country=82 / IE — Ireland
- country=83 / IT — Italy
- country=84 / LV — Latvia
- country=85 / LS — Lesotho
- country=86 / LI — Liechtenstein
- country=87 / LT — Lithuania
- country=88 / LU — Luxembourg
- country=89 / MK — Macedonia (Republic of)
- country=90 / MT — Malta
- country=91 / ME — Montenegro
- country=92 / MZ — Mozambique
- country=93 / NA — Namibia
- country=94 / NL — Netherlands
- country=95 / NZ — New Zealand
- country=96 / NO — Norway
- country=97 / PL — Poland
- country=98 / PT — Portugal
- country=99 / RO — Romania
- country=100 / RU — Russia
- country=101 / RS — Serbia and Kosovo
- country=102 / SK — Slovakia
- country=103 / SI — Slovenia
- country=104 / ZA — South Africa
- country=105 / ES — Spain
- country=106 / SZ — Swaziland
- country=107 / SE — Sweden
- country=108 / CH — Switzerland
- country=109 / TR — Turkey
- country=110 / GB — United Kingdom
- country=111 / ZM — Zambia
- country=112 / ZW — Zimbabwe
- country=113 / AZ — Azerbaijan
- country=114 / MR — Mauritania
- country=115 / ML — Mali
- country=116 / NE — Niger
- country=117 / TD — Chad
- country=118 / SD — Sudan
- country=119 / ER — Eritrea
- country=120 / DJ — Djibouti
- country=121 / SO — Somalia
- country=122 / AD — Andorra
- country=123 / GI — Gibraltar
- country=124 / GG — Guernsey
- country=125 / IM — Isle of Man
- country=126 / JE — Jersey
- country=127 / MC — Monaco
- country=169 / IN — India
- country=184 / SM — San Marino
- country=185 / VA — Vatican City

</details>

<details>
<summary>region=4 CHN</summary>

- country=144 / HK — Hong Kong
- country=160 / CN — China

</details>

<details>
<summary>region=5 KOR</summary>

- country=136 / KR — South Korea

</details>

<details>
<summary>region=6 TWN</summary>

- country=128 / TW — Taiwan
- country=144 / HK — Hong Kong

</details>

Sources: [Azahar CFG language/country implementation](https://raw.githubusercontent.com/azahar-emu/azahar/987f18405073960622e1043bb44005989135c6e1/src/core/hle/service/cfg/cfg.cpp); US system languages are also confirmed by [Nintendo's 3DS language instructions](https://en-americas-support.nintendo.com/app/answers/detail/a_id/233/p/605/c/188). Country combinations use [PKHeX Locale3DS](https://raw.githubusercontent.com/kwsch/PKHeX/641c26ba100225f3605e5a44b61b3b16e15a3774/PKHeX.Core/Legality/Restrictions/Locale/Locale3DS.cs), a community Pokemon-data validator, not an official Nintendo promise for every country. Nintendo's [country/region instructions](https://pt-americas-support.nintendo.com/app/answers/detail/a_id/15653/~/como-mudar-a-configura%C3%A7%C3%A3o-de-pa%C3%ADs%2Fregi%C3%A3o-do-console) restrict stock selection to the purchase region and require the linked NNID country to match.

Known differences: Azahar assigns Bermuda (186) to EUR/AUS while PKHeX assigns it to USA. PKHeX accepts Hong Kong (144) under CHN/TWN; Azahar lists only CHN. Macao (145) has a CFG ID but no accepted PKHeX combination. Emulator selectability alone does not establish a valid Pokemon transfer combination. AUS=3 is reserved and is not a valid Pokemon console region in PKHeX.

System language 11 for TWN does not mean a VC Pokemon language. For example, PKHeX's VC language check allows Japanese sources for JPN/TWN; English/French/Spanish for USA; English/French/German/Italian/Spanish for EUR; and Korean for KOR. CHN has no supported VC-language set there. This explains distinct identifiers; it does not overwrite a Pokemon's source language.

These are reference combinations, not hard patch restrictions. Luma can independently override region/language/country/state; manually forced values remain intact. For conventional combinations, retain the first captured console values and choose state only under that same country.

### Country → subdivision

Lists 133 nonzero countries/territories and 1,524 published subdivision/unset entries. Expand a country for its state values. Keep the 0x prefix when entering hexadecimal.

<details>
<summary>country=1 (0x01, JP) — Japan</summary>

| state decimal | Hexadecimal | Subdivision |
| --- | --- | --- |
| 0 | 0x00 | Unset |
| 2 | 0x02 | Tokyo |
| 3 | 0x03 | Hokkaido |
| 4 | 0x04 | Aomori |
| 5 | 0x05 | Iwate |
| 6 | 0x06 | Miyagi |
| 7 | 0x07 | Akita |
| 8 | 0x08 | Yamagata |
| 9 | 0x09 | Fukushima |
| 10 | 0x0A | Ibaraki |
| 11 | 0x0B | Tochigi |
| 12 | 0x0C | Gunma |
| 13 | 0x0D | Saitama |
| 14 | 0x0E | Chiba |
| 15 | 0x0F | Kanagawa |
| 16 | 0x10 | Toyama |
| 17 | 0x11 | Ishikawa |
| 18 | 0x12 | Fukui |
| 19 | 0x13 | Yamanashi |
| 20 | 0x14 | Nagano |
| 21 | 0x15 | Niigata |
| 22 | 0x16 | Gifu |
| 23 | 0x17 | Shizuoka |
| 24 | 0x18 | Aichi |
| 25 | 0x19 | Mie |
| 26 | 0x1A | Shiga |
| 27 | 0x1B | Kyoto |
| 28 | 0x1C | Osaka |
| 29 | 0x1D | Hyogo |
| 30 | 0x1E | Nara |
| 31 | 0x1F | Wakayama |
| 32 | 0x20 | Tottori |
| 33 | 0x21 | Shimane |
| 34 | 0x22 | Okayama |
| 35 | 0x23 | Hiroshima |
| 36 | 0x24 | Yamaguchi |
| 37 | 0x25 | Tokushima |
| 38 | 0x26 | Kagawa |
| 39 | 0x27 | Ehime |
| 40 | 0x28 | Kochi |
| 41 | 0x29 | Fukuoka |
| 42 | 0x2A | Saga |
| 43 | 0x2B | Nagasaki |
| 44 | 0x2C | Kumamoto |
| 45 | 0x2D | Oita |
| 46 | 0x2E | Miyazaki |
| 47 | 0x2F | Kagoshima |
| 48 | 0x30 | Okinawa |

</details>

<details>
<summary>country=8 (0x08, AI) — Anguilla</summary>

| state decimal | Hexadecimal | Subdivision |
| --- | --- | --- |
| 0 | 0x00 | Unset |
| 1 | 0x01 | Anguilla |

</details>

<details>
<summary>country=9 (0x09, AG) — Antigua and Barbuda</summary>

| state decimal | Hexadecimal | Subdivision |
| --- | --- | --- |
| 0 | 0x00 | Unset |
| 2 | 0x02 | Saint John |
| 3 | 0x03 | Barbuda |
| 4 | 0x04 | Saint George |
| 5 | 0x05 | Saint Mary |
| 6 | 0x06 | Saint Paul |
| 7 | 0x07 | Saint Peter |
| 8 | 0x08 | Saint Philip |

</details>

<details>
<summary>country=10 (0x0A, AR) — Argentina</summary>

| state decimal | Hexadecimal | Subdivision |
| --- | --- | --- |
| 0 | 0x00 | Unset |
| 2 | 0x02 | Distrito Federal |
| 3 | 0x03 | Buenos Aires |
| 4 | 0x04 | Catamarca |
| 5 | 0x05 | Chaco |
| 6 | 0x06 | Chubut |
| 7 | 0x07 | Córdoba |
| 8 | 0x08 | Corrientes |
| 9 | 0x09 | Entre Ríos |
| 10 | 0x0A | Formosa |
| 11 | 0x0B | Jujuy |
| 12 | 0x0C | La Pampa |
| 13 | 0x0D | La Rioja |
| 14 | 0x0E | Mendoza |
| 15 | 0x0F | Misiones |
| 16 | 0x10 | Neuquén |
| 17 | 0x11 | Río Negro |
| 18 | 0x12 | Salta |
| 19 | 0x13 | San Juan |
| 20 | 0x14 | San Luis |
| 21 | 0x15 | Santa Cruz |
| 22 | 0x16 | Santa Fe |
| 23 | 0x17 | Santiago del Estero |
| 24 | 0x18 | Tierra del Fuego, Antártida e Islas del Atlántico Sur |
| 25 | 0x19 | Tucumán |

</details>

<details>
<summary>country=11 (0x0B, AW) — Aruba</summary>

| state decimal | Hexadecimal | Subdivision |
| --- | --- | --- |
| 0 | 0x00 | Unset |
| 1 | 0x01 | Aruba |

</details>

<details>
<summary>country=12 (0x0C, BS) — Bahamas</summary>

| state decimal | Hexadecimal | Subdivision |
| --- | --- | --- |
| 0 | 0x00 | Unset |
| 1 | 0x01 | Bahamas |

</details>

<details>
<summary>country=13 (0x0D, BB) — Barbados</summary>

| state decimal | Hexadecimal | Subdivision |
| --- | --- | --- |
| 0 | 0x00 | Unset |
| 1 | 0x01 | Barbados |

</details>

<details>
<summary>country=14 (0x0E, BZ) — Belize</summary>

| state decimal | Hexadecimal | Subdivision |
| --- | --- | --- |
| 0 | 0x00 | Unset |
| 2 | 0x02 | Cayo |
| 3 | 0x03 | Belize |
| 4 | 0x04 | Corozal |
| 5 | 0x05 | Orange Walk |
| 6 | 0x06 | Stann Creek |
| 7 | 0x07 | Toledo |

</details>

<details>
<summary>country=15 (0x0F, BO) — Bolivia</summary>

| state decimal | Hexadecimal | Subdivision |
| --- | --- | --- |
| 0 | 0x00 | Unset |
| 2 | 0x02 | La Paz |
| 3 | 0x03 | Chuquisaca |
| 4 | 0x04 | Cochabamba |
| 5 | 0x05 | El Beni |
| 6 | 0x06 | Oruro |
| 7 | 0x07 | Pando |
| 8 | 0x08 | Potosí |
| 9 | 0x09 | Santa Cruz |
| 10 | 0x0A | Tarija |

</details>

<details>
<summary>country=16 (0x10, BR) — Brazil</summary>

| state decimal | Hexadecimal | Subdivision |
| --- | --- | --- |
| 0 | 0x00 | Unset |
| 2 | 0x02 | Distrito Federal |
| 3 | 0x03 | Acre |
| 4 | 0x04 | Alagoas |
| 5 | 0x05 | Amapá |
| 6 | 0x06 | Amazonas |
| 7 | 0x07 | Bahia |
| 8 | 0x08 | Ceará |
| 9 | 0x09 | Espírito Santo |
| 10 | 0x0A | Mato Grosso do Sul |
| 11 | 0x0B | Maranhão |
| 12 | 0x0C | Mato Grosso |
| 13 | 0x0D | Minas Gerais |
| 14 | 0x0E | Pará |
| 15 | 0x0F | Paraíba |
| 16 | 0x10 | Paraná |
| 17 | 0x11 | Piauí |
| 18 | 0x12 | Rio de Janeiro |
| 19 | 0x13 | Rio Grande do Norte |
| 20 | 0x14 | Rio Grande do Sul |
| 21 | 0x15 | Rondônia |
| 22 | 0x16 | Roraima |
| 23 | 0x17 | Santa Catarina |
| 24 | 0x18 | São Paulo |
| 25 | 0x19 | Sergipe |
| 26 | 0x1A | Goiás |
| 27 | 0x1B | Pernambuco |
| 28 | 0x1C | Tocantins |

</details>

<details>
<summary>country=17 (0x11, VG) — British Virgin Islands</summary>

| state decimal | Hexadecimal | Subdivision |
| --- | --- | --- |
| 0 | 0x00 | Unset |
| 1 | 0x01 | British Virgin Islands |

</details>

<details>
<summary>country=18 (0x12, CA) — Canada</summary>

| state decimal | Hexadecimal | Subdivision |
| --- | --- | --- |
| 0 | 0x00 | Unset |
| 2 | 0x02 | Ontario |
| 3 | 0x03 | Alberta |
| 4 | 0x04 | British Columbia |
| 5 | 0x05 | Manitoba |
| 6 | 0x06 | New Brunswick |
| 7 | 0x07 | Newfoundland and Labrador |
| 8 | 0x08 | Nova Scotia |
| 9 | 0x09 | Prince Edward Island |
| 10 | 0x0A | Quebec |
| 11 | 0x0B | Saskatchewan |
| 12 | 0x0C | Yukon |
| 13 | 0x0D | Northwest Territories |
| 14 | 0x0E | Nunavut |

</details>

<details>
<summary>country=19 (0x13, KY) — Cayman Islands</summary>

| state decimal | Hexadecimal | Subdivision |
| --- | --- | --- |
| 0 | 0x00 | Unset |
| 1 | 0x01 | Cayman Islands |

</details>

<details>
<summary>country=20 (0x14, CL) — Chile</summary>

| state decimal | Hexadecimal | Subdivision |
| --- | --- | --- |
| 0 | 0x00 | Unset |
| 2 | 0x02 | Región Metropolitana |
| 3 | 0x03 | Valparaíso |
| 4 | 0x04 | Aisén del General Carlos Ibáñez del Campo |
| 5 | 0x05 | Antofagasta |
| 6 | 0x06 | Araucanía |
| 7 | 0x07 | Atacama |
| 8 | 0x08 | Bío-Bío |
| 9 | 0x09 | Coquimbo |
| 10 | 0x0A | Libertador General Bernardo O'Higgins |
| 11 | 0x0B | Los Lagos |
| 12 | 0x0C | Magallanes y Antártica Chilena |
| 13 | 0x0D | Maule |
| 14 | 0x0E | Tarapacá |

</details>

<details>
<summary>country=21 (0x15, CO) — Colombia</summary>

| state decimal | Hexadecimal | Subdivision |
| --- | --- | --- |
| 0 | 0x00 | Unset |
| 2 | 0x02 | Distrito Capital |
| 3 | 0x03 | Cundinamarca |
| 4 | 0x04 | Amazonas |
| 5 | 0x05 | Antioquia |
| 6 | 0x06 | Arauca |
| 7 | 0x07 | Atlántico |
| 8 | 0x08 | Bolívar |
| 9 | 0x09 | Boyacá |
| 10 | 0x0A | Caldas |
| 11 | 0x0B | Caquetá |
| 12 | 0x0C | Cauca |
| 13 | 0x0D | Cesar |
| 14 | 0x0E | Chocó |
| 15 | 0x0F | Córdoba |
| 16 | 0x10 | Guaviare |
| 17 | 0x11 | Guainía |
| 18 | 0x12 | Huila |
| 19 | 0x13 | La Guajira |
| 20 | 0x14 | Magdalena |
| 21 | 0x15 | Meta |
| 22 | 0x16 | Nariño |
| 23 | 0x17 | Norte de Santander |
| 24 | 0x18 | Putumayo |
| 25 | 0x19 | Quindío |
| 26 | 0x1A | Risaralda |
| 27 | 0x1B | Archipiélago de San Andrés, Providencia y Santa Catalina |
| 28 | 0x1C | Santander |
| 29 | 0x1D | Sucre |
| 30 | 0x1E | Tolima |
| 31 | 0x1F | Valle del Cauca |
| 32 | 0x20 | Vaupés |
| 33 | 0x21 | Vichada |
| 34 | 0x22 | Casanare |

</details>

<details>
<summary>country=22 (0x16, CR) — Costa Rica</summary>

| state decimal | Hexadecimal | Subdivision |
| --- | --- | --- |
| 0 | 0x00 | Unset |
| 2 | 0x02 | San José |
| 3 | 0x03 | Alajuela |
| 4 | 0x04 | Cartago |
| 5 | 0x05 | Guanacaste |
| 6 | 0x06 | Heredia |
| 7 | 0x07 | Limón |
| 8 | 0x08 | Puntarenas |

</details>

<details>
<summary>country=23 (0x17, DM) — Dominica</summary>

| state decimal | Hexadecimal | Subdivision |
| --- | --- | --- |
| 0 | 0x00 | Unset |
| 1 | 0x01 | Dominica |

</details>

<details>
<summary>country=24 (0x18, DO) — Dominican Republic</summary>

| state decimal | Hexadecimal | Subdivision |
| --- | --- | --- |
| 0 | 0x00 | Unset |
| 2 | 0x02 | Distrito Nacional |
| 3 | 0x03 | Azua |
| 4 | 0x04 | Baoruco |
| 5 | 0x05 | Barahona |
| 6 | 0x06 | Dajabón |
| 7 | 0x07 | Duarte |
| 8 | 0x08 | Espaillat |
| 9 | 0x09 | Independencia |
| 10 | 0x0A | La Altagracia |
| 11 | 0x0B | Elías Piña |
| 12 | 0x0C | La Romana |
| 13 | 0x0D | María Trinidad Sánchez |
| 14 | 0x0E | Monte Cristi |
| 15 | 0x0F | Pedernales |
| 16 | 0x10 | Peravia |
| 17 | 0x11 | Puerto Plata |
| 18 | 0x12 | Salcedo |
| 19 | 0x13 | Samaná |
| 20 | 0x14 | Sánchez Ramírez |
| 21 | 0x15 | San Juan |
| 22 | 0x16 | San Pedro de Macorís |
| 23 | 0x17 | Santiago |
| 24 | 0x18 | Santiago Rodríguez |
| 25 | 0x19 | Valverde |
| 26 | 0x1A | El Seíbo |
| 27 | 0x1B | Hato Mayor |
| 28 | 0x1C | La Vega |
| 29 | 0x1D | Monseñor Nouel |
| 30 | 0x1E | Monte Plata |
| 31 | 0x1F | San Cristóbal |

</details>

<details>
<summary>country=25 (0x19, EC) — Ecuador</summary>

| state decimal | Hexadecimal | Subdivision |
| --- | --- | --- |
| 0 | 0x00 | Unset |
| 2 | 0x02 | Pichincha |
| 3 | 0x03 | Galápagos |
| 4 | 0x04 | Azuay |
| 5 | 0x05 | Bolívar |
| 6 | 0x06 | Cañar |
| 7 | 0x07 | Carchi |
| 8 | 0x08 | Chimborazo |
| 9 | 0x09 | Cotopaxi |
| 10 | 0x0A | El Oro |
| 11 | 0x0B | Esmeraldas |
| 12 | 0x0C | Guayas |
| 13 | 0x0D | Imbabura |
| 14 | 0x0E | Loja |
| 15 | 0x0F | Los Ríos |
| 16 | 0x10 | Manabí |
| 17 | 0x11 | Morona-Santiago |
| 18 | 0x12 | Pastaza |
| 19 | 0x13 | Tungurahua |
| 20 | 0x14 | Zamora-Chinchipe |
| 21 | 0x15 | Sucumbios |
| 22 | 0x16 | Napo |
| 23 | 0x17 | Orellana |
| 24 | 0x18 | Santa Elena |
| 25 | 0x19 | Santo Domingo de los Tsáchilas |

</details>

<details>
<summary>country=26 (0x1A, SV) — El Salvador</summary>

| state decimal | Hexadecimal | Subdivision |
| --- | --- | --- |
| 0 | 0x00 | Unset |
| 2 | 0x02 | San Salvador |
| 3 | 0x03 | Ahuachapán |
| 4 | 0x04 | Cabañas |
| 5 | 0x05 | Chalatenango |
| 6 | 0x06 | Cuscatlán |
| 7 | 0x07 | La Libertad |
| 8 | 0x08 | La Paz |
| 9 | 0x09 | La Unión |
| 10 | 0x0A | Morazán |
| 11 | 0x0B | San Miguel |
| 12 | 0x0C | Santa Ana |
| 13 | 0x0D | San Vicente |
| 14 | 0x0E | Sonsonate |
| 15 | 0x0F | Usulután |

</details>

<details>
<summary>country=27 (0x1B, GF) — French Guiana</summary>

| state decimal | Hexadecimal | Subdivision |
| --- | --- | --- |
| 0 | 0x00 | Unset |
| 1 | 0x01 | French Guiana |

</details>

<details>
<summary>country=28 (0x1C, GD) — Grenada</summary>

| state decimal | Hexadecimal | Subdivision |
| --- | --- | --- |
| 0 | 0x00 | Unset |
| 1 | 0x01 | Grenada |

</details>

<details>
<summary>country=29 (0x1D, GP) — Guadeloupe</summary>

| state decimal | Hexadecimal | Subdivision |
| --- | --- | --- |
| 0 | 0x00 | Unset |
| 1 | 0x01 | Guadeloupe |

</details>

<details>
<summary>country=30 (0x1E, GT) — Guatemala</summary>

| state decimal | Hexadecimal | Subdivision |
| --- | --- | --- |
| 0 | 0x00 | Unset |
| 2 | 0x02 | Guatemala |
| 3 | 0x03 | Alta Verapaz |
| 4 | 0x04 | Baja Verapaz |
| 5 | 0x05 | Chimaltenango |
| 6 | 0x06 | Chiquimula |
| 7 | 0x07 | El Progreso |
| 8 | 0x08 | Escuintla |
| 9 | 0x09 | Huehuetenango |
| 10 | 0x0A | Izabal |
| 11 | 0x0B | Jalapa |
| 12 | 0x0C | Jutiapa |
| 13 | 0x0D | Petén |
| 14 | 0x0E | Quetzaltenango |
| 15 | 0x0F | Quiché |
| 16 | 0x10 | Retalhuleu |
| 17 | 0x11 | Sacatepéquez |
| 18 | 0x12 | San Marcos |
| 19 | 0x13 | Santa Rosa |
| 20 | 0x14 | Sololá |
| 21 | 0x15 | Suchitepéquez |
| 22 | 0x16 | Totonicapán |
| 23 | 0x17 | Zacapa |

</details>

<details>
<summary>country=31 (0x1F, GY) — Guyana</summary>

| state decimal | Hexadecimal | Subdivision |
| --- | --- | --- |
| 0 | 0x00 | Unset |
| 2 | 0x02 | Demerara-Mahaica |
| 3 | 0x03 | Barima-Waini |
| 4 | 0x04 | Cuyuni-Mazaruni |
| 5 | 0x05 | East Berbice-Corentyne |
| 6 | 0x06 | Essequibo Islands-West Demerara |
| 7 | 0x07 | Mahaica-Berbice |
| 8 | 0x08 | Pomeroon-Supenaam |
| 9 | 0x09 | Potaro-Siparuni |
| 10 | 0x0A | Upper Demerara-Berbice |
| 11 | 0x0B | Upper Takutu-Upper Essequibo |

</details>

<details>
<summary>country=32 (0x20, HT) — Haiti</summary>

| state decimal | Hexadecimal | Subdivision |
| --- | --- | --- |
| 0 | 0x00 | Unset |
| 2 | 0x02 | Ouest |
| 3 | 0x03 | Nord-Ouest |
| 4 | 0x04 | Artibonite |
| 5 | 0x05 | Centre |
| 6 | 0x06 | Grand'Anse |
| 7 | 0x07 | Nord |
| 8 | 0x08 | Nord-Est |
| 9 | 0x09 | Sud |
| 10 | 0x0A | Sud-Est |
| 11 | 0x0B | Nippes |

</details>

<details>
<summary>country=33 (0x21, HN) — Honduras</summary>

| state decimal | Hexadecimal | Subdivision |
| --- | --- | --- |
| 0 | 0x00 | Unset |
| 2 | 0x02 | Francisco Morazán |
| 3 | 0x03 | Atlántida |
| 4 | 0x04 | Choluteca |
| 5 | 0x05 | Colón |
| 6 | 0x06 | Comayagua |
| 7 | 0x07 | Copán |
| 8 | 0x08 | Cortés |
| 9 | 0x09 | El Paraíso |
| 10 | 0x0A | Gracias a Dios |
| 11 | 0x0B | Intibucá |
| 12 | 0x0C | Islas de la Bahía |
| 13 | 0x0D | La Paz |
| 14 | 0x0E | Lempira |
| 15 | 0x0F | Ocotepeque |
| 16 | 0x10 | Olancho |
| 17 | 0x11 | Santa Bárbara |
| 18 | 0x12 | Valle |
| 19 | 0x13 | Yoro |

</details>

<details>
<summary>country=34 (0x22, JM) — Jamaica</summary>

| state decimal | Hexadecimal | Subdivision |
| --- | --- | --- |
| 0 | 0x00 | Unset |
| 2 | 0x02 | Saint Thomas |
| 3 | 0x03 | Clarendon |
| 4 | 0x04 | Hanover |
| 5 | 0x05 | Manchester |
| 6 | 0x06 | Portland |
| 7 | 0x07 | Saint Andrew |
| 8 | 0x08 | Saint Ann |
| 9 | 0x09 | Saint Catherine |
| 10 | 0x0A | Saint Elizabeth |
| 11 | 0x0B | Saint James |
| 12 | 0x0C | Saint Mary |
| 13 | 0x0D | Trelawny |
| 14 | 0x0E | Westmoreland |
| 15 | 0x0F | Kingston |

</details>

<details>
<summary>country=35 (0x23, MQ) — Martinique</summary>

| state decimal | Hexadecimal | Subdivision |
| --- | --- | --- |
| 0 | 0x00 | Unset |
| 1 | 0x01 | Martinique |

</details>

<details>
<summary>country=36 (0x24, MX) — Mexico</summary>

| state decimal | Hexadecimal | Subdivision |
| --- | --- | --- |
| 0 | 0x00 | Unset |
| 2 | 0x02 | Distrito Federal |
| 3 | 0x03 | Aguascalientes |
| 4 | 0x04 | Baja California |
| 5 | 0x05 | Baja California Sur |
| 6 | 0x06 | Campeche |
| 7 | 0x07 | Chiapas |
| 8 | 0x08 | Chihuahua |
| 9 | 0x09 | Coahuila de Zaragoza |
| 10 | 0x0A | Colima |
| 11 | 0x0B | Durango |
| 12 | 0x0C | Guanajuato |
| 13 | 0x0D | Guerrero |
| 14 | 0x0E | Hidalgo |
| 15 | 0x0F | Jalisco |
| 16 | 0x10 | México |
| 17 | 0x11 | Michoacán de Ocampo |
| 18 | 0x12 | Morelos |
| 19 | 0x13 | Nayarit |
| 20 | 0x14 | Nuevo León |
| 21 | 0x15 | Oaxaca |
| 22 | 0x16 | Puebla |
| 23 | 0x17 | Querétaro de Arteaga |
| 24 | 0x18 | Quintana Roo |
| 25 | 0x19 | San Luis Potosí |
| 26 | 0x1A | Sinaloa |
| 27 | 0x1B | Sonora |
| 28 | 0x1C | Tabasco |
| 29 | 0x1D | Tamaulipas |
| 30 | 0x1E | Tlaxcala |
| 31 | 0x1F | Veracruz-Llave |
| 32 | 0x20 | Yucatán |
| 33 | 0x21 | Zacatecas |

</details>

<details>
<summary>country=37 (0x25, MS) — Montserrat</summary>

| state decimal | Hexadecimal | Subdivision |
| --- | --- | --- |
| 0 | 0x00 | Unset |
| 1 | 0x01 | Montserrat |

</details>

<details>
<summary>country=38 (0x26, AN) — Netherlands Antilles</summary>

| state decimal | Hexadecimal | Subdivision |
| --- | --- | --- |
| 0 | 0x00 | Unset |
| 1 | 0x01 | Netherlands Antilles |

</details>

<details>
<summary>country=39 (0x27, NI) — Nicaragua</summary>

| state decimal | Hexadecimal | Subdivision |
| --- | --- | --- |
| 0 | 0x00 | Unset |
| 2 | 0x02 | Managua |
| 3 | 0x03 | Boaco |
| 4 | 0x04 | Carazo |
| 5 | 0x05 | Chinandega |
| 6 | 0x06 | Chontales |
| 7 | 0x07 | Estelí |
| 8 | 0x08 | Granada |
| 9 | 0x09 | Jinotega |
| 10 | 0x0A | León |
| 11 | 0x0B | Madriz |
| 12 | 0x0C | Masaya |
| 13 | 0x0D | Matagalpa |
| 14 | 0x0E | Nueva Segovia |
| 15 | 0x0F | Río San Juan |
| 16 | 0x10 | Rivas |
| 17 | 0x11 | Atlántico Norte |
| 18 | 0x12 | Atlántico Sur |

</details>

<details>
<summary>country=40 (0x28, PA) — Panama</summary>

| state decimal | Hexadecimal | Subdivision |
| --- | --- | --- |
| 0 | 0x00 | Unset |
| 2 | 0x02 | Panamá |
| 3 | 0x03 | Bocas del Toro |
| 4 | 0x04 | Chiriquí |
| 5 | 0x05 | Coclé |
| 6 | 0x06 | Colón |
| 7 | 0x07 | Darién |
| 8 | 0x08 | Herrera |
| 9 | 0x09 | Los Santos |
| 10 | 0x0A | Kuna Yala |
| 11 | 0x0B | Veraguas |

</details>

<details>
<summary>country=41 (0x29, PY) — Paraguay</summary>

| state decimal | Hexadecimal | Subdivision |
| --- | --- | --- |
| 0 | 0x00 | Unset |
| 2 | 0x02 | Central |
| 3 | 0x03 | Alto Paraná |
| 4 | 0x04 | Amambay |
| 5 | 0x05 | Caaguazú |
| 6 | 0x06 | Caazapá |
| 7 | 0x07 | Concepción |
| 8 | 0x08 | Cordillera |
| 9 | 0x09 | Guairá |
| 10 | 0x0A | Itapúa |
| 11 | 0x0B | Misiones |
| 12 | 0x0C | Ñeembucú |
| 13 | 0x0D | Paraguarí |
| 14 | 0x0E | Presidente Hayes |
| 15 | 0x0F | San Pedro |
| 16 | 0x10 | Canindeyú |
| 17 | 0x11 | Asunción |
| 18 | 0x12 | Alto Paraguay |
| 19 | 0x13 | Boquerón |

</details>

<details>
<summary>country=42 (0x2A, PE) — Peru</summary>

| state decimal | Hexadecimal | Subdivision |
| --- | --- | --- |
| 0 | 0x00 | Unset |
| 2 | 0x02 | Lima |
| 3 | 0x03 | Amazonas |
| 4 | 0x04 | Ancash |
| 5 | 0x05 | Apurímac |
| 6 | 0x06 | Arequipa |
| 7 | 0x07 | Ayacucho |
| 8 | 0x08 | Cajamarca |
| 9 | 0x09 | Callao |
| 10 | 0x0A | Cuzco |
| 11 | 0x0B | Huancavelica |
| 12 | 0x0C | Huánuco |
| 13 | 0x0D | Ica |
| 14 | 0x0E | Junín |
| 15 | 0x0F | La Libertad |
| 16 | 0x10 | Lambayeque |
| 17 | 0x11 | Loreto |
| 18 | 0x12 | Madre de Dios |
| 19 | 0x13 | Moquegua |
| 20 | 0x14 | Pasco |
| 21 | 0x15 | Piura |
| 22 | 0x16 | Puno |
| 23 | 0x17 | San Martín |
| 24 | 0x18 | Tacna |
| 25 | 0x19 | Tumbes |
| 26 | 0x1A | Ucayali |

</details>

<details>
<summary>country=43 (0x2B, KN) — St. Kitts and Nevis</summary>

| state decimal | Hexadecimal | Subdivision |
| --- | --- | --- |
| 0 | 0x00 | Unset |
| 2 | 0x02 | Saint George Basseterre |
| 3 | 0x03 | Christ Church Nichola Town |
| 4 | 0x04 | Saint Anne Sandy Point |
| 5 | 0x05 | Saint George Gingerland |
| 6 | 0x06 | Saint James Windward |
| 7 | 0x07 | Saint John Capesterre |
| 8 | 0x08 | Saint John Figtree |
| 9 | 0x09 | Saint Mary Cayon |
| 10 | 0x0A | Saint Paul Capesterre |
| 11 | 0x0B | Saint Paul Charlestown |
| 12 | 0x0C | Saint Peter Basseterre |
| 13 | 0x0D | Saint Thomas Lowland |
| 14 | 0x0E | Saint Thomas Middle Island |
| 15 | 0x0F | Trinity Palmetto Point |

</details>

<details>
<summary>country=44 (0x2C, LC) — St. Lucia</summary>

| state decimal | Hexadecimal | Subdivision |
| --- | --- | --- |
| 0 | 0x00 | Unset |
| 1 | 0x01 | St. Lucia |

</details>

<details>
<summary>country=45 (0x2D, VC) — St. Vincent and the Grenadines</summary>

| state decimal | Hexadecimal | Subdivision |
| --- | --- | --- |
| 0 | 0x00 | Unset |
| 1 | 0x01 | St. Vincent and the Grenadines |

</details>

<details>
<summary>country=46 (0x2E, SR) — Suriname</summary>

| state decimal | Hexadecimal | Subdivision |
| --- | --- | --- |
| 0 | 0x00 | Unset |
| 2 | 0x02 | Paramaribo |
| 3 | 0x03 | Brokopondo |
| 4 | 0x04 | Commewijne |
| 5 | 0x05 | Coronie |
| 6 | 0x06 | Marowijne |
| 7 | 0x07 | Nickerie |
| 8 | 0x08 | Para |
| 9 | 0x09 | Saramacca |
| 10 | 0x0A | Sipaliwini |
| 11 | 0x0B | Wanica |

</details>

<details>
<summary>country=47 (0x2F, TT) — Trinidad and Tobago</summary>

| state decimal | Hexadecimal | Subdivision |
| --- | --- | --- |
| 0 | 0x00 | Unset |
| 2 | 0x02 | Port-of-Spain |
| 3 | 0x03 | Arima |
| 4 | 0x04 | Caroni |
| 5 | 0x05 | Mayaro |
| 6 | 0x06 | Nariva |
| 7 | 0x07 | Saint Andrew |
| 8 | 0x08 | Saint David |
| 9 | 0x09 | Saint George |
| 10 | 0x0A | Saint Patrick |
| 11 | 0x0B | San Fernando |
| 12 | 0x0C | Tobago |
| 13 | 0x0D | Victoria |
| 14 | 0x0E | Point Fortin |

</details>

<details>
<summary>country=48 (0x30, TC) — Turks and Caicos Islands</summary>

| state decimal | Hexadecimal | Subdivision |
| --- | --- | --- |
| 0 | 0x00 | Unset |
| 1 | 0x01 | Turks and Caicos Islands |

</details>

<details>
<summary>country=49 (0x31, US) — United States</summary>

| state decimal | Hexadecimal | Subdivision |
| --- | --- | --- |
| 0 | 0x00 | Unset |
| 2 | 0x02 | District of Columbia |
| 3 | 0x03 | Alaska |
| 4 | 0x04 | Alabama |
| 5 | 0x05 | Arkansas |
| 6 | 0x06 | Arizona |
| 7 | 0x07 | California |
| 8 | 0x08 | Colorado |
| 9 | 0x09 | Connecticut |
| 10 | 0x0A | Delaware |
| 11 | 0x0B | Florida |
| 12 | 0x0C | Georgia |
| 13 | 0x0D | Hawaii |
| 14 | 0x0E | Iowa |
| 15 | 0x0F | Idaho |
| 16 | 0x10 | Illinois |
| 17 | 0x11 | Indiana |
| 18 | 0x12 | Kansas |
| 19 | 0x13 | Kentucky |
| 20 | 0x14 | Louisiana |
| 21 | 0x15 | Massachusetts |
| 22 | 0x16 | Maryland |
| 23 | 0x17 | Maine |
| 24 | 0x18 | Michigan |
| 25 | 0x19 | Minnesota |
| 26 | 0x1A | Missouri |
| 27 | 0x1B | Mississippi |
| 28 | 0x1C | Montana |
| 29 | 0x1D | North Carolina |
| 30 | 0x1E | North Dakota |
| 31 | 0x1F | Nebraska |
| 32 | 0x20 | New Hampshire |
| 33 | 0x21 | New Jersey |
| 34 | 0x22 | New Mexico |
| 35 | 0x23 | Nevada |
| 36 | 0x24 | New York |
| 37 | 0x25 | Ohio |
| 38 | 0x26 | Oklahoma |
| 39 | 0x27 | Oregon |
| 40 | 0x28 | Pennsylvania |
| 41 | 0x29 | Rhode Island |
| 42 | 0x2A | South Carolina |
| 43 | 0x2B | South Dakota |
| 44 | 0x2C | Tennessee |
| 45 | 0x2D | Texas |
| 46 | 0x2E | Utah |
| 47 | 0x2F | Virginia |
| 48 | 0x30 | Vermont |
| 49 | 0x31 | Washington |
| 50 | 0x32 | Wisconsin |
| 51 | 0x33 | West Virginia |
| 52 | 0x34 | Wyoming |
| 53 | 0x35 | Puerto Rico |

</details>

<details>
<summary>country=50 (0x32, UY) — Uruguay</summary>

| state decimal | Hexadecimal | Subdivision |
| --- | --- | --- |
| 0 | 0x00 | Unset |
| 2 | 0x02 | Montevideo |
| 3 | 0x03 | Artigas |
| 4 | 0x04 | Canelones |
| 5 | 0x05 | Cerro Largo |
| 6 | 0x06 | Colonia |
| 7 | 0x07 | Durazno |
| 8 | 0x08 | Flores |
| 9 | 0x09 | Florida |
| 10 | 0x0A | Lavalleja |
| 11 | 0x0B | Maldonado |
| 12 | 0x0C | Paysandú |
| 13 | 0x0D | Río Negro |
| 14 | 0x0E | Rivera |
| 15 | 0x0F | Rocha |
| 16 | 0x10 | Salto |
| 17 | 0x11 | San José |
| 18 | 0x12 | Soriano |
| 19 | 0x13 | Tacuarembó |
| 20 | 0x14 | Treinta y Tres |

</details>

<details>
<summary>country=51 (0x33, VI) — US Virgin Islands</summary>

| state decimal | Hexadecimal | Subdivision |
| --- | --- | --- |
| 0 | 0x00 | Unset |
| 1 | 0x01 | US Virgin Islands |

</details>

<details>
<summary>country=52 (0x34, VE) — Venezuela</summary>

| state decimal | Hexadecimal | Subdivision |
| --- | --- | --- |
| 0 | 0x00 | Unset |
| 2 | 0x02 | Distrito Federal |
| 3 | 0x03 | Amazonas |
| 4 | 0x04 | Anzoátegui |
| 5 | 0x05 | Apure |
| 6 | 0x06 | Aragua |
| 7 | 0x07 | Barinas |
| 8 | 0x08 | Bolívar |
| 9 | 0x09 | Carabobo |
| 10 | 0x0A | Cojedes |
| 11 | 0x0B | Delta Amacuro |
| 12 | 0x0C | Falcón |
| 13 | 0x0D | Guárico |
| 14 | 0x0E | Lara |
| 15 | 0x0F | Mérida |
| 16 | 0x10 | Miranda |
| 17 | 0x11 | Monagas |
| 18 | 0x12 | Nueva Esparta |
| 19 | 0x13 | Portuguesa |
| 20 | 0x14 | Sucre |
| 21 | 0x15 | Táchira |
| 22 | 0x16 | Trujillo |
| 23 | 0x17 | Yaracuy |
| 24 | 0x18 | Zulia |
| 25 | 0x19 | Dependencias Federales |
| 26 | 0x1A | Vargas |

</details>

<details>
<summary>country=64 (0x40, AL) — Albania</summary>

| state decimal | Hexadecimal | Subdivision |
| --- | --- | --- |
| 0 | 0x00 | Unset |
| 2 | 0x02 | Tirana |
| 3 | 0x03 | Berat |
| 4 | 0x04 | Dibër |
| 5 | 0x05 | Durrës |
| 6 | 0x06 | Elbasan |
| 7 | 0x07 | Fier |
| 8 | 0x08 | Gjirokastër |
| 9 | 0x09 | Korçë |
| 10 | 0x0A | Kukës |
| 11 | 0x0B | Lezhë |
| 12 | 0x0C | Shkodër |
| 13 | 0x0D | Vlorë |

</details>

<details>
<summary>country=65 (0x41, AU) — Australia</summary>

| state decimal | Hexadecimal | Subdivision |
| --- | --- | --- |
| 0 | 0x00 | Unset |
| 2 | 0x02 | Australian Capital Territory |
| 3 | 0x03 | New South Wales |
| 4 | 0x04 | Northern Territory |
| 5 | 0x05 | Queensland |
| 6 | 0x06 | South Australia |
| 7 | 0x07 | Tasmania |
| 8 | 0x08 | Victoria |
| 9 | 0x09 | Western Australia |

</details>

<details>
<summary>country=66 (0x42, AT) — Austria</summary>

| state decimal | Hexadecimal | Subdivision |
| --- | --- | --- |
| 0 | 0x00 | Unset |
| 2 | 0x02 | Vienna |
| 3 | 0x03 | Burgenland |
| 4 | 0x04 | Carinthia |
| 5 | 0x05 | Lower Austria |
| 6 | 0x06 | Upper Austria |
| 7 | 0x07 | Salzburg |
| 8 | 0x08 | Styria |
| 9 | 0x09 | Tyrol |
| 10 | 0x0A | Vorarlberg |

</details>

<details>
<summary>country=67 (0x43, BE) — Belgium</summary>

| state decimal | Hexadecimal | Subdivision |
| --- | --- | --- |
| 0 | 0x00 | Unset |
| 2 | 0x02 | Brussels Region |
| 3 | 0x03 | Flanders |
| 4 | 0x04 | Wallonia |

</details>

<details>
<summary>country=68 (0x44, BA) — Bosnia and Herzegovina</summary>

| state decimal | Hexadecimal | Subdivision |
| --- | --- | --- |
| 0 | 0x00 | Unset |
| 2 | 0x02 | Federation of Bosnia and Herzegovina |
| 3 | 0x03 | Republika Srpska |
| 4 | 0x04 | Brčko District |

</details>

<details>
<summary>country=69 (0x45, BW) — Botswana</summary>

| state decimal | Hexadecimal | Subdivision |
| --- | --- | --- |
| 0 | 0x00 | Unset |
| 1 | 0x01 | Botswana |

</details>

<details>
<summary>country=70 (0x46, BG) — Bulgaria</summary>

| state decimal | Hexadecimal | Subdivision |
| --- | --- | --- |
| 0 | 0x00 | Unset |
| 2 | 0x02 | Sofia City |
| 3 | 0x03 | Sofia Province |
| 4 | 0x04 | Blagoevgrad |
| 5 | 0x05 | Pleven |
| 6 | 0x06 | Vidin |
| 7 | 0x07 | Varna |
| 8 | 0x08 | Burgas |
| 9 | 0x09 | Dobrich |
| 10 | 0x0A | Gabrovo |
| 11 | 0x0B | Haskovo |
| 12 | 0x0C | Yambol |
| 13 | 0x0D | Kardzhali |
| 14 | 0x0E | Kyustendil |
| 15 | 0x0F | Lovech |
| 16 | 0x10 | Montana |
| 17 | 0x11 | Pazardzhik |
| 18 | 0x12 | Pernik |
| 19 | 0x13 | Plovdiv |
| 20 | 0x14 | Razgrad |
| 21 | 0x15 | Ruse |
| 22 | 0x16 | Silistra |
| 23 | 0x17 | Sliven |
| 24 | 0x18 | Smolyan |
| 25 | 0x19 | Stara Zagora |
| 26 | 0x1A | Shumen |
| 27 | 0x1B | Targovishte |
| 28 | 0x1C | Veliko Tarnovo |
| 29 | 0x1D | Vratsa |

</details>

<details>
<summary>country=71 (0x47, HR) — Croatia</summary>

| state decimal | Hexadecimal | Subdivision |
| --- | --- | --- |
| 0 | 0x00 | Unset |
| 6 | 0x06 | Zagreb |
| 7 | 0x07 | Bjelovar-Bilogora County |
| 8 | 0x08 | Brod-Posavina County |
| 9 | 0x09 | Dubrovnik-Neretva County |
| 10 | 0x0A | Istria County |
| 11 | 0x0B | Karlovac County |
| 12 | 0x0C | Koprivnica-Križevci County |
| 13 | 0x0D | Krapina-Zagorje County |
| 14 | 0x0E | Lika-Senj County |
| 15 | 0x0F | Međimurje County |
| 16 | 0x10 | Osijek-Baranja County |
| 17 | 0x11 | Požega-Slavonia County |
| 18 | 0x12 | Primorje-Gorski Kotar County |
| 19 | 0x13 | Sisak-Moslavina County |
| 20 | 0x14 | Split-Dalmatia County |
| 21 | 0x15 | Šibenik-Knin County |
| 22 | 0x16 | Varaždin County |
| 23 | 0x17 | Virovitica-Podravina County |
| 24 | 0x18 | Vukovar-Syrmia County |
| 25 | 0x19 | Zadar County |
| 26 | 0x1A | Zagreb County |

</details>

<details>
<summary>country=72 (0x48, CY) — Cyprus</summary>

| state decimal | Hexadecimal | Subdivision |
| --- | --- | --- |
| 0 | 0x00 | Unset |
| 1 | 0x01 | Cyprus |

</details>

<details>
<summary>country=73 (0x49, CZ) — Czech Republic</summary>

| state decimal | Hexadecimal | Subdivision |
| --- | --- | --- |
| 0 | 0x00 | Unset |
| 2 | 0x02 | Prague |
| 3 | 0x03 | Central Bohemian Region |
| 4 | 0x04 | South Bohemian Region |
| 5 | 0x05 | Plzeň Region |
| 6 | 0x06 | Karlovy Vary Region |
| 7 | 0x07 | Ústí nad Labem Region |
| 8 | 0x08 | Liberec Region |
| 9 | 0x09 | Hradec Králové Region |
| 10 | 0x0A | Pardubice Region |
| 11 | 0x0B | Olomouc Region |
| 12 | 0x0C | Moravian-Silesian Region |
| 13 | 0x0D | South Moravian Region |
| 14 | 0x0E | Zlín Region |
| 15 | 0x0F | Vysočina Region |

</details>

<details>
<summary>country=74 (0x4A, DK) — Denmark (Kingdom of)</summary>

| state decimal | Hexadecimal | Subdivision |
| --- | --- | --- |
| 0 | 0x00 | Unset |
| 18 | 0x12 | Greenland |
| 19 | 0x13 | Capital Region of Denmark |
| 20 | 0x14 | Central Denmark Region |
| 21 | 0x15 | North Denmark Region |
| 22 | 0x16 | Region Zealand |
| 23 | 0x17 | Region of Southern Denmark |
| 24 | 0x18 | Faroe Islands |

</details>

<details>
<summary>country=75 (0x4B, EE) — Estonia</summary>

| state decimal | Hexadecimal | Subdivision |
| --- | --- | --- |
| 0 | 0x00 | Unset |
| 1 | 0x01 | Estonia |

</details>

<details>
<summary>country=76 (0x4C, FI) — Finland</summary>

| state decimal | Hexadecimal | Subdivision |
| --- | --- | --- |
| 0 | 0x00 | Unset |
| 8 | 0x08 | Uusimaa / Nyland |
| 9 | 0x09 | Lappi / Lapland |
| 10 | 0x0A | Pohjois-Pohjanmaa / Norra Österbotten |
| 11 | 0x0B | Kainuu / Kajanaland |
| 12 | 0x0C | Pohjois-Karjala / Norra Karelen |
| 13 | 0x0D | Pohjois-Savo / Norra Savolax |
| 14 | 0x0E | Etelä-Savo / Södra Savolax |
| 15 | 0x0F | Etelä-Pohjanmaa / Södra Österbotten |
| 16 | 0x10 | Pohjanmaa / Österbotten |
| 17 | 0x11 | Pirkanmaa / Birkaland |
| 18 | 0x12 | Satakunta / Satakunda |
| 19 | 0x13 | Keski-Pohjanmaa / Mellersta Österbotten |
| 20 | 0x14 | Keski-Suomi / Mellersta Finland |
| 21 | 0x15 | Varsinais-Suomi / Egentliga Finland |
| 22 | 0x16 | Etelä-Karjala / Södra Karelen |
| 23 | 0x17 | Päijät-Häme / Päijänne Tavastland |
| 24 | 0x18 | Kanta-Häme / Egentliga Tavastland |
| 25 | 0x19 | Itä-Uusimaa / Östra Nyland |
| 26 | 0x1A | Kymenlaakso / Kymmenedalen |
| 27 | 0x1B | Ahvenanmaa / Åland |

</details>

<details>
<summary>country=77 (0x4D, FR) — France</summary>

| state decimal | Hexadecimal | Subdivision |
| --- | --- | --- |
| 0 | 0x00 | Unset |
| 2 | 0x02 | Île-de-France |
| 3 | 0x03 | Alsace |
| 4 | 0x04 | Aquitaine |
| 5 | 0x05 | Auvergne |
| 6 | 0x06 | Lower Normandy |
| 7 | 0x07 | Burgundy |
| 8 | 0x08 | Brittany |
| 9 | 0x09 | Centre |
| 10 | 0x0A | Champagne-Ardenne |
| 11 | 0x0B | Corsica |
| 12 | 0x0C | Franche-Comté |
| 13 | 0x0D | Upper Normandy |
| 14 | 0x0E | Languedoc-Roussillon |
| 15 | 0x0F | Limousin |
| 16 | 0x10 | Lorraine |
| 17 | 0x11 | Midi-Pyrénées |
| 18 | 0x12 | Nord-Pas-de-Calais |
| 19 | 0x13 | Pays de la Loire |
| 20 | 0x14 | Picardy |
| 21 | 0x15 | Poitou-Charentes |
| 22 | 0x16 | Provence-Alpes-Côte d'Azur |
| 23 | 0x17 | Rhône-Alpes |
| 24 | 0x18 | Guadeloupe |
| 25 | 0x19 | Martinique |
| 26 | 0x1A | French Guiana |
| 27 | 0x1B | Réunion |

</details>

<details>
<summary>country=78 (0x4E, DE) — Germany</summary>

| state decimal | Hexadecimal | Subdivision |
| --- | --- | --- |
| 0 | 0x00 | Unset |
| 2 | 0x02 | Berlin |
| 3 | 0x03 | Hesse |
| 4 | 0x04 | Baden-Württemberg |
| 5 | 0x05 | Bavaria |
| 6 | 0x06 | Brandenburg |
| 7 | 0x07 | Bremen |
| 8 | 0x08 | Hamburg |
| 9 | 0x09 | Mecklenburg-Vorpommern |
| 10 | 0x0A | Lower Saxony |
| 11 | 0x0B | North Rhine-Westphalia |
| 12 | 0x0C | Rhineland-Palatinate |
| 13 | 0x0D | Saarland |
| 14 | 0x0E | Saxony |
| 15 | 0x0F | Saxony-Anhalt |
| 16 | 0x10 | Schleswig-Holstein |
| 17 | 0x11 | Thuringia |

</details>

<details>
<summary>country=79 (0x4F, GR) — Greece</summary>

| state decimal | Hexadecimal | Subdivision |
| --- | --- | --- |
| 0 | 0x00 | Unset |
| 2 | 0x02 | Attica |
| 3 | 0x03 | Central Greece |
| 4 | 0x04 | Central Macedonia |
| 5 | 0x05 | Crete |
| 6 | 0x06 | East Macedonia and Thrace |
| 7 | 0x07 | Epirus |
| 8 | 0x08 | Ionian Islands |
| 9 | 0x09 | North Aegean |
| 10 | 0x0A | Peloponnese |
| 11 | 0x0B | South Aegean |
| 12 | 0x0C | Thessaly |
| 13 | 0x0D | West Greece |
| 14 | 0x0E | West Macedonia |

</details>

<details>
<summary>country=80 (0x50, HU) — Hungary</summary>

| state decimal | Hexadecimal | Subdivision |
| --- | --- | --- |
| 0 | 0x00 | Unset |
| 2 | 0x02 | Budapest |
| 3 | 0x03 | Bács-Kiskun County |
| 4 | 0x04 | Baranya County |
| 5 | 0x05 | Békés County |
| 6 | 0x06 | Borsod-Abaúj-Zemplén County |
| 7 | 0x07 | Csongrád County |
| 8 | 0x08 | Fejér County |
| 9 | 0x09 | Győr-Moson-Sopron County |
| 10 | 0x0A | Hajdú-Bihar County |
| 11 | 0x0B | Heves County |
| 12 | 0x0C | Jász-Nagykun-Szolnok County |
| 13 | 0x0D | Komárom-Esztergom County |
| 14 | 0x0E | Nógrád County |
| 15 | 0x0F | Pest County |
| 16 | 0x10 | Somogy County |
| 17 | 0x11 | Szabolcs-Szatmár-Bereg County |
| 18 | 0x12 | Tolna County |
| 19 | 0x13 | Vas County |
| 20 | 0x14 | Veszprém County |
| 21 | 0x15 | Zala County |

</details>

<details>
<summary>country=81 (0x51, IS) — Iceland</summary>

| state decimal | Hexadecimal | Subdivision |
| --- | --- | --- |
| 0 | 0x00 | Unset |
| 1 | 0x01 | Iceland |

</details>

<details>
<summary>country=82 (0x52, IE) — Ireland</summary>

| state decimal | Hexadecimal | Subdivision |
| --- | --- | --- |
| 0 | 0x00 | Unset |
| 2 | 0x02 | Dublin |
| 10 | 0x0A | County Carlow |
| 11 | 0x0B | County Cavan |
| 12 | 0x0C | County Clare |
| 13 | 0x0D | County Cork |
| 14 | 0x0E | County Donegal |
| 15 | 0x0F | County Galway |
| 16 | 0x10 | County Kerry |
| 17 | 0x11 | County Kildare |
| 18 | 0x12 | County Kilkenny |
| 19 | 0x13 | County Laois |
| 20 | 0x14 | County Leitrim |
| 21 | 0x15 | County Limerick |
| 22 | 0x16 | County Longford |
| 23 | 0x17 | County Louth |
| 24 | 0x18 | County Mayo |
| 25 | 0x19 | County Meath |
| 26 | 0x1A | County Monaghan |
| 27 | 0x1B | County Offaly |
| 28 | 0x1C | County Roscommon |
| 29 | 0x1D | County Sligo |
| 30 | 0x1E | County Tipperary |
| 31 | 0x1F | County Waterford |
| 32 | 0x20 | County Westmeath |
| 33 | 0x21 | County Wexford |
| 34 | 0x22 | County Wicklow |

</details>

<details>
<summary>country=83 (0x53, IT) — Italy</summary>

| state decimal | Hexadecimal | Subdivision |
| --- | --- | --- |
| 0 | 0x00 | Unset |
| 2 | 0x02 | Lazio |
| 3 | 0x03 | Aosta Valley |
| 4 | 0x04 | Piedmont |
| 5 | 0x05 | Liguria |
| 6 | 0x06 | Lombardy |
| 7 | 0x07 | Trentino-Alto Adige |
| 8 | 0x08 | Veneto |
| 9 | 0x09 | Friuli Venezia Giulia |
| 10 | 0x0A | Emilia-Romagna |
| 11 | 0x0B | Tuscany |
| 12 | 0x0C | Umbria |
| 13 | 0x0D | Marche |
| 14 | 0x0E | Abruzzo |
| 15 | 0x0F | Molise |
| 16 | 0x10 | Campania |
| 17 | 0x11 | Apulia |
| 18 | 0x12 | Basilicata |
| 19 | 0x13 | Calabria |
| 20 | 0x14 | Sicily |
| 21 | 0x15 | Sardinia |

</details>

<details>
<summary>country=84 (0x54, LV) — Latvia</summary>

| state decimal | Hexadecimal | Subdivision |
| --- | --- | --- |
| 0 | 0x00 | Unset |
| 1 | 0x01 | Latvia |

</details>

<details>
<summary>country=85 (0x55, LS) — Lesotho</summary>

| state decimal | Hexadecimal | Subdivision |
| --- | --- | --- |
| 0 | 0x00 | Unset |
| 2 | 0x02 | Maseru |
| 3 | 0x03 | Berea |
| 4 | 0x04 | Butha-Buthe |
| 5 | 0x05 | Leribe |
| 6 | 0x06 | Mafeteng |
| 7 | 0x07 | Mohale's Hoek |
| 8 | 0x08 | Mokhotlong |
| 9 | 0x09 | Qacha's Nek |
| 10 | 0x0A | Quthing |
| 11 | 0x0B | Thaba-Tseka |

</details>

<details>
<summary>country=86 (0x56, LI) — Liechtenstein</summary>

| state decimal | Hexadecimal | Subdivision |
| --- | --- | --- |
| 0 | 0x00 | Unset |
| 1 | 0x01 | Liechtenstein |

</details>

<details>
<summary>country=87 (0x57, LT) — Lithuania</summary>

| state decimal | Hexadecimal | Subdivision |
| --- | --- | --- |
| 0 | 0x00 | Unset |
| 2 | 0x02 | Vilnius |
| 3 | 0x03 | Alytus |
| 4 | 0x04 | Kaunas |
| 5 | 0x05 | Klaipėda |
| 6 | 0x06 | Marijampolė |
| 7 | 0x07 | Panevėžys |
| 8 | 0x08 | Šiauliai |
| 9 | 0x09 | Taurage |
| 10 | 0x0A | Telšiai |
| 11 | 0x0B | Utena |

</details>

<details>
<summary>country=88 (0x58, LU) — Luxembourg</summary>

| state decimal | Hexadecimal | Subdivision |
| --- | --- | --- |
| 0 | 0x00 | Unset |
| 1 | 0x01 | Luxembourg |

</details>

<details>
<summary>country=89 (0x59, MK) — Macedonia (Republic of)</summary>

| state decimal | Hexadecimal | Subdivision |
| --- | --- | --- |
| 0 | 0x00 | Unset |
| 1 | 0x01 | Macedonia (Republic of) |

</details>

<details>
<summary>country=90 (0x5A, MT) — Malta</summary>

| state decimal | Hexadecimal | Subdivision |
| --- | --- | --- |
| 0 | 0x00 | Unset |
| 1 | 0x01 | Malta |

</details>

<details>
<summary>country=91 (0x5B, ME) — Montenegro</summary>

| state decimal | Hexadecimal | Subdivision |
| --- | --- | --- |
| 0 | 0x00 | Unset |
| 1 | 0x01 | Montenegro |

</details>

<details>
<summary>country=92 (0x5C, MZ) — Mozambique</summary>

| state decimal | Hexadecimal | Subdivision |
| --- | --- | --- |
| 0 | 0x00 | Unset |
| 1 | 0x01 | Mozambique |

</details>

<details>
<summary>country=93 (0x5D, NA) — Namibia</summary>

| state decimal | Hexadecimal | Subdivision |
| --- | --- | --- |
| 0 | 0x00 | Unset |
| 1 | 0x01 | Namibia |

</details>

<details>
<summary>country=94 (0x5E, NL) — Netherlands</summary>

| state decimal | Hexadecimal | Subdivision |
| --- | --- | --- |
| 0 | 0x00 | Unset |
| 2 | 0x02 | North Holland |
| 3 | 0x03 | Drenthe |
| 4 | 0x04 | Flevoland |
| 5 | 0x05 | Friesland |
| 6 | 0x06 | Gelderland |
| 7 | 0x07 | Groningen |
| 8 | 0x08 | Limburg |
| 9 | 0x09 | North Brabant |
| 10 | 0x0A | Overijssel |
| 11 | 0x0B | South Holland |
| 12 | 0x0C | Utrecht |
| 13 | 0x0D | Zeeland |

</details>

<details>
<summary>country=95 (0x5F, NZ) — New Zealand</summary>

| state decimal | Hexadecimal | Subdivision |
| --- | --- | --- |
| 0 | 0x00 | Unset |
| 2 | 0x02 | Wellington |
| 3 | 0x03 | Auckland |
| 4 | 0x04 | Bay of Plenty |
| 5 | 0x05 | Canterbury |
| 6 | 0x06 | Otago |
| 7 | 0x07 | Hawke's Bay |
| 8 | 0x08 | Manawatu-Wanganui |
| 9 | 0x09 | Nelson |
| 10 | 0x0A | Northland |
| 12 | 0x0C | Southland |
| 13 | 0x0D | Taranaki |
| 14 | 0x0E | Waikato |
| 15 | 0x0F | Gisborne |
| 16 | 0x10 | West Coast |
| 17 | 0x11 | Marlborough |
| 18 | 0x12 | Tasman |

</details>

<details>
<summary>country=96 (0x60, NO) — Norway</summary>

| state decimal | Hexadecimal | Subdivision |
| --- | --- | --- |
| 0 | 0x00 | Unset |
| 7 | 0x07 | Oslo |
| 8 | 0x08 | Akershus |
| 9 | 0x09 | Aust-Agder |
| 10 | 0x0A | Buskerud |
| 11 | 0x0B | Finnmark |
| 12 | 0x0C | Hedmark |
| 13 | 0x0D | Hordaland |
| 14 | 0x0E | Møre og Romsdal |
| 15 | 0x0F | Nordland |
| 16 | 0x10 | Nord-Trøndelag |
| 17 | 0x11 | Oppland |
| 18 | 0x12 | Rogaland |
| 19 | 0x13 | Sogn og Fjordane |
| 20 | 0x14 | Sør-Trøndelag |
| 21 | 0x15 | Telemark |
| 22 | 0x16 | Troms |
| 23 | 0x17 | Vest-Agder |
| 24 | 0x18 | Vestfold |
| 25 | 0x19 | Østfold |
| 26 | 0x1A | Svalbard |

</details>

<details>
<summary>country=97 (0x61, PL) — Poland</summary>

| state decimal | Hexadecimal | Subdivision |
| --- | --- | --- |
| 0 | 0x00 | Unset |
| 2 | 0x02 | Masovia |
| 3 | 0x03 | Lower Silesia |
| 4 | 0x04 | Kuyavian-Pomeranian Voivodeship |
| 5 | 0x05 | Lodz |
| 6 | 0x06 | Lublin |
| 7 | 0x07 | Lubusz |
| 8 | 0x08 | Lesser Poland |
| 9 | 0x09 | Opole |
| 10 | 0x0A | Subcarpathia |
| 11 | 0x0B | Podlachia |
| 12 | 0x0C | Pomerania |
| 13 | 0x0D | Silesia |
| 14 | 0x0E | Świętokrzyskie |
| 15 | 0x0F | Warmian-Masurian Voivodeship |
| 16 | 0x10 | Greater Poland |
| 17 | 0x11 | Western Pomerania |

</details>

<details>
<summary>country=98 (0x62, PT) — Portugal</summary>

| state decimal | Hexadecimal | Subdivision |
| --- | --- | --- |
| 0 | 0x00 | Unset |
| 2 | 0x02 | Lisbon |
| 7 | 0x07 | Madeira |
| 8 | 0x08 | Azores |
| 9 | 0x09 | Aveiro |
| 10 | 0x0A | Beja |
| 11 | 0x0B | Braga |
| 12 | 0x0C | Bragança |
| 13 | 0x0D | Castelo Branco |
| 14 | 0x0E | Coimbra |
| 15 | 0x0F | Évora |
| 16 | 0x10 | Faro |
| 17 | 0x11 | Guarda |
| 18 | 0x12 | Leiria |
| 19 | 0x13 | Portalegre |
| 20 | 0x14 | Porto |
| 21 | 0x15 | Santarém |
| 22 | 0x16 | Setúbal |
| 23 | 0x17 | Viana do Castelo |
| 24 | 0x18 | Vila Real |
| 25 | 0x19 | Viseu |

</details>

<details>
<summary>country=99 (0x63, RO) — Romania</summary>

| state decimal | Hexadecimal | Subdivision |
| --- | --- | --- |
| 0 | 0x00 | Unset |
| 2 | 0x02 | Bucharest |
| 3 | 0x03 | Alba |
| 4 | 0x04 | Arad |
| 5 | 0x05 | Arges |
| 6 | 0x06 | Bacau |
| 7 | 0x07 | Bihor |
| 8 | 0x08 | Bistrita-Nasaud |
| 9 | 0x09 | Botosani |
| 10 | 0x0A | Braila |
| 11 | 0x0B | Brasov |
| 12 | 0x0C | Buzau |
| 13 | 0x0D | Calarasi |
| 14 | 0x0E | Caras-Severin |
| 15 | 0x0F | Cluj |
| 16 | 0x10 | Constanta |
| 17 | 0x11 | Covasna |
| 18 | 0x12 | Dâmbovita |
| 19 | 0x13 | Dolj |
| 20 | 0x14 | Galati |
| 21 | 0x15 | Giurgiu |
| 22 | 0x16 | Gorj |
| 23 | 0x17 | Harghita |
| 24 | 0x18 | Hunedoara |
| 25 | 0x19 | Ialomita |
| 26 | 0x1A | Iasi |
| 27 | 0x1B | Ilfov |
| 28 | 0x1C | Maramures |
| 29 | 0x1D | Mehedinti |
| 30 | 0x1E | Mures |
| 31 | 0x1F | Neamt |
| 32 | 0x20 | Olt |
| 33 | 0x21 | Prahova |
| 34 | 0x22 | Salaj |
| 35 | 0x23 | Satu Mare |
| 36 | 0x24 | Sibiu |
| 37 | 0x25 | Suceava |
| 38 | 0x26 | Teleorman |
| 39 | 0x27 | Timis |
| 40 | 0x28 | Tulcea |
| 41 | 0x29 | Vâlcea |
| 42 | 0x2A | Vaslui |
| 43 | 0x2B | Vrancea |

</details>

<details>
<summary>country=100 (0x64, RU) — Russia</summary>

| state decimal | Hexadecimal | Subdivision |
| --- | --- | --- |
| 0 | 0x00 | Unset |
| 9 | 0x09 | Moscow City |
| 10 | 0x0A | Adygey |
| 11 | 0x0B | Gorno-Altay |
| 12 | 0x0C | Altay |
| 13 | 0x0D | Amur |
| 14 | 0x0E | Arkhangel'sk |
| 15 | 0x0F | Astrakhan' |
| 16 | 0x10 | Bashkortostan |
| 17 | 0x11 | Belgorod |
| 18 | 0x12 | Bryansk |
| 19 | 0x13 | Buryat |
| 20 | 0x14 | Chechnya |
| 21 | 0x15 | Chelyabinsk |
| 22 | 0x16 | Chukot |
| 23 | 0x17 | Chuvash |
| 24 | 0x18 | Dagestan |
| 25 | 0x19 | Ingushetia |
| 26 | 0x1A | Irkutsk |
| 27 | 0x1B | Ivanovo |
| 28 | 0x1C | Kabardin-Balkar |
| 29 | 0x1D | Kaliningrad |
| 30 | 0x1E | Kalmyk |
| 31 | 0x1F | Kaluga |
| 32 | 0x20 | Kamchatka |
| 33 | 0x21 | Karachay-Cherkess |
| 34 | 0x22 | Karelia |
| 35 | 0x23 | Kemerovo |
| 36 | 0x24 | Khabarovsk |
| 37 | 0x25 | Khakassia |
| 38 | 0x26 | Khanty-Mansiy |
| 39 | 0x27 | Kirov |
| 40 | 0x28 | Komi |
| 41 | 0x29 | Kostroma |
| 42 | 0x2A | Krasnodar |
| 43 | 0x2B | Krasnoyarsk |
| 44 | 0x2C | Kurgan |
| 45 | 0x2D | Kursk |
| 46 | 0x2E | Leningrad |
| 47 | 0x2F | Lipetsk |
| 48 | 0x30 | Magadan |
| 49 | 0x31 | Mariy-El |
| 50 | 0x32 | Mordovia |
| 51 | 0x33 | Moscow |
| 52 | 0x34 | Murmansk |
| 53 | 0x35 | Nenets |
| 54 | 0x36 | Nizhegorod |
| 55 | 0x37 | Novgorod |
| 56 | 0x38 | Novosibirsk |
| 57 | 0x39 | Omsk |
| 58 | 0x3A | Orenburg |
| 59 | 0x3B | Orel |
| 60 | 0x3C | Penza |
| 61 | 0x3D | Perm' |
| 62 | 0x3E | Primor'ye |
| 63 | 0x3F | Pskov |
| 64 | 0x40 | Rostov |
| 65 | 0x41 | Ryazan' |
| 66 | 0x42 | Sakha |
| 67 | 0x43 | Sakhalin |
| 68 | 0x44 | Samara |
| 69 | 0x45 | St. Petersburg |
| 70 | 0x46 | Saratov |
| 71 | 0x47 | North Ossetia |
| 72 | 0x48 | Smolensk |
| 73 | 0x49 | Stavropol' |
| 74 | 0x4A | Sverdlovsk |
| 75 | 0x4B | Tambov |
| 76 | 0x4C | Tatarstan |
| 77 | 0x4D | Tomsk |
| 78 | 0x4E | Tula |
| 79 | 0x4F | Tver' |
| 80 | 0x50 | Tyumen' |
| 81 | 0x51 | Tuva |
| 82 | 0x52 | Udmurt |
| 83 | 0x53 | Ul'yanovsk |
| 84 | 0x54 | Vladimir |
| 85 | 0x55 | Volgograd |
| 86 | 0x56 | Vologda |
| 87 | 0x57 | Voronezh |
| 88 | 0x58 | Yamal-Nenets |
| 89 | 0x59 | Yaroslavl' |
| 90 | 0x5A | Yevrey |
| 91 | 0x5B | Zabaykal'ye |

</details>

<details>
<summary>country=101 (0x65, RS) — Serbia and Kosovo</summary>

| state decimal | Hexadecimal | Subdivision |
| --- | --- | --- |
| 0 | 0x00 | Unset |
| 1 | 0x01 | Serbia and Kosovo |

</details>

<details>
<summary>country=102 (0x66, SK) — Slovakia</summary>

| state decimal | Hexadecimal | Subdivision |
| --- | --- | --- |
| 0 | 0x00 | Unset |
| 2 | 0x02 | Bratislava |
| 3 | 0x03 | Banská Bystrica |
| 4 | 0x04 | Košice |
| 5 | 0x05 | Nitra |
| 6 | 0x06 | Prešov |
| 7 | 0x07 | Trencín |
| 8 | 0x08 | Trnava |
| 9 | 0x09 | Žilina |

</details>

<details>
<summary>country=103 (0x67, SI) — Slovenia</summary>

| state decimal | Hexadecimal | Subdivision |
| --- | --- | --- |
| 0 | 0x00 | Unset |
| 1 | 0x01 | Slovenia |

</details>

<details>
<summary>country=104 (0x68, ZA) — South Africa</summary>

| state decimal | Hexadecimal | Subdivision |
| --- | --- | --- |
| 0 | 0x00 | Unset |
| 2 | 0x02 | Gauteng |
| 3 | 0x03 | Western Cape |
| 4 | 0x04 | Northern Cape |
| 5 | 0x05 | Eastern Cape |
| 6 | 0x06 | KwaZulu-Natal |
| 7 | 0x07 | Free State |
| 8 | 0x08 | North West |
| 9 | 0x09 | Mpumalanga |
| 10 | 0x0A | Limpopo |

</details>

<details>
<summary>country=105 (0x69, ES) — Spain</summary>

| state decimal | Hexadecimal | Subdivision |
| --- | --- | --- |
| 0 | 0x00 | Unset |
| 2 | 0x02 | Madrid |
| 3 | 0x03 | Andalusia |
| 4 | 0x04 | Aragon |
| 5 | 0x05 | Principality of Asturias |
| 6 | 0x06 | Balearic Islands |
| 7 | 0x07 | Canary Islands |
| 8 | 0x08 | Cantabria |
| 9 | 0x09 | Castile-La Mancha |
| 10 | 0x0A | Castilla y León |
| 11 | 0x0B | Catalonia |
| 12 | 0x0C | Valencia |
| 13 | 0x0D | Extremadura |
| 14 | 0x0E | Galicia |
| 15 | 0x0F | Murcia |
| 16 | 0x10 | Navarre |
| 17 | 0x11 | Basque Country |
| 18 | 0x12 | La Rioja |
| 19 | 0x13 | Ceuta |
| 20 | 0x14 | Melilla |

</details>

<details>
<summary>country=106 (0x6A, SZ) — Swaziland</summary>

| state decimal | Hexadecimal | Subdivision |
| --- | --- | --- |
| 0 | 0x00 | Unset |
| 2 | 0x02 | Hhohho |
| 3 | 0x03 | Lubombo |
| 4 | 0x04 | Manzini |
| 5 | 0x05 | Shiselweni |

</details>

<details>
<summary>country=107 (0x6B, SE) — Sweden</summary>

| state decimal | Hexadecimal | Subdivision |
| --- | --- | --- |
| 0 | 0x00 | Unset |
| 2 | 0x02 | Stockholm County |
| 3 | 0x03 | Skåne County |
| 4 | 0x04 | Västra Götaland County |
| 5 | 0x05 | Östergötland County |
| 6 | 0x06 | Södermanland County |
| 7 | 0x07 | Värmland County |
| 8 | 0x08 | Uppsala County |
| 9 | 0x09 | Gävleborg County |
| 10 | 0x0A | Västerbotten County |
| 11 | 0x0B | Norrbotten County |
| 12 | 0x0C | Gotland Island |
| 13 | 0x0D | Jämtland County |
| 14 | 0x0E | Dalarna County |
| 15 | 0x0F | Blekinge County |
| 16 | 0x10 | Örebro County |
| 17 | 0x11 | Västernorrland County |
| 18 | 0x12 | Jönköping County |
| 19 | 0x13 | Kronoberg County |
| 20 | 0x14 | Kalmar County |
| 21 | 0x15 | Västmanland County |
| 22 | 0x16 | Halland County |

</details>

<details>
<summary>country=108 (0x6C, CH) — Switzerland</summary>

| state decimal | Hexadecimal | Subdivision |
| --- | --- | --- |
| 0 | 0x00 | Unset |
| 2 | 0x02 | Bern |
| 4 | 0x04 | Aargau |
| 5 | 0x05 | Basel-City |
| 6 | 0x06 | Fribourg |
| 7 | 0x07 | Geneva |
| 8 | 0x08 | Glarus |
| 9 | 0x09 | Graubünden |
| 10 | 0x0A | Jura |
| 11 | 0x0B | Luzern |
| 12 | 0x0C | Neuchâtel |
| 13 | 0x0D | Obwalden |
| 14 | 0x0E | St. Gallen |
| 15 | 0x0F | Schaffhausen |
| 16 | 0x10 | Schwyz |
| 17 | 0x11 | Solothurn |
| 18 | 0x12 | Thurgau |
| 19 | 0x13 | Ticino |
| 20 | 0x14 | Uri |
| 21 | 0x15 | Valais |
| 22 | 0x16 | Vaud |
| 23 | 0x17 | Zug |
| 24 | 0x18 | Zurich |
| 25 | 0x19 | Appenzell Outer Rhodes |
| 26 | 0x1A | Appenzell Inner Rhodes |
| 27 | 0x1B | Basel-Landschaft |
| 28 | 0x1C | Nidwalden |

</details>

<details>
<summary>country=109 (0x6D, TR) — Turkey</summary>

| state decimal | Hexadecimal | Subdivision |
| --- | --- | --- |
| 0 | 0x00 | Unset |
| 2 | 0x02 | Ankara |
| 3 | 0x03 | İstanbul |
| 4 | 0x04 | İzmir |
| 5 | 0x05 | Bursa |
| 6 | 0x06 | Adana |
| 7 | 0x07 | Gaziantep |
| 8 | 0x08 | Konya |
| 9 | 0x09 | Antalya |
| 10 | 0x0A | Diyarbakır |
| 11 | 0x0B | Mersin |
| 12 | 0x0C | Kayseri |
| 14 | 0x0E | Şanlıurfa |
| 15 | 0x0F | Malatya |
| 16 | 0x10 | Erzurum |
| 17 | 0x11 | Samsun |
| 18 | 0x12 | Van |
| 19 | 0x13 | Kahramanmaraş |
| 20 | 0x14 | Denizli |
| 21 | 0x15 | Batman |
| 22 | 0x16 | Elazığ |
| 23 | 0x17 | Sakarya |
| 24 | 0x18 | Kocaeli |
| 25 | 0x19 | Sivas |
| 26 | 0x1A | Manisa |
| 27 | 0x1B | Trabzon |
| 28 | 0x1C | Balıkesir |
| 29 | 0x1D | Adıyaman |
| 30 | 0x1E | Tekirdağ |
| 31 | 0x1F | Kırıkkale |
| 32 | 0x20 | Osmaniye |
| 33 | 0x21 | Kütahya |
| 34 | 0x22 | Çorum |
| 35 | 0x23 | Isparta |
| 36 | 0x24 | Aydın |
| 37 | 0x25 | Hatay |
| 38 | 0x26 | Mardin |
| 39 | 0x27 | Aksaray |
| 40 | 0x28 | Afyonkarahisar |
| 41 | 0x29 | Tokat |
| 42 | 0x2A | Edirne |
| 43 | 0x2B | Karaman |
| 44 | 0x2C | Ordu |
| 45 | 0x2D | Siirt |
| 46 | 0x2E | Erzincan |
| 47 | 0x2F | Çankırı |
| 48 | 0x30 | Zonguldak |
| 49 | 0x31 | Yozgat |
| 50 | 0x32 | Uşak |
| 51 | 0x33 | Ağrı |
| 52 | 0x34 | Amasya |
| 53 | 0x35 | Ardahan |
| 54 | 0x36 | Artvin |
| 55 | 0x37 | Bartın |
| 56 | 0x38 | Bayburt |
| 57 | 0x39 | Bilecik |
| 58 | 0x3A | Bingöl |
| 59 | 0x3B | Bitlis |
| 60 | 0x3C | Bolu |
| 61 | 0x3D | Burdur |
| 62 | 0x3E | Çanakkale |
| 63 | 0x3F | Düzce |
| 64 | 0x40 | Eskişehir |
| 65 | 0x41 | Giresun |
| 66 | 0x42 | Gümüşhane |
| 67 | 0x43 | Hakkari |
| 68 | 0x44 | Iğdır |
| 69 | 0x45 | Karabük |
| 70 | 0x46 | Kars |
| 71 | 0x47 | Kastamonu |
| 72 | 0x48 | Kilis |
| 73 | 0x49 | Kırklareli |
| 74 | 0x4A | Kırşehir |
| 75 | 0x4B | Muğla |
| 76 | 0x4C | Muş |
| 77 | 0x4D | Nevşehir |
| 78 | 0x4E | Niğde |
| 79 | 0x4F | Rize |
| 80 | 0x50 | Sinop |
| 81 | 0x51 | Şırnak |
| 82 | 0x52 | Tunceli |
| 83 | 0x53 | Yalova |

</details>

<details>
<summary>country=110 (0x6E, GB) — United Kingdom</summary>

| state decimal | Hexadecimal | Subdivision |
| --- | --- | --- |
| 0 | 0x00 | Unset |
| 2 | 0x02 | England |
| 4 | 0x04 | Scotland |
| 5 | 0x05 | Wales |
| 6 | 0x06 | Northern Ireland |

</details>

<details>
<summary>country=111 (0x6F, ZM) — Zambia</summary>

| state decimal | Hexadecimal | Subdivision |
| --- | --- | --- |
| 0 | 0x00 | Unset |
| 1 | 0x01 | Zambia |

</details>

<details>
<summary>country=112 (0x70, ZW) — Zimbabwe</summary>

| state decimal | Hexadecimal | Subdivision |
| --- | --- | --- |
| 0 | 0x00 | Unset |
| 1 | 0x01 | Zimbabwe |

</details>

<details>
<summary>country=113 (0x71, AZ) — Azerbaijan</summary>

| state decimal | Hexadecimal | Subdivision |
| --- | --- | --- |
| 0 | 0x00 | Unset |
| 1 | 0x01 | Azerbaijan |

</details>

<details>
<summary>country=114 (0x72, MR) — Mauritania</summary>

| state decimal | Hexadecimal | Subdivision |
| --- | --- | --- |
| 0 | 0x00 | Unset |
| 1 | 0x01 | Mauritania |

</details>

<details>
<summary>country=115 (0x73, ML) — Mali</summary>

| state decimal | Hexadecimal | Subdivision |
| --- | --- | --- |
| 0 | 0x00 | Unset |
| 1 | 0x01 | Mali |

</details>

<details>
<summary>country=116 (0x74, NE) — Niger</summary>

| state decimal | Hexadecimal | Subdivision |
| --- | --- | --- |
| 0 | 0x00 | Unset |
| 1 | 0x01 | Niger |

</details>

<details>
<summary>country=117 (0x75, TD) — Chad</summary>

| state decimal | Hexadecimal | Subdivision |
| --- | --- | --- |
| 0 | 0x00 | Unset |
| 1 | 0x01 | Chad |

</details>

<details>
<summary>country=118 (0x76, SD) — Sudan</summary>

| state decimal | Hexadecimal | Subdivision |
| --- | --- | --- |
| 0 | 0x00 | Unset |
| 1 | 0x01 | Sudan |

</details>

<details>
<summary>country=119 (0x77, ER) — Eritrea</summary>

| state decimal | Hexadecimal | Subdivision |
| --- | --- | --- |
| 0 | 0x00 | Unset |
| 1 | 0x01 | Eritrea |

</details>

<details>
<summary>country=120 (0x78, DJ) — Djibouti</summary>

| state decimal | Hexadecimal | Subdivision |
| --- | --- | --- |
| 0 | 0x00 | Unset |
| 1 | 0x01 | Djibouti |

</details>

<details>
<summary>country=121 (0x79, SO) — Somalia</summary>

| state decimal | Hexadecimal | Subdivision |
| --- | --- | --- |
| 0 | 0x00 | Unset |
| 1 | 0x01 | Somalia |

</details>

<details>
<summary>country=122 (0x7A, AD) — Andorra</summary>

| state decimal | Hexadecimal | Subdivision |
| --- | --- | --- |
| 0 | 0x00 | Unset |
| 1 | 0x01 | Andorra |

</details>

<details>
<summary>country=123 (0x7B, GI) — Gibraltar</summary>

| state decimal | Hexadecimal | Subdivision |
| --- | --- | --- |
| 0 | 0x00 | Unset |
| 1 | 0x01 | Gibraltar |

</details>

<details>
<summary>country=124 (0x7C, GG) — Guernsey</summary>

| state decimal | Hexadecimal | Subdivision |
| --- | --- | --- |
| 0 | 0x00 | Unset |
| 1 | 0x01 | Guernsey |

</details>

<details>
<summary>country=125 (0x7D, IM) — Isle of Man</summary>

| state decimal | Hexadecimal | Subdivision |
| --- | --- | --- |
| 0 | 0x00 | Unset |
| 1 | 0x01 | Isle of Man |

</details>

<details>
<summary>country=126 (0x7E, JE) — Jersey</summary>

| state decimal | Hexadecimal | Subdivision |
| --- | --- | --- |
| 0 | 0x00 | Unset |
| 1 | 0x01 | Jersey |

</details>

<details>
<summary>country=127 (0x7F, MC) — Monaco</summary>

| state decimal | Hexadecimal | Subdivision |
| --- | --- | --- |
| 0 | 0x00 | Unset |
| 1 | 0x01 | Monaco |

</details>

<details>
<summary>country=128 (0x80, TW) — Taiwan</summary>

| state decimal | Hexadecimal | Subdivision |
| --- | --- | --- |
| 0 | 0x00 | Unset |
| 2 | 0x02 | Taipei City |
| 3 | 0x03 | Kaohsiung City |
| 4 | 0x04 | Keelung City |
| 5 | 0x05 | Hsinchu City |
| 6 | 0x06 | Taichung City |
| 7 | 0x07 | Chiayi City |
| 8 | 0x08 | Tainan City |
| 9 | 0x09 | New Taipei City |
| 10 | 0x0A | Taoyuan City |
| 11 | 0x0B | HsinChu County |
| 12 | 0x0C | Miaoli County |
| 14 | 0x0E | Changhua County |
| 15 | 0x0F | Nantou County |
| 16 | 0x10 | Yunlin County |
| 17 | 0x11 | Chiayi County |
| 20 | 0x14 | Pingtung County |
| 21 | 0x15 | Yilan County |
| 22 | 0x16 | Hualien County |
| 23 | 0x17 | Taitung County |
| 24 | 0x18 | Penghu County |
| 25 | 0x19 | Kinmen County |
| 26 | 0x1A | Lienchiang County |

</details>

<details>
<summary>country=136 (0x88, KR) — South Korea</summary>

| state decimal | Hexadecimal | Subdivision |
| --- | --- | --- |
| 0 | 0x00 | Unset |
| 2 | 0x02 | Seoul-teukbyeolsi |
| 3 | 0x03 | Busan-gwangyeoksi |
| 4 | 0x04 | Daegu-gwangyeoksi |
| 5 | 0x05 | Incheon-gwangyeoksi |
| 6 | 0x06 | Gwangju-gwangyeoksi |
| 7 | 0x07 | Daejeon-gwangyeoksi |
| 8 | 0x08 | Ulsan-gwangyeoksi |
| 9 | 0x09 | Gyeonggi-do |
| 10 | 0x0A | Gangwon-do |
| 11 | 0x0B | Chungcheongbuk-do |
| 12 | 0x0C | Chungcheongnam-do |
| 13 | 0x0D | Jeollabuk-do |
| 14 | 0x0E | Jeollanam-do |
| 15 | 0x0F | Gyeongsangbuk-do |
| 16 | 0x10 | Gyeongsangnam-do |
| 17 | 0x11 | Jeju-teukbyeoljachido |

</details>

<details>
<summary>country=144 (0x90, HK) — Hong Kong</summary>

| state decimal | Hexadecimal | Subdivision |
| --- | --- | --- |
| 0 | 0x00 | Unset |
| 1 | 0x01 | Hong Kong |

</details>

<details>
<summary>country=145 (0x91, MO) — Macao</summary>

Country ID has a single public-table source (the locale switcher), not a PKHeX listing; this is not a verified Pokemon transfer-support claim.

No separate published subdivision table was found; IDs are not invented. state=0 leaves it unset. Do not reuse another country's subdivision number.

</details>

<details>
<summary>country=152 (0x98, ID) — Indonesia</summary>

Country ID has a single public-table source (the locale switcher), not a PKHeX listing; this is not a verified Pokemon transfer-support claim.

No separate published subdivision table was found; IDs are not invented. state=0 leaves it unset. Do not reuse another country's subdivision number.

</details>

<details>
<summary>country=153 (0x99, SG) — Singapore</summary>

| state decimal | Hexadecimal | Subdivision |
| --- | --- | --- |
| 0 | 0x00 | Unset |
| 1 | 0x01 | Singapore |

</details>

<details>
<summary>country=154 (0x9A, TH) — Thailand</summary>

Country ID has a single public-table source (the locale switcher), not a PKHeX listing; this is not a verified Pokemon transfer-support claim.

No separate published subdivision table was found; IDs are not invented. state=0 leaves it unset. Do not reuse another country's subdivision number.

</details>

<details>
<summary>country=155 (0x9B, PH) — Philippines</summary>

Country ID has a single public-table source (the locale switcher), not a PKHeX listing; this is not a verified Pokemon transfer-support claim.

No separate published subdivision table was found; IDs are not invented. state=0 leaves it unset. Do not reuse another country's subdivision number.

</details>

<details>
<summary>country=156 (0x9C, MY) — Malaysia</summary>

| state decimal | Hexadecimal | Subdivision |
| --- | --- | --- |
| 0 | 0x00 | Unset |
| 2 | 0x02 | Kuala Lumpur |
| 3 | 0x03 | Johor |
| 4 | 0x04 | Kedah |
| 5 | 0x05 | Kelantan |
| 6 | 0x06 | Melaka |
| 7 | 0x07 | Negeri Sembilan |
| 8 | 0x08 | Pahang |
| 9 | 0x09 | Perak |
| 10 | 0x0A | Perlis |
| 11 | 0x0B | Penang |
| 12 | 0x0C | Sarawak |
| 13 | 0x0D | Selangor |
| 14 | 0x0E | Terengganu |
| 15 | 0x0F | Labuan |
| 16 | 0x10 | Sabah |
| 17 | 0x11 | Putrajaya |

</details>

<details>
<summary>country=160 (0xA0, CN) — China</summary>

| state decimal | Hexadecimal | Subdivision |
| --- | --- | --- |
| 0 | 0x00 | Unset |
| 2 | 0x02 | Beijing |
| 3 | 0x03 | Chongqing |
| 4 | 0x04 | Shanghai |
| 5 | 0x05 | Tianjin |
| 6 | 0x06 | Anhui |
| 7 | 0x07 | Fujian |
| 8 | 0x08 | Gansu |
| 9 | 0x09 | Guangdong |
| 10 | 0x0A | Guizhou |
| 11 | 0x0B | Hainan |
| 12 | 0x0C | Hebei |
| 13 | 0x0D | Heilongjiang |
| 14 | 0x0E | Henan |
| 15 | 0x0F | Hubei |
| 16 | 0x10 | Húnán |
| 17 | 0x11 | Jiangsu |
| 18 | 0x12 | Jiangxi |
| 19 | 0x13 | Jilin |
| 20 | 0x14 | Liaoning |
| 21 | 0x15 | Qinghai |
| 22 | 0x16 | Shanxi |
| 23 | 0x17 | Shandong |
| 24 | 0x18 | Shanxi |
| 25 | 0x19 | Sichuan |
| 26 | 0x1A | Yunnan |
| 27 | 0x1B | Zhejiang |
| 29 | 0x1D | Guangxi-Zhuangzu |
| 30 | 0x1E | Nei-Menggu |
| 31 | 0x1F | Ningxia-huizu |
| 32 | 0x20 | Xinjiang-Weiwu'er-zu |
| 33 | 0x21 | Xizang |

</details>

<details>
<summary>country=168 (0xA8, AE) — U.A.E.</summary>

| state decimal | Hexadecimal | Subdivision |
| --- | --- | --- |
| 0 | 0x00 | Unset |
| 2 | 0x02 | Abu Dhabi |
| 3 | 0x03 | Ajman |
| 4 | 0x04 | Ash Shariqah |
| 5 | 0x05 | Ras al-Khaimah |
| 6 | 0x06 | Dubai |
| 7 | 0x07 | Al Fujayrah |
| 8 | 0x08 | Umm al Qaywayn |

</details>

<details>
<summary>country=169 (0xA9, IN) — India</summary>

| state decimal | Hexadecimal | Subdivision |
| --- | --- | --- |
| 0 | 0x00 | Unset |
| 2 | 0x02 | Delhi |
| 3 | 0x03 | Andaman and Nicobar Islands |
| 4 | 0x04 | Andhra Pradesh |
| 5 | 0x05 | Assam |
| 6 | 0x06 | Chandīgarh |
| 7 | 0x07 | Dadra and Nagar Haveli |
| 8 | 0x08 | Gujarāt |
| 9 | 0x09 | Haryāna |
| 10 | 0x0A | Himachal Pradesh |
| 12 | 0x0C | Kerala |
| 13 | 0x0D | Lakshadweep |
| 14 | 0x0E | Mahārāshtra |
| 15 | 0x0F | Manipur |
| 16 | 0x10 | Meghālaya |
| 17 | 0x11 | Karnātaka |
| 18 | 0x12 | Nāgāland |
| 19 | 0x13 | Odisha |
| 20 | 0x14 | Puducherry |
| 21 | 0x15 | Punjab |
| 22 | 0x16 | Rājasthān |
| 23 | 0x17 | Tamil Nadu |
| 24 | 0x18 | Tripura |
| 25 | 0x19 | West Bengal |
| 26 | 0x1A | Sikkim |
| 28 | 0x1C | Mizoram |
| 29 | 0x1D | Daman and Diu |
| 30 | 0x1E | Goa |
| 31 | 0x1F | Bihār |
| 32 | 0x20 | Madhya Pradesh |
| 33 | 0x21 | Uttar Pradesh |
| 34 | 0x22 | Chhattīsgarh |
| 35 | 0x23 | Jharkhand |
| 36 | 0x24 | Uttarakhand |
| 37 | 0x25 | Other |

</details>

<details>
<summary>country=170 (0xAA, EG) — Egypt</summary>

Country ID has a single public-table source (the locale switcher), not a PKHeX listing; this is not a verified Pokemon transfer-support claim.

No separate published subdivision table was found; IDs are not invented. state=0 leaves it unset. Do not reuse another country's subdivision number.

</details>

<details>
<summary>country=171 (0xAB, OM) — Oman</summary>

Country ID has a single public-table source (the locale switcher), not a PKHeX listing; this is not a verified Pokemon transfer-support claim.

No separate published subdivision table was found; IDs are not invented. state=0 leaves it unset. Do not reuse another country's subdivision number.

</details>

<details>
<summary>country=172 (0xAC, QA) — Qatar</summary>

Country ID has a single public-table source (the locale switcher), not a PKHeX listing; this is not a verified Pokemon transfer-support claim.

No separate published subdivision table was found; IDs are not invented. state=0 leaves it unset. Do not reuse another country's subdivision number.

</details>

<details>
<summary>country=173 (0xAD, KW) — Kuwait</summary>

Country ID has a single public-table source (the locale switcher), not a PKHeX listing; this is not a verified Pokemon transfer-support claim.

No separate published subdivision table was found; IDs are not invented. state=0 leaves it unset. Do not reuse another country's subdivision number.

</details>

<details>
<summary>country=174 (0xAE, SA) — Saudi Arabia</summary>

| state decimal | Hexadecimal | Subdivision |
| --- | --- | --- |
| 0 | 0x00 | Unset |
| 2 | 0x02 | Ar Riyad |
| 3 | 0x03 | Al Bahah |
| 4 | 0x04 | Al Madinah |
| 5 | 0x05 | Ash Sharqiyah |
| 6 | 0x06 | Al Qasim |
| 7 | 0x07 | 'Asir |
| 8 | 0x08 | Ha'il |
| 9 | 0x09 | Makkah |
| 10 | 0x0A | Al Hudud ash Shamaliyah |
| 11 | 0x0B | Najran |
| 12 | 0x0C | Jizan |
| 13 | 0x0D | Tabuk |
| 14 | 0x0E | Al Jawf |

</details>

<details>
<summary>country=175 (0xAF, SY) — Syria</summary>

Country ID has a single public-table source (the locale switcher), not a PKHeX listing; this is not a verified Pokemon transfer-support claim.

No separate published subdivision table was found; IDs are not invented. state=0 leaves it unset. Do not reuse another country's subdivision number.

</details>

<details>
<summary>country=176 (0xB0, BH) — Bahrain</summary>

Country ID has a single public-table source (the locale switcher), not a PKHeX listing; this is not a verified Pokemon transfer-support claim.

No separate published subdivision table was found; IDs are not invented. state=0 leaves it unset. Do not reuse another country's subdivision number.

</details>

<details>
<summary>country=177 (0xB1, JO) — Jordan</summary>

Country ID has a single public-table source (the locale switcher), not a PKHeX listing; this is not a verified Pokemon transfer-support claim.

No separate published subdivision table was found; IDs are not invented. state=0 leaves it unset. Do not reuse another country's subdivision number.

</details>

<details>
<summary>country=184 (0xB8, SM) — San Marino</summary>

| state decimal | Hexadecimal | Subdivision |
| --- | --- | --- |
| 0 | 0x00 | Unset |
| 1 | 0x01 | San Marino |

</details>

<details>
<summary>country=185 (0xB9, VA) — Vatican City</summary>

| state decimal | Hexadecimal | Subdivision |
| --- | --- | --- |
| 0 | 0x00 | Unset |
| 1 | 0x01 | Vatican City |

</details>

<details>
<summary>country=186 (0xBA, BM) — Bermuda</summary>

| state decimal | Hexadecimal | Subdivision |
| --- | --- | --- |
| 0 | 0x00 | Unset |
| 1 | 0x01 | Bermuda |

</details>

### Sources and reliability

Console-region/system-language IDs and CFG interfaces are checked against [libctru](https://raw.githubusercontent.com/devkitPro/libctru/master/libctru/include/3ds/services/cfgu.h); four-part overrides and address-byte positions against [Luma IPC](https://raw.githubusercontent.com/LumaTeam/Luma3DS/master/k11_extension/source/ipc.c). Country ID/ISO mapping comes from [Reworked country.c](https://raw.githubusercontent.com/OasisAkari/LumaLocaleSwitcher_Reworked_zh_hans/433ec044911ef7f3a04ae8237676523a9ce7eba4/source/fbi/country.c); subdivisions from [states_data.c at the same revision](https://raw.githubusercontent.com/OasisAkari/LumaLocaleSwitcher_Reworked_zh_hans/433ec044911ef7f3a04ae8237676523a9ce7eba4/source/fbi/states_data.c).

Shared country IDs and all nonzero subdivision IDs are cross-checked with [PKHeX countries](https://raw.githubusercontent.com/kwsch/PKHeX/641c26ba100225f3605e5a44b61b3b16e15a3774/PKHeX.Core/Resources/text/locale3DS/countries.txt) and each `subregions/sr_NNN.txt` ([Japan example](https://raw.githubusercontent.com/kwsch/PKHeX/641c26ba100225f3605e5a44b61b3b16e15a3774/PKHeX.Core/Resources/text/locale3DS/subregions/sr_001.txt)). Numeric subdivision differences are limited to explicit zero/unset entries omitted by some switcher lists; those entries are retained. The 11 countries absent from PKHeX are identified individually. Their CFG IDs/ISO codes are cross-checked against Azahar; this does not establish a valid Pokemon transfer combination. Community locale data is not an official Nintendo support list; missing values are not inferred. Configuration does not force players to use only these named entries.
