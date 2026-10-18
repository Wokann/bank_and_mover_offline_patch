# Client and user selection

[简体中文](readme_client.zh-cn.md)

Use sd:/3ds/Bank/client/client.conf. Missing configuration is generated; valid manually supplied values are used. The example below shows first-use defaults; prefilled registries are read as configured.

```ini
[global]
activeUser=auto;

[users]
```

The initial `[users]` section is empty; software registers users after actual creation or identification. Manual entries use `number=userID`, not colons; a leading `#` denotes a comment. Shared `server.conf` does not store the registry or personal balances.

## Selection

- activeUser=number selects that registry entry without automatic console-account selection; for example, activeUser=2 selects entry 2. See the [shared guide](../readme_fakeServer.md) for a complete fixed-user example.
- auto reads FRD's environment and ACT's account username, checking linked local account slots. Standard Nintendo maps to nnid_<username>, Pretendo/Nimbus to pnid_<username>. Principal IDs, friend codes and Mii nicknames are not used for naming.
- Missing/unimplemented account APIs, empty accounts, errors, unknown environments or contradictory results fall back to CFG's console profile username: localid_<console username>. This is the 3DS system-settings name emulated by the emulator, not the host Windows username.
- If the console name is unavailable too, reuse the localid with the **lowest registry number**, not name or line order. Only when none exists is a zero-padded twelve-digit random suffix generated. Collisions with the registry or either directory tree retry; persistent RNG failures fail safely without overwriting files or waiting forever.
- Matching name-based directories are reused. nnid_, pnid_ and localid_ remain separate namespaces; ASCII case variants do not register the same SD directory twice. Spaces, control characters and path/configuration separators in console names become underscores, as does a trailing period. UTF-8 names such as Chinese access the SD through UTF-16 paths.
- Valid existing directories on either side are registered. Automatically read account names require 6–16 ASCII letters, digits, underscores, hyphens or internal periods; localid also permits valid non-ASCII characters. Generated names allow sixteen UTF-16 units; the current CFG console name allows ten. Thirty-two character slots cover prefix/name/NUL, with 64 UTF-8 bytes of storage: a multibyte character is not one byte. Existing longer/manual IDs retain the 63-byte total limit and are not truncated or forcibly renamed. Registry storage grows dynamically.
- Invalid selected numbers repair to auto. First valid binding wins for duplicate numbers; duplicate IDs create no extra entry. Selection stays pinned during a session, never redirecting an in-progress save.

This is local account information, not login/authentication, and cannot identify every third-party redirect. Hardware, HLE and LLE results use the same checks; enabling LLE alone is not proof of an available account. Console usernames can change or collide. Renaming may select another directory under auto; use a fixed number to retain old data. Existing numeric-ID directories remain manually selectable and are never automatically renamed, merged or deleted.

## Title selection

Bank displays a dark-gray user block at the upper left in Offline and Download Modes; Mover displays it in Offline Mode only. Multiple users also show the switch hint. For example:

```text
Current user: auto(2) (↑/↓ to switch)
2=localid_Example3DS
```

The parentheses show auto's resolved registry number; fixed selection shows its number instead. The second line is that entry's exact mapping. With multiple users, D-pad or Circle Pad down/up selects forward/backward in numeric order; the C-stick is ignored. The first item is auto and the last is the highest-numbered user. Selection stops at either end without wrapping; gaps in numbering are allowed. A tap moves one item. Holding repeats every 0.15 seconds after a 0.5-second delay, accelerating to every 0.06 seconds after two seconds held. Same-direction inputs merge; opposing directions do not switch. Pending background saves never accumulate a catch-up burst, and holding at an end does not repeatedly read or write files. A single user still shows both information lines, without the switch hint or up/down switching. Bank Unlock/Original, Mover Online or an empty registry hide the whole block.

Bank Offline selects the local Bank source; Download selects the local destination, not the online console account. Bank Unlock/Original and Mover Online hide the selector and do not verify an unrelated local choice at entry. An outstanding selection save still finishes first.

Entering the title or switching into one of these selectable modes reads client.conf, scans both user-directory trees and caches the registry and fixed messages in memory; switching between Bank Offline and Download refreshes too. Browsing updates immediately, without per-item directory scans, message loads or SD writes. Releasing directional input for about 0.15 seconds coalesces changes into a commit of the final selection. Browsing remains available during a save; older replies never overwrite newer displayed selections. Returning to the already saved choice requires no extra write.

