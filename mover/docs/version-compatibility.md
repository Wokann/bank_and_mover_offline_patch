# Mover v1.2 conversion compatibility

[简体中文](version-compatibility.zh-cn.md) · [Configuration](../../configTemplate/templateForCopy/Bank/fakeServers/shared/readme_server.md#enablemoverv12)

Shared `[custom] enableMoverv1.2=false;` defaults off. Enabling it restores verified
offline RGBY conversion differences, not an old executable or the entire service.
Gen2, Gen5 and online retain v1.5.

## Differences restored

| Item | Enabled | Default v1.5 |
| --- | --- | --- |
| Mixed-gender species | Keep native constructor RNG using species ratio | Apply Attack-DV correction after construction; fixed/genderless remain native |
| Shiny | Attack DV 10; Defense 2/3/6/7/10/11/14/15; Speed/Special 10 | Defense/Speed/Special 10; Attack one of those eight values |
| Japanese/Western text | Old byte tables and language handling, including period→hyphen, apostrophe→space and unsupported digits/diacritics | Current tables and added language-specific handling |

Both shiny predicates accept eight of 65536 DV combinations; only one overlaps.
This changes which individuals qualify, not overall probability. No new RNG or 50/50
gender policy is introduced. Legacy text conversion precedes native nickname comparison;
it is not a global post-save punctuation replacement.

## Values that remain unchanged

The option changes the three behaviors listed above. Other v1.5 conversion and saving behavior remains in effect.

| Item | Finding / policy |
| --- | --- |
| Nature/experience | Both callers use experience modulo 25 for nature, then reset experience for level; no additional switch |
| Guaranteed IVs | Three perfect stats, five for Mew, use matching construction parameters; retain the current constructor |
| PID/random values | Native construction remains; no additional caller-level formula requiring rollback identified; cross-version/seed byte equality is not promised |
| Ability slot | Matching RGBY Hidden Ability/exception caller policy; no Gen5 ability rollback |
| TID/SID | Matching source-TID byte-order conversion and zero SID construction parameters |
| Locale/language | Keep current source and explicitly configured personal locale; no user.conf bypass |
| Mew | Native fateful, guaranteed-IV and default-name branches remain; no historical server acceptance replay |
| PK7/date/checksum/transactions | Keep current objects, native encryption/checks, callbacks and saving; no old object-layout copy |

Explicit fan-translation permission restores Chinese after conversion. NGWord and
Pokémon screening remain in force; this switch does not make arbitrary specimens eligible.

## References

- [Bulbapedia: Generation I and II conversion](https://bulbapedia.bulbagarden.net/wiki/Pok%C3%A9_Transporter#From_Generation_I_and_II) describes the historical gender, shiny and text-conversion changes. It is community documentation, not an official complete algorithm.
- The compatibility option wraps RGBY converter `0x0024F428`; current native objects, save format and transactions remain in use.
- [Server configuration](../../configTemplate/templateForCopy/Bank/fakeServers/shared/readme_server.md#enablemoverv12) describes activation and interactions with fan-translation support.
