# Mover 宝可梦筛选配置

[English](readme_checkPokemon.md) · [词表及来源许可](readme_checkName.zh-cn.md) · [服务器配置](../readme_server.zh-cn.md)

NDS 与 VC 共用本说明中的语法，但仍分别读取自己的文件及全局开关；不会混用规则。

| 来源 | 配置文件（相对本目录） |
| --- | --- |
| 第五世代 NDS | [nds/checkPokemon.conf](nds/checkPokemon.conf) |
| Virtual Console | [vc/checkPokemon.conf](vc/checkPokemon.conf) |

实际路径位于 `sd:/3ds/Bank/fakeServers/shared/mover/` 下。文件缺失或全局开关损坏时，
不启用该来源附加规则，不影响另一来源及原版本地检查。这是可编辑的本地兼容预设，
不是官方 v1.5 完整服务端合法性表的提取结果。说明与许可证为外部资料，游戏不读取。

## 组织和编辑

开头写 `[global]`、`enableCheckPokemon=true;` 或 `enableCheckPokemon=false;`。
关闭时跳过后续规则。补丁不修改规则文件；账户汉化许可仅在内存中覆盖 BanUnicodeChinese。一条规则从 `[rule.name]`
开始，到下一条规则段或文件末尾结束；名称接受 1–31 个大小写 ASCII 字母、数字或下划线，
仅是本地标签，不写入游戏／银行存档。

每条恰好三项且不得重复：`enabled=0/1;`、`action=reject/require;`、
`condition=表达式;`。段名无分号，字段以引号外的 `;` 结束。条件可换行、缩进、加空白。
`#` 注释到当前物理行末，引号内的 `#`／`;` 是正文；不支持块注释、反斜线续行或引号内直接换行。

每个 `[rule.xxx]` 独立校验。任一字段非法、重复、缺少必要字段、表达式语法错误或
缺少结束分号时，整组按 `enabled=0;` 处理，不采用不完整的条件，并继续读取下一组。
同名段涉及的规则均关闭；关闭只作用于内存，不改写玩家文件。正常不满足某只宝可梦的条件
不属于配置损坏，不会关闭规则。实际读写失败、内存不足仍作为加载错误处理。

```ini
[global]
enableCheckPokemon=true;

[rule.025_custom_example]
enabled=0;
action=require;
condition=
    species == 25 &&
    level >= 5 &&
    (ot == "Example" || ot == "Other");
```

此关闭的自定义示例只解释语法，不代表官方限制。
删除规则或 `enabled=0;` 停用。目录决定来源，没有 source、group、blocked_species 或 action=allow 字段。

## 表达式

`&&` 为全部满足，`||` 为满足其一，AND 优先于 OR，支持嵌套括号。
比较须完整写，如 `species == 25 || species == 26`，不能写
`species == 25 || 26` 或 `species=25,26`。支持 `== != < <= > >=`；
不支持算术、赋值表达式、函数、变量、单个 `&/|` 或前缀 `!`。
数字为十进制非负 uint32，溢出拒绝，不截断。

| 字段 | 作用 |
| --- | --- |
| `species` | 全国图鉴编号；多个种族用完整比较加 OR。 |
| `form` | 原生观察到的形态编号。 |
| `level` / `met_level` | 当前等级／记录的相遇或迁移等级。 |
| `shiny` / `fateful` | 闪光／命运相遇标志，true/false 或 1/0。 |
| `tid` / `sid` | 原生训练家 ID／隐藏 ID。 |
| `language` / `origin` | 宝可梦原生语言／原始游戏，不是界面语言或当前所选游戏。 |
| `move` | == 表示四槽任一个含该招式，!= 表示四槽均不含；不支持其他比较。 |
| `duplicate_moves` | 非零招式是否重复，多个空槽不算重复。 |
| `UnicodeChinese` | 昵称或 OT 是否含 CJK 汉字，true/false 或 1/0；与姓名白名单独立。 |
| `ot` | 双引号 UTF-8，加载为原生 UTF-16 精确比较；仅 ==/!=，区分大小写、空格及字形。双引号／反斜线用反斜线转义。 |

