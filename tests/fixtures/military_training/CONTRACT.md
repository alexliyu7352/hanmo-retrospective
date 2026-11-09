# 真实驻军训练服务与可选军阵实例

正式来源：06《世界AI、事件与治理》§6.8 的训练为4半日、钱150与真实口粮、训练增加3+对应技能级并封顶100；03§3.6军阵与04 Q13/Q14分别有手动成绩和任务包装，本片不混用其时长或接受调用者自报成绩。05§5.8仅明定行军疲劳+3、合格驻地休整−10，未定义训练疲劳；因此训练疲劳只读作者显式0–20参数，夹具4是实施候选，绝非全军默认或角色修业+6。

## 所有权与最小接口

新生产叶 `game/core/domains/military/training_rules.gd`、薄适配 `training_action.gd`。不修改MilitaryAssets、Kernel、阶段、registry、共享Schema、Round或攻城。

- `initial_profile(definition)` 只投影作者 `initial_training/initial_fatigue`；只由新局初始化器消费，不遍历旧档补值
- `preview(state,data,actor_id,{office_id,unit_id,challenge_mode?},own_action='')`
- `begin(state,data,action,offer)`、`step(state,data,action,finish_tick)`、`finish(...)`
- `preview_cancel(state,data,actor_id,{action_id})`、`cancel(...)`
- `validate_action(...)`、`validate_state(...)`

状态：真实 `military_units[id].training/fatigue/training_action_id`。已有单位缺profile明确 `TrainingProfileRequired`；可信迁移由中央另做。本叶当前从作者真初值和训练课时回算训练/疲劳，未来行军/休整等生产者需对应可信来源接入后才可修改这些值。

作者最小字段：源garrison.units条目的 `initial_training/initial_fatigue` 为整数0–100；训练营 `training_wallet_id`、`training_fatigue_per_tick` 为显式服务配置。服务钱包必须不同于营付款钱包，且两者均属于当前合法组织；不默认发工资给玩家。初值和服务账户在新局夹具准备阶段明示，未生成历史训练或虚构已有口粮事实。

等级依赖中央已封 `6c2a338:game/core/domains/cultivation/skill_levels.gd` 的纯 `Levels.level(real_actor.skills[skill_id])`；没有第二套等级阈值或可写level。本树仅放该精确未提交编译overlay，helper由中央独占登记。

## 已冻结逐半日条款

- 真实受任公职对应当前营、组织与单位；一课一部固定fit名册，不训练已编入army、正动员、参战或被另一训练action占用的单位
- 本人带训；实际单位将领和fit中所有命名人物都在营且由同一action占用。开始与每半日重验身份、地点、名册和作者条款
- 开始预留150，尚未提供课时不转账
- 每实际提供第n半日支付 `floor(150*n/4)-floor(150*(n-1)/4)`，即37/38/37/38
- 训练逐半日落账：累计 `floor((3+真实教官对应技能级)*n/4)` 的差额，训练最终不超过100；部队疲劳每已提供半日增加作者值，最高100。角色疲劳不变
- 只读同一半日真实 `military_supply`、当前营/组织/产权来源、shortage=0及完整参训名册/人员mouth_ids；粮仍由原军事供给扣，不另预留或扣第二份
- 唯一 `action.id + tick` 防止同课重复支付或训练；正常完成、中断或取消只封账/释放自己的单位标记和未来费用，不再发整课奖励
- 失权、断供或环境不再满足时保留已发生训练/付款/疲劳，停止未来份额；已消费口粮不返还。缺少本半日供给证明是阶段合同错误，不能假装提供或正常吞掉

永久事实 `facts[action.id]` 的kind为 `military_training`，固定terms（含真实appointment来源）和lessons。每课记录tick、supply_fact_id、fee_paid、训练/疲劳前后。事实、活动、占用和余额余量相互校验；历史回算拒重复tick、别部名册、伪整数费用与无理由的提前完课。

## 中央必须接通的边界

1. 注册military_train及取消命令、action适配与状态/事实owner和版本；新局按作者真值写profile，旧缺字段不自动迁移
2. 当前原军事供给已在actions_step之前；继续传其finish_tick，训练不新增世界时钟
3. 编组、出征、战斗、其他训练等所有占用入口须共同核training_action_id；本叶只是其中一方守卫，尚不能声称全世界互斥已接通
4. 真实供给mouth并集须包含合法带训者/命名参训者（教官不是现unit commander时旧口粮名单未必覆盖），仍只扣一次；公共服务钱包的产权登记亦由中央统一处理
5. finish/cancel后原Step.release继续释放人物/actions；失败由原Kernel丢弃完整候选，不在本叶造第二个事务引擎
6. 军阵叶接线见下节；中央另接可选Schema、模块版本和忙碌instant，不能在finish追改已经发生的课时

## 实际验证与限制

固定Godot4.7.2，复用既有轻core工程。原Factory驻军、真实office_accept、原MilitaryAssets供给、ActionRuntime薄适配实跑最终191项：四课150/资金守恒、部分取消75/75、实际短粮与失权中断、同action+tick去重、单位/角色疲劳分离、上限、两课后StrictJSON保存并整state/RNG/ID同命令续跑、坏历史拒绝。

