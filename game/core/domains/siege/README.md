# 攻城安全离城条款

本片将既有 `safe_departure` 从记录交涉文字接到实际军令返程。仍由 `military` 持有军队与资产，`SiegeWorld` 在 Kernel 候选内调用原服务。没有新世界时钟、军团、库存、规则注册或状态迁移。

`safe_departure.gd` 的 `quote(state, data, siege)` 只读当前尚未退出的守军，预核每军可用的城外友营后，按确定顺序求完整全队安置。路线用 `Campaign.return_destination`，公物、预留、容量与金额用正式归还共用的 `MilitaryAssets._return_assets`，实际可起行再经 `Travel.plan` 检查。同营多军的接纳重量与金额联合剪枝并完整回溯，不能因先军贪占大营而误拒后军。失败在任何公共交接之前返回。

自动求解预算为16支待离城军、32个候选友营、8192个搜索状态；这些不是全游戏军团或营地总量上限。超预算返回 `SiegeDeparturePlanningLimit` 和 `feasibility=undetermined`，保持全部原态，不伪报无容量。只缓存已穷尽的不可行状态；结果按军团 ID 输出。

纯城防规则新增内部 `safe_departure_request`，只带现有守军身份。`SiegeWorld.finish_events` 再核全部安置，之后在同一候选中完成原公共占领、解除守军城防节点引用、调用 `Campaign.apply(campaign_return)`，并将原 `journey_requests` 交给既有 `MilitaryCoordination`。人员名册、伤员、随军钱货均留在原军团，真实旅行后再通过原 `army_return` 卸装。

当前预检不是永久接纳容量预约。途中若友营产权、库存容量或路线变化，仍由原军令/到营规则重验；本片不承诺未来行程免战，也不扩展战争外交。尚未到真实出城入口的人沿原 `NotAtCityExit` 拒绝，不替人物瞬移到城门。

局部新门 `game/tests/test_siege_safe_departure.gd` 使用正式准备的作者输入和真实 Kernel，覆盖两军同营接纳、无友营/无路/汇总容量不足、第二军旅行开工后整候选回滚、公共与私人资产分离、在途完整 Codec 恢复、实际到达和卸装。两张既有 tiny PNG 仅满足内容合同；未进行 GUI 或视觉验收。

## 节点资格一致性

封锁与移动共用当前门区边、阻挡节点耐久和待回写会战检查。有效守军仍控制目标补给口时不能计作封锁；同轮守军进入目标后会在轮末复核。因此不能隔完好城门封锁其后的口，也不能靠旧预览维持已失效的断粮累计。

世界观察中的内城退出/溃退判定复用城防纯层的有效守点谓词：有兵员、可用、士气为正。会战占用本身不让守军消失；零士气的边界投影与目标判定一致。仍须满足原连续守点轮数，目标达成只请求原公共交接，不删除部队。

这部分由 `test_siege_node_eligibility.gd` 单独验证。士气零值的世界投影采用明确的只读边界副本，未将该副本提交或恢复为真实世界，也不作为自然战斗链证明。

## 显式攻具实施约定

正式设计支持合法材料、真实制造时间和实物攻具的攻门加成；以下分配粒度与字段是本轮明确批准的实施约定，并非原文已有细则。

`assault` 可以携带 `engine_use={inventory_id,item_id,quantity:1}`。来源须是该部所属军团自己的公共未预留库存，物品须等于当前城防定义的 `engine_item_id`。省略表示不用，其余动作拒绝该字段；不能用空声明、数量0/2、友军库或另一物品取得加成。

`validate_orders` 与 `resolve` 共用整轮额度分配，一件器械最多服务一部、一部最多一次加成。器械使用不消费实物，但同一件不能再作为同轮制造或修复的消耗材料；本轮末完工的物品下一轮才可使用。AI 保存同样的显式声明，按稳定单位顺序分配，生成时不读取玩家未提交草案。额度只在候选内核算，不滥用 `lot.reserved_by` 或新增持久器械/耐久记录。

`test_siege_engine_use.gd` 独立核验来源/数量、整轮独占、材料互斥、完工边界、AI与真实Kernel待执行军议/半日攻击。省略声明从旧隐式bonus变为不用，是规则语义变更，中央须以military模块版本和保存计划门明确拒绝旧版本或提供可信迁移，不能静默重解释旧军议。

`SiegeScreen` 已在源码接通显式攻具入口：GD/native 的有限 `siege_page` 只列实际可控攻军的本军公共未预留实物；选择、取消和异步换点仍走原 `siege_draft/plan`，整轮合量与材料互斥由原规则复核，轮后只显示已经结算的破坏回执。没有新增库存、持久预留、伤害公式或存档字段。开发期已完成原生编译和Godot解析检查；`test_siege_engine_player_flow.gd` 已准备同源native Main/原Kernel的真实制造、选择/取消、半日攻门和回执对照，等待下一统一节点执行，不据此宣称运行链或像素体验已验收。历史页面oracle只排除新增攻具/回执行，仍精确核对旧控件、草案和地图，完整重绘比较继续包含新控件。

## 墙远射来源投影

正式设计支持城墙提高相连守方的远射伤害，原文120%为候选值。本轮批准的实施约定为：`kind=wall` 且当前HP为正的节点，保护其自身与拓扑实际相连节点的守方；多墙只记录多个来源，不叠加倍率。名字、贴图或复用同一个battlefield都不能取得保护。

`fortification_context.gd` 提供两个纯入口：

