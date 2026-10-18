# bankdata GUI 查看器

[English](gui.md)

这是一个只读 Tkinter 查看器，用于显示盒子、群组、多语言物种名、
存档昵称和文件元数据。程序只使用 Python 标准库和 `resources/` 中的
固化资源，运行时无需读取 RomFS。

```powershell
python .\gui\bank_viewer.py
python .\gui\bank_viewer.py "D:\path\to\bankdata.bin"
python .\gui\bank_viewer.py "D:\path\to\bankdata.bin" --language zh-Hans
```

不传入文件路径时，启动后点击“打开 bankdata”自行选择；程序不预设
bankdata 路径。界面支持日语纯假名、日语汉字、英语、德语、意大利语、
法语、西班牙语、韩语、简体中文和繁体中文。中文物种名使用普通 Unicode
名称表，存档昵称则另行使用私用区字码转换表。

无界面自检：

```powershell
python .\gui\bank_viewer.py --self-test "D:\path\to\bankdata.bin"
```

下载模式取得的 `bankdata.bin` 可能包含与账户有关的私人标识、玩家及训练家
信息和精确时间记录。请勿公开上传或随意分享该文件。
