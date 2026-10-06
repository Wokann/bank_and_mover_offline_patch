# Combined online and offline patch

See [`docs/code-analysis.md`](docs/code-analysis.md) for the stock
program's memory map, network path, BankObject, and Transport Box layout. This
document covers only the maintained patch design, implementation, and
independent build procedure.

The current payload overwrites `0x00261C74–0x00262C10`, an active native
startup initializer rather than a safe code cave. This is a known startup
risk, documented in the analysis. Continuing after disabling exception
handling does not make the layout safe; static checks and source-scanning
tests do not establish safe end-to-end operation.

This is the maintained Poke Mover v5.5.0 patch for base title
`00040000000C9C00`. It combines the verified standalone offline behavior with
native online Bank/network transactions. Gen 5 source redirection is shared
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

The title screen starts in **Offline Mode**. Press the physical **R** button to
switch between **Offline Mode** and **Online Mode**. The mode line is shown
below the HOME-menu help text, uses the same button glyph as the Bank patch,
and updates immediately.

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

## Mode-specific Bank/network flow, shared source redirection

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

Online Mode therefore does not read, write, create, rename, or delete any
Bank file under `sd:/3ds/Bank/`. It keeps the official server flow and its network
messages. If an SD-backed Gen 5 source is selected, however, Online Mode uses
the same redirect to read and write its paired `.sav` under
`sd:/roms/nds/saves/`.

The ticket test policy defaults to `0`. Setting it to `1` makes Online Mode
use local ticket/campaign results for emulator testing; NNID connection, remote
Pokemon validation, downloading and remote saving remain native. This test
configuration is not unmodified online behavior and does not prove server-side
entitlement approval.

Stock message entries remain unchanged. The LayeredFS archive only appends two
title variants and four offline connection/save messages; runtime hooks select
those entries only in Offline Mode. All ten shipped languages are rebuilt and
validated.

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

## Shared source discovery and the offline transfer route

### Gen 5 source discovery shared by both modes

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

A missing directory means that no SD digital source is present and the native
flow continues. If directory enumeration IPC fails after opening the directory,
the current uncommitted scan is discarded and the native no-usable-software
error route is used; the failure is not mistaken for a normal end of directory
and no partial list is shown. Custom directory and save-validation phases still
run the native loading-view update once per frame. Closing a file performs both
the FSFILE service close and the kernel-handle close, preventing handle leaks
while many candidates are scanned.

Up to four logical Gen 5 sources are emitted first, after which the native flow
continues appending VC sources. Because the stock array holds only 40 entries,
the hook at `0x0024149C` replays the original count store and ends VC enumeration
when Gen 5 plus VC sources reach 40; below that limit, the stock loop is left
unchanged. Selecting a digital source pins its UTF-16 save path; the actual
transfer reopens it and runs native validation again. A later I/O failure
follows the current mode's native error or rollback route instead of silently
changing to another copy mid-transaction.

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

### Ticket routing and test policy

`ONLINE_TICKET_CHECK_BYPASS` in `main.s` emits one policy byte. Both routes
remain in the same image:

| Session mode | Policy byte | Ticket job / free-campaign query |
|---|---:|---|
| Offline | `0` or `1` | Local results; no ticket network client or system shop job |
| Online | `0` (default) | Native online checks and callbacks |
| Online | `1` | Local results for emulator testing; other network operations remain native |

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

Change this value and rebuild; a second source tree is unnecessary. The entry
at `0x00249600`, UI waiting, job construction, result propagation, validity
decision, state exit and destruction remain native. Only four calls are routed:

| Call site | Native interface | Local replacement |
|---|---|---|
| `0x00249754` | `TicketJob_Initialize` | Initialize local dates and job result fields |
| `0x00249774` | `TicketJob_Poll` | Report a completed job with valid entitlement |
| `0x00249818` | `MoverCampaign_Request` | Complete as no free campaign and resume the native entitlement decision |
| `0x00249CEC` | `TicketJob_Unbind` | No client was bound; still proceed with native destruction |