- `project(candidate, data, source_id)` 成功返回 `{ok:true,context}`。军事桥必须先通过既有 `MilitaryCombat._contact` 的完整来源校验；本叶再绑定当前active围城、pending节点、battlefield、实际单位名单与所属军团侧别。普通已校验野战来源返回空context，坏来源明确失败。
- 攻城context固定为 `{kind:'siege_wall',source_id,siege_id,topology_id,node_id,battlefield_id,wall_node_ids,defender_unit_ids,ranged_damage_bp}`。名单唯一且按ID排序；无有效墙时保留完整攻城来源并给空墙名单及10000。字段不进入作者模型或持久战场。
- `factor(context, attacking_unit_id, ranged)` 返回 `{ok:true,applies,factor_bp}`。只有真实守方名单中的部队、有效墙来源与上游已验证的远射同时成立才应用；其余10000。调用方不能把作者声明的字典直接当作受信context。

倍率复用 `rules.wall_defense_bp` 的10000..30000任意整数范围，其本次含义是明确城墙来源的远射伤害倍率。中央每次实际候选攻击重新投影，再将 `factor_bp` 传给军团伤害叶的 `source_factors.siege_wall_bp`。仅显式 `legacy_base_factors_v1` 模式远射消费，参与同一最终乘积、夹限和一次取整；不借用战法power，也不再叠为额外命中或防御。原结构攻门公式保持原职责。

墙在本轮候选中损毁后，新投影立即回到10000。不能缓存整场固定buff；历史战斗来源校验也不能误调用要求当前pending的投影。`test_siege_fortification_context.gd` 使用原Siege生成的节点接敌请求与明确军事边界来源夹具，仅证明投影/资格合同。真实军事桥、伤害第七因子接线和最终版本门由中央与军事owner完成，此叶单独通过不表示完整军团战已接入。

## 后续整编缺口

`integrate` 尚未执行守军整编。现有条款字符串与公共占领不能作为整编完成依据。

- 现有 Army 的组织和成员绑定 `military_action/army_form` 事实；Unit 的组织还受初始驻军来源约束。直接改组织、清旧军团或复写编组事实会破坏历史与守恒校验。
- 需要资产 owner 提供受信的只读整编报价与原子应用入口，明确接收军团/营地、实际人员分类、伤员与俘虏看管、军官同意/控制权，以及公物交接。局面或攻城不得复制该业务。
- 新的归属/交接凭据必须能由军事 validator 解释，保留原名册和旧事实；模块版本与精确旧锁迁移均由中央处理。
- 攻城侧可在该接口冻结后只输出整编请求并消费成功凭据，再结束对应节点占用。本片未接入任何未经审查的整编资产写口。

## 未激活的整编提议合同

`integration_contract.gd` 是生产未接线的只读合同。 输入前提是现有Kernel的完整世界快照；有限的围城/军令/会战坏类型守卫不替代全Schema校验，也不宣称能接收任意构造的全世界字典。没有新增命令、NPC同意策略、持久事实写口或实际整编执行。现有 `integrate` 普通占领行为未改；本叶的 `proposal.ok` 只表示可描述当前参与者，不能当成已同意、完整资产报价或可执行交接。

- `proposal(state, data, siege_id, actor_id)`：重用军事资产与Campaign的当前来源校验及原任官权限，固定接收者为 `siege.campaign_id` 对应的现有主攻军。接收者和全部未撤守军须实际同地、可用、无行动或会战占用；不另选营地、不新建军团、不移动人物。原围困/士气/条款资格仍须真实命令调用既有 `Siege.strategic`，本叶不复制谈判规则。
- 返回 `production_active=false` 与 `proposal`。提议绑定围城/目标/主军令、提议人、接收军与守军当前组织、原编组事实、军令、容器身份、实际地点、完整部队和五类兵员分类、看管与借调来源，以及每位守军军官的原编组和实际任命快照。没有独立公职的部将仍绑定原编组事实，不能伪造一张任命。数组按ID规范化，字典键按规范JSON排序后计算 `proposal_key`；全局revision和tick不参与身份；本片不新增同意过期政策。
- `validate_proposal` 从当前真实状态重建后逐字段比较，名册、分类、接收军、旗号、位置、原任命变化均使旧提议失效；仅时间流逝、供给金额或士气变化不改变身份，真实命令仍须重验当前资格和全部资产容量；返回值不能回写原世界。
- `acceptance(state, data, proposal, source_id)` 只读取永久来源ID。未来来源形状固定为 `format=HM-SIEGE-INTEGRATION-ACCEPTANCE/1`、`kind=siege_integration_acceptance`、`decision=accept`，并含 `actor_id/proposal_key/siege_id/recipient_army_id/source_army_id/unit_ids/formation_source_id/appointment_sources/actor_action_source_id`。一位军官只能覆盖本人在该提议内的精确单位与原任命。
- 当前没有生成该来源的真实actor-action命令及中央来源校验owner。因此即使字段完全匹配，仍返回 `SiegeIntegrationAcceptanceSourceUnavailable`；任官、编组旧事实、会裁剪的命令缓存/历史、整体willingness、信任值及 `consent=true` 均不是同意。`candidate` 不存在可成功执行的路径，缺逐将来源或重复来源明确拒绝。

中央后续接线必须另行核准：真实接受命令和永久来源校验；全部公私产/人数/容量/看管的资产quote/apply；原编组加交接链的当前名册/归属历史解释；人物与原任命/行动解除；旧Campaign的真实整编关闭凭据；版本与存档门。占领与以上步骤必须处于同一Kernel候选，不能用本片的结构检查跳过任何出口。

`test_siege_integration_contract.gd` 复用安全离城正式tiny作者短链，检查只读提议、独立军官绑定、字典重排、真实军议造成的无关revision、真实半日推进而语义未变的稳定身份、Codec恢复，以及各类坏来源拒绝。为检查恶意输入而变更的边界副本绝不提交或恢复为真实世界；未来接受形状只作否定夹具。该门不证明任何真实接受命令、NPC接受或资产整编成功。
