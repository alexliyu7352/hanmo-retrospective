## 当前个人战整页的有限只读输入；身份守卫与画布候选按需读取，不授予控制权或泄露AI承诺。
extends RefCounted
const Values=preload("res://core/base/value_checks.gd")
const Identity=preload("res://core/actor_identity.gd")
const CharacterInput=preload("res://core/views/character_input.gd")
const Tactical=preload("res://core/domains/personal/personal_projection.gd")
const Round=preload("res://core/personal_round.gd")
const ArmorDefinition=preload("res://core/domains/personal/personal_armor_content.gd")
const BODY_FIELDS={"actor_id":TYPE_STRING,"side":TYPE_INT,"position":TYPE_ARRAY,"heading":TYPE_INT,"radius":TYPE_INT,"hp":TYPE_INT,"initial_hp":TYPE_INT,"stamina":TYPE_INT,"protected":TYPE_BOOL,"statuses":TYPE_DICTIONARY}

## 原选择器保插入序、控制者优先和最后fallback；倒地参与者仍能查看未关闭结果。
static func active(state: Dictionary) -> String:
	return _select(state).get("battle_id","")

## 安全选择与原短路共用一次遍历；只在当前分支消费必要形状，命中控制者立即停止。
static func _select(state: Dictionary) -> Dictionary:
	var fallback: String=""
	if not state.get("battles",{}) is Dictionary: return _fail()
	for id in state.get("battles",{}):
		var battle: Variant=state.battles[id]
		if not id is String or not Values.fields(battle,{"kind":TYPE_STRING}): return _fail()
		if battle.kind!="personal": continue
		if not battle.get("phase") is String: return _fail()
		if battle.phase=="closed": continue
		if not state.get("player_actor_id") is String or not battle.get("units",{}) is Dictionary: return _fail()
		if not state.player_actor_id in battle.get("units",{}): continue
		if not battle.get("controller_id") is String: return _fail()
		if battle.controller_id==state.player_actor_id: return {"ok":true,"battle_id":id}
		fallback=id
	return {"ok":true,"battle_id":fallback}

## 页面只选当前参与的个人会话；没有个人战是正常分支，不加载其他页面的数据。
static func query(state: Dictionary,data: Dictionary) -> Dictionary:
	var base: Dictionary=_base(state)
	if not base.ok: return base
	var selected: Dictionary=_select(state)
	if not selected.ok: return selected
	var id: String=selected.battle_id
	if id.is_empty(): return {"ok":true,"actor_id":state.player_actor_id,"revision":state.revision,"battle_id":""}
	return for_battle(state,data,id)

## 旧render显式会话调用与新页面共用装配；仅取必要公开字段，人物失败保独立子结果。
static func for_battle(state: Dictionary,data: Dictionary,battle_id: String) -> Dictionary:
	var stamp: Dictionary=context(state,battle_id)
	if not stamp.ok: return stamp
	if not Values.fields(state,{"actors":TYPE_DICTIONARY,"tick":TYPE_INT}) or not Values.fields(data,{"actors":TYPE_DICTIONARY,"texts":TYPE_DICTIONARY,"personal_objectives":TYPE_DICTIONARY,"personal_battlefields":TYPE_DICTIONARY}): return _fail()
	var battle: Dictionary=state.battles[battle_id]
	var equipment: Dictionary=_equipment_feedback(battle,state.player_actor_id,data)
	if not equipment.get("ok",false): return _fail()
	if not Values.fields(battle,{"battlefield_id":TYPE_STRING,"objective_id":TYPE_STRING,"outcome":TYPE_STRING,"last_events":TYPE_ARRAY}) or not data.personal_battlefields.has(battle.battlefield_id) or not data.personal_objectives.has(battle.objective_id): return _fail()
	var objective: Variant=data.personal_objectives[battle.objective_id]
	if not Values.fields(objective,{"max_rounds":TYPE_INT,"allow_withdrawal":TYPE_BOOL}) or not data.personal_battlefields[battle.battlefield_id] is Dictionary: return _fail()
	for key in ["kind","escort_model"]:
		if objective.has(key) and not objective[key] is String: return _fail()
	if objective.has("initial_stamina") and not Values.whole(objective.initial_stamina,0,100): return _fail()
	var public_battle: Dictionary={"id":battle.id,"controller_id":battle.controller_id,"battlefield_id":battle.battlefield_id,"objective_id":battle.objective_id,"round":battle.round,"phase":battle.phase,"outcome":battle.outcome,"units":{},"damage_feedback":[]}
	var names: Dictionary={}; var characters: Dictionary={}
	for id in battle.units:
		var unit: Variant=battle.units[id]
		if not id is String or not Values.fields(unit,BODY_FIELDS) or unit.actor_id!=id or unit.position.size()!=2 or not unit.position[0] is int or not unit.position[1] is int: return _fail()
		if unit.has("withdrawn_round") and not unit.withdrawn_round is int: return _fail()
		for expiry in unit.statuses.values():
			if not expiry is int: return _fail()
		var body: Dictionary={}
		for key in BODY_FIELDS: body[key]=_copy(unit[key])
		if id==battle.controller_id:
			if not Values.fields(unit,{"weapon_id":TYPE_STRING,"learned":TYPE_DICTIONARY,"recovery_until":TYPE_INT}): return _fail()
			for mastery in unit.learned.values():
				if not mastery is int: return _fail()
			for key in ["weapon_id","learned","recovery_until"]: body[key]=_copy(unit[key])
		public_battle.units[id]=body
		var actor: Variant=state.actors.get(id)
		var prototype: Variant=actor.get("prototype_id",id) if actor is Dictionary else id
		if prototype is String and data.actors.has(prototype) and not data.actors[prototype] is Dictionary: return _fail()
		var definition: Dictionary=Identity.definition(state,data,id)
		var authored: Variant=data.actors.get(id,{"id":id})
		if not authored is Dictionary: return _fail()
		var fallback: Variant=authored.get("display_name",data.texts.get(authored.get("name_key",authored.get("title_key","")),authored.get("id","")))
		var name: Variant=definition.get("display_name",fallback)
		if not name is String: return _fail()
		names[id]=name; characters[id]=CharacterInput.query(state,data,id)
	# 原画布只读target/damage，不沿文本事件动作白名单删掉已有飘字，也不带其他内部字段。
	for event in battle.last_events:
		if not event is Dictionary: return _fail()
		if event.has("target_id") and event.has("damage"):
			if not event.target_id is String or not event.damage is int: return _fail()
			public_battle.damage_feedback.append({"target_id":event.target_id,"damage":event.damage})
	return {"ok":true,"actor_id":state.player_actor_id,"revision":state.revision,"battle_id":battle_id,"battle":public_battle,"equipment_feedback":{"actor_id":state.player_actor_id,"armor_id":equipment.armor_id,"armor_defense":equipment.armor_defense},"present_actor_ids":Tactical.present_actor_ids(battle),"selectable_target_ids":Tactical.selectable_targets(battle,battle.controller_id),"objective":Tactical.goal(battle,data),"events":Tactical.events(battle),"result":Tactical.result(battle,data),"display_names":names,"character_inputs":characters}

