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
| Main thread stack | Length `0x40000` | ExHeader setting; SDK background threads allocate their own stacks |

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

### Title language and user selection

The stock console `data:/turtle` record is the language authority. Startup
`0x002A47F4` loads and validates it, and `0x0025C08C` applies its language. A
missing record follows native first-language initialization before title selection;
I/O errors never authorize formatting.

Title L uses native confirmation `0x002D139C` and follows `2 → 1 → 21 → 2`.
After the native save succeeds, the selected user's SD settings are synchronized:
copy the full validated `0x200` bytes when missing, otherwise update only
`0x28–0x2B`. Identity, transaction fields, CRC and opaque tail remain intact.
Native language and font selection do not change when switching modes or users.

Offline account confirmation retires the previous local session/body cache even
for the same user, then retires a settled Bank baseline when both the main and
backup were deleted. It publishes the empty baseline before discarding a sole
Bank tmp and entering native first-use creation. Settings retain their own
recovery authority; active/corrupt journals and source/settings work cannot reset.

The current mode is a separate single-line pane above START. R-mode, L-language
and HOME hints sit below it. All use native font size and integer coordinates;
their lower-screen centers are `88/120/156/182/208` for mode/START/R/L/HOME.
The START and copyright assets stay in place. Layout and font formats follow
the original resources; see [font coverage](../../common/docs/fonts.md).

Offline and Download titles show the current user in dark gray at the upper left.
The registry is refreshed at title/local-mode entry. Browsing uses an in-memory
list, with auto first and the highest numbered user last, without wrapping.
D-pad and Circle Pad up/down support repeat and acceleration; C-stick is ignored.
The worker publishes the final selection, and entry waits for acknowledgement.
Download selects a local destination, not a different official online account.

Unavailable actions retain their native name plus a localized disabled suffix.
Support code remains clickable in every mode and uses native encoder `0x002A4078`.
ACT/FRD replies are observed for errors and placeholder values; those conditions
add an Invalid label without replacing the identity or making an extra request.
This label is not official account authentication or a ban check.

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

### Slot movement and update time

The 3000 64-bit timestamps at `0x0B5658` are independent of Poké Mile balances,
reward anchors and ticket expiry. Native code obtains a packed date and converts
it to seconds since `2000-01-01` with `0x001F2BD4`. The object's eight-byte header
makes the corresponding runtime field offset `0x0B5660`.

| Native operation | Date call | Timestamp behavior |
| --- | --- | --- |
| `0x001D86E0`: write a Bank slot | `0x001D887C` | Give the newly registered slot the current time; Transfer Box/game deposits can use this path |
| `0x001D95C8`: clear a Bank slot | `0x001D96E4` | Update the cleared slot; withdrawals and replacement operations can also use this path |
| `0x002B9C94`: move a group of Bank slots | `0x002B9DC4` | Stamp cleared source slots and preserve the original records' timestamps at the destination |
| `0x002BA148`: exchange individual Bank slots | No new date query | Exchange Pokémon, format tags, source software IDs and existing timestamps together |

The first three paths call `0x001D3BF4` with the network context at root-object
offset `+0x138`. It checks the date-output pointer but not the context pointer;
the subsequent `0x0022E3E0` requires a valid network object and mutex. Offline
Mode creates neither, so it cannot use this query. This explains a failing
Transfer Box deposit alongside a working single-slot exchange within Bank.

