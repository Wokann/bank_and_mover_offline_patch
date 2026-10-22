# Poke Mover Code Analysis

This document analyzes the supported stock binary and compares its execution
order with the offline adaptation. See [`../README.md`](../README.md)
for build instructions and transaction rules.

## Target and memory layout

- Title ID: `00040000000C9C00`
- Version: v1.5
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
The loader and ticket compatibility policy share the environment-identification helper;
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
| Text tail `0x0028D2E0–0x0028DFC0`: mapped executable padding | Startup trampoline and automatic-environment `common/fs_system/runtime_environment.o` | `0x0028D56C` / `0xA54` bytes |
| Original RW-page padding `0x00363FE0–0x00363FF0`, after logical BSS | Environment/SVC/ticket capability and ticket-result cache | 16 bytes; does not overlap native objects or mode storage |
| Added code `0x00365000–0x0038E000`: extended data, executable after startup | Wrappers, native trampolines, features, fakeServer and async transport | Used end varies by build; check `CombinePatch_OfflinePayloadUsedEnd` against `CombinePatch_CodeEnd` |
| Context `0x0038E000–0x00492000` | Service data and separate 1 MiB I/O workspace | `0x104000` reserved, ARM object `0x101660`, rules, word pools and parsing workspaces allocated on demand; final byte is a ticket-job marker, RW |
| Worker control `0x00492000–0x00493000` | Platform, task and UI-side request registry | ARM size `0x9D8`, RW |
| Task body `0x00493000–0x00593000` | Upload/download and NDS path copies | 1 MiB, RW; independent of SDK object destruction |
| Worker stack `0x00593000–0x005A3000` | Service thread entered through native SDK TLS trampoline | 64 KiB, RW; main-thread stack unchanged |
| Local source control `0x005A3000–0x005A4000` | Original tail holds `FsLocalSource` | ARM object `0xAE8`, RW, not an unmapped guard page |
| Local Gen5 image `0x005A4000–0x00624000` | Independent complete working image | 512 KiB RW; no Bank/task-buffer alias |

The startup hook redirects only the call at `0x00100010` to the small loader.
It preserves `r0–r12/LR`, duplicates the current-process pseudo-handle with SVC
`0x27`, queries SVC `0x2A` for the environment, and uses the real handle for
SVC `0x70` (operation `6`, permission `7`) on
only the 41 code-reservation pages, leaving the service and worker runtime RW; it closes the handle, restores registers, and continues the
native constructor walker at `0x00102ADC` in Thumb state. Hardware errors stop
through SVC Break. Only the identified-Azahar compatibility case below may
continue without a successful permission change.

