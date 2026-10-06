# Combined offline, download, and unlock patch

See [`../docs/code-analysis.md`](../docs/code-analysis.md) for the stock
program's memory map, state table, network paths, BankObject, and bankdata
layout. This document covers only the maintained patch design, implementation,
and independent build procedure.

This is the maintained Pokemon Bank v1.5 patch for base title
`00040000000C9B00`. It incorporates the useful behavior and safety checks of
the former standalone download and offline patches into one runtime-selectable
build. It does not concatenate their IPS files: every shared network, data,
message, and save hook dispatches according to a session mode.

The upload patch has been permanently discontinued. This project imports
server data in Download Mode and uses a separate local copy in Offline Mode; it
never uploads offline changes to the official service. Unlock Mode enters the
stock online transaction-recovery flow but never substitutes local Bankdata for
the server Bank.

## Modes and recommended workflow

The title screen defaults to **Offline Mode**. Each press of the physical **R**
button advances through **Offline Mode**, **Download Mode**, and **Unlock
Mode**. The line below the stock HOME-menu help updates immediately. Press
**A**, **START**, or touch the lower screen to latch the displayed mode for the
whole session; R no longer changes it after leaving the title screen.

```text
Title screen (default: Offline)
        │
        ├── R ───────────────► Download
        ├── R again ─────────► Unlock
        ├── R again ─────────► Offline
        └── A / START / touch
                  │ latch displayed mode
                  ├── Download ─► official server download ─► local file
                  ├── Unlock  ─► stock forced-unlock path
                  └── Offline  ─► local file ─► normal Bank use
```

Recommended first use:

1. Back up the SD card and any existing `sd:/3ds/Bank/` files.
2. Select Download Mode, enter the first feature-menu item, select any
   compatible game, and let the official service return the complete Bank
   object.
3. Wait for the download-complete message and the return to the title screen.
4. Confirm that `sd:/3ds/Bank/bankdata.bin` exists, then use Offline Mode for
   normal Bank sessions.

Offline Mode can also create a new local Bank file through the stock first-use
initializer when no recoverable local file exists. Downloading first is still
preferred when an existing official Bank account must be preserved locally.

## Local files and invariants

Offline and Download modes use the same checked Bankdata-file implementation.
All three modes share the redirected Turtle-record backend described below.

| File | Purpose |
|---|---|
| `sd:/3ds/Bank/bankdata.bin` | Current complete Bank file (`0xBB518` bytes) |
| `sd:/3ds/Bank/bankdata.tmp` | Fully written staging file used before commit |
| `sd:/3ds/Bank/bankdata.bak` | Previous complete file retained during commit |
| `sd:/3ds/Bank/bankdata.bin.break` | Byte-for-byte preservation copy of an unusable primary file |
| `sd:/3ds/Bank/bankdata.bak.break` | Byte-for-byte preservation copy of an unusable backup file |
| `sd:/3ds/Bank/sav.bin` | Complete redirected local-record image (`0x200` bytes) |
| `sd:/3ds/Bank/sav.tmp` | Complete staging image used before replacing the local record |

The patch creates `sd:/3ds` and `sd:/3ds/Bank` when needed. Complete writes
check file creation/opening, size setting, service results, flush behavior, and
the actual byte count. A short or failed write is never accepted as a complete
Bank file.

`bankdata.tmp` is intentional: it prevents a failed write from replacing the
last known primary file. `bankdata.bak` is the previous committed generation,
not a second live Bank. `.break` files are only preservation copies made when
an existing invalid file cannot participate in recovery.

### Local-record redirection

All three patch modes redirect the persistence backend of the stock logical
`data:/turtle` record to `sd:/3ds/Bank/sav.bin`. They neither replace nor
directly parse the outer `00000001.sav`. Stock object clearing, language setup,
transaction-field updates, checksum generation, validation, the loaded flag,
and the upper-level initialization state machine remain in control. Only the
storage backend used to check, load, format, and save the record is replaced.