Only these three `BL` sites are redirected to `CombinePatch_SelectBankSlotDate`.
Offline Mode reuses the current console date from shared `SystemTime_GetCurrentDate`;
other modes pass the original arguments unchanged to `0x001D3BF4`. The local
date has no 999-day adjustment. A null output or unavailable date returns
failure, leaving each native caller's existing failure branch intact. All other
slot-write, clear, group-move, timestamp-conversion and single-slot-exchange
instructions remain native. Shared network-context and mutex functions are not
replaced.

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
| Turtle backend | Stock `data:/turtle` storage | Stock backend after mode confirmation | Redirected to `sd:/3ds/Bank/client/users/<userID>/sav.bin` | Stock backend after mode confirmation |
| state 5 | Real connection | Stock | Complete the session locally; no remote job | Stock |
| state 8 | Server account summary | Stock | Classify existing/first-use from local `bankdata.bin/.bak` | Stock |
| state 15 | Remote entitlement/campaign | Public policy `0` stays native; automatic policy `1` uses local results only for a confirmed missing Azahar interface | Always supply required ticket fields locally; disable online campaigns | Same policy as Download Mode |
| state 9 | Server-side first creation | Stock creation, upload and asynchronous success wait in substate 7 | Create the initial local `bankdata.bin` | Stock creation and upload |
| state 11 | Select recovery path | Not entered | Native existing → 17 / pending → 18 split | Stock |
| state 18 | Current Turtle remote recovery and game matching | Not entered | Native ID/version matching, state-1 rollback/state-2 commit and mismatch UI; service calls local | Native requests, validation and combination; an acknowledged mismatch recreates state 10 after cleanup |
| state 23 | Official forced-unlock challenge and rollback | Not normally entered | Native challenge request receives three local candidates; native keyboard validation precedes service-authorized rollback; error/cancel branches retained | Display the first server candidate; successful result `4 → 17` latches completion, with other native branches unchanged |
| state 17 | Selected-game remote recovery | Not entered | Native reading, 1/2 decisions and token cleanup/save; service calls local | Finish native transactions/game saving, show no-recovery/automatic-recovery/forced-unlock result, then disconnect after acknowledgment |
| state 16 | Download complete file | Not entered | DownloadBank loads the local body plus a state-1 reservation; native source/Turtle saves continue | Not normally entered |
| state 28 | Game-independent complete download | Keep native `+0x41=1`; capture the loaded current-format record in its success callback | Not normally entered | Not normally entered |
| state 29 | Pre-HOME remote release and UI cleanup | Keep native job and destruction; success enters state 20 instead of state 27 | Not normally entered | Not normally entered |
| state 12/13 | Local mileage plus online gifts | Skipped after download | Keep local mileage; gift lookup uses optional local events | Not normally entered |
| state 25 | Normal Bank Box | Not entered after capture | Stock Bank Box | Not normally entered |
| state 7 | Remote stage, game save, commit/rollback | Normally not reached; stock if reached | Local file stage, commit, and rollback | Stock |
| state 19/20 | Remote release and disconnect | Disconnect in state 20 after state 29 completes release | State 19 releases the load reservation, including Box no-save exits; native prompt/timed finalizer then state 20 cleanup | Stock |

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
All ten language resources call the local endpoint the "local server" in
connection/save/download messages and explicitly say "official server" in
Unlock Mode notices. Paths/filenames are omitted. Dialogs use at most three rows;
mode descriptions leave one row for the native greeting. The three-row balance
notice explains that each offline virtual top-up adds an annual-pass price and
that simulated credit/balances have no relationship to official eShop funds or
purchases. Mover's corresponding offline connection/save texts are aligned.
Connection, purchase, download and publication behavior is unchanged.
HOME and Mover/eShop entries stay disabled; Support ID retains its native
console-identity path. Download and Unlock reload stock Turtle
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
5. The C capture helper checks the loaded object's normal vtable and freezes
   exactly `0xBB518` bytes from object `+8` before native session-flag changes.
   It performs no SD access. A checked native-heap allocation uses heap 23;
   allocation failure does not write. Download updates nonblockingly queue
   `EXPORT_BANK` (operation 34), copying into task-owned storage before freeing
   the transient snapshot. The worker checks version 2/100 boxes and performs
   the BANK_ONLY stage, verification, commit and publication. Empty Banks are
   valid; unresolved transactions block replacement.
6. Session byte `0x003FAFF1` is independent of the native response flag:
   0 means not attempted, 2 is set before capture, 3 means export pending,
   and 1 requires successful publication and release of the worker result.
   Busy submission/release returns the original update's WAIT value without
   advancing its substate. Failure preserves native response/metadata processing.
   The destructor at `0x002AFE70` cancels delivery before replaying its original
   vtable load and resuming at `0x002AFE74`; running file effects are not undone.
7. State 28's native direct flag avoids selected-game metadata/save phases;
   state 29 retains its remote release, callbacks, delay, sound and UI destruction.
   Download changes only successful routing to state 20 instead of HOME state 27.
   Local capture failure also reaches cleanup with the new failure message.

Empty existing Banks are downloaded, without invoking state 27's empty-Bank
warning. First-use empty Banks are downloaded only after official creation
succeeds. Request failures retain native error handling; they do not create a
fake empty file. A failed state-29 release retains the native error even if the
local copy was already installed. SD errors and interrupted installation do not authorize bypassing official release or error handling.

Original Mode follows the Stock column, including native first-creation UI
timing, all menu functions, original messages, and the no-game HOME question.
It does not capture or load local Bankdata. Tickets share Download/Unlock's
automatic/native-only policy: `0` requires native results; `1` permits local results only
for the confirmed missing Azahar interface. The title mode hint is still shown.

Startup uses only the native save for language and first-language initialization.
Title L saves natively, then synchronizes SD: complete copy if missing, language
fields only if present. No mode has an internal language switch. Mode confirmation
binds settings/transaction storage without applying a backend language or reloading
fonts/messages. Title returns retain the selected mode; a fresh launch defaults
to Offline. An SD failure never imports SD settings into the native save.

