# Pokemon Bank Code Analysis

This document records the supported stock binary's memory/data structures,
state-machine and transaction branches, and differences in the current patch.
See [`../README.md`](../README.md) for the maintained implementation,
transaction rules, and independent build procedure.

## Target and memory layout

- Title ID: `00040000000C9B00`
- Internal software version: v1.5
- Image base: `0x00100000`
- Decompressed code SHA-256: `2DCE4796F54807CF8A67F1CE6297BF472D969B30ED7A7E8E25C2A6C2BDC40ABF`

| Region | Virtual range | Properties |
|---|---|---|
| `.text` | `0x00100000–0x00314000` | Read/execute; actual content ends at `0x00313910` |
| `.rodata` | `0x00314000–0x0036A000` | Read-only, non-executable |
| `.data` | `0x0036A000–0x003AC000` | Read/write |
| Logical `.bss` | `[0x003ABACC, 0x003FA904)`, length `0x4EE38`; RW pages extend to `0x003FB000` | Read/write, zero-initialized |
| Thread stack | Length `0x40000` | ExHeader setting |

The last non-zero image byte in `.text` is before `0x00313910`, leaving
`0x6F0` bytes in the mapped executable page. Ghidra's last referenced
instruction/data location is `0x003138EC`, and its last function ends at
`0x003138F3`. The following `.rodata` region is non-executable.

The image also has zero-filled tails in `.rodata` (`0x00369370–0x0036A000`,
`0xC90` bytes) and `.data` (`0x003ABACC–0x003AC000`, `0x534` bytes), but they
are not equivalent to executable free space. Ghidra records references as far
as `0x00369374` and `0x003ABFE0`, including locations whose stored bytes are
zero, so raw zero scanning alone is not a safe allocation rule.

