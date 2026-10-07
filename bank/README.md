# Combined offline, download, unlock, and original patch

See [`docs/code-analysis.md`](docs/code-analysis.md) for the stock
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
the server Bank. Original Mode uses the stock save and complete official Bank
flow; it never loads the offline Bank for upload.

## Modes and recommended workflow

On a fresh launch, the title screen defaults to **Offline Mode**. Each press of the physical **R**
button advances through **Offline Mode**, **Download Mode**, **Unlock Mode**,
and **Original Mode**. The line below the stock HOME-menu help updates immediately. Press
**A**, **START**, or touch the lower screen to latch the displayed mode for the
whole session; R no longer changes it after leaving the title screen.
Returning to the title retains the selected mode. Restarting the application
resets it to Offline; no mode preference is written to a save.

```text
Title screen (default: Offline)
        │
        ├── R ───────────────► Download
        ├── R again ─────────► Unlock
        ├── R again ─────────► Original
        ├── R again ─────────► Offline
        └── A / START / touch
                  │ latch displayed mode
                  ├── Download ─► official server download ─► local file
                  ├── Unlock  ─► stock forced-unlock path
                  ├── Original ─► stock save and complete official flow
                  └── Offline  ─► local file ─► normal Bank use
```

Recommended first use:

1. Back up the SD card and any existing `sd:/3ds/Bank/` files.
2. Select Download Mode and enter the first feature-menu item to retrieve the
   complete Bank object without selecting a game. If no usable game save is
   found, accept the direct server-download prompt.
3. Wait for the download-complete message and the return to the title screen.
4. Confirm that `sd:/3ds/Bank/bankdata.bin` exists, then use Offline Mode for
   normal Bank sessions.

Offline Mode can also create a new local Bank file through the stock first-use
initializer when no recoverable local file exists. Downloading first is still
preferred when an existing official Bank account must be preserved locally.

## Local files and invariants

Offline and Download modes use the same checked Bankdata-file implementation.
Only Offline Mode redirects the Turtle record; the other three modes use its
stock backend after the title selection is confirmed.

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

Offline Mode redirects the persistence backend of the stock logical
`data:/turtle` record to `sd:/3ds/Bank/sav.bin`. The patch neither replaces nor
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
local-record reads and writes in Offline Mode use it exclusively. An SD write
failure is returned as a save failure and never falls back to modifying the
stock save.

`sav.bin` is a fixed `0x200`-byte logical-record image. The stock serialized
object occupies `0x30` bytes, of which the first `0x2C` bytes are currently
identified fields. See
[`include/turtle_redirect.h`](include/turtle_redirect.h) for the declaration.
Because the record can be migrated again from the stock save or rebuilt by the
first-use flow, it uses only `sav.tmp` to prevent short writes and does not
create `.bak` or `.break` files.

Before mode selection, common startup/title handling uses the SD backend as
before, including read-only migration from a valid stock record if needed.
After the native asynchronous release of the shared prompt view and destruction
of the title view, the chosen backend is checked and reloaded:

| Selected mode | Record for subsequent checks, loads, formatting and saves |
| --- | --- |
| Offline | `sd:/3ds/Bank/sav.bin` |
| Download / Unlock / Original | Stock logical `data:/turtle`; native archive, commit and transaction handling |

Reloading clears the previous record's loaded flag and pending language.
A valid record supplies its own language and Kanji setting. Subsequent native
states recreate the prompt/menu view using the same language archive and font,
so different SD/native save languages do not reuse the previous view's resources.
Missing/invalid records enter native language selection, then resume game checking and its
initialization chain in the selected mode. The SD record is never imported
into the stock save. Returning to the title reloads the SD preview before
creating its UI. Native-mode saves are not mirrored to `sav.bin`; save failures
never switch to the other backend.

> **Privacy:** A `sav.bin` migrated from the original internal save for offline use may contain personal or account-related information. Do not upload it publicly or share it casually. The same precaution applies to downloaded `bankdata.bin` files.

## Download Mode

Download Mode retains stock startup, account queries, first-user server record
creation, ticket checks, complete-file requests, and disconnect jobs. It uses
the game-independent HOME downloader without entering the HOME transfer UI.
After title-mode confirmation, language and native initialization use the stock
Turtle save, not `sav.bin`; no source game is selected or saved. Common title
preview/migration still uses the SD record as described above.