State 15 retains its native entry, transitions, job construction and cleanup body.
Initialization at `0x002B0444` selects the backend; polling at `0x002B0464` and
the outer unbind call at `0x002B1994` remain native. Both backends run original initialization;
`0x002B0464` is completely unmodified. Send provider `0x0021EB10` supplies
service response bytes to the original parsers. Wrappers occupy added pages after
native BSS; HOME box-selection UI stays intact. Four additional Offline-only UI
hooks skip applet screen clearing at `0x002B0374` and fades at
`0x002B0404/0x002B0438`. At `0x002B19D0`, only successful Offline phase `4`
retains the connection message for the local-data transition. No-pass/failure
exit animation, destruction, callback cleanup and resource release remain native.
Other modes replay the original instructions/calls at all four UI sites,
including online ticket compatibility response. Original Yes/No and three-choice purchase observers
are unchanged. Local purchase polls at `0x002B110C/0x002B1248` additionally handle an
acknowledged insufficient-funds notice before resuming the native retry branch; native backends
forward directly to the original poll. Other bytes in `[0x002B0270, 0x002B1AD0)` remain intact.
Inside the job, only system binding/unbinding, shop/EC calls and the send-provider
boundary are adapted; its state machine, parsing and cleanup remain native. Download, Unlock and Original modes call native
interfaces with `ONLINE_FAKE_SERVER_TICKET_FALLBACK=0`, including native failure when an
interface is missing. Automatic policy `1` uses fakeServer results only for Azahar's verified
unimplemented-handler reply; hardware, unknown environments, implemented
interfaces and ordinary errors remain native. Online fallback uses an independent read-only
backend-2 policy with the fixed window `2000-01-01 00:00:00..2099-12-30 23:59:59`
shared by ticket expiry and campaign responses: no offline configuration, pass dates, purchase counts or transaction-bound
policy are consumed. Catalog/history responses are empty native vectors and count slots stay
`-1`; shop, credit and purchase operations cannot report success or write files. The initial
campaign request selects the current backend; only campaign waiting is adapted online,
while account-record and transaction queries retain their native providers.
Online fallback currently covers only the fixed free window, not the complete
historical shop. Outside that window, the zero-expiry reply makes original ticket
job `0x002A1CAC` request shop initialization at `0x002A1EE8`. The fallback returns
`FS_UNSUPPORTED`, so the job fails rather than completing with no entitlement;
existing-account paths in outer state 15 substate 2 then finish through substate
52 before the original purchase prompt. Native behavior is not unconditional
disconnection outside a free period: successful shop/product queries are required
before the purchase decision. Forcing job success or an entitlement flag would
not fix this; the fallback also has empty product/history vectors and must not
consume offline purchases, credit or configuration online. The complete purchase
prompt needs separately verified shop/product responses and is not yet restored.
The independent long window supports historical console dates without simulated online
purchases. Native expiry checks remain: dates outside the window or too close to
its end may still reach the unsupported-shop path.
Offline Mode defaults to actual
console time and expiry `2050-12-31 23:59:59`. Trial/annual counts default to
`0`; the other three remain `-1`. Existing EC account fields are retained. The default free-campaign window
starts at `2023-02-14 00:00:00` and ends at that expiry, matching the verified
official `FREE` resource in the native decimal timestamp format. Native
state 15 calculates the free flag, remaining days and hours and selects the
free-use text. Actual console time remains the mileage reference rather than
the campaign start. The automatically created `server.conf` can set shared
independent free/trial calendars and pass state, preserving valid existing values.
Independent boolean switches are `free_enable`/`trial_enable`; `free_periods`/`trial_periods`
contain only comma-separated date ranges without `mode`. Matching regional keys replace the
common key. Legacy date toggles are removed after migration. Defaults include the 2020 HOME campaign.
Pass dates use `paid_periods`; `purchased=false` ignores residue. `trial_claimed` is boolean and
annual counts remain numeric; BIN formats are unchanged. Free periods never extend purchased passes.
Missing/invalid basic fields regain built-in defaults; misplaced valid values retain their contents
and return to canonical order. Unknown/damaged entries are removed. Four blocks share one `[ticket]`
section, with blank separators and currency tuple labels only. Detailed notes live in adjacent Markdown,
not embedded, generated or loaded by the patch; canonical repair omits old explanatory comments.
Missing/malformed optional checks, words and
gift configuration add no extension without blocking connection. Real I/O errors and invalid Bank
bodies or existing transactions and Bank bodies retain native error handling rather than resetting defaults.
The service generates the native campaign body from the current applicable window, without another SD
file. No entitlement is a normal response with a zero expiry date, not a client
assignment of validity or completion. The initial native 15-day shop-refresh check remains.
Substates `12/13` re-query expiry; `14/15` retain original catalog loading. Applets
`5/6/7` dispatch at `0x002A1EE8/0x002A0560/0x002A2184`; initialization is not a purchase.
The service supplies local catalog availability and console-country currency/amount display. Original product
matching, pass descriptions and Purchase/Add Credit/Do not purchase choices remain intact,
without added menu choices or shortcut keys. The historical 30-day trial requires an enabled matching claim period, no
previous trial, no valid pass and the native first-use status `'5'`; it does not supply a `FREE` campaign.
Existing-bank statuses `'2'`/`'4'` still select the annual pass; annual counts alone do not choose the trial.
The 365-day pass is offered in every mode, with no enable switch. Original purchase request `0x002A2160` stores kind at
job `+0x148`; its other three arguments are product-description/title strings, not prices or durations.
The service adds `30/365` days for kind `0/1`, extending an active local pass or starting
from the purchase date otherwise. Add Credit and annual debit use `shop.$currency` tuples;
only balances change. Missing components use historical defaults and valid manual values remain.
Automatic currency updates `currentCurrency` from console location; manual mode uses its configured
value. Unsupported/conflicting locations are not guessed; insufficient balance denies purchase.
Currency wallets are independent. One `server.tmp` publishes the debit, local pass dates, purchased status
and the corresponding record without changing campaign dates. Mode changes also preserve passes. Original consumer
`0x002A0ADC` re-queries history and then expiry; original parsers and state decisions calculate
entitlement. Cancellation makes no purchase request; service failure supplies the original
error result. The client never assigns job phases, validity or expiry, and never overrides a
failed re-query. Native backends retain original eShop/EC calls. System EC call sites and
local checkout-result prompting are adapted; native workers and jobs continue running. The local read-only catalog view
uses the native `0x2800`-byte allocation/destructor, not platform authorization or official encrypted receipts.
The stock checkout `0x002A0D84` and result callback `0x002A0624` classify SDK results.
After an unfinished purchase, substates `39/41` read job `+0xAA`: nonzero returns to
trial substate `27` or annual substate `18`, refreshing balance and rebuilding the menu;
zero ends the ticket flow through substate `52`. Stock Bank message resources contain
no standalone insufficient-balance notice; checkout prompting belongs to the SDK/system
boundary. This establishes the client routing, not the exact original system-dialog wording.
Local checkout retains `FS_INSUFFICIENT_FUNDS` as a denied purchase, with no debit or pass
update. Poll adapters at `0x002B110C/0x002B1248` use the native UI/message view to display
localized patch line `121`, wait for confirmation, then set the native return flag to `1`.
Local values `2/3` at `+0xAA` mean notice pending/shown and are consumed before returning
to native routing; no unknown reserved fields or new persistent files are used. The native
UI restoration call reverses the checkout entry fade before the notice. Configuration,
IO and publication failures still abort, and successful purchases still re-query history
and expiry through `0x002A0ADC`. Native backends bypass this local prompt handling,
preserving original checkout, result classification, state transitions and cleanup.
Native amount copy `0x002C028C` reads 32 bytes at price-view `+4` through a bounded string copy.
The service supplies zero-padded amount strings, not embedded binary fields; original amount
function `0x002A20F8` and the balance worker pass them to the formatting boundary without changing global currency settings.
Successful SDK decoding alone does not establish usable entitlement; configuration/IO
failure follows native errors.
Native exit cleanup copies the resulting entitlement and date.
Local system unbinding does not access a nonexistent network client; native
SDK construction, parsing, polling and destruction still run. No system loading-animation
simulation is added. Both paths remain present, and only the `CombinePatch_OnlineTicketFallback` policy byte
changes between builds. Initialization records this job's backend at `0x003FAFF4`;
polling and cleanup keep that selection. The policy does not alter other networking,
transaction recovery, mileage calculations, or post-download exit branches.
The shared runtime module caches environment/SVC capabilities at startup and
ticket capability once at title initialization after services/TLS are ready.
The cache occupies `[0x003FAFE0, 0x003FAFF0)`, outside native BSS; jobs only read it.
The shared selector reads it before campaign/ticket sending, not after a failed
online request. Only confirmed Azahar plus a missing ticket capability can use
the enabled online local policy; SVC capability and date do not select it.
Detection also runs for policy `0`, which never selects local online results.
The subproject README documents the probe criteria. The two client adapters
use `FakeServerClient_TicketReply` to obtain native-format response bytes: console time is an observation, while current
date, expiry, purchase counts and campaign bounds come from the server response.
Client code does not maintain a second ticket policy or calendar conversion.

