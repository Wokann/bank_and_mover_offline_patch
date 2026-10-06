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
| `.data` | `0x002EC000–0x0032A000` | Read/write |
| `.bss` | Starts at `0x0032A000`, length `0x3A4A8` | Read/write, zero-initialized |
| Thread stack | Length `0x40000` | ExHeader setting |

The ExHeader declares the `.text` content end at `0x0028D1AC`, while the last
mapped page ends at `0x0028E000`. Ghidra's last referenced instruction/data
location is `0x0028D188`, and its last function ends at `0x0028D18F`.
Luma installs its LayeredFS payload at the declared content end when the page
padding is sufficient, after applying IPS. This project leaves
`0x0028D1AC–0x0028D2DC` (`0x130` bytes) untouched for it and aligns its own tail
start to `0x0028D2E0`. The inspected Luma payload is `0x114` bytes; this reservation
must be rechecked if that payload grows. See the official
[installation logic](https://raw.githubusercontent.com/LumaTeam/Luma3DS/master/sysmodules/loader/source/patcher.c)
and [payload assembly](https://raw.githubusercontent.com/LumaTeam/Luma3DS/master/sysmodules/loader/source/romfsredir.s).

The first project tail area is `0x0028D2E0–0x0028DD00`, and the per-slot
local-validation payload occupies `0x0028DDE0–0x0028DF54`.
The `0xE0` bytes at `0x0028DD00–0x0028DDE0` form a dedicated area reserved for
the Transporter Redirect Patch, and `0x0028DFC0–0x0028E000` holds this project's
version identifier. Project payloads may use both
`0x0028D2E0–0x0028DD00` and `0x0028DDE0–0x0028DFC0`, totaling `0xC00` bytes before
subtracting their contents. Assembly area limits and static verification protect
the LayeredFS and external-patch reservations.

The four ticket hooks replace existing four-byte `BL` instructions in place.
The remaining native ticket-state and job bytes are unchanged.

The zero-filled tails in `.rodata` (`0x002EBA58–0x002EC000`, `0x5A8` bytes)
and `.data` (`0x003293FC–0x0032A000`, `0xC04` bytes) are non-executable and may
still be referenced. Expanding only the ExHeader `.text` size would overlap
the existing `.rodata` mapping. Expansion beyond `0x0028E000` therefore needs
a relocated full code image and matching ExHeader segment addresses, not only
a Luma `code.ips`.

## Ticket state and local results

The native ticket state is `0x00249600`; its local substate is at state `+0x10`.
Both modes retain this function, callback `0x0024991C` and cleanup `0x00249CD8`.
Only these interfaces are routed:

| Call site | Native function | Offline or test policy `1` | Original Mode, policy `0` |
|---|---|---|---|
| `0x00249754` | `0x0023D3C0` initialization | Populate local job fields; return `0` on failure | Original call and arguments |
| `0x00249774` | `0x0023E230` polling | Report completion and successful result | Original call and arguments |
| `0x00249818` | `0x0023BE4C` free-campaign request | No campaign; set substate `3` | Original request and asynchronous callback |
| `0x00249CEC` | `0x0023E63C` unbind | No client was bound; proceed with destruction | Original unbind |

The wrappers use caller-temporary `r12` for mode/policy checks. Tail calls
preserve the original `BL` return address and stack. Only local initialization
loads shared data from state `+0x28` into `r1`; only local campaign completion
passes the state in `r0`. Native routes preserve their original arguments.

| Substate / node | Native flow | Local-result flow |
|---|---|---|
| `0` | Update UI and wait for the next stage | Unchanged |
| `1` | Wait for UI, allocate/construct the `0x158`-byte job, initialize its client | Keep waiting, allocation and construction; replace client initialization only |
| `2` | Poll; on success compute entitlement, copy account/purchase values, set validity and register the campaign callback | Same result consumer and registration; replace polling and campaign request only |
| `6` | Wait for the campaign callback, which advances to `3` | Local campaign completion directly advances to `3`, without fabricating a callback packet |
| `3` | Check expiry/campaign; available advances to `7`, unavailable to `4` | Same decision; valid local dates and no campaign advance to `7` |
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
purchase count remains `-1`; the free-campaign flag at `+0x335` is `0`.
This does not calculate Poke Miles or change Bankdata transactions or Pokemon
validation.

`ONLINE_TICKET_CHECK_BYPASS` defaults to `0` and selects local test results only
in Original Mode; Offline Mode always uses local interfaces. Setting `1` does
not replace NNID authentication, other server requests or remote permission.
It is for emulator testing, not evidence of server-side transfer approval.

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
2. For Gen 5, Original Mode submits all 30 raw BOX1 records, including empty
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
check functions always succeed and therefore do not set those bits. In Original
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

Original Mode submits all 30 candidates and, after validating the reply,
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

### Original and offline execution order

| Step | Gen 5 Original Mode | Gen 5 Offline Mode |
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
  an Original Mode attempt in the same process from surviving a title-screen
  mode switch.
- Original Mode returns to the original remote-request code. The 14 Gen 5
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