```text
Confirm title mode → state 3 game check
        ├── no usable game: ask to download directly
        │       └── No ─► stock cleanup → title
        └── usable game / Yes
                ▼
Stock Turtle check/load; initialize and save the stock record if required
        │
        ▼
States 5 → 8 → 15 → 9: create and upload a missing server Bank, then wait for success
        │
        ├── usable game ─► state 4: select Download Bank Data
        └── no game and download accepted ─► continue directly
        │
        ▼
State 28: stock complete Bank download without game selection
        │
        ▼
Native success callback loads current data or expands a legacy body
        │
        ▼
Capture the loaded 0xBB518-byte record before session-flag adjustments
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
State 29: stock remote release and UI teardown → state 20: disconnect
        │
        ▼
Local-save success / failure message; return to title
```

The capture occurs only after the complete object has been loaded; network
chunks are not written incrementally. At `0x002D1248`, both native body loaders
have returned, but the callback has not yet adjusted session flags. The native
legacy loader accepts `0xACA48` bytes and expands them into the current record;
the patch copies the loaded `0xBB518` bytes rather than reading a current-sized
block from a shorter response. A second mode check permits local writing only
for the state-28 route in Download Mode. Other routes cannot overwrite
`bankdata.bin`.

Download Mode does not enter game selection, local mileage, Bank Box, or save
screens. Without a usable game save it asks for confirmation; declining follows
stock cleanup, while accepting first completes native Turtle check/initialization,
then connection and first-use checks.
A missing local file does not recreate an existing server Bank. If the server
Bank is missing, the official creation callback must succeed before requesting
the complete server file; the pre-upload local buffer is not used as a download.
If local capture fails, the patch does not falsify the official callback
result or silently mark the capture successful.

The existing download-to-SD path message (`0x61`) is reused. A successful temp
write and file rotation select the existing completion message (`0x62`); a failed
capture selects the new local-SD-save failure message (`0x74`). Before a successful
response/capture attempt, disconnect retains its native text. State 29 retains
its remote request, asynchronous wait, and UI teardown, then proceeds directly
to disconnect instead of state 27's HOME transfer controller. This cleanup still
sends a server request.

### Startup and server-data cases

Turtle initialization, first server Bank creation, and local file installation
are separate stages. The server account query—not the presence of local
`bankdata.bin`—decides whether state 9 creates a Bank.

| Usable game save | Server Bank | Download route |
| --- | --- | --- |
| Present | Exists, populated | Native startup → menu first item → state 28 download. |
| Present | Exists, empty | Same route; capture the complete empty record without entering HOME's empty-Bank warning. |
| Absent | Exists, populated | Accept the direct-download question → Turtle checks → native connection → state 28. |
| Absent | Exists, empty | Same direct route; zero Pokemon does not suppress capture. |
| Either | Missing | Native first-use instructions and initialization → upload → wait for the real success callback → state 28. With a usable game, the feature menu remains between creation and download. |
| Absent | Either, user selects Cancel | Native cleanup/title return before connecting; no server creation or download. |

The no-game Download question uses **Start Download** and the stock **Cancel**
button. Original Mode retains the stock HOME transfer confirmation labels.

With a valid stock Turtle, state 3 loads it and retains native language/save
handling. Missing or invalid records use the original initialization confirmation,
format, initialization and save chain; declining or a native save failure follows
the stock error/return path. Accepting the no-game question sets the native direct
flag but does not bypass this chain. An existing SD `sav.bin` is not substituted
for a missing stock record after Download Mode is confirmed.

### Failure and interruption cases