Mileage is still calculated by native state 12 (`0x002AD1BC`), not fakeServer.
It uses the stored baseline and balance, occupied Bank slots and elapsed hours,
then updates the baseline and caps the balance at `65535`. The server stores
those fields in the complete Bankdata body; it does not recompute the points.
The current time reaches this state through the ticket service response: real
networking uses the returned server time, while the local fakeServer formats
the console clock as that response. Mileage does not directly read the host clock.
Shared `system_time.c` supplies clock observations and Bank-slot timestamps,
which are separate from the mileage baseline. No replacement mileage module exists.

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
local expiry or entitlement display fields into that body was identified;
local trial/annual counts reach native stored-count/reward decisions and are not official history. These findings
do not establish server-side correction of client-uploaded values. Native-only
policy `0` remains available; automatic fallback is not an online-safety guarantee.

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

#### Offline exit before the Bank Box

Declining to continue at the state-12 full-gift warning returns result `21`;
state 13 can also terminate with result `21`. Both retain the native transition
to state 19. Allocation, binding, the native rollback request, observer and wait
are retained. Provider `0x001D5CA4` submits `FsServer_RequestTransaction`; a subsequent
wait poll completes the real SDK job and its original callback selects success/error.
The service distinguishes a clean-session zero-token acknowledgement from an actual
reservation rollback, never releasing unrelated pending work. Native prompts, the
1,500-ms final wait, unbinding and destruction precede state 20. Other modes use
the original remote provider.

