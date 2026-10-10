# Combined online and offline patch

See [`docs/code-analysis.md`](docs/code-analysis.md) for the stock
program's memory map, network path, BankObject, and Transport Box layout. This
document covers only the maintained patch design, implementation, and
independent build procedure.

The build preserves the native startup initializer and places new features in
independent pages after the original BSS. A single `code.bps` and matching
`exheader.bin` serve Luma and Azahar: BPS declares the expanded image length,
and the startup helper automatically handles permission changes. Hardware must
successfully execute SVC `0x70`; only identified Azahar may continue when an
unimplemented SVC leaves the input process handle unchanged. No layout switch
or initializer-overwrite fallback is used. Static checks do not replace console
or emulator end-to-end testing; exact boundaries are documented in the analysis.

This is the maintained Poke Mover v5.5.0 patch for base title
`00040000000C9C00`. It combines the verified standalone offline behavior with
native online Bank/network transactions. Gen5 digital compatibility is shared
by both modes.

Poke Mover has no download mode. It neither creates a new local Bank nor
downloads the supported local file. Use Pokemon Bank's combined patch in
Download Mode to obtain `sd:/3ds/Bank/bankdata.bin`, or let Bank's Offline Mode
create it during first-time initialization, then use Poke Mover in Offline
Mode. The discontinued standalone Mover download experiment is not part of
this workflow.

The upload patch has also been permanently discontinued. Offline transfers are
committed only to the local Bank file and are never uploaded by this project.

## Mode selection

On a fresh launch, the title screen starts in **Offline Mode**. Press the physical **R** button to
switch between **Offline Mode** and **Online Mode**. The mode line is shown
below the HOME-menu help text, uses the same button glyph as the Bank patch,
and updates immediately. Returning to the title retains the selected mode;
restarting the application resets it to Offline. The choice stays in runtime
memory and is not written to a save.

Press **A**, **START**, or touch the lower screen to latch the displayed mode
for the session. R no longer changes mode after leaving the title screen.

```text
Title screen (default: Offline)
        │
        ├── R ───────────────► Online
        ├── R again ─────────► Offline
        └── A / START / touch
                  │ latch displayed mode
                  ├── Offline ─► local bankdata.bin and local transactions
                  └── Online ─► official network and server flow
```

## Mode-specific Bank/network flow, shared Gen5 digital compatibility

Every hook inherited from the offline patch reads the latched session flag.
Offline Mode enters the local implementation. Online Mode replays the exact
overwritten instruction or resumes the original function body for:

- network availability and connection jobs;
- ticket handling;
- Gen 5 and Gen 1/2 legality requests;
- server Bank-data download;
- transfer eligibility and candidate states;
- remote staging, commit, rollback, and disconnect.

That separation applies to the Bank backend, network transactions, tickets,
and legality services. Gen 5 source discovery and I/O for the selected digital
`.sav` form a shared input layer in both modes; they do not switch Online
Mode to the local Bank backend.

Online Mode therefore does not read, write, create, rename, or delete local
`bankdata.bin`, `bankdata.tmp`, or `bankdata.bak`. It keeps the official server flow and its network
messages. If an SD-backed Gen 5 source is selected, however, Online Mode uses
the same Gen5 digital compatibility layer to read and write its paired `.sav` under
`sd:/roms/nds/saves/`.

Public ticket policy should be `0`: Online Mode requires native tickets
and retains native errors when the interface is missing. Test policy `1` uses
local results only after identifying Azahar's missing interface; implemented
interfaces, hardware and unknown environments remain native. This does not
prove server-side approval or exclude downstream effects on cloud operations.
NNID, remote Pokemon checks, downloading and remote saving remain native.

Stock message entries remain unchanged. LayeredFS appends two title-screen
mode variants, four offline connection/save messages, and 28 Gen 5 game names
copied from the original seven game-language resources. Connection/save
overrides apply only in Offline Mode; ROM-language game names apply in both
modes. All ten shipped UI languages are rebuilt and validated.

## Shared UI language