Background commits re-read only client.conf, preserving the current user registry, and retain temporary-file verification and backup rotation. Entering a user-selectable mode via START/touch waits for the current selection to be verified, successfully committed and its response released. Switching to a mode without local accounts still saves an outstanding choice first. If a number was externally remapped to another ID, the binding is not overwritten and entry under another user is not permitted. Actual I/O failures retain the desired selection and retry after a half-second backoff, never treating failure as successful persistence. Session data is rebound instead of reusing the previous user's server cache.

Users registered during startup appear in the first title catalogue. Entries manually added while staying on the title join the next refresh; switch to another mode and back to Offline/Download to refresh. Read-only title queries create no directories and migrate no files. Mover still requires Bank to initialize the selected profile and Bank data first.

## Currency

Currency and console locale belong to each account's user.conf, not client.conf. Currency defaults to auto and follows that user's effective region/country; explicit currency is independent. See the [user guide and complete numeric reference](../fakeServers/users/readme_user.md).

## Files and migration

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
    │       │   ├── checkPokemon.bak
    │       │   ├── checkPokemon.tmp
    │       │   └── checkName.conf
    │       └── vc/
    │           ├── checkPokemon.conf
    │           ├── checkPokemon.bak
    │           ├── checkPokemon.tmp
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

The tree includes shipped files and possible runtime/temporary files; it is not a request to create them. `<userID>` represents each independent user. Root bankdata and sav files appear only before legacy migration. See the [shared layout guide](../readme_fakeServer.md) for details.

Bank/Mover share one registry and selected user's data. Three basic configurations are generated automatically. Players supply optional gift/Mover policy, words and assets.

Use Bank first to initialize the offline user and create or migrate its Bank. Offline Mover neither creates a Bank nor migrates legacy files. If the selected user's directory is absent, it reads shared policy/defaults in memory without creating directories or rewriting configuration, preserving Bank's first automatic migration. A missing Bank body follows the native download-failure exit, not the server-locked message, and transfer saving does not continue. Established profiles retain normal configuration repair and Bank backup recovery.

Automatic v0.9.1 migration requires **neither side to have any user directory**. Root bankdata.bin/.bak/.tmp move directly into the first selected user's server directory; sav.bin/.bak/.tmp move into that user's client directory. Same-SD renames remove old paths on success, without leaving another copy. Any existing file among the six targets prevents the entire migration; nothing is overwritten. Old transactions, passes and balances are not imported. Once any user directory exists, remaining root files require manual migration. Partial failure retains moved files at their destinations and unmoved originals; do not delete remaining sources indiscriminately.

For manual moves/renames, stop both applications and back up; adjust both client/users and fakeServers/users directory names plus registry IDs together. Fixed selection uses the number. Auto derives NNID/PNID from actual accounts, so renaming cannot change console identity; use fixed selection for custom IDs. Do not publish private Bank/settings/ID/journal data.

## References and limitations

- [libctru FRD](https://github.com/devkitPro/libctru/blob/master/libctru/source/services/frd.c): environment/local account reads and SDK version.
- [libctru ACT](https://github.com/devkitPro/libctru/blob/master/libctru/include/3ds/services/act.h): username field 8 differs from Principal ID field 12; this patch uses the former and the linked Friends slot.
- [Azahar CFG](https://github.com/azahar-emu/azahar/blob/master/src/core/hle/service/cfg/cfg.cpp): console profile username block and ten-UTF-16-unit limit.
- [Nimbus initialization](https://github.com/PretendoNetwork/nimbus/blob/main/app/source/main.cpp), [environment definitions](https://github.com/PretendoNetwork/nimbus/blob/main/app/source/common.hpp): standard environment mapping.
- [Azahar ACT HLE](https://github.com/azahar-emu/azahar/blob/master/src/core/hle/service/act/act.cpp): successful stubs may leave output untouched; status alone is insufficient.

These are open-source implementation references, not official guarantees for this patch. Other redirected environments may require localid.

## Configuration backup

client.bak retains the pre-write registry image; client.tmp is staging.
A missing primary reads backup before defaults; temporary data never chooses a
user. Valid manual values survive, and unchanged content is not written. See the
[personal configuration guide](../fakeServers/users/readme_user.md) and [complete protection policies](../readme_fakeServer.md#files-written-and-protection-policies).