OT 受原生十二个 UTF-16 单元容量限制。NDS 读取 PK5 字段；VC 读取原版转换后的 PK7 候选字段。
语言编号共用，原始游戏和形态编号的来源差异分别列在下方。

### 规则判定

`reject`：完整表达式为真即拒绝。`require`：从本条种族比较确定适用对象，
保留 AND／OR／括号，其他比较暂视为“不限制范围”，再要求适用对象满足完整条件。
某个 OR 分支没有种族限制时，该分支范围是全部种族；完全不含种族比较的 require 也作用于全部种族。

各规则独立：任一 reject 命中或适用 require 不满足就拒绝，不跨规则拼接条件，
也不能覆盖结构／校验、有效空槽分流及原版十四项本地检查。
姓名修正属于独立 [词表规则](readme_checkName.zh-cn.md)，不属于种族拒收规则。

### UnicodeChinese 与默认汉字拒绝

`UnicodeChinese` 为布尔观察值：昵称或 OT 含下列任一 CJK 汉字区块即为 true，
不依赖语言编号、昵称标记或姓名白名单。默认规则为：

```ini
[rule.BanUnicodeChinese]
enabled=1;
action=reject;
condition=
    UnicodeChinese == true;
```

命中返回原生逐槽拒绝值 1，整只宝可梦被排除，不调用姓名修正；NGWord 命中则独立使用
NDS 250–252／VC 253–255。Gen5 默认拒绝有用户实机观察依据，VC 同样拒绝是本地自定义
策略，并非复原官方完整服务器规则。账户开启 [汉化许可](../../users/readme_user.zh-cn.md#allowfantranslationchn)
时，仅在当前用户内存中关闭此规则，共享文件不变；未开启的账户仍按共享 enabled 值读取。

范围依据 [Unicode 17 区块定义](https://www.unicode.org/Public/17.0.0/ucd/Blocks.txt)，包含区块保留码位；
不把假名、韩文、私用区、标点或部首区整体当作汉字。补充平面按有界 UTF-16 代理对读取。

| 范围 | 内容 |
| --- | --- |
| U+3400–4DBF、U+4E00–9FFF | 扩展 A、基本汉字 |
| U+F900–FAFF、U+2F800–2FA1F | 兼容汉字及补充 |
| U+20000–2A6DF、U+2A700–2B73F | 扩展 B、C |
| U+2B740–2B81F、U+2B820–2CEAF | 扩展 D、E |
| U+2CEB0–2EBEF、U+2EBF0–2EE5F | 扩展 F、I |
| U+30000–3134F、U+31350–323AF、U+323B0–3347F | 扩展 G、H、J |

### language 数值

这是十进制 PKM 语言编号，不是 Bank／Mover 界面语言、国家、CTR 主机区域或词表区域后缀。
例如写 `language == 2`；不支持 `language == ENG` 这样的文字别名。

| 数值 | 语言 | 与当前输入的关系 |
| --- | --- | --- |
| 0 | 未设置 | 不应自动判为违规；部分日文五代游戏内交换个体保留 0。 |
| 1 | 日语 | 原来源游戏语言。 |
| 2 | 英语 | 美国／英国／澳洲英语共用，不区分主机区域。 |
| 3 | 法语 | 原来源游戏语言。 |
| 4 | 意大利语 | 原来源游戏语言。 |
| 5 | 德语 | 原来源游戏语言。 |
| 6 | 未使用／保留 | 不是韩语。 |
| 7 | 西班牙语 | 原来源游戏语言。 |
| 8 | 韩语 | VC 韩文版来自《金／银》，不包含一代／《水晶》。 |
| 9 | 简体中文 | 后世代新增，不是原五代／VC 来源游戏语言。 |
| 10 | 繁体中文 | 后世代新增，不是原五代／VC 来源游戏语言。 |
| 11 | 拉美西班牙语 | 晚于 v1.5 对应来源游戏。 |

新增后世代姓名白名单不会给宝可梦赋予 9–11 的语言编号，也不会让非原生语言记录变合法。
字段只观察、不修补。编号依据 [PKHeX LanguageID][language-ids]，补丁原样读取 PK5
或原版转换后的 PK7 语言字段，不重新映射。

### origin 数值

#### NDS

这是 PKM 内保存的原始游戏编号，不是当前选中的 NDS 游戏或本项目客户端游戏编号。
从《黑》读出的宝可梦仍可能保留 `origin == 11`（珍珠）；尤其注意白为 20、黑为 21。

| 数值 | 原始游戏 |
| --- | --- |
| 0 | 未设置／未知，不是通配符 |
| 1 | 蓝宝石 |
| 2 | 红宝石 |
| 3 | 绿宝石 |
| 4 | 火红 |
| 5 | 叶绿 |
| 7 | 心金 |
| 8 | 魂银 |
| 10 | 钻石 |
| 11 | 珍珠 |
| 12 | 白金 |
| 15 | 宝可梦竞技场／XD（共用编号） |
| 16 | 宝可梦对战革命（记录编号，不代表一定可作为合法来源传送） |
| 20 | 白 |
| 21 | 黑 |
| 22 | 白２ |
| 23 | 黑２ |

空缺编号不等于其他游戏；后世代编号也不是正常五代来源。
参考 [GameVersion 的存储值部分][game-ids]，不要使用其后半段仅供编辑器分组的枚举。

#### VC

VC 筛选观察原 Mover 已转换出的 PK7 候选，不是 GB／GBC 原始头部字节；
这里的 VC 来源编号与 NDS／3DS 游戏选择菜单编号不同。

| 数值 | VC 原始游戏 |
| --- | --- |
| 35 | 红 |
| 36 | 绿（日版）／蓝（国际版） |
| 37 | 蓝（日版） |
| 38 | 黄 |
| 39 | 金 |
| 40 | 银 |
| 41 | 水晶 |

例如 `origin == 41` 表示转换后候选的来源为水晶，不等于“当前从水晶存档读出的所有个体”。
不能用 1／2 表示 VC 红／蓝，也不能因为候选是 PK7 就填 30／31（太阳／月亮）。
未设置的 0 不是通配符。依据见 [GameVersion 的 VC 存储编号][game-ids]。

### form 数值

#### NDS

form 没有一套全种族通用枚举，必须结合 species 使用。性别外观差异、闪光配色、晃晃斑斑点
不属于这个形态字段；单一形态的种族通常为 0。

以下是常用五代编号，仅解释数值，不是传送白名单；战斗临时、依赖道具及合体形态
仍受原版检查。这里不重新列出没有额外形态的全部种族。

| 种族 | 形态编号 |
| --- | --- |
| 201 未知图腾 | 0–25 为 A–Z；26 为 !；27 为 ? |
| 351 飘浮泡泡 | 0 一般；1 太阳；2 雨水；3 雪云 |
| 386 代欧奇希斯 | 0 普通；1 攻击；2 防御；3 速度 |
| 412 结草儿／413 结草贵妇 | 0 草木蓑衣；1 砂土蓑衣；2 垃圾蓑衣 |
| 421 樱花儿 | 0 阴天；1 晴天 |
| 422 无壳海兔／423 海兔兽 | 0 西海；1 东海 |
| 479 洛托姆 | 0 普通；1 加热；2 清洗；3 结冰；4 旋转；5 切割 |
| 487 骑拉帝纳 | 0 别种；1 起源 |
| 492 谢米 | 0 陆上；1 天空 |
| 493 阿尔宙斯 | 0 一般；1 格斗；2 飞行；3 毒；4 地面；5 岩石；6 虫；7 幽灵；8 钢；9 火；10 水；11 草；12 电；13 超能力；14 冰；15 龙；16 恶 |
| 550 野蛮鲈鱼 | 0 红条纹；1 蓝条纹 |
| 555 达摩狒狒 | 0 普通；1 达摩 |
| 585 四季鹿／586 萌芽鹿 | 0 春；1 夏；2 秋；3 冬 |
| 641 龙卷云／642 雷电云／645 土地云 | 0 化身；1 灵兽 |
| 646 酋雷姆 | 0 普通；1 焰白；2 暗黑（合体形态不可传送） |
| 647 凯路迪欧 | 0 平常；1 觉悟 |
| 648 美洛耶塔 | 0 歌声；1 舞步 |
| 649 盖诺赛克特 | 0 无卡带；1 水流；2 闪电；3 火焰；4 冰冻 |

例如 `species == 479 && form == 2` 选择清洗洛托姆，不是所有 form 为 2 的宝可梦。
依据 [FormConverter][form-ids] 的五代上下文；后世代妖精阿尔宙斯、白条纹野蛮鲈鱼、
地区／超级进化等编号不能套入五代。

#### VC

这里是原版 VC→PK7 转换后给出的形态，不是随意取 GB／GBC 某个字节。
绝大多数 VC 种族为 0；未知图腾（201）以 0–25 对应 A–Z，一／二代没有 !／?（26／27）。

例如 `species == 201 && form == 1` 表示未知图腾 B。性别外观、闪光配色、晃晃斑斑点
不属于形态选择；后世代阿罗拉／其他地区形态、超级进化、心金魂银刺刺耳皮丘不是 VC 输入形态。
原生转换的已知特殊行为仍交给原程序，不在这里新增形态合法性表。
依据见 [FormConverter 的未知图腾／二代处理][form-ids]。

## 加载和异常

一次相关请求加载／编译一次，逐槽只访问内存，不重复读 SD；下次请求采用当前文本，
不重算已返回结果。规则数、表达式长度和解析深度按内容动态增长，受可用堆及 32 位检查约束，
不是把配置当作 C 代码执行，也不生成二进制配置镜像。

缺分号／字段、重复字段、括号错误或不支持语法时，整份本来源附加表撤销，不采用半份规则；
这与词表“只跳过坏单条”不同。真实 I/O／内存不足仍为资源错误。
没有附加规则时，原版本地检查依旧运行。

## 默认规则及证据

条件由本项目独立组织。社区拒收记录不等于编辑器合法性判定，也不证明完整官方服务器算法。
[任天堂版本说明][mover-version]记录 v1.5 发布于 2020-02-11，联网要求更新；更早实验
属于历史证据，后续记录只支持 v1.5 时代，不证明服务端规则从未变化。
单一来源、本地推定或未隔离变量的情况均明确标注，不包装成受控复测。

### NDS 规则

三条规则默认开启。名称前后的编号表示组合中最小／最大种族，不表示中间所有种族都受限。

| 规则段 | 条件 | 依据及边界 |
| --- | --- | --- |
| `BanUnicodeChinese` | 昵称或 OT 含 Unicode 汉字时整只拒绝，默认启用。 | 用户 Gen5 实机观察；VC 同策略为本地自定义，见上方范围和汉化许可。 |
| `251_648_shiny_locked` | 时拉比、阿尔宙斯、比克提尼、凯路迪欧、美洛耶塔为闪光时拒收。 | [历史汇总][pp-bans]、[2016 样本][pp-sample]、[2021 阿尔宙斯讨论][arceus-case]、[时拉比反馈][nds-celebi]。社区样本存在其他变量，不是完整 v1.5 受控表。 |
| `491_darkrai_pearl_fateful0` | 达克莱伊且 `origin == 11 && fateful == false` 时拒收。 | [珍珠拒收个案][gcl-void]及 [2023 讨论][pp-pearl]；字段组合是本地推定，未证明为单独隔离的服务器条件。 |
| `492_shaymin_dp_fateful0` | 谢米且 `(origin == 10 \|\| origin == 11) && fateful == false` 时拒收。 | [2023 讨论][pp-pearl]同时修改多个字段，精确组合属推定，缺单变量复测。 |

Project Pokémon 部分帖子直读曾返回 403，依据可读索引正文；编辑建议不是官方算法。[Glitch City 归档][gcl-void]记录珍珠达克莱伊升至 50 仍失败。这些规则不识别具体漏洞，也不证明命运相遇标志为真就一定通过。

默认不全禁闪光莱希拉姆／捷克罗姆、非闪光虚空阿尔宙斯、仅凭语言判断的梦幻、所有活动个体或所有秘传招式。相关反例／分歧见[阿尔宙斯][arceus-conflict]、[英文闪光梦幻][english-mew]、[秘传招式迁移阶段区别][hm-distinction]；VC 限制不套到 NDS。

其他反馈不自动变成限制：[努力值总和][ev-research]还须确认原值／转换后的观察阶段；[大葱鸭名称格式][farfetch-name]是来源名称一致性个案，不是昵称一刀切禁令。[愤怒漏洞招式][rage-moves]、[烈空坐拒收提问][nobunaga-case]及[编辑器 PP 警告][pp-overflow]不构成完整逐种招式表。HOME 的交易／OT 限制属于另一服务，不移植为本预设。反例和不确定反馈应促使收窄规则，不是新增客户端改写的依据。

### VC 规则

六条预设中四条开启、两条明确关闭。把关闭候选改成 1，表示主动采用旧观察，不表示已验证当前 v1.5 限制。

| 规则段 | 条件／默认状态 | 依据及边界 |
| --- | --- | --- |
| `BanUnicodeChinese` | 昵称或 OT 含 Unicode 汉字时整只拒绝，默认启用。 | 用户 Gen5 实机观察；VC 同策略为本地自定义，见上方范围和汉化许可。 |
| `025_240_dizzy_punch` | 开启：列出的十九种奇异蛋家族成员任一招式为迷昏拳（146）即拒收。 | [合法性整理][bp-legality]、[水晶奇异蛋数据][odd-eggs]、[进化关系][odd-family]、[快拳郎个案][odd-case]、[电击怪个案][odd-recent]及[招式研究][odd-research]。十九种范围由闭包推导，不是逐种受控拒收实验。 |
| `115_duplicate_moves` | 关闭：袋兽有重复的非零招式则拒收。 | [2017 两个水枪对照实验][vc-duplicate]；单一历史实验，其他重复招式属推定，无 v1.5 受控复测。 |
| `144_146_min_level50` | 关闭：急冻鸟／闪电鸟／火焰鸟当前等级须至少 50。 | [2017 研究][vc-floor-history]含闪电鸟练级后通过的反馈，缺三种分别的现代边界样本。 |
| `150_mewtwo_min_level70` | 开启：超梦当前等级须至少 70。 | [2020 五级个体提问及升至七十的回复][mewtwo-2020]、[2022 低等级反馈][mewtwo-2022]和[早期研究][vc-floor-history]；多社区来源，不是单变量服务器代码实证。 |
| `151_mew_ot_tid22796` | 开启：须 `tid == 22796 && (ot == "GF" \|\| ot == "ゲーフリ")`，不额外禁闪光。 | [合法性整理][bp-legality]及[配信／传送研究][mew-report]。 |
| `251_celebi_min_level30` | 开启：时拉比当前等级至少 30，不查相遇等级，不全禁闪光。 | [合法性整理][bp-legality]及[社区答复][celebi-level]；缺 29／30 受控边界样本，辅助答复曾仅索引正文可读。 |

迷昏拳范围为 25,26,35,36,39,40,106,107,124,125,126,172,173,174,236,237,238,239,240。七种幼体及其一／二代进化闭包只能证明家族范围，不单独证明拒收；袋兽、图图犬不在该条范围内。公开水晶数据只是格式／数据资料，不是任天堂服务器源码。

不新增所有闪光梦幻／时拉比、超过 100 级、VC 252–255 的一刀切禁令；保留原版转换及本地检查。[聚宝功超梦更正][payday-fixed]是对旧拒收说法的反例，不新增封禁。梦幻除配置字段外不再要求独立配信来源证明；其他迁移阶段的秘传招式限制和 HOME 交易限制不套到这里。

[arceus-case]: https://www.reddit.com/r/ShinyPokemon/comments/o435ib/
[arceus-conflict]: https://projectpokemon.org/home/forums/topic/39815-unreleased-prototype-pokemon/page/30/
[english-mew]: https://www.reddit.com/r/pokemonrng/comments/1atxe96/
[ev-research]: https://bnossu.hatenablog.com/entry/2025/07/20/191222
[farfetch-name]: https://github.com/kwsch/PKHeX/issues/4066
[gcl-void]: https://archives.glitchcity.info/forums/board-110/thread-7845/page-1.html
[hm-distinction]: https://www.reddit.com/r/pokemon/comments/r8z7ox/
[mover-version]: https://en-americas-support.nintendo.com/app/answers/detail/a_id/25447
[language-ids]: https://raw.githubusercontent.com/kwsch/PKHeX/641c26ba100225f3605e5a44b61b3b16e15a3774/PKHeX.Core/Game/Enums/LanguageID.cs
[game-ids]: https://raw.githubusercontent.com/kwsch/PKHeX/641c26ba100225f3605e5a44b61b3b16e15a3774/PKHeX.Core/Game/Enums/GameVersion.cs
[form-ids]: https://raw.githubusercontent.com/kwsch/PKHeX/641c26ba100225f3605e5a44b61b3b16e15a3774/PKHeX.Core/PKM/Util/Conversion/FormConverter.cs
[nds-celebi]: https://www.reddit.com/r/pokemonrng/comments/18rjkwp/
[nobunaga-case]: https://detail.chiebukuro.yahoo.co.jp/qa/question_detail/q13293642568
[pp-bans]: https://projectpokemon.org/home/forums/topic/29147-pokebank-banned-pokemon/
[pp-overflow]: https://projectpokemon.org/home/forums/topic/55864-pp-show-up-incorrectly-in-pkhex-but-not-in-the-game-on-one-pokemon/
[pp-pearl]: https://projectpokemon.org/home/forums/topic/63625-unable-to-match-an-encounter-from-origin-game-darkrai-shaymin/
[pp-sample]: https://projectpokemon.org/home/forums/topic/38312-whats-wrong-with-these-pkms-rejected-by-poke-transporter/
[rage-moves]: https://bulbapedia.bulbagarden.net/wiki/Rage_glitch
[bp-legality]: https://bulbapedia.bulbagarden.net/wiki/Pok%C3%A9_Transporter#Legality_checking
[celebi-level]: https://www.reddit.com/r/pokemoncrystal/comments/1i7jvmr/
[mew-report]: https://projectpokemon.org/home/forums/topic/39235-vc-rby-virtual-console-mew-uk/page/5/
[mewtwo-2020]: https://www.reddit.com/r/pokemon/comments/fp34py/
[mewtwo-2022]: https://medaka.5ch.io/test/read.cgi/poke/1646476301
[odd-case]: https://www.reddit.com/r/pokemon/comments/f9akpf/
[odd-eggs]: https://raw.githubusercontent.com/pret/pokecrystal/master/data/events/odd_eggs.asm
[odd-family]: https://raw.githubusercontent.com/PokeAPI/pokeapi/81d20919845b6a445969b9265aab5cec6b04cece/data/v2/csv/pokemon_species.csv
[odd-recent]: https://www.reddit.com/r/pokemoncrystal/comments/1vu7i6k/poke_transporter/
[odd-research]: https://www.smogon.com/forums/threads/comprehensive-list-of-moves-exclusive-to-the-virtual-console-games.3577468/post-10611983
[payday-fixed]: https://www.smogon.com/forums/threads/bug-reports-v2-0-read-op-before-posting.3469932/page-401
[vc-duplicate]: https://archives.glitchcity.info/forums/board-107/thread-7799/page-1.html
[vc-floor-history]: https://archives.glitchcity.info/forums/board-107/thread-7799/page-3.html
