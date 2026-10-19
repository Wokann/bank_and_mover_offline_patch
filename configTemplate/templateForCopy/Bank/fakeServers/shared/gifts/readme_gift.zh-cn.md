# 礼物配置

[English](readme_gift.md) · [服务器配置](../readme_server.zh-cn.md)

编辑 `sd:/3ds/Bank/fakeServers/shared/gifts/gift.conf`，模板见 [gift.conf](gift.conf)。
配置由玩家手动修改，补丁只读，不写暂存或备份；本说明不由游戏读取。使用 UTF-8，开头必须是
`[global]` 和 `enableGift=true/false`。关闭时立即结束正常加载，不解析后续活动、不打开素材。
文件缺失或全局开关损坏时不启用礼物。每个 `[gift.xxx]` 是独立的一组：任一字段非法、
重复、缺少必要字段，或日期、素材路径、世代与游戏范围相互矛盾，整组按 `enabled=0`
处理，继续读取下一组；不会只忽略坏条件后发放礼物。同名段或重复批次 ID 涉及的组均关闭。
上述关闭只作用于本次读取的内存结果，不改写配置。素材缺失／损坏不发放对应礼物，
不结束整个游戏；已交付正文不因后续编辑被撤回。实际读写失败、内存不足不冒充普通坏字段。

## 活动段

```ini
[global]
enableGift=true;

[gift.my_event]
enabled=1;
id=60000;
generation=7;
games=SM,USUM;
periods=2030-01-01..2030-12-31;
folder=gifts/my_event;
files=first.wc7,second.wc7;
```

使用此示例需自己提供两个文件，它不是额外附带的默认活动。
活动名接受 1–31 个小写 ASCII 字母、数字或下划线，可以自定义，不必带数字前缀，
也不要求与文件夹同名。

| 选项 | 作用 |
| --- | --- |
| `enabled=0/1` | 关闭／开启本活动，仍受总开关控制。 |
| `id` | 可选的 Bank 响应内容／批次编号，1–65535；PL6 必须指定。 |
| `generation=6/7` | 必须与素材及接收世代一致。 |
| `games` | `X/Y/OR/AS/S/M/US/UM`；支持组合别名 `XY/ORAS/SM/USUM`，逗号分隔。 |
| `periods` | 开放日期区间，包含起止日。 |
| `exclude` | 可选禁配区间，优先于开放日期。 |
| `file` | 单个 PL6、WC、Full WC 或受支持的 Bank 响应 `.bin` 文件。 |
| `folder` + `files` | 与 file 二选一：同文件夹最多六张 WC，逗号按发放顺序排列。 |

路径相对于 `fakeServers/`，不接受绝对路径或上级目录跳转。
`file` 不可与 `folder/files` 混用；PL6、响应 `.bin` 使用 file，不放入 WC 列表。
当前最多十六个活动、合计三十二条日期规则；每批最多六张卡。

## 日期和地区

使用 `YYYY-MM-DD..YYYY-MM-DD`，多个区间以逗号分隔，包含两端，按主机本地日历日期。
`periods`、`exclude` 都支持主机区域 `.JPN/.USA/.EUR/.AUS/.CHN/.KOR/.TWN`，
以及受支持的两字母国家后缀（如 `.HK`）。国家＞区域＞通用；匹配覆盖项替代通用区间，
不做并集，也不在今天超出区域区间时恢复通用日期。
`periods.JPN=disable` 可关闭该区域；删除覆盖项则恢复通用值。

```ini
periods=2030-01-01..2030-03-31,2030-07-01..2030-09-30;
periods.JPN=2030-02-01..2030-03-31;
exclude=2030-02-10..2030-02-12;
```

## 素材、编号和接收

原生 PL6 为 `0xA47` 字节；普通 WC6/WC7 为 `0x108`，Full 为 `0x310`，
分别采用 `.wc6/.wc7/.wc6full/.wc7full`。WC6 列表先组合成同一 Link 批次，
原生 PL6 和 WC 批次最终都由服务生成原 Bank 响应封装。修改 generation 不能把 WC7
转换为 WC6。支持宝可梦／道具卡；不支持的豆子／BP 或额外模板字段不会被静默丢弃。

