## 保护者的局部确定性导航；只读轮初人物和场地，以既有直线移动意图跟随主角或进入同一出口。
extends RefCounted
const Objectives=preload("res://core/domains/personal/personal_objectives.gd")
const Withdrawal=preload("res://core/domains/personal/personal_withdrawal.gd")
const Navigation=preload("res://core/domains/personal/personal_navigation.gd")

## 使用有限可见拐点找首段合法路径；无路、已就位或预算不足都原地等待，不跨墙或传送。
static func destination(battle: Dictionary, data: Dictionary, actor_id: String, max_distance: int) -> Array:
	if Objectives.exit_enabled(data.personal_objectives[battle.objective_id]): return Withdrawal.destination(battle,data,actor_id,max_distance)
	var unit: Dictionary=battle.units[actor_id]
	var origin: Array=unit.position
	if max_distance<=0: return origin.duplicate()
	var leader: Dictionary=battle.units[battle.controller_id]
	var arena: Dictionary=data.personal_battlefields[battle.battlefield_id]
	var occupied: Array=[]
	var ids: Array=battle.units.keys(); ids.sort()
	for id in ids:
		if id!=actor_id and not battle.units[id].has("withdrawn_round"): occupied.append({"position":battle.units[id].position,"radius":battle.units[id].radius})
	var goals: Array=[]
	for gate in arena.exits:
		if gate.side==unit.side and _distance(leader.position,gate.position)<=gate.radius:
			if _distance(origin,gate.position)<=gate.radius: return origin.duplicate()
			goals.append(gate.position.duplicate())
			for radius in [maxf(0.0,gate.radius-1.0),gate.radius*0.5]:
				for step in range(16): goals.append(_radial(gate.position,radius,TAU*step/16.0))
	if goals.is_empty():
		var spacing: int=unit.radius+leader.radius+4
		if _distance(origin,leader.position)<=spacing: return origin.duplicate()
		for step in range(16): goals.append(_radial(leader.position,float(spacing),TAU*step/16.0))
	return Navigation.destination(arena,origin,unit.radius,occupied,goals,max_distance)

## 圆周候选使用整数坐标；所有舍入后的点仍须通过真实身体检查。
static func _radial(center: Array, radius: float, angle: float) -> Array:
	return [roundi(center[0]+cos(angle)*radius),roundi(center[1]+sin(angle)*radius)]

## 仅计算连续规则坐标距离，不读取渲染位置或玩家草案。
static func _distance(first: Array, second: Array) -> float:
	return Vector2(float(first[0])-float(second[0]),float(first[1])-float(second[1])).length()
