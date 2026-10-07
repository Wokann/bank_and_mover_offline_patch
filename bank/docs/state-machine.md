# Pokemon Bank Stock State Machine and Patch Differences

This document records the outer state machine, network paths, and transaction
branches in the supported stock binary, then compares them with the current
combined patch's Download, Offline, Unlock, and Original modes. See
[`code-analysis.md`](code-analysis.md) for the memory map, function map, and
data-structure overview.

The conclusions below come from the current decompilation and the patch's
actual hook sites. Four different kinds of values must not be conflated:

- outer state IDs are `0–29`;
- each state object writes a business result at object offset `+0x30`, which
  `0x002A5580` uses to select the next state;
- Turtle offset `+0x1C` stores a separate persistent transaction state `0/1/2`;
- HTTP statuses such as `200/307/409/500` belong to the lower transport layer.

## Outer controller

| Address | Stock responsibility |
|---:|---|
| `0x002A5580` | Select the next state from the current state ID and result byte at `+0x30` |
| `0x002A593C` | Drive state initialization, per-frame update, finalization, and destruction |
| `0x002A5A7C` | Construct states `0–29`; state 24 is a terminal sentinel with no object |

Three controller flags also affect routing:

- force-cleanup flag `+0x1A`: routes any current state to state 20;
- language-initialized flag `+0x1B`: decides whether state 0 must enter state 1;
- direct-HOME flag `+0x1C`: sends startup completion to the feature menu or
  directly to the HOME path.

## Exact stock transitions

The result values below are the current state object's `+0x30` value. “Other”
means a result not explicitly accepted by that state; it normally reaches
state 20 cleanup or state 24 termination.

| Current state | Result and next state | Scenario |
|---:|---|---|
| 0 | `5 → 1` without language; `5 → 2` with language; `6/other → 24` | Version check followed by optional language initialization |
| 1 | `5 → 21`; `6/other → 24` | Finish language selection through the quiet title-return state |
| 2 | `4 → 3`; `3/other → 24` | Start from the title screen or terminate the application |
| 3 | `4 → 5`; `23 → 5` and set direct-HOME; `3/other → 20` | Game confirmation and normal/HOME entry routing |
| 4 | `12 → 10`; `13 → 14`; `23 → 28`; `3/other → 20` | Feature menu: Bank, another feature, HOME, or exit |
| 5 | `4 → 8`; `18 → 6`; `3/other → 20` | Establish the service session; outdated software enters state 6 |
| 6 | every completion `→ 20` | Clean up after the outdated-version message |
| 7 | `20 → 22`; `3/4 → 20`; other `→ 20` | Save completion or save-failure handling |
| 8 | `4 → 15`; `3/other → 20` | Query remote-record existence, account mode, and transaction summary |
| 9 | `4 → 4`, or `4 → 28` with direct-HOME; `3/other → 20` | Skip creation for an existing record or continue after first-use creation |
| 10 | `5 → 11`; `19 → 4`; `6/other → 20` | Game selected, selection cancelled, or selection failed |
| 11 | `9 → 18`; `10 → 17`; `11/other → 20` | Choose the transaction-reconciliation path |
| 12 | `5 → 13`; `6/12 → 25`; other `→ 19` | Pending reward, no reward, or terminate this use |
| 13 | `5 → 25`; other `→ 19` | Enter the box after receipt, or leave without saving |
| 14 | every completion `→ 4` | Return from eShop/another menu operation |
| 15 | `4 → 9`; `3/other → 20` | Entitlement, ticket, campaign, and online-gift checks |
| 16 | `4 → 12`; `20 → 22`; `3/other → 20` | Full Bank download success, persistence failure, or ordinary failure |
| 17 | `4 → 16`; `20 → 22`; `3/other → 20` | Selected-game transaction reconciliation |
| 18 | `4 → 17`; `22 → 23`; `3/other → 20` | Current-user Turtle transaction reconciliation |
| 19 | every completion `→ 20` | Release/rollback before a no-save exit |
| 20 | every completion `→ 2` | Disconnect, clean up, and return to title |
| 21 | every completion `→ 2` | Quiet return variant after language selection |
| 22 | every completion `→ 20` | Save-failure message followed by cleanup |
| 23 | `9 → 18`; `4 → 17`; `14 → 24`; `3/other → 20` | Retry forced recovery, continue other-side recovery, or terminate |
| 24 | — | Outer-flow termination sentinel |
| 25 | `4 → 7`; every other result, including `21`, `→ 19` | Save from Bank Box or leave without saving |
| 26 | every completion `→ 0` | Return from the auxiliary Pokédex-like entry |
| 27 | every completion `→ 19` | Finish HOME operations through no-save cleanup |
| 28 | `4 → 29`; `3/other → 20` | Download complete Bank data for HOME mode |
| 29 | `4 → 27`; `3/other → 20` | Release/rollback before HOME operations |