Increasing only ExHeader text size overlaps rodata at `0x00314000`. An
alternative preserves native segment and variable addresses: materialize
original BSS as zero-filled data, add pages after it, then enable execution on
those pages from a startup loader. The maintained BPS/ExHeader layout and
preserved-function ranges are described in the
[developer document](../README.md#injection-space-provenance-and-bounds).

## Main flow and function addresses

| Address | Function |
|---:|---|
| `0x002A5580` | Select the next state from the current flow result |
| `0x002A5A7C` | Factory for 30 state objects |
| `0x002AF1FC` | Network connection state update |
| `0x002AF460` | Complete Bank-data download and metadata synchronization; shared by states 16/28 |
| `0x002ACBDC` | Startup remote-record and transaction-status query |
| `0x002AE568` | Initial remote Bank file creation state |
| `0x002AE8B0` | Initialize the complete first-use Bank object |
| `0x002A37E0` | Submit the serialized initial Bank file |
| `0x002B0270` | Entitlement, ticket, or subscription check |
| `0x002AD1BC` | Points/reward eligibility decision |
| `0x002A9750` | Points/reward receipt state |
| `0x002A7094` | Bank Box UI entry |
| `0x002B1CF8` | Save, commit, and rollback state machine |
| `0x002ABC24` | Return-to-title state |

### Outer state table

Vtable slot `+0x0C` is the per-frame update method. After an update completes, `0x002A5580` selects the next state from the state number and the result byte at object offset `+0x30`. “Confirmed” means dedicated calls, resource text, and transitions agree; “Partial” means the position in the flow is known but the exact business label still needs dynamic confirmation.

| State | Update | Working classification | Confidence |
|---:|---:|---|---|
| 0 | `0x002A9204` | Patch or application-version check | Confirmed |
| 1 | `0x002A9320` | Language selection | Confirmed |
| 2 | `0x002B1AD0` | Title screen | Confirmed |
| 3 | `0x002ABE08` | Game-software confirmation and entry dispatch; can set the HOME direct-entry flag | Confirmed |
| 4 | `0x002A6750` | Feature menu; results `12/13/23` select Bank, another menu action, and Pokémon HOME | Confirmed |
| 5 | `0x002AF1FC` | Internet/service-session connection | Confirmed |
| 6 | `0x002AED78` | Outdated-application handling | Confirmed |
| 7 | `0x002B1CF8` | Bank save transaction: serialize, remote stage, game save, commit/rollback | Confirmed |
| 8 | `0x002ACBDC` | Query remote-record existence/account mode and transaction status; it does not receive the complete Bank-data body | Confirmed |
| 9 | `0x002AE568` | Create, serialize, and upload a first-use Bank file | Confirmed |
| 10 | `0x002ADC50` | Game-software selection after Use Bank | Confirmed |
| 11 | `0x002AF034` | Check server update/transaction state and select a recovery path | Confirmed |
| 12 | `0x002AD1BC` | Decide whether points or rewards are pending | Confirmed |
| 13 | `0x002A9750` | Receive pending points or rewards | Confirmed |
| 14 | `0x002A8404` | Return to the feature menu after the eShop action | Confirmed |
| 15 | `0x002B0270` | Entitlement, ticket, or subscription check | Confirmed |
| 16 | `0x002AF460` | Post-selection complete Bank-data download and metadata synchronization | Confirmed |
| 17 | `0x002A93F4` | Other-side transaction reconciliation/retry | Confirmed |
| 18 | `0x002A8760` | Current-user transaction reconciliation/retry | Confirmed |
| 19 | `0x002AEB9C` | Remote transaction rollback/release for the no-save path | Confirmed |
| 20 | `0x002ABC24` | Normal disconnect, cleanup, and return to startup flow | Confirmed |
| 21 | `0x002ABC24` | Silent disconnect/title-return variant used after language selection | Confirmed |
| 22 | `0x002A9118` | Save-error handling | Confirmed |
| 23 | `0x002AD7BC` | Forced rollback or transaction recovery | Confirmed |
| 24 | — | Outer-flow termination sentinel; no state object is created | Confirmed |
| 25 | `0x002A7094` | Bank Box UI; result `4` saves and result `21` exits without saving | Confirmed |
| 26 | `0x002ACB34` | Auxiliary Pokédex-related entry; returns to state 0 | High |
| 27 | `0x002A7BF0` | Pokémon HOME transfer UI and operation controller | Confirmed |
| 28 | `0x002AF460` | HOME-mode complete Bank-data download and metadata synchronization | Confirmed |
| 29 | `0x002AFE94` | Remote transaction rollback/release before entering HOME operations | Confirmed |

Core normal-Bank chain:

```text
0 → [1 → 21] → 2 → 3 → 5 → 8 → 15 → 9 → 4
4 --result 12--> 10 → 11 → [18] → 17 → 16 → 12
12 --pending reward--> 13 → 25
12 --no pending reward--> 25
12 --rollback return--> 19 → 20
25 --result 4--> 7                         save
25 --result 21--> 19 → 20                no-save exit
```

Pokémon HOME is a separate path:

```text
Feature menu: 4 --result 23--> 28 → 29 → 27 → 19 → 20
Direct HOME: 3 sets the direct flag → 5 → 8 → 15 → 9 → 28 → 29 → 27
```

State 28 and state 16 share the same update function, but the factory sets mode byte `+0x41=1` for state 28; its initializer additionally selects the server-connection message. States 29 and 19 have the same structure and both call `0x001D5C28` to handle an unfinished remote transaction, but they continue to HOME controller state 27 and normal disconnect state 20 respectively. State 13 is unrelated to these cleanup states; it receives pending points or rewards.

```text
Initial game and account validation
  → ACT/NNID and network session
  → state 8 queries remote-record/account mode and transaction status
  → state 15 checks account entitlement, ticket, or subscription state
  → state 9 officially creates and uploads the initial file for a new account
  → feature menu
  → select Use Pokemon Bank and a game
  → transaction recovery and the state-16 Bank-data download
  → state 12/13 decides and receives pending points or rewards
  → Bank Box UI
  → serialize, stage remotely, save the game, commit or roll back
  → title screen
```

## Network states

The outer transition table places state 15 and state 9 after the startup state-8 operation, followed by state 4 (the feature menu). State 8 calls the remote interface at vtable slot `+0x11C`; its callback copies a small remote-record descriptor and then queries transaction status. It does not pass a `0xBB518` data pointer to `0x002D11B0`.

Selecting Use Bank later performs transaction recovery and reaches state 16. The HOME path reaches state 28. States 16 and 28 share `0x002AF460`, call the remote interface at vtable slot `+0x16C`, and receive the complete Bank-data body through `0x002D11B0`.

Complete Bank-file request chain:

```text
0x002AF460 BankDataSyncState_Update (state 16 or 28)
  → 0x002A3550 start the complete-data request
  → 0x002D11B0 BankRemote_DownloadSuccessCallback
  → load the Bank object
  → synchronize the accompanying metadata
  → completion result
```

The eleventh argument of `0x002D11B0` is the complete data pointer. It is held in `r7` and passed to vtable slot `+0x0C` at `0x002D1234`. Vtable slot `+0x10` supplies the file length.

`0x002AF460` substates:

| State | Function |
|---:|---|
| 0 | Create the remote job and callback context |
| 1 | Request the complete file and its accompanying descriptor |
| 2 | Wait for the complete-data callback; HOME mode can continue directly from here |
| 3–9 | Apply and synchronize the accompanying metadata |
| 10/11/12 | Return success, ordinary failure, or service error |

### First-use Bank-file creation

State 9 at `0x002AE568` checks a shared account-mode byte. It performs creation only when that byte is ASCII `'5'`; other accounts skip the creation stages. The current binary implements the following path:

```text
state 15 completes the account-entitlement check
  → state 9 phases 4/5 obtain official initialization inputs
  → 0x002AE8B0 initializes 100 Bank Boxes, auxiliary tables, and date fields
  → BankObject vtable +0x10 returns 0xBB518
  → BankObject vtable +0x08 serializes into a new buffer
  → 0x002A37E0 submits the initial Bank file
  → state 9 reports success only after the asynchronous server callback
```

This path does not call local FS and does not wait for a later Bank Box save. `0x002A37E0` builds the upload data source and invokes the remote interface; the creation state contains none of the stage/commit/rollback calls used by an ordinary save. Consequently, once first-use creation succeeds, the initial server record already exists even if the user subsequently takes the normal no-save exit. If creation fails, state 9 reports failure and a later session will enter the missing-file/creation path again.

The transfer stack combines a control-plane request with HPP/HTTP:

| Address | Function |
|---:|---|
| `0x00198A78` | Prepare and start an HTTP request |
| `0x00199570` | HTTP streaming send/receive worker |
| `0x001C1EF8` | Build a multipart file stream containing `name="file"` |
| `0x001C22F0` | Emit multipart prefix, file body, and suffix |
| `0x00245414` | Send an HTTPC chunk |
| `0x002454A4` | Finish the POST body |
| `0x002454C4` | Receive the HTTP response |
| `0x002CC878` | Prepare BankObject GET |
| `0x002CC988` | Validate response `Content-Length` |
| `0x001B23E8` | Initialize an HPP POST job |
| `0x001B2544` | Process HPP result, redirects, and retries |
| `0x001B2820` | Rebuild a retry request |

The four HPP resources are `CACERT_PUBLIC_CA_5.der` through `_8.der`, not bankdata fragments. The HTTP layer follows up to five `307` redirects, retries `409/500`, and limits cached error responses to `0x2800` bytes.

## BankObject and bankdata

The Bank object vtable is `0x003626FC`. The runtime object header is eight bytes; the file corresponds to `obj_bank + 8`.

| Vtable slot | Address | Function |
|---:|---:|---|
| `+0x08` | `0x002CB870` | Serialize the body at object `+8` |
| `+0x0C` | `0x0023650C` | Load input into object `+8` |
| `+0x10` | `0x002CB864` | Return `0xBB518` |
| `+0x14` | `0x00116294` | Initialize the current format |
| `+0x18` | `0x002BA490` | Load and upgrade the `0xACA48` legacy format |
| `+0x1C` | `0x002CB84C` | Check `u16(data+0x15C)==2` |

Runtime object chain:

```text
slot_address   = *(u32*)0x002ACEEC     ; 0x003AB938
root           = *(u32*)slot_address
manager        = *(u32*)(root + 0x1C)
obj_bank       = *(u32*)(manager + 0xCC)
bank_file_data = obj_bank + 8
```

The object must satisfy `*(u32*)obj_bank == 0x003626FC`. The disk file is exactly `0xBB518` bytes. Minimum load checks are `u16(data+0x15C)==2` and `u16(data+0x15E)==100`.

### bankdata layout

| File offset | Length | Content |
|---:|---:|---|
| `0x000000` | `0x17C` | Header, names, version, and state fields |
| `0x00017C` | `100 × 0x1B56` | 100 Bank Boxes, each with 30 `0xE8` slots and box metadata |
| `0x0AAF14` | `30 × 0xE8` | 30 Transfer Box slots |
| `0x0ACA44` | `3000` | Per-slot Bank format tags |
| `0x0AD5FC` | `30` | Per-slot Transfer Box tags |
| `0x0AD61A` | `2` | Reserved/alignment |
| `0x0AD61C` | `8 × 0x44` | Source-game summaries |
| `0x0AD83C` | `0x7260` | Pokedex-like aggregate data |
| `0x0B4A9C` | `4` | Deposit/withdrawal counters |
| `0x0B4AA0` | `3000` | Source software IDs for Bank slots |
| `0x0B5658` | `3000 × 8` | Update timestamps for Bank slots |
| `0x0BB418` | `0x100` | Tail flags and reserved bytes |

Header fields include a 64-bit identity-bound value at `+0x000000`, ten fixed UTF-16 name buffers at `+0x000008`, date fields at `+0x000160`, remote content identifiers at `+0x000168/+0x00016C`, counters at `+0x000170/+0x000174`, and four flag bytes at `+0x000178`.

The current-format load and serialize methods perform fixed-size copies. No client-side bankdata checksum, MAC, compression, or decryption routine was identified in this object layer. Server-side upload rules remain separate.

## Save and upload

```text
0x002B2320 BankSave_SerializeAndStage
  → 0x002B2490 completes 0xBB518 serialization
  → 0x002B24A0 / 0x002A2504 stages the remote update
  → game and local save operations
  → 0x001D5D74 commits
  → 0x001D5C28 rolls back on failure
```

The 32-byte transaction descriptor belongs to remote stage/commit/rollback control and is not part of bankdata. Faking one HTTPC return value is insufficient because the outer state machine still waits for HTTP status, response body, control-plane result, and asynchronous completion flags.

## State machine, transactions and patch differences

The conclusions below come from the current decompilation and the patch's
actual hook sites. Four different kinds of values must not be conflated:

- outer state IDs are `0–29`;
- each state object writes a business result at object offset `+0x30`, which
  `0x002A5580` uses to select the next state;
- Turtle offset `+0x1C` stores a separate persistent transaction state `0/1/2`;
- HTTP statuses such as `200/307/409/500` belong to the lower transport layer.

### Outer controller

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

### Exact stock transitions

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

### Stock scenarios

#### Ordinary startup with an existing language

```text
0 → 2 → 3 → 5 → 8 → 15 → 9 → 4
```

State 8 obtains record existence, account mode, and a transaction summary; it
does not download the complete `0xBB518` Bank file. State 15 handles
entitlement, ticket, campaign, and online-gift data. State 9 skips creation for
an existing record and creates the initial file only when the account mode
indicates that no remote record exists.

#### First launch and language selection

```text
0 → 1 → 21 → 2
```

State 1 stores the language choice. State 21 reuses the title-return
implementation with its quiet variant enabled.

#### First use without a server-side Bank file

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

#### Use Bank without a pending transaction

```text
4 --result 12--> 10 → 11 → 18 → 17 → 16 → 12
12 --nothing to receive--> 25
```

State 11 selects a recovery entry from account mode and the selected game's
record. State 18 checks the current-user Turtle transaction, while state 17
checks the selected-game transaction block. When neither side has pending
work, both finish their consistency checks and state 16 requests the complete
Bank file.

#### Pending local points or a reward

```text
16 → 12 --result 5--> 13 --result 5--> 25
```

State 12 decides whether receipt UI is needed and state 13 performs it. With
nothing pending, state 12 returns result `6` or `12` and enters state 25.

#### Leave without saving

```text
25 --any result except 4--> 19 → 20 → 2
```

State 19 releases or rolls back a remaining remote staged transaction. State
20 disconnects and returns to title. This is distinct from save-failure state
22.

#### HOME path

```text
Feature menu: 4 --result 23--> 28 → 29 → 27 → 19 → 20
Direct HOME:  3 --result 23--> 5 → 8 → 15 → 9 → 28 → 29 → 27
```

States 28 and 16 share the complete-file download implementation, but the
HOME mode byte selects a HOME-specific continuation. States 29 and 19 both
clean up pending remote work; state 29 continues to HOME controller state 27.

### Stock network layers

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

### Stock Turtle transaction record

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

#### Stock successful save

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

### Current patch versus stock

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

#### Download confirmation, response and local installation

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
`-1` and existing EC account fields are retained. The local free-campaign window
reuses the current/expiry pair in the native decimal timestamp format. Native
state 15 calculates the free flag, remaining days and hours and selects the
free-use text. Each local initialization rebuilds the window from console time
without changing the mileage reference or relying on fixed server campaign dates.
Native exit cleanup copies the resulting entitlement and date.
Local unbinding does not access a nonexistent network client; native
destruction still runs. No system eShop applet or loading-animation simulation
is used. Both paths remain present, and only the policy byte at `0x003FEB68`
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

#### Offline save transaction

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

### Boundaries

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
