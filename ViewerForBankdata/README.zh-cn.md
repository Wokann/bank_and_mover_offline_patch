# bankdata 查看器

本目录包含只读的 Python 标准库 GUI 查看器和少量 bankdata 诊断工具。
GUI 已固化十种语言选择所需的文本资源，运行时不依赖解包的 RomFS。
银行和资源须为独立普通文件，不支持符号链接、目录联接或硬链接。

```powershell
cd ViewerForBankdata
python .\gui\bank_viewer.py
```

启动时不预设输入文件，请在文件选择窗口中自行打开 bankdata。参数见
[GUI 说明](docs/gui.zh-cn.md)，文件检查工具见[命令行说明](docs/cli.zh-cn.md)。