A result number has no universal meaning. Result `5`, for example, means a
successful game selection in state 10, a pending reward in state 12, and a
completed receipt in state 13.

## Stock scenarios

### Ordinary startup with an existing language

```text
0 → 2 → 3 → 5 → 8 → 15 → 9 → 4
```

State 8 obtains record existence, account mode, and a transaction summary; it
does not download the complete `0xBB518` Bank file. State 15 handles
entitlement, ticket, campaign, and online-gift data. State 9 skips creation for
an existing record and creates the initial file only when the account mode
indicates that no remote record exists.

### First launch and language selection

```text
0 → 1 → 21 → 2
```

State 1 stores the language choice. State 21 reuses the title-return
implementation with its quiet variant enabled.

### First use without a server-side Bank file

```text
5 → 8 (account mode '5') → 15 → 9
state 9: initialize 100 boxes and related fields
       → serialize 0xBB518
       → upload the initial Bank file
       → 4
```

The initial file is uploaded before the feature menu is shown. This does not
depend on a later Bank Box save. If creation fails, the next session remains on
the missing-file/creation branch.

### Use Bank without a pending transaction

```text
4 --result 12--> 10 → 11 → 18 → 17 → 16 → 12
12 --nothing to receive--> 25
```

State 11 selects a recovery entry from account mode and the selected game's
record. State 18 checks the current-user Turtle transaction, while state 17
checks the selected-game transaction block. When neither side has pending
work, both finish their consistency checks and state 16 requests the complete
Bank file.

### Pending local points or a reward

```text
16 → 12 --result 5--> 13 --result 5--> 25
```

State 12 decides whether receipt UI is needed and state 13 performs it. With
nothing pending, state 12 returns result `6` or `12` and enters state 25.

### Leave without saving

```text
25 --any result except 4--> 19 → 20 → 2
```

State 19 releases or rolls back a remaining remote staged transaction. State
20 disconnects and returns to title. This is distinct from save-failure state
22.

### HOME path

```text
Feature menu: 4 --result 23--> 28 → 29 → 27 → 19 → 20
Direct HOME:  3 --result 23--> 5 → 8 → 15 → 9 → 28 → 29 → 27
```

States 28 and 16 share the complete-file download implementation, but the
HOME mode byte selects a HOME-specific continuation. States 29 and 19 both
clean up pending remote work; state 29 continues to HOME controller state 27.

## Stock network layers

| Layer | State/function | Data and purpose |
|---|---|---|
| Session | state 5 / `0x002AF1FC` | Establish Internet and service sessions; no complete Bank file |
| Account summary | state 8 / `0x002ACBDC` | Record existence, account mode, and a small transaction description |
| Entitlement | state 15 / `0x002B0270` | Ticket, subscription, campaign, and online-gift status |
| First creation | state 9 / `0x002AE568` | Initialize and upload a complete `0xBB518` file |
| Complete download | state 16/28 / `0x002AF460` | Receive the complete file and attached description through `0x002D11B0` |
| Save staging | state 7 after `0x002B2490` | Upload serialized data and receive a 32-byte transaction descriptor |
| Commit/rollback | states 7, 17, 18, 19, 23, 29 | Resolve staged remote work from persistent transaction state |

