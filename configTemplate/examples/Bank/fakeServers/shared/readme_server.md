# Shared server configuration

[简体中文](readme_server.zh-cn.md)

Three basic files may be supplied manually. Missing files are generated; malformed entries are repaired individually.

| Relative to sd:/3ds/Bank/ | Purpose |
| --- | --- |
| `client/client.conf` | User selection and registry; see the [client guide](../../client/readme_client.md). |
| `fakeServers/shared/server.conf` | Shared free/trial windows, annual prices and recharge amounts. |
| `fakeServers/users/<userID>/user.conf` | Personal console locale, currency, pass/trial records and balances; see the [personal guide](../users/readme_user.md). |

Balances share each user's user.conf with pass records. Players supply optional gift/Mover configuration, words and assets themselves; missing extensions remain disabled.

See the [shared layout guide](../../readme_fakeServer.md) for the complete Bank tree and the purposes of the copy-ready template and populated example.

## Reading and repair

Use UTF-8 `key=value;`, true/false switches and non-negative decimal amounts/counters. End assignments with a semicolon; headers, NGWord patterns and exact-name entries do not gain one. Basic configuration readers retain valid older unterminated values and publish canonical terminated assignments. Valid values survive. Missing, empty or malformed entries reset to built-in defaults and canonical section/order. The last valid duplicate wins; later invalid values do not erase valid ones. Shop components are repaired independently. Normalization removes unknown keys/sections and general comments; only the shop-format comment is generated.

New relevant requests read current files without changing delivered replies. User selection stays pinned within a session; leave and re-enter to switch users. Purchases/recharges do not change shared prices or recharge policy.

The three basic configurations verify staging, rotate the old primary to .bak and publish the new primary; unchanged content is not written. Missing primaries read backup before using defaults. Orphan .tmp files are not adopted. Valid current values survive per-entry repair; genuine I/O failure is not success or an empty Bank. See the [file-protection inventory](../../readme_fakeServer.md#files-written-and-protection-policies).

## Shared server.conf

Ticket policy belongs to [ticket], custom options to the final [custom]; see the complete [template](server.conf).

### Free and trial windows

free_enable=false prevents free_periods from granting access regardless of its dates. Default enabled windows are `2020-02-12..2020-03-12,2023-02-14..2050-12-31`: editable project presets, not a claim that every boundary is an official historical service date. Free access does not extend paid validity.

trial_enable=false disables trials and ignores claim windows. trial_periods specifies when a thirty-day trial may be claimed, not its validity after claiming. Additional conditions are described under trial_claimed in the [personal guide](../users/readme_user.md).

| Common / region | Default claim window |
| --- | --- |
| Common, JPN, KOR, TWN, HK | 2013-12-25..2014-02-21 |
| USA | 2014-02-05..2014-03-14 |
| EUR, AUS | 2014-02-04..2014-03-14 |

### Dates

Free, trial and paid periods use YYYY-MM-DD..YYYY-MM-DD, comma-separated ranges with inclusive endpoints, using console local calendar dates. 0000-00-00..0000-00-00 means no active interval. Switch words enable/disable/true/false are not dates.

Unsuffixed entries are common defaults. Region suffixes include .JPN/.USA/.EUR/.AUS/.CHN/.KOR/.TWN; implemented country suffixes such as .HK are more specific. Country overrides region, which overrides common. Being outside a selected override today does not fall back to common.

### Shops

`shop.$<currency>=<annual_price>,<recharge>`: two non-negative integers, no balance.

| Currency | Default price / recharge | Integer unit |
| --- | --- | --- |
| $NONE | 807,807 | Local unlabelled units |
| $JPY | 500,500 | Yen |
| $USD | 499,499 | Cent |
| $GBP | 449,449 | Penny |
| $EUR | 499,499 | Cent |
| $KRW | 5000,5000 | Won |
| $TWD | 150,150 | Taiwan dollar |
| $HKD | 4000,4000 | Cent |
| $AUD | 650,650 | Cent |
| $NZD | 850,850 | Cent |
| $CAD | 699,699 | Cent |
| $CHF | 700,700 | Rappen / centime |
| $ZAR | 6000,6000 | Cent |

For $NONE, 807 is a custom value representing the highest Pokedex number supported by Bank; both the default annual price and recharge amount are 807, while the personal balance starts at zero. Other currencies use project historical-price presets, without taxes or live exchange rates. USD 499 means $4.99. Currency selection belongs to [ticket] in the selected user's user.conf, immediately above the balances: autoCurrency=true derives currentCurrency from the user's effective region/country; false uses currentCurrency manually, even if it conflicts with the region. The UI language is not used. Manual NONE and unsupported automatic mappings use NONE policy and personal NONE balance. $NONE supports local purchases/recharges. Balances are not real eShop funds. Valid manually configured prices/recharge amounts, including zero, are retained; only missing or invalid components reset to defaults.

## Custom options

Place these project-specific options at the end of shared `server.conf`; both default off:

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

Before loading the selected source's `checkPokemon.conf`, this option actually writes
`enabled=0;` in an existing `[rule.BanUnicodeChinese]`. Missing/damaged enabled fields
become zero; duplicates merge. Other fields, expressions, comments and rules are retained.
Publication verifies `checkPokemon.tmp`, rotates the previous file to `checkPokemon.bak`,
then publishes staging. An already-disabled unchanged rule causes no write. Missing primary
may read published backup; orphan staging is not authority. Missing files/rules are not
invented. Turning permission off never silently re-enables the rule; edit it manually.

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

## Personal configuration

Personal pass/trial records and balances belong to each user's user.conf; see the [user.conf guide](../users/readme_user.md).

## Optional content

[Gifts](gifts/readme_gift.md), [NDS words](mover/readme_checkName.md#nds), [NDS checks](mover/readme_checkPokemon.md#nds-rules), [VC words](mover/readme_checkName.md#vc), [VC checks](mover/readme_checkPokemon.md#vc-rules). Missing/malformed optional policy does not authorize resetting Bank/game transactions; each guide explains skipped entries versus disabled features.
