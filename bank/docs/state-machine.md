# Pokemon Bank Stock State Machine and Patch Differences

This document records the outer state machine, network paths, and transaction
branches in the supported stock binary, then compares them with the current
combined patch's Download, Offline, and Unlock modes. See
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
| Turtle backend | Stock `data:/turtle` storage | Redirected to `sd:/3ds/Bank/sav.bin` | Redirected to the same `sav.bin` | Redirected to the same `sav.bin` |
| state 5 | Real connection | Stock | Complete the session locally; no remote job | Stock |
| state 8 | Server account summary | Stock | Classify existing/first-use from local `bankdata.bin/.bak` | Stock |
| state 15 | Remote entitlement/campaign | Supply required ticket fields locally; disable online campaigns | Same as Download Mode | Same project-wide local result |
| state 9 | Server-side first creation | Stock creation and upload | Create the initial local `bankdata.bin` | Stock creation and upload |
| state 11 | Select recovery path | Stock | Keep the test, but redirect result `9` to state 17 instead of state 18 | Stock |
| state 18 | Current Turtle remote recovery | Stock | Never entered | Stock; challenge UI may append the first server candidate |
| state 17 | Selected-game remote recovery | Stock | Keep stock UI timing without remote commit/rollback | Stock |
| state 16 | Download complete file | Stock download plus local capture | Load the complete object from `bankdata.bin` | Stock download; no local capture |
| state 12/13 | Local mileage plus online gifts | Skipped after download | Keep local mileage; skip only online-gift lookup | Stock |
| state 25 | Normal Bank Box | Not entered after capture | Stock Bank Box | Stock Bank Box |
| state 7 | Remote stage, game save, commit/rollback | Normally not reached; stock if reached | Local file stage, commit, and rollback | Stock |
| state 19/20 | Remote release and disconnect | Stock no-save exit after capture | Local cleanup and title return | Stock |

Download Mode is not a full stock mode. It retains real networking, first-use
creation, transaction recovery, and complete-file download, but state 15 uses
the local ticket result and a successful capture immediately takes the stock
no-save exit. HOME, support-code, and Mover/eShop menu operations are also
disabled or redirected by the patch. Because the Turtle backend is redirected
globally, server transaction descriptors written in Download Mode are stored
in `sav.bin`, not the stock logical-record backend.

Unlock Mode follows the stock state 11/18/17 transaction-recovery branches. At
the stock state-18 challenge screen, the server response already owns a vector
of accepted candidate values. The original validator compares the entered
eight-digit number against every candidate modulo `100,000,000`. Unlock Mode
uses the same rule to place the first candidate in message number register 1
and appends it as a third prompt line. The stock challenge value in register 0,
input validation, rollback request, success/failure result, and continuation
remain unchanged. An empty candidate vector retains the stock two-line prompt.

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
