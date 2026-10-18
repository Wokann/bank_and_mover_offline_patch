# Pokemon Bank patch

[简体中文](README.zh-cn.md) · [Project](../README.md) · [Native flow and implementation](docs/code-analysis.md)

Targets base title `00040000000C9B00`. Install `code.bps + exheader.bin + romfs/`
together. Original section and BSS variable addresses remain unchanged.

## Modes

| Title mode | Purpose | Bank/settings source |
| --- | --- | --- |
| Offline | Local boxes, native mileage and optional local gifts | SD `bankdata.bin` and `sav.bin` |
| Download | Export the complete official Bank to SD, then return to title | Official Bank and stock save; back up the local Bank before overwriting |
| Unlock | Native online recovery with a returned candidate displayed | Official server and stock save, never the offline Bank |
| Original | Complete native menus and online Bank flow | Official server and stock save |

Press **R** at the title; A/START/touch latches the session mode. Returning to
title retains it; a fresh launch defaults to Offline. There is no upload feature
for offline modifications.

Press **L** on the title to choose a language; the original Mover hint is above HOME.
Both L and R use native button glyphs; the L hint's wording is otherwise unchanged.
The current mode is centered above START, with R-mode, L-language and HOME hints
below it. Text uses native white, native size and integer pixel coordinates,
without obscuring START or copyright.
There is no language switch inside a mode. All modes use the stock console
save's language; an uninitialized console save follows native language selection
and initialization before the title. A new choice is saved natively first,
then synchronized to SD before returning to title.
On startup and after language saving, a missing `sav.bin` receives an exact copy
of the valid native record. An existing file only changes `0x28–0x2B` when needed,
preserving identities, transactions and opaque tail bytes. Corruption/read errors
are not absence. Failed SD synchronization does not undo the console save and is
retried next launch. SD settings never select the interface language.

Unavailable entries in the first three modes retain their full native label and
append a localized disabled suffix, such as “Move to Pokémon HOME (Disabled)”.
All ten text sets fit the native 54-UTF-16-unit pane capacity and use native horizontal
fitting for wider labels. Original Mode keeps its native labels and actions.
Support ID is available in every mode through the native console-identity path,
not a fabricated ID or local transaction token. A zero identity from a console/
emulator is not replaced, and its display does not establish an official account.
Failed/incomplete native replies or placeholder identity values append a localized
“(Invalid)” to the still-clickable menu entry; valid replies keep its native label.
The observer neither repeats identity reads nor connects to a server.

Offline and Download Modes add a dark-gray upper-left title block showing
`auto(resolved number)`/the fixed selection and `number=userID` beneath it.
A single user omits the switch hint. With multiple users, D-pad/Circle Pad ↑/↓
selects auto followed by actual numbers without wrapping; C-stick is ignored.
Holding repeats and accelerates. Browsing uses a memory snapshot, with one
coalesced background save after release. Entry verifies the final activeUser
before loading that user's data.
Offline selects the local Bank source; Download selects its local export
destination, not the console's online account. Unlock and Original hide the
selector and do not verify an unrelated local selection at entry. Leaving a
local mode still finishes any outstanding selection save.
Automatic naming uses ACT account usernames or CFG console usernames, not PIDs;
see [client selection](../configTemplate/templateForCopy/Bank/client/readme_client.md).

An Offline transaction mismatch preserves the native game/trainer/ID notice,
then adds one native-confirmed three-line page: forced unlocking may lose the
previous Bank transfer. Only after checking that no save matches the local
server should the user select a game and hold L+A+START while confirming.
Unlock Mode's game-selection notice carries the same risk warning but refers
to the official server. Matching, the forced-unlock entry and native cleanup
are unchanged.

Campaign requests read current date/region policy independently of ticket jobs.
The original callback validates their bodies, and native entitlement/menu logic
prefers an active free campaign while preserving independent purchased-pass records.

Downloading needs no selected game; a direct-download prompt is available when
none is usable. A new official account first follows native Bank creation.
Offline can initialize a missing local Bank, but not overwrite damage or
ambiguous pending work as if it were empty. Language, game validation, box
editing, mileage and native game saves stay client-owned.

Mileage uses current time from the ticket response: real networking uses the
returned server time; the local service formats console time as that response.
Changing console time therefore affects offline calculation, not a real server's clock.

## Cross-language glyphs

All three main fonts cover default Bank/group/box names, species 1–807 in ten
language resources, and native keyboard characters. Native artwork is reused or
copied without resizing, changing saves, altering old glyph indices/metrics or
increasing texture memory. This is not full CJK coverage for arbitrary translations;
see [coverage, memory and checks](../common/docs/fonts.md).

## Installation

