# Pokemon Bank and Poke Mover Offline Patch

[简体中文](README.zh-cn.md) · [Download latest release](../../releases/latest) · [All releases](../../releases)

This project provides Luma3DS patches for Pokemon Bank and Poke Mover on Nintendo 3DS. Bank data can be stored on the SD card and used without a server connection. Both applications share:

```text
sd:/3ds/Bank/bankdata.bin
```

The patches never upload offline changes to the official server. An upload patch has been permanently discontinued because the server-side identity, revision, timestamp, transaction, and conflict rules are not sufficiently established for safe use.

## Features

| Application | Title-screen modes | Purpose |
|---|---|---|
| Pokemon Bank | Offline / Download / Unlock / Original | Use a local Bank offline; download the official Bank; display an unlock candidate; or use the complete official Bank interface |
| Poke Mover | Offline / Online | Transfer Pokemon into the same local Bank's Transport Box, or use the official server path |

- Press the physical **R** button on the title screen to switch modes. Pressing **A**, **START**, or the lower screen latches the displayed mode for that session.
- Bank Offline Mode can invoke the stock first-use initializer to create a new local Bank when no usable file exists.
- Bank validates the primary file, attempts recovery from `bankdata.bak`, and protects saves with `bankdata.tmp`, complete-write checks, and a previous-generation backup.
- Bank Download Mode retrieves the complete server Bank without selecting a game, with a direct-download prompt when no usable game save exists. New accounts first follow official Bank creation; the download returns to the title without entering boxes or uploading offline changes.
- Bank Unlock Mode preserves the stock online transaction-recovery flow and exposes one server-returned unlock candidate on the official challenge-code screen.
- Bank Original Mode restores the official menus, messages, HOME/eShop operations, ticket checks, and normal online Bank flow. After confirming the mode, only Offline uses `sav.bin`; Download, Unlock, and Original use the stock save.
- Poke Mover Offline Mode retains stock game reading, filtering, conversion, Transport Box checks, and source-game saving while replacing server Bank I/O with local transactions.
- Both Poke Mover modes can scan Gen 5 ROM/save pairs on the SD card in addition to a physical cartridge and redirect subsequent I/O for the selected digital save back to the SD card.
- Added messages cover all ten language archives shipped with both applications.

## Download and installation

Players do not need an original `.code`, a RomFS dump, or a local build environment.

1. Download and extract the latest archive from [GitHub Releases](../../releases/latest).
2. Copy the required complete Title ID directory to `SD:/luma/titles/`:

   | Application | Title ID | Installed path |
   |---|---|---|
   | Pokemon Bank | `00040000000C9B00` | `SD:/luma/titles/00040000000C9B00/` |
   | Poke Mover | `00040000000C9C00` | `SD:/luma/titles/00040000000C9C00/` |

   Both Bank and Mover contain `code.bps`, `exheader.bin` and `romfs/`.
   Install all three together and remove stale `code.ips` or `code.bin` from
   the corresponding mod directory.
3. Hold `SELECT` while booting to open the Luma3DS configuration, enable `Enable game patching`, then save and reboot.
4. Back up the SD card and any existing `SD:/3ds/Bank/` directory before first use.

The project targets the base applications, not update titles:

```text
Pokemon Bank: 00040000000C9B00
Poke Mover:   00040000000C9C00
```

## Usage

### Pokemon Bank

On a fresh launch, the title screen defaults to **Offline Mode**. Press **R** to cycle through **Download Mode**, **Unlock Mode**, **Original Mode**, and back to Offline Mode. Returning to the title retains the mode just used; restarting the application resets it to Offline. This choice is not written to a save.

If the official server already contains your Bank, first enter Download Mode, complete the stock account checks, and choose “Download Bank data locally”; no game selection is needed. Without a usable game save, accept the direct-download prompt. A missing server Bank is created through the official first-use flow before downloading. After the completion message returns to the title screen, switch back to Offline Mode and use the normal Bank interface to manage boxes and save locally.

Download Mode overwrites the current local Bank, so back it up first. If no server copy needs to be preserved, Offline Mode may be used directly; when neither a valid primary nor backup exists, the patch invokes the stock first-use initializer to create a local Bank.