State 16 already reserves a state-1 transaction when loading the Bank. Both
pre-Box exits and the state-25 no-save exit retain state 19 to release that
reservation before state 20 disconnects, without publishing edited Bank data.
The service checks the journal/files before reporting rollback success. Local
save-failure rollback belongs to state 7.

#### Offline save transaction

Offline state 7 keeps the stock game-save ordering while replacing remote
file operations:

```text
state 16: DownloadBank reads 0xBB518, snapshots old Bank and returns a state-1 reservation
  → native substates 6/7: write descriptor and state 1 to source; save and poll
  → native substates 8/9: save Turtle descriptor and state 0; continue normal flow

state 7: serialize 0xBB518; Stage updates the same live-session reservation
  → reread bankdata.tmp, persist its new full fingerprint; keep the native wait UI
  → native substates 3/4: source state 2 and descriptor; save/poll
  → native substates 5/6: save complete Turtle descriptor and state 2
  → substate 9 RequestTransaction: acknowledge the native state-2 decision; COMMIT_DECIDED → publish Bank and backups
  → retain Turtle state 2 and its descriptor, without an extra save/reset to 0
  → native substates 13/14: clear only the source's 64-bit token at +0x08; save/poll
  → native success message; commit/cleanup failures keep native error/restart recovery

selected-game save failure
  → native substates 7/8: save Turtle state 1 and descriptor
  → substate 11 RequestTransaction rollback; continue the native failure message
```

The local service generates a nonzero descriptor in native SDK layout, copied
to source and Turtle by native setters. State 11 keeps its existing/pending
split: existing routes to state 17; pending routes to state 18. State 18 compares
the Turtle data ID/current version, then the selected game's fields if needed.
Native state 1 rolls back, state 2 commits, and mismatch UI/key-combination
branches remain intact. State 17 then handles the source token and saving.
Offline state 23 retains the mismatch-only key combination, challenge, numeric
keyboard and rollback request. Provider `0x002A2838` forwards the challenge to
`FsServer_Support`, which verifies a pending reservation and generates three
distinct eight-digit candidates. Native allocation, containers and the observer
copy them to state 23. The existing third line shows the first; the native
keyboard accepts all three and retains wrong-input/retry, cancel and failure branches.
`AuthorizeRollback` then rechecks challenge, input, transaction and descriptor;
only that authorized reservation can be rolled back. Changed challenges or
transactions and post-success replay cannot reuse the authorization. Native
callbacks proceed to state 17 for token handling and saving; no other game save
is rewritten. This local password protocol is not the official algorithm:
Download, Unlock and Original continue using official requests and responses. Download's
live restaging permission exists only in server context and is not reauthorized
from a journal or game version after restart.

