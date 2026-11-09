# 青川与松门 · 春粮往来 1.6.0

正式后继包 `hanmo.local_society@1.6.0`，精确选择 `content/packs/selections/local-society-1.6.0.json`。唯一正常章节 `scenario.qingchuan_spring_reserve`；compose后为 `society::scenario.qingchuan_spring_reserve`。从源 `84dc63f4` 的1.5继承原人物、款物、生活计划和逐课结算；1.5及更早作者包、旧银行和工坊原件不回写。

## 同一地方中的开始、选择与回访

程禾从青川街口 `place.courtyard` 回到旧宅附近。县仓书佐说明储备补足事务，可以先核对公托、去粮行问工或去讲舍问课。院前普通对白只介绍已声明的地方关系；动态公托仍由 `topic.city.grain` 与原货单实际生成，介绍不等于承接。

青川讲舍 `place.school` 的陶慎沿原礼法课程、`per_lesson_v1` 逐课结算；粮行 `place.grain` 的杜衡沿原买卖和半日短工。公托的期限、课程和出行都用同一本世界时钟。玩家可在手记安排先后，执行仍须满足原占用、资金、报价和确认条件。

从院前沿原道路到松门山门 `place.sword_gate`，知客办理另一组织的储备补足公托；原通路通向孟修所在 `place.sword_court`，可正常工作、休息、授业与修业。本章只有这五个实际场所，不把北驿、军籍或战斗塞进主经历。

玩家可以亲自履约，也可以先修业或做工，接受窗口和库存因此产生不同结果。已经接下的责任不会因看对白、安排未来日程或离开页面而消失。回到原经办人处，结果话题核对本人原合同的实交、部分验收、逾期、取消和清算；回访本身不交货、不发钱、不免除剩余责任。

## 明确初态与原责任

以下均为 provisional 作者参数，来源为1.5正式初态及已确认1.6章节合同，不是最终平衡承诺：

- 程禾私人钱包240钱、随身2粟；旧宅 `estate.apprentice_home` 私库40粟、独立钱柜0钱，成年管事程叔仍实际在宅。原庄园消费继续，家中没有新增生产。
- 粮行40粟、900钱；县仓120粟、组织总账200钱；松门60粟、组织总账200钱；县务与松门工资账各240钱。所有初始owner/库存来源沿1.5，没有剧情补款、补货或预置成功。
- 县仓目标保持124，松门只把本章储备目标从10改为64。两处各四粟的初始缺口是两个组织独立Need，共享同一粮行的实际货，不是复制一单，也不表示饥荒。
- `template.city_grain`／`policy.city_need` 仍为4半日期限，`template.sect_grain`／`policy.sect_need` 仍为24半日。每份最多4粟、原80钱采购预算与8钱全量报酬按实际承接和冻结条款结算，不从对白另领。
- 粮行原短工0费用、30钱工资、1半日、疲劳+8；课程仍由当时报价和逐课记录决定成本与所得。孟修工作工资来自 `wallet.sect_wages`，不是其私人钱袋。
- 具名人物不计入匿名cohort人口；原家户生产、口粮与工资责任保持。无新增饥饿、强制掉粮、演员或免费替班。

谷诚仍是已有 `actor.study_caravanner`，初态钱粮沿1.5足备局；本章策略使用原合法个人承包模板和公开 `supply.city`／`source.market`，可在县仓实际承接、采购、交付，也可能受期限、提供人占用或存货所阻。组织ID只界定这项政策，不把他假定为已入职成员。沈蘅保留原私人物资准备与回松门行程；两人的结果不预先写进剧情。

县仓书佐恢复已有 `recruitment_posts`，向程禾当面提出 `terms.city_contract` 原条款。邀请、接受／拒绝／暂缓、真实工资和合法退出均走原规则。原工资率为每2半日5/2钱，保留有理余数和终止补偿合同。当前没有NPC自动组织派单，因此入职不会凭空增加采购职责，更不生成“职责完成”奖励事实。

## 事实回访作者声明

`outcome_topics` 的最小闭合字段为 `id, role_id, task_template_id, dialogue_id, cooldown_ticks`：

- `topic.city.outcome` → `role.city_supply` / `template.city_grain` / `dialog.region_city_outcome`
- `topic.sect.outcome` → `role.sect_supply` / `template.sect_grain` / `dialog.region_sect_outcome`

两者冷却2半日，只允许say/choice/end。主题ID是内部身份；actor、role、template、dialogue等实体由compose正常加 `society::`。正文只引入核账语境；真实数量、结算和停止原因由本人实际Task/Contract、Delivery/Transfer及仍保留的原TaskEffect回执投影。

部分行动的 `TaskEffect.success=true` 不等于整单成功；只有实际交付数量可证明已交。逾期来自原合同 `execution_stopped/stop_reason` 与结算状态，没有交货就不制造交货回执。完成回执缓存已回收时保留可验证的耐久合同/交付来源，不伪造receipt。展示与选择复验精确结果轮次、人和地方；发生变化应重新打开，不能换成另一张公托。读取不消耗机会，关闭不取消责任，反复回访不重复结算。

## 表现与验证边界

原六项资源仍按原SHA沿用。成人十二代表完整元组与正式粮行分层资源由各视觉owner提供，中央串行合入共享与包manifest；选择只影响原AppearanceProfile，不改变角色身份、年龄、属性、私产或权限。除粮行外的场所继续原合法表现，不把酒肆/布铺/客舍改名冒充讲舍或松门。

本次作者/叶源码准备不宣称Main、GUI、原生运行或新进程存读已通过。中央组合后才以同一精确catalog/schema/资源/库/脚本身份证明正常选择→真实行动→两类经历→回访→菜单保存→恢复继续。原1.5历史边界不因新章被重新判成通过。