HPP/HTTP is below these business states. One successful HTTPC call is not an
outer-state success: response bodies, control-plane results, and asynchronous
completion flags are still required.

## Stock Turtle transaction record

Turtle is the stock logical record `data:/turtle`. Its confirmed serialized
fields are:

| Offset | Size | Content |
|---:|---:|---|
| `0x00` | 8 | Remote data ID |
| `0x08` | 8 | Pending transaction token/credential |
| `0x10` | 4 | Current data version |
| `0x14` | 4 | Update version |
| `0x18` | 4 | Transaction data length |
| `0x1C` | 4 | State: `0` none, `1` rollback pending, `2` commit pending |
| `0x20` | 4 | Integrity marker `FEED` |
| `0x24` | 4 | Checksum over the serialized transaction fields |
| `0x28` | 2 | Language ID |
| `0x2A` | 2 | Japanese-kanji display flag |

State `1/2` is meaningful only with descriptor bytes `0x00–0x1B`. Stock state
18 first compares remote metadata with the Turtle descriptor: state `1`
invokes rollback and state `2` invokes commit. A mismatch also causes it to
inspect the selected-game transaction block. State 17 applies the same
`1=rollback, 2=commit` rule to that game block, then clears its transaction
fields and saves the game again.

### Stock successful save

The important internal stages of state 7 are:

```text
serialize the complete Bank file
  → stage it remotely and receive a 32-byte transaction descriptor
  → write "state 2 + descriptor" to the selected-game transaction block
  → save the selected game
  → write "state 2 + descriptor" to Turtle
  → save Turtle
  → commit remotely
  → clear selected-game transaction fields and save again
  → complete
```

If the selected-game save fails, stock code writes `state 1 + descriptor` to
Turtle, saves Turtle, and rolls the remote stage back. If the program exits in
the middle, states 18 and 17 resolve the persisted state on a later use.

## Current patch versus stock

| Node | Stock | Download Mode | Offline Mode | Unlock Mode |
|---|---|---|---|---|
| state 3 without usable game data | Ask whether to move directly to HOME after the warning | Ask to download the server Bank directly; confirmation uses the native direct-entry connection path | Keep the two missing-data/Pokédex warning pages and return after confirmation | Same as Offline Mode |
| state 4 first entry | Result `12 → 10`, select a game | Result `12 → 28`, no game selection | Stock entry | Stock entry |
| Turtle backend | Stock `data:/turtle` storage | Stock backend after mode confirmation | Redirected to `sd:/3ds/Bank/sav.bin` | Stock backend after mode confirmation |
| state 5 | Real connection | Stock | Complete the session locally; no remote job | Stock |
| state 8 | Server account summary | Stock | Classify existing/first-use from local `bankdata.bin/.bak` | Stock |
| state 15 | Remote entitlement/campaign | Public policy `0` stays native; test policy `1` uses local results only for a confirmed missing Azahar interface | Always supply required ticket fields locally; disable online campaigns | Same policy as Download Mode |
| state 9 | Server-side first creation | Stock creation, upload and asynchronous success wait in substate 7 | Create the initial local `bankdata.bin` | Stock creation and upload |
| state 11 | Select recovery path | Not entered | Keep the test, but redirect result `9` to state 17 instead of state 18 | Stock |
| state 18 | Current Turtle remote recovery and game matching | Not entered | Never entered | Native requests, validation and combination; an acknowledged mismatch recreates state 10 after cleanup |
| state 23 | Official forced-unlock challenge and rollback | Not normally entered | Not normally entered | Display the first server candidate; successful result `4 → 17` latches completion, with other native branches unchanged |
| state 17 | Selected-game remote recovery | Not entered | Keep stock UI timing without remote commit/rollback | Finish native transactions/game saving, show no-recovery/automatic-recovery/forced-unlock result, then disconnect after acknowledgment |
| state 16 | Download complete file | Not entered | Load the complete object from `bankdata.bin` | Not normally entered |
| state 28 | Game-independent complete download | Keep native `+0x41=1`; capture the loaded current-format record in its success callback | Not normally entered | Not normally entered |
| state 29 | Pre-HOME remote release and UI cleanup | Keep native job and destruction; success enters state 20 instead of state 27 | Not normally entered | Not normally entered |
| state 12/13 | Local mileage plus online gifts | Skipped after download | Keep local mileage; skip only online-gift lookup | Not normally entered |
| state 25 | Normal Bank Box | Not entered after capture | Stock Bank Box | Not normally entered |
| state 7 | Remote stage, game save, commit/rollback | Normally not reached; stock if reached | Local file stage, commit, and rollback | Stock |
| state 19/20 | Remote release and disconnect | Disconnect in state 20 after state 29 completes release | Local cleanup and title return | Stock |

