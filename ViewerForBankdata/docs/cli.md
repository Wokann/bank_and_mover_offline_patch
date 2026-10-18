# bankdata diagnostic tools

[简体中文](cli.zh-cn.md)

The Python files in this directory provide read-only format diagnostics used
by the GUI or by command-line investigation. Input files must be selected
explicitly; the tools do not search for bankdata or RomFS.

All three commands accept the serialized `0xBB518`-byte body or an eight-byte
prefixed `0xBB520`-byte memory object. Length and format/box-count checks run
before parsing; they are not full Pokémon-legality validation. Species scanning
uses halfword alignment so the `0x1B56` tray stride does not hide alternate trays.

Bank inputs, `--names` tables, fixed resources and reports must be independent
regular files. Symbolic links, junctions, hard links and linked ancestor
directories are rejected.
`list_bank_file.py --output` exports a UTF-8 text report, but must not refer to
the Bank input or the `--names` table, including path aliases. A new temporary file in the
same directory is written and closed, then the destination is rechecked and
replaced without truncating an existing file. Write/replace failures preserve the previous
report and attempt to remove this operation's staging file.
Missing/non-UTF-8 name tables and output I/O failures produce command-line
argument errors; a name-table read failure leaves the report untouched.

```powershell
python .\tools\parse_bank_file.py "D:\path\to\bankdata.bin"
python .\tools\list_bank_file.py --input "D:\path\to\bankdata.bin" --grid --range 0 3
python .\tools\find_species.py 698 --input "D:\path\to\bankdata.bin"
```

`text_resources.py` is the GUI's local-resource reader. It is not a RomFS
extractor. Descriptive field names document observed behavior and do not claim
to be original program symbols.
