# Pokemon Bank 与 Poke Mover 离线补丁

[English](README.md) · [下载最新版](../../releases/latest) · [所有发行版本](../../releases)

本项目为 Nintendo 3DS 上的 Pokemon Bank 和 Poke Mover 提供 Luma3DS 补丁，使银行数据可以保存在 SD 卡并离线读写。两款软件共用：

```text
sd:/3ds/Bank/bankdata.bin
```

补丁不会把离线修改上传回官方服务器。上传补丁已因服务端身份、版本、时间戳、事务状态和冲突校验无法得到充分保证而永久废止。

## 功能

| 软件 | 标题界面模式 | 功能 |
|---|---|---|
| Pokemon Bank | 离线模式／下载模式／解锁模式 | 离线读写本地银行、从官方服务器下载完整银行数据到 SD 卡，或进入原版存档不匹配强制解锁流程 |
| Poke Mover | 离线模式／原版模式 | 离线把 Pokemon 写入同一份本地银行的传送盒，或按原版方式连接官方服务器 |

- 在标题界面按实体 **R** 键切换模式；按 **A**、**START** 或触摸下屏进入后，本次会话的模式固定。
- Bank 离线模式可以在没有可用文件时调用原版首次使用流程创建新的本地银行。
- Bank 会验证主文件、尝试从 `bankdata.bak` 恢复，并通过 `bankdata.tmp`、完整写入检查和上一版本备份保护保存过程。
- Bank 下载模式保留官方账户、游戏检测和服务器下载流程；数据保存到本地后直接返回标题界面，不进入盒子或上传流程。
- Bank 解锁模式保留原版联网事务恢复流程，并在官方挑战码界面显示服务器返回的一个解锁候选值。
- Poke Mover 离线模式保留原版游戏读取、筛选、转换、传送盒检查和来源游戏保存，只把服务器银行读写替换成本地事务。
- 两款软件的新增文本均覆盖原软件自带的十套语言资源。

## 下载与安装

普通玩家不需要原始 `.code`、RomFS 或本地编译环境。

1. 从 [GitHub Releases](../../releases/latest) 下载最新发行压缩包并解压。
2. 把所需的完整 Title ID 目录复制到 `SD:/luma/titles/`：

   | 软件 | Title ID | 安装后路径 |
   |---|---|---|
   | Pokemon Bank | `00040000000C9B00` | `SD:/luma/titles/00040000000C9B00/` |
   | Poke Mover | `00040000000C9C00` | `SD:/luma/titles/00040000000C9C00/` |

   每个目录内应包含 `code.ips` 和 `romfs/`。
3. 按住 `SELECT` 开机进入 Luma3DS 配置，启用 `Enable game patching`，保存并重启。
4. 使用前备份 SD 卡以及已有的 `SD:/3ds/Bank/` 目录。

本项目针对软件本体，而不是更新数据：

```text
Pokemon Bank: 00040000000C9B00
Poke Mover:   00040000000C9C00
```

## 使用方法

### Pokemon Bank

标题界面默认是**离线模式**，按 **R** 可依次切换到**下载模式**、**解锁模式**，再回到离线模式。

如果官方服务器中已有自己的银行数据，建议先进入下载模式，选择“下载银行数据到本地”，按原版流程完成账户确认并选择任意可用游戏。看到下载完成并返回标题界面后，切回离线模式，即可按原版 Bank 流程管理盒子并保存到本地。

下载模式会覆盖当前本地银行，执行前请备份。如果不需要保留服务器数据，也可以直接进入离线模式；本地没有有效主文件或备份时，补丁会走原版首次使用流程创建新的本地银行。

解锁模式只用于原版的“存档不匹配”锁定。进入第一项并选择对应游戏，在确认游戏的同时按住 **L + A + START**，即可进入官方强制解锁界面。服务器若返回可接受的候选值，补丁会在第三行显示其中第一个值的八位形式；后续回滚、验证和服务器处理全部保持原版流程。该模式不会捕获 `bankdata.bin`，也不会套用下载模式的提前退出路线。

> **隐私提醒：** 下载模式取得的 `bankdata.bin` 可能包含与账户有关的私人标识、玩家及训练家信息和时间记录。请勿公开上传或随意分享该文件。

### Poke Mover

标题界面默认是**离线模式**，按 **R** 可切换到**原版模式**。

- 离线模式使用 Bank 已下载或首次初始化生成的 `bankdata.bin`；Mover 自身不会下载或创建这个文件。
- 传送成功后，Pokemon 会写入同一份本地银行的传送盒，可用 Bank 离线模式取出。
- 原版模式完整使用官方联网路线，不会读取、写入或整理 `SD:/3ds/Bank/` 下的本地文件。
- 本地文件无效或传送盒已被占用时，Mover 会保留原文件并进入相应错误路线。

### bankdata 查看器