Download Mode retains real networking, first-use creation, and native ticket
and online-gift checks. It uses the stock HOME complete-file downloader without
entering HOME operations. No game is selected, and selected-game states
11/18/17/16 are not entered. Native `+0x41=1` finishes from the complete response
without updating selected-game metadata or game/Turtle transaction records.
State 29 still releases remote work and destroys its UI, then disconnects:

```text
Usable game: state 4 first entry --result 12--> 28 → 29 → 20 → 2
No game: state 3 confirmation --result 23--> 5 → 8 → 15 → 9 → 28 → 29 → 20 → 2
```

State 9 initializes and uploads only if the server has no Bank. Substate 7 must
wait for the native success callback before requesting the complete server file.
Upload failure retains native error cleanup; the pre-upload memory object is
not treated as a downloaded file. Existing appended messages `0x61/0x62` provide
download progress and local completion; `0x74` reports local capture failure.
HOME, support-code and Mover/eShop menu
entries stay disabled or redirected. Download and Unlock reload stock Turtle
after title confirmation; other retained native initialization/save operations
use the stock backend, not `sav.bin`.

### Download confirmation, response and local installation

1. State 3 checks usable games. Without one, Download shows the appended question
   `0x73`, with **Start Download** (appended label `0x75`) and stock **Cancel**.
   Original Mode retains the HOME transfer labels. Cancellation returns via
   result 3. Confirmation sets native `+0x44=1`
   and enters substate `0x12`, the Turtle check, rather than connecting immediately.
   Native load/initialization and its success wait must finish before result 23.
2. Account, ticket and first-use states run natively. An existing server Bank
   skips creation; a missing Bank is initialized and uploaded, with substate 7
   waiting for the real upload callback. Local file availability does not choose
   the server's existing/first-use branch.
3. With a usable game, state 4 result 12 enters state 28. Without one, the native
   direct flag sends state 9 result 4 there. No selected-game recovery is needed.
4. The full-response callback `0x002D11B0` first loads via
   `SizedObject_LoadBody` or `BankFile_LoadLegacyBody`. The legacy branch copies
   `0xACA48` bytes into an initialized object and changes its version to 2.
   The capture hook at `0x002D1248` runs after either load and before native
   preservation of prior session flags.
5. The C capture helper accepts the loaded object's normal vtable, version 2
   and 100 boxes. Zero Pokemon remains valid. It writes exactly `0xBB518` bytes
   from object `+8` through the existing checked temp-write and rotation helpers.
6. Session byte `0x003FAFF1` is independent of the native state object's response
   flag: 0 means no capture attempt, 2 is set before an attempt, and 1 is set only
   after both write and installation succeed. Failure does not change the native
   response-success flag or suppress its metadata updates.
7. State 28's native direct flag avoids selected-game metadata/save phases;
   state 29 retains its remote release, callbacks, delay, sound and UI destruction.
   Download changes only successful routing to state 20 instead of HOME state 27.
   Local capture failure also reaches cleanup with the new failure message.

Empty existing Banks are downloaded, without invoking state 27's empty-Bank
warning. First-use empty Banks are downloaded only after official creation
succeeds. Request failures retain native error handling; they do not create a
fake empty file. A failed state-29 release retains the native error even if the
local copy was already installed. See the subproject README's case tables for
SD errors and interruption/retry behavior. These are client-code checks, not
an end-to-end test of first creation or server rejection.