An existing Bank without a reservation returns native status `2`; only a
nonzero pending token returns `4`. First-time service creation cannot fabricate
a lock or empty trainer metadata. The preliminary record query omits redundant
body verification; the later transaction query still validates participants.
Complete body loading hashes that same memory snapshot. Service I/O uses
a 1 MiB buffer, large enough for one complete Bank read, with a 1 KiB stack fallback when busy. CRC32 uses journal
version 5. Status/metadata and the following download share one snapshot; only
body delivery consumes the load. Box operations do not read server files.
Local settings updates dispatch before connecting, using the fs_system settings
backend and existing durable record without policy/Bank scans. Original record
serialization/validation and save success/error branches remain.
The service's volatile loaded view reuses body observations across
read-only requests in the native load; it is neither persisted nor journal
authority. Journals/settings/configuration stay independently read. New loads,
source selection, reconnect/restart and mutations invalidate affected views;
failed I/O clears the view. Actual uploads recheck primaries. External edits
within a load become visible on reloading, not automatically on every guard.
Actual short reads, failed close and corrupt new writeback still fail.
Settled and unregistered existing Banks now inspect group/order metadata in the
same stream that produces the fingerprint. Initial registration reuses it only
throughout that load, without another body pass or 100 per-box reads. Recovery
and pending phases retain existing paths. One native ticket job shares its
policy/time snapshot; a new job, reload/edit, purchase/recharge or location change
invalidates it. Staged bodies and commit/rollback decisions are retained, not a frozen policy. Canonical configuration
output/verification uses 512-byte batches without dropping source/temp checks. Download passes
its validated snapshot through the current native load to reserve
the unchanged Bank; it cannot authorize recovery against a replaced source.
Read-only reservations leave the previous backup untouched. The primary is
rechecked before a real upload; a changed body snapshots the old main before staging.
Unchanged downloads do not stage a duplicate tmp; actual save restaging still
writes, flushes and verifies it. Missing tmp is allowed only for unchanged
native Bank reservations, never damaged/unreadable tmp, other sources or changed
bodies. Whole-image copying reuses verified buffer bytes when available, otherwise
verifies the actual read before writing, then reads back the new destination once.
Publication distinguishes before/after/missing from one observation and checks
physical file existence/length before replacing the primary. Native save acknowledgement
is unchanged.

The new `0x002B1CF8` entry hook invokes response polling only in Offline
Mode. Download, Unlock and Original replay `push {r4-r6,lr}` and continue at
`0x002B1CFC`. Native source saving, result polling and token-cleanup bodies are
retained. The offline wrapper calls that same native body, delivers service replies
at native wait stages, and permits native Turtle saves to use the SD backend.
It does not clear Turtle or manufacture a completion phase.

#### Local journal format and recovery evidence

`/3ds/Bank/fakeServers/users/<userID>/transaction.bin` is one little-endian `0x370`-byte
[FsTransactionRecord](../../common/include/fakeServer/transaction.h). Both applications compile the same independent server core and filesystem implementation, with separate title-specific SDK addresses.

| Offset | Size | Content |
|---|---:|---|
| `0x00` | `0x20` | LTX2, version 5, size, sequence, internal transaction sequence, owner, operation, phase |
| `0x20` | `0x20` | Mode, participants, flags, source kind, game code, path size, reserved title ID |
| `0x40` | `0x220` | 272 UTF-16 units for the NDS path; zero for native sources |
| `0x260` | `0x20` | SDK-layout descriptor: ID, versions, size, zero alignment word, token |
| `0x280` | `0x24` | Native recovery display data: format, name format, 13-unit trainer name, trainer ID, game ID |
| `0x2A4` | `0x04` | Zero alignment padding |
| `0x2A8` | `0x18` | bankBefore: presence, reserved zero, 64-bit size, four-byte CRC32, explicit zero reserved word |
| `0x2C0` | `0x18` | bankAfter |
| `0x2D8` | `0x18` | sourceBefore: NDS files only |
| `0x2F0` | `0x18` | sourceAfter: first NDS save |
| `0x308` | `0x18` | sourceClean: NDS token-cleanup save |
| `0x320` | `0x18` | turtleBefore |
| `0x338` | `0x18` | turtleCurrent: published |
| `0x350` | `0x18` | turtleAfter: pending target |
| `0x368` | `0x04` | CRC32 of preceding `0x368` bytes |
| `0x36C` | `0x04` | Zero tail alignment word |

Scalar integers remain little-endian; CRC32 is four bytes in most-significant-byte-first order.
The journal remains version 5; binary configuration bindings are removed/ignored.
Later loads use current text without reevaluating staged bodies. Old SHA-256 journal authority remains
preserved/refused, not automatically erased. Bank bodies,
settings/game saves and native SDK descriptor layouts are unchanged. CRC32 identifies local
non-adversarial versions and accidental changes; it is not cryptographic authentication or a
collision-resistance guarantee. Random dataIds, tokens and challenge codes still use randomness,
never file CRC32. Configuration temporary seals use eight hexadecimal CRC32 digits and retain
valid user configuration values.