这是本叶与实际相邻规则的局部门。ActionRuntime的测试注册表是明示适配夹具；完整中央registry/全Schema/命令入口/完整v2恢复尚未验收。没有GUI、全量或长期世界。首准备诊断为新capabilities误配旧lived_city header，已换回与header哈希完全一致的原5ec81e能力文件，未改header/绕校验；原诊断保留。本地运行所用真实lived_city header及Schema/能力原件只复制约百KB，未stage大资源。

## 独审两项收口

- 非空产权字段：原02e2111报价误读owner.ownership_id，实际公共API字段为occupation_id。一行f3288c2改正。专属 `test_military_training_occupation.gd` 复用实际任官/编组、两次原Siege胜利与Assets产权接收、真实归营/重新任命后核首课。原02得到owner=military:67但quote空，16项2红；仅换f328后17项全过，真实37钱课时引用同一产权供给。原191门未重跑
- 非队长教官：独审actor.he原合法开课后不在原supply mouth并集，课程硬失败。中央9c034a1只改Assets，严格依真实训练行动/事实和当前任命将教官加入原字典并集。新增 `test_military_training_supply.gd` 用该精确只读overlay跑40项，通过actor.he及已为队长的去重对照：各恰一口、按唯一receipt消耗、训练不二扣粮、支付37/预留113。没有放宽本叶receipt参训名单

专属产权准备最初遇到既有garrison_ownership_fixture.fresh重造catalog不带ok，当前Snapshot返回NoCatalog；新门改复用已经由军事夹具创建的两营，再调用原win/receipt，不改工厂守卫/旧fixture。第二次准备误将army_id传给Campaign.army_returned的receipt_id参数，改为实际返回收据后得到上述真实旧红/新绿；两次准备日志同样保留，不能当产品缺陷或验收通过。

当前证据分层：191属于02e2111；非空来源17属于f328；供给40属于f328与中央9c034a1组合。中央完整命令注册、单位跨域互斥、Schema/版本和真实Kernel入口仍未由本门证明。


## 军阵唯一结算接线（8ae5433）

受信依赖是P的 `ace079d:training_drill.gd`，复用正式 `drill_challenge.gd`、`challenge_basis.gd` 与共享纯等级函数。本模块不写题面算法或接受调用者自报倍率。营地可选 `training_drill_id`；请求可选 `challenge_mode`，只能是automatic/manual，省略为automatic。无题面时仅automatic合法，保持原terms23/fact9且不抽minigame流。

可选terms.drill固定7键：kind/challenge_id/mode/tactics_xp/actor_snapshot/source_sha256/group_source；可选fact.drill固定5键：seed_origin/challenge/phase/frozen_tick/performance_bp。二者来源由P的validate完整重验，不能删除字段退回自动。个人步兵/弓术/骑术决定课程gain_total；题面本人兵法tactics与智谋intellect冻结为独立表现快照，不改用将领或别人的技能。

顺序：

1. preview按真实单位/人物/钱包条件报价，再生成固定题面条款；begin全部资格通过后给临时fact调用Drill.start，恰抽一次minigame，随后才设置原unit/钱包预留和唯一fact
2. step先核原活动/事实、当前军务和本半日真实口粮；所有条件满足才freeze_before_lesson，与追加首lesson处于同一候选。未完成手动题在这里按自动基线关闭
3. 训练累计只取 `normal=floor(gain_total*n/4)`，再 `adjusted=floor(normal*performance_bp/10000)`，最后加起始训练并夹到100；本课兑现相对已提供累计的差额。费用与部队疲劳继续原公式，绝不乘军阵系数
4. 首课后保留同一成绩/seed；续课只验证冻结题面，当前兵法/智谋变化不重新报价。每课历史也用同一累计公式重算
5. 零课取消/失效先close_unprovided，再封原事实；成绩可在题面内保留但performance_bp仍null，不给训练。部分取消保留冻结成绩和已提供差额，不补满课程

`transition_drill(state,data,actor_id,kind,params)`先验证本人当前military_train与精确action_id和原预留，再委托P四种transition，成功只写fact.drill.challenge。它不增加tick、ID、RNG、钱款或训练；中央busy命令门负责只把这四个精确instant送到这里。结束或首课后不能复活/重新选择。

新 `test_military_training_drill.gd` 继承旧帮助函数，未执行旧191组。真实原任职、Assets供给及ActionRuntime局部门覆盖首课前/两课后StrictJSON保存、同命令续跑整态/RNG/ID、满分正常gain6累计1/3/4/7、费用37/38/37/38、疲劳每课4、零课/两课取消、未完成题自动基线、无题面原形状、首课后/取消后/过时/他人输入拒绝、假倍率/缺实例拒绝、同tick/finish去重。作者夹具仅在新局边界明示本人步兵XP650；原供给来源和任命均真实执行。人物兵法/智谋变化一项仅是冻结快照的状态边界探针，不声称测了另一成长行动。

本片仍是相邻规则局部门，中央完整busy命令/Schema/版本与v2恢复未由此证明。普通军团跨域占用与训练服务正式入口继续由中央统一接线；本片没有GUI、完整stage或长世界。
