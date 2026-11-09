## BA08协防的纯局部仲裁：只分配既定减员，不改部队、随机流、资产或世界时钟。
extends RefCounted
const Hex=preload("res://core/military_hex.gd")

## 主动选择一个仍在场的邻接友部，不能自保、跨阵营或保护已离场单位。
static func preview(battle: Dictionary, unit_id: String, target_id: String) -> Dictionary:
	if unit_id==target_id: return {"ok":false,"code":"MilitarySupportRequiresOtherUnit"}
	var source: Variant=battle.units.get(unit_id)
	var target: Variant=battle.units.get(target_id)
	if not source is Dictionary or not target is Dictionary or not _active(source) or not _active(target): return {"ok":false,"code":"MilitarySupportUnavailable"}
	if source.side!=target.side: return {"ok":false,"code":"MilitarySupportRequiresAlly"}
	if Hex.distance(source.position,target.position)!=1: return {"ok":false,"code":"MilitarySupportRequiresAdjacent"}
	return {"ok":true,"target_id":target_id,"share_bp":2000}

## 同步移动后只保留仍相邻的协防声明；显式按单位ID排列，不窥视未提交玩家草案。
static func assignments(battle: Dictionary, data: Dictionary, plans: Dictionary) -> Dictionary:
	var result: Dictionary={}
	var ids: Array=plans.keys(); ids.sort()
	for id in ids:
		var plan: Dictionary=plans[id]
		if plan.action!="tactic": continue
		var support: bool=false
		for tactic in data.troop_types[battle.units[id].troop_type_id].tactics:
			if tactic.id==plan.tactic_id and tactic.kind=="support": support=true
		if not support or not preview(battle,id,plan.target_id).ok: continue
		if not result.has(plan.target_id): result[plan.target_id]=[]
		result[plan.target_id].append(id)
	return result

## 实施约定：同一友部多个声明只选稳定ID最小者，不叠加；本函数可单独替换该仲裁。
static func select_supporter(eligible: Array) -> String:
	if eligible.is_empty(): return ""
	var ids: Array=eligible.duplicate(); ids.sort()
	return ids[0]

## 首个正数直接伤害只按2000基点取整一次；不足承伤容量留原目标，转移结果不再递归分配。
static func allocate(battle: Dictionary, guards: Dictionary, consumed: Dictionary, casualties: Dictionary, target_id: String, original: int) -> Dictionary:
	var result: Dictionary={"target_casualties":original,"supporter_id":"","supporter_casualties":0,"consumed":false}
	if original<=0 or consumed.has(target_id): return result
	result.consumed=true
	var supporter: String=select_supporter(guards.get(target_id,[]))
	if supporter.is_empty(): return result
	var remaining: int=maxi(0,battle.units[supporter].fit.size()-int(casualties.get(supporter,0)))
	@warning_ignore("integer_division")
	var share: int=mini(remaining,original*2000/10000)
	result.supporter_id=supporter; result.supporter_casualties=share
	result.target_casualties=original-share
	return result

## 同一轮可承担协防者必须仍有可战兵员，已撤离或士气归零不成为隐形盾牌。
static func _active(unit: Dictionary) -> bool:
	return not unit.withdrawn and not unit.fit.is_empty() and unit.morale>0