Both modes read the language ID and Japanese kana/kanji preference from
`sd:/3ds/Bank/sav.bin` before native startup binds the message archive and font.
The file must be exactly `0x200` bytes and contain a supported language ID.
Missing, unreadable or unsupported settings retain the original startup fallback,
including console-based language selection.

A confirmed native Mover language selection also updates those settings in an
existing complete `sav.bin`. The update preserves every other byte, including
transaction metadata, checksum and the unused tail. Language fields
`0x28–0x2B` lie outside the original checksum's `0x00–0x1F` input, so the checksum
does not need to be recalculated. Mover does not create a missing Bank record or
write the original Bank save archive.

The complete updated record is written and closed as `sav.tmp` before deleting
`sav.bin` and renaming the temporary file. Failure before the replacement keeps
the existing record; failure after deletion leaves the completed temporary file
for Bank's existing recovery. No `.bak` or `.break` is added for these settings.
This shared language file is independent of the mode-specific Bankdata backend.

## Offline prerequisites and local file

Offline Mode uses the same file as Pokemon Bank:

```text
sd:/3ds/Bank/bankdata.bin
```

It must already be a valid current-format Bank object of exactly `0xBB518`
bytes. Poke Mover does not perform Bank's missing-file recovery or first-use
creation. A missing, unreadable, incorrectly sized, or invalid file follows the
error path.

The local Transfer Box must be empty before a new transfer. Existing Transfer
Box contents are never overwritten. Back up `bankdata.bin` before testing on
hardware.

## Shared Gen5 digital compatibility and the offline transfer route

### Gen5 digital compatibility shared by both modes

After the native physical-cartridge detection and validation finish, Online
Mode and Offline Mode both enumerate one regular file per frame from
`sd:/roms/nds/`. The `.nds` extension is case-insensitive. Names pass through
the FS UTF-16 interface, so ASCII names continue to work as a subset of UTF-16.
Only the first `0x10` bytes of each ROM are read. Bytes `0x0C–0x0F` identify
Black (`IRB?`), White (`IRA?`), Black 2 (`IRE?`), or White 2 (`IRD?`).

`sd:/roms/nds/<name>.nds` is paired with
`sd:/roms/nds/saves/<name>.sav`. The save must open for read and write without
creation, then pass the native asynchronous save load, redundant-side
selection, and integrity validation. The patch neither repairs checksums itself
nor admits a candidate from its filename alone.

Only one valid source is retained for each game:

1. A physical cartridge that passes native validation locks its game and cannot
   be replaced by a digital copy. An invalid cartridge still falls back to SD.
2. Digital copies use `J > O > F > I > D > S > K`, taken from the final game-code
   byte. A later valid copy replaces the current candidate only at a better rank.
3. Among valid copies of the same language, the first encountered wins. An
   invalid copy occupies no slot, so later duplicates are still tested.
4. Enumeration can stop early only after all four games have an unbeatable
   source: a valid cartridge or a valid `J` copy. Otherwise it reaches the end
   of the directory.

A missing directory continues the native flow without SD digital sources only
after its archive closes successfully. Enumeration, directory-close, or
archive-close failures discard the uncommitted scan and use the native
no-usable-software error route. The list is published only after successful
cleanup; failed cleanup of old handles also prevents a new scan. Custom
directory and save-validation phases still
run the native loading-view update once per frame. File and directory close
still attempt kernel-handle release after a failed service-close request and
return the first error, preventing handle leaks while scanning many candidates.

Up to four logical Gen 5 sources are emitted first, after which the native flow
continues appending VC sources. Because the stock array holds only 40 entries,
the hook at `0x0024149C` replays the original count store and ends VC enumeration
when Gen 5 plus VC sources reach 40; below that limit, the stock loop is left
unchanged. Selecting a digital source pins its UTF-16 save path; the actual
transfer reopens it and runs native validation again. A later I/O failure
follows the current mode's native error or rollback route instead of silently
changing to another copy mid-transaction.

Gen 5 game names follow the selected cartridge or ROM's game-code language,
not the Mover UI language: `J` Japanese, `O` English, `F` French, `I` Italian,
`D` German, `S` Spanish, or `K` Korean. The physical cartridge's game code is
read after native validation; SD sources retain the code read during scanning.
All four bundled body-text fonts contain the characters used by these original
names. The UI language, trainer name/ID, source priority, and native VC title
selection remain unchanged. If the game code cannot be read or recognized,
the original UI-language title is retained.