Internal sequence increments per update; the internal transaction number is the
first record-update sequence. Server dataId is stable, random and nonzero;
each reservation gets a new nonzero random 64-bit token, different from the
previous one, and updateVersion is currentVersion plus one. The SDK alignment
word at +0x14 is not filled by native calls and is excluded from matching.
Tokens map SDK +0x18 to game +0x08, not a whole-block copy.
File times and native 1/2 markers do not identify generations. Updates fully
write/flush/close/reread tmp, preserve the previous main as `transaction.bak`,
rename tmp to bin and verify. A valid main wins; only a missing main permits
the valid backup to resume. Invalid main or unknown authority stops. Temporary
journals are never promoted; missing published history uses the explicit local
adoption/pure-temporary reset policy. The backup holds one small published journal,
not historical Bank data, and is not a power-loss atomicity guarantee.

A zero-length `transaction.bak` is created before the first tmp write. It is
not authority. Missing published history is classified again after bootstrap
interruption. A checked Bank baseline/backup-adoption decision must be published
before Bank temporaries are retired; tmp never supplies the old decision.
Settings-baseline flag `0x10` keeps the version-5 structure size. Known
`turtleCurrent` survives Bank/source-only operations and Bank repair; reads,
writes and completion verify the complete settings file, without rebasing
unexpected present bytes or overwriting trusted `sav.bak`. A missing primary
may recover an exact current-baseline backup/temporary. Once settled, deleting
all three settings files records absence and permits native initialization,
without changing the Bank ID/version or last-source metadata. Pending loss of
all settings evidence still stops. Normal recorded format and
reinitialization remain supported. First untracked settings need native format
validation; successful writes establish the baseline, not historical integrity. If recovery cannot establish matching versions, preserve all files rather than clearing an unresolved native transaction.

PREPARING → SOURCE_PENDING → STAGED → COMMIT_DECIDED → PUBLISHED →
CLEANUP_PENDING → CLEANUP_READY → COMPLETED connects native saves to publication.
Bank/settings-only need no source save. After source-save publication, reconnect
may verify and finish PUBLISHED/CLEANUP_PENDING without repeating or inventing a
lost token-cleanup acknowledgement. Native residual tokens still use the stock
recovery callback. NDS recovery verifies the committed source and never publishes
unacknowledged cleanup temporaries. Online NDS SOURCE_ONLY retains current
Bank baseline and uploads no local Bank. Undecided work aborts only with provable
old Bank/full old NDS source; decisions proceed forward. ROLLBACK_DECIDED and
DETACHED_ROLLBACK_DECIDED are manual decisions retryable after interruption.
No extra title warning, X/Y controls or risk menu is added. Native matching and
state-1/2 branches choose rollback/commit; NDS image validation/publication stays
in the file backend. R switching remains; native modes use stock Turtle and
their native source saves have no local journal-based cross-mode restriction.
Official transaction calls do not enter fakeServer.

Bank/settings backups remain server-managed pre-write snapshots after completion;
Finish never overwrites them with the new main. Read-only native Bank reservations
leave the previous backup untouched; the actual changed upload backs up its old
main before staging. NDS sibling work files are normally retired after verified
publication and successful token cleanup. If publication succeeded but its cleanup
acknowledgement was lost, recovery verifies the committed source before retiring
unacknowledged cleanup temporaries and the old backup, without rewriting the source.
A damaged, truncated or missing settled Bank is repaired
only from a backup matching the journal's bankAfter fingerprint, with a new
RESTORE_BACKUP decision recorded before copying. Unknown or stale backups are
preserved, never promoted on readability alone.