```text
Inspect sav.bin
    ├── present and exactly 0x200 bytes ─► load object ─► stock validator
    ├── present but wrong-sized or invalid ─► stock first-use initialization
    │                                         └── write sav.bin
    └── missing
          ├── complete sav.tmp ─► promote to sav.bin
          └── no recoverable tmp ─► read-only attempt of stock data:/turtle
                                      ├── valid ─► migrate to sav.bin
                                      └── missing or invalid
                                            └── stock first-use initialization
                                                  └── write sav.bin
```

First-use initialization still lets the stock flow clear the object, apply the
selected language, and invoke its normal save wrapper. Redirected formatting
only resets `sav.bin` and `sav.tmp`; it does not fake object initialization or
format the stock save archive. Once `sav.bin` is available, all later
local-record reads and writes in all three modes use it exclusively. An SD write
failure is returned as a save failure and never falls back to modifying the
stock save.

`sav.bin` is a fixed `0x200`-byte logical-record image. The stock serialized
object occupies `0x30` bytes, of which the first `0x2C` bytes are currently
identified fields. See
[`../include/turtle_redirect.h`](../include/turtle_redirect.h) for the declaration.
Because the record can be migrated again from the stock save or rebuilt by the
first-use flow, it uses only `sav.tmp` to prevent short writes and does not
create `.bak` or `.break` files.

## Download Mode

Download Mode retains the stock startup, account initialization, first-user
server record creation, game detection, game selection, transaction recovery,
network connection, Bank-data request, and disconnect jobs.

```text
Feature menu: Download Bank Data
        │
        ▼
Stock game detection and game selection
        │
        ▼
Stock server connection and complete Bank download
        │
        ▼
Ordinary-Bank remote-success callback receives 0xBB518 bytes
        │
        ▼
Write and flush bankdata.tmp; verify size and actual bytes written
        ├── failure ─► leave the stock callback result unchanged
        └── success
              │
              ▼
Rotate old bin to bak; rename tmp to bin
        ├── failure ─► preserve or restore the previous file where possible
        └── success ─► mark the local capture complete
                            │
                            ▼
Skip mileage, Box, and save UI
        │
        ▼
Stock no-save transaction release and disconnect
        │
        ▼
Download-complete message; return to title
```

The capture occurs only after the complete object has been returned; network
chunks are not written incrementally. A second mode check at the success
callback permits local writing only for ordinary Bank mode. Other internal
routes cannot overwrite `bankdata.bin`.

The game-selection prompt explains that any selectable game can be used to
obtain the complete Bank data. After a successful local commit, the flow
bypasses local mileage, Bank Box, and save screens and uses the stock no-save
exit. If local capture fails, the patch does not falsify the official callback
result or silently mark the capture successful.

Download Mode preserves the native state 15 entitlement, ticket, campaign, and
online-gift checks by default, including their waits, messages, and error
handling. The one-byte switch below enables a local bypass for emulator tests;
it does not change the post-download mileage, Box, and save-screen skip.

## Unlock Mode

Unlock Mode is a separate entry into the stock online transaction-recovery
flow. It does not inherit Download Mode's Bankdata capture, post-download state
skip, or download-complete message.

```text
Title screen: Unlock Mode
        │
        ▼
Feature menu: Enter Force Unlock
        │
        ▼
Select the relevant game while holding L + A + START
        │
        ▼
Stock forced-unlock challenge state
        ├── no server candidate ─► retain the stock two-line prompt
        └── candidate available ─► append its first accepted eight-digit form
                                      as “Enter this unlock code: xxxxxxxx”
        │
        ▼
Stock input validation, rollback request, result handling, and continuation
```

The server response owns a vector of candidate values. The stock validator
accepts an entered number when it equals any vector element modulo
`100,000,000`. The patch displays the first element using exactly that rule and
lets the existing message system add leading zeroes. It does not invent a code,
skip validation, force a success result, or replace the subsequent server
operation. If the vector is empty, the original prompt is used unchanged.

This mode still shares project-wide menu restrictions and the redirected Turtle
record. It preserves native state 15 ticket and online-gift checks by default,
and its recovery state remains the stock state machine. The local offline
`bankdata.bin` is neither loaded nor uploaded by the unlock display hook.

## Ticket and online-gift checks: fixed test switch

Change this value at the top of [`main.s`](main.s), then rebuild:

```asm
.definelabel ONLINE_TICKET_CHECK_BYPASS, 0
```