## 只核本人入战冻结护具；他人没有已知标记，不读取其甲或以同侧、控制者身份推定可见。
static func _equipment_feedback(battle: Dictionary,actor_id: String,data: Dictionary) -> Dictionary:
	var unit: Variant=battle.units.get(actor_id)
	if not unit is Dictionary: return _fail()
	if not unit.has("armor_id") and not unit.has("armor_defense"): return {"ok":true,"armor_id":"","armor_defense":0}
	if not data.get("items") is Dictionary: return _fail()
	return ArmorDefinition.snapshot(unit,data.items)

## 身份核只读取原必要戳和参与引用；不读AI内容、人物外观、目标、钱包或事实。
static func context(state: Dictionary,battle_id: String) -> Dictionary:
	var base: Dictionary=_base(state)
	if not base.ok: return base
	if not state.battles.has(battle_id): return {"ok":false,"code":"UnknownBattle"}
	var battle: Variant=state.battles[battle_id]
	if not Values.fields(battle,{"id":TYPE_STRING,"kind":TYPE_STRING,"round":TYPE_INT,"phase":TYPE_STRING,"controller_id":TYPE_STRING,"units":TYPE_DICTIONARY}): return _fail()
	if battle.id!=battle_id or battle.kind!="personal" or not state.player_actor_id in battle.units or not battle.units.has(battle.controller_id): return _fail()
	return {"ok":true,"actor_id":state.player_actor_id,"revision":state.revision,"battle_id":battle_id,"round":battle.round,"phase":battle.phase,"controller_id":battle.controller_id}

## 表现候选从完整真实battle计算；它不代替preview的当前人物/控制者/阶段/版本授权。
static func geometry(state: Dictionary,data: Dictionary,battle_id: String,draft: Dictionary) -> Dictionary:
	var stamp: Dictionary=context(state,battle_id)
	if not stamp.ok: return stamp
	var result: Dictionary=canvas_inputs(state.battles[battle_id],data,draft)
	if not result.ok: return result
	result.merge(stamp,true); return result

## 生产geometry与旧Canvas完整会话夹具共用同一算法；绝不从公开DTO拼造空AI会话。
static func canvas_inputs(battle: Dictionary,data: Dictionary,draft: Dictionary) -> Dictionary:
	if not Values.fields(draft,{"skill_id":TYPE_STRING,"action":TYPE_STRING}): return {"ok":false,"code":"InvalidPlan"}
	return {"ok":true,"movement_distance":Round.movement_distance(battle,data,battle.controller_id,draft.skill_id,draft.action),"fallback_tactical_preview":Tactical.plan(battle,data,battle.controller_id,draft)}

## 三个入口共用最低身份源形状；不是完整WorldSchema，更不创建新world_id。
static func _base(state: Dictionary) -> Dictionary:
	if state.is_empty(): return {"ok":false,"code":"NoWorld"}
	if not Values.fields(state,{"player_actor_id":TYPE_STRING,"revision":TYPE_INT,"battles":TYPE_DICTIONARY}) or not Values.whole(state.revision): return _fail()
	return {"ok":true}

## 可变集合深复制，标量保持原值；UI不能回写已提交状态。
static func _copy(value: Variant) -> Variant:
	return value.duplicate(true) if value is Dictionary or value is Array else value

## 新有限输入的坏必要源明确失败，不以假目标、假坐标或假权限继续绘制。
static func _fail() -> Dictionary: return {"ok":false,"code":"InvalidPersonalBattleViewSource"}
