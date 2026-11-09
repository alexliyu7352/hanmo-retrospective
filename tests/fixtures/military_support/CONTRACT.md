# 野战 BA08 / T44 协防片

基线：82c82d02306c9668a3c035c0d6f6c6a9e056c466。只扩既有六角同步 Round，不改变个人战、攻城、军团资产、补给或世界时钟。

原文：03_people_growth_cards.md 的 BA08 是“选择邻友，友军首受伤的20%由本部承担”；05_duel_and_warfare.md §5.8 要求一部一轮一个主动战法、分摊前后总损失守恒且不递归；T44 对应守恒门。

根任务确认的实施约定，不冒充原文：

- 一次声明明确选一个同侧相邻单位；支援者原地行动，同步移动后再核相邻
- 同一目标由多个支援者声明时，按稳定 unit ID 选择一个，绝不叠加；仲裁隔离在 `select_supporter`
- 只在本轮首个正数直接命中时按 2000 基点一次取整；取整为零也已消耗这次首伤机会
- 只转移支援者剩余可战人数能承担的减员，余量仍在原目标；转移伤害不会触发另一协防
- 命中与转移沿现有稳定攻击者 ID 顺序；无额外随机抽取。伤亡仍由原统一分类阶段处理

输入沿既有 `tactic` 命令：`target_id` 选邻友，`destination` 等于当前位置，`tactic_id` 引用兵种已有定义。定义仅增加 `kind: support`；仅 infantry。`point_cost` 使用合法作者值，20只在测试样本。其余固定为 `power_bp:10000, accuracy_bonus:0, morale_effect:0, move_bonus:0, status_rounds:1`；不接受借这些字段更改比例、移动或附加效果。

事件：攻击事件 `casualties` 是原目标实际减员；另记 `support` 的 `unit_id`（承担者）、`target_id`（原目标）、`attacker_id`、实际转移 `casualties` 和 `original_casualties`。相加等于未分摊原伤害。事件存入原 `last_events`，无新持久状态根、phase或编号前缀。

中央接线清单：在当前内容 Schema 的 troop_type.tactics.kind 加 support；Python 对应语义校验同步允许同约束；按统一军事模块版本与精确旧锁兼容策略发布。原 military_plan / Round 接口与 MilitaryContent 对 Round 校验的委托不变。正式内容、能力取得/兵种技能准入及表现入口由中央按后续范围接入；本片不冒称它们已验收。

自主选招沿既有AI评分：只读取轮初公开兵种、点数、位置/射界，估计邻友所受一次普通直接攻击的最大期望减员，再对可承担的20%使用既有“100基础分＋每名减员30分”；只有比原移动/攻击候选更高时选协防，撤退优先不变。同效战法选合法成本最低者、同成本按ID。它不读保存的敌我意图或玩家草案，不能保证敌人最终攻击被保护的友军。实际同步移动/攻击仍在结算时重验，AI不会预知未来目标。

局部入口：`source <本地路径>` 后以 `$HANMO_PYTHON tests/fixtures/military_support/run_support.py` 执行；`--adjacent` 可再运行原 military_round 门。所有日志、class 缓存、生成回执只在独立临时目录。


## 2026-10-05 玩家协防入口

正式 `MilitaryBattleScreen` 现在可以消费作者已经配置的 `kind: support`：选择己方可指挥部队和协防后，地图点击设保护目标；上方名册明确切换指挥对象，允许重新选择当前查看的友部。只读友部可被保护但不会获得一条玩家命令。取消战法回到原地防御，地图空格不改变协防的原地落点。

可保护目标与原地范围均通过既有 `Round.preview` / `movement_options(..., tactic_id)` 查询；敌军、自保、远方友部、离场和不足点数继续由同一规则拒绝。青色轮廓仅表示当前草案合法，预览明确共同移动后须仍相邻。提交走原军令入口；战报显示原目标实际减员及 `support` 事件中承担者的实际分摊，不重新算损失。

回归用例并入已有 `game/tests/test_military_battle_screen.gd`。协防选择和名册重选通过真实 PopupMenu 的 Enter 输入，战场与确认使用真实 Viewport 鼠标输入；本门仍沿既有军事 UI 夹具执行真实 Round，不冒称完整世界补给或存档验证。既有 `test_military_battle_presentation_input.gd` 保留非协防草案与旧绘图 oracle 的相邻覆盖。

正式清溪剧本未增加协防、费用或解锁条件。本次新增可玩的作者配置消费入口；BA08 的正式能力取得和剧本投放仍是未完成内容，不能把该夹具战役当成已投放。