`id` 是响应的内容／批次编号，不是事务 dataId／令牌，也不是最终写入游戏的卡片号；
名称前缀不决定编号。不同自定义批次应采用不同值，同内容的 WC6 和 PL6 不宜同时开启。

发放、重复判断、保存继续走原客户端流程，不另建领取账本。
若原接收器在其原生记录变化后允许重复领取，服务不额外封堵。
单独改日期不等于清除原客户端领取状态。

## 默认素材和来源

六批默认全部开启，但仍受日期、游戏及原接收器判断限制；附带三份 PL6、六份普通 WC7。

| 活动段／目录 | ID | 素材 | 通用日期；区域覆盖 |
| --- | --- | --- | --- |
| `01_celebi` | 1 | `celebi.pl6` | 2013-12-25..2014-09-30；USA 从 2014-02-05，EUR/AUS 从 2014-02-04 |
| `02_johto_starters` | 2 | `johto_starters.pl6` | 2015-02-27..2015-11-30 |
| `03_regi_trio` | 3 | `regi_trio.pl6` | 2016-03-04..2016-10-31 |
| `04_mewnium_z` | 4 | `mewnium_z.wc7` | 2017-01-25..2017-10-02 |
| `05_alola_starters` | 5 | `decidueye.wc7,incineroar.wc7,primarina.wc7` | 2018-03-09..2018-10-31 |
| `06_ha_apes` | 6 | `oranguru.wc7,passimian.wc7` | 2019-03-28..2019-10-31；JPN 从 2019-03-29 |

素材来自 [Project Pokémon EventsGallery 固定版本][gallery]。
六代原样采用归档原生 PL6；七代采用 `Unreleased/Gen 7/Bank/` 中对应 Full 文件
`0x208` 起的普通 WC7，不把素材改造成虚构的 WC6。这是社区保存的输入素材，
不是完整官方网络响应。编号 1／2 是按发放先后推定的本地约定，尚无原 Bank 批次号实证；
3 有公开 `present_0`／Full 数据支持，4／5／6 来自 WC7 输入。

日期参考 [Serebii 银行活动归档][dates]，并交叉核对官方的[时拉比公告][celebi]、
[梦幻 Z 公告][mewnium]、[阿罗拉御三家公告][alola]、[日本猩猴礼物公告][apes]。
猩猴通用起日依赖二手归档，日本覆盖起日有官方公告支持，不能抹平地区差异。

暂停下载不能证明所有已安装用户都无法领取。[日本重开公告][reopen-jp]、
[港台重开公告][reopen-hk]说明的是下载开放，不是已证实的每日礼物禁配窗口。
默认不加入这种禁配；需要模拟已确认的停服时再自行添加 exclude。
格式交叉参考：[PL6][pl6]、[WC6][wc6]、[WC7][wc7]、[WC7 Full][wc7full]。

[gallery]: https://github.com/projectpokemon/EventsGallery/tree/154d81be88453f6f78ec1d6d86e85fe0f2f5c240
[dates]: https://www.serebii.net/bank/events.shtml
[celebi]: https://www.nintendo.com/hk/pressrelease/3ds_20140127_pokemonbank.html
[mewnium]: https://www.pokemon.co.jp/ex/pokemonbank/topics/170125_01.html
[alola]: https://www.pokemon.co.jp/ex/pokemonbank/topics/180309_01.html
[apes]: https://www.pokemon.co.jp/ex/pokemonbank/topics/190329_01.html
[reopen-jp]: https://support.nintendo.com/jp/information/2014/0122.html
[reopen-hk]: https://www.nintendo.com/hk/pressrelease/3ds_20140122_pokemonbank.html
[pl6]: https://raw.githubusercontent.com/kwsch/PKHeX/master/PKHeX.Core/MysteryGifts/PL6.cs
[wc6]: https://raw.githubusercontent.com/kwsch/PKHeX/master/PKHeX.Core/MysteryGifts/WC6.cs
[wc7]: https://raw.githubusercontent.com/kwsch/PKHeX/master/PKHeX.Core/MysteryGifts/WC7.cs
[wc7full]: https://raw.githubusercontent.com/kwsch/PKHeX/master/PKHeX.Core/MysteryGifts/WC7Full.cs

本文件及礼物素材由玩家手动维护，补丁严格只读，不生成或清理 gift.bak／gift.tmp，也不从它们恢复已删除的活动配置。
