## 个人战显式有限候选评分；只处理轮初可见投影的固定指标，不读取意图、世界战略或随机。
extends RefCounted
const MODEL: String="scored_v1"
const MAX_CANDIDATES: int=512

## 缺省仍用旧AI；只有已知固定模型可以启用本片，不接受作者表达式或自定义权重。
static func validate_objective(objective: Dictionary, path: String="") -> Dictionary:
	if objective.has("ai_model") and (typeof(objective.ai_model)!=TYPE_STRING or objective.ai_model!=MODEL): return {"ok":false,"code":"PERSONAL_AI_MODEL","path":path+"/ai_model"}
	return {"ok":true}

## 新模型是显式作者选择，不按场景名或人物ID偷开。
static func enabled(objective: Dictionary) -> bool:
	return objective.get("ai_model","")==MODEL

## 四分点固定式完整保留成本/4；目标与保护各最多一次，不把未定义的距离进度塞进权重。
static func evaluate(plan: Dictionary, origin: Array, goal: bool, protects: bool, damage_q: int, incoming_q: int, cost: int) -> Dictionary:
	var terms: Dictionary={"objective_q":400 if goal else 0,"protection_q":160 if protects else 0,"damage_q":damage_q,"incoming_q":-incoming_q,"cost_q":-cost}
	var score: int=0
	for value in terms.values(): score+=value
	var reasons: Array=[]
	if goal: reasons.append("ProjectedObjective")
	if protects: reasons.append("ReducedProtectedRisk")
	if damage_q>0: reasons.append("ExpectedEffectiveDamage")
	if incoming_q>0: reasons.append("ExpectedIncomingDamage")
	if cost>0: reasons.append("CommittedStaminaCost")
	return {"plan":plan.duplicate(true),"score_q":score,"terms":terms,"reason_codes":reasons,"action_key":action_key(plan,origin)}

## 动作编码是唯一同分规则；无进展权重时，合法移动等待排在纯原地等待之前，不搜索未来回合。
static func action_key(plan: Dictionary, origin: Array) -> String:
	var order: int={"withdraw":0,"attack":2,"skill":3,"defend":4,"wait":5}.get(plan.action,9)
	if plan.action=="wait" and plan.destination!=origin: order=1
	return str(order)+":"+JSON.stringify(plan,"",true)

## 从同一输入的只读评估中选唯一结果；相同分按规范编码，不依赖字典枚举或随机抽签。
static func choose(evaluations: Array) -> Dictionary:
	var best: Dictionary={}
	for entry in evaluations:
		if best.is_empty() or entry.score_q>best.score_q or (entry.score_q==best.score_q and entry.action_key<best.action_key): best=entry
	return best.duplicate(true)