Original Mode follows the Stock column, including native first-creation UI
timing, all menu functions, original messages, and the no-game HOME question.
It does not capture or load local Bankdata. Tickets share Download/Unlock's
public/test policy: `0` requires native results; `1` permits local results only
for the confirmed missing Azahar interface. The title mode hint is still shown.

Common startup/title handling uses the SD preview. After title exit animations,
the native shared-prompt view is released asynchronously before title destruction.
The selected record is then checked and reloaded, clearing its predecessor's
loaded flag and pending language. After applying its language and font, subsequent
states recreate the prompt/menu view with the matching archive. Missing or invalid records go
through language selection (state 1), disconnect cleanup (state 21), and game
checking (state 3), whose native chain initializes the selected backend.
Download/Unlock's added language-menu operation saves its choice to the stock
record; Offline saves to SD. Returning to the title restores the SD preview
before title UI creation. These rebinds do not copy SD data into the stock save
or mirror native writes back to SD, and write failures never switch backends.
The title keeps the selected mode across returns within this run; a fresh
launch defaults to Offline. Preview and session-backend selection remain separate.

State 15 retains its native entry, transitions, job construction and cleanup.
Only job initialization at `0x002B0444`, result polling at `0x002B0464`, and
unbinding at `0x002B1994` are redirected. Wrappers occupy added pages after
native BSS; HOME box-selection UI stays intact. Other bytes in
`[0x002B0270, 0x002B1AD0)` and the
native job implementation remain intact. Download, Unlock and Original modes call native
interfaces with `ONLINE_TICKET_CHECK_BYPASS=0`, including native failure when an
interface is missing. Test policy `1` uses local results only for Azahar's verified
unimplemented-handler reply; hardware, unknown environments, implemented
interfaces and ordinary errors remain native. Offline Mode always supplies
console time and an expiry 999 days later. Purchase counts stay
`-1`, existing EC account fields are retained, and no free-campaign window is
provided. Native getters and exit cleanup copy the resulting entitlement and
date. Local unbinding does not access a nonexistent network client; native
destruction still runs. No system eShop applet or loading-animation simulation
is used. Both paths remain present, and only the policy byte at `0x003FEAD4`
changes between builds. Initialization records this job's backend at `0x003FAFF4`;
polling and cleanup keep that selection. The policy does not alter other networking,
transaction recovery, mileage calculations, or post-download exit branches.
The subproject README documents the capability probe.

Local results are not display-only: `0x002B1924` copies the job's current date
at `+0xB8` to shared `+0x28`. First Bankdata initialization at `0x002AE8B0`
reads that date; state 9 serializes the body and uploads it. Mileage at
`0x002AD1BC` also reads the date and updates mileage/date fields; Unlock and Original
can reach the native save path, whose `0x002B2320` serializer stages the body
for upload. Download Mode with an existing bank skips mileage and normal saving,
but not first creation.

The server download callback at `0x002D11B0` replaces the Bankdata body and
transaction descriptor, not shared ticket dates. Refreshing native tickets later
does not undo earlier uploaded Bankdata changes. No direct serialization of the
999-day expiry or entitlement display fields into that body was identified;
purchase count `-1` skips the native stored-count/reward update. These findings
do not establish server-side correction of client-uploaded values. Public policy
stays `0`, and test policy is not a guarantee of safe online use.

With no usable game data in Offline/Unlock, `0x002AC958` branches to the existing failure substate
at `0x002ACA90` after the stock message completes and its text is cleared.
Result `3` returns through `state 3 → 20 → 2` without creating HOME choices,
setting the HOME direct-entry flag, or starting a network session. All ten
language archives retain the first two stock warning pages and both page-break /
input-wait tags. The second page must be confirmed before returning to the
title; only the final HOME question is removed. The feature-menu HOME entry
still redirects to language selection in the first three modes. Download uses
the appended direct-download question `0x73` with native confirm/cancel callbacks.
Confirmation first completes the native Turtle check/initialization, then uses
result `23` and the direct-entry flag for account checks, first creation, and
state 28 download. Cancellation follows native failure/title
return. Original Mode retains the complete HOME question.

