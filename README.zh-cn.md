# Pokemon Bank 与 Poke Mover 离线补丁

[English](README.md) · [下载](../../releases/latest) · [更新记录](CHANGELOG.zh-cn.md)

让 Nintendo 3DS 上的 Pokemon Bank 和 Poke Mover 使用保存在 SD 卡上的银行数据。保留熟悉的盒子管理与传送界面，并提供彼此独立的离线和官方联网模式。

## 主要功能

- 无需连接官方服务器，即可管理本地银行并接收 Mover 传送。
- 下载已有官方银行供离线使用，或创建新的本地银行。
- 支持多个本地用户，各自保存银行、设置、余额及主机区域信息。
- 通过文本配置调整礼物、姓名词表与宝可梦筛选规则。
- Mover 除卡带与 VC 来源外，也支持 SD 卡上的第五世代 ROM／存档组合。
- 标题界面按 **R** 切换模式、按 **L** 选择语言，并可选择本地用户；新增文本覆盖原软件的十套语言资源。

本地服务器与官方服务器的数据彼此独立；本项目不提供将离线修改上传回官方服务器的功能。

## 安装

操作前请备份银行数据及游戏存档；Bank 与 Mover 补丁需要一起更新。

### 1. 安装补丁

下载[最新发行版](../../releases/latest)。使用 Luma3DS 的主机，将所需的完整 Title ID 文件夹复制到 `SD:/luma/titles/`：

| 软件 | 补丁目录 |
| --- | --- |
| Pokemon Bank | `SD:/luma/titles/00040000000C9B00/` |
| Poke Mover | `SD:/luma/titles/00040000000C9C00/` |

每个目录都必须包含 `code.bps`、`exheader.bin` 和 `romfs/`。移除对应补丁目录里的旧 `code.ips` 或 `code.bin`。按住 `SELECT` 开机，在 Luma 配置中启用 `Enable game patching`，保存并重启。

这里使用软件本体的 Title ID，不是更新数据的 ID。使用发行补丁不需要提取可执行文件或配置编译环境。

### 2. 复制配置模板

全新安装时，将 [configTemplate/templateForCopy](configTemplate/templateForCopy) 中的 **Bank** 文件夹复制到 `SD:/3ds/`，形成：

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

请完整复制模板，包括 `gifts/` 和 `mover/` 内的文件。缺失的客户端、服务器及用户基础配置可以自动生成，但礼物素材、Mover 姓名词表及宝可梦筛选配置需要手动放入 SD 卡才能使用。银行及用户的运行数据会按需创建，不要自行创建空白二进制文件。

已有数据时，请保留个人文件与设置，不要整目录覆盖；按照[配置与迁移说明](configTemplate/templateForCopy/Bank/readme_fakeServer.zh-cn.md)更新。`configTemplate/examples/` 是带演示用户的独立示例，不是安装模板。

**Azahar：** 补丁目录放在模拟器用户目录的 `load/mods/`，而不是虚拟 SD 卡内；同一套 Bank 配置放在该用户目录的 `sdmc/3ds/Bank/`。

## 开始使用

1. 先启动 **Bank**。在标题界面按 **R** 选择模式；显示用户信息时，确认目标本地用户。
2. 若要保留已有官方银行，先使用**下载模式**；它会覆盖所选用户的本地银行，务必先备份。不需要下载时，直接进入**离线模式**创建本地银行。
3. 使用 **Mover 离线模式**将宝可梦送入该用户银行的传送盒，再用 **Bank 离线模式**取出。Mover 要求 Bank 已完成银行初始化或迁移。

| 软件 | 模式 | 用途 |
| --- | --- | --- |
| Bank | 离线 | 管理并保存所选本地银行。 |
| Bank | 下载 | 将官方银行复制到所选本地用户，完成后返回标题。 |
| Bank | 解锁 | 使用官方事务恢复流程；强制恢复可能丢失中断传输中的宝可梦。 |
| Bank | 原版 | 使用官方银行界面及联网服务。 |
| Mover | 离线 | 向所选本地银行传送。 |
| Mover | 在线 | 通过官方服务传送。 |

