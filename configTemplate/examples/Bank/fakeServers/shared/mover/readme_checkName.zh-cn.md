# Mover 敏感词配置

[English](readme_checkName.md) · [宝可梦筛选](readme_checkPokemon.zh-cn.md) · [服务器配置](../readme_server.zh-cn.md)

NDS 与 VC 共用本说明中的词表语法，实际配置、开关、白名单及字符处理按来源分别读取。

| 来源 | 词表文件（相对本目录） |
| --- | --- |
| 第五世代 NDS | [nds/checkName.conf](nds/checkName.conf) |
| Virtual Console | [vc/checkName.conf](vc/checkName.conf) |

实际路径位于 `sd:/3ds/Bank/fakeServers/shared/mover/` 下。说明及本目录共用的
[许可证](LICENSE.badword.txt)均为外部资料，游戏不读取；移动说明不会合并两份运行词表。

## 文件组织

```ini
[global]
enableCheckName=true;

[ngword]
^example$

[allow_nickname]
ExactNickname

[allow_ot]
ExactOT
```

使用 UTF-8，可带 BOM，一行一个条目；上面只是格式示例，不是额外默认规则。
`enableCheckName=false` 时正常加载立即关闭文件，不读后续词表；缺失可选文件不增加姓名限制。
空行、纯空白行及以 `#`／`;` 开头的整行注释跳过。段名／注释可缩进，正文首尾空格则
保留参与匹配，不要为了排版缩进词条。支持 LF／CRLF；行首字面 `#`／`;` 要转义，
只匹配空格可写 `[ ]` 或 `\x20`。

禁词只有 `[ngword]` 一种格式。`[allow_nickname]`、`[allow_ot]` 是整个姓名字段的
精确允许值，区分大小写，不归一化、不裁剪空格、不解释正则或子串。昵称豁免不放行 OT
或宝可梦合法性，段落先后不影响优先级。实际原生姓名最多十二个 UTF-16 单元，超长值不会命中。

三类段均可加 `.JPN/.USA/.EUR/.AUS/.CHN/.KOR/.TWN`，如 `[allow_nickname.USA]`。
这里是 CTR 主机区域，不是语言、国家或货币。通用规则与匹配区域规则共同生效，主机区域
未知时不启用带后缀段。无法识别的段隔离到下个合法段；缺少／损坏全局开关不会擅自启用。

## 汉字与汉化支持