Unlock Mode retains native transaction requests and recovery validation in
states 11/18/17. At the stock state-23 challenge screen, the server response owns a vector
of accepted candidate values. The original validator compares the entered
eight-digit number against every candidate modulo `100,000,000`. Unlock Mode
uses the same rule to place the first candidate in message number register 1
and appends it as a third prompt line. The stock challenge value in register 0,
input validation, rollback request, success/failure result, and state-23
destinations remain unchanged. An empty vector retains the stock two-line prompt.

Unlock Mode adds completion notices and mismatch reselection:

- **Native matching/recovery succeeds:** finish state 17, including any selected-game
  transaction commit/rollback, game saving and its result wait. On native success
  `4`, choose the notice from the actual session recovery result: `0x76` means
  “Server status is normal. Unlock Mode is not needed.”, `0x77` means “Automatic
  recovery completed. The server lock has been cleared.”, and `0x78` means “Forced
  unlock completed. The server lock has been cleared.” Retain native waiting-sound
  stopping, loading-pane hiding and message animation setup. Wait for native
  message result `4` (acknowledgment), then return disconnect result `3 → state 20`.
  All three results share this exit, without entering state 16, mileage or boxes.
  Error `3` and game-save failure `20` retain native routes and cannot report success.
- **Locked without the combination:** keep the stock mismatch notice in state 18.
  Only a native update from substate `10` that returns pending `0` and advances
  to `12` identifies acknowledgment; change that result to `19`. The native flow
  controller still waits for local saving and remote-client cleanup, unbinds and
  destroys the client and old state, then reconstructs state 10 and calls its
  native initializer. This is the first menu entry's post-confirmation
  `12 → state 10` path, not a menu return or a jump into an old UI. An asynchronous
  network error returns complete `1` with error `3`, so it cannot trigger this retry.

Runtime byte `0x003FAFF6` stores this session's result: `0` none, `1` after state
23's successful official forced rollback `4 → 17`, or `2` after state 18's native
automatic recovery succeeds `4 → 17`. Title session binding clears it. Matching
a save, holding the combination, opening the challenge UI or entering a candidate
does not set a success result early. The byte is not serialized into Turtle or
Bankdata. Cancellation, empty candidates, official rollback failures and other
network errors retain native results; the other three modes do not use these
Unlock-only branches.

### Offline save transaction

Offline state 7 keeps the stock game-save ordering while replacing remote
file operations:

```text
serialize 0xBB518
  → write bankdata.tmp
  → preserve Turtle 0x00–0x1B, set only 0x1C to 2, and save via stock serialization
  → local commit: rotate bankdata.bin/.bak, then clear Turtle 0x1C and save

selected-game save failure
  → preserve Turtle 0x00–0x1B, set only 0x1C to 1, and save
  → local rollback: delete bankdata.tmp, then clear Turtle 0x1C and save
```

Local staging does not create a server 32-byte descriptor. The patch therefore
does not overwrite Turtle with an empty descriptor; it changes only `0x1C`
through the stock setter and retains stock object serialization and checksum
generation.

If an old patch or interrupted run leaves `sav.bin+0x1C = 1/2`, Offline Mode
does not enter stock state 18. State 11's corresponding branch is redirected
to state 17, and a successful local state-16 load restores state `0` and saves
it. Exiting before that load leaves the value temporarily intact, but the next
completed offline path clears it.

## Boundaries

- Local Bankdata validation checks fixed size, format version, and box count;
  it is not equivalent to server-side content validation.
- Download Mode still requires valid account, transaction, and complete-file
  responses from the official service.
- Unlock Mode can display only a candidate actually present in the server
  response; it cannot guarantee that the service accepts the later rollback.
- Offline Mode neither interprets nor fabricates a remote descriptor;
  `bankdata.tmp/.bin/.bak` forms a separate local file transaction.
- This document states branches implemented in the current binary and patch;
  unpublished server-side checks are not asserted as facts.
