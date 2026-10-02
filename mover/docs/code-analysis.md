# Poke Mover Code Analysis

This document records static-analysis conclusions about the supported stock
binary only; it does not describe patch design or build instructions. See
[`../src/README.md`](../src/README.md) for the maintained implementation,
transaction rules, and independent build procedure.

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

The last non-zero image byte in `.text` is before `0x0028D1AC`. The ARM-aligned
usable tail is `0x0028D1B0–0x0028E000`, or `0xE50` bytes. Ghidra's last
referenced instruction/data location is `0x0028D188`, and its last function
ends at `0x0028D18F`. The current offline payload ends at `0x0028DBF1`.
The `0xE0` bytes at `0x0028DD00–0x0028DDE0` form a dedicated area reserved for
the Transporter Redirect Patch, and `0x0028DFC0–0x0028E000` holds this project's
version identifier. Future project payloads may use both
`0x0028D1B0–0x0028DD00` and `0x0028DDE0–0x0028DFC0`, with `0x2EF` bytes currently
free in total.

The zero-filled tails in `.rodata` (`0x002EBA58–0x002EC000`, `0x5A8` bytes)
and `.data` (`0x003293FC–0x0032A000`, `0xC04` bytes) are non-executable and may
still be referenced. Expanding only the ExHeader `.text` size would overlap
the existing `.rodata` mapping. Expansion beyond `0x0028E000` therefore needs
a relocated full code image and matching ExHeader segment addresses, not only
a Luma `code.ips`.

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