Follow the [project instructions](../README.md#installation) for
Luma. Azahar reads `load/mods/00040000000C9B00/` in its user directory, not
`sdmc/luma/titles/`. Update all three patch components together and remove old
`code.ips/code.bin`. Do not put README, changelog, configuration or saves in the
Title ID patch directory.

## Local transaction journal

| File | Role |
| --- | --- |
| `fakeServers/users/<userID>/bankdata.bin` | Current complete Bank body |
| `bankdata.bak` | Previous version before an actual change; not mirrored to the new main at completion |
| `bankdata.tmp` | Staging/publication body, not independent recovery authority |
| `client/users/<userID>/sav.bin/.bak/.tmp` | Local Bank settings and native transaction record |
| `fakeServers/users/<userID>/transaction.bin/.bak/.tmp` | Per-user coordination journal, previous publication step and staging; not Bank-body backups |

Saving stages the Bank, follows native game-save callbacks for commit/rollback,
then clears the token and confirms cleanup. Read-only entry/no-save exit do not
rotate the Bank backup. Gen5 files follow [Mover's policy](../mover/README.md#saving-and-recovery).
File fingerprints use CRC32; IDs/tokens remain random. CRC32 is not cryptographic authentication.

Do not infer interrupted decisions from names or timestamps. Preserve ambiguous
work and consult [file combinations and risks](../common/docs/fakeServer/architecture.zh-cn.md#删除损坏和手动换入的处理边界).
Primary, backup-only and orphan-temp cases have different rules. Old SHA-256
journals are not silently cleared. Automatic recovery stops when the available
files cannot establish a consistent version; keep the complete user directory
and source-game saves rather than forcing an unrelated backup into place.

Offline challenges return three local candidates through native keyboard and
rollback handling, not the official algorithm or Unlock Mode's online backend.
Bank/settings files may contain private identifiers, Trainer information and
timestamps; do not publish them.

## Configuration

Templates and guides are separate from the patch, in `configTemplate/templateForCopy/Bank/`:

- [client.conf](../configTemplate/templateForCopy/Bank/client/readme_client.md): user selection and migration.
- [server.conf / user.conf](../configTemplate/templateForCopy/Bank/fakeServers/shared/readme_server.md): shared windows/prices and personal purchase records/balances.
- [gift.conf](../configTemplate/templateForCopy/Bank/fakeServers/shared/gifts/readme_gift.md): global switch, dates and PL6/WC inputs.
- [Local service](../common/README.md): dynamic policy, native responses and transactions.

New reads use current text without freezing the whole policy or reevaluating
delivered replies/staged decisions. Gifts use native receipt fields without
an extra ledger. Missing/invalid optional assets
offer no content; genuine I/O errors are not reported as success.

## Injection-space provenance and bounds

New code/workspace occupies independent pages after the original BSS, with a
1 MiB I/O buffer, separate request body and worker stack. Native SDK objects
and body allocations retain their original owners. Workers never borrow game
objects, and the UI only consumes completed results. Dynamic policy memory is
released instead of retaining unrelated word lists in the native heap.

See [code analysis](docs/code-analysis.md) for layouts, hooks, permissions,
capability routing and preserved instructions. Unsupported hardware permission
changes remain failures.

## Independent build

Requires GNU Make, a POSIX shell, Python 3 and devkitARM with `DEVKITARM` set.
Windows armips/Floating IPS are included; other platforms set `ARMIPS` and
`IPS_TOOL`.

Extract decompressed/decrypted code, full RomFS and ExHeader from the same title you own:

```text
bank/rom/
├── exheader.bin
├── exefs/00040000000C9B00.dec.code
└── romfs/
```

Code size: `2,801,664` bytes; SHA-1: `5AB630856835DCF2DBDF9A62244DD19E46AE1C7C`.
Prefer the full `0x800`-byte `extheader.bin` from GodMode9, renamed `exheader.bin`.
The tool also recognizes `0x400`-byte header inputs; never mix titles or updates.
Do not commit or redistribute extracted resources.

```sh
make -C bank
# Override tool paths when needed:
make -C bank ARMIPS=/path/to/armips IPS_TOOL=/path/to/flips
```

Output: `release/00040000000C9B00/`. Builds verify BPS reconstruction, native
hooks/modes, ExHeader and all ten language archives. A preceding `clean` is
normally unnecessary; it deletes this title's build/release outputs. Do not
store saves or unique data in generated directories.

## Usage limits

Back up Bank and game saves before first use. Coverage of every source, gift
combination and actual power-loss scenario is not guaranteed. See the
[shared service guide](../common/README.md) for technical boundaries.

Only a positively identified emulator missing the ticket interface, with
global compatibility enabled, uses the independent read-only free response.
It does not import offline purchase history, balances or gifts; other online
requests stay native. This does not guarantee official-server acceptance.
Local code must not invent official support-code, HOME or authorization success.
