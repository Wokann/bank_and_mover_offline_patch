# Pokemon Bank and Poke Mover Offline Patch

[简体中文](README.zh-cn.md) · [Download](../../releases/latest) · [Changelog](CHANGELOG.md)

Use Pokemon Bank and Poke Mover with Bank data stored on your Nintendo 3DS SD card. The patches keep the familiar box-management and transfer interfaces, with separate offline and official-server modes.

## Features

- Manage a local Bank and receive Mover transfers without connecting to the official server.
- Download an existing official Bank for offline use, or initialize a new local Bank.
- Keep separate local users, each with their own Bank, settings, balance and console locale.
- Configure gifts, name filtering and Pokemon screening with editable text files.
- Use Gen5 ROM/save pairs on the SD card as Mover sources, alongside supported cartridges and VC games.
- Switch modes with **R**, change language with **L**, and select local users on the title screen. Added text covers all ten original language archives.

Local and official Banks are independent. This project does not upload offline changes to the official server.

## Installation

Back up your Bank data and game saves first. Bank and Mover must be updated together.

### 1. Install the patches

Download the [latest release](../../releases/latest). On a 3DS with Luma3DS, copy each complete Title ID folder to `SD:/luma/titles/`:

| Application | Patch directory |
| --- | --- |
| Pokemon Bank | `SD:/luma/titles/00040000000C9B00/` |
| Poke Mover | `SD:/luma/titles/00040000000C9C00/` |

Each folder must contain `code.bps`, `exheader.bin` and `romfs/`. Remove old `code.ips` or `code.bin` files from those patch directories. Hold `SELECT` during boot, enable Luma's `Enable game patching`, then save and reboot.

These are base-application Title IDs, not update-title IDs. Players using release patches do not need executable dumps or a build environment.

### 2. Copy the configuration template

For a new installation, copy the **Bank** folder from [configTemplate/templateForCopy](configTemplate/templateForCopy) to `SD:/3ds/`, producing:

```text
SD:/3ds/Bank/
├── client/
│   └── client.conf
└── fakeServers/
    └── shared/
        ├── server.conf
        ├── gifts/
        └── mover/
```

Copy the template folder in full, including the files inside `gifts/` and `mover/`. Basic client/server/user configuration can be generated automatically, but gift assets and Mover name/Pokemon rules must be supplied on the SD card to use those features. Runtime Bank and user files are created when needed; do not create empty binary placeholders.

For an existing installation, keep personal files and settings instead of replacing the whole Bank directory. Follow the [configuration and migration guide](configTemplate/templateForCopy/Bank/readme_fakeServer.md). The separate `configTemplate/examples/` tree contains demonstration users; it is not the installation template.

**Azahar:** install patch folders under the emulator user directory's `load/mods/`, not its virtual SD card. Put the same Bank configuration folder under `sdmc/3ds/Bank/` in that user directory.

## Getting started

1. Start **Bank** first. At the title screen, use **R** to select a mode and choose the target local user where shown.
2. To keep an existing official Bank, use **Download Mode** first. It overwrites the selected local Bank, so back it up beforehand. Otherwise enter **Offline Mode** to initialize a new local Bank.
3. Use **Mover Offline Mode** to send Pokemon to that local Bank's Transport Box, then retrieve them with **Bank Offline Mode**. Mover requires a Bank already initialized or migrated by Bank.

| Application | Mode | Purpose |
| --- | --- | --- |
| Bank | Offline | Manage and save the selected local Bank. |
| Bank | Download | Copy the official Bank to the selected local user, then return to title. |
| Bank | Unlock | Use the official transaction-recovery flow; forced recovery can lose Pokemon from an interrupted transfer. |
| Bank | Original | Use the official Bank interface and online service. |
| Mover | Offline | Transfer to the selected local Bank. |
| Mover | Online | Transfer through the official service. |

Modes default to Offline at application startup. **A**, **START** or the touch screen confirms the displayed selection. In Bank Offline/Download and Mover Offline, **↑/↓** or the Circle Pad selects users; **auto** chooses from the current account environment. Language selection is on title **L** and is shared between the two applications.

See [Bank usage](bank/README.md) and [Mover usage](mover/README.md) for recovery, language and source-save details. For Gen5 digital sources, pair `sd:/roms/nds/name.nds` with `sd:/roms/nds/saves/name.sav`.

## Configuration and documentation

