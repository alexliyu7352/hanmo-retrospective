## 查询真实人物的战斗占用；保留原世界位置与行程，不另造参战人物或推进时间。
extends RefCounted
const Location=preload("res://core/world_location.gd")

## 返回人物尚未关闭的会话副本；没有参战时返回空字典，调用者不能借查询写入状态。
static func for_actor(state: Dictionary, actor_id: String) -> Dictionary:
	for battle in state.get("battles",{}).values():
		if battle.phase!="closed" and _participates(state,battle,actor_id): return battle.duplicate(true)
	return {}

## 判断人物是否仍被战斗或结果回顾占用；服务提供者也必须经过同一检查。
static func occupied(state: Dictionary, actor_id: String) -> bool:
	for battle in state.get("battles",{}).values():
		if battle.phase!="closed" and _participates(state,battle,actor_id): return true
	return false

## 个人战思考、演出和结果回顾期间冻结世界；战术回合不额外产生半日。
static func pauses_world(state: Dictionary) -> bool:
	for battle in state.get("battles",{}).values():
		if battle.kind=="personal" and battle.phase!="closed": return true
	return false

## 军团任一部接战时统帅与同军真人不能另行移动；个人战仍按真人战场单位查询。
static func _participates(state: Dictionary, battle: Dictionary, actor_id: String) -> bool:
	if battle.kind=="personal": return battle.units.has(actor_id)
	return actor_id in battle.get("participant_actor_ids",[]) or Location.army_id(state,actor_id) in battle.get("army_ids",[])