| Stage | Behavior |
| --- | --- |
| Turtle initialization, account/ticket check, or network request fails | Keep native messages and error cleanup; do not report a completed local download. Public ticket policy remains `0`. |
| Initial server upload cannot start or its callback fails | Keep the native failure branch; do not export the pre-upload initialization buffer. |
| Full download fails or the server does not provide a successful body | Keep native failure handling; do not manufacture an empty Bank or a successful local result. |
| Loaded object header is unusable, or temp write/size/flush/close checks fail | Record local capture failure, keep the native callback result, and continue native cleanup. A partial `.tmp` may remain; it is not promoted just because it exists. |
| Backup deletion or renaming fails | Report local failure. The existing rotation helper stops or attempts to restore the old `.bin`; recovery is not guaranteed if the SD card remains unusable. |
| State 29 release fails after the SD copy succeeded | The file was saved, but the native remote error is still reported. Local completion is not a claim that a server transaction was unlocked. |
| Software/power stops during download or rotation | No completed message is shown. A later download queries the server again; it does not upload the local file or treat a leftover temp as a completed download. Offline recovery retains its existing `.bin/.bak` rules. |

All completed-download paths skip mileage, Box, normal saving, and HOME transfer.
Initial stock Turtle saving and first server Bank upload are still allowed.
Server rejection or transaction locking is not forcibly bypassed by this route.

Download Mode preserves the native state 15 entitlement, ticket, campaign, and
online-gift checks by default, including their waits, messages, and error
handling. The one-byte switch below enables a local bypass for emulator tests;
it does not change the post-download mileage, Box, and save-screen skip.

## Unlock Mode

Unlock Mode is a separate entry into the stock online transaction-recovery
flow. It does not inherit Download Mode's Bankdata capture, post-download state
skip, or download-complete message.

First confirm that none of the cartridge or digital saves matches the server.
Then select any game and hold L + A + START while confirming. Without the
combination, the stock mismatch notice remains. Acknowledging it lets native
recovery cleanup and destruction finish before recreating state 10, the same
game-selection chain reached after confirming the first feature-menu entry.
This does not return to the menu or directly reuse the old selection UI.

After native selected-game transaction cleanup, show the appropriate result
and wait for acknowledgment before native disconnection. None of these routes
enters mileage or boxes:

- No recovery needed: “Server status is normal. Unlock Mode is not needed.”
- Automatic recovery of a matching save: “Automatic recovery completed. The server lock has been cleared.”
- Successful official forced unlock: “Forced unlock completed. The server lock has been cleared.”

Recovery success requires an accepted native commit/rollback; matching a save
or opening the unlock screen alone does not qualify. Network errors,
cancellation and empty candidate lists retain their native handling.

```text
Title screen: Unlock Mode
        │
        ▼
Feature menu: Enter Force Unlock
        │
        ▼
Confirm all saves mismatch; select any game while holding L + A + START
        │
        ▼
Stock forced-unlock challenge state
        ├── no server candidate ─► retain the stock two-line prompt
        └── candidate available ─► append its first accepted eight-digit form
                                      as “Enter this unlock code: xxxxxxxx”
        │
        ▼
Stock input validation, rollback request, and selected-game transaction cleanup
        │
        ▼
Forced-unlock completion notice → acknowledgment → native disconnect to title
```

The server response owns a vector of candidate values. The stock validator
accepts an entered number when it equals any vector element modulo
`100,000,000`. The patch displays the first element using exactly that rule and
lets the existing message system add leading zeroes. It does not invent a code,
skip validation, force a success result, or replace the subsequent server
operation. If the vector is empty, the original prompt is used unchanged.

This mode still shares project-wide menu restrictions but uses the stock Turtle
record. It preserves native state 15 ticket and online-gift checks by default,
and native transaction requests/validation remain intact; only the described
post-notice branches change. The local offline
`bankdata.bin` is neither loaded nor uploaded by the unlock display hook.

## Ticket and online-gift checks: public and test builds

Change this value at the top of [`src/main.s`](src/main.s), then rebuild:

The source defaults to the public policy `0`. Set `1` only for personal tests
that allow the missing-interface fallback.

```asm
.definelabel ONLINE_TICKET_CHECK_BYPASS, 0
```

| Session mode / environment | `0`: require native checks, public default | `1`: allow automatic missing-interface fallback for tests |
| --- | --- | --- |
| Offline, any environment | Local 999-day entitlement without online campaign queries | Same as left |
| Download / Unlock / Original, hardware or unknown environment | Native ticket, campaign and online-gift checks | Same as left |
| Download / Unlock / Original, Azahar with the interface implemented | Native checks | Same as left |
| Download / Unlock / Original, Azahar confirmed to lack the interface | Native error handling; never fabricate results | Automatically select local ticket results |
| Download / Unlock / Original, failed probe or ordinary ticket error | Native error handling | Native error handling; no automatic approval |

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