Unlock Mode is intended only for the stock save-mismatch lock. Enter its first menu item, select the relevant game, and hold **L + A + START** while confirming the game to open the official forced-unlock screen. If the server supplies an accepted candidate, the patch shows its first eight-digit form on a third line. The subsequent rollback, validation, and server handling remain stock behavior; this mode does not capture `bankdata.bin` or use Download Mode's early-exit route.

Original Mode uses the complete official Bank menus and online flow, including HOME, support-code, and Mover/eShop entries. It does not load or capture the local `bankdata.bin`.

Startup/title handling still previews the redirected record. Once a mode is confirmed, its record is reloaded: Offline uses `SD:/3ds/Bank/sav.bin`; the other modes use the stock `data:/turtle`. Each record supplies its own language and transaction state. Missing or invalid stock records follow native language selection and initialization, without importing SD data or mirroring native saves back to SD. The first three modes never enter HOME transfer operations without a game: Download offers direct downloading, Offline/Unlock warn and return, and Original retains the official HOME question.

> **Privacy:** `bankdata.bin` downloaded in Download Mode may contain private account-related identifiers, player and Trainer information, and timestamps. The `sav.bin` copied from the original internal save for local offline use may also contain personal or account-related information. Do not upload either file publicly or share it casually.

### Poke Mover

On a fresh launch, the title screen defaults to **Offline Mode**. Press **R** to select **Online Mode**. Returning to the title retains the mode just used; restarting the application resets it to Offline. This choice is not written to a save.

- Offline Mode requires a `bankdata.bin` previously downloaded or initialized by Bank. Mover does not download or create this file.
- A successful offline transfer writes Pokemon into the Transport Box of the same local Bank; retrieve them later with Bank in Offline Mode.
- Online Mode follows the official network path and does not read, write, rename, or remove local Bank files under `SD:/3ds/Bank/`.
- If the offline Bank file is invalid or its Transport Box is occupied, Mover preserves the file and follows the corresponding error path.

Gen 5 source redirection is shared by both modes: Online Mode and Offline
Mode both scan the following directory and read or write the paired `.sav` when
a digital source is selected. A ROM and save must have the same base filename:

```text
sd:/roms/nds/
├── any-name.nds
└── saves/
    └── any-name.sav
```

The scanner accepts only Black, White, Black 2, and White 2. A `.sav` must be
readable, writable, and pass Mover's native save validation. At most one source
is shown for each game: a valid physical cartridge takes priority over an SD
copy; digital copies use `J > O > F > I > D > S > K`, retaining the first valid
copy at the best language rank encountered. Invalid candidates are skipped and
do not prevent later files from being tested.

### bankdata viewer

The included [bankdata viewer](ViewerForBankdata/README.md) can be used to inspect a local `bankdata.bin`. Run it with Python 3, then select the file to open:

```powershell
cd ViewerForBankdata
python .\gui\bank_viewer.py
```

## Data safety and limitations

- Back up the SD card, source-game saves, and `SD:/3ds/Bank/` before real-console use.
- Do not mix Bank files belonging to different accounts.
- Download Mode is server-to-local only; Offline Mode is local only; no local-to-server upload feature exists.
- Unlock Mode uses the official online recovery path and may change the official transaction state. It does not upload the local offline `bankdata.bin`.
- Poke Mover's server Bank in Online Mode and the local offline Bank remain
  independent. Gen 5 source discovery and selected digital-save I/O are shared
  by both modes.
- Mover now includes its own Gen 5 SD-source redirect. Do not merge or install it together with DreamRadarCartRedirect's external Transporter Redirect Patch.

## Unofficial-project disclaimer

This is an independently developed, unofficial community patch. It is not
affiliated with, sponsored by, endorsed by, or authorized by Nintendo,
The Pokemon Company, Creatures Inc., or GAME FREAK inc. Nintendo 3DS, Pokemon,
Pokemon Bank, Poke Mover, and all related games, software, characters, names,
logos, artwork, text, music, and other original content remain the trademarks,
copyrights, and other intellectual property of their respective owners. This
project is provided solely for technical research, software preservation, and
interoperability and claims no ownership of the underlying original content.

## Building locally