The Bank connection, validation, and save state diagram below applies only to
Offline Mode. Online Mode performs the same source selection, then continues
through the stock network, server-Bank, and transaction flow.

```text
Source-game selection (valid cartridge / local Gen 5 ROM / native VC)
        │
        ▼
Generic Connecting message
        │
        ├── report network available locally
        └── allocate no remote connection job
        │
        ▼
Enter the native ticket state; supply console time + 999 days at its job interfaces
        │
        ▼
Connect to local offline Bank data (at least 2 seconds)
        │
        ▼
Native source-save reading, filtering, and Pokemon conversion
        │
        ├── Gen 5: mark empty slots as stock result 20;
        │          run stock local checks for non-empty slots
        └── Gen 1/2: clear stale server results;
                    retain stock null-slot and local checks
                                              │
                                              ▼
                  Clear stale remote transaction and complete check locally
                                              │
                                              ▼
                         Load and validate complete bankdata.bin
                                ├── invalid/missing ─► error
                                ├── Transfer Box busy ► stock message 5;
                                │                       preserve file
                                └── Transfer Box empty ► stock confirmation UI
```

The ticket step supplies Mover's runtime entitlement date, remaining-day/hour,
and validity fields. It is not a Poké Mile calculation: Mover has no local
Poké Mile accumulation or reward path in this patch.

### Ticket routing: public and automatic-detection test builds

`ONLINE_TICKET_CHECK_BYPASS` in `main.s` emits one policy byte. Both routes
remain in the same image:

The source defaults to the public policy `0`; set `1` only for personal tests
that allow the missing-interface fallback.

| Session mode / environment | Policy byte | Ticket job / free-campaign query |
|---|---:|---|
| Offline, any environment | `0` or `1` | Local results; no ticket network client or system shop job |
| Online, any environment | `0` (public default) | Native checks/callbacks; no fabricated missing-interface success |
| Online, Azahar confirmed to lack the interface | `1` (test) | Automatically select local results; other network operations remain native |
| Online, implemented interface, hardware or unknown environment | `1` | Native checks and callbacks |
| Online, failed probe or ordinary ticket error | `1` | Native error handling, not local fallback |

