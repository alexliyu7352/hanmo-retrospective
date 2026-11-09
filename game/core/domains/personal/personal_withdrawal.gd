## 个人战脱离的纯状态与出口规则；离场保留身份、位置与体力，不等同倒地或世界死亡。
extends RefCounted
const Navigation=preload("res://core/domains/personal/personal_navigation.gd")
const LOW_HP_PERCENT: int=25

## 缺省仍在场，只有已校验的实际离场轮才表示脱离；不借用伤势或HP零伪装。
static func departed(unit: Dictionary) -> bool:
	return unit.has("withdrawn_round")

## 只为仍在场者提供几何身体；倒地身体保留，脱离者的历史出口位置不再阻挡。
static func bodies(units: Dictionary) -> Dictionary:
	var result: Dictionary={}
	for id in units:
		if not departed(units[id]): result[id]=units[id]
	return result

## 返回稳定且完整的脱离名单，供唯一战后事实核对，不从事件日志猜测。
static func participants(units: Dictionary) -> Array:
	var result: Array=[]
	for id in units:
		if departed(units[id]): result.append(id)
	result.sort()
	return result

## 首片采用明确的25%低体力候选；不是平衡结论，保护者不因低血独自离开。
static func low_hp(unit: Dictionary) -> bool:
	return not unit.protected and unit.hp>0 and unit.hp*100<=unit.initial_hp*LOW_HP_PERCENT

## 出口只读战斗固定场地和实际坐标；历史不会改读人物后来的世界位置。
static func at_exit(battle: Dictionary, data: Dictionary, actor_id: String) -> bool:
	var unit: Dictionary=battle.units[actor_id]
	return point_at_exit(data.personal_battlefields[battle.battlefield_id],unit.side,unit.position)

## 圆形出口按本方与人物中心判断，不把对方出口或计划终点算作已抵达。
static func point_at_exit(arena: Dictionary, side: int, position: Array) -> bool:
	for gate in arena.exits:
		if gate.side==side and _distance(position,gate.position)<=gate.radius: return true
	return false

## 仅用轮初可见身体和场地找本方出口首段；已有合法出口位置可原地声明撤离。
static func destination(battle: Dictionary, data: Dictionary, actor_id: String, maximum: int) -> Array:
	var unit: Dictionary=battle.units[actor_id]
	var arena: Dictionary=data.personal_battlefields[battle.battlefield_id]
	if point_at_exit(arena,unit.side,unit.position): return unit.position.duplicate()
	var occupied: Array=[]; var ids: Array=battle.units.keys(); ids.sort()
	for id in ids:
		if id!=actor_id and not departed(battle.units[id]): occupied.append({"position":battle.units[id].position,"radius":battle.units[id].radius})
	var goals: Array=[]
	for gate in arena.exits:
		if gate.side!=unit.side: continue
		goals.append(gate.position.duplicate())
		for radius in [maxf(0.0,gate.radius-1.0),gate.radius*0.5]:
			for step in range(16): goals.append([roundi(gate.position[0]+cos(TAU*step/16.0)*radius),roundi(gate.position[1]+sin(TAU*step/16.0)*radius)])
	return Navigation.destination(arena,unit.position,unit.radius,occupied,goals,maximum)

## 伤害已同步落地后才执行NPC意图；真实近战命中或未到出口只记失败事件，不能传送消失。
static func apply_npcs(battle: Dictionary, data: Dictionary, plans: Dictionary, melee_hits: Dictionary) -> void:
	var ids: Array=plans.keys(); ids.sort()
	for id in ids:
		var unit: Dictionary=battle.units[id]
		if id==battle.controller_id or unit.protected or unit.hp<=0 or departed(unit) or plans[id].action!="withdraw": continue
		var code: String="Withdrawn"
		if not at_exit(battle,data,id): code="ExitNotReached"
		elif melee_hits.has(id): code="WithdrawalMeleeHit"
		else: unit.withdrawn_round=battle.round
		battle.last_events.append({"actor_id":id,"action":"withdraw","code":code})

## 玩家安全撤离包含已通过出口和缠斗检查的保护者；同轮共同标记，其他同伴保留其真实状态。
static func apply_group(battle: Dictionary) -> void:
	var controller: Dictionary=battle.units[battle.controller_id]
	var ids: Array=battle.units.keys(); ids.sort()
	for id in ids:
		var unit: Dictionary=battle.units[id]
		if id==battle.controller_id or (unit.side==controller.side and unit.protected):
			unit.withdrawn_round=battle.round
			battle.last_events.append({"actor_id":id,"action":"withdraw","code":"Withdrawn"})

## 字段缺省兼容未脱离者；实际字段须对应存活、既有出口、准许撤离及合理轮次，不重算历史世界位置。
static func validate_unit(battle: Dictionary, data: Dictionary, actor_id: String) -> Dictionary:
	var unit: Dictionary=battle.units[actor_id]
	if not departed(unit): return {"ok":true}
	var round_value: Variant=unit.withdrawn_round
	if typeof(round_value)!=TYPE_INT or round_value<1 or round_value>battle.round or unit.hp<=0: return {"ok":false,"code":"InvalidWithdrawalState"}
	if not data.personal_objectives[battle.objective_id].allow_withdrawal or not at_exit(battle,data,actor_id): return {"ok":false,"code":"InvalidWithdrawalExit"}
	if battle.phase=="planning" and (round_value>=battle.round or actor_id==battle.controller_id or unit.protected): return {"ok":false,"code":"InvalidWithdrawalPhase"}
	if actor_id==battle.controller_id or unit.protected:
		if battle.get("outcome","")!="withdrawn" or battle.units[battle.controller_id].get("withdrawn_round")!=round_value: return {"ok":false,"code":"InvalidGroupWithdrawal"}
	return {"ok":true}

## 连续场地的出口与身体坐标共用欧氏距离，计算不消耗随机。
static func _distance(first: Array, second: Array) -> float:
	return Vector2(float(first[0])-second[0],float(first[1])-second[1]).length()