Offline import policy gives a well-formed primary priority over old backups.
A primary differing from the settled image, or from both pending images, gets
a new ID/version 0 through recorded `ADOPT_BANK=6`; old work files and the Bank
backup are removed only after the adoption decision is published. When Bank
main and backup images are missing, valid settled authority may establish an empty
baseline and enter native creation, discarding a sole unconfirmed Bank tmp only
after publishing that decision. Active/corrupt authority never permits this reset.
A settings deletion alone is not a Bank import; same-Bank
repair/settings rebuilding retains native last-source metadata. Adoption does
not rewrite game saves or prove paired rollback. Native callbacks handle stale
source tokens. Unknown well-formed corruption is indistinguishable from import
under this policy; damaged authority and I/O faults still stop. Without published
history, a checked primary wins, a sole valid backup is adopted as a new baseline,
and a sole Bank tmp is abandoned before native creation. Other pure temporaries
are classified by their associated published evidence; no blank NDS save or
replacement policy for an active bound transaction is invented.
See [file combinations](../../common/docs/fakeServer/architecture.zh-cn.md#删除损坏和手动换入的处理边界).

Native source saving/polling remain. Bank state 7 reports first-save success
only on substate `4 → 5`, and native restoration-save success only on `8 → 11`;
waiting or failed polls cannot create acknowledgement. The state-2 commit request
still selects Bank publication, not final completion. Substate 13 records cleanup
start before native token clearing/saving, and only actual `14 → 15` success
acknowledges cleanup and finishes. Failure preserves the original descriptor and
native error exit. State 17 also observes native cleanup substates `5/6/7`, allowing
another application's transaction to resume using its matching native descriptor.
No native-save fingerprint or full snapshot is added. Turtle writes record intent, complete snapshots
and fingerprints; native serialization/checksum remains. Failed cleanup never
deletes Bank as if uncommitted.

[Mover's NDS transaction](../../mover/docs/code-analysis.md#offline-local-transaction-journal)
supports full source fingerprints/snapshots and restart cleanup. Native cartridge,
VC and Bank 3DS sources lack full snapshots and have no extra native-block hash.
The service matches native request IDs, versions, size and nonzero token. It may
acknowledge the same decision for its last completed reservation, never successful
commit/rollback of unknown history. Native callbacks decide whether a definitive
expired/not-found reply permits residual-token cleanup.
Bank state 17 keeps native reading, state-1/2 decisions and cleanup/save
substates 5/6/7. The native cancel field is no longer borrowed as a local delay
latch. Local IDs/tokens are not official credentials.
When Bank and Mover alternate, an obsolete descriptor cannot acknowledge a newer
reservation. A valid formal journal is authoritative; only its absence allows
the previous `.bak`, never a newer-looking `.tmp`. Settled/absent old-token cleanup
is a guarded no-op on Bank, not a commit or rollback of the current reservation.
Native save events do not replace Gen 5 file SavePoll, validation or publication.
Journals are not uploaded. One writer and storage
persistence assumptions remain; file rereads do not establish real SD power-loss safety.

### Boundaries

- Local Bankdata validation checks fixed size, format version, box count,
  groups and in-group order, plus complete fingerprints for journal generations;
  first import without a journal cannot detect every middle-body corruption and is not
  equivalent to server-side content validation.
- Download Mode still requires valid account, transaction, and complete-file
  responses from the official service.
- Unlock Mode can display only a candidate actually present in the server
  response; it cannot guarantee that the service accepts the later rollback.
- Offline Mode generates local descriptors to reuse native client branches,
  not official credentials. The journal manages file publication/backups;
  native sources lack full snapshots and paired rollback/atomicity guarantees.
- This document states branches implemented in the current binary and patch;
  unpublished server-side checks are not asserted as facts.

## Local service integration

Only Offline routes Bank, local settings and transaction requests to the embedded
service. Other modes preserve official dispatch, apart from Download's explicit
local export and the capability-gated emulator ticket response. The service does
not replace official authentication or invent a valid HOME transfer.

Original download state `0x002AF460` retains its SDK job, observer and Bank-file
consumer. The service owns a complete response; the client copies it into the
native buffer in bounded chunks, releases it, then completes the original job.
Cancellation unbinds observers and discards late results without joining the worker.

Initial settings format enters native substate 21 and polls `FORMAT_TURTLE` at
`0x002AC68C`. Pending remains pending; success and failure follow the unchanged
table in `0x002ABE08`. Initial creation parameters do not depend on a Bank already
being present. Native begin/poll `0x002B4AB4/0x002B4A20` retain save preparation,
completion and cleanup; local settings publication runs in the worker.

Offline connection monitoring does not query an official session that was never
created. It still respects local cancellation and error results. Online modes
retain native connection monitoring.

### Gifts and mileage

Native gift callback `0x002AB17C` consumes service-supplied frames. Gen6 writes
the source game's Link area and updates its CRC/Bank IDs; Gen7 inserts cards and
uses native capacity and repeat handling. Saving follows ordinary Bank/game
transactions, without a separate receipt ledger.

Gift query `0x002A9824` uses the local-server connection label in Offline.
Mileage dialogue at `0x002AA990/0x002AAEC8` retains portrait table 37, line 14;
it is not replaced with a generic connection-message index.
Offline time comes from the local service response, based on the console clock.
Online time remains the official service's response.

### Per-user console locale

The selected user's locale and currency live in `user.conf`. Explicit values
remain fixed; autoRegion refreshes only after the title selection is confirmed,
and autoCurrency derives the currency from that profile's region. Browsing users
does not update their locale or balances. Bank does not rewrite the console-region
fields of Pokemon already stored in the Bank.

See [service architecture](../../common/docs/fakeServer/architecture.zh-cn.md),
[personal configuration](../../configTemplate/templateForCopy/Bank/fakeServers/users/readme_user.md)
and [gift configuration](../../configTemplate/templateForCopy/Bank/fakeServers/shared/gifts/readme_gift.md).
