# Changelog

[简体中文](CHANGELOG.zh-cn.md)

## v0.9.1

### Pokémon Bank

- Added Unlock Mode, which displays one server-returned unlock candidate on the official challenge-code screen while retaining native input validation and transaction recovery.
- Added Original Mode, restoring official menus, messages, tickets, HOME, eShop, and online Bank operations.
- Offline Mode stores the local record in `sd:/3ds/Bank/sav.bin`. Download, Unlock, and Original modes reload the native save after mode confirmation; neither backend mirrors saves to the other.
- Download Mode no longer requires game selection. It offers direct downloading when no usable game exists and follows native initialization when the server Bank has not been created. Successful downloads exit without saving; locked server transactions do not enter downloading.

### Poké Mover

- Added Gen5 digital compatibility in both Online and Offline modes. Dynamically scan `sd:/roms/nds/*.nds` and read/write the paired `sd:/roms/nds/saves/<same-name>.sav`.
- Prefer a physical cartridge with a usable save for each game version. Rank digital sources by `J > O > F > I > D > S > K`, require writable saves that pass native validation, and retain the first valid copy for each language. The four Gen 5 sources and VC share the original 40-entry limit.
- Added UTF-16 file paths and per-frame scanning. Gen 5 titles follow the cartridge/ROM language rather than the UI language.
- Use the language from an existing Bank `sav.bin`, falling back to native system-language handling when absent.

### Shared features

- Retain the selected mode when returning to the title screen. A fresh launch still defaults to Offline; mode selection is not saved.

## v0.9.0

### Pokémon Bank

- Provided Offline and Download modes, selected with R at the title screen and latched for the session on entry.
- Read, edit, and save `sd:/3ds/Bank/bankdata.bin` offline. Native first-use initialization creates a local Bank when neither the main file nor its backup is usable.
- Download Mode retains official account handling and game selection, saves the complete server Bank to SD, and returns to the title without entering boxes or uploading local changes.

### Poké Mover

- Provided Offline and Original modes. Offline Mode retains native source reading, filtering, conversion, and game-save writing while placing Pokémon in the shared local Bank's Transport Box.
- Bank must download or initialize the local Bank first. Mover neither downloads nor creates it and does not overwrite an occupied Transport Box.
- Original Mode uses official networking and the server Bank independently of the local offline Bank.

### Tools

- Included the bankdata viewer.