The local job supplies separate console-current and 999-day expiry dates.
Native getters calculate `999` remaining days, `23976` total hours and the
validity flag. Existing account fields are preserved; purchase counts remain
unknown (`-1`), without inventing purchases or campaign rewards. An out-of-range
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
| `src/fs_helpers.c` / `include/fs_helpers.h` | Shared checked SD operations; TLS/SVC primitives remain ARM |
| `src/nds_fs.c` / `include/nds_fs.h` | UTF-16 directory enumeration and ranged file I/O |
| `src/nds_sources.c` / `include/nds_sources.h` | Per-frame Gen 5 source discovery, native validation, ranking, and save-I/O dispatch |
| `src/bankdata_redirect.c` | Local Bankdata loading, transfer-slot preservation, eligibility update, and crash-safe temporary write, commit, and rollback |
| `src/local_ticket.c` | Local native-job/campaign results and console-calendar calculation |
| `src/local_validation.c` | Read-only Gen 5 integrity/empty-slot classification and Gen 5/VC per-slot result isolation |
| `src/offline_flow.c` | Independent network, disconnect, remote-check, no-transfer, and save-delay state updates |
| `src/patch_paths.c` | Shared SD path constants |
| `src/patch_messages.py` | Appends and validates title/offline text in all ten language archives |
| `tools/message_archive.py` | Self-contained GARC and encrypted message-file codec |
| `tools/verify_patch.py` | Verifies the base hash, code regions, hooks, native replay, IPS reconstruction, and resources |
| `Makefile` | Compiles functional objects, enforces payload bounds, creates IPS, rebuilds messages, and writes the release tree |

## Independent build and installation

The Mover subproject does not depend on Bank source or build artifacts. It can
compile the payload, create the IPS, rebuild all ten language archives, and run
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
   `Mount image to drive`, and copy the complete mounted `romfs` directory.
4. Place the inputs in this layout:

```text
mover/rom/
├── exefs/
│   └── 00040000000C9C00.dec.code
└── romfs/
    └── ...
```

The base code must be `2,269,184` bytes with SHA-1
`583859C1E874D11650EFBDDE51F470ECF96900C4`. The build checks the input again.
Do not commit or redistribute the code image or RomFS. `ROMFS_SOURCE` may
override the default RomFS path when required.

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
object for inspection. armips imports those objects into the initializer-overwrite
region and executable text tail, subject to the known startup risk above.
The local ticket module and
TLS/SVC primitives use ARM; the filesystem, source scanner, local classification,
Bankdata and offline-flow modules use Thumb with explicit interworking entries.
Floating IPS creates `code.ips`, the ten-language RomFS is rebuilt, and the static
verifier checks the complete result. The complete output is:

```text
release/00040000000C9C00/
├── code.ips
└── romfs/
```

The verifier checks the base hash, executable payload ranges, every ARM hook,
overwritten stock instructions, Offline/Online mode dispatch and continuation
sites, byte-for-byte IPS reconstruction, and all stock and added localized
messages. Copy `00040000000C9C00` to `SD:/luma/titles/` and enable Luma game
patching. Back up the SD card and source-game saves before real-console use.

## Relationship to other cartridge-redirect patches

This patch now owns the three native NDS cartridge-I/O entries and includes
multi-ROM scanning, source ranking, and native save validation. The former
`0xE0` reservation for the
[DreamRadarCartRedirect Transporter Redirect Patch](https://github.com/zaksabeast/DreamRadarCartRedirect/blob/b518a9868c23c69fe94c2818a9e600fd09c24a92/transporter.s)
has therefore been removed. Both patches modify the same entry points and
**must no longer be combined directly with an IPS merger**. Do not install the
external Transporter redirect patch together with this patch.

## External open-source references

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
