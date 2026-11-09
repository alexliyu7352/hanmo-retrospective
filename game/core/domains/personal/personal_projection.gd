## 个人战必要只读接口投影；复用合法计划/几何预览，不读AI承诺、不调用轮次结算或推进随机。
extends RefCounted
const Round=preload("res://core/personal_round.gd")
const Strike=preload("res://core/domains/personal/personal_strike.gd")
const Objectives=preload("res://core/domains/personal/personal_objectives.gd")
const Withdrawal=preload("res://core/domains/personal/personal_withdrawal.gd")

## 仍在战场上的真实身体列表；倒地者保留，已退出者仅在战果参与者中出现，不再可点选。
static func present_actor_ids(battle: Dictionary) -> Array:
	var result: Array=[]
	for id in battle.units:
		if not Withdrawal.departed(battle.units[id]): result.append(id)
	result.sort()
	return result

## 敌方存活且未离场者才是可选目标；具体落点/朝向能否命中另由同一攻击预览说明。
static func selectable_targets(battle: Dictionary, actor_id: String) -> Array:
	if not battle.units.has(actor_id): return []
	var result: Array=[]; var side: int=battle.units[actor_id].side
	for id in battle.units:
		var unit: Dictionary=battle.units[id]
		if unit.side!=side and unit.hp>0 and not Withdrawal.departed(unit): result.append(id)
	result.sort()
	return result

## 原合法plan报价附带当前快照可知信息；攻击目标可能在共同移动后改变，不把预览当已提交伤害。
static func plan(battle: Dictionary, data: Dictionary, actor_id: String, intent: Dictionary) -> Dictionary:
	var valid: Dictionary=Round.validate_plan(battle,data,actor_id,intent)
	if not valid.ok: return valid
	var normalized: Dictionary=valid.plan
	var unit: Dictionary=battle.units[actor_id]
	var skill: Dictionary=data.personal_skills.get(normalized.skill_id,{})
	var base_weapon: Dictionary=data.items.get(unit.weapon_id,{}).get("weapon",{})
	var effective: Dictionary=Strike.weapon(base_weapon,skill) if not base_weapon.is_empty() else {}
	var targets: Array=selectable_targets(battle,actor_id)
	var result: Dictionary={"ok":true,"basis":"current_snapshot_candidate","may_change_after_simultaneous_movement":true,"movement_distance":Round.movement_distance(battle,data,actor_id,normalized.skill_id,normalized.action),"selectable_target_ids":targets,"present_actor_ids":present_actor_ids(battle),"attackable_target_ids":[],"objective":goal(battle,data),"nominal_stamina_cost":0,"recent_events":events(battle)}
	if not effective.is_empty(): result.effective_weapon={"category":effective.category,"minimum_range":effective.minimum_range,"maximum_range":effective.maximum_range,"arc_degrees":effective.arc_degrees}
	if normalized.action=="skill": result.nominal_stamina_cost=skill.stamina_cost
	elif normalized.action=="attack": result.nominal_stamina_cost=base_weapon.stamina_cost
	if skill.has("requires_stationary"): result.requires_stationary=skill.requires_stationary
	if skill.has("guard_facing"): result.guard_facing=skill.guard_facing
	if normalized.action=="attack" or (normalized.action=="skill" and skill.kind=="strike"):
		result.attack=Round.preview_attack(battle,data,actor_id,normalized.destination,normalized.heading,normalized.target_id,normalized.skill_id)
		for id in targets:
			if Round.preview_attack(battle,data,actor_id,normalized.destination,normalized.heading,id,normalized.skill_id).ok: result.attackable_target_ids.append(id)
	return result

## 把既有目标合同转成稳定语义码；不以场景/技能ID猜任务完成，不包括世界奖惩或AI意图。
static func goal(battle: Dictionary, data: Dictionary) -> Dictionary:
	var definition: Dictionary=data.personal_objectives[battle.objective_id]
	var protected_ids: Array=[]; var side: int=battle.units[battle.controller_id].side
	for id in battle.units:
		if battle.units[id].side==side and battle.units[id].protected: protected_ids.append(id)
	protected_ids.sort()
	var rule: String="defeat_opponents"
	if definition.get("kind")=="escort": rule="protected_exit" if Objectives.exit_enabled(definition) else "protect_and_defeat_opponents"
	var timeout: String="draw"
	if Objectives.exit_enabled(definition): timeout="protected_exit_or_truce"
	elif definition.get("kind")=="sparring" and battle.units.size()==2: timeout="hp_ratio_10pp"
	return {"kind":definition.get("kind",""),"goal_rule":rule,"timeout_rule":timeout,"max_rounds":definition.max_rounds,"allow_withdrawal":definition.allow_withdrawal,"protected_actor_ids":protected_ids,"initial_stamina":definition.get("initial_stamina",50)}

## 只投影已经发生的已知事件字段；未知内部数据与未来ai_plans绝不整包透传。
static func events(battle: Dictionary) -> Array:
	var result: Array=[]
	var fields: Dictionary={"actor_id":TYPE_STRING,"target_id":TYPE_STRING,"action":TYPE_STRING,"skill_id":TYPE_STRING,"damage":TYPE_INT,"code":TYPE_STRING,"status":TYPE_STRING,"expires_round":TYPE_INT,"amount":TYPE_INT,"hit_bp":TYPE_INT}
	for event in battle.get("last_events",[]):
		if not event is Dictionary or not event.get("action") in ["attack","skill","counter","withdraw","objective","outcome","stamina","status","defend","wait"]: continue
		var row: Dictionary={}
		for field in fields:
			if typeof(event.get(field))==fields[field]: row[field]=event[field]
		result.append(row)
	return result

## 战果仅来自已结束会话，区分正HP退出/倒地和明确目标理由；原battle_close仍独占世界释放与一次回写。
static func result(battle: Dictionary, data: Dictionary) -> Dictionary:
	if not battle.get("phase") in ["result","closed"]: return {"ok":false,"code":"NoBattleResult"}
	var people: Array=[]; var defeated: Array=[]; var ids: Array=battle.units.keys(); ids.sort()
	for id in ids:
		var unit: Dictionary=battle.units[id]
		var status: String="withdrawn" if Withdrawal.departed(unit) else ("down" if unit.hp<=0 else "standing")
		if unit.hp<=0: defeated.append(id)
		people.append({"actor_id":id,"side":unit.side,"hp":unit.hp,"initial_hp":unit.initial_hp,"status":status})
	var actual_events: Array=events(battle); var reasons: Array=[]
	for event in actual_events:
		if event.action=="objective" and event.get("code") in ["EscortExitReached","EscortExitTimeout"] and not event.code in reasons: reasons.append(event.code)
	return {"ok":true,"outcome":battle.outcome,"objective":goal(battle,data),"participants":people,"defeated_actor_ids":defeated,"withdrawn_actor_ids":Withdrawal.participants(battle.units),"reason_codes":reasons,"events":actual_events}
