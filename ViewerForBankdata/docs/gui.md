# bankdata GUI viewer

[简体中文](gui.zh-cn.md)

This read-only Tkinter application displays boxes, groups, localized species
names, stored nicknames, and file metadata. It uses only the Python standard
library and the fixed resources under `resources/`; RomFS is not accessed at
runtime.

```powershell
python .\gui\bank_viewer.py
python .\gui\bank_viewer.py "D:\path\to\bankdata.bin"
python .\gui\bank_viewer.py "D:\path\to\bankdata.bin" --language zh-Hans
```

With no input path, use **Open bankdata** to select a file. No bankdata path is
preselected. The interface supports both Japanese writing modes, English,
German, Italian, French, Spanish, Korean, Simplified Chinese, and Traditional
Chinese. Chinese species names and stored nicknames use separate display-name
and private-glyph conversion tables.

For a non-GUI validation run:

```powershell
python .\gui\bank_viewer.py --self-test "D:\path\to\bankdata.bin"
```

A `bankdata.bin` produced by Download Mode may contain private account-related
identifiers, player and Trainer information, and exact timestamps. Do not
upload it publicly or share it casually.
