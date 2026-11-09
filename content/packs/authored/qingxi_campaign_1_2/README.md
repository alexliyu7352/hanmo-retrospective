# 清溪粮道 1.2：原北营正规训练入口

本版本仅在原1.1完整作者上开放北营本人部队的普通训练。保留原军营/角色/任职/钱包/库存/人口/军令/表现资源，不新增人、钱、粮；1.1原件不改。

- 原 garrison.north 的 training_wallet_id 明确绑定已存在、同组织的 wallet.north.service；原 wallet.north.public 为付款军库。
- 本人 unit.north.work_detail 显式 initial_training=10、initial_fatigue=0。
- 北营 training_fatigue_per_tick=4 是此版本作者参数，沿原普通训练Kernel样本；不是全军默认或GDD固定军规。
- 原训练规则仍为4半日、总150（37/38/37/38），真实口粮由原军旅供给只扣一次；正常增量为3+本人相应兵种技能级。当前作者玩家步兵XP100，等级由既有成长规则读取，不在作者或见闻消费者另算奖励。

正常流程为 qingxi::actor.player 从 north.gate 进入 north.camp，经真实 role.north.officer 接受空缺 office.north_leader、原半日完成任命，再提交 military_train(unit.north.work_detail)。开始与逐课仍查实际军职、地点、参与者/单位占用、军库预留和同一口粮来源；没有预造任职、课程或见闻事实。

不为另一 north.blockade 部猜补训练profile，不为南营NPC部队改军令或取消义务。新见闻场景可显式依赖 qingxi.campaign 1.2.0，由成长owner新增其profile与场景；训练账和机制仍只由S2产生。

本次最小作者已经原正式compose验证为STRUCTURE_VALID、warnings为空；Native四课和Main尚未运行，旧GD训练Kernel门不能冒充本版本已运行。所有资源字节/许可沿原1.1清单。
