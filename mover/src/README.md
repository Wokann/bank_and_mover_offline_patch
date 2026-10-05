# Combined original and offline patch

See [`../docs/code-analysis.md`](../docs/code-analysis.md) for the stock
program's memory map, network path, BankObject, and Transport Box layout. This
document covers only the maintained patch design, implementation, and
independent build procedure.

This is the maintained Poke Mover v5.5.0 patch for base title
`00040000000C9C00`. It combines the verified standalone offline behavior with
an exact runtime path back to the original application.

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
switch between **Offline Mode** and **Original Mode**. The mode line is shown
below the HOME-menu help text, uses the same button glyph as the Bank patch,
and updates immediately.

Press **A**, **START**, or touch the lower screen to latch the displayed mode
for the session. R no longer changes mode after leaving the title screen.

```text
Title screen (default: Offline)
        │
        ├── R ───────────────► Original
        ├── R again ─────────► Offline
        └── A / START / touch
                  │ latch displayed mode
                  ├── Offline ─► local bankdata.bin and local transactions
                  └── Original ─► official network and server flow
```

## Strict separation between modes

Every hook inherited from the offline patch reads the latched session flag.
Offline Mode enters the local implementation. Original Mode replays the exact
overwritten instruction or resumes the original function body for:

- network availability and connection jobs;
- ticket handling;
- Gen 5 and Gen 1/2 legality requests;
- server Bank-data download;
- transfer eligibility and candidate states;
- remote staging, commit, rollback, and disconnect.

Original Mode therefore does not read, write, create, rename, or delete any
file under `sd:/3ds/Bank/`. It keeps the original server flow and original
messages.

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

## Offline transfer route

```text
Stock source-game selection
        │
        ▼
Generic Connecting message
        │
        ├── report network available locally
        └── allocate no remote connection job
        │
        ▼
Set runtime offline ticket to console time + 999 days
        │
        ▼
Connect to local offline Bank data (at least 2 seconds)
        │
        ▼
Stock cartridge read, filtering, and Pokemon conversion
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

### Gen 5 original and offline order

| Stage | Original | Offline |
|---|---|---|
| Source load | Validate the source save and select its redundant side | Native flow retained |
| Before submission | Obtain 30 raw records before the 14 checks or transfer conversion | Read-only decode and integrity-check the same records |
| Per-slot results | Submit raw records, validate the response, obtain per-slot decisions | Replace all results with `0`, `20`, or `1`; an unavailable source is a whole-flow error |
| After submission | Skip empty slots, report rejections, run the 14 checks on other candidates | Resume the same native postprocessing entry |
| Conversion/confirmation | Convert accepted records, enqueue in-memory transfer slots and source mappings, display warnings and confirmation | Native flow retained; the classifier never clears source slots |
| Save | Remote stage, source-game save, remote commit or rollback | Retain the local bankdata transaction below |

Result `0` is not final transfer approval. The classifier does not duplicate
the transfer-condition checks that follow. See
[Per-slot transfer decisions](../docs/code-analysis.md#per-slot-transfer-decisions)
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
prior Original Mode attempt in the same process and sets the shared Bank status
to its known ready value. Original Mode still obtains fresh values from
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
  seconds before stock cartridge reading and conversion proceed.
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
| `main.s` | Title-mode latch and mode dispatch for every offline hook |
| `fs_helpers.c` | Checked SD archive/file primitives shared by local loading and transactions |
| `bankdata_redirect.c` | Local Bankdata loading, transfer-slot preservation, eligibility update, and crash-safe temporary write, commit, and rollback |
| `local_ticket.c` | Runtime offline ticket and console-calendar calculation |
| `local_validation.c` | Read-only Gen 5 integrity/empty-slot classification and Gen 5/VC per-slot result isolation |
| `offline_flow.c` | Independent network, disconnect, remote-check, no-transfer, and save-delay state updates |
| `patch_paths.c` | Shared SD path constants |
| `patch_messages.py` | Appends and validates title/offline text in all ten language archives |
| `message_archive.py` | Self-contained GARC and encrypted message-file codec |
| `verify_patch.py` | Verifies the base hash, code regions, hooks, native replay, IPS reconstruction, and resources |
| `Makefile` | Compiles the functional objects, injects them into audited code regions, creates IPS, rebuilds messages, and writes the release tree |

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

The subproject can also be invoked directly:

```sh
make -C mover/src clean
make -C mover/src all
```

Example for a non-Windows host:

```sh
make -C mover ARMIPS=/path/to/armips IPS_TOOL=/path/to/flips
```

The build compiles the functional C modules into separate objects while
retaining module-local inlining, and emits an `.s` disassembly beside each
object for inspection. armips imports those objects consecutively into the
audited executable tail, Floating IPS creates `code.ips`, the ten-language
RomFS is rebuilt, and the static verifier checks the complete result. The
complete output is:

```text
release/00040000000C9C00/
├── code.ips
└── romfs/
```

The verifier checks the base hash, executable payload ranges, every ARM hook,
overwritten stock instructions, Offline/Original mode dispatch and continuation
sites, byte-for-byte IPS reconstruction, and all stock and added localized
messages. Copy `00040000000C9C00` to `SD:/luma/titles/` and enable Luma game
patching. Back up the SD card and source-game saves before real-console use.

## Transporter Redirect Patch compatibility

This patch leaves the stock hook ranges `0x0021A7E0–0x0021A7E4`,
`0x0021AA0C–0x0021AA24`, and `0x0021AB50–0x0021AB68`, plus the executable-tail
range `0x0028DD00–0x0028DDE0`, available for the
[DreamRadarCartRedirect Transporter Redirect Patch](https://github.com/zaksabeast/DreamRadarCartRedirect/blob/b518a9868c23c69fe94c2818a9e600fd09c24a92/transporter.s).
Those bytes remain identical to the supported stock image, and the build
verifier prevents future payload growth from entering them. Two IPS files made
against the same v5.5.0 base can therefore be combined with an IPS merger that
supports disjoint records. This reservation guarantees address compatibility
only with the audited external-patch revision; this project does not bundle or
redistribute the external patch.

## External open-source references

- [zaksabeast/Transporter-Offline-Patch](https://github.com/zaksabeast/Transporter-Offline-Patch)
  provided a public precedent for replacing Poke Mover network-facing states,
  including the two legality-request bypass locations.
- [Transporter-PKSM-Bank-Patch state-machine notes](https://github.com/zaksabeast/Transporter-PKSM-Bank-Patch/blob/master/TRANSPORTER_DOCS.md)
  were used to cross-check the high-level state order.
- [devkitPro/libctru FS declarations](https://github.com/devkitPro/libctru/blob/master/libctru/include/3ds/services/fs.h)
  and [implementation](https://github.com/devkitPro/libctru/blob/master/libctru/source/services/fs.c)
  were used to verify the public FSUSER/FSFILE interfaces and result handling.

The current Poke Mover binary remains authoritative for addresses, state
transitions, object layouts, file offsets, patch sites, and every Original Mode
continuation. External projects were used only as public cross-checks.
