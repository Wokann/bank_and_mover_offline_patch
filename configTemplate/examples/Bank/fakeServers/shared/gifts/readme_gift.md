# Gift configuration

[简体中文](readme_gift.zh-cn.md) · [Server configuration](../readme_server.md)

Edit [gift.conf](gift.conf) at `sd:/3ds/Bank/fakeServers/shared/gifts/gift.conf`.
Players edit this configuration manually; the patch only reads it and never writes staging or backups. This README is not read by the game.
Use UTF-8. Start with `[global]` and `enableGift=true/false`. A false switch closes
the file early without parsing gift sections or opening assets. Missing/malformed
optional gift data adds no available gift, rather than terminating the game.
An already delivered response is not withdrawn by a later edit. Configuration
editing/publication validates the complete file, including disabled sections.

## Gift sections

```ini
[global]
enableGift=true;

[gift.my_event]
enabled=1;
id=60000;
generation=7;
games=SM,USUM;
periods=2030-01-01..2030-12-31;
folder=gifts/my_event;
files=first.wc7,second.wc7;
```

Supply those two files; this example is not an additional bundled event.
Section names accept 1–31 lowercase ASCII letters, digits or underscores.
Names and folders are customizable; a numeric prefix is not required.

| Setting | Meaning |
| --- | --- |
| `enabled=0/1` | Disable/enable this gift, subject to the global switch. |
| `id` | Optional 16-bit Bank response content identifier, 1–65535; required for PL6. |
| `generation=6/7` | Must match the input format and recipient generation. |
| `games` | `X/Y/OR/AS/S/M/US/UM`; aliases `XY/ORAS/SM/USUM`, comma-separated. |
| `periods` | Inclusive availability ranges. |
| `exclude` | Optional exclusion ranges, taking precedence over availability. |
| `file` | A single PL6, WC, Full WC or compatible Bank-response `.bin` file. |
| `folder` + `files` | Alternative to `file`: up to six WC cards, comma-separated in delivery order. |

Paths are relative to `fakeServers/`. Absolute paths and parent traversal are
not accepted. Do not combine `file` with `folder/files`. PL6 and response `.bin`
use `file`, not the WC card list. Current capacity is sixteen gifts and
thirty-two combined date rules; a batch accepts at most six cards.

## Dates and regions

Use `YYYY-MM-DD..YYYY-MM-DD`, with commas for multiple inclusive intervals and
the console's local calendar date. Both `periods` and `exclude` accept console
regions `.JPN/.USA/.EUR/.AUS/.CHN/.KOR/.TWN` and supported two-letter countries
such as `.HK`. Country overrides region, then common. An override replaces,
not extends, the common ranges; being outside it does not restore common dates.
`periods.JPN=disable` disables that region. Remove the override to use common dates.

```ini
periods=2030-01-01..2030-03-31,2030-07-01..2030-09-30;
periods.JPN=2030-02-01..2030-03-31;
exclude=2030-02-10..2030-02-12;
```

## Formats, identifiers and receiving

Native PL6 is `0xA47` bytes. Normal WC6/WC7 is `0x108`; Full is `0x310`, using
`.wc6/.wc7/.wc6full/.wc7full` as appropriate. WC6 lists are packed as one Link
batch; native PL6 and WC batches are then wrapped into the original Bank response.
Changing `generation` does not convert WC7 into WC6. Pokémon/item cards are
supported; unsupported Bean/BP or extra template fields are not silently discarded.

`id` is the response's content/batch identifier, not a transaction dataId/token
or the final gift-card number stored by the game. Prefixes do not determine it.
Use a distinct value for a distinct custom batch. Do not simultaneously enable
equivalent WC6 and PL6 copies of the same content.

Receiving, duplicate handling and saving use the original client flow. There
is no extra server claim ledger. If
the stock receiver allows a repeat claim after its native records change,
this service does not add another restriction. Changing a date does not itself
clear the original client's claim state.

