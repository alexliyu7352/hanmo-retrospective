## 个人战显式轮末气势模型；仅解释固定已知规则，恢复根据本轮实际结果，不读取世界或随机。
extends RefCounted
const MODEL: String="round_v1"
const ROUND_CAP: int=20

## 缺省完全保留旧行为；未知模型明确拒绝，不把作者字符串作为表达式执行。
static func validate_objective(objective: Dictionary, path: String="") -> Dictionary:
	if objective.has("stamina_model") and (typeof(objective.stamina_model)!=TYPE_STRING or objective.stamina_model!=MODEL): return {"ok":false,"code":"PERSONAL_STAMINA_MODEL","path":path+"/stamina_model"}
	if objective.has("initial_stamina") and (typeof(objective.initial_stamina)!=TYPE_INT or objective.initial_stamina<0 or objective.initial_stamina>100): return {"ok":false,"code":"PERSONAL_INITIAL_STAMINA","path":path+"/initial_stamina"}
	return {"ok":true}

## 入战只读本场作者初值，所有参与者同一配置；缺省50，不创建人物持续气势或恢复时重补初值。
static func starting_amount(objective: Dictionary) -> Dictionary:
	var valid: Dictionary=validate_objective(objective)
	if not valid.ok: return valid
	return {"ok":true,"stamina":objective.get("initial_stamina",50)}

## 防御额外恢复仅属于配置心法的固定5点效果；不把未知数值或主动技能扩成新的被动收益。
static func validate_skill(skill: Dictionary, path: String="") -> Dictionary:
	if skill.has("defend_recovery_bonus") and (skill.get("kind")!="heart" or typeof(skill.defend_recovery_bonus)!=TYPE_INT or skill.defend_recovery_bonus!=5): return {"ok":false,"code":"PERSONAL_DEFEND_RECOVERY","path":path+"/defend_recovery_bonus"}
	return {"ok":true}

## 只有显式声明才启用本片恢复时点和候选数值，旧内容不因工具升级而重新平衡。
static func enabled(objective: Dictionary) -> bool:
	return objective.get("stamina_model","")==MODEL

## 纯计算实际恢复差额：原地等待、防御、普通真实命中与每次正伤受击累计，死亡和离场均不得领奖。
static func grants(units: Dictionary, plans: Dictionary, starting_units: Dictionary, events: Array, defend_bonuses: Dictionary={}) -> Dictionary:
	var pending: Dictionary={}; var ordinary_hit: Dictionary={}
	for id in plans:
		if not units.has(id) or units[id].hp<=0 or units[id].has("withdrawn_round"): continue
		var plan: Dictionary=plans[id]
		pending[id]=0
		if plan.action=="defend": pending[id]=10+defend_bonuses.get(id,0)
		elif plan.action=="wait" and plan.destination==starting_units[id].position and units[id].position==starting_units[id].position: pending[id]=15
	for event in events:
		if event.get("code")!="Hit" or not event.get("action") in ["attack","skill","counter"] or typeof(event.get("damage"))!=TYPE_INT or event.damage<=0: continue
		var target: String=event.get("target_id","")
		if pending.has(target): pending[target]+=3
		var attacker: String=event.get("actor_id","")
		if event.action=="attack" and pending.has(attacker) and plans[attacker].action=="attack": ordinary_hit[attacker]=true
	var result: Dictionary={}; var ids: Array=pending.keys(); ids.sort()
	for id in ids:
		var amount: int=pending[id]+(5 if ordinary_hit.has(id) else 0)
		amount=mini(mini(ROUND_CAP,amount),100-units[id].stamina)
		if amount>0: result[id]=amount
	return result

## 恢复在所有扣费、伤害和实际离场标记之后一次应用，不能先借来支付同一轮发动成本。
static func apply(battle: Dictionary, plans: Dictionary, starting_units: Dictionary, defend_bonuses: Dictionary={}) -> void:
	var recovered: Dictionary=grants(battle.units,plans,starting_units,battle.last_events,defend_bonuses)
	for id in recovered:
		battle.units[id].stamina+=recovered[id]
		battle.last_events.append({"actor_id":id,"action":"stamina","amount":recovered[id],"code":"Recovered"})
