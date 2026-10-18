# fakeServer 配置、目录与数据保护

[English](readme_fakeServer.md) · [客户端配置](client/readme_client.zh-cn.md) · [共享服务器配置](fakeServers/shared/readme_server.zh-cn.md) · [个人配置](fakeServers/users/readme_user.zh-cn.md)

## 两种分发内容

- `configTemplate/templateForCopy/Bank/`：供正式使用直接复制的空用户模板。保留 `activeUser=auto`，用户表为空，不预建用户目录。
- `configTemplate/examples/Bank/`：整套独立副本，包含同样的共享配置、礼物素材、Mover 词表、说明，以及三份虚构用户配置；`activeUser=2` 固定选择第二项。
- 两套 README 内容相同，差别只在客户端配置与演示个人数据。各自引用自己的相对路径，不共用另一套文件。

正式使用请将空模板的 Bank 文件夹复制到 `sd:/3ds/Bank/`。不要将有演示用户的副本合并到正式 SD 卡：有效用户表会参与选择，任何已有用户子目录都会阻止首次自动迁移。

## 指定用户的格式示例

```ini
[global]
activeUser=2;

[users]
1=nnid_ExampleNNID;
2=pnid_ExamplePNID;
3=localid_Example3DS;
```

三条记录均为有效语法，不加注释符号；`activeUser=2` 指的是用户表编号 2，不是 ID 后缀。改为 1、3 或 auto 可体验其他选择方式。示例使用虚构的账号用户名／主机用户名，不使用 PID 或好友码；详见[用户选择](client/readme_client.zh-cn.md#选择)。

| 虚构用户 ID | 演示个人配置内容 |
| --- | --- |
| `nnid_ExampleNNID` | 未购券、未领试用；日元本地余额 1000。 |
| `pnid_ExamplePNID` | 已购一次年券；期限示例 2026-01-01 至 2026-12-31；美元本地余额 499（4.99）。 |
| `localid_Example3DS` | 已领试用、当前无有效券；无币种本地余额 1614。 |

这些 ID 和余额不是实际账号、认证资料或 eShop 资金，也不是新用户的默认值。新用户的购券、试用记录和余额仍以补丁内置默认值初始化。

## 完整文件树

```text
sd:/3ds/Bank/
├── readme_fakeServer.md
├── readme_fakeServer.zh-cn.md
├── bankdata.bin
├── bankdata.bak
├── bankdata.tmp
├── sav.bin
├── sav.bak
├── sav.tmp
├── client/
│   ├── client.conf
│   ├── client.bak
│   ├── client.tmp
│   ├── readme_client.md
│   ├── readme_client.zh-cn.md
│   └── users/
│       └── <userID>/
│           ├── sav.bin
│           ├── sav.bak
│           └── sav.tmp
└── fakeServers/
    ├── shared/
    │   ├── server.conf
    │   ├── server.bak
    │   ├── server.tmp
    │   ├── readme_server.md
    │   ├── readme_server.zh-cn.md
    │   ├── gifts/
    │   │   ├── gift.conf
    │   │   ├── readme_gift.md
    │   │   ├── readme_gift.zh-cn.md
    │   │   ├── 01_celebi/
    │   │   │   └── celebi.pl6
    │   │   ├── 02_johto_starters/
    │   │   │   └── johto_starters.pl6
    │   │   ├── 03_regi_trio/
    │   │   │   └── regi_trio.pl6
    │   │   ├── 04_mewnium_z/
    │   │   │   └── mewnium_z.wc7
    │   │   ├── 05_alola_starters/
    │   │   │   ├── decidueye.wc7
    │   │   │   ├── incineroar.wc7
    │   │   │   └── primarina.wc7
    │   │   └── 06_ha_apes/
    │   │       ├── oranguru.wc7
    │   │       └── passimian.wc7
    │   └── mover/
    │       ├── readme_checkPokemon.md
    │       ├── readme_checkPokemon.zh-cn.md
    │       ├── readme_checkName.md
    │       ├── readme_checkName.zh-cn.md
    │       ├── LICENSE.badword.txt
    │       ├── nds/
    │       │   ├── checkPokemon.conf
    │       │   ├── checkPokemon.bak
    │       │   ├── checkPokemon.tmp
    │       │   └── checkName.conf
    │       └── vc/
    │           ├── checkPokemon.conf
    │           ├── checkPokemon.bak
    │           ├── checkPokemon.tmp
    │           └── checkName.conf
    └── users/
        ├── readme_user.md
        ├── readme_user.zh-cn.md
        └── <userID>/
            ├── user.conf
            ├── user.bak
            ├── user.tmp
            ├── bankdata.bin
            ├── bankdata.bak
            ├── bankdata.tmp
            ├── transaction.bin
            ├── transaction.bak
            └── transaction.tmp
```

- 上图列出当前分发的全部配置、素材和说明，以及运行时可能出现的个人、临时与迁移文件；不表示这些文件同时存在。
- `<userID>` 表示每位用户各自的目录，两端必须使用同一个完整 ID；不是要求创建名为占位符的目录。
- 用户目录中的 `sav.bin`、`bankdata.bin` 和事务文件由正常流程创建或迁移，不在空模板中放入伪造或零字节占位二进制。
- `.tmp` 是尚未发布的暂存，不应手工创建；`.bak` 是否保留由对应保存／恢复流程决定。
- 根目录的 `bankdata.bin`、`bankdata.bak`、`bankdata.tmp`、`sav.bin`、`sav.bak`、`sav.tmp` 是旧布局的迁移来源，成功迁移后原路径消失。
- 客户端用户选择在 `client.conf`；个人地区在 `user.conf` 的 `[console]`，币种在 `[ticket]`。共享政策与素材在 `fakeServers/shared/`。个人购券记录和 `balance.$币种` 都在 `user.conf` 的 `[ticket]` 下，以空行分隔，不另设余额段。
- 修改个人数据前请停止软件并备份，只修改所需配置字段；不要覆盖银行、事务或游戏存档。

## 写入文件与保护策略

下表是 Bank／Mover 及补丁直接维护或重定向的本地运行数据；不包含构建输出、PC 工具导出及模拟器自身的缓存／日志。路径相对 `sd:/3ds/Bank/`，备份和暂存均与主文件同目录。

| 主文件／对象 | 备份文件 | 暂存文件 | 发布与保留策略 |
| --- | --- | --- | --- |
| `client/client.conf` | `client.bak` | `client.tmp` | 通用保护；保存用户选择及用户表，保留写入前一版。 |
| `fakeServers/shared/server.conf` | `server.bak` | `server.tmp` | 通用保护；保存共享窗口、价格及充值额，保留写入前一版。 |
| `fakeServers/users/<userID>/user.conf` | `user.bak` | `user.tmp` | 通用保护；个人购券／试用记录与余额作为一份文件一起发布，保留写入前一版。 |
| `client/users/<userID>/sav.bin` | `sav.bak` | `sav.tmp` | 专用事务保护；包括身份和事务字段，保留写入前一版，不按普通配置回退。 |
| `fakeServers/users/<userID>/bankdata.bin` | `bankdata.bak` | `bankdata.tmp` | 专用银行事务保护；保留实际修改前一版，恢复须符合事务及正文检查。 |
| `fakeServers/users/<userID>/transaction.bin` | `transaction.bak` | `transaction.tmp` | 专用日志保护；保留上一条已发布日志，核对记录 CRC32、格式、序号与事务阶段。 |
| `fakeServers/shared/mover/<nds或vc>/checkPokemon.conf` | `checkPokemon.bak` | `checkPokemon.tmp` | 仅汉化许可把 BanUnicodeChinese 的 enabled 写为 0；保留修改前一版，不生成缺失检查表。 |
| 所选 NDS 游戏旁的 `x.sav`，不在 Bank 目录内 | `x.bak` | `x.tmp` | 来源及令牌约束的事务保护；确认保存及清令牌保存完成后清理，意外中断时保留。 |
| Bank 主机存档，Mover 直接访问其中的 `/turtle` | 不额外增加 SD 备份 | 不额外增加 SD 暂存 | Bank 沿原存档流程；Mover 仅更新语言字段，提交后重新打开并核对整份记录。 |
| 所选卡带、3DS 数字版及 VC 游戏存档 | 由原存档系统管理 | 由原存档系统管理 | 保留原版校验、保存及事务流程，不套用通用配置回退，不臆造物理文件路径。 |
| 旧根目录的六份 `bankdata`／`sav` 文件，逐项见上树 | 原名搬迁 | 原名搬迁 | 仅首次迁移时同 SD 改名移动；不覆盖目标，成功后源路径消失，失败的源文件保留。 |

基础配置及下述指定规则字段使用的公共流程为：生成新内容 → 写齐并刷新、关闭暂存 → 回读核对 → 旧主文件轮替为备份 → 暂存发布为主文件。内容不变时不写盘，也不轮替备份；首次生成且没有旧主文件时不伪造备份。成功发布后保留上一版，不把新主文件再复制成相同的备份。

主配置存在时采用当前文本并逐项修复，保留合法值，不因某条格式错误就整份回退。主文件缺失时先读备份，再解析并重建主配置；主文件及备份都缺失才使用内置默认值，孤立暂存不作已提交依据。I/O 错误不是文件缺失，不静默清零。只读启动可读取备份视图，但不在后台悄悄发布或清理文件。没有可恢复来源时，默认值不能重现旧余额或购券历史。

`transaction.bak` 是上一条日志决定，不是银行正文备份：仅正式日志缺失才读它；损坏／不可读的正式日志不会静默回退，日志暂存不作权威。一般文件保护不决定银行、来源存档或日志的提交／回滚；这些仍由事务层核对后处理。需要清理备份的业务，必须先明确确认整个操作完成，不能仅因一次写入回调成功就删除恢复依据。

### 补丁严格只读的文件

checkPokemon 只有上表所列指定字段写入例外，不是通用规则编辑器。

- `fakeServers/shared/gifts/gift.conf`
- `fakeServers/shared/mover/nds/checkName.conf`
- `fakeServers/shared/mover/vc/checkName.conf`
- 礼物 PL6／WC6／WC7 素材、NDS ROM、README 和许可证。

这些内容只由玩家手动维护，补丁没有编辑入口，也不创建、恢复或删除它们的同名备份／暂存。缺少可选主文件表示未配置，不从旧备份重新开启功能。说明与许可证不会由补丁自动生成。

上述轮替可保护文件替换中断，不保证 SD 控制器或文件系统在任意断电时仍原子落盘。重要数据仍应另行备份。
