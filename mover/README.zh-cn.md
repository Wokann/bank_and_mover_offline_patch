# Poke Mover 补丁

[English](README.md) · [项目首页](../README.zh-cn.md) · [原版流程与实现分析](docs/code-analysis.zh-cn.md)

支持本体 Title ID `00040000000C9C00`。同一套 `code.bps + exheader.bin + romfs/`
用于 Luma 与 Azahar。默认保留原版存档校验、转换、后续本地检查、提示和游戏保存。
明确开启的自定义转换选项默认关闭，见[配置用法](../configTemplate/templateForCopy/Bank/fakeServers/shared/readme_server.zh-cn.md#自定义选项)
与 [v1.2 兼容范围](docs/version-compatibility.zh-cn.md)。

## 模式和前置条件

标题默认离线，按 **R** 切换在线；A／START／触摸确认本次模式。返回标题保留选择，
重启恢复离线。

当前模式居中显示于 START 上方，下方依次是 R 模式切换、L 语言及 HOME 提示。
文本使用原白色、字号与整数像素坐标，不遮挡 START 或版权标记；L、R 使用专用按键字形，
十套语言的 HOME 原文保持不变。

语言以 Bank 的主机原存档为准。确认后沿原流程关闭选择界面，进入转圈画面，再在后台
更新 Bank 原档的两个语言字段；提交、关闭并重新打开核对成功后，才同步共用 `sav.bin`。
已有 SD 记录只改语言，缺失时复制经过验证的完整原档。Bank 原档缺失／损坏不格式化、
不创建，也不放行 SD 写入。保存或释放结果尚未完成时逐帧等待，不借用已销毁的语言界面。
Mover 不另建自己的主机存档，访问权限只额外列入 Bank；首次初始化仍由 Bank 负责。
沿原流程返回标题，不附加语言保存成功／失败提示，也不替换原生错误处理。

- 离线把宝可梦写入 `sd:/3ds/Bank/fakeServers/users/<userID>/bankdata.bin` 的传送盒。
- 在线仍与官方银行交互，不用离线银行替代服务器正文。
- Mover 不负责首次建行或将官方银行导出为本地文件。离线银行正文缺失按原下载失败流程结束；先用 Bank 完成创建或迁移。Mover 不提前创建用户目录而阻止首次迁移；传送盒已有内容时按原提示处理。
- 两种模式共用 Gen5 数字化兼容功能，所选数字版 `.sav` 的读写均由本地文件后端管理。

[安装说明](../README.zh-cn.md#安装)：Luma 复制完整 Title ID 目录；
Azahar 放到用户目录的 `load/mods/00040000000C9C00/`，不是虚拟 SD 的 Luma 路径。
成套更新并移除旧 `code.ips/code.bin`，补丁目录不附带 README、更新日志或存档。

## 跨语言字形

三份主字库均覆盖默认银行／群组／盒子名称、十套第 1～807 号宝可梦名称及原输入表字符。
复用／复制原生字形，不改变 NDS／VC 名称转换、存档编码、原字号或字宽，纹理占用不增加。
不保证任意汉化文本的全量 CJK；[覆盖、内存与校验说明](../common/docs/fonts.zh-cn.md)。

## Gen5 数字化兼容功能

```text
sd:/roms/nds/
├── 游戏名.nds
└── saves/
    └── 游戏名.sav
```

仅识别《黑》《白》《黑2》《白2》，ROM／存档基本文件名一致。`.sav` 必须可读、可写，
并通过原版校验。每款游戏最多一个来源：有效实卡优先，数字版语言按
`J > O > F > I > D > S > K` 取优先项，同语言保留最先扫描到的有效副本。

SD 上的 Gen5 来源不要求插入 NDS 卡带或已有 VC 存档。
SD ROM 目录不可用时，其他可用的卡带或 VC 来源仍可正常列出。

不要与外部 Transporter Redirect Patch 合并或同时安装。VC 读取对应 VC 游戏的存档；
上述 NDS 路径适用于第五世代数字版，姓名与宝可梦筛选配置在选定游戏后生效。

## 词表与宝可梦筛选

NDS 与 VC 各有独立 `checkName.conf`、`checkPokemon.conf` 和开头的 `[global]` 开关。
关闭时跳过后续内容；缺失或无效的可选配置不替换成全部拒绝。

- NDS：[姓名词表](../configTemplate/templateForCopy/Bank/fakeServers/shared/mover/readme_checkName.zh-cn.md#nds)、[筛选语法／来源](../configTemplate/templateForCopy/Bank/fakeServers/shared/mover/readme_checkPokemon.zh-cn.md#nds-规则)。
- VC：[姓名词表](../configTemplate/templateForCopy/Bank/fakeServers/shared/mover/readme_checkName.zh-cn.md#vc)、[筛选语法／来源](../configTemplate/templateForCopy/Bank/fakeServers/shared/mover/readme_checkPokemon.zh-cn.md#vc-规则)。

词表按 NGWord 语法匹配，名单、地区例外及授权见对应说明。条件规则支持跨字段
`&&`、`||`、括号、多行及分号结尾；字段枚举见配置文档。不按种族列表另设硬编码封禁。

一次整盒请求只加载当前来源的规则／词表，后台完成数值判定后释放；不会逐槽重读 SD。
非法词条只跳过该条；内存／真实 I/O 失败不冒充检测成功。名字变更由原客户端执行。
原版首次姓名提醒与单只命中不是同一条件，详细判定见[分析](docs/code-analysis.zh-cn.md)。

## 保存与恢复

每用户的 `fakeServers/users/<用户ID>/transaction.bin` 协调当前银行、设置及所选来源的操作；
它不是所有游戏的历史，也不复制整份银行、存档或配置。
银行、客户端记录及余额按共用用户表隔离，见[用户选择与迁移](../configTemplate/templateForCopy/Bank/client/readme_client.zh-cn.md)。
离线时标题左上角显示深灰色当前用户和 `编号=用户ID`；单用户不显示切换提示。
多用户可用十字键／左摇杆的↑/↓选择，C 摇杆不参与；按住连续切换并加速。
首项为 auto，末项为最大编号，两端不循环；浏览只操作内存，松手后合并后台保存，
核对并提交最终 activeUser 后再进入。
标题查询不代建用户，也不提前迁移旧档。
数字来源用完整 UTF-16 路径和游戏代码区分，原版描述符仍含编号、版本、长度及令牌。

| 文件 | 策略 |
| --- | --- |
| `bankdata.bin` | 当前银行 |
| `bankdata.bak` | 实际修改前的上一版，不在成功收尾时镜像新版 |
| `bankdata.tmp` | 银行暂存，由事务决定是否发布 |
| `游戏名.sav` | 所选数字版正式存档 |
| 同目录 `游戏名.bak / 游戏名.tmp` | 位于所选 `.sav` 旁的恢复快照／工作镜像 |

确认原保存、提交及清令牌保存成功后，只留下 `.sav`；意外中断保留工作文件。
正常配对回滚恢复成功也会清理；仅回滚银行而未恢复来源时保留来源证据。
只读、不传送退出不轮替银行备份。不要手动删除不明的未决工作；
[文件组合与导入风险](../common/docs/fakeServer/architecture.zh-cn.md#删除损坏和手动换入的处理边界)
列出了缺失、损坏和手动换入的不同处理。

实卡／VC 的真正保存由原 SDK 完成，不能把文件后端的备份等同于原生跨存档原子事务。
CRC32 只作本地一致性摘要，编号／令牌仍随机，原版校验不变。

## 后台与联网边界

票务、服务读写和整盒规则判定可后台运行。SDK 对象分配、正文交付、解析与完成回调仍在
原客户端的等待流程中，后台不借用游戏或 SDK 指针。正文分帧交付，不增加重复 SD 读取；
释放短锁忙时等下一帧，取消或失效结果不能回调为成功。

原系统目录辅助函数保留；不支持的票务操作即时返回原失败，不排队后等待界面解锁。
在线仅在明确识别的模拟器缺票务接口且全局策略允许时使用独立免费兼容响应；
不采用离线商店或礼物配置，其他联网流程仍原样。具体 ABI、模式分派和限制见
[代码分析](docs/code-analysis.zh-cn.md)及[共用服务](../common/README.zh-cn.md)。

## 独立编译

需要 GNU Make、POSIX shell、Python 3、devkitARM，并设置 `DEVKITARM`。
从自己拥有的同一本体取得以下输入：

```text
mover/rom/
├── exheader.bin
├── exefs/00040000000C9C00.dec.code
└── romfs/
```

代码为 `2,269,184` 字节，SHA-1 为 `583859C1E874D11650EFBDDE51F470ECF96900C4`。
推荐复制 GodMode9 挂载根目录的完整 `0x800` 字节 `extheader.bin` 并改名；
工具也识别 `0x400` 字节头部。不要混用其他标题／更新的代码和 ExHeader，不传播原资源。

```sh
make -C mover
# 如工具路径不同：
make -C mover ARMIPS=/path/to/armips IPS_TOOL=/path/to/flips
```

默认显示根目录 `VERSION` 加 Git 提交简写；正式构建须显式使用 `PATCH_RELEASE=1`。补丁版本显示在上屏左上角，官方版本号保持原样和原位置。版本更新及无 Git 源码副本的参数见[统一构建说明](../README.zh-cn.md#从源码构建)。

输出 `release/00040000000C9C00/`。构建保留原初始化，把新增区放在原 BSS 之后，
并校验 BPS、ExHeader、模式跳转和十种语言。默认无需先运行 `clean`；
它会删除此软件的 `build` 和发行输出，不要在其中保存存档或唯一数据。

## 使用限制

首次使用应先备份来源存档与银行，不保证全部来源、真实 SD 断电或官方远端规则。
技术边界见[共用服务说明](../common/README.zh-cn.md)。
HOME、本地数据上传官方服务器及官方认证算法均不属于本地服务提供的能力。