Bank and Mover can be built independently. Each subproject documents its inputs, build commands, implementation, state machine, file transactions, verification, and references:

- [Pokemon Bank developer documentation](bank/README.md)
- [Poke Mover developer documentation](mover/README.md)

### Basic requirements

- GNU Make, a POSIX-compatible shell, and Python 3.
- [devkitARM](https://devkitpro.org/wiki/Getting_Started), with the `DEVKITARM` environment variable set.
- The repository includes Windows builds of [armips](https://github.com/Kingcom/armips) and [Floating IPS](https://github.com/Sir-Walrus/Flips). Other platforms must obtain or build them from the official projects and set `ARMIPS` and `IPS_TOOL`.
- [GodMode9](https://github.com/d0k3/GodMode9), used to dump a decrypted and decompressed `.code` plus the complete RomFS from the user's own title.

Do not commit or redistribute extracted code and resources. The project supports these base images:

| Application | Project path | Size | SHA-1 |
|---|---|---:|---|
| Pokemon Bank | `bank/rom/exefs/00040000000C9B00.dec.code` | 2,801,664 bytes | `5AB630856835DCF2DBDF9A62244DD19E46AE1C7C` |
| Poke Mover | `mover/rom/exefs/00040000000C9C00.dec.code` | 2,269,184 bytes | `583859C1E874D11650EFBDDE51F470ECF96900C4` |

Place each complete RomFS at `bank/rom/romfs/` or `mover/rom/romfs/`. Both
subprojects also require their own base title's complete `0x800`-byte ExHeader:
copy GodMode9's mounted root `extheader.bin` to `bank/rom/exheader.bin` or
`mover/rom/exheader.bin`. These headers are not interchangeable. See each
subproject's developer document for inputs and the unified Luma/Azahar loader.
With the inputs prepared, run from the repository root:

```sh
make -C bank clean
make -C bank

make -C mover clean
make -C mover
```

Override tool paths on other platforms when required:

```sh
make -C bank ARMIPS=/path/to/armips IPS_TOOL=/path/to/flips
make -C mover ARMIPS=/path/to/armips IPS_TOOL=/path/to/flips
```

Outputs are written to:

```text
release/00040000000C9B00/
├── code.bps
├── exheader.bin
└── romfs/

release/00040000000C9C00/
├── code.bps
├── exheader.bin
└── romfs/
```

## References and acknowledgements

- [Luma3DS](https://github.com/LumaTeam/Luma3DS) provides the Title ID-based IPS/BPS and LayeredFS runtime.
- [GodMode9](https://github.com/d0k3/GodMode9) is used to dump executable code and RomFS from the user's own titles.
- [devkitPro / libctru](https://github.com/devkitPro/libctru) provides the devkitARM ecosystem and public 3DS FSUSER/FSFILE references.
- [armips](https://github.com/Kingcom/armips) provides ARM assembly, object import, and code injection.
- [Magikoopa's expansion implementation](https://github.com/RicBent/Magikoopa/blob/master/MagikoopaUI/patchmaker.cpp) provides a public reference for placing new code after the original BSS and using an ExHeader plus SVC permission change.
- [oot3d_practice_menu](https://github.com/gamestabled/oot3d_practice_menu) provides a public reference for separate hardware/Citra ExHeader layouts and loader behavior.
- [Floating IPS](https://github.com/Sir-Walrus/Flips) creates the IPS/BPS files.
- [pkNX TextFile](https://github.com/kwsch/pkNX/blob/master/pkNX.Structures/Text/TextFile.cs) was consulted for the message encoding and line-table format.
- [zaksabeast/Transporter-Offline-Patch](https://github.com/zaksabeast/Transporter-Offline-Patch) provides a public precedent for replacing Poke Mover network states.
- [Transporter-PKSM-Bank-Patch state-machine notes](https://github.com/zaksabeast/Transporter-PKSM-Bank-Patch/blob/master/TRANSPORTER_DOCS.md) were used to cross-check the high-level Poke Mover state order.

Program addresses, state transitions, object layouts, file offsets, and patch sites are based on analysis and testing of the supported binaries. External projects are used only to cross-check public interfaces, formats, and high-level behavior.
