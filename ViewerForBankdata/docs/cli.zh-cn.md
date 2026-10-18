# bankdata 诊断工具

[English](cli.md)

该目录内的 Python 文件提供 GUI 和命令行分析所需的只读格式解析功能。
输入文件必须显式指定，工具不会自行搜索 bankdata 或 RomFS。

三个命令都接受 `0xBB518` 字节服务器正文或带八字节前缀的 `0xBB520` 字节内存对象。
读取先检查长度、格式和盒数，不代表宝可梦合法性审核。种族扫描按两字节对齐，避免
`0x1B56` 盒子步长导致隔盒漏查；最后一个可能记录也会检查。

银行输入、`--names` 名称表、固化资源和报告都必须使用独立普通文件；
符号链接、目录联接和硬链接均不支持，路径中的链接目录也会拒绝。
`list_bank_file.py --output` 可导出 UTF-8 文本报告，但不能指向银行输入或
名称表的同一个文件，包括路径别名。
报告先写入同目录的新临时文件，成功关闭并复核目标后才替换输出路径，不直接截断
已有文件。写入或替换失败时保留旧报告，
并尝试清理本次临时文件。名称表缺失、编码不是 UTF-8 或报告写出失败时，会给出
命令行参数错误；名称表读取失败不会改写报告。

```powershell
python .\tools\parse_bank_file.py "D:\path\to\bankdata.bin"
python .\tools\list_bank_file.py --input "D:\path\to\bankdata.bin" --grid --range 0 3
python .\tools\find_species.py 698 --input "D:\path\to\bankdata.bin"
```

`text_resources.py` 只负责读取 GUI 的本地固化资源，不包含 RomFS 提取功能。
代码中的描述性字段名用于说明实测数据作用，不代表原程序符号。
