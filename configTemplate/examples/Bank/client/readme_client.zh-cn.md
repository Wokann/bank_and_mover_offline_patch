# 客户端及用户选择

[English](readme_client.md)

配置为 sd:/3ds/Bank/client/client.conf；缺失自动生成，手动放入的合法值直接使用。下面展示初次使用的默认格式；已预填的用户表按实际条目读取。

`log` 子目录中的 `bank.log`、`mover.log` 仅在共享 `server.conf` 的
`[debug] enablelog=true;` 时记录，默认关闭；见[日志开关说明](../fakeServers/shared/readme_server.zh-cn.md#诊断日志)。
关闭时不记录、不写日志，已有文件保持不变。启用后，每次启动对应软件
覆盖自己的旧日志，不生成日志备份；切换用户或返回标题不清空本次日志。日志不含账户名、
事务令牌或文件正文；写日志失败不会阻止正常业务。
单次日志上限为 1 MiB，达到上限后停止追加，下一次启动重新覆盖。普通记录合批写入，
请求开始与结束时刷新；意外中断时末尾尚未刷新的记录可能缺失。

```ini
[global]
activeUser=auto;

[users]
```

`[users]` 初始留空，由软件在真实创建或识别用户后登记。手动填写时采用 `编号=用户ID`，不接受冒号分隔；行首 `#` 表示注释。共享 `server.conf` 不保存用户表或个人余额。

## 选择

- activeUser=编号 直接选择对应用户表条目，不自动读取主机账号分流；例如 activeUser=2 选择第二项。完整指定用户示例见[统一说明](../readme_fakeServer.zh-cn.md)。
- auto 只读 FRD 当前服务环境及 ACT 账号用户名，核对两者关联的本地账户槽。标准 Nintendo 环境使用 nnid_<用户名>，Pretendo/Nimbus 使用 pnid_<用户名>；不读取 Principal ID 来命名，也不使用好友码或 Mii 昵称。
- 账号接口未实现、无账号、失败、环境不明或数据矛盾时，读取 CFG 主机个人资料用户名，使用 localid_<主机用户名>。这是 3DS 系统设置／模拟器模拟的主机名，不是电脑的 Windows 用户名。
- 主机名也不可读取时，复用用户表中**编号最小**的 localid，不按名称、目录或行序；没有才随机生成 12 位十进制后缀，不足左补 0。随机名核对用户表和两端目录，重名重新随机；持续异常安全返回失败，不覆盖文件或无限等待。
- 用户名目录可直接复用同名既有数据。前缀区分 nnid_、pnid_、localid_；ASCII 大小写不同不会登记为两个相同的 SD 目录。主机名中的空格、控制符及路径／配置分隔符替换为下划线，末尾点也替换为下划线；中文等 UTF-8 名字通过 UTF-16 路径访问 SD。
- 两端已有合法用户目录会补登记。自动读取的账号名须为 6–16 个 ASCII 字母、数字、下划线、连字符及非首尾的点；localid 还允许合法的非 ASCII 字符。生成名字最多 16 个 UTF-16 单元，当前 CFG 主机名最多 10 个；按前缀与终止符预留 32 字符槽，UTF-8 储存区为 64 字节，不能把多字节汉字当成单字节。旧长 ID 和手动 ID 仍限完整 ID 63 字节，不会被截断或强制更名；用户表动态分配。
- 无效选择编号恢复 auto；编号重复保留首个合法绑定，重复 ID 不另建。会话内固定选定用户，不能中途将保存事务转到另一个人。

这是本地账号线索，不发起登录／联网认证，不保证识别任意第三方服务重定向。实机、HLE、LLE 均校验实际结果，不能只凭启用 LLE 推断账号可用。主机用户名可以更改、也可能重名；修改后 auto 可能选择另一个目录，需固定旧数据时使用编号。旧数字 ID 目录仍可手选，不自动改名、合并或删除。

## 标题切换

Bank 在离线及下载模式、Mover 在离线模式的标题左上角显示深灰色用户块；多用户时显示切换提示。例如：

```text
当前用户：auto(2)（按↑/↓键切换用户）
2=localid_Example3DS
```

auto 括号内为当前解析到的编号；手选时第一行显示该编号，第二行为用户表中该编号的原始映射。多用户时，十字键或左摇杆的下／上方向按实际编号正／反向选择；C 摇杆不参与。首项固定为 auto，末项为编号最大的用户，到达两端后停住、不循环；编号可以不连续。轻按移动一项，按住 0.5 秒后每 0.15 秒移动一项，持续按住 2 秒后加速为每 0.06 秒一项。相同方向的两种输入合并，相反方向同时输入不切换；后台保存未完成时不会积压连跳，到达边界后持续按住也不反复读写。单用户仍显示两行用户信息，但不显示切换提示，上下方向不切换。Bank 解锁／原版、Mover 在线模式或没有用户时整块隐藏。

Bank 离线选择所使用的本地银行，下载选择写入哪个本地账户；不会改变官方联网账号。Bank 解锁／原版和 Mover 在线模式隐藏此块，进入这些流程无需核对无关的本地账户；尚未保存的选择仍先完成保存。

标题进入或切入上述可选择用户的模式时读取 client.conf、扫描两侧用户目录，并把用户列表及固定提示缓存到内存；Bank 离线与下载之间切换也会刷新。浏览立即更新显示，不逐项扫描目录、加载提示或写入 SD；松开方向输入约 0.15 秒后，合并保存最终选择。保存期间仍可继续浏览，旧保存结果不会覆盖新的显示；最终选择与已保存选择相同时，无需重复写入。

后台提交只重新读取 client.conf，保留当前用户表，并沿用临时文件校验和备份轮替。在可选择用户的模式按 START／触屏进入前，必须核对并成功提交当前选择、释放响应后才能继续；切入不使用本地账户的模式时，也会先保存尚未提交的选择。若当前编号被外部改成另一个 ID，不覆盖该绑定，也不冒用另一个用户进入。真实 I/O 失败保留期望选择，半秒退避后重试，不把失败当成保存成功。会话数据重新绑定所选用户，不复用前一个人的服务器缓存。

启动阶段已登记的新用户会在首次标题读取中显示；停在标题期间手动新增的条目在下次刷新时加入，可切到其他模式再切回离线／下载以刷新。标题目录刷新还会按当前模板修复已有的 `client.conf`、共享 `server.conf` 及账户 `user.conf`：保留全部合法用户和设置，补齐新条目，移除未知或废弃条目。地区／币种只在格式损坏或越界时采用默认值，不因与本机不同或组合不一致而丢弃。标题阶段不创建账户目录、不迁移银行或刷新自动设置；Mover 仍需先由 Bank 初始化对应用户及银行数据。

## 币种

币种和主机地区均属于各账户的 user.conf，不在 client.conf。币种默认 auto，跟随该用户最终生效的区域／国家；手动币种独立生效。见[个人配置及完整枚举](../fakeServers/users/readme_user.zh-cn.md)。

## 目录和迁移

```text
sd:/3ds/Bank/
├── readme_fakeServer.md
├── readme_fakeServer.zh-cn.md
├── client/
│   ├── client.conf
│   ├── client.bak
│   ├── client.tmp
│   ├── log/
│   │   ├── bank.log
│   │   └── mover.log
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
    │       │   └── checkName.conf
    │       └── vc/
    │           ├── checkPokemon.conf
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

上图同时列出分发文件及运行时可能出现的用户／临时文件，不要求预先创建。`<userID>` 代表每个独立用户；客户端记录位于 `client/users/<userID>/`，银行及事务位于 `fakeServers/users/<userID>/`。分类说明见[统一目录说明](../readme_fakeServer.zh-cn.md)。

Bank/Mover 共用同一用户表及所选用户数据。三份基础配置可自动生成；礼物、Mover 配置／词表／素材由玩家手动放入。附加输入保持只读；汉化许可仅影响当前账户的内存规则，详见用户配置说明。

请先用 Bank 完成离线用户初始化及银行创建或迁移。离线 Mover 不代建银行，也不触发旧文件迁移：若当前用户目录尚不存在，进入服务时只在内存读取共享配置／默认值，不创建该用户的目录或配置，不影响之后 Bank 的首次自动迁移。标题对其他既有正式配置的结构修复不代表该账户已初始化。银行正文缺失按原下载失败流程结束，不显示“服务器被锁定”，也不继续传送保存。已有用户的正常配置修复及银行备份恢复不受影响。

### 旧布局迁移

以下六个根目录文件仅用于旧布局升级迁移，不属于当前账户的常驻文件。下表路径相对 `sd:/3ds/Bank/`：

| 旧布局迁移来源 | 当前用户目标 |
| --- | --- |
| `bankdata.bin` | `fakeServers/users/<userID>/bankdata.bin` |
| `bankdata.bak` | `fakeServers/users/<userID>/bankdata.bak` |
| `bankdata.tmp` | `fakeServers/users/<userID>/bankdata.tmp` |
| `sav.bin` | `client/users/<userID>/sav.bin` |
| `sav.bak` | `client/users/<userID>/sav.bak` |
| `sav.tmp` | `client/users/<userID>/sav.tmp` |

v0.9.1 自动迁移仅在两端均没有**任何用户目录**时进行：旧根目录 bankdata.bin/.bak/.tmp 移至第一个所选用户的服务器目录；sav.bin/.bak/.tmp 移至同一用户的 client 目录。使用同一 SD 内改名移动，成功后旧路径消失，不额外复制一套。六个目标中任一已有文件则整组不迁，不覆盖。旧事务、购券和余额不自动导入。已有任一用户目录后，根目录遗留文件只能手动迁移。若部分移动失败，成功移动的文件保留在目标，未移动的源文件保留，不能把源文件全部当垃圾删除。

手动搬家／更名时停止两款软件并备份，同时调整 client/users 与 fakeServers/users 两端目录名和用户表 ID。固定选择按编号。auto 会按真实账号重新推导 nnid/pnid，不能靠目录更名改变主机账号；自定义 ID 可固定编号使用。不要公开银行、客户端记录、ID 或事务中的私人数据。

## 接口依据及限制

- [libctru FRD](https://github.com/devkitPro/libctru/blob/master/libctru/source/services/frd.c)：服务环境、本地账户槽及调用 SDK 版本。
- [libctru ACT](https://github.com/devkitPro/libctru/blob/master/libctru/include/3ds/services/act.h)：账号用户名字段 8 与 Principal ID 字段 12 不同；本补丁使用前者及关联 Friends 槽。
- [Azahar CFG](https://github.com/azahar-emu/azahar/blob/master/src/core/hle/service/cfg/cfg.cpp)：主机个人资料用户名块及十个 UTF-16 单元的上限。
- [Nimbus 初始化](https://github.com/PretendoNetwork/nimbus/blob/main/app/source/main.cpp)和[环境定义](https://github.com/PretendoNetwork/nimbus/blob/main/app/source/common.hpp)：标准 Nintendo/Pretendo 环境映射。
- [Azahar ACT HLE](https://github.com/azahar-emu/azahar/blob/master/src/core/hle/service/act/act.cpp)：成功返回可能未填入账号数据，故补丁不仅检查成功码。

这是开源实现依据，不是官方对补丁的识别保证；其他重定向环境可能只能使用 localid。

## 配置备份

client.bak 保留写入前一版用户表，client.tmp 只作暂存。主文件缺失时先读备份，
两者均缺失才重建默认配置；不从 tmp 猜测用户选择。合法的手动值保留，内容不变不写盘。
个人 user.conf 的字段说明见[个人配置](../fakeServers/users/readme_user.zh-cn.md)，完整策略见[统一说明](../readme_fakeServer.zh-cn.md#写入文件与保护策略)。
