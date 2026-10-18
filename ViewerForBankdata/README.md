# Viewer for bankdata

This directory contains a read-only, standard-library Python viewer and small
diagnostic tools for local bankdata files. The GUI includes fixed text
resources for all ten supported language selections and does not require an
extracted RomFS at runtime.
Bank inputs and resources must be independent regular files; symbolic links,
junctions and hard links are not supported.

```powershell
cd ViewerForBankdata
python .\gui\bank_viewer.py
```

The viewer opens without a preset input. Select the bankdata file through the
file dialog. See the [GUI guide](docs/gui.md) for options and the
[command-line guide](docs/cli.md) for file inspection tools.
