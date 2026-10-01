# Combined offline and download patch

See [`../docs/code-analysis.md`](../docs/code-analysis.md) for the stock
program's memory map, state table, network paths, BankObject, and bankdata
layout. This document covers only the maintained patch design, implementation,
and independent build procedure.

This is the maintained Pokemon Bank v1.5 patch for base title
`00040000000C9B00`. It incorporates the useful behavior and safety checks of
the former standalone download and offline patches into one runtime-selectable
build. It does not concatenate their IPS files: every shared network, data,
message, and save hook dispatches according to a session mode.

The upload patch has been permanently discontinued. This project only imports
server data in download mode and uses a separate local copy in offline mode; it
never uploads offline changes to the official service.

## Modes and recommended workflow

The title screen defaults to **Offline Mode**. Press the physical **R** button
to switch between **Offline Mode** and **Download Mode**. The line below the
stock HOME-menu help updates immediately. Press **A**, **START**, or touch the
lower screen to latch the displayed mode for the whole session; R no longer
changes it after leaving the title screen.

```text
Title screen (default: Offline)
        │
        ├── R ───────────────► Download
        ├── R again ─────────► Offline
        └── A / START / touch
                  │ latch displayed mode
                  ├── Download ─► official server download ─► local file
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

Both modes use the same checked local-file implementation.

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

Both patch modes redirect the persistence backend of the stock logical
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
local-record reads and writes in both modes use it exclusively. An SD write
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

Download Mode locally completes the separate entitlement/campaign state to
disable `free_campaign` and online gifts while supplying the downstream
runtime fields. This state is independent of the normal local mileage states.

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

| Feature-menu entry | Offline Mode | Download Mode |
|---|---|---|
| First entry | Use Pokemon Bank | Download Bank Data |
| About Pokemon Bank | Stock information screen | Stock information screen |
| Support | Disabled; return to feature menu | Disabled; return to feature menu |
| Poke Mover/eShop | Disabled; return to feature menu | Disabled; return to feature menu |
| Pokemon HOME | Stock language-selection flow | Stock language-selection flow |
| Back | Stock behavior | Stock behavior |

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
| `local_ticket.c` | Locally completes the entitlement/optional-reward state and supplies ticket fields without implementing Poké Mile calculation |
| `patch_paths.c` | Path constants placed in the verified tail-code region |
| `turtle_redirect.c` | Redirected Turtle-record backend placed in the verified HOME-code region |
| `../include/bankdata_redirect.h` | Confirmed serialized Bankdata layout, partial native Bank views, redirection constants, and stock entry points |
| `../include/fs_helpers.h` | Shared filesystem types, SDK entry points, and checked SD-helper declarations |
| `../include/local_mileage.h` | Local mileage-date input declaration |
| `../include/local_ticket.h` | Offline-ticket shared-data view, entitlement constants, and state entry point |
| `../include/offline_flow.h` | Stock timer entry points used by local flow states |
| `../include/patch_types.h` | Fixed-width primitive types shared by the injected C objects |
| `../include/patch_paths.h` | Path declarations shared across the separately placed objects |
| `../include/system_time.h` | Shared-memory system-time structure and addresses |
| `../include/turtle_redirect.h` | Confirmed logical-record layout, backend result contract, stock entry points, and exported backend declarations |
| `patch_messages.py` | Rebuilds and validates the ten localized LayeredFS archives |
| `message_archive.py` | Self-contained GARC and encrypted message-file codec |
| `verify_patch.py` | Verifies base hash, code ranges, hooks, IPS reconstruction, native instruction replay, and resources |
| `Makefile` | Compiles, injects, creates the IPS, rebuilds messages, and writes the Luma release tree |

The patch leaves `0x00313910–0x00313A3F` unused because Luma installs its
LayeredFS redirection payload from the title's original `.text` end. Patch code
starts after that reserved area and remains below the executable limit at
`0x00314000`.

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
