# Bank／Mover 共用功能

[English](README.md) · [项目首页](../README.zh-cn.md)

Bank 和 Mover 分别编译同一套本地服务与存储代码。客户端负责原状态机、界面、宝可梦转换和游戏存档；本地服务负责 SD 银行、配置、票务、礼物响应与事务。

本地服务不监听 HTTP，不建立官方账户会话，也不上传本地银行至官方服务器。

## 功能

- 按用户隔离银行、客户端记录、余额、购券和主机地区；共享政策与礼物素材。
- 在请求边界读取当前配置；已交付响应和提交／回滚决定不因之后的编辑重算。
- 后台处理银行读写及整盒筛选，使用自有数据副本；原客户端执行回调、改名和保存。
- NDS／VC 分别使用宝可梦规则与姓名词表；自定义转换默认关闭，开启也不绕过普通筛选。
- 礼物沿原银行／游戏记录判断领取状态，不另建领取账本。

## 配置

空模板位于 [configTemplate/templateForCopy/Bank](../configTemplate/templateForCopy/Bank)，完整演示位于 [configTemplate/examples/Bank](../configTemplate/examples/Bank)。游戏不读取 Markdown 说明。

- [client.conf](../configTemplate/templateForCopy/Bank/client/readme_client.zh-cn.md)：用户表、选择和迁移。
- [server.conf](../configTemplate/templateForCopy/Bank/fakeServers/shared/readme_server.zh-cn.md)：共享免费／试用期、价格、充值额与可选诊断日志。
- [user.conf](../configTemplate/templateForCopy/Bank/fakeServers/users/readme_user.zh-cn.md)：个人地区、币种、购券、试用、余额与自定义转换开关。
- [gift.conf](../configTemplate/templateForCopy/Bank/fakeServers/shared/gifts/readme_gift.zh-cn.md)：活动开关、日期与礼物素材。
- [checkPokemon.conf](../configTemplate/templateForCopy/Bank/fakeServers/shared/mover/readme_checkPokemon.zh-cn.md)：NDS／VC 宝可梦筛选。
- [checkName.conf](../configTemplate/templateForCopy/Bank/fakeServers/shared/mover/readme_checkName.zh-cn.md)：NDS／VC 姓名匹配与白名单。

## 数据保护与注意事项

银行、事务、客户端记录及来源存档使用各自的保护流程；普通配置的备份不能替代银行事务恢复。`bankdata.bak` 保留实际修改前的上一版，`transaction.bak` 是上一条日志决定；暂存文件不是独立的已提交数据。

Gen5 数字来源使用所选 `x.sav` 旁的 `x.bak/x.tmp`，确认保存及令牌清理完成后移除工作文件。无法判定的未决工作保留，不能凭一个摘要证明银行与游戏一致。

文件必须是独立普通文件，不支持符号链接、目录联接或硬链接。手动修改或迁移前请停止软件并备份，不要删除未完成事务的恢复依据。详情见[文件清单与保护策略](../configTemplate/templateForCopy/Bank/readme_fakeServer.zh-cn.md#写入文件与保护策略)。

## 技术文档

- [服务架构、接口与数据生命周期](docs/fakeServer/architecture.zh-cn.md)
- [字体覆盖与内存行为](docs/fonts.zh-cn.md)
- [Bank 实现说明](../bank/docs/code-analysis.zh-cn.md)
- [Mover 实现说明](../mover/docs/code-analysis.zh-cn.md)
