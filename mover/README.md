# Poke Mover patch

[简体中文](README.zh-cn.md) · [Project](../README.md) · [Native flow and implementation](docs/code-analysis.md)

Targets base title `00040000000C9C00`. The same `code.bps + exheader.bin + romfs/`
set serves Luma and Azahar. Default behavior retains native validation, conversion,
subsequent local checks, messages and game saves. Explicit default-off conversion
options are described in the [custom settings](../configTemplate/templateForCopy/Bank/fakeServers/shared/readme_server.md#custom-options)
and [v1.2 compatibility](docs/version-compatibility.md).

## Modes and prerequisites

Fresh launches default to Offline; press **R** to switch Online. A/START/touch
confirms the session mode. Returning to title retains it; restarting resets it.

The current mode (Offline/Online) is centered above START, with R-mode,
L-language and HOME hints below. Text uses native white, native size and integer
pixel coordinates, without obscuring START or copyright. L and R use native
button glyphs; the HOME wording is unchanged in all ten language resources.

Bank's native save is the language authority. Confirmation closes the selection
view first; while the native return animation runs, the worker updates only Bank's
two language fields, commits/closes/reopens and verifies that record, then
synchronizes `sav.bin`. Existing SD records receive language-only changes; a
missing SD record is copied from the validated native record. Missing/corrupt
native saves are never created/formatted and cannot authorize SD writes. Waiting
and result release yield each frame without borrowing the destroyed language view.
Mover adds only Bank to its save whitelist, not a new native save of its own;
Bank still owns first initialization.
The native return-to-title flow has no added language-save success/failure notice;
native error handling is unchanged.

- Offline writes to the Transport Box in `sd:/3ds/Bank/fakeServers/users/<userID>/bankdata.bin`.
- Online uses the official Bank, never substitutes the offline body for it.
- Mover does not initialize a Bank or export the official Bank to a local file. Missing offline Bank data follows the native download-failure exit; use Bank to create or migrate it first. Mover does not create user directories that would prevent first migration. Occupied Transport Boxes retain native handling.
- Both modes share Gen5 digital compatibility and the local file backend for the selected digital `.sav`.

[Installation](../README.md#installation): copy the complete title
directory for Luma. Azahar uses `load/mods/00040000000C9C00/` in its user directory,
not the virtual SD's Luma path. Update the full set and remove stale
`code.ips/code.bin`; no README, changelog or saves belong in the patch directory.

## Cross-language glyphs

All three main fonts cover default Bank/group/box names, species 1–807 in ten
language resources, and native keyboard characters. Native artwork is reused or
copied without changing NDS/VC name conversion, save encoding, native size/widths
or texture memory. This is not full CJK coverage for arbitrary translations;
see [coverage, memory and checks](../common/docs/fonts.md).

## Gen5 digital compatibility

```text
sd:/roms/nds/
├── game.nds
└── saves/
    └── game.sav
```

Only Black, White, Black 2 and White 2 are accepted. ROM/save basenames must
match; the save must be readable/writable and pass native validation. Each game
gets at most one source: valid cartridges take priority, then digital language
rank `J > O > F > I > D > S > K`; keep the first valid copy at the best rank.

SD Gen5 sources can be used without an inserted NDS cartridge or a VC save.
An unavailable SD ROM directory does not hide otherwise usable cartridge or VC sources.

Do not combine this with an external Transporter Redirect Patch. VC reads the
corresponding VC game's save. The NDS paths above apply to Gen5 digital
sources; name and Pokémon filters are applied after a game is selected.

## Words and Pokémon filters

NDS and VC have independent `checkName.conf`, `checkPokemon.conf` and initial `[global]`
switches. A disabled file skips later content. Missing/invalid optional policy
does not become a reject-everything rule.

- NDS: [names](../configTemplate/templateForCopy/Bank/fakeServers/shared/mover/readme_checkName.md#nds), [filter syntax/evidence](../configTemplate/templateForCopy/Bank/fakeServers/shared/mover/readme_checkPokemon.md#nds-rules).
- VC: [names](../configTemplate/templateForCopy/Bank/fakeServers/shared/mover/readme_checkName.md#vc), [filter syntax/evidence](../configTemplate/templateForCopy/Bank/fakeServers/shared/mover/readme_checkPokemon.md#vc-rules).

Words use NGWord syntax; species names, regional exceptions and licensing are
documented in the guides. Conditions support cross-field `&&`/`||`, parentheses,
multiline text and semicolon termination, with enums listed in the guide.
There is no separate hardcoded species-only blocklist.

One box request loads only its source policy, computes numeric results on the
worker and releases it; no per-slot SD rereads. Invalid word entries are skipped
individually. Allocation/real I/O failure is not fabricated success. Native code
applies name corrections. The first-use name warning is distinct from an
individual match; see [analysis](docs/code-analysis.md).

## Saving and recovery

Each user's `fakeServers/users/<userID>/transaction.bin` coordinates the current Bank/settings/source
operation, not a history of every game or a full Bank/save/configuration copy.
Bank/settings and balances are isolated by the shared client user registry;
see [user selection and migration](../configTemplate/templateForCopy/Bank/client/readme_client.md).
Offline Mode shows the dark-gray upper-left selection and `number=userID`.
A single user omits the switch hint. With multiple users, D-pad/Circle Pad ↑/↓
selects auto followed by actual numbers, stopping at either end; C-stick is ignored.
Holding repeats and accelerates in memory; release coalesces a background save,
and entry verifies the final activeUser. Title queries never provision users or
preempt legacy migration.
Digital sources bind the complete UTF-16 path and game code; the native
descriptor retains ID, versions, length and token.

| File | Policy |
| --- | --- |
| `bankdata.bin` | Current Bank |
| `bankdata.bak` | Previous version before a real change, not mirrored at completion |
| `bankdata.tmp` | Staging, published only by the transaction decision |
| `game.sav` | Selected digital source's published save |
| Sibling `game.bak / game.tmp` | Recovery snapshot/work image beside the selected `game.sav` |

After confirmed native saving, commit and token cleanup, only `.sav` remains.
Interrupted work retains recovery files. Successful paired rollback also retires
them; Bank-only rollback preserves evidence when the source was not restored.
Read-only/no-transfer exits do not rotate the Bank backup. Do not discard
ambiguous pending work; see [file combinations/import risks](../common/docs/fakeServer/architecture.zh-cn.md#删除损坏和手动换入的处理边界).

Cartridge/VC saving belongs to the native SDK. File backups do not establish
native cross-save atomicity. CRC32 supplies local consistency checks, not
cryptographic authentication; IDs/tokens remain random and native checks stay.

## Worker and online boundaries

Tickets, service I/O and box policies can run on the worker. SDK allocation,
body delivery, parsing and callbacks remain in native client waits; workers
never borrow game/SDK pointers. Framed copying adds no SD rereads. Release
contention waits for another frame; canceled/stale results cannot become success.

Native catalog helpers remain. Unsupported ticket operations fail immediately
rather than queueing a result behind their synchronous wait. Only an identified
emulator missing the ticket API, with global compatibility enabled, uses the
independent free response. Offline shops/gifts are not imported into Online;
other network paths remain native. See [analysis](docs/code-analysis.md) and
[shared services](../common/README.md).

## Independent build

Requires GNU Make, a POSIX shell, Python 3 and devkitARM with `DEVKITARM` set.
Extract these inputs from the same title you own:

```text
mover/rom/
├── exheader.bin
├── exefs/00040000000C9C00.dec.code
└── romfs/
```

Code size: `2,269,184` bytes; SHA-1: `583859C1E874D11650EFBDDE51F470ECF96900C4`.
Prefer GodMode9's full `0x800`-byte `extheader.bin` renamed `exheader.bin`;
`0x400`-byte headers are also recognized. Never mix titles/updates or redistribute
original resources.

```sh
make -C mover
# Override tool paths when needed:
make -C mover ARMIPS=/path/to/armips IPS_TOOL=/path/to/flips
```

The default label combines the root `VERSION` with the Git short revision. Release mode requires explicit `PATCH_RELEASE=1`. It appears at the upper screen's top left; the official version is unchanged. See [shared build instructions](../README.md#building-from-source) for version updates and builds without Git metadata.

Output: `release/00040000000C9C00/`. Builds preserve startup initialization,
place expansion after original BSS, and check BPS, ExHeader, mode branches and
all ten language archives. A preceding `clean` is unnecessary; it deletes this
title's build/release outputs; do not store saves or unique data there.

## Usage limits

Back up the source save and Bank before first use. Every source, real SD power
loss and official-server policy are not guaranteed. See the [shared service
guide](../common/README.md) for technical boundaries.
HOME, uploading the offline Bank and official authentication algorithms are not
provided by the local service.
