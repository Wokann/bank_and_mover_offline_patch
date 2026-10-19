# fakeServer configuration, layout and data protection

[简体中文](readme_fakeServer.zh-cn.md) · [Client configuration](client/readme_client.md) · [Shared server configuration](fakeServers/shared/readme_server.md) · [Personal configuration](fakeServers/users/readme_user.md)

## Two distributions

- `configTemplate/templateForCopy/Bank/`: the copy-ready template for normal use. It retains `activeUser=auto`, an empty registry and no pre-created user directories.
- `configTemplate/examples/Bank/`: a complete independent copy with the same shared policies, gift assets, Mover word lists and guides, plus three fictional personal configurations; `activeUser=2` selects entry 2.
- Both distributions have identical README contents. Only client configuration and example personal data differ. Each uses its own relative paths, not files borrowed from the other copy.

For normal use, copy the empty template's Bank folder to `sd:/3ds/Bank/`. Do not merge populated example users onto a live SD card: active registry entries participate in selection, and any existing user subdirectory prevents first-user automatic migration.

## Fixed-user configuration example

```ini
[global]
activeUser=2;

[users]
1=nnid_ExampleNNID;
2=pnid_ExamplePNID;
3=localid_Example3DS;
```

All three entries use active syntax, without comment markers. `activeUser=2` means registry entry 2, not an ID suffix. Change it to 1, 3 or auto to demonstrate other selection modes. These examples use fictional account/console usernames, not Principal IDs or friend codes; see [selection](client/readme_client.md#selection).

| Fictional user ID | Example personal data |
| --- | --- |
| `nnid_ExampleNNID` | No pass or claimed trial; local JPY balance 1000. |
| `pnid_ExamplePNID` | One annual pass, example period 2026-01-01 through 2026-12-31; local USD balance 499 (4.99). |
| `localid_Example3DS` | Trial previously claimed, no current pass; unitless local balance 1614. |

These IDs and balances are not real accounts, authentication data, eShop funds or new-user defaults. Actual initialization still uses built-in pass/trial/balance defaults.

## Complete file tree

```text
sd:/3ds/Bank/
├── readme_fakeServer.md
├── readme_fakeServer.zh-cn.md
├── bankdata.bin
├── bankdata.bak
├── bankdata.tmp
├── sav.bin
├── sav.bak
├── sav.tmp
├── client/
│   ├── client.conf
│   ├── client.bak
│   ├── client.tmp
│   ├── log/
│   │   ├── bank.log
│   │   └── mover.log
│   ├── readme_client.md
│   ├── readme_client.zh-cn.md
│   └── users/
│       └── <userID>/
│           ├── sav.bin
│           ├── sav.bak
│           └── sav.tmp
└── fakeServers/
    ├── shared/
    │   ├── server.conf
    │   ├── server.bak
    │   ├── server.tmp
    │   ├── readme_server.md
    │   ├── readme_server.zh-cn.md
    │   ├── gifts/
    │   │   ├── gift.conf
    │   │   ├── readme_gift.md
    │   │   ├── readme_gift.zh-cn.md
    │   │   ├── 01_celebi/
    │   │   │   └── celebi.pl6
    │   │   ├── 02_johto_starters/
    │   │   │   └── johto_starters.pl6
    │   │   ├── 03_regi_trio/
    │   │   │   └── regi_trio.pl6
    │   │   ├── 04_mewnium_z/
    │   │   │   └── mewnium_z.wc7
    │   │   ├── 05_alola_starters/
    │   │   │   ├── decidueye.wc7
    │   │   │   ├── incineroar.wc7
    │   │   │   └── primarina.wc7
    │   │   └── 06_ha_apes/
    │   │       ├── oranguru.wc7
    │   │       └── passimian.wc7
    │   └── mover/
    │       ├── readme_checkPokemon.md
    │       ├── readme_checkPokemon.zh-cn.md
    │       ├── readme_checkName.md
    │       ├── readme_checkName.zh-cn.md
    │       ├── LICENSE.badword.txt
    │       ├── nds/
    │       │   ├── checkPokemon.conf
    │       │   └── checkName.conf
    │       └── vc/
    │           ├── checkPokemon.conf
    │           └── checkName.conf
    └── users/
        ├── readme_user.md
        ├── readme_user.zh-cn.md
        └── <userID>/
            ├── user.conf
            ├── user.bak
            ├── user.tmp
            ├── bankdata.bin
            ├── bankdata.bak
            ├── bankdata.tmp
            ├── transaction.bin
            ├── transaction.bak
            └── transaction.tmp
```

- This tree lists all current distributed policies, assets and guides, plus possible personal, temporary and migration files. They need not all exist at once.
- `<userID>` represents each independent user directory, with the same complete ID on both sides; do not create a literal placeholder-named directory.
- Normal initialization or migration creates `sav.bin`, `bankdata.bin` and journal files. The empty template contains no fabricated or zero-byte binary placeholders.
- `.tmp` denotes unpublished staging, not a file to create manually. Corresponding save/recovery logic determines whether `.bak` remains.
- Root `bankdata.bin`, `bankdata.bak`, `bankdata.tmp`, `sav.bin`, `sav.bak` and `sav.tmp` are legacy migration inputs; their old paths disappear after successful migration.
- `client.conf` owns user selection; personal locale belongs to `[console]` and currency selection belongs to `[ticket]` in `user.conf`. Shared policies/assets live in `fakeServers/shared/`. Personal pass records and `balance.$currency` share `[ticket]` in `user.conf`, separated by a blank line without a separate balance section.
- Stop the software and back up before changing personal data. Edit only the intended configuration fields; do not overwrite Banks, journals or game saves.

## Files written and protection policies

This inventory covers local runtime data directly maintained or redirected by Bank/Mover and the patch, not build output, PC-tool exports, or emulator-owned caches/logs. Paths are relative to `sd:/3ds/Bank/`; backup and temporary names are siblings of their primary.

| Primary file / object | Backup | Temporary | Publication and retention |
| --- | --- | --- | --- |
| `client/log/bank.log`, `client/log/mover.log` | None | None | Recorded only with shared server.conf [debug] enablelog=true;, off by default. Enabled launches overwrite that title's log, not on title returns or user changes. Disabled logging records nothing and leaves old files untouched. No account names, tokens or file bodies are recorded; sink failure does not alter business results. |
| `client/client.conf` | `client.bak` | `client.tmp` | Common protection; user selection/registry; retain the pre-write version. |
| `fakeServers/shared/server.conf` | `server.bak` | `server.tmp` | Common protection; shared schedules, prices and recharge amounts; retain the pre-write version. |
| `fakeServers/users/<userID>/user.conf` | `user.bak` | `user.tmp` | Common protection; pass/trial records and balances publish together; retain the pre-write version. |
| `client/users/<userID>/sav.bin` | `sav.bak` | `sav.tmp` | Dedicated transaction protection; includes identity/transaction fields; retain the pre-write version, not ordinary configuration fallback. |
| `fakeServers/users/<userID>/bankdata.bin` | `bankdata.bak` | `bankdata.tmp` | Dedicated Bank transaction protection; retain the version before an actual change; recovery requires transaction/body evidence. |
| `fakeServers/users/<userID>/transaction.bin` | `transaction.bak` | `transaction.tmp` | Dedicated journal protection; retain the previous published decision; verify record CRC32, format, sequence and transaction phase. |
| `fakeServers/shared/mover/<nds-or-vc>/checkPokemon.conf` | None | None | Player-supplied read-only rules; account fan permission overrides the named rule only in RAM. |
| The selected NDS game's `x.sav`, outside the Bank directory | `x.bak` | `x.tmp` | Source/token-bound transaction protection; retire after saving and token-cleanup saving are acknowledged; preserve on interruption. |
| Bank's console-save archive; Mover directly accesses its `/turtle` | No additional SD backup | No additional SD staging | Bank retains its original save flow; Mover updates only language fields, commits, reopens and verifies the whole record. |
| Selected cartridge, 3DS digital and VC game saves | Managed by the original save system | Managed by the original save system | Keep original checks, saving and transaction handling; no common configuration fallback or invented physical paths. |
| Six legacy root-level `bankdata`/`sav` files, individually listed in the tree | Move under original names | Move under original names | First migration only: same-SD rename, no overwrite; successful source paths disappear, failed sources remain. |

Common publication for basic configurations and the designated rule field is: render new content → write/flush/close staging → read back and verify → rotate old primary to backup → publish staging as primary. Identical content causes no write or rotation. First creation without an old primary does not fabricate a backup. The published image is never copied back over the pre-write backup.

An existing primary supplies current text and per-entry repair; a malformed entry does not roll back the whole file. A missing primary reads backup before parsing and rebuilding it. Only when neither exists are built-in defaults used; an orphan temporary is not committed authority. I/O errors are not absence and never silently zero data. A read-only startup can use a backup view without publishing or removing files. Defaults cannot recreate lost balances or purchase history without a recoverable source.

`transaction.bak` is a previous journal decision, not a Bank-body backup. It is read only if the main journal is missing; an invalid/unreadable main does not silently fall back, and a temporary journal is never authority. Common file protection does not decide Bank/source/journal commit or rollback: their dedicated transaction layer still checks evidence. Retiring backups requires explicit acknowledgement of the whole business operation, not merely a successful write callback.

### Strictly read-only patch inputs

checkPokemon has only the designated write exception above, not a general rule editor.

- `fakeServers/shared/gifts/gift.conf`
- `fakeServers/shared/mover/nds/checkName.conf`
- `fakeServers/shared/mover/vc/checkName.conf`
- Gift PL6/WC6/WC7 assets, NDS ROMs, README files and licenses.

Players maintain these inputs manually. The patch has no editor for them and never creates, restores or removes their backup/temporary siblings. A missing optional primary means unconfigured, not permission to reactivate an old backup. Guides and licenses are not generated by the patch.

Rotation protects interrupted file replacement, not arbitrary controller/filesystem power-loss atomicity. Keep independent backups of important data.