Both paths remain in the same code. This definition changes only the policy
byte at `[0x003FEBB8, 0x003FEBB9)`. State 15 retains its original entry and
state transitions. Only three calls are redirected: job initialization at
`0x002B0444`, result polling at `0x002B0464`, and unbinding at `0x002B1994`.
The initialization wrapper passes job, shared data, session mode and policy to
`LocalTicket_InitializeSelected`. Each job selects its backend once, recording it
at `0x003FAFF4`; polling and unbinding reuse that result. Native allocation,
construction, result checks, field copying, UI cleanup, and destruction remain
in place. Local unbinding succeeds without a network call because no client
was bound.

Local results contain the current console date and an expiry exactly 999 days
later. Native getters calculate remaining days, hours, and validity. The
current date itself is not advanced and remains the local mileage reference.
Purchase counts stay `-1` (unknown), so no ticket-purchase reward is invented.
Existing EC account fields are preserved. The free-campaign window reuses the
same current/expiry dates in the native decimal timestamp format; state 15
calculates the free flag and remaining time, so the menu uses its native free-use
text. This window is rebuilt from console time on each local initialization,
not from fixed server campaign dates. No system eShop applet/loading animation
is simulated. The completed job uses the
same status as native verification with at least 15 entitlement days. Clock
failure or an expiry beyond the native year range follows native error handling.

