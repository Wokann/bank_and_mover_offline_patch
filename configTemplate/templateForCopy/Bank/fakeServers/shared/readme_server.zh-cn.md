# 共享服务器配置

[English](readme_server.md)

基础配置分为三份，均可手动预先放入；缺失时补丁自动生成，坏条目逐项修复：

| 相对 sd:/3ds/Bank/ 的文件 | 作用 |
| --- | --- |
| `client/client.conf` | 用户表及用户选择，见[客户端说明](../../client/readme_client.zh-cn.md)。 |
| `fakeServers/shared/server.conf` | 共用免费／试用窗口、年券价格及充值额度。 |
| `fakeServers/users/<用户ID>/user.conf` | 个人主机地区、币种、购券／试用记录及各币种余额，见[个人说明](../users/readme_user.zh-cn.md)。 |

余额与购券记录保存在同一份 user.conf。礼物、Mover 配置／词表／素材须由玩家手动放入共享目录，缺失则不启用对应附加功能。

完整 Bank 文件树及空模板／演示的用途见[统一目录说明](../../readme_fakeServer.zh-cn.md)。

## 读取与修复

UTF-8 文本，赋值采用 `key=value;`，末尾必须写分号；段名、NGWord 正文和精确姓名条目不附加分号。基础配置读取时仍保留旧格式中没有终止分号的合法值，发布时统一规范。开关 true/false，金额、计数为非负十进制整数。合法值保留；缺失、空值、格式损坏恢复为内置默认值，归回所属段的标准顺序。重复键采用最后一个合法值，后面的坏值不擦除已有合法值；商店分量分别修复。规范化删除未知段／键及一般注释，自动配置只保留货币格式说明。

下次相关请求读取当时的文件，不追溯修改已交付响应；一次会话固定用户，更换用户需退出再进入。正常购买、充值不会改变共享价格和充值额度。