项目附带的 [bankdata 查看器](ViewerForBankdata/README.zh-cn.md) 可用于查看本地 `bankdata.bin`。使用 Python 3 运行后，选择需要打开的文件即可：

```powershell
cd ViewerForBankdata
python .\gui\bank_viewer.py
```

## 数据安全与限制

- 在实机使用前备份 SD 卡、来源游戏存档和 `SD:/3ds/Bank/`。
- 不要在不同账户之间混用银行文件。
- 下载模式只负责“服务器到本地”，离线模式只操作本地文件，不存在“本地到服务器”的上传功能。
- 解锁模式使用官方联网恢复路线，可能改变官方事务状态，但不会上传本地离线 `bankdata.bin`。
- Poke Mover 原版模式与本地离线银行相互独立。

## 非官方声明

本项目是独立制作的非官方社区补丁，与 Nintendo、The Pokemon Company、Creatures Inc.
及 GAME FREAK inc. 没有从属或合作关系，也未获得这些公司赞助、认可或授权。Nintendo
3DS、Pokemon、Pokemon Bank、Poke Mover，以及相关游戏、软件、角色、名称、标志、
图像、文本、音乐和其他原作内容的商标、著作权及其他知识产权均归各自权利人所有。
本项目仅用于技术研究、软件保存与互操作性，不主张拥有任何相关原作内容。

## 开发者本地编译

Bank 与 Mover 可以分别独立构建。各子项目的实现原理、内存与状态机、文件事务、输入提取、编译、验证和参考资料见：

- [Pokemon Bank 开发文档](bank/README.zh-cn.md)
- [Poke Mover 开发文档](mover/README.zh-cn.md)

### 基本工具

- GNU Make、兼容 POSIX 的 shell 和 Python 3。
- [devkitARM](https://devkitpro.org/wiki/Getting_Started)，并设置 `DEVKITARM` 环境变量。
- 仓库自带 Windows 版 [armips](https://github.com/Kingcom/armips) 与 [Floating IPS](https://github.com/Sir-Walrus/Flips)。其他平台需从官方项目下载或编译，并通过 `ARMIPS` 和 `IPS_TOOL` 指定路径。
- [GodMode9](https://github.com/d0k3/GodMode9)，用于从用户自己的软件中提取已解密、已解压的 `.code` 与完整 RomFS。

请勿提交或传播提取出的代码和资源。本工程只支持以下本体镜像：

| 软件 | 工程内路径 | 大小 | SHA-1 |
|---|---|---:|---|
| Pokemon Bank | `bank/rom/exefs/00040000000C9B00.dec.code` | 2,801,664 字节 | `5AB630856835DCF2DBDF9A62244DD19E46AE1C7C` |
| Poke Mover | `mover/rom/exefs/00040000000C9C00.dec.code` | 2,269,184 字节 | `583859C1E874D11650EFBDDE51F470ECF96900C4` |

对应的完整 RomFS 分别放入 `bank/rom/romfs/` 和 `mover/rom/romfs/`。准备输入后，在仓库根目录运行：

```sh
make -C bank clean
make -C bank

make -C mover clean
make -C mover
```

其他平台可覆盖工具路径：

```sh
make -C bank ARMIPS=/path/to/armips IPS_TOOL=/path/to/flips
make -C mover ARMIPS=/path/to/armips IPS_TOOL=/path/to/flips
```

输出位于：

```text
release/00040000000C9B00/
├── code.ips
└── romfs/

release/00040000000C9C00/
├── code.ips
└── romfs/
```

## 引用与鸣谢

- [Luma3DS](https://github.com/LumaTeam/Luma3DS)：提供按 Title ID 加载 IPS 与 LayeredFS 资源的运行环境。
- [GodMode9](https://github.com/d0k3/GodMode9)：用于从用户自己的软件中提取可执行代码与 RomFS。
- [devkitPro / libctru](https://github.com/devkitPro/libctru)：devkitARM 工具链及公开的 3DS FSUSER／FSFILE 接口参考。
- [armips](https://github.com/Kingcom/armips)：ARM 汇编、对象导入和代码注入。
- [Floating IPS](https://github.com/Sir-Walrus/Flips)：生成 IPS 补丁。
- [pkNX TextFile](https://github.com/kwsch/pkNX/blob/master/pkNX.Structures/Text/TextFile.cs)：消息文件编码与行表格式参考。
- [zaksabeast/Transporter-Offline-Patch](https://github.com/zaksabeast/Transporter-Offline-Patch)：Poke Mover 联网状态替换的公开先例。
- [Transporter-PKSM-Bank-Patch 状态机笔记](https://github.com/zaksabeast/Transporter-PKSM-Bank-Patch/blob/master/TRANSPORTER_DOCS.md)：用于交叉核对 Poke Mover 的高层状态顺序。

程序地址、状态转换、对象布局、文件偏移和补丁位置均以本项目支持版本的实际二进制分析与测试为准；外部项目只用于核对公开接口、格式和高层行为。