| Session mode | `0`: default native checks | `1`: emulator test bypass |
| --- | --- | --- |
| Offline | Supply a local 999-day entitlement at the state 15 job interfaces without requesting online campaign data | Same as left |
| Download | Native state 15 ticket, campaign, and online-gift checks | Use the same local state 15 result as Offline Mode |
| Unlock | Native state 15 ticket, campaign, and online-gift checks | Use the same local state 15 result as Offline Mode |

At the time of this ticket-routing commit, official Azahar does not implement
`am:net`'s `GetRightsOnlyTicketData` (command `0x0821`; see the
[interface registration](https://github.com/azahar-emu/azahar/blob/6280521bf26ef1393974cd8c898de29cf75f5752/src/core/hle/service/am/am_net.cpp#L115)).
The two ticket configurations therefore remain available: `0` runs native checks;
`1` bypasses native ticket queries and supplies local results for emulators
missing this interface. The author's Azahar
[`pokebank` test branch](https://github.com/Wokann/azahar/tree/pokebank)
implements the interface, which the author has verified in online testing.
For that fork or real hardware, the value can remain fixed at `0`; no ticket
bypass is needed. Offline Mode always uses local results regardless of this value.

That Azahar feature implementation was generated entirely by AI and does not meet the
[Azahar AI Use Policy](https://github.com/azahar-emu/azahar/blob/master/AI-POLICY.md)
requirements for accepting contributions written entirely or substantially by AI.
It is therefore kept only in the personal test branch and will not be submitted
to Azahar's official main branch.

Both paths remain in the same code. This definition changes only the policy
byte at `[0x002A7E04, 0x002A7E05)`. State 15 retains its original entry and
state transitions. Only three calls are redirected: job initialization at
`0x002B0444`, result polling at `0x002B0464`, and unbinding at `0x002B1994`.
With `0`, Download and Unlock modes call the native job interfaces. Offline
Mode, or online modes with `1`, use local job results. Native allocation,
construction, result checks, field copying, UI cleanup, and destruction remain
in place. Local unbinding succeeds without a network call because no client
was bound.

Local results contain the current console date and an expiry exactly 999 days
later. Native getters calculate remaining days, hours, and validity. The
current date itself is not advanced and remains the local mileage reference.
Purchase counts stay `-1` (unknown), so no ticket-purchase reward is invented.
Existing EC account fields are preserved; there is no free-campaign window or
system eShop applet/loading-animation simulation. The completed job uses the
same status as native verification with at least 15 entitlement days. Clock
failure or an expiry beyond the native year range follows native error handling.

This switch controls only state 15. It does not skip NNID login, other network
requests, or server-side validation, and it does not change the post-download
exit, unlock transaction handling, or existing state 12/13 mode dispatch. It is
for emulator compatibility; hardware and emulators implementing this interface
use `0`. Native check errors retain their original handling rather than being
reported as successful networking.

## Offline Mode: startup, recovery, and first use

Offline Mode forces only the required network-facing state results and does
not allocate the corresponding remote jobs. It first classifies the local
files:

```text
Inspect bankdata.bin length and format header at offset 0x15C
        ├── valid ─────────────────────────────► existing-record mode
        └── missing or invalid
                  │
                  ▼
             Inspect bankdata.bak
                  ├── valid ─► read and validate all 0xBB518 bytes
                  │            copy to bankdata.bin; retain bak
                  │            create no .break file
                  │            └───────────────► existing-record mode
                  └── missing or invalid
                            │
                            ▼
            Copy each existing invalid file to its .break path
            (initial attempt plus up to three retries per file)
                            │
                            └──────────────────► stock first-use mode
```

A valid primary or recoverable backup never creates a `.break` file. Missing
files do not create placeholder `.break` files. If preservation copying fails
four times, the invalid original remains untouched and first-use creation is
still allowed to continue. If both files are absent, first use starts directly.

The stock first-use decision and messages remain in control. The patch skips
only the remote record-ID substates, then lets the native empty-object
initializer obtain the available console/account values, create 100 localized
Bank Boxes, and set the real creation date. The remote creation call is
replaced by a checked direct write of the complete new `bankdata.bin`; a failed
first write removes its partial output.

The runtime offline entitlement expires at the console time plus 999 days.
That value is separate from mileage time. Existing identity fields and the
creation date stored in a valid local file are not rewritten.

## Offline Mode: loading and Bank use

```text
Feature menu: Use Pokemon Bank
        │
        ▼
Stock game-software checks and game selection
        │
        ▼
Local Bank-data connection message (at least 2 seconds)
        │
        ▼
Read and validate exactly 0xBB518 bytes from bankdata.bin
        ├── failure ─► original error path
        └── success
              │
              ▼
Resume native metadata copy and selected-game callbacks
        │
        ▼
Native local mileage states and reward UI when applicable
        │
        ▼
Stock Bank Box and game interaction
```

The full file is loaded only after a game has been selected. The patch bypasses
the remote request portion of the shared synchronization state, then resumes
its native local substates. Box display, Pokemon movement, validation, and
game interaction remain stock code.

The stock local mileage calculation is retained. Offline Mode skips the two
online-gift lookups for that state instance without clearing the persistent
update-gift flag, so a later official session can still use it. The separate
remote entitlement/campaign state is completed locally.

The wait screen, spinner, and rhythmic sound remain owned by the original
state initializer, update, and exit path. The combined patch reports local
completion through the original state object instead of introducing a second
UI/sound owner; native state exit performs the cleanup before the Box flow.

## Offline save, commit, and rollback

```text
Stock full Bank serialization (0xBB518 bytes)
        │
        ▼
Write bankdata.tmp + set size + flush + verify bytes written
        ├── failure ─► report save failure; keep bankdata.bin
        └── success
              │
              ▼
Keep the save-progress screen visible for at least 2 seconds
        │
        ▼
Stock cartridge / digital-game save
        ├── failure ─► delete tmp; retain bin and bak
        └── success
              │
              ▼
Delete old bak ─► rename bin to bak ─► rename tmp to bin
        ├── success ─► normal local disconnect
        └── failure ─► restore bak where possible
```

The local staging write is synchronous, followed by a nonblocking state-machine
delay; rendering continues and the file is not written repeatedly. Commit is
allowed only after the original game-save result succeeds. Delete, rename,
close, size, write-length, restoration, and rollback results are checked.
Ending Bank use without saving does not install a new `bankdata.bin`.

## Shared menu, messages, and language handling

| Feature-menu entry | Offline Mode | Download Mode | Unlock Mode |
|---|---|---|---|
| First entry | Use Pokemon Bank | Download Bank Data | Enter Force Unlock |
| About Pokemon Bank | Stock information screen | Stock information screen | Stock information screen |
| Support | Disabled; return to feature menu | Disabled; return to feature menu | Disabled; return to feature menu |
| Poke Mover/eShop | Disabled; return to feature menu | Disabled; return to feature menu | Disabled; return to feature menu |
| Pokemon HOME | Stock language-selection flow | Stock language-selection flow | Stock language-selection flow |
| Back | Stock behavior | Stock behavior | Stock behavior |

The former HOME entry is redirected before any HOME networking or Box state is
created. It opens the stock language selector. Before returning to the title,
the patch copies the pending language and Japanese Kanji setting into the
persistent settings object, starts native local-save mode 0, and waits for it
to finish. The language therefore survives an immediate restart. A blank
disconnect entry prevents a newly selected font from rendering stale text in
the previous language.

Stock network, Bank-connection, save, and disconnect entries remain unchanged.
Mode-specific text is appended and selected only by the state that owns it.
Title hints, menu descriptions, game-selection guidance, local connection/save
messages, and download completion are provided for all ten shipped language
archives: Japanese kana, Japanese kanji, English, French, Italian, German,
Spanish, Korean, Simplified Chinese, and Traditional Chinese.

## Source layout

| File | Role |
|---|---|
| `main.s` | Original-address hooks, mode dispatch, small trampolines, state routing, and placement of imported C objects |
| `bankdata_redirect.c` | Checked Bank-data validation, recovery, loading, and local transaction implementation |
| `fs_helpers.c` | Shared checked SD-filesystem implementation used by Bankdata and Turtle backends |
| `offline_flow.c` | Local connection, disconnection, and save-display state updates |
| `local_mileage.c` | Converts console time into the packed date consumed by the stock Poké Mile states; it does not replace the native point calculation |
| `local_ticket.c` | Supplies local ticket results at the native entitlement state's job interfaces without implementing Poké Mile calculation |
| `unlock_mode.c` | Selects and normalizes the first server-returned unlock candidate for the stock challenge-code UI |
| `patch_paths.c` | Path constants placed in the verified tail-code region |
| `turtle_redirect.c` | Redirected Turtle-record backend placed in the verified HOME-code region |
| `../include/bankdata_redirect.h` | Confirmed serialized Bankdata layout, partial native Bank views, redirection constants, and stock entry points |
| `../include/fs_helpers.h` | Shared filesystem types, SDK entry points, and checked SD-helper declarations |
| `../include/local_mileage.h` | Local mileage-date input declaration |
| `../include/local_ticket.h` | Ticket-job and shared-data views, entitlement constants, and local job interfaces |
| `../include/offline_flow.h` | Stock timer entry points used by local flow states |
| `../include/patch_types.h` | Fixed-width primitive types shared by the injected C objects |
| `../include/patch_paths.h` | Path declarations shared across the separately placed objects |
| `../include/system_time.h` | Shared-memory system-time structure and addresses |
| `../include/turtle_redirect.h` | Confirmed logical-record layout, backend result contract, stock entry points, and exported backend declarations |
| `../include/unlock_mode.h` | Unlock-candidate display helper declaration |
| `patch_messages.py` | Rebuilds and validates the ten localized LayeredFS archives |
| `message_archive.py` | Self-contained GARC and encrypted message-file codec |
| `verify_patch.py` | Verifies base hash, code ranges, hooks, IPS reconstruction, native instruction replay, and resources |
| `Makefile` | Compiles, injects, creates the IPS, rebuilds messages, and writes the Luma release tree |

### Injection-space provenance and bounds

All addresses below are runtime virtual addresses, not IPS file offsets. Ranges
use `[start, end)`: the start is included and the end is excluded. Layout
definitions are in [`main.s`](main.s) and
[`symbol.inc`](../include/symbol.inc); actual placements after a build are in
`bank/build/armips-symbols.txt`.

The original executable has the following segment bounds. This patch neither
enlarges the ExHeader segment sizes nor moves the following segments.

| Original segment | Start | Declared size | Declared content end | End of 4 KiB page mapping | Permissions |
| --- | --- | --- | --- | --- | --- |
| `.text` | `0x00100000` | `0x00213910` | `0x00313910` | `0x00314000` | Read, execute |
| `.rodata` | `0x00314000` | `0x00055370` | `0x00369370` | `0x0036A000` | Read-only, non-executable |
| `.data` | `0x0036A000` | `0x00041ACC` | `0x003ABACC` | `0x003AC000` | Read/write, non-executable |

The patch uses in-place edits, reclaimed disabled functions, and existing
last-page padding. These are distinct sources of space; reclaimed functions
were not unused gaps in the original image.

| Address range | Provenance and original purpose | Current use and boundary constraints |
| --- | --- | --- |
| Scattered in-place hooks in `main.s` | Instructions at original function entries, call sites, or branches | Change jumps, conditions, or a few instructions; wrappers replay overwritten instructions where necessary. A hook does not make the whole original function reclaimable. |
| `[0x00285BA8, 0x0028711C)` | Reclaimed disabled code: input, update, construction, and cleanup functions of the dedicated HOME box-selection UI; original span `0x1574` (5492 bytes) | Holds six C objects for FS, Bankdata, offline flow, local mileage, local entitlement, and unlock display, followed by ticket-job wrappers. Ordinary box and shared functions are excluded. `0x0028711C` starts an adjacent normal UI function and must not be overwritten. |
| `[0x002A7BF0, 0x002A8404)` | Reclaimed disabled code: state 27 HOME operation control and its creation, initialization, and cleanup functions | Assembly dispatch, wrappers, trampolines, and the one-byte ticket switch end at `0x002A82E4`. The Turtle backend starts there and continues into the reclaimed region in the next row. |
| `[0x002A8404, 0x002A8760)` | Reclaimed disabled code: state 14 eShop launch UI and companion functions | Holds the rest of the Turtle backend. These two reclaimed regions total `0xB70` (2928 bytes) and are fully occupied. `0x002A8760` is the native transaction-recovery entry, which remains intact and must not be overwritten. |
| Four bytes each at `0x002B0444`, `0x002B0464`, and `0x002B1994` | In-place edits: state 15 job initialization, result polling, and exit unbinding calls | Three `BL` calls enter mode wrappers. Offline Mode always uses local results; the fixed test byte selects native or local jobs for Download and Unlock modes. |
| `[0x002B0270, 0x002B1AD0)`, excluding those three calls | Preserved: original state 15 body and companion functions | All other bytes match the original image. Every mode retains native state transitions, result copying, and cleanup; this is not payload space. The native ticket-job functions are also unchanged. |
| `[0x00313910, 0x00313A40)` | Existing padding in the last `.text` page | Reserves `0x130` (304 bytes) for Luma LayeredFS; this patch does not write here. `0x00313A40` is this project's chosen separation boundary, not a fixed Luma address. |
| `[0x00313A40, 0x00313FC0)` | Existing padding in the last `.text` page; span `0x580` (1408 bytes) | Holds tail assembly and `patch_paths.o`, ending at `0x00313E2B`. The remaining padding is unused by this project. |
| `[0x00313FC0, 0x00314000)` | Existing padding in the last `.text` page | A 64-byte version-identifier slot containing `offline_patch_v1.0.0` with zero padding; it does not currently affect runtime behavior. It must not cross the `.rodata` start at `0x00314000`. |
| `[0x003ABFFC, 0x003AC000)` | Verified final four spare bytes in the original `.data` page | Fixed storage for the session mode, download-captured flag, previous title R-key state, and selected title mode. This is not dynamically allocated heap memory and contains no executable code. |

All three modes block both the menu HOME entry and the direct HOME entry offered
when no usable game save exists. The eShop download entry is also disabled in
every mode. Only functions exclusive to these disabled paths are reclaimed;
shared SDK functions, ordinary box functions, and other screens remain intact.

Current C-object and tail-area placements are listed below, including alignment
padding between objects.

| Content | Actual range | Size |
| --- | --- | --- |
| `fs_helpers.o` | `[0x00285BA8, 0x00286150)` | `0x5A8` (1448 bytes) |
| `bankdata_redirect.o` | `[0x00286150, 0x002867AC)` | `0x65C` (1628 bytes) |
| `offline_flow.o` | `[0x002867AC, 0x002868CC)` | `0x120` (288 bytes) |
| `local_mileage.o` | `[0x002868CC, 0x00286CF0)` | `0x424` (1060 bytes) |
| `local_ticket.o` | `[0x00286CF0, 0x00286EB8)` | `0x1C8` (456 bytes) |
| `unlock_mode.o` | `[0x00286EB8, 0x00286F08)` | `0x50` (80 bytes) |
| Ticket-job assembly wrappers and literal pool | `[0x00286F08, 0x00286F90)` | `0x88` (136 bytes) |
| Assembly wrappers, trampolines, and ticket switch in reclaimed code | `[0x002A7BF0, 0x002A82E4)` | `0x6F4` (1780 bytes) |
| `turtle_redirect.o` | `[0x002A82E4, 0x002A8760)` | `0x47C` (1148 bytes) |
| `.text` tail assembly | `[0x00313A40, 0x00313D78)` | `0x338` (824 bytes) |
| `patch_paths.o` | `[0x00313D78, 0x00313E2B)` | `0xB3` (179 bytes) |

The six C objects occupy `0x1360` (4960 bytes); the additional 136-byte ticket-job
wrappers bring the total to `0x13E8` (5096 bytes), leaving
`[0x00286F90, 0x0028711C)`, or `0x18C` (396 bytes). This remainder still
contains disabled HOME functions, not natural zero padding. The tail area leaves
`[0x00313E2B, 0x00313FC0)`, or `0x195` (405 bytes). For 4-byte-aligned ARM
code, the usable start is `0x00313E2C`, leaving `0x194` (404 bytes).

Only `0x6F0` (1776 bytes) separate the declared `.text` content end from its
mapped end. This is unused space in the last mapped page, not an additional
allocated page. Luma installs LayeredFS after applying IPS and does not
automatically avoid IPS-written ranges, so this project separately reserves the
304 bytes above. With this layout, LayeredFS path strings use the 39 bytes at
`[0x00369370, 0x00369397)` in the `.rodata` tail, separately from the executable
payload in `.text`; this patch does not occupy that range. See the
[Luma loader](https://github.com/LumaTeam/Luma3DS/blob/master/sysmodules/loader/source/loader.c)
and [LayeredFS installation logic](https://github.com/LumaTeam/Luma3DS/blob/master/sysmodules/loader/source/patcher.c).
If the Luma implementation or original segment layout changes, recheck the
payload size and placement rather than assuming this separation boundary is
valid for every version.

Other zero-filled bytes in the `.rodata` and `.data` tails are not automatically
safe spare space, and neither segment is executable. This patch allocates no
additional heap space for injected code. Before adding payloads, check `.area`
limits, actual build symbols, and adjacent original functions; zero runs or a
bypassed entry alone do not justify enlarging a reclaimed region.

## Independent build and installation

The Bank subproject does not depend on Mover source or build artifacts. It can
compile the payload, create the IPS, rebuild all ten language archives, and run
static verification independently.

Install GNU Make, a POSIX-compatible shell, Python 3, and devkitARM, then set
the `DEVKITARM` environment variable. Windows builds of armips and Floating
IPS are bundled. Other platforms may set `ARMIPS` and `IPS_TOOL` to locally
downloaded or compiled executables.

Dump the inputs from the user's own Bank base title `00040000000C9B00`:

1. In GodMode9 `Title manager`, select the Bank base title and open
   `Open title folder`.
2. Select the executable `.app`, then run `NCCH image options...` →
   `Extract .code`.
3. Select the same `.app`, run `NCCH image options...` →
   `Mount image to drive`, and copy the complete mounted `romfs` directory.
4. Place the inputs in this layout:

```text
bank/rom/
├── exefs/
│   └── 00040000000C9B00.dec.code
└── romfs/
    └── ...
```

The base code must be `2,801,664` bytes with SHA-1
`5AB630856835DCF2DBDF9A62244DD19E46AE1C7C`. The build checks the input again.
Do not commit or redistribute the code image or RomFS. `ROMFS_SOURCE` may
override the default RomFS path when required.

Build Bank independently from the repository root:

```sh
make -C bank clean
make -C bank
```

The subproject can also be invoked directly:

```sh
make -C bank/src clean
make -C bank/src all
```

Example for a non-Windows host:

```sh
make -C bank ARMIPS=/path/to/armips IPS_TOOL=/path/to/flips
```

The build compiles the seven C translation units into objects for three verified
injection regions, emits their disassemblies for inspection, imports them and patches the
base image with armips, creates `code.ips` with Floating IPS, rebuilds the
ten-language RomFS, and runs static verification. The complete output is:

```text
release/00040000000C9B00/
├── code.ips
└── romfs/
```

The verifier checks the base hash, executable payload ranges, every ARM hook,
overwritten stock instructions, mode dispatch, byte-for-byte IPS reconstruction,
and all localized messages. Copy `00040000000C9B00` to `SD:/luma/titles/` and
enable Luma game patching. Back up the SD card and game saves before real-console
use.

## External open-source references

- [devkitPro/libctru FS declarations](https://github.com/devkitPro/libctru/blob/master/libctru/include/3ds/services/fs.h)
  and [implementation](https://github.com/devkitPro/libctru/blob/master/libctru/source/services/fs.c)
  were used to verify the public FSUSER/FSFILE interfaces and result handling.
- [Luma3DS loader patcher](https://github.com/LumaTeam/Luma3DS/blob/master/sysmodules/loader/source/patcher.c)
  was used to verify the title-specific `code.ips` and LayeredFS layout.
- [pkNX TextFile](https://github.com/kwsch/pkNX/blob/master/pkNX.Structures/Text/TextFile.cs)
  was used to verify the public message-file encoding and line-table format.

The current Bank binary remains authoritative for application addresses, state
transitions, object layouts, file offsets, and patch sites. External references
are used only for public platform and file-format interfaces.
