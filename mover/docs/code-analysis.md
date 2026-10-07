# Poke Mover Code Analysis

This document analyzes the supported stock binary and compares its execution
order with the offline adaptation. See [`../README.md`](../README.md)
for build instructions and transaction rules.

## Target and memory layout

- Title ID: `00040000000C9C00`
- Version: v5.5.0
- Image base: `0x00100000`
- Decompressed code SHA-256: `001C20ADA74016507C969BB44A0A50F8EDF803EC06A3FC46834263BA8DF0FD2F`

| Region | Virtual range | Properties |
|---|---|---|
| `.text` | `0x00100000–0x0028E000` | Read/execute; actual content ends at `0x0028D1AC` |
| `.rodata` | `0x0028E000–0x002EC000` | Read-only, non-executable |
| `.data` | `0x002EC000–0x003293FC`, with loaded pages through `0x0032A000` | Read/write |
| `.bss` | Logical range `0x003293FC–0x003638A4` | Read/write, zero-initialized |
| RW page padding | `0x003638A4–0x00364000` | Read/write, zero-initialized |
| Thread stack | Length `0x40000` | ExHeader setting |

The ExHeader declares the `.text` content end at `0x0028D1AC`, while the last
mapped page ends at `0x0028E000`. Ghidra's last referenced instruction/data
location is `0x0028D188`, and its last function ends at `0x0028D18F`.
Luma installs its LayeredFS payload at the declared content end when the page
padding is sufficient, after applying code patches. This project leaves
`0x0028D1AC–0x0028D2DC` (`0x130` bytes) untouched for it and aligns its own tail
start to `0x0028D2E0`. The inspected Luma payload is `0x114` bytes; this reservation
must be rechecked if that payload grows. See the official
[installation logic](https://raw.githubusercontent.com/LumaTeam/Luma3DS/master/sysmodules/loader/source/patcher.c)
and [payload assembly](https://raw.githubusercontent.com/LumaTeam/Luma3DS/master/sysmodules/loader/source/romfsredir.s).

The project tail area is `0x0028D2E0–0x0028DFC0`; the final
`0x0028DFC0–0x0028E000` (`0x40` bytes) remains reserved for the version
identifier. The integrated Gen 5 source redirect owns the native cartridge I/O
hooks, so the external Transporter Redirect Patch's `0xE0` reservation is
removed. Assembly limits and static verification still protect LayeredFS and
the version marker. All feature objects use the added-page layout below.
The loader and ticket test policy share the environment-identification helper;
the latter also checks ticket-interface replies rather than assuming that the
environment alone establishes ticket support.

`0x00261C74–0x00262C10` is part of an active native global-registration
initializer, not a reclaimable gap. Startup enters `0x00102ADC`, which walks
the PREL32 constructor table at `0x002EB744–0x002EBA58`. Entry `0x002EB7BC`
stores `0xFFF764B8`; adding it to the entry address gives `0x00261C74`.
This call is indirect and therefore absent from direct-branch and absolute-pointer
searches. The entire initializer and constructor table match the stock image
byte for byte. No native ticket-job or free-campaign function is reclaimed.

| Payload area | Modules | Used end / remaining space |
|---|---|---|
| Text tail `0x0028D2E0–0x0028DFC0`: mapped executable padding | Startup trampoline and automatic-environment `code_expansion.o` | `0x0028D3C0` / `0xC00` bytes |
| Added pages `0x00365000–0x00368000`: extended data, executable after startup | All mode wrappers, native trampolines, and feature objects | `0x00367040` / `0xFC0` bytes |

The startup hook redirects only the call at `0x00100010` to the small loader.
It preserves `r0–r12/LR`, duplicates the current-process pseudo-handle with SVC
`0x27`, queries SVC `0x2A` for the environment, and uses the real handle for
SVC `0x70` (operation `6`, permission `7`) on
the three added pages, closes the handle, restores registers, and continues the
native constructor walker at `0x00102ADC` in Thumb state. Hardware errors stop
through SVC Break. Only the identified-Azahar compatibility case below may
continue without a successful permission change.

The matching ExHeader retains all original segment bases and text/rodata sizes.
It extends data to `0x7C` pages, ending at `0x00368000`, sets BSS size to zero,
and grants only required SVC bits while preserving existing descriptors.
Original BSS is explicit zero-filled image data at unchanged addresses;
`0x00364000–0x00365000` is also a zero-filled RW separator, not an unmapped
guard page. This title already allows `0x23/0x27/0x2A/0x3C`, so only `0x70`
is added. The design follows
[Magikoopa's data-tail expansion](https://github.com/RicBent/Magikoopa/blob/master/MagikoopaUI/patchmaker.cpp),
but retains the actual text size so Luma's LayeredFS injection point does not move.
The added pages load as RW data and alone become executable; old data/BSS and
the rest of the process are not globally made executable.

### Shared BPS loading and automatic environment handling

The BPS source length is `0x22A000`, target length is `0x268000`, and metadata
length is zero. Luma allocates the target range from the expanded ExHeader before
patching; unmodified Azahar's BPS implementation resizes its image to the target
length before reconstructing it. Both obtain the same bytes and map data through
`0x00368000`. The original BSS-clear bounds `0x003293FC–0x003638A4` remain intact.
See [Luma's BPS decoder](https://github.com/LumaTeam/Luma3DS/blob/master/sysmodules/loader/source/bps_patcher.cpp)
and [Azahar's BPS resizing](https://github.com/azahar-emu/azahar/blob/9e6f523a57fac9564ac0bf8286db3c3702d301ec/src/core/file_sys/patch.cpp#L260-L280).

`GetSystemInfo(0x20000, 0)` identifies Azahar only when its result is `0`, output
low word is `2`, and high word is `0`. Luma hardware returns `1` and output `0`,
so it cannot select that branch. The process then calls SVC `0x70` in every
environment. Result `0` succeeds normally. A nonzero result is accepted only
when Azahar was identified and the result equals the input real process handle,
matching its unimplemented handler's unchanged `r0`. Negative errors, unrelated
positive results, unknown environments, and handle creation/closing errors fail.
This is not a generic "ignore SVC errors" policy. A missing-SVC emulator still
relies on permissive instruction fetching; an implemented SVC is used normally.
References: [Azahar's identity query](https://github.com/azahar-emu/azahar/blob/9e6f523a57fac9564ac0bf8286db3c3702d301ec/src/core/hle/kernel/svc.cpp#L1866-L1870),
[unimplemented handler](https://github.com/azahar-emu/azahar/blob/9e6f523a57fac9564ac0bf8286db3c3702d301ec/src/core/hle/kernel/svc.cpp#L2440-L2448),
and [Luma's hardware query](https://github.com/LumaTeam/Luma3DS/blob/master/k11_extension/source/svc/GetSystemInfo.c#L218-L222).

The ticket module remains ARM. Other functional modules use Thumb; shared
TLS/SVC primitives remain non-inlined ARM functions called through literal
addresses. ARM dispatch explicitly switches state with `BX/BLX`, and verification
rejects unsafe immediate Thumb-to-ARM calls produced by the object importer.
The card I/O entries, list vtable slot,
and source-selection comparison are in-place hooks; their native bodies remain
available through trampolines rather than holding new payloads. Native data/BSS
addresses do not move.

The scanner allocates a `0xE60`-byte context from the native heap on demand.
Its pointer uses `0x00363FF0`, and the three mode bytes begin at `0x00363FF4`.
The current ticket-job backend is stored at `0x00363FF8`.
These addresses are after the logical BSS end `0x003638A4` and before the RW
page end `0x00364000`, rather than in the native resource-pointer table at
`0x00329FF8/0x00329FFC`. Source scanning ignores the mode bytes and is shared
by Online and Offline Mode.

The four ticket hooks replace existing four-byte `BL` instructions in place.
The remaining native ticket-state and job bytes are unchanged.

The zero-filled tails in `.rodata` (`0x002EBA58–0x002EC000`, `0x5A8` bytes)
and the code image padding at `0x003293FC–0x0032A000` (`0xC04` bytes) are
non-executable. The latter is the start of the logical runtime BSS, not free
data. Static zeros do not establish the absence of references. Expanding only the ExHeader `.text` size would overlap
the existing `.rodata` mapping. The SVC layout instead extends data, materializes
BSS, and pairs its BPS with an ExHeader and startup permission change, without
relocating the original program.

## Expansion development and approach comparison

### Development sequence

1. **Magikoopa-based hardware expansion.** Materialize original BSS as zeros,
   extend the loaded data pages, place a small bootstrap in the original text
   padding, and make the added pages executable through SVC `0x70`. Existing
   variables stay at their addresses without relocating rodata, data, or stock
   code. The initial IPS failed to load in unmodified Azahar before reaching SVC.
   Reference: [Magikoopa's image/ExHeader preparation](https://github.com/RicBent/Magikoopa/blob/master/MagikoopaUI/patchmaker.cpp).
2. **Implement both missing emulator capabilities.** The personal
   [`svc112` branch](https://github.com/Wokann/azahar/tree/svc112) addresses two
   separate problems: its
   [loader](https://github.com/Wokann/azahar/blob/svc112/src/core/loader/ncch.cpp)
   allocates the complete segment-layout buffer before patching; its
   [SVC dispatch](https://github.com/Wokann/azahar/blob/svc112/src/core/hle/kernel/svc.cpp)
   and [process-memory implementation](https://github.com/Wokann/azahar/blob/svc112/src/core/hle/kernel/process.cpp)
   validate the real process handle, perform the Protect operation needed here,
   and invalidate the code cache. This is not a success-return stub: the loader
   enables IPS placement, while SVC implements the permission change.
3. **Try a separate Citra layout.**
   [oot3d_practice_menu's instructions](https://github.com/gamestabled/oot3d_practice_menu/blob/master/README.md)
   distribute separate patches and ExHeaders for hardware and Citra. Its
   [hardware header](https://github.com/gamestabled/oot3d_practice_menu/blob/master/exheader.bin)
   extends data with zero BSS; its
   [Citra header](https://github.com/gamestabled/oot3d_practice_menu/blob/master/exheader_citra.bin)
   retains original data and enlarges BSS. The resulting experiment here added
   `0x3D000` BSS to the `0x22A000` image, giving the old emulator a `0x267000`
   patch buffer, and used a compile-time permission bypass. The reference's
   [shared loader](https://github.com/gamestabled/oot3d_practice_menu/blob/master/src/loader.c)
   still calls SVC `0x70` but rejects only negative results, so an unhandled
   call retaining a positive handle can continue. This neither implements SVC
   nor means its Citra build macro removes that call.
4. **Unify loading and permission handling.** BPS now reconstructs an image of
   the target length without using additional BSS as an emulator buffer trick.
   Hardware and SVC-capable Azahar perform the permission change normally;
   missing-SVC Azahar continues only under the identity/result conditions above.
   The initializer overwrite and platform-layout switches are removed. Every
   environment uses the same ExHeader and payload layout.

### Why the earlier approaches are not interchangeable

| Project approach | Image loading | Execution of added pages | Applicable environment |
|---|---|---|---|
| Initial hardware IPS | Extended data, zero BSS | SVC `0x70` must succeed | Hardware or `svc112` with both loader and SVC fixes |
| Citra-style IPS experiment | Original data, enlarged BSS for emulator patch buffering | Compile-time bypass; permissive instruction fetching | Emulator only, not hardware |
| Final unified BPS | Extended data, zero BSS; target-length reconstruction | Normal SVC, or narrowly identified Azahar unhandled-call compatibility | Same files for hardware and the two tested Azahar configurations |

Loading and execution are separate gates. Before patching, the old Azahar
[loader](https://github.com/azahar-emu/azahar/blob/9e6f523a57fac9564ac0bf8286db3c3702d301ec/src/core/loader/ncch.cpp)
extends the original image only by BSS, not by the enlarged data-page count;
its [IPS decoder](https://github.com/azahar-emu/azahar/blob/9e6f523a57fac9564ac0bf8286db3c3702d301ec/src/core/file_sys/patch.cpp)
rejects out-of-buffer records. With zero BSS, the hardware IPS therefore fails
to apply before startup. Even after resolving that gate, an unimplemented SVC
does not return normal success and fails the earlier strict startup check.

Conversely, [Luma's loader](https://github.com/LumaTeam/Luma3DS/blob/master/sysmodules/loader/source/loader.c)
allocates the patch buffer from loaded text/rodata/data pages, **excluding extra
BSS**. Enlarged BSS neither provides hardware IPS with that buffer nor loads
file payloads as data; it denotes zero-initialized memory. Hardware also needs
real execute permission, so an emulator bypass remains insufficient even if
loading is solved separately. The headers differ in loading and permissions,
not machine performance, and cannot simply be exchanged.

The final approach handles both gates: BPS reconstructs the full target image,
the expanded-data ExHeader specifies its mapping, and the bootstrap handles
permissions and environment detection. Hardware errors are not ignored, and
the missing-SVC Azahar route does not actually grant execute permission. Exact
result checks and memory boundaries are documented above.

### Operation and scope of the new approach

Build with `make -C mover` and install `code.bps`, `exheader.bin`, and `romfs/`
together. Use `SD:/luma/titles/00040000000C9C00/` with Luma game patching enabled
on hardware, or the user directory's `load/mods/00040000000C9C00/` with a full
`0x800`-byte ExHeader in Azahar. Remove stale `code.ips` and `code.bin` from that
directory. No expansion macro or platform-specific layout is required. The
ticket policy separately enables missing-interface fallback. Its test setting
reuses environment detection but probes ticket replies, not permission results.

This project's new contribution is the unified combination of fixed-address
data-tail expansion, BPS target-length loading, and Azahar identity/unhandled-SVC
result detection, not a new kernel expansion primitive. The underlying mechanisms
have public precedents. This investigation did not establish whether the exact
combination already exists elsewhere, so it does not claim worldwide novelty.
Comparisons of the reference projects come from their public source and ExHeader
fields; they were not run during this investigation. Missing-SVC compatibility
is limited to the tested Azahar behavior, not every Citra fork or a future emulator
that strictly enforces instruction-fetch permissions. An implemented SVC should
continue to use the normal success path.

## Shared Bank language settings

Two call-site hooks share the SD language settings in both modes:

| Hook | Original operation | Replacement |
|---|---|---|
| `0x00240D7C` | Load cached language from game data `+0xD0` before constructing startup views | Read the language fields from an exact-size `sav.bin`, update cached language and kanji flag `+0xD4`, then return the selected ID in `r1` |
| `0x0025C618` | `LanguageSelection_SetSettings` (`0x0022C06C`) after the native language-confirmation callback has rebound resources | Retain that setter, then update the existing SD record through `sav.tmp` |

The startup wrapper preserves `r0`, `r2`, `r3` and the return address. Its native
continuation still calls `0x0022AF28`, which selects both the message archive and
font, and sets the controller's language-ready flag. Missing/unreadable files,
wrong file lengths and IDs outside `1/2/3/4/5/7/8/9/10` leave the cached settings
unchanged; the normal console-language/selection fallback remains reachable.
The Japanese preference is normalized to boolean. The resource-binding function
and the native setter remain intact.

`language_settings.c/.h` describes the two 16-bit fields at `0x28` and `0x2A` in
the serialized `0x200`-byte Bank record. Bank's original checksum validation
(`0x002CB920`) covers only `0x00–0x1F`; its checksum update at `0x002BAF38` uses
the same input. Both exclude language settings. The Mover update changes only
`0x28–0x2B`, retaining `0x00–0x27` and `0x2C–0x1FF` exactly, without rebuilding
transaction metadata or recalculating its checksum.

Startup only reads. A confirmed selection retains the native in-memory setter,
then reads the entire existing file; missing/wrong-size records are not created
or replaced. Identical settings require no write. Updates use the existing
`readCompleteFile`, `writeCompleteFile`, `openArchive`, `pathCommand`,
`renamePath` and `closeArchive` helpers. The order is delete stale `sav.tmp`,
write/flush/close the complete temporary record, delete `sav.bin`, then rename.
Read/archive/delete-temporary/write failures cannot delete the original;
delete-original failure cannot proceed to rename. A failed final rename leaves
the complete temporary record for Bank's existing missing-file recovery.
No original Bank archive is opened, and the Bankdata transaction hooks are
unrelated to these language operations.

## Ticket state and local results

The native ticket state is `0x00249600`; its local substate is at state `+0x10`.
Both modes retain this function, callback `0x0024991C` and cleanup `0x00249CD8`.
Only these interfaces are routed:

| Call site | Native function | Selected local backend: Offline, or test policy `1` with a confirmed missing Azahar interface | Selected native backend: public policy `0`, or other online cases |
|---|---|---|---|
| `0x00249754` | `0x0023D3C0` initialization | Populate local job fields; return `0` on failure | Original call and arguments |
| `0x00249774` | `0x0023E230` polling | Report completion and successful result | Original call and arguments |
| `0x00249818` | `0x0023BE4C` free-campaign request | Deliver the local window through the original campaign callback | Original request and asynchronous callback |
| `0x00249CEC` | `0x0023E63C` unbind | No client was bound; proceed with destruction | Original unbind |

Initialization keeps the job in `r0`, loads shared state `+0x28` into `r1`,
passes mode/policy in `r2/r3`, and tail-calls the C selector. Its native route
passes only the job to native initialization. The backend is selected once and
stored at `0x00363FF8`. Polling, campaign and unbinding use only volatile `r12`
to read that flag; local campaign completion passes the state in `r0`, while
native routes preserve their original arguments. The original `BL` return
address, stack alignment and callee-saved registers are preserved.

| Substate / node | Native flow | Local-result flow |
|---|---|---|
| `0` | Update UI and wait for the next stage | Unchanged |
| `1` | Wait for UI, allocate/construct the `0x158`-byte job, initialize its client | Keep waiting, allocation and construction; replace client initialization only |
| `2` | Poll; on success compute entitlement, copy account/purchase values, set validity and register the campaign callback | Same result consumer and registration; replace polling and campaign request only |
| `6` | Wait for the campaign callback, which advances to `3` | A local window record invokes the same callback synchronously, advancing to `3` |
| `3` | Check expiry/campaign; available advances to `7`, unavailable to `4` | Same decision; valid local dates and the free flag advance to `7` |
| `4/5` | Show unavailable message and wait for confirmation, then error exit | Retained, not forcibly approved |
| `7` | Set completion phase `3` and finish | Unchanged |
| `8` | Set error phase `2` and finish | Out-of-range local dates or failed allocation also use this route |
| Cleanup | Unbind, virtual destruction, clear job pointer, remove callback and release campaign resources | Omit only unbinding an uncreated client; retain all remaining cleanup |

Local values populate the native job, not the shared entitlement outputs:

| Job field | Local value and meaning |
|---|---|
| `+0xB8` current date | Console-current time in the native eight-byte packed layout |
| `+0x70` expiry date | Current date plus `999` days, retaining time of day; not a substitute current time |
| `+0xB0` account fields | Preserve shared `+0x30`; do not generate an account ID |
| `+0x78–0x8B` five purchase counts | `-1`, no local purchase history |
| `+0x04`, `+0xA0` | System-shop result and matched catalog count: `0` |
| `+0xA4` job substate | `0` at initialization, `10` after successful polling |
| `+0xA8/+0xA9/+0xAA` | First-shop flag `0`, expiry-known flag `0→1`, cancellation flag `0` |
| SDK pointers, containers and strings | Keep native constructed values; no ticket network client is created |

Native getters `0x001DB83C` and `0x0023DFA8` consume current/expiry dates and
calculate `999` days and `23976` total hours. The native state writes shared
`+0x3C/+0x40`, and its original validity decision sets `+0x44 = 1`. Annual
purchase count remains `-1`. Local campaign completion encodes the same
current-to-expiry window in the native 20-byte record: four reserved bytes,
two seven-byte calendar dates, and a two-byte CRC. It reuses native CRC
`0x0019BD88` with seed `0xFFFF` and a callback source whose request ID matches
state `+0x44`, then calls unchanged callback `0x0024991C`. The callback retains
request-ID, length, CRC and inclusive date-window validation; it calculates
remaining days/hours and sets shared valid `+0x44` and free `+0x335` to `1`.
No request is sent and no output flag or remaining-time field is manually assigned.
This is the same current-to-`999`-days-later window supplied by Bank, using the
representation required by each native consumer. Each local initialization
rebuilds it from console time without using fixed server campaign dates.
This does not calculate Poke Miles or change Bankdata transactions or Pokemon
validation.

Public releases set `ONLINE_TICKET_CHECK_BYPASS` to `0`: Online Mode uses native
tickets, retaining errors for missing interfaces or failed checks rather than
fabricating results. Policy `1` allows a read-only probe only in identified
Azahar. Only the exact short-success unimplemented reply and successful handle
closure select local results. Full replies, missing tickets, service/transport
errors and unknown responses stay native. Offline Mode is unaffected.
The subproject README documents the request/reply criteria; the probe adds no
ExHeader service permissions.

Policy `1` is for the author's tests, not an online-safety promise. Native cleanup
does not copy the job's current date into Bank's shared-date/mileage path.
Download callback `0x0025C978` loads the server Bankdata body and transaction
descriptor. Commit state `0x0024A0C4` serializes that body; no path was identified
that writes local ticket dates or entitlement display fields into it. Local
results still change entitlement/campaign decisions. NNID, other requests and
server permission are not replaced. Public builds default to `0`, accepting
native errors rather than continuing real online operations with local results.

## Main flow and function addresses

| Address | Function |
|---:|---|
| `0x0019CEE4` | Factory for 19 main state objects |
| `0x00242BA0` | Select the next main state |
| `0x00248C68` | Transfer Box and transfer eligibility state |
| `0x0025C978` | Consume a downloaded complete Bank file |
| `0x0024A0C4` | Transfer, serialize, save, and commit state |

```text
Select game
  → network and transfer eligibility
  → read, filter, and convert candidates from the game
  → download the complete Bank file
  → merge candidates into the Transfer Box
  → serialize the complete Bank file
  → remote stage
  → save the game
  → commit or roll back
```

## Source list and Gen 5 cartridge interfaces

`0x00244D2C` updates the source-list state, and its vtable slot is at
`0x002E6620`. The fields directly involved in enumeration are:

| Object offset | Content |
|---:|---|
| `+0x08` | Gen 5 source/save context |
| `+0x10` | List state number |
| `+0x38` | List UI object |
| `+0x3C` | 40-entry source-ID array |
| `+0x64` | Current source count |
| `+0x66` | Current cursor |
| `+0x68` | Asynchronous-read work area |
| `+0x78` | VC enumerator |

Source IDs `1–4` represent Black, White, Black 2, and White 2; IDs from `5`
enter the VC route. The native list first detects, reads, and validates the
current physical cartridge, then appends VC sources in states 3/4. Its total
capacity is fixed at 40. The selection branch at `0x00244870` distinguishes
Gen 5 from VC with `sourceId < 5`.

The shared scanner can emit up to four Gen 5 sources before VC enumeration.
The hook at the native VC count store `0x0024149C` therefore replays
`strh r0,[r6,#0x58]`, then checks the combined Gen 5 and VC count. At 40 it
causes the native loop to exit after its next increment; below 40 it leaves the
loop state unchanged. This guard runs in both modes and prevents an overrun of
the fixed array.

Gen 5 source reads and the final save use three separate entries:

| Address | Native role |
|---:|---|
| `0x0021A7E0` | Read cartridge-save bytes at an offset |
| `0x0021AA0C` | Read the cartridge game code |
| `0x0021AB50` | Write cartridge-save bytes at an offset |

The native asynchronous save load starts at `0x0019ADEC`, is polled at
`0x0019AD60`, and performs format/redundant-side selection at `0x0019AA30`.
The current patch reuses that upper state and validation. A digital source
selected in either mode redirects the three low-level entries to its SD save.
A physical cartridge in either mode uses exact trampolines that replay the
original prologues and continue through the untouched function bodies. Mode
differences begin only in the later Bank, network, legality-service, and
transaction states.

### Gen 5 game names follow the source language

The native display routine `0x0019B1F0` maps source IDs `1–4` to message IDs
`4–7` in message file `24` of the current UI-language archive. Consequently a
Gen 5 game's title originally follows the UI language. VC IDs already select
language-specific entries from the native 43-element table at `0x002B47E0`.

Only the `BL MoverUi_SetMessageLine` at `0x0019B240` is redirected. Its wrapper
uses the source-list state retained in `r4`, replaces the title ID in `r3`,
restores `r0–r2/LR`, and tail-calls the original text-pane function. Font choice,
width adjustment, the subsequent trainer-name/ID calls, and the original title
table remain untouched.

`NdsSources_TitleMessage` uses the winning source's game code, not a filename or
the currently active I/O backend. SD winners already retain it; valid physical
cartridges additionally read it through `NdsSources_OriginalReadGameCode` after
native discovery. Failure to read or recognize the code falls back to the stock
message without changing source availability. Lookup never allocates a scanner
context and does not read mode storage.

The build copies the four original Gen 5 titles from each `J/O/F/I/D/S/K`
language archive into message file `24` of every UI archive, retaining their
UTF-16 values and flags. Existing IDs `0–53` stay unchanged; new IDs `54–81`
are `54 + languageRank * 4 + sourceId - 1`. The seven source archives are
`a/0/0/5` through `a/0/0/9`, then `a/0/1/0` and `a/0/1/1`. All four body-text
CFNT resources in `a/0/2/3` contain every character required by these 28 names;
the numeric-only fonts are not game-title fonts and are not modified. No global
UI-language switch or new font resource is needed. Both modes use this title
selection, while VC titles and discovery/validation rules remain native.

## Network states

`0x0025C978 MoverDownload_ConsumeBankData` loads the complete file. It first backs up the 30 candidate transfer slots and restores them only when the downloaded Transfer Box is empty.

The state-1 call at `0x00248D3C` is followed by an unconditional transition to state 2, while the asynchronous callback moves the parent state directly to 3 or 7.

| Address | Function |
|---:|---|
| `0x0018AA38` | Prepare and start an HTTP request |
| `0x0018B58C` | HTTP streaming send/receive worker |
| `0x0020815C` | Build a multipart stream containing `name="file"` |
| `0x00208554` | Emit multipart prefix, file body, and suffix |
| `0x00215208` | Send an HTTPC chunk |
| `0x00215308` | Finish the POST body |
| `0x00215328` | Receive the response |
| `0x0025AF1C` | Prepare BankObject GET |
| `0x0025B02C` | Validate response `Content-Length` |
| `0x001F8D5C` | Initialize an HPP POST job |
| `0x001F8EB8` | Process HPP result, redirects, and retries |
| `0x001F9194` | Rebuild a retry request |

The four HPP resources are `CACERT_PUBLIC_CA_5.der` through `_8.der`, not bankdata. A successful low-level HTTPC call does not replace the outer HTTP status, response body, control-plane completion, and asynchronous state transitions.

## Per-slot transfer decisions

### Complete decision layers

A transfer does not run one monolithic “legality check.” It passes through four
layers:

1. The source save must load and pass the native integrity checks; a Gen 5 save
   also selects a usable redundant side.
2. For Gen 5, Online Mode submits all 30 raw BOX1 records, including empty
   slots, before running the 14 local checks or converting transfer records. VC
   converts its candidates earlier. After validating the response, the client obtains
   one 32-bit result code for each of the 30 slots. A code tells the client to
   accept, skip an empty slot, reject on the server's decision, or normalize the
   nickname and/or Original Trainer name. The server's actual checks are not
   present in the client executable; the client only consumes their result codes.
3. Gen 5 candidates allowed to continue by the remote result then run 14 native
   local checks; VC uses a separate, shorter local decision path.
4. Accepted candidates still require a valid local `bankdata.bin` and an empty
   Transfer Box before they can be inserted.

For Gen 5, the confirmed remote-result meanings are: `0` continues, `20` marks
an empty slot, nonzero values below `250` other than `20` reject, and `250`,
`251`, and `252` request nickname, Original Trainer, or both name normalizations. VC uses
`0`, `20`, and the corresponding normalization codes `253`, `254`, and `255`;
other nonzero results reject. The “remote information” here is therefore a
per-slot decision code, not another Pokemon record and not the Gen 5 local
14-check failure mask.

In Offline Mode, this patch replaces only the unavailable layer-2 array: Gen 5
records are read-only classified for integrity: valid empty records receive
`20`, nonempty candidates receive `0`, and explicit failures receive generic
rejection result `1`; VC writes `0`
after its earlier null-object skip. Layers 1, 3, and 4 retain their native flows.
Local validation therefore still runs, but server-only rejections and name
normalizations cannot be reproduced.

### Gen 5 remote and local results

State 4 of decompiled function `0x002455D0` iterates over 30 fixed `0x88`-byte
records. The shared flow object stores signed per-slot results at `+0x48`. Its
essential control flow can be represented as follows:

```c
for (slot = 0; slot < 30; slot++) {
    result = slot_result[slot];

    if (result == 20)
        continue;                       // Native empty slot; no warning
    if (result != 0 && result < 250) {
        mark_remote_rejection();
        continue;
    }

    apply_known_remote_name_fixes(result); // 250, 251, 252
    failures = run_all_14_local_checks(candidate[slot]);
    handle_nonfatal_item_and_name_bits(&failures);

    if (failures == 0)
        convert_and_enqueue(candidate[slot]);
    else
        mark_local_rejection(failures);
}
```

The 14 checks are driven by a function-address/failure-bit table at
`0x002AFFE0`. All checks run before their failure bits are interpreted; the
loop does not stop at the first failure.

| Failure bit | Check entry | Actual local check | Final effect on failure | Patch action |
|---:|---:|---|---|---|
| `0x0001` | `0x00242F8C` | Egg flag must be clear | Fatal; sets the egg error | Unchanged |
| `0x0002` | `0x00243028` | Kyurem must not use a fused form | Fatal; sets its dedicated error | Unchanged |
| `0x0004` | `0x00242FA0` | Held-item presence | Nonfatal; caller clears the bit, reports, then removes the item | Unchanged |
| `0x0008` | `0x00243124` | Original Trainer name check position | **Always succeeds**: only `r0=1; return`; cannot create a local failure | Unchanged |
| `0x0010` | `0x002432E0` | Nickname check position | **Always succeeds**: only `r0=1; return`; cannot create a local failure | Unchanged |
| `0x0020` | `0x00243050` | Species and origin-version combination is allowed | Fatal; generic can't-send result | Unchanged |
| `0x0040` | `0x0024320C` | Event marker matches selected origin Trainer IDs | Fatal; generic can't-send result | Unchanged |
| `0x0080` | `0x002431FC` | Met/acquisition level is nonzero | Fatal; generic can't-send result | Unchanged |
| `0x0100` | `0x002432E8` | Hatch location, acquisition level, and origin generation agree | Fatal; generic can't-send result | Unchanged |
| `0x0200` | `0x00243390` | Non-hatched record meets the species minimum capture level | Fatal; generic can't-send result | Unchanged |
| `0x0400` | `0x00242FB4` | Language ID is supported, with one special-origin exception | Fatal; generic can't-send result | Unchanged |
| `0x0800` | `0x00243018` | Origin version is nonzero | Fatal; generic can't-send result | Unchanged |
| `0x1000` | `0x0024312C` | Legacy origin version agrees with migration/met location | Fatal; generic can't-send result | Unchanged |
| `0x2000` | `0x00243318` | Legacy origin is compatible with the hidden-ability marker | Fatal; generic can't-send result | Unchanged |

The caller explicitly removes failure bits `0x0004`, `0x0008`, and `0x0010`.
Items are removed. The two name bits retain repair branches, but their local
check functions always succeed and therefore do not set those bits. In Online
Mode, remote result `250` requests nickname normalization, `251` requests
Original Trainer normalization, and `252` requests both; this remote behavior
is independent of the two always-successful local functions.

### VC local decisions

VC uses decompiled function `0x002463D8` rather than the 14-entry Gen 5 table.
Its order is:

```c
for (slot = 0; slot < 30; slot++) {
    if (converted_candidate_is_null(slot))
        continue;
    if (source_had_item[slot]) {
        mark_item_rejection();
        continue;
    }
    if (source_was_egg[slot]) {
        mark_egg_rejection();
        continue;
    }

    result = slot_result[slot];
    if (result == 20)
        continue;
    if (result != 0 && result != 253 && result != 254 && result != 255) {
        mark_remote_rejection();
        continue;
    }
    if (result == 254 || result == 255)
        normalize_original_trainer_name();
    if (result == 253 || result == 255)
        normalize_nickname();
    enqueue_converted_candidate(slot);
}
```

VC therefore checks null objects, held items, and eggs locally. Broader legality
decisions and name-normalization results `253–255` come from the remote service.
It does not run the Gen 5 14-entry local table.

### What this patch changes

Online Mode submits all 30 candidates and, after validating the reply,
replaces the complete result array. Source accessor `0x0019A6D8` directly
locates BOX1 in the selected save side without normalizing empty records.
`0x002B3FD4` is the template used to clear successfully transferred source
slots, not a unique representation of every input empty slot.

At the Gen 5 remote-request site `0x00245728`, Offline Mode performs this
read-only classification:

1. Retain the preceding native save-block validation and redundant-side
   selection. An unavailable source sets read-error substate `13` and returns
   through `0x00246368` for the frame. The native error message, acknowledgment,
   and exit path then run instead of candidate conversion.
2. Read the native PID, control flags, checksum, and 64 body words. The native
   exclusion bit `0x04` produces rejection result `1`.
3. Honor decoded-body control bit `0x02`. Otherwise seed the decoder with the
   stored checksum, update `seed = seed * 0x41C64E6D + 0x6073` with 32-bit
   wraparound for each word, and XOR with `seed >> 16`. Only temporary values
   are decoded; source bytes, control flags, and checksums are not rewritten.
4. The low 16 bits of the sum of the decoded words must match the stored
   checksum, or the result is `1`. Record checksums and save-block checksums
   are independent; the patch does not repair either checksum.
5. Use `(PID >> 13) & 31` with native block-order table `0x002B42E4` to locate
   the species. A checksum-valid species `0` receives `20`; species `1–649`
   receive `0`; other species receive `1`. An empty slot need not match a
   complete byte template or have every remaining field zero. Level, language,
   origin-version, and name rules are not applied before the empty-slot skip.
6. Replace all 30 results before resuming native processing at `0x00245800`.
   Result `1` is only a local generic rejection value; it does not reproduce
   a specific server error code or the server's complete legality algorithm.

### Online and offline execution order

| Step | Gen 5 Online Mode | Gen 5 Offline Mode |
|---|---|---|
| Load source | Native read, save-block validation, redundant-side selection | Unchanged |
| Prepare 30 slots | Copy raw `0x88`-byte records, including empty slots | Read-only integrity and species classification of the same buffer |
| Obtain per-slot results | Send request, wait for and validate reply, consume 30 results | Fill `0`, `20`, or `1`; source failure follows the native error path |
| Begin postprocessing | Clear in-memory candidate slots, warnings, and source-slot mapping | Resume the same native entry |
| Consume results | Skip `20` without warning; reject failures; continue allowed records | Same branches |
| Prepare nonempty record | Copy to temporary storage, apply remote name fixes, ensure decoding and block locations | Native copying/decoding retained; no name-fix codes generated |
| Local decisions | Run all 14 checks and interpret the combined failure mask | Unchanged; see the complete table above |
| Convert and enqueue | Convert successful records, write in-memory transfer slots and source mapping | Unchanged |
| Confirm and save | Display warnings/confirmation, then run the native save transaction | Retain the existing local bankdata transaction; classification never edits source records |

Result `0` permits further checks; it is not final transfer approval. Rejected
records never enter the successful source-slot mapping. A can't-send warning
excludes a candidate from this transfer, not from its source save. Successfully
enqueued candidates are handled later by the native confirmed-transfer save
flow.

The remaining adaptations are:

- At the VC remote-request site `0x002460B8`, Offline Mode writes `0` to all 30
  results and resumes at `0x002461D0`. VC already skips null candidates before
  consulting the result, so a synthetic result `20` is unnecessary.
- Both offline paths replace all 30 entries every time, preventing values from
  an Online Mode attempt in the same process from surviving a title-screen
  mode switch.
- Online Mode returns to the original remote-request code. The 14 Gen 5
  functions, failure-bit postprocessing, VC null/item/egg logic, and candidate
  insertion are unchanged.

Offline Mode retains every native local check and adds locally verifiable
record integrity and empty-slot classification. Unknown server-only rejection
rules and name normalization are not recreated. Clearing stale remote
transaction parameters is session-state isolation, not a Pokemon legality
check.

## BankObject and bankdata

The Mover Bank object vtable is `0x002E6DD0`. File data starts at object `+8` and has a fixed length of `0xBB518`.

| Vtable slot | Address | Function |
|---:|---:|---|
| `+0x08` | `0x0025AE44` | Serialize the complete file |
| `+0x0C` | `0x0024D868` | Load the complete file |
| `+0x10` | `0x0025AE38` | Return `0xBB518` |
| `+0x18` | `0x0024D898` | Load and upgrade the `0xACA48` legacy format |
| `+0x1C` | `0x0025AE20` | Check `u16(data+0x15C)==2` |

Mover and Bank use the same complete file format. The current format recognized by the program has length `0xBB518`, version `2`, and box count `100`.

### Transfer Box

| File offset | Length | Content |
|---:|---:|---|
| `0x0AAF14` | `30 × 0xE8` | 30 transfer records |
| `0x0AD5FC` | `30` | 30 parallel tags |

| Address | Function |
|---:|---|
| `0x0019A224` | Load one transfer record and tag |
| `0x0019A6F4` | Write a record and tag, then run the original normalization |
| `0x0019A7E0` | Clear a record and tag |
| `0x0019A858` | Test whether a slot contains a valid record |
| `0x0024D624` | Count the 30 occupied slots |

No separate per-record server ID or signature generator was identified for the Transfer Box. The original program processes both the `0xE8` record and its one-byte tag. `0x0025C978` backs up candidate slots, loads the complete object, and restores the candidates when the downloaded Transfer Box is empty.

## Save

`0x0024A0C4` serializes the complete `0xBB518` object before remote update, game save, and commit/rollback.

| Address | Original behavior |
|---:|---|
| `0x0024A240` | Start remote staging and wait for its asynchronous result |
| `0x0024A3E4` | Start remote commit after the game save succeeds |
| `0x0024A420` | Start remote rollback after the game save fails |

All three branches advance the outer state machine through asynchronous completion flags. Mover serializes and uploads the same complete file format used by Bank.