## Bundled gifts and sources

All six are enabled by default. Their availability still depends on dates,
games and the original receiver. Three PL6 files and six normal WC7 files are supplied.

| Section / folder | ID | Input | Common dates; regional override |
| --- | --- | --- | --- |
| `01_celebi` | 1 | `celebi.pl6` | 2013-12-25..2014-09-30; USA starts 2014-02-05, EUR/AUS 2014-02-04 |
| `02_johto_starters` | 2 | `johto_starters.pl6` | 2015-02-27..2015-11-30 |
| `03_regi_trio` | 3 | `regi_trio.pl6` | 2016-03-04..2016-10-31 |
| `04_mewnium_z` | 4 | `mewnium_z.wc7` | 2017-01-25..2017-10-02 |
| `05_alola_starters` | 5 | `decidueye.wc7,incineroar.wc7,primarina.wc7` | 2018-03-09..2018-10-31 |
| `06_ha_apes` | 6 | `oranguru.wc7,passimian.wc7` | 2019-03-28..2019-10-31; JPN starts 2019-03-29 |

Assets come from [Project Pokémon EventsGallery, pinned revision][gallery].
Generation six uses the archived native PL6 files unchanged. Generation seven
uses the embedded normal WC7 at offset `0x208` of the corresponding Full cards
in `Unreleased/Gen 7/Bank/`; templates are not rewritten into invented WC6 cards.
These are community-preserved input assets, not complete official network responses.
IDs 1/2 are local inferences from release order, not confirmed original Bank
batch IDs; 3 is supported by archived `present_0`/Full data, 4/5/6 by WC7 inputs.

Dates use [Serebii's Bank event archive][dates], cross-referenced with official
announcements for [Celebi][celebi], [Mewnium Z][mewnium], [Alola starters][alola]
and [the Japanese ape gifts][apes]. The ape common start date relies on the
secondary archive; the Japanese override has an official announcement.

Suspended downloads do not prove that every installed user was unable to claim.
The [Japanese reopening notice][reopen-jp] and [Hong Kong/Taiwan notice][reopen-hk]
describe download availability, not a confirmed daily gift blackout. No such
blackout is enabled by default; add `exclude` only for outages you intend to model.
Format cross-checks: [PL6][pl6], [WC6][wc6], [WC7][wc7], [WC7 Full][wc7full].

[gallery]: https://github.com/projectpokemon/EventsGallery/tree/154d81be88453f6f78ec1d6d86e85fe0f2f5c240
[dates]: https://www.serebii.net/bank/events.shtml
[celebi]: https://www.nintendo.com/hk/pressrelease/3ds_20140127_pokemonbank.html
[mewnium]: https://www.pokemon.co.jp/ex/pokemonbank/topics/170125_01.html
[alola]: https://www.pokemon.co.jp/ex/pokemonbank/topics/180309_01.html
[apes]: https://www.pokemon.co.jp/ex/pokemonbank/topics/190329_01.html
[reopen-jp]: https://support.nintendo.com/jp/information/2014/0122.html
[reopen-hk]: https://www.nintendo.com/hk/pressrelease/3ds_20140122_pokemonbank.html
[pl6]: https://raw.githubusercontent.com/kwsch/PKHeX/master/PKHeX.Core/MysteryGifts/PL6.cs
[wc6]: https://raw.githubusercontent.com/kwsch/PKHeX/master/PKHeX.Core/MysteryGifts/WC6.cs
[wc7]: https://raw.githubusercontent.com/kwsch/PKHeX/master/PKHeX.Core/MysteryGifts/WC7.cs
[wc7full]: https://raw.githubusercontent.com/kwsch/PKHeX/master/PKHeX.Core/MysteryGifts/WC7Full.cs

Players maintain this file and gift assets manually. The patch treats them as strictly read-only: no gift.bak/gift.tmp creation, cleanup or recovery of removed policies.