汉字整只拒绝属于 [checkPokemon](readme_checkPokemon.zh-cn.md#unicodechinese-与默认汉字拒绝)，汉化许可和 VC 转码表见共享 [server.conf](../readme_server.zh-cn.md#自定义选项)。
本文件只处理 NGWord 与姓名白名单；没有 `[rule]` 自定义开关段。

## NGWord 语法

| 内容 | 支持写法 |
| --- | --- |
| 文字／通配符 | 普通文字、转义标点、`.`（不匹配换行） |
| 分组／或 | `(...)`、`(?:...)`、`a\|b` |
| 字符集 | `[abc]`、`[a-z]`、`[^abc]`；范围两端必须是单字符 |
| 重复 | `?`、`*`、`+`、`{m}`、`{m,}`、`{m,n}`，可加惰性后缀 `?` |
| 字符转义 | `\xHH`、`\uHHHH`、`\t`、`\n`、`\v`、`\f`、`\r` |
| 字符类别 | 字符集内外的 `\d/\D/\w/\W/\s/\S` |
| 锚点 | `^`、`$`，最外侧采用下述 NGWord 词边界语义 |

`\d` 是 ASCII 数字，`\w` 是 ASCII 字母／数字／下划线，`\s` 按 ECMAScript 空白分类。
`[^...]` 是反集；`[\dA]` 合法，`[a-\d]` 不合法。重复次数是非负 uint32，结果只表示
是否命中，不输出捕获及贪婪／惰性位置。不支持回溯引用、环视、`\b`、Unicode 属性、
POSIX 命名类、正则选项或捕获输出。

同时检查原名、归一化名及去空格视图。归一化包含字母／数字宽度、BMP 小写、假名宽度／
平片／小假名、半角浊音组合，不是完整 NFKC，也不使用 Switch 相似字表。BMP 小写参考
[Unicode 数据][unicode]和[无条件特殊大小写][casing]，不采用语言／上下文特殊映射，
代理单元保留。

最外侧未转义 `^`／`$` 按原名的普通／全角空格词边界，约束整条规则及全部顶层分支。
`^ab|cd$` 匹配完整词 ab 或 cd，不匹配 abx／xcd；建议写 `^(ab|cd)$`。
括号内部锚点是当前整个名字视图的首尾。`^bad$` 是完整词示例，`.*bad.*` 是子串示例。

整条能匹配空串的表达式（`.*`、`()`、`a|`）跳过；非空整条内的可空子表达式允许，
例如 `a()b`、`(?:a|)b`。非法 UTF-8、内含 NUL 或不支持语法只跳过该条，前后合法条目
仍生效；真正 I/O／内存不足仍返回错误。不设固定条数、行长或嵌套上限，但受可用堆、
32 位表示和溢出检查约束，不表示物理内存无限。

## 默认内容及精确姓名豁免

两来源词表都采用 [abcboy101/badword 固定版本][badword]的十七份
`romfs/NgWord/18/{0..16}.txt`；[版本表][version]将 18 对应为 3DS 11.1.0-34。
仅转换 UTF-16→UTF-8、精确合并去重、增加配置段；4016 条表达式原文保留，包括大小写及
文字空格。采用所有语言并集，不按界面语言选择；不混入 v19+ Switch 词表／白名单／相似字表。

种族名精确白名单可自行增删，不是官方服务器白名单提取结果。按日、英、法、意、德、
西、拉美西语、韩、简中、繁中分组，组内按全国图鉴排序；同名只在最先出现组保存，
意／西／拉美西语组因此保留标题而不重复英文。规则说明中的名字不自动进入词表。
后续世代名称只是昵称豁免，不给原生存档增加新语言编号；汉字名仍须先启用上述汉字允许。

名字采用 [kwsch/PKHeX 固定版本资源][names]，与 [PokéAPI 固定名字表][pokeapi]
交叉核对九种独立语言表；拉美西语资源与西语逐字一致。CSV 中混用繁体的 `負电拍拍`
不采用，[官方负电拍拍图鉴][official-minun]确认其应为 `负电拍拍`。
按[世代名称][generation]、[NDS 编码][encoding-nds]／[VC 编码][encoding-vc]资料
处理来源范围、精确去重、世代大小写／标点、旧名及已核实的尼多兰／尼多朗存储字形。
两者是社区格式资料，不证明原服务器完整豁免策略。原来源语言参考任天堂的
[语言页面][languages]及[韩文手册][korean]；后续新增语言参考[中文公告][chinese]及
[官方拉美西语介绍][latin-spanish]。

仅补充官方游戏名称及官方确认的更名前后写法，不导入港台旧译或社区别名。
各词表编号范围内，按[2020 年 3 月公告][official-change-march]同时保留
`多边兽Ⅱ`／`多边兽２型`，以及适用时的 `多边兽Ｚ`／`多边兽乙型`；
按[10 月公告][official-change-october]保留 `死神棺`／`迭失棺`。
法／韩文历代官方游戏写法及已核实的存储字形仍保留；超出 251／649 范围的更名不加入。

### NDS

默认添加全国图鉴 001–649 的 5225 个完整种族名／字形值。Gen5 读取 PK5 原生 16 位姓名，存储雌雄符号可能不是普通 Unicode；世代大写、标点及 Farfetch'd 的直撇号等变体均显式列入，不自动放行任意变体。

NDS 补充：昵称精确允许 `omfg`、`bbwtzh`、`bbwten`；OT 精确允许 `せいしゃ`；禁词增加 `^adhd$`。不放行包含它们的长名字、其他大小写或另一姓名字段。姓名修正码 250／251／252 分别表示昵称／OT／两者。

### VC

默认添加全国图鉴 001–251 的 2134 个完整种族名／字形值。韩文范围来自《金／银》，不表示一代或《水晶》有韩文版。原 Mover 先把 VC 字符转为 PK7 候选，服务读取转换后的 UTF-16 字段，不是对原始 VC 字节补零；精确豁免应填写转换后的字形。保留 MR.MIME／M.MIME、法文旧格式 TETARTE、韩文旧名 수륙챙이／덩구리 及对应现名；不预置仅 v1.2 出现的 MR-MIME／FARFETCH D。依据见[名字／转码说明][censored]及[VC 转换器][encoding-vc]。

VC 的 USA 主机区域昵称豁免：`bbwjjp`, `bbwefr`, `bbwees`, `bbwpen`, `bbwpfr`, `bbwpes`, `bbwkko`, `bbwtzh`, `bbwten`, `adhd`。`bbween`、`con` 仍按禁词处理，不外推其他区域。另加 `[ㅘㅙㅚㅝㅞㅟㅢ괻읆]` 匹配资料列明的九个不支持韩文字符。姓名修正码 253／254／255 分别表示昵称／OT／两者。

兼容补充主要依据单一社区的[词表实测记录][word-tests]，不是完整官方 v1.5 名单；
[Transporter 姓名说明][censored]提供版本及字符背景。不得从一个区域的例外推导全区域
豁免，也不得扩展为所有假名无条件放行。

## 重载及返回结果

每次相关策略请求加载一次，逐槽／逐姓名只读内存，不按名字重复读取 SD。
后续请求采用修改后的文件，已返回结果不重算。非法条目只跳过该条；补丁不写词表或其配套文件，
不把文本镜像进另一个配置 bin。

普通 NGWord 昵称检查针对带昵称标记的记录；汉字限制不受该标记豁免，启用自定义汉化后
保留的无昵称标记汉字名也检查 NGWord。OT 始终检查；普通词表命中返回原版姓名修正结果，
Gen5 禁用汉字的字符检查则在此前直接返回逐槽拒绝值 `1`。
真正改名、提示及保存交给原接收器。首次使用时的姓名说明不表示词表命中。
筛选要求改名时，汉化保留不会恢复该字段的旧名字；在本文件 [global] 中设置
`enableCheckName=false;` 可关闭姓名筛选，不关闭汉化转换或 checkPokemon 筛选。
精确昵称豁免不设置／清除昵称标记，不改名，也不证明昵称等于实际种族。

## 来源与许可

NGWord：abcboy101/badword，固定提交
`1a76c29d53f2f6e98c23b6849215e99801c4224b`，GPL-3.0；来源路径、变换方式见上文。
种族名资源／格式资料：kwsch 及 PKHeX 贡献者，固定提交
`641c26ba100225f3605e5a44b61b3b16e15a3774`，[GPL 第三版][pkhex-license]。
完整未改动的 GPL 原文见本目录 [LICENSE.badword.txt](LICENSE.badword.txt)。
NDS／VC 合计另有十六条兼容记录及各自种族名豁免，原 NGWord 表达式不变；上游来源和
可编辑数据通过上述链接及附带文本供接收者获取。C 匹配器独立实现，未复制上游
TypeScript／Python、PKHeX 或任天堂原生过滤代码；公开参考不证明官方服务器行为完全相同。

PokéAPI 用于种族名二次核对，未引入其可执行代码；数据和名字仍受
各自权利人约束。以下保留必须附带的 BSD-3-Clause 英文原文；其大意是允许按条件再分发、
须保留声明、不得借贡献者名义背书，并不提供担保，法律含义以原文为准。

```text
Copyright (c) © 2013–2023 Paul Hallett and PokéAPI contributors
(https://github.com/PokeAPI/pokeapi#contributing).
Pokémon and Pokémon character names are trademarks of Nintendo.
All rights reserved.

Redistribution and use in source and binary forms, with or without modification,
are permitted provided that the following conditions are met:
* Redistributions of source code must retain the above copyright notice,
  this list of conditions and the following disclaimer.
* Redistributions in binary form must reproduce the above copyright notice,
  this list of conditions and the following disclaimer in the documentation
  and/or other materials provided with the distribution.
* Neither the name of PokéAPI nor the names of its contributors may be used
  to endorse or promote products derived from this software without specific
  prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
```

[badword]: https://github.com/abcboy101/badword/tree/1a76c29d53f2f6e98c23b6849215e99801c4224b
[version]: https://github.com/abcboy101/badword/blob/1a76c29d53f2f6e98c23b6849215e99801c4224b/web/index.html
[names]: https://github.com/kwsch/PKHeX/tree/641c26ba100225f3605e5a44b61b3b16e15a3774/PKHeX.Core/Resources/text/other
[pokeapi]: https://github.com/PokeAPI/pokeapi/blob/81d20919845b6a445969b9265aab5cec6b04cece/data/v2/csv/pokemon_species_names.csv
[generation]: https://github.com/kwsch/PKHeX/blob/641c26ba100225f3605e5a44b61b3b16e15a3774/PKHeX.Core/PKM/Util/SpeciesName.cs
[encoding-nds]: https://github.com/kwsch/PKHeX/blob/641c26ba100225f3605e5a44b61b3b16e15a3774/PKHeX.Core/PKM/Strings/StringConverter5.cs
[languages]: https://www.nintendo.com/nl-nl/Games/Nintendo-3DS-games/Pokemon-Sun-1092368.html
[korean]: https://www.nintendo.com/kr/pdf/pkm_mov.pdf
[chinese]: https://www.nintendo.com/en-za/News/2016/February/New-Pokemon-games-announced-via-Pokemon-Direct-1088288.html
[word-tests]: https://bulbapedia.bulbagarden.net/wiki/Talk:Pok%C3%A9_Transporter#Word_filter_tests
[censored]: https://bulbapedia.bulbagarden.net/wiki/Pok%C3%A9_Transporter#Censored_words
[unicode]: https://www.unicode.org/Public/17.0.0/ucd/UnicodeData.txt
[casing]: https://www.unicode.org/Public/17.0.0/ucd/SpecialCasing.txt
[pkhex-license]: https://github.com/kwsch/PKHeX/blob/641c26ba100225f3605e5a44b61b3b16e15a3774/LICENSE
[official-change-march]: https://web.archive.org/web/20200505203356/https://cn.portal-pokemon.com/topics/event/200323190120_post_19.html
[official-change-october]: https://web.archive.org/web/20201020125159/https://cn.portal-pokemon.com/topics/event/201020170000_post_21.html
[official-minun]: https://dex.pokemon.cn/play/pokedex/0312
[latin-spanish]: https://www.pokemon.com/el/videojuegos-pokemon/leyendas-pokemon-z-a
[encoding-vc]: https://github.com/kwsch/PKHeX/blob/641c26ba100225f3605e5a44b61b3b16e15a3774/PKHeX.Core/PKM/Strings/StringConverter12Transporter.cs