软件启动时默认离线模式。按 **A**、**START** 或触摸下屏确认当前选择。Bank 离线／下载模式及 Mover 离线模式可用 **↑/↓** 或左摇杆选择用户；**auto** 按当前账号环境自动选择。标题界面的 **L** 键用于选择两款软件共用的语言。

恢复、语言及来源存档的详细说明见 [Bank 使用说明](bank/README.zh-cn.md)与 [Mover 使用说明](mover/README.zh-cn.md)。第五世代数字来源需将 `sd:/roms/nds/name.nds` 与 `sd:/roms/nds/saves/name.sav` 同名配对。

## 配置与文档

- [SD 目录及文件保护](configTemplate/templateForCopy/Bank/readme_fakeServer.zh-cn.md)
- [用户选择与迁移](configTemplate/templateForCopy/Bank/client/readme_client.zh-cn.md)
- [服务器政策与可选转换](configTemplate/templateForCopy/Bank/fakeServers/shared/readme_server.zh-cn.md)
- [个人地区、币种与使用券](configTemplate/templateForCopy/Bank/fakeServers/users/readme_user.zh-cn.md)
- [礼物配置](configTemplate/templateForCopy/Bank/fakeServers/shared/gifts/readme_gift.zh-cn.md)
- [宝可梦筛选及规则来源](configTemplate/templateForCopy/Bank/fakeServers/shared/mover/readme_checkPokemon.zh-cn.md)
- [姓名词表、白名单及来源](configTemplate/templateForCopy/Bank/fakeServers/shared/mover/readme_checkName.zh-cn.md)
- [银行数据查看器](ViewerForBankdata/README.zh-cn.md)

## 数据安全与限制

- 保留独立的 SD 卡及来源游戏存档备份。不要混用不同用户的文件，也不要删除事务记录来绕过保存中断错误。
- 手动修改或替换银行文件前，请停止两款软件。无法确定匹配版本时，保留全部相关文件。
- 下载模式覆盖本地数据；解锁模式会改变官方事务状态，并可能放弃中断的传输。两者都不会上传本地银行。
- 本地检测及配置规则不是官方服务器私有规则的完整复现。CRC32 用于发现意外变化，不提供密码学认证。
- 银行及设置文件含私人标识与训练家信息，请勿公开上传。
- 已内置第五世代数字来源支持，不要再叠加外部 Transporter Redirect Patch。

## 从源码构建

Bank 与 Mover 分别使用 GNU Make、Python 3、devkitARM、armips 和 Floating IPS 构建。原始解密／解压代码、ExHeader 及 RomFS 必须自行从合法持有的软件中提取，本仓库不分发这些输入。

- [Bank 构建与实现说明](bank/README.zh-cn.md#独立编译)
- [Mover 构建与实现说明](mover/README.zh-cn.md#独立编译)
- [共用服务与存储](common/README.zh-cn.md)

## 鸣谢

本项目使用 [Luma3DS](https://github.com/LumaTeam/Luma3DS)、[devkitPro/libctru](https://github.com/devkitPro/libctru)、[armips](https://github.com/Kingcom/armips) 和 [Floating IPS](https://github.com/Sir-Walrus/Flips)。[GodMode9](https://github.com/d0k3/GodMode9) 用于软件数据提取；词表、筛选规则和礼物素材的来源及许可证列在对应配置说明中。

## AI 使用声明

本项目使用 AI 工具（包括 OpenAI Codex）辅助分析反编译结果、设计补丁、编写代码及审查修改。如果你介意使用 AI 参与反编译分析或代码开发的补丁，请在使用前了解这一点，并自行决定是否使用。

## 非官方声明

本项目是独立制作的非官方社区补丁，与 Nintendo、The Pokemon Company、Creatures Inc. 及 GAME FREAK inc. 没有从属或合作关系，也未获得其赞助、认可或授权。原软件、角色、名称及素材的知识产权归各自权利人所有。本项目用于软件保存、技术研究与互操作性。