- [SD layout and file protection](configTemplate/templateForCopy/Bank/readme_fakeServer.md)
- [User selection and migration](configTemplate/templateForCopy/Bank/client/readme_client.md)
- [Server policy and diagnostic logging](configTemplate/templateForCopy/Bank/fakeServers/shared/readme_server.md)
- [Personal locale, currency and passes](configTemplate/templateForCopy/Bank/fakeServers/users/readme_user.md)
- [Gifts](configTemplate/templateForCopy/Bank/fakeServers/shared/gifts/readme_gift.md)
- [Pokemon screening and rule sources](configTemplate/templateForCopy/Bank/fakeServers/shared/mover/readme_checkPokemon.md)
- [Name filtering, exceptions and sources](configTemplate/templateForCopy/Bank/fakeServers/shared/mover/readme_checkName.md)
- [Bank data viewer](ViewerForBankdata/README.md)

## Data safety and limitations

- Keep independent backups of the SD card and source-game saves. Do not mix files from different users or delete journals to bypass an interrupted-save error.
- Stop both applications before manually editing or replacing Bank files. If recovery cannot establish matching versions, preserve all related files.
- Download Mode replaces local data. Unlock Mode changes official transaction state and can discard an interrupted transfer; neither mode uploads a local Bank.
- Local checks and configurable rules are not a complete reconstruction of the official server's private rules. CRC32 detects accidental changes; it is not cryptographic authentication.
- Bank/settings files contain private identifiers and Trainer information. Do not publish them.
- Gen5 source support is built in. Do not combine this patch with an external Transporter Redirect Patch.

## Building from source

Build with GNU Make, Python 3, devkitARM, armips and Floating IPS. Original decrypted/decompressed code, ExHeader and RomFS inputs must come from your own titles and are not distributed here.

Run `make` from the repository root to build and verify both titles. `make package` also uses
7-Zip ZIP/LZMA at maximum compression to create
`release/Offline Patch v<version>-<revision>-<YYYYMMDDHHmm>.zip`; the timestamp uses Beijing
time (UTC+8). The archive contains only the two title patch directories and the complete
empty `Bank` configuration template. Existing ZIPs are not included or overwritten.
7-Zip is auto-detected, or can be selected with `SEVENZIP="/path/to/7z"`.
The per-title `make -C bank` and `make -C mover` commands remain available.

The patch label appears in white at the upper screen's top left; the official version stays in its original position. The root `VERSION` file defines the base patch version. Ordinary `make` always produces a development label such as `Offline Patch v0.9.1-abcdef`, using the current Git short revision; a tag does not select release mode. Uncommitted changes on the same commit retain the same Git suffix.

For an official patch release, explicitly update and commit `VERSION`, then build both titles:

```sh
make -C bank PATCH_RELEASE=1
make -C mover PATCH_RELEASE=1
```

Only explicit `PATCH_RELEASE=1` omits the Git suffix, displaying `Offline Patch v<version>`. `PATCH_VERSION` overrides the base version for one build. Source copies without Git metadata require an explicit `PATCH_REVISION=<short revision>` for development builds. Generated identifiers are not written back to source and do not change SD configuration or saves.

- [Bank build instructions and implementation](bank/README.md#independent-build)
- [Mover build instructions and implementation](mover/README.md#independent-build)
- [Shared service and storage](common/README.md)

## Acknowledgements

Built with [Luma3DS](https://github.com/LumaTeam/Luma3DS), [devkitPro/libctru](https://github.com/devkitPro/libctru), [armips](https://github.com/Kingcom/armips) and [Floating IPS](https://github.com/Sir-Walrus/Flips). [GodMode9](https://github.com/d0k3/GodMode9) supplies the title-dumping workflow. Configuration guides identify the sources and licenses for filtering data and gift assets.

## AI usage statement

This project uses AI tools, including OpenAI Codex, to help analyze decompiled output, design patches, write code, and review changes. If you have concerns about using patches developed with AI-assisted reverse-engineering analysis or coding, please take this into account before deciding whether to use them.

## Unofficial-project disclaimer

This is an independently developed, unofficial community patch. It is not affiliated with, sponsored by, endorsed by, or authorized by Nintendo, The Pokemon Company, Creatures Inc., or GAME FREAK inc. Original software, characters, names and assets remain the intellectual property of their respective owners. The project is intended for software preservation, technical research and interoperability.
