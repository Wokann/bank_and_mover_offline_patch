# Pokemon Bank Code Analysis

This document records static-analysis conclusions about the supported stock
binary only. See [`state-machine.md`](state-machine.md) for exact stock
transitions and a comparison with the current patch. See
[`../README.md`](../README.md) for the maintained implementation,
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
| `.bss` | Starts at `0x003AC000`, length `0x4EE38` | Read/write, zero-initialized |
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

Increasing only the ExHeader `.text` size cannot extend executable code past
`0x00314000`: `.rodata` begins at that same page boundary. A real expansion
would require moving `.rodata` and `.data`, updating their ExHeader virtual
addresses, and relocating every affected absolute reference. A Luma
`code.ips` cannot express that complete image-layout change by itself. The
current patch therefore uses existing executable padding and intentionally
unreachable function bodies instead of claiming `.rodata` or `.bss` as code.

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
