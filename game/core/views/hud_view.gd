## 当前玩家HUD的有限标量投影；只读原日期、姓名、钱与显示HP，不授予行动或自动推进许可。
extends RefCounted
const Values=preload("res://core/base/value_checks.gd")
const Identity=preload("res://core/actor_identity.gd")
const TimeRules=preload("res://core/base/time_rules.gd")

## 个人战上下文由原页面选择器明确传入；只核同玩家unit，不重选战斗或借其它会话显示HP。
static func query(state: Dictionary, data: Dictionary, personal_battle_id: String="") -> Dictionary:
	if state.is_empty(): return {"ok":false,"code":"NoWorld"}
	if not Values.fields(state,{"player_actor_id":TYPE_STRING,"revision":TYPE_INT,"tick":TYPE_INT,"actors":TYPE_DICTIONARY,"wallets":TYPE_DICTIONARY}) or not Values.whole(state.revision) or not Values.whole(state.tick): return _fail()
	if not Values.fields(data,{"actors":TYPE_DICTIONARY,"texts":TYPE_DICTIONARY}): return _fail()
	var actor_id: String=state.player_actor_id
	var actor: Variant=state.actors.get(actor_id)
	if not Values.fields(actor,{"id":TYPE_STRING,"wallet_id":TYPE_STRING,"hp":TYPE_INT}) or actor.id!=actor_id or not Values.whole(actor.hp): return _fail()
	var wallet: Variant=state.wallets.get(actor.wallet_id)
	if not Values.fields(wallet,{"id":TYPE_STRING,"owner":TYPE_DICTIONARY,"balance":TYPE_INT}) or wallet.id!=actor.wallet_id or not Values.whole(wallet.balance): return _fail()
	if not Values.fields(wallet.owner,{"kind":TYPE_STRING,"id":TYPE_STRING}) or wallet.owner.kind!="actor" or wallet.owner.id!=actor_id: return _fail()
	var prototype: Variant=actor.get("prototype_id",actor_id)
	if prototype is String and data.actors.has(prototype) and not data.actors[prototype] is Dictionary: return _fail()
	var definition: Dictionary=Identity.definition(state,data,actor_id)
	# 与原Main._name保持同一降级；空身份定义仍为空名，不加入画像或成长资格。
	var display_name: Variant=definition.get("display_name",data.texts.get(definition.get("name_key",definition.get("title_key","")),definition.get("id","")))
	if not display_name is String: return _fail()
	var hp: int=actor.hp
	if not personal_battle_id.is_empty():
		if not state.get("battles") is Dictionary: return _fail()
		var battle: Variant=state.battles.get(personal_battle_id)
		if not Values.fields(battle,{"id":TYPE_STRING,"kind":TYPE_STRING,"phase":TYPE_STRING,"units":TYPE_DICTIONARY}) or battle.id!=personal_battle_id or battle.kind!="personal" or battle.phase=="closed": return _fail()
		var unit: Variant=battle.units.get(actor_id)
		if not Values.fields(unit,{"hp":TYPE_INT}) or not Values.whole(unit.hp): return _fail()
		hp=unit.hp
	return {"ok":true,"actor_id":actor_id,"revision":state.revision,"day_index":TimeRules.day_index(state.tick),"half_day":TimeRules.half_day(state.tick),"display_name":display_name,"balance":wallet.balance,"hp":hp}

## 新薄口的坏必要来源明确失败，不以默认姓名、钱或体力伪装完整HUD。
static func _fail() -> Dictionary: return {"ok":false,"code":"InvalidHudViewSource"}