The matching ExHeader retains all original segment bases and text/rodata sizes.
It extends data to `0x338` pages, ending at `0x00624000`, sets BSS size to zero,
and grants only required SVC bits while preserving existing descriptors.
Original BSS is explicit zero-filled image data at unchanged addresses;
`0x00364000–0x00365000` is also a zero-filled RW separator, not an unmapped
guard page. This title already allows `0x23/0x27/0x2A/0x3C`, so only `0x70`
is added. The design follows
[Magikoopa's data-tail expansion](https://github.com/RicBent/Magikoopa/blob/master/MagikoopaUI/patchmaker.cpp),
but retains the actual text size so Luma's LayeredFS injection point does not move.
The added pages load as RW data; only 41 code pages become executable. The service/worker runtime, old data/BSS and
the rest of the process are not globally made executable.

### Shared BPS loading and automatic environment handling

The BPS source length is `0x22A000`, target length is `0x524000`, and metadata
length is zero. Luma allocates the target range from the expanded ExHeader before
patching; unmodified Azahar's BPS implementation resizes its image to the target
length before reconstructing it. Both obtain the same bytes and map data through
`0x00624000`. The 1 MiB I/O workspace and task body have separate ownership;
the worker has its own stack. Main-thread stack size and original BSS-clear
bounds `0x003293FC–0x003638A4` remain intact.
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

The scanner allocates a `0xF40`-byte context from the native heap on demand.
Its pointer uses `0x00363FF0`, and the three mode bytes begin at `0x00363FF4`.
The current ticket-job backend is stored at `0x00363FF8`.
These addresses are after the logical BSS end `0x003638A4` and before the RW
page end `0x00364000`, rather than in the native resource-pointer table at
`0x00329FF8/0x00329FFC`. Source scanning ignores the mode bytes and is shared
by Online and Offline Mode.

Ticket initialization, resources, system binding/unbinding and send hooks replace
four-byte `BL` instructions in place. Native polling, parsers, consumers and
destruction remain; job bodies do not hold new payloads.

The zero-filled tails in `.rodata` (`0x002EBA58–0x002EC000`, `0x5A8` bytes)
and the code image padding at `0x003293FC–0x0032A000` (`0xC04` bytes) are
non-executable. The latter is the start of the logical runtime BSS, not free
data. Static zeros do not establish the absence of references. Expanding only the ExHeader `.text` size would overlap
the existing `.rodata` mapping. The SVC layout instead extends data, materializes
BSS, and pairs its BPS with an ExHeader and startup permission change, without
relocating the original program.

## Shared Bank language settings

Bank's existing console save is the language authority; Mover does not initialize
a separate native record. Startup reads Bank's valid `0x200`-byte record before
binding resources. Missing or invalid native records retain the original fallback.

| Hook | Current operation |
| --- | --- |
| `0x00240F38` | Poll the task-owned native settings snapshot before startup initialization. |
| `0x00241024` | Cancel undelivered startup results before native destruction. |
| `0x0025C618` | Apply the original language setter and retain numerical language fields only. |
| `0x002470F4` | During the animated title-return state, save Bank-native language, then synchronize SD. |
| `0x002472E8` | Cancel outstanding delivery before the return state's original destruction. |

The language view closes before file I/O. The worker opens Bank's permitted
save archive `0x567890B4`, path `[1, 0xC9B, 0]`, and `/turtle`. It checks the
native marker and CRC16, modifies only the two 16-bit fields at `0x28/0x2A`,
commits, closes, reopens and verifies the complete record. Only then may it update
the selected user's `sav.bin`. Genuine SD absence permits copying the full native
record; an existing SD record receives language fields only.

The native CRC16 covers the first `0x20` bytes, using initial `0xFFFF` and polynomial
`0x1021`, without reflection or final xor. Language is outside that covered region.
The ExHeader adds only Bank's packed save ID to the permitted-save list, preserving
other entries and permissions. I/O errors do not authorize initialization or formatting.
Native/SD saves are not a cross-storage atomic operation; a failed SD sync does not
undo a successful native commit. Identical values require no write.

## Title mode help and native hints

The current mode is a single-line label immediately above START. R-mode,
L-language and HOME hints remain three independent lines below it, centered at
lower-screen Y `156/182/208`; mode and START are at `88/120`.
All positions are integer pixels with native `15×19` font sizing. START and
copyright stay in place. The original HOME wording is not shortened.

Offline title user selection follows the shared registry: auto first, then
ascending actual numbers, without wrapping. D-pad/Circle Pad up/down supports
hold/repeat acceleration, not C-stick. Browsing is in memory; the final selection
is published in the background before entering. Mover never provisions a missing
Bank or user directory that would prevent Bank's first migration.

The native name-warning flag is independent of this title layout and of name
screening; see [per-slot decisions](#per-slot-transfer-decisions).

## Ticket state and local results

The native ticket state is `0x00249600`; its local substate is at state `+0x10`.
Both modes retain this function, callback `0x0024991C` and cleanup `0x00249CD8`.
These job interfaces are routed:

| Call site | Native function | Selected local backend: Offline, or automatic policy `1` with a confirmed missing Azahar interface | Selected native backend: native-only policy `0`, or other online cases |
|---|---|---|---|
| `0x00249754` | `0x0023D3C0` initialization | Record backend selection; both backends run original initialization | Original call and arguments |
| `0x00249774` | `0x0023E230` polling | Original call retained; SDK parsing and polling decide completion/failure | Original call and arguments |
| `0x00249818` | `0x0023BE4C` free-campaign request | Deliver the local window through the original campaign callback | Original request and asynchronous callback |
| `0x00249CEC` | `0x0023E63C` unbind | Outer call retained; internal system unbind selects backend; destruction remains | Original unbind |

Three additional UI branches check only whether the session is Offline:
`0x00249650` skips applet screen clearing, `0x002496CC` skips its fade, and
`0x00249704` skips repeated clearing/fade preparation. Offline substates still
advance, construct the job and retain callback removal, destruction and resource
release. Online Mode replays the original instructions and calls at all three
sites, even when compatibility policy selects local ticket replies. Other ticket-state
bytes remain native outside backend selection, resource/unbind calls and three UI
hooks. Inside the job, only system binding/unbinding and send provider `0x001D17E0`
adapt the backend. Native SDK construction, operation 10/1/5/6 parsers, polling
and destruction remain.

Initialization keeps the job in `r0`, loads shared state `+0x28` into `r1`,
passes mode/policy in `r2/r3`, and tail-calls the C selector. Its native route
passes only the job to native initialization. The backend is selected once and
stored at `0x00363FF8`. Sending, campaign and unbinding use only volatile `r12`
to read that flag; local campaign completion passes the state in `r0`, while
native routes preserve their original arguments. The original `BL` return
address, stack alignment and callee-saved registers are preserved.

| Substate / node | Native flow | Local-result flow |
|---|---|---|
| `0` | Update UI and wait for the next stage | Offline skips applet clearing but advances the native substate; online ticket compatibility keeps native UI |
| `1` | Wait for UI, allocate/construct the `0x158`-byte job, initialize its client | Offline skips applet fades; allocation, construction and initialization remain; only system provider binding is adapted |
| `2` | Poll; on success compute entitlement, copy account/purchase values, set validity and register the campaign callback | Original SDK parsing/polling; replace only send provider and campaign-body source |
| `6` | Wait for the campaign callback, which advances to `3` | A local window record invokes the same callback synchronously, advancing to `3` |
| `3` | Check expiry/campaign; available advances to `7`, unavailable to `4` | Same decision; valid local dates and the free flag advance to `7` |
| `4/5` | Show unavailable message and wait for confirmation, then error exit | Retained, not forcibly approved |
| `7` | Set completion phase `3` and finish | Unchanged |
| `8` | Set error phase `2` and finish | Out-of-range local dates or failed allocation also use this route |
| Cleanup | Unbind, virtual destruction, clear job pointer, remove callback and release campaign resources | Omit only unbinding an uncreated client; retain all remaining cleanup |

Original parsers consume service response bytes; the adapter does not populate job
or shared entitlement fields. The table describes native consumption with a valid
default configuration, not a client-side assignment list:

| Job field | Local value and meaning |
|---|---|
| `+0xB8` current date | Console-current time in the native eight-byte packed layout |
| `+0x70` expiry date | Configured expiry, default `2050-12-31 23:59:59`; not a substitute current time |
| `+0xB0` account fields | Preserve shared `+0x30`; do not generate an account ID |
| `+0x78–0x8B` five purchase counts | Trial/annual counts from the shared service, default `0`; other three are `-1` |
| `+0x04`, `+0xA0` | System-shop result and matched catalog count: `0` |
| `+0xA4` job substate | `0` at initialization, `10` after successful polling |
| `+0xA8/+0xA9/+0xAA` | Set by native catalog/expiry/cancellation handling, not the adapter |
| SDK pointers, containers and strings | Native initialization/request construction/consumption/destruction; only real network-client binding is omitted |

Native getters `0x001DB83C` and `0x0023DFA8` consume actual current time and
configured expiry to calculate remaining days and total hours. The native state writes shared
`+0x3C/+0x40`, and its original validity decision sets `+0x44 = 1`. Annual
purchase count comes from the service, default `0`. Local campaign completion encodes the configured
window as the native 20-byte `FREE` body: four identifier bytes,
two seven-byte calendar dates and a two-byte CRC, all encoded by the independent
server. The client requests the resource and supplies a source whose request ID matches
state `+0x44`, then calls unchanged callback `0x0024991C`. The callback retains
request-ID, length, CRC and inclusive date-window validation; it calculates
remaining days/hours and sets shared valid `+0x44` and free `+0x335` to `1`.
No request is sent and no output flag or remaining-time field is manually assigned.
Both titles share a policy whose default window is `2023-02-14 00:00:00` through
`2050-12-31 23:59:59`, matching the verified official `FREE` resource.
Actual current time is retained separately. The automatically created `server.conf`
can set fixed dates and free/purchased status, preserving existing values;
no separate SD campaign body is read. No entitlement is a normal response with a
zero expiry date. Mover passes minimum days `0` to polling, so request completion
alone does not establish entitlement; its later state checks native expiry/campaign
results without client validity overrides. Basic configuration damage repairs to defaults;
missing/malformed optional checks, words and gifts add no extension. Real file I/O failures
and invalid existing transaction/claim records still use native request failure.
Bank retains its 15-day minimum and original shop branch for
insufficient entitlement. Its local replacement publishes the shared purchase configuration
and then retains native expiry re-query; no actual eShop purchase occurs. Mover has no
new purchase UI: its native minimum-0 path consumes the shared updated configuration.
Local pass dates use `paid_periods`; `purchased=false` ignores residue. Independent boolean switches
are `free_enable`/`trial_enable`; date tables contain only comma-separated ranges without `mode`.
Matching regional keys replace the common key; legacy date toggles are removed after migration.
Bank retains the native first-use conditions for its one-time 30-day trial;
`trial_claimed` is boolean and annual counts stay numeric. Annual passes remain available.
Free campaigns and passes coexist without extending passes. `shop.$currency` tuples retain manual
price/credit amounts; only balances change. Automatic currency updates `currentCurrency`, manual
mode uses its configured value, and unsupported locations are not guessed.
Known missing/empty/invalid settings are repaired in schema order; misplaced valid fields return
to that order without changing their values. Four bilingual blocks with blank separators share
one `[ticket]` section; valid manual components remain.
Existing BIN formats stay unchanged. Mode changes preserve local pass dates and counts. Confirmation
publishes dates/counts, then the original consumer re-queries history and expiry, without assigning validity.
This does not calculate Poke Miles or change Bankdata transactions or Pokemon
validation.

Native-only builds set `ONLINE_FAKE_SERVER_TICKET_FALLBACK` to `0`: Online Mode uses native
tickets, retaining errors for missing interfaces or failed checks rather than
fabricating results. Source-default policy `1` permits fallback only in identified
Azahar. Only the exact short-success unimplemented reply and successful handle
closure select local results. Full replies, missing tickets, service/transport
errors and unknown responses stay native. Offline Mode is unaffected.
Online fallback uses an independent read-only backend-2 policy, with the fixed window
`2000-01-01 00:00:00..2099-12-30 23:59:59` shared by ticket expiry and campaign responses.
Offline defaults and native expiry checks are unchanged, ignoring offline configuration,
pass dates, purchase counts and custom campaigns. Catalog/history vectors are empty and native
count slots stay `-1`. The campaign uses the fixed free window; purchases and configuration
writes are unsupported. Backend bytes `0/1/2` denote native, configured offline and fixed-free online.
The shared runtime module caches environment/SVC capabilities at startup and
ticket capability once at title initialization after services/TLS are ready.
The 16-byte cache is `[0x00363FE0, 0x00363FF0)`, outside native BSS; later jobs
only read it. Detection is independent of policy; `0` never selects local online
results. Shared `FakeServerClient_Ticket` observes console time and requests
the server response; the adapter only copies that response into native fields.
There is no separate Mover calendar or fabricated entitlement policy.
The subproject README documents the criteria; no ExHeader service permissions are added.

Automatic fallback is not an online-safety promise. Native cleanup
does not copy the job's current date into Bank's shared-date/mileage path.
Download callback `0x0025C978` loads the server Bankdata body and transaction
descriptor. Commit state `0x0024A0C4` serializes that body; no path was identified
that writes local ticket dates or entitlement display fields into it. Local
results still change entitlement/campaign decisions. NNID, other requests and
server permission are not replaced. Policy `0` remains available to require
native tickets rather than continuing real online operations with local results.

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

Selection update `0x002445A4` is distinct from enumeration `0x00244D2C`.
For an SD selection, substate 9 submits a copied path to background `ObserveSource`.
CRC32 observation of the complete `0x80000`-byte save runs alongside the native
fade. Only the native fade-completion call at `0x0024483C` waits for observation
before the existing ID dispatch at `0x00244870`; the entire UI update is no longer
held. Native tables, results and subsequent loading remain. Fade retries do not repeat observation;
cancellation cannot deliver into a new selection, and failure cannot authorize
staging. Physical cartridges and VC do not gain this file observation.

Native NDS I/O runs inside a save-job thread. Constructor `0x001C181C` installs
vtable `0x002E654C`, whose `+0x0C` entry is `0x00243510`.
`0x001E39D4 → 0x001E3F58 → 0x0010E250` creates a thread (SVC `0x08`);
TLS trampoline `0x0011F100` calls job body `0x0011C678`, which invokes vtable
`+0x0C` at `0x0011C6F8`. That job calls `0x0021A7E0/0x0021AB50`. A blocking
lock in those synchronous range APIs therefore does not establish UI-thread I/O,
nor permit changing their return ABI to asynchronous polling. This evidence is
the original binary call chain, not hardware scheduling or disk timing.

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

In Offline Mode, the fake server supplies the layer-2 response array. The client
observes records and names without editing them; the service classifies structure,
empty slots and species range, then applies optional Boolean-expression rules and name words.
Valid empty slots receive `20`, structural/rule failures receive `1`, and other
records receive `0` or a native name-normalization code. Missing optional
configuration adds no rule/word hits. Layers 1, 3 and 4 remain native. These optional
policies do not reconstruct the official server's complete checks or word list.

### General name warning versus a detected name violation

Native prompt state `0x00247304` first displays message-file 23, line `7`, explaining
transfer and held items. Substate `2` checks runtime Bank-object byte `+0x182`.
Its serialized body starts at object `+8`, so this is file offset `0x17A` in
`bankdata.bin`, not file offset `0x182`. When the warning has not been shown,
it sets that byte to `1` and displays line `16`, the
general warning that names will change **if** they contain prohibited words. This
branch does not inspect words, per-slot decisions or name-change flags. The byte
means the warning has been shown, not that a prohibited name was detected.

Native full-body serialization includes this flag. The fake server stages that
same body in `bankdata.tmp` and publishes it as `bankdata.bin` on commit.
The Bank backup remains the pre-write generation after transaction completion.
Download/reload, display-metadata updates and configuration reload do not rewrite
this byte. Bank's native download callback also preserves it while adjusting
neighboring session flags. A normal completed save therefore suppresses message
`16` on the next use of the same Bank. A memory-only warning, cancellation before
commit, rollback to a pre-warning version, or importing another Bank with the flag
at `0` may show it again; hiding a prompt must not publish an extra Bank update.

Actual detection notifications are later, in substate `10` (`0x00247884`): shared
byte `+0x1C7` triggers line `24` about a detected nickname, and `+0x1C8` triggers
line `25` about an Original Trainer name. Each flag is consumed once. The native
Gen 5 postprocessor still consumes `250–252` and its fourteen local checks; the
patch does not force or suppress name flags in the UI. This prompt state's machine
code and all three message lines remain unchanged.

### Gen 5 remote and local results

With the `BanUnicodeChinese` Pokemon rule enabled and Han not permitted, a Gen5 nickname or
OT containing a restricted Han character returns per-slot rejection `1` before
word/exception matching, not `250–252`. `allowFanTranslationCHN` implicitly permits
Han. This follows the user's hardware observation; `1` is a native-recognized
generic rejection, not a claim about an exact proprietary server reason code.

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
   continue to optional policy; other species receive `1`. An empty slot need not match a
   complete byte template or have every remaining field zero. Level, language,
   origin-version, and name rules are not applied before the empty-slot skip.
6. Valid nonempty records supply form, native level, moves, shiny status, OT/TID/SID,
   language, origin, nickname flag, met/transfer level and fateful-encounter flag
   to CheckPokemon. PK5 met level is `0x84 & 0x7F`, and fateful encounter is
   bit 0 of `0x40`. These are read-only observations, not client-side rule decisions.
   Combined rules reject
   with `1`; otherwise word hits return native `250–252` name fixes or `0`.
   Missing configuration adds no policy.
7. Replace all 30 results before resuming native processing at `0x00245800`.
   Result `1` is only a local generic rejection value; it does not reproduce
   a specific server error code or the server's complete legality algorithm.

### Online and offline execution order

| Step | Gen 5 Online Mode | Gen 5 Offline Mode |
|---|---|---|
| Load source | Native read, save-block validation, redundant-side selection | Unchanged |
| Prepare 30 slots | Copy raw `0x88`-byte records, including empty slots | Read-only integrity and species classification of the same buffer |
| Obtain per-slot results | Send request, wait for and validate reply, consume 30 results | Integrity/empty classification, then optional rules/words; source failure follows native errors |
| Begin postprocessing | Clear in-memory candidate slots, warnings, and source-slot mapping | Resume the same native entry |
| Consume results | Skip `20` without warning; reject failures; continue allowed records | Same branches |
| Prepare nonempty record | Copy to temporary storage, apply remote name fixes, ensure decoding and block locations | Same native name-fix branch consumes local codes; source records are not directly renamed |
| Local decisions | Run all 14 checks and interpret the combined failure mask | Unchanged; see the complete table above |
| Convert and enqueue | Convert successful records, write in-memory transfer slots and source mapping | Unchanged |
| Confirm and save | Display warnings/confirmation, then run the native save transaction | Retain the existing local bankdata transaction; classification never edits source records |

Result `0` permits further checks; it is not final transfer approval. Rejected
records never enter the successful source-slot mapping. A can't-send warning
excludes a candidate from this transfer, not from its source save. Successfully
enqueued candidates are handled later by the native confirmed-transfer save
flow.

The remaining adaptations are:

- At the VC remote-request site `0x002460B8`, native accessors inspect converted
  records: empty `20`, integrity/exclusion/policy rejection `1`, otherwise `0`
  or native `253–255` name-fix codes. Resume at `0x002461D0`; native null and
  local-decision branches remain. Converted species `1–255` may enter policy,
  avoiding an extra ban on Gen 2 `252–255`; Gen 1 native conversion/local limits
  remain effective. PK7 met level is `0xDD & 0x7F`; fateful encounter is bit 0 of `0x1D`.
- Both offline paths replace all 30 entries every time, preventing values from
  an Online Mode attempt in the same process from surviving a title-screen
  mode switch.
- Online Mode returns to the original remote-request code. The 14 Gen 5
  functions, failure-bit postprocessing, VC null/item/egg logic, and candidate
  insertion are unchanged.

Offline Mode retains every native local check and adds locally verifiable
record integrity and empty-slot classification. Unknown server-only rejection
rules are not recreated. Optional combined rules and words are administrator
policy, not a complete official v1.5 implementation; word hits request native
name correction/prompts, not rejection. Clearing stale remote
transaction parameters is session-state isolation, not a Pokemon legality
check.

Service rules use only enabled/action/condition fields, semicolon terminators,
multiline whitespace and # comments. Cross-field &&/|| and parentheses compile to
an iterative RAM program at load. Require applies within the species-only projection
of its own expression; unrelated species skip it. Rules cannot bypass integrity or
the 14 checks above. Group/allow and standalone predicate keys are removed. Words use
heap-backed variable records with no fixed length/count/pool cap, defaulting to all 4016 public 3DS v18 expressions plus documented exceptions
and exact ten-language species-name nickname presets (NDS 001–649, VC 001–251).
Groups are Japanese, English, French, Italian, German, Spanish, Latin American Spanish, Korean,
Simplified Chinese and Traditional Chinese, then National Dex within each. Official previous game
spellings remain, not regional old translations; the stock nickname flag is unchanged.
OT and legality are not exempt; both sources merge to 9268 records / 226572 logical bytes.
The ARM policy header is 84 bytes; words, hash slots, dynamic lines and iterative parse/evaluation buffers
are separately heap-allocated. Invalid/unsupported entries are skipped individually without discarding valid ones.
Rules are separately allocated at 40 bytes each on ARM, plus dynamic expression programs
and reusable evaluation space, without a fixed section cap. Observations remain 76 bytes. No binary configuration
snapshot is read/written. New loads use current text, while delivered results and staged
bodies are not reevaluated; the journal uses v5. VC names are observed as UTF-16 from
stock-converted PK7 candidates, not raw legacy bytes. Gen5 uses raw 16-bit PK5 fields.
The load-only dynamic index/line/parser scratch is released on success/error; matching workspace
is reused until reload/release. Disabled/missing lists allocate no word pool/index.
Native name/view field boundaries and 32-bit overflow checks remain; real I/O/heap failures are errors. See
[templates, license and evidence limits](../../common/docs/fakeServer/architecture.zh-cn.md#mover-可选判定和词表).
Compiled ARM checks cover 32 block-order rows, encrypted/decoded records and VC
observation APIs; they do not prove final transfer of actual glitch saves or
complete official legality behavior.

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

### Offline local transaction journal

Independent common/src/fakeServer/transaction.c coordinates
Bank, NDS source and SD Turtle through `sd:/3ds/Bank/fakeServers/users/<userID>/transaction.bin`. fs_helpers.c uses
Mover SDK addresses. See [Bank format](../../bank/docs/code-analysis.md#local-journal-format-and-recovery-evidence).

`0x0024A0C4` routes both modes through save accounting, then replays the native
prologue/resumes `0x0024A0C8`. Offline must Finish before substate 16 success
text; Online never substitutes local Bank. Native begin `0x0024B92C` and poll
`0x0024B868` each replace one entry instruction; trampolines replay
`push {r4-r6,lr}`/`push {r3-r5,lr}` and resume untouched bodies.

Physical/VC save begin/poll no longer request local file permission. Only a real
first-save success (`5→10`) hands off one durable SOURCE_SAVED acknowledgement,
before the original substate-10 commit request. Failure cannot advance commit.
SD publication already supplied that acknowledgement; online stays native.
An online SD save may not overwrite a pending offline Bank reservation. Existing
cleanup-start/success/Finish checkpoints remain.

The local backend loads a complete selection image; the native loader then uses
RAM. Save begin rechecks readable/writable access and identity. x.bak retains
the old image; native segmented I/O modifies separate RAM. Save completion writes
and reads back the complete x.tmp. Both first/cleanup saves require
complete-image native redundant-face validation, expected Box 1/decoded
transaction contents, and matching memory/file CRC32 before sealing.
Native job success alone does not publish unverified work.

Those local decisions belong to `common/src/fs_system/source_image.c`, not a
remote endpoint. It shares scheduling/durable checkpoints with the server, never
opening policy or scanning Bank for segmented I/O. Staging compares its captured
before-image with that selection before persisting preparation. The client
retains native decoding, redundant-face checks and expected-edit validation.
Native clients instead query descriptors and display metadata through
`QueryRemoteStatus`/`QueryMetadata`, then send the result of their existing
transaction branches to `RequestTransaction`. No custom resume permission or
native transaction-block fingerprint is added.

`SettledSource` returning no matching completed history is not a failed
observation of the current selection. Native zero-token recovery also reaches
this completion boundary. On `FS_MISSING`, retain any already-validated SD
selection fingerprint and its valid marker; do not fabricate an observation
if none exists. Only a successful complete `0x80000`-byte source fingerprint
replaces it. Journal read/validation failures and incomplete successful replies
still take the error route. Each new selection resets the settlement-query
marker rather than reusing another game's completion. `BeginSourceSave` still
rechecks the file against the selected fingerprint, and native saving,
whole-work-image validation and publication order remain required.

An ended transaction at the same `.sav` path must not overwrite a fresh selection
after a normal game save or manual import. `SettledSource` confirms only a
completed image matching the supplied fingerprint; mismatched or absent
observations return `FS_MISSING`, without changing files or rebuilding a
transaction. After an actual successful save, completion queries use this save's
validated working-image fingerprint. New selections and save starts clear the
old working-image marker. This compares existing fingerprints without another
whole-save read/hash; pre-write rechecks and pending-transaction protection remain.

Offline seals first-save work, records STAGED and publishes the validated NDS
source so native loading can observe state 2. Bank stays old until the client's
commit request records COMMIT_DECIDED and publishes it as PUBLISHED.
Cleanup recopies the first image, uses native setters/
saving, seals CLEANUP_READY, publishes/verifies cleanup, retires NDS sibling recovery files and persists COMPLETED before
success. Online SOURCE_ONLY validates the complete image and records
COMMIT_DECIDED before publishing/verifying original .sav, then returns native
success for later cloud operations. Restarts resume that acknowledged single-file
decision. Publication failure cannot mistakenly invoke first-save failure's
remote rollback.

Title adds no recovery prompt or X/Y controls. Metadata recovery state 14
(`0x00243AF4`) keeps native ID/current-version comparisons and mismatch UI;
its commit, rollback and existing forced-rollback requests use the local service
only in Offline Mode. Forced recovery retains the native two confirmations,
without Bank's numeric keyboard. The service
requires the complete descriptor of a live nonzero reservation; post-commit
rollback and stale replay fail. Bank-only rollback does not rewrite another
game save. The shared service uses CRC32, reduces reads with a 1 MiB
workspace and hashes a complete Bank body's already-loaded memory snapshot.
The volatile loaded view reuses observations across read-only requests within
the native load; it is not persisted or journal authority. Exact source path/game
code keys prevent mixing sources. Journals/settings/configuration remain fresh.
Reentrant reads use a 1 KiB fallback. Mutations invalidate affected views and
failures clear them; a new load, reselection, reconnect or restart refreshes inputs.
Actual uploads/source-save preparation recheck primaries, and publication checks
physical existence/length. Pre-entry SD preparation reuses an identical verified
`x.bak`; whole-image copying uses the verified buffer or authenticates actual
read bytes before writing, with one complete post-write destination verification.
The SD `BeginSourceSave` branch relies on Stage's existing full guard rather
than scanning Bank immediately before it. SOURCE_ONLY SourceSaved already
persists the decision and publishes the source, so the reply proceeds to
verified Finish without a duplicate Commit. Publication classifies the primary
with one observation; temp/backup checks and native CRC16/save polling remain.
Source recovery state 13 (`0x00246B68`) keeps native
transaction reading: token absent continues; state 1 rolls back; state 2 commits.
Success or expired/not-found replies tolerated by the native callback permit
token-only clearing and save kind 2. Other errors retain native messages/results
and do not clear tokens. Source loading,
VC validation and NDS redundant-face checks remain native, not a separate
selection-time recovery route. Recovery never repeats conversion/removal.

The Offline shared service registers a valid untracked primary and may adopt a
primary differing from both pending images through recorded `ADOPT_BANK=6`.
It retires the old reservation, assigns a new ID and removes the old Bank
backup, without rewriting the actual source .sav or claiming paired rollback.
A normal recorded pending image still uses native recovery. All Bank images
missing with valid settled authority may reset the baseline, discarding a sole
unconfirmed Bank tmp only after the new decision is published; Mover retains its
native download-failure route and never creates a Bank. Missing settings recover an
exact recorded image, or reinitialize after settled complete deletion, without
changing Bank ID/metadata. Pending settings loss and damaged authority still stop;
configuration loads use current text without reevaluating staged bodies. With no published history,
checked Bank backup-only data may become a new baseline; pure tmp is explicitly
abandoned, including unbound local settings/source staging, not promoted to a
committed save. Mover reloads the real selected .sav, never creates a blank one.
See [combination rules and import risks](../../common/docs/fakeServer/architecture.zh-cn.md#删除损坏和手动换入的处理边界).

For settled and unregistered existing Banks, group/order inspection and full
hashing share one stream and supply the loaded view. A loaded Bank's transfer
permission no longer reloads an unused header after source work replaces the
I/O buffer; the native client still determines transfer-box occupancy.
External same-size changes become visible on a new load and cannot overwrite
the primary through an old actual upload. Pending/recovery decisions remain.
Native ticket subrequests share
policy/time only within a job; a new job, reload/edit, purchase/recharge or location
change invalidates it. Staged transfer bodies and commit/rollback decisions are
retained without freezing the whole configuration, and batched
configuration output still verifies source and temporary files. Bank's unchanged-download snapshot
optimization is internal to the shared service; Mover still receives its complete
Bank body through the existing read API, without creating a Bank download reservation.

Cartridge/VC staging records the SDK-layout descriptor and 36-byte display
metadata, not before/after/cleaned native-block fingerprints. The server matches
ID, versions, size and nonzero token, ignoring the SDK's uninitialized alignment
word at +0x14. A known last-settled request may acknowledge only the same decision;
unknown history or mismatched requests never receive fake commit/rollback success.
Native callbacks decide residual-token cleanup for definitive business errors. Native source saves
use their original result handling; Online cartridge/VC writes have no local
journal-based cross-mode restriction. NDS file paths, complete images, working
files and SOURCE_ONLY coordination remain at the file backend. R switching stays
available. Complete native snapshots/paired rollback are not implemented.
Journals are not uploaded. Flush/reread are not real
SD power-loss proof. Concurrent
writers to one SD directory are unsupported.

Offline cartridge/VC saves retain their native cleanup callbacks. Save substate
14 matches the descriptor and records cleanup start; only actual native polling
success on `15 → 16` acknowledges final cleanup. State 13 recovery also observes
the original cleanup save at substates `6/8/9`. Failures retain the descriptors and
block unverified replacement staging. Once the server commit is published, a
later connection/download may verify the Bank, settings and committed NDS source
and finish the server record without inventing a lost cleanup acknowledgement.
Unacknowledged cleanup temporaries are never published; native callbacks and the
independent source-file backend still clear residual tokens. Uncommitted work
and mismatched files retain their matching, rollback and error paths. Matching
native descriptors allow cross-application Bank/Mover recovery.
Digital sources retain image-validation/publication handling,
and Online native saves do not use these local acknowledgements.
An old Bank/Mover descriptor cannot acknowledge a newer reservation. Valid formal
journal authority, missing-formal `.bak` recovery and non-authoritative `.tmp`
handling also apply across applications. Settled/absent old-token cleanup is a
guarded Bank no-op; native events cannot replace the Gen 5 file save/validation
path or set its source/cleanup acknowledgement flags.

## Native service integration

The embedded service supplies offline Bank bodies, ticket/campaign replies,
metadata, per-slot results and transaction decisions. It does not own native
Pokemon objects or game conversion. Online requests continue to the official
provider; an explicit emulator ticket capability fallback is separate from the
offline account policy.

Downloads retain native SDK construction, allocation, observation and file
consumption. Results are copied in bounded chunks and released before SDK
completion. Cancellation detaches observation without waiting for the worker,
and late results cannot be delivered into a reused request address.

Ticket operation `6` and free-campaign completion continue through native
`0x00249600/0x0024991C` once response delivery and release finish. Synchronous
catalog helpers do not queue work that would require a future UI frame to finish.
Unsupported operations complete failure directly. Offline connection monitoring
does not poll a nonexistent official session; online monitoring remains native.

## Per-user locale assignment in v1.5 conversion

Offline conversion uses the confirmed user's `region/language/country/state`
values from `user.conf` for console-origin fields. These are not Pokemon's source
language or game-of-origin fields. autoRegion refreshes only at account entry;
autoCurrency follows the configured region. Manual settings can intentionally
override the physical console or a Luma locale override.

Bank does not rewrite these Pokemon fields after receiving them. Online Mover
retains the original console-settings calls.

## Optional offline Chinese name conversion

`allowFanTranslationCHN=false;` in the user's `user.conf` defaults off. The successful
connection response delivers that user's custom options before candidate conversion.
When enabled, the configured US/Korean VC fan-translation tables apply, and the
BanUnicodeChinese rule is disabled in memory without changing shared configuration.
Other Pokemon and NGWord screening remains active.

Gen5 names are already UTF-16. Before the default-name write, the converter
selects a permitted original Chinese nickname and skips that database-name lookup,
retaining native special-glyph and control-character normalization.
Native conversion of already-nicknamed text and OT names remains. The per-slot
name-correction code travels on the current call's stack: a server-requested
nickname correction cannot enter the preservation branch, and the nickname flag
is not changed by that branch.

English, French, Italian, German and Spanish VC sources use the English fan table
for two-byte sequences and retain each source language's native single-byte meanings.
The byte-prefix ranges do not overlap. Korean Gold/Silver independently uses the
Korean table with mixed single-byte support; Japanese sources are excluded. Source
classification follows the native selected-game version/language mapping, not names
or console locale.
VC selects the appropriate fan-translation text at the native nickname/OT
write calls, including all three conversion paths and the special Mew renaming
branch. Native string comparisons still determine the nickname flag; no converted
record is reopened to replace an already-written default name. The resulting
UTF-16 names are subsequently screened by the service, with name corrections
applied by the native consumer. Disabled options or invalid/incomplete fan codes
retain native text, and no global candidate pointer is kept.

`enableMoverv1.2=false;` independently controls old offline RGBY conversion;
it does not change Gen2, Gen5 or online paths. See [version compatibility](version-compatibility.md)
and [custom settings](../../configTemplate/templateForCopy/Bank/fakeServers/users/readme_user.md).