三份基础配置先写齐、关闭并回读验证暂存，再将旧主文件轮替为 .bak、发布新主文件；内容相同不写盘。正式文件缺失先读备份，两者都缺失才采用默认值；孤立 .tmp 不接纳。当前文本的合法值仍保留，坏条目逐项修复。真实 I/O 错误不能伪装成功或空银行。完整清单和边界见[文件保护说明](../../readme_fakeServer.zh-cn.md#写入文件与保护策略)。

## 共享 server.conf

票务政策放在 [ticket]，自定义选项放在末尾 [custom]，默认值及顺序见[模板](server.conf)。

### 免费和试用

`free_enable=false` 时忽略 free_periods 授权，不论日期内容是什么。默认免费窗口为 `2020-02-12..2020-03-12,2023-02-14..2050-12-31`。这是可编辑项目预设，不宣称每个边界均为官方历史服务日期。免费期不会顺延已存付费期限。

`trial_enable=false` 关闭试用，忽略领取窗口。trial_periods 是可领取三十天试用券的日期，而非领取后有效期；其他触发条件见个人 [user.conf 说明](../users/readme_user.zh-cn.md)中的 trial_claimed。

| 通用／区域 | 默认领取窗口 |
| --- | --- |
| 通用、JPN、KOR、TWN、HK | `2013-12-25..2014-02-21` |
| USA | `2014-02-05..2014-03-14` |
| EUR、AUS | `2014-02-04..2014-03-14` |

### 日期

免费、试用、购券均采用 `YYYY-MM-DD..YYYY-MM-DD`，逗号分隔多时段，两端包含，按主机本地日历。`0000-00-00..0000-00-00` 为无有效时段，不能用 enable/disable/true/false 代替日期。

无后缀为通用；支持 .JPN/.USA/.EUR/.AUS/.CHN/.KOR/.TWN 及实现已收录的国家后缀（如 .HK）。国家覆盖区域，区域覆盖通用；选中覆盖值替代通用，不因今天不在覆盖窗口而退回通用。

### 商店

`shop.$<币种>=<年券价格>,<单次充值额>`，两项非负整数；余额保存在个人 user.conf。

| 币种 | 默认价格、充值额 | 整数单位 |
| --- | --- | --- |
| $NONE | 807,807 | 无币种本地单位 |
| $JPY | 500,500 | 日元 |
| $USD | 499,499 | 美分 |
| $GBP | 449,449 | 便士 |
| $EUR | 499,499 | 欧分 |
| $KRW | 5000,5000 | 韩元 |
| $TWD | 150,150 | 新台币元 |
| $HKD | 4000,4000 | 港分 |
| $AUD | 650,650 | 澳分 |
| $NZD | 850,850 | 新西兰分 |
| $CAD | 699,699 | 加拿大分 |
| $CHF | 700,700 | 瑞士分 |
| $ZAR | 6000,6000 | 南非分 |

$NONE 的 807 为自定数值，代表 Bank 支持的最高图鉴编号；默认年券价格和单次充值额均为 807，个人余额从 0 开始。其他币种金额是项目历史价格预设，不计算税费和实时汇率。USD 499 代表 4.99 美元。币种选择在所选用户 user.conf 的 [ticket] 余额列表上方：autoCurrency=true 按该用户有效区域／国家计算并更新 currentCurrency；false 使用 currentCurrency 的手动值，允许与地区不一致，不按界面语言。手动 NONE 或自动未匹配都用 NONE 政策及个人 NONE 余额；$NONE 支持本地购买和充值。这些不是真实 eShop 资金。手动设置的合法价格／充值额（包括 0）保留，只有缺失或非法分量恢复默认值。

## 自定义选项

以下条目放在共享 `server.conf` 末尾，默认全部关闭，不是官方设置：

```ini
[custom]
enableMoverv1.2=false;
allowFanTranslationCHN=false;
```

### enableMoverv1.2

只影响离线第一世代 RGBY 的转换：保留原构造器按种族性别比例生成的随机性别，
不再按攻击 DV 覆盖；闪光条件改为攻击 DV=10、防御 DV 为 2/3/6/7/10/11/14/15，
速度与特殊 DV 都为 10。还使用旧日文／西文字符映射：例如西文 0xE8 句点变连字符、
0xE0 撇号变空格，以及旧表不支持的数字／重音字符。无性别及固定性别仍由原构造器处理。

关闭时保留 v1.5。金银／水晶、Gen5、在线模式不进入旧版包装；不会回退服务器筛选、
银行格式、事务、地区设置或整个软件。它只复现已核实的本地转换差异，不恢复历史服务器。
版本差异见 [Poké Transporter 转换说明](https://bulbapedia.bulbagarden.net/wiki/Pok%C3%A9_Transporter#From_Generation_I_and_II)；这是社区资料，不是官方完整算法规范。

### allowFanTranslationCHN

开启后，在加载所选来源的 `checkPokemon.conf` 前，若存在 `[rule.BanUnicodeChinese]`，
程序会实际将其 `enabled` 改为 `0;`。缺失／损坏的该字段补为 0，重复字段合并；其他字段、
条件、注释和其他规则保留。用 `checkPokemon.tmp` 写齐并回读验证，再将旧配置轮替到
`checkPokemon.bak` 后发布；已为 0 且无需修改时不写盘。正式文件缺失可读已发布备份，
不采用孤立暂存。没有文件或没有该规则时不凭空创建；关闭许可不自动重新启用，需手动改回。

- NDS：已有姓名是 UTF-16，无需转码；恢复中文昵称／OT，即使没有昵称标记也不让默认
  种族名覆盖中文，昵称标记本身不伪造。
- VC：原始英文字母表 1 使用美版《水晶》汉化表，韩文字母表 6 使用韩版《金银》汉化表。
  指实际字符串来源，不是界面语言、主机地区或外壳地区；基于韩版编码的国际版汉化仍用韩文表。
- 只修改生成候选的姓名，原存档不改。未知／截断双字节序列整段回退原转换，不交付半截名字。
  普通韩文可能与汉化码冲突，未汉化的韩文存档应关闭此项。
- 普通 NGWord、其他筛选、原校验／加密、回调及事务保持。该选项独立于姓名词表开关；
  关闭 checkName 不会关闭汉化。与旧版选项同时开启时，最后由明确启用的汉化表恢复中文名字。
  若筛选要求修改昵称／OT，汉化保留不会恢复该字段的旧名字。要保留中文且不做姓名筛选，
  在对应来源的 checkName.conf 的 [global] 中设置 enableCheckName=false;；其他宝可梦筛选不受影响。

转换选项随后台银行下载结果交给客户端；后续策略请求读取当时的检查／姓名表。
已生成的候选不会因中途改配置追溯转码，修改选项后应返回标题重新连接。

两个汉化表均只有各自项目这一个来源，不是官方编码规范，不保证其他改版或以后改表兼容：

- [TomJinW/pokecrystal_cn](https://github.com/TomJinW/pokecrystal_cn/tree/f487d44c0d6f75a48a4a4b975ca04bc3914b3f52)：
  固定 [charmap_cn.asm](https://github.com/TomJinW/pokecrystal_cn/blob/f487d44c0d6f75a48a4a4b975ca04bc3914b3f52/constants/charmap_cn.asm)，7615 个双字节映射。
- [TomJinW/PKMN_GSCHS](https://github.com/TomJinW/PKMN_GSCHS/tree/c3947b83c6c66a015103e1bc08bbca7f2a862d3b)：
  固定最终 [fontTBL.txt](https://github.com/TomJinW/PKMN_GSCHS/blob/c3947b83c6c66a015103e1bc08bbca7f2a862d3b/05_Font/fontTBL.txt)，2350 个映射。

不附带 ROM、上游补丁或字体图像；NGWord 的 GPL 文本不自动授予这些独立资料同样的许可。

## 个人配置

个人购券、试用及余额在各账户的 user.conf，见 [user.conf 说明](../users/readme_user.zh-cn.md)。

## 可选内容

[礼物](gifts/readme_gift.zh-cn.md)、[NDS 词表](mover/readme_checkName.zh-cn.md#nds)、[NDS 筛选](mover/readme_checkPokemon.zh-cn.md#nds-规则)、[VC 词表](mover/readme_checkName.zh-cn.md#vc)、[VC 筛选](mover/readme_checkPokemon.zh-cn.md#vc-规则)。可选政策损坏不授权重置银行或游戏事务；按各说明跳过坏条目或关闭对应功能。