At the time of this ticket-routing commit, official Azahar does not implement
`am:net`'s `GetRightsOnlyTicketData` (command `0x0821`; see the
[interface registration](https://github.com/azahar-emu/azahar/blob/6280521bf26ef1393974cd8c898de29cf75f5752/src/core/hle/service/am/am_net.cpp#L115)).
Two configurations remain available: `0` forbids fabricated online results and
is the public default; `1` automatically selects local results only after confirming
the missing interface in Azahar, for the author's tests. The author's Azahar
[`pokebank` test branch](https://github.com/Wokann/azahar/tree/pokebank)
implements the interface, which the author has verified in online testing.
For that fork or real hardware, the value can remain fixed at `0`; no ticket
bypass is needed. Offline Mode always uses local results regardless of this value.

That Azahar feature implementation was generated entirely by AI and does not meet the
[Azahar AI Use Policy](https://github.com/azahar-emu/azahar/blob/master/AI-POLICY.md)
requirements for accepting contributions written entirely or substantially by AI.
It is therefore kept only in the personal test branch and will not be submitted
to Azahar's official main branch.

Change this value and rebuild; a second source tree is unnecessary. The entry
at `0x00249600`, UI waiting, job construction, result propagation, validity
decision, state exit and destruction remain native. Only four calls are routed:

| Call site | Native interface | Local replacement |
|---|---|---|
| `0x00249754` | `TicketJob_Initialize` | Initialize local dates and job result fields |
| `0x00249774` | `TicketJob_Poll` | Report a completed job with valid entitlement |
| `0x00249818` | `MoverCampaign_Request` | Deliver the local current-to-expiry window to the native campaign callback |
| `0x00249CEC` | `TicketJob_Unbind` | No client was bound; still proceed with native destruction |

The policy byte is at `0x00365428`. Initialization selects one backend per job,
recorded at `0x00363FF8`; polling, campaign and unbinding reuse it without changing
route mid-flight. Public policy `0` does not probe. Test policy `1` reuses the
`GetSystemInfo(0x20000, 0)` Azahar ID `2` check, then opens `am:net` nonblockingly
and issues a read-only query with zero title/ticket IDs and a four-byte buffer.
Only successful transport, exact short-success header `0x08210040`, result `0`
and successful handle closure identify the missing interface; see the verified
[unimplemented-handler behavior](https://github.com/azahar-emu/azahar/blob/6280521bf26ef1393974cd8c898de29cf75f5752/src/core/hle/service/service.cpp#L155).
Full replies, missing tickets, account/service errors and unknown responses stay native.

**Test policy is not a guarantee of safe online use**. Unlike Bank, Mover does not
copy the local ticket date into a mileage calculation. Its online Bankdata body
is loaded from the server, and no path was identified that serializes local
ticket dates or entitlement display fields into that body. Local results still
change the client's entitlement/campaign decision; this is not proof of real
server approval. Keep public builds at `0`, accepting native failures when the
interface is missing instead of using fabricated online ticket results.

The local job supplies separate console-current and 999-day expiry dates.
Native getters calculate `999` remaining days, `23976` total hours and the
validity flag. Existing account fields are preserved; purchase counts remain
unknown (`-1`), without inventing purchases or purchase rewards. The local
campaign result passes this same window, with a matching request ID and valid
CRC, to the unchanged native callback. It computes remaining time and sets the
free/valid flags; the patch does not assign those outputs. Each local initialization rebuilds
the window from console time, not fixed server campaign dates. An out-of-range
date or failed job allocation uses the native error exit instead of success.
See [Ticket state and local results](docs/code-analysis.md#ticket-state-and-local-results)
for the field and branch comparison.

The native candidate-conversion state remains responsible for reading the
source game, filtering records, and constructing transfer candidates. After
bypassing its two embedded server legality requests, Offline Mode reconstructs
the per-slot results required by the native continuation. The Gen 5 classifier
honors native flags and block order while decoding temporary values and checking
the record checksum. Explicit exclusion flags, checksum failures, and unsupported
species receive generic rejection result `1`; intact species-0 records receive
`20`; remaining nonempty species `1–649` receive `0` for native local checks.
An empty slot need not match a complete byte template. Source records, names,
control flags, and checksums are never rewritten. An unavailable source follows
the native read-error message and exit path, not a synthetic empty/candidate
result. The VC null/item/egg decisions remain unchanged; all 30 stale results
are cleared. Unknown server-only legality rules and name normalization are not
recreated in Offline Mode.

### Gen 5 online and offline order

| Stage | Online | Offline |
|---|---|---|
| Source load | Validate the source save and select its redundant side | Native flow retained |
| Before submission | Obtain 30 raw records before the 14 checks or transfer conversion | Read-only decode and integrity-check the same records |
| Per-slot results | Submit raw records, validate the response, obtain per-slot decisions | Replace all results with `0`, `20`, or `1`; an unavailable source is a whole-flow error |
| After submission | Skip empty slots, report rejections, run the 14 checks on other candidates | Resume the same native postprocessing entry |
| Conversion/confirmation | Convert accepted records, enqueue in-memory transfer slots and source mappings, display warnings and confirmation | Native flow retained; the classifier never clears source slots |
| Save | Remote stage, source-game save, remote commit or rollback | Retain the local bankdata transaction below |

Result `0` is not final transfer approval. The classifier does not duplicate
the transfer-condition checks that follow. See
[Per-slot transfer decisions](docs/code-analysis.md#per-slot-transfer-decisions)
for the full order, fields, and algorithms.

### Retained 14 local checks

Every check runs before the combined failure mask is interpreted. The
classifier does not replace these rules.

| Number | Check | Native handling |
|---:|---|---|
| 1 | Egg flag | Reject eggs |
| 2 | Fused Kyurem form | Reject fused forms |
| 3 | Held item | Nonfatal; report and remove the item |
| 4 | Original Trainer name check position | Always succeeds locally; no new name handling |
| 5 | Nickname check position | Always succeeds locally; no new name handling |
| 6 | Species/origin-version combination | Reject combinations outside the native allowlist |
| 7 | Event marker and selected origin Trainer IDs | Reject inconsistent native combinations |
| 8 | Met/acquisition level | Reject zero |
| 9 | Hatch location, acquisition level, origin generation | Reject inconsistent combinations |
| 10 | Minimum capture level for non-hatched records | Reject values below the species requirement |
| 11 | Language ID | Apply supported-language checks and the native special-origin exception |
| 12 | Origin version | Reject zero |
| 13 | Legacy origin and migration/met location | Reject combinations outside the native rules |
| 14 | Legacy origin and hidden-ability marker | Reject incompatible combinations |

Rejected records are excluded from this transfer's candidates, not erased from
their source save. The classifier never resets them to empty slots.

The offline remote-check state also clears transaction parameters left by a
prior Online Mode attempt in the same process and sets the shared Bank status
to its known ready value. Online Mode still obtains fresh values from
the server.

Before loading `bankdata.bin`, the patch preserves candidates in native
Transfer Slot objects. After validating the complete local Bank, it restores
the candidates only when the Transfer Box is empty. Stock slot-write operations
update each `0xE8`-byte record and its parallel tag; the patch does not copy raw
slot bytes over existing data. Existing account identity fields are preserved.

## Offline save transaction

```text
User confirms the transfer
        │
        ▼
Stock full Bank serialization (0xBB518 bytes)
        │
        ▼
Write bankdata.tmp + set size + flush + verify bytes written
        ├── failure ─► abort; preserve bankdata.bin
        └── success
              │
              ▼
Keep save-progress screen visible for at least 2 seconds
        │
        ▼
Stock source-game save
        ├── failure ─► delete tmp; retain bin and bak
        └── success
              │
              ▼
Delete old bak ─► rename bin to bak ─► rename tmp to bin
        ├── success ─► stock disconnect route
        └── failure ─► restore bak where possible
```

Local staging, commit, and rollback replace only their remote counterparts.
The staging write is synchronous, followed by a nonblocking two-second
state-machine delay; rendering continues and the file is not written twice.
Commit occurs only after the original source-game save succeeds. File close,
size, write length, delete, rename, restoration, and rollback results are
checked.

When there are no transferable Pokemon or the user cancels, the stock
no-transfer route is retained. Its final remote transaction is completed
locally before entering the stock disconnect route.

## Offline display and timing

- The initial Internet line becomes a generic **Connecting...** message.
- The local Bank-data connection message remains visible for at least two
  seconds before source-save reading and conversion proceed.
- The save message identifies local offline Bank data and remains visible for
  at least two seconds after the complete staging write.
- The disconnect line is generic. Its existing 1.5-second local first phase is
  retained, and no remote disconnect job is allocated.

The stock state functions remain the sole owner of the waiting UI, spinner,
and rhythmic sound. Offline updates report completion through the original
state object; the native state exit path performs cleanup.

## Source layout

| File | Role |
|---|---|
| `src/main.s` | Title-mode latch, offline-hook dispatch, and shared source entries/native trampolines |
| `src/code_expansion.c` / `include/code_expansion.h` | Identify Azahar, obtain a real process handle, and enable execution only on the added pages |
| `src/fs_helpers.c` / `include/fs_helpers.h` | Shared checked SD operations; TLS/SVC primitives remain ARM |
| `src/nds_fs.c` / `include/nds_fs.h` | UTF-16 directory enumeration and ranged file I/O |
| `src/nds_sources.c` / `include/nds_sources.h` | Gen5 digital compatibility: per-frame source discovery, native validation, ranking, ROM-language titles, and save-I/O dispatch |
| `src/bankdata_redirect.c` | Local Bankdata loading, transfer-slot preservation, eligibility update, and crash-safe temporary write, commit, and rollback |
| `src/local_ticket.c` | Job-backend selection, Azahar missing-interface probe, local job/campaign results and console-calendar calculation |
| `src/local_validation.c` | Read-only Gen 5 integrity/empty-slot classification and Gen 5/VC per-slot result isolation |
| `src/offline_flow.c` | Independent network, disconnect, remote-check, no-transfer, and save-delay state updates |
| `src/language_settings.c` / `include/language_settings.h` | Share Bank's SD language settings before native resource binding and update confirmed selections through `sav.tmp` |
| `src/patch_paths.c` | Shared SD path constants |
| `src/patch_messages.py` | Appends and validates mode/offline text and original ROM-language game names in all ten UI-language archives |
| `tools/message_archive.py` | Self-contained GARC and encrypted message-file codec |
| `tools/verify_patch.py` | Verifies the base hash, code regions, preserved constructors, hooks, native replay, BPS reconstruction/CRCs, and resources |
| `tools/prepare_expanded_code.py` | Materialize BSS at its original addresses, extend data pages, authorize required SVCs, and generate the matching ExHeader |
| `Makefile` | Compiles functional objects, enforces payload bounds, creates BPS, rebuilds messages, and writes the release tree |

## Independent build and installation

The Mover subproject does not depend on Bank source or build artifacts. It can
compile the payload, create the BPS, rebuild all ten language archives, and run
static verification independently. At runtime, Offline Mode still requires a
`sd:/3ds/Bank/bankdata.bin` previously downloaded or initialized by Bank.

Install GNU Make, a POSIX-compatible shell, Python 3, and devkitARM, then set
the `DEVKITARM` environment variable. Windows builds of armips and Floating
IPS are bundled. Other platforms may set `ARMIPS` and `IPS_TOOL` to locally
downloaded or compiled executables.

Dump the inputs from the user's own Mover base title `00040000000C9C00`:

1. In GodMode9 `Title manager`, select the Mover base title and open
   `Open title folder`.
2. Select the executable `.app`, then run `NCCH image options...` →
   `Extract .code`.
3. Select the same `.app`, run `NCCH image options...` →
   `Mount image to drive`, and copy the complete mounted `romfs` directory and
   root `extheader.bin`. Rename the latter to `exheader.bin` in the layout below.
4. Place the inputs in this layout:

```text
mover/rom/
├── exheader.bin
├── exefs/
│   └── 00040000000C9C00.dec.code
└── romfs/
    └── ...
```

The base code must be `2,269,184` bytes with SHA-1
`583859C1E874D11650EFBDDE51F470ECF96900C4`. The build checks the input again.
Do not commit or redistribute the code image or RomFS. `ROMFS_SOURCE` may
override the default RomFS path when required.

The build also requires the original ExHeader from the same base
title (`0x400` or `0x800` bytes), not another game, update, or modified header.
Set `EXHEADER=/path/to/exheader.bin` to use a different input location. The tool
does not change that input. GodMode9's mounted filename is `extheader.bin`.

Build Mover independently from the repository root:

```sh
make -C mover clean
make -C mover
```

From the subproject directory:

```sh
cd mover
make clean
make
```

Example for a non-Windows host:

```sh
make -C mover ARMIPS=/path/to/armips IPS_TOOL=/path/to/flips
```

The build compiles the functional C modules into separate objects while
retaining module-local inlining, and emits an `.s` disassembly beside each
object for inspection. armips imports those objects consecutively into the
added payload pages, with matching code/ExHeader preparation.
The local ticket module and
TLS/SVC primitives use ARM; the filesystem, source scanner, local classification,
Bankdata and offline-flow modules use Thumb with explicit interworking entries.
Floating IPS creates `code.bps`, the ten-language RomFS is rebuilt, and the static
verifier checks the complete result. The complete output is:

```text
release/00040000000C9C00/
├── code.bps
├── exheader.bin
└── romfs/
```

Always install `code.bps`, `exheader.bin`, and `romfs/` together. Remove stale
`code.ips` or `code.bin` files from the same mod directory; both loaders can
otherwise apply another patch or load another base image. BPS also checks that
the source code matches the supported version. The build removes the generated
`code.ips` from its own Mover release directory.

The text-tail bootstrap obtains a real process handle through `DuplicateHandle`,
queries `GetSystemInfo(0x20000, 0)`, and calls `ControlProcessMemory` with operation
`6` and permission `7` only for `0x00365000–0x00368000`. SVC result `0` continues
normally. The only nonzero compatibility case is a successful environment query
with emulator ID `2`, high word `0`, and an SVC result equal to its input process
handle. Hardware and unknown environments retain strict result checks; negative
results and every other nonzero result stop startup. Handle creation and closing
must succeed. Registers are restored before continuing the native constructor
walker. Original segment bases, BSS variables, LayeredFS, and the version marker
do not move. Ticket test policy reuses environment detection but probes its own
interface; permission compatibility does not imply ticket support or enable local online results.

Unmodified Azahar can load this BPS without implementing SVC `0x70`; this path
relies on its permissive instruction fetching and is not a successful permission
change. [Wokann/azahar's svc112 branch](https://github.com/Wokann/azahar/tree/svc112)
instead executes the permission change normally. Install the same `code.bps`,
full `0x800`-byte `exheader.bin`, and `romfs/` together in Azahar's
`load/mods/00040000000C9C00/` directory. Its ExHeader override is not read from
the emulated `sdmc/luma/titles/` path. Static checks are not end-to-end verification.

For the progression from Magikoopa-based hardware expansion, personal emulator
loader/SVC fixes, and the OOT dual-layout reference to unified BPS loading, see
[Expansion development and approach comparison](docs/code-analysis.md#expansion-development-and-approach-comparison).
That section explains the differences and compatibility limits.

The verifier checks the base hash, executable payload ranges, every ARM hook,
overwritten stock instructions, Offline/Online mode dispatch and continuation
sites, byte-for-byte BPS reconstruction and all three CRCs, and all stock and added localized
messages. Copy `00040000000C9C00` to `SD:/luma/titles/` and enable Luma game
patching. Back up the SD card and source-game saves before real-console use.

## Relationship to other cartridge-redirect patches

The Gen5 digital compatibility feature owns the three native NDS cartridge-I/O entries and includes
multi-ROM scanning, source ranking, and native save validation. The former
`0xE0` reservation for the
[DreamRadarCartRedirect Transporter Redirect Patch](https://github.com/zaksabeast/DreamRadarCartRedirect/blob/b518a9868c23c69fe94c2818a9e600fd09c24a92/transporter.s)
has therefore been removed. Both patches modify the same entry points and
**must no longer be combined directly with an IPS merger**. Do not install the
external Transporter redirect patch together with this patch.

## External open-source references

- [Magikoopa's expansion implementation](https://github.com/RicBent/Magikoopa/blob/master/MagikoopaUI/patchmaker.cpp)
  informed fixed-address data-tail expansion, BSS materialization, and startup
  permission handling.
- [oot3d_practice_menu](https://github.com/gamestabled/oot3d_practice_menu)
  provided the hardware/Citra ExHeader-layout and loader-result comparison.
- [zaksabeast/Transporter-Offline-Patch](https://github.com/zaksabeast/Transporter-Offline-Patch)
  provided a public precedent for replacing Poke Mover network-facing states,
  including the two legality-request bypass locations.
- [Transporter-PKSM-Bank-Patch state-machine notes](https://github.com/zaksabeast/Transporter-PKSM-Bank-Patch/blob/master/TRANSPORTER_DOCS.md)
  were used to cross-check the high-level state order.
- [devkitPro/libctru FS declarations](https://github.com/devkitPro/libctru/blob/master/libctru/include/3ds/services/fs.h)
  and [implementation](https://github.com/devkitPro/libctru/blob/master/libctru/source/services/fs.c)
  were used to verify the public FSUSER/FSFILE interfaces and result handling.
- [GBATEK's DS cartridge-header documentation](https://problemkaputt.de/gbatek-ds-cartridge-header.htm)
  was used to cross-check the location of the header game-code field.

The current Poke Mover binary remains authoritative for addresses, state
transitions, object layouts, file offsets, patch sites, and every Online Mode
continuation. External projects were used only as public cross-checks.