Test policy `1` reuses the loader's `GetSystemInfo(0x20000, 0)` check: successful
result, low ID `2`, high word `0`. Only identified Azahar is probed. A nonblocking
`am:net` session sends command `0x0821` with zero title/ticket IDs and a four-byte
output buffer; it does not read a real ticket. Local results require successful
transport, the exact short reply `0x08210040` with result `0`, and successful handle
closure. This matches the verified revision's
[unimplemented-handler reply](https://github.com/azahar-emu/azahar/blob/6280521bf26ef1393974cd8c898de29cf75f5752/src/core/hle/service/service.cpp#L155).
Full replies, missing tickets, service/transport failures and unknown responses
stay native. Public policy `0` does not probe and always uses native online tickets.

The policy does not skip NNID, other requests or server checks, or change download
exit, unlock transactions, mileage or box dispatch. **Test policy is not a guarantee
of safe online use**. The 999-day expiry and displayed entitlement fields have no
identified direct serialization path into Bankdata. However, the local current
date is copied into shared data: first creation writes it into the new Bankdata
before upload, and Unlock/Original's native mileage/save flow can upload the resulting
mileage and date. Existing-bank Download Mode skips that mileage/save flow, but
first creation still uploads.

A server download replaces the Bankdata body, not the current session's ticket
date. A later native ticket check also does not undo already uploaded Bankdata
changes. Client-side analysis cannot guarantee that the server corrects those
values. Keep public builds at `0`, accepting native errors when the interface is
missing instead of using local ticket results for real cloud operations.

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

Declining to continue at the full-gift warning, or terminating the reward flow
with result `21`, keeps the native transition to state 19. Offline completes
this state without creating a remote rollback job; its stock prompt, timer and
1,500-ms finalizer remain intact before state 20 cleans up and returns to title.
This exit does not change Turtle transaction fields or rotate Bankdata files.
The other three modes retain the complete native remote rollback flow.

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

## Original Mode

Original Mode follows Unlock in the R-button cycle. Once confirmed, it reloads
the stock Turtle record and uses the original initialization, transaction
recovery, ticket and gift checks, mileage, Box, save and disconnect paths.
Missing stock records use that path, not an existing SD record. The complete
feature menu, original upper-screen text, support-code, Mover/eShop and HOME
operations are restored. Without usable game data, the original HOME question
is available. The title's mode-switch hint remains.

Tickets share Download/Unlock's public/test policy: `0` requires native checks;
`1` permits local results only after confirming the missing Azahar interface.
Other official operations are not bypassed.

This is not an offline-data upload mode: it neither captures nor supplies
`bankdata.bin`, and it does not synchronize the two Turtle records.

## Shared menu, messages, and language handling

| Feature-menu entry | Offline Mode | Download Mode | Unlock Mode | Original Mode |
|---|---|---|---|---|
| First entry | Use Pokemon Bank | Download Bank Data | Enter Force Unlock | Stock Use Pokemon Bank |
| About Pokemon Bank | Stock information screen | Stock information screen | Stock information screen | Stock information screen |
| Support | Disabled | Disabled | Disabled | Stock support code |
| Poke Mover/eShop | Disabled | Disabled | Disabled | Stock behavior |
| Pokemon HOME | Language selection | Language selection | Language selection | Stock HOME transfer |
| Back | Stock behavior | Stock behavior | Stock behavior | Stock behavior |

In the first three modes, the former HOME entry is redirected before any HOME networking or Box state is
created. It opens the stock language selector. Before returning to the title,
the patch copies the pending language and Japanese Kanji setting into the
persistent settings object, starts native local-save mode 0, and waits for it
to finish. Offline changes go to `sav.bin`; Download/Unlock changes go to the
stock record and then use the native disconnect job. A blank
disconnect entry prevents a newly selected font from rendering stale text in
the previous language.

The first three modes also block no-game entry to HOME transfer operations.
Offline/Unlock show the two original warning pages, retaining both confirmation
waits before returning to the title. Download instead asks to retrieve the server
Bank directly, then downloads and disconnects without game selection.
The complete original message and challenge-code
prompt are preserved for Original Mode; custom variants are appended instead
of overwriting original message entries.

Stock network, Bank-connection, save, and disconnect entries remain unchanged.
Mode-specific text is appended and selected only by the state that owns it.
Title hints, menu descriptions, game-selection guidance, local connection/save
messages, and download completion are provided for all ten shipped language
archives: Japanese kana, Japanese kanji, English, French, Italian, German,
Spanish, Korean, Simplified Chinese, and Traditional Chinese.

## Source layout

| File | Role |
|---|---|
| `src/main.s` | Original-address hooks, mode dispatch, small trampolines, state routing, and placement of imported C objects |
| `src/bankdata_redirect.c` | Checked Bank-data validation, recovery, loading, and local transaction implementation |
| `src/code_expansion.c` | Unified hardware/Azahar loader using the same architecture as Mover |
| `src/fs_helpers.c` | Shared checked SD-filesystem implementation used by Bankdata and Turtle backends |
| `src/offline_flow.c` | Local connection, disconnection, and save-display state updates |
| `src/local_mileage.c` | Converts console dates shared by local miles, tickets and Bank-slot timestamps; it does not replace the native point calculation |
| `src/local_ticket.c` | Job-backend selection, Azahar missing-interface probe and local ticket results; no Poké Mile calculation |
| `src/unlock_mode.c` | Selects and normalizes the first server-returned unlock candidate for the stock challenge-code UI |
| `src/patch_paths.c` | Path constants placed in added pages |
| `src/turtle_redirect.c` | Redirected Turtle-record backend placed in added pages |
| `include/bankdata_redirect.h` | Confirmed serialized Bankdata layout, partial native Bank views, redirection constants, and stock entry points |
| `include/fs_helpers.h` | Shared filesystem types, SDK entry points, and checked SD-helper declarations |
| `include/local_mileage.h` | Date input shared by local miles, tickets and Bank slots |
| `include/local_ticket.h` | Ticket-job and shared-data views, entitlement constants, and local job interfaces |
| `include/offline_flow.h` | Stock timer entry points used by local flow states |
| `include/code_expansion.h` | Expansion loader interface and SVC constants |
| `include/patch_types.h` | Fixed-width primitive types shared by the injected C objects |
| `include/patch_paths.h` | Path declarations shared across the separately placed objects |
| `include/system_time.h` | Shared-memory system-time structure and addresses |
| `include/turtle_redirect.h` | Confirmed logical-record layout, backend result contract, stock entry points, and exported backend declarations |
| `include/unlock_mode.h` | Unlock-candidate display helper declaration |
| `src/patch_messages.py` | Rebuilds and validates the ten localized LayeredFS archives |
| `tools/message_archive.py` | Self-contained GARC and encrypted message-file codec |
| `tools/prepare_expanded_code.py` | Validates code/ExHeader, materializes native BSS and prepares a fixed-address expanded image/header |
| `tools/verify_patch.py` | Verifies preserved native functions, expansion layout, loader, hooks, BPS reconstruction and resources |
| `Makefile` | Compiles, expands, injects, creates BPS, rebuilds messages and writes the Luma release tree |

### Injection-space provenance and bounds

Addresses are runtime virtual addresses with half-open ranges `[start, end)`.
Definitions are in [`src/main.s`](src/main.s) and [`symbol.inc`](include/symbol.inc);
actual placements are recorded in `build/armips-symbols.txt`. Bank and Mover
share the fixed-address expansion architecture, not their image boundaries,
startup entries or SDK addresses. Bank's build does not reference Mover files.

Original segment and logical BSS boundaries:

| Original region | Start | Declared size | Content end | Page-mapped end | Permissions |
| --- | --- | --- | --- | --- | --- |
| `.text` | `0x00100000` | `0x213910` | `0x00313910` | `0x00314000` | Read/execute |
| `.rodata` | `0x00314000` | `0x55370` | `0x00369370` | `0x0036A000` | Read-only, non-executable |
| `.data` | `0x0036A000` | `0x41ACC` | `0x003ABACC` | `0x003AC000` | Read/write, non-executable |
| Logical BSS | `0x003ABACC` | `0x4EE38` | `0x003FA904` | `0x003FB000` | Read/write, zero-initialized |

Logical BSS begins at the declared data-content end, not the mapped page end.
Native clearing constants, the constructor walker and PREL32 constructor table
at `[0x00369050, 0x00369370)` remain unchanged.

Expansion moves no native segments or variables. Original BSS becomes explicit
zero bytes in the image, followed by a separator page and three payload pages.
ExHeader data becomes `0x95` pages, size `0x95000`, with BSS size `0`;
native clearing still runs. The image grows from `0x2AC000` to `0x2FF000`
bytes, while runtime mapping grows by only four pages, `0x4000` (16 KiB).

| Range or hook | Original space / purpose | Current use and constraints |
| --- | --- | --- |
| Scattered hooks in `main.s` | Native entries, calls or branches | Existing mode dispatch and instruction replay; no complete injected functions. |
| `0x00100010`, 4 bytes | Native startup `BLX` to the Thumb constructor walker | Calls the loader, restores registers and tail-resumes at `0x00102BA5`. |
| `[0x00285BA8, 0x0028711C)` | Native HOME box-selection UI | Original bytes preserved throughout; not reclaimed. |
| `[0x002A7BF0, 0x002A8404)` | Native state 27 HOME flow and companions | Original bytes preserved throughout; not reclaimed. |
| `[0x002A8404, 0x002A8760)` | Native state 14 eShop flow and companions | Original bytes preserved; not reclaimed. |
| `0x002A8760`, `0x002A93F4`, 4 bytes each | Native state 18/17 update prologues | Branch to added-page mode dispatch, then replay the original push and complete native body; only Unlock Mode adds acknowledgment-driven retry/exit branches. |
| `0x002A5860`, `0x002A588C`, 4 bytes each | Native state-18 result test and state-23 success branch | Unlock Mode reconstructs state 10 after confirmed mismatch and latches successful official forced rollback; native controller, construction and cleanup remain intact. |
| `0x002B0444`, `0x002B0464`, `0x002B1994`, 4 bytes each | State 15 job initialization, polling and unbinding | Call added-page wrappers; all other state-15 bytes and native jobs stay intact. |
| `0x001D887C`, `0x001D96E4`, `0x002B9DC4`, 4 bytes each | Date calls in Bank-slot writes, clears and group moves | Offline uses the current console date; other modes retain the native network-clock query. Slot-operation bodies are not reclaimed. |
| `0x002AEB9C`, 4 bytes | Native state 19 no-save remote rollback update prologue | Offline reports local completion without a remote job; other modes replay the native push and resume the original body. Initialization, callbacks and timed finalization are not reclaimed. |
| `0x002B1B98`, `0x002B1BDC`, 4 bytes each | Title-exit UI-root load and completion return | Wait for native shared-prompt release before native title destruction, then reload the selected record and language; cleanup bodies remain intact. |
| `[0x00313910, 0x00313A40)` | Original last text-page padding | `0x130` bytes reserved for Luma LayeredFS; untouched. |
| `[0x00313A40, 0x00313B1C)` | Original last text-page padding | Startup assembly and `code_expansion.o`, `0xDC` bytes. |
| `[0x00313B1C, 0x00313FC0)` | Original last text-page padding | Unused executable padding, `0x4A4` bytes. |
| `[0x00313FC0, 0x00314000)` | Original last text-page padding | Zero-padded 64-byte `offline_patch_v1.0.0` identifier. |
| `[0x003ABACC, 0x003FA904)` | Native logical BSS | Explicit zeros; native variables and startup clearing retained. No payload. |
| `[0x003FAFF0, 0x003FB000)` | Final 16 bytes of original RW mapping, after logical BSS | First four bytes hold session mode, capture status (`0` not attempted, `1` saved, `2` failed), R-key history and title selection; `+4` stores this ticket job's backend, `+5` records pending first-language selection, `+6` stores session recovery result (`0` none, `1` successful official forced unlock, `2` successful native automatic recovery); nine bytes reserved. |
| `[0x003FB000, 0x003FC000)` | Added RW mapping | Zero-filled separator, not an unmapped guard page. |
| `[0x003FC000, 0x003FF000)` | Three new pages after native BSS | All feature wrappers, C backends and paths; enabled for execution on hardware by the loader. |

Offline, Download and Unlock still disable or redirect HOME, support-code and
eShop entries without consuming their function bodies. Original Mode restores
their native routes. Title, feature, language and Turtle hooks select the
appropriate behavior without replacing the native storage implementation.

Added-page placements total `0x2BBC` (11196 bytes), leaving `0x444` (1092 bytes):

| Content | Actual range | Size |
| --- | --- | --- |
| Text, save and language assembly wrappers | `[0x003FC000, 0x003FC414)` | `0x414` |
| `patch_paths.o` | `[0x003FC414, 0x003FC4C7)` | `0xB3`, then one alignment byte |
| Mode, title, connection, unlock, exit and slot-date branch wrappers | `[0x003FC4C8, 0x003FCEBC)` | `0x9F4` |
| `turtle_redirect.o` | `[0x003FCEBC, 0x003FD400)` | `0x544` |
| `fs_helpers.o` | `[0x003FD400, 0x003FD9D0)` | `0x5D0` |
| `bankdata_redirect.o` | `[0x003FD9D0, 0x003FE0A8)` | `0x6D8` |
| `offline_flow.o` | `[0x003FE0A8, 0x003FE1D8)` | `0x130` |
| `local_mileage.o` | `[0x003FE1D8, 0x003FE600)` | `0x428` |
| `local_ticket.o` | `[0x003FE600, 0x003FE9A4)` | `0x3A4` |
| `unlock_mode.o` | `[0x003FE9A4, 0x003FEB60)` | `0x1BC` |
| Ticket assembly wrappers, literal pool and policy byte | `[0x003FEB60, 0x003FEBBC)` | `0x5C` |
| Unused added-page space | `[0x003FEBBC, 0x003FF000)` | `0x444` |

Native text's actual size stays fixed, preserving Luma LayeredFS placement.
Its path still uses the rodata tail at `[0x00369370, 0x00369397)`, which this
patch leaves untouched. Recheck separation when changing Luma; zero-filled
padding alone does not prove free space.

### One loader for hardware and Azahar

The release uses one `code.bps`, `exheader.bin` and `romfs/` set. BPS declares
the larger target length. Luma allocates using the expanded ExHeader; Azahar's
BPS loader resizes its code buffer using that length. Native segment addresses
stay fixed; text is not stretched across other native segments.

Before the original constructor walker, the startup loader:

1. Duplicates the current-process pseudo-handle using `SVC 0x27`.
2. Queries `SVC 0x2A` with `type=0x20000, param=0`; only success with ID `2`
   identifies Azahar.
3. Calls `SVC 0x70` on the added pages with `MEMOP_PROT=6` and `RWX=7`.
   Hardware and implemented emulator interfaces must return success.
4. Accepts only identified Azahar's missing-SVC behavior where `r0` retains
   the exact input handle. Unknown environments cannot use this fallback;
   other nonzero results still fail.
5. Closes the real handle. Handle or permission failures stop before added-page execution; success
   restores registers, stack and return address and resumes native constructors.

Only necessary data/BSS fields and SVC capabilities change. The current input
requires only `0x70` added. Other capabilities and the full header's signed
descriptor copy remain intact; this is a CFW override, not a new retail
signature. Ticket test policy reuses the same environment helper but separately
checks the ticket reply. Permission compatibility does not imply ticket support
or enable local online results. The probe adds no ExHeader service permissions.

Public references: [Luma loader](https://github.com/LumaTeam/Luma3DS/blob/master/sysmodules/loader/source/loader.c),
[Luma BPS loader](https://github.com/LumaTeam/Luma3DS/blob/master/sysmodules/loader/source/bps_patcher.cpp)
and [Azahar BPS buffer expansion](https://github.com/azahar-emu/azahar/blob/9e6f523a57fac9564ac0bf8286db3c3702d301ec/src/core/file_sys/patch.cpp).
Identification follows [Luma's query implementation](https://github.com/LumaTeam/Luma3DS/blob/master/k11_extension/source/svc/GetSystemInfo.c)
and [Azahar's SVC implementation](https://github.com/azahar-emu/azahar/blob/9e6f523a57fac9564ac0bf8286db3c3702d301ec/src/core/hle/kernel/svc.cpp).

## Independent build and installation

The Bank subproject does not depend on Mover source or build artifacts. It can
compile the payload, create BPS/ExHeader, rebuild all ten language archives, and run
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
   `Mount image to drive`, and copy the complete mounted `romfs` directory
   and the mounted root's `extheader.bin`.
4. Place the inputs in this layout:

```text
bank/rom/
├── exheader.bin
├── exefs/
│   └── 00040000000C9B00.dec.code
└── romfs/
    └── ...
```

Rename the mounted `extheader.bin` to `exheader.bin`. The shared release uses the
complete `0x800`-byte ExHeader from the same Bank base title as the code image.

The base code must be `2,801,664` bytes with SHA-1
`5AB630856835DCF2DBDF9A62244DD19E46AE1C7C`. The build checks the input again.
Do not commit or redistribute the code image, RomFS or ExHeader. `ROMFS_SOURCE` may
override the default RomFS path when required.

Build Bank independently from the repository root:

```sh
make -C bank clean
make -C bank
```

From the subproject directory:

```sh
cd bank
make clean
make
```

Example for a non-Windows host:

```sh
make -C bank ARMIPS=/path/to/armips IPS_TOOL=/path/to/flips
```

The build compiles nine C units and emits disassemblies, validates the native
code/ExHeader and reserves zero-filled added pages, imports the loader and
feature objects with armips, creates `code.bps` plus the matching ExHeader,
rebuilds all ten language archives and runs static verification. The complete output is:

```text
release/00040000000C9B00/
├── code.bps
├── exheader.bin
└── romfs/
```

The verifier checks the base hash, exact ExHeader changes, native constructor
and BSS boundaries, preserved functions, loader, every ARM hook, instruction
replay, mode dispatch, BPS CRCs/reconstruction and localized messages.
Install the complete `00040000000C9B00` under `SD:/luma/titles/` and enable
Luma game patching, or install the same complete directory in Azahar.
Remove stale `code.ips` and `code.bin` from that mod directory; do not mix them.
The build removes only its own generated release IPS, not installed emulator files.
Back up the SD card and game saves before real-console use.

## External open-source references

- [devkitPro/libctru FS declarations](https://github.com/devkitPro/libctru/blob/master/libctru/include/3ds/services/fs.h)
  and [implementation](https://github.com/devkitPro/libctru/blob/master/libctru/source/services/fs.c)
  were used to verify the public FSUSER/FSFILE interfaces and result handling.
- [Luma3DS loader patcher](https://github.com/LumaTeam/Luma3DS/blob/master/sysmodules/loader/source/patcher.c)
  was used to verify the title-specific BPS/ExHeader and LayeredFS layout.
- [pkNX TextFile](https://github.com/kwsch/pkNX/blob/master/pkNX.Structures/Text/TextFile.cs)
  was used to verify the public message-file encoding and line-table format.

The current Bank binary remains authoritative for application addresses, state
transitions, object layouts, file offsets, and patch sites. External references
are used only for public platform and file-format interfaces.
