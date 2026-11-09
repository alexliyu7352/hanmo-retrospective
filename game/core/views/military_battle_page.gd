## 野战表现的有限只读来源；复用原军令、补给与人物查询，不携AI、随机流或结算写口。
extends RefCounted
const Round=preload("res://core/military_round.gd")
const Combat=preload("res://core/military_combat.gd")
const Identity=preload("res://core/actor_identity.gd")
const Assets=preload("res://core/military_assets.gd")
const Location=preload("res://core/world_location.gd")
const CONDITION_HISTORY_LIMIT=6

## 空ID依原两遍优先选择；显式失效会话失败，不把其它世界内容补成可操纵战场。
static func query(state: Dictionary, data: Dictionary, battle_id: String="") -> Dictionary:
	if battle_id.is_empty():
		battle_id=_active(state)
		if battle_id.is_empty(): return {"ok":true,"active":false}
	if not _available(state,battle_id): return {"ok":false,"code":"MilitaryBattleUnavailable"}
	var battle: Dictionary=state.battles[battle_id]
	var field: Dictionary=data.battlefields[battle.battlefield_id]
	var objective: Dictionary=data.military_objectives[battle.objective_id]
	var units: Dictionary={}
	for id in battle.units:
		var unit: Dictionary=battle.units[id]; var troop: Dictionary=data.troop_types[unit.troop_type_id]
		var row: Dictionary={"unit_id":id,"display_name":Identity.definition(state,data,unit.commander_actor_id).get("display_name",id)+"部","troop_display_name":_name(troop,data) if troop.has("name_key") else {"infantry":"步卒","archer":"弓手","cavalry":"骑兵"}.get(troop.category,"部队"),"category":troop.category,"portrait_kind":troop.get("battle_portrait_kind",""),"controllable":_controllable(state,battle,id),"command_member":id in battle.command_unit_ids,"protected":id in battle.objective_unit_ids,"tactics":[]}
		for key in ["position","heading","side","morale","tactic_points","withdrawn","statuses"]: row[key]=_copy(unit[key])
		for key in ["fit","wounded","dead","captured","deserted"]: row[key+"_count"]=unit[key].size()
		for key in ["tactic_capacity","movement","minimum_range","maximum_range"]: row[key]=troop[key]
		for tactic in troop.tactics:
			row.tactics.append({"id":tactic.id,"kind":tactic.kind,"point_cost":tactic.point_cost,"display_name":_name(tactic,data) if tactic.has("name_key") else {"rally":"鼓舞","charge":"突击","volley":"齐射","support":"协防"}.get(tactic.kind,tactic.id)})
		if row.controllable:
			var condition: Dictionary=_condition(state,data,id)
			if not condition.is_empty(): row.condition=condition
		units[id]=row
	var geometry: Dictionary={"cells":[],"exits":[],"visual_tag":field.get("visual_tag","")}
	for cell in field.cells: geometry.cells.append({"q":cell.q,"r":cell.r,"terrain":cell.terrain})
	for exit in field.exits: geometry.exits.append({"position":exit.position.duplicate(),"side":exit.side})
	if objective.has("point"): geometry.point=objective.point.duplicate()
	var events: Array=[]
	for event in battle.last_events:
		var row: Dictionary={}
		for key in ["kind","unit_id","target_id","casualties","code","captured","deserted"]:
			if event.has(key): row[key]=_copy(event[key])
		events.append(row)
	var result: Dictionary={}
	for key in ["winner_side","reason"]:
		if battle.result.has(key): result[key]=battle.result[key]
	return {"ok":true,"active":true,"identity":_identity(state,battle),"header":{"display_name":_name(field,data),"stage_visual_tag":field.get("visual_tag","city"),"max_rounds":objective.max_rounds,"controller_side":battle.controller_side,"command_scope":battle.command_scope,"command_unit_ids":battle.command_unit_ids.duplicate(),"plan_submitted":battle.get("plan_submitted",false)},"units":units,"objective":{"kind":objective.kind,"text":objective_text(battle,objective)},"result":result,"events":events,"logistics":_logistics(state,data,battle.command_army_id),"geometry":geometry}

## 草案报价复用本轮真实墙来源；战法候选再经完整草案逐格校验，不保留普通圈或缓存目标朝向。
static func draft(state: Dictionary, data: Dictionary, battle_id: String, selected_id: String, orders: Dictionary) -> Dictionary:
	if not _available(state,battle_id): return {"ok":false,"code":"MilitaryBattleUnavailable"}
	var battle: Dictionary=state.battles[battle_id]
	var sources: Dictionary=Combat.damage_sources(state,data,battle)
	if not sources.ok: return sources
	var result: Dictionary={"ok":true,"identity":_identity(state,battle),"orders_validation":Round.validate_orders(battle,data,state.player_actor_id,orders),"selected_quote":{},"movement":[],"attackable":[],"supportable":[]}
	if orders.has(selected_id): result.selected_quote=Round.preview(battle,data,selected_id,orders[selected_id],sources.factors)
	if battle.phase!="planning" or not battle.units.has(selected_id): return result.duplicate(true)
	var order: Dictionary=orders.get(selected_id,{})
	var support: bool=false
	if not order.is_empty():
		for tactic in data.troop_types[battle.units[selected_id].troop_type_id].tactics:
			if order.action=="tactic" and tactic.id==order.tactic_id: support=tactic.kind=="support"
	var is_tactic: bool=order.get("action")=="tactic"
	var available: Dictionary=Round.movement_options(battle,data,selected_id,order.get("tactic_id","") if is_tactic else "")
	if available.ok:
		if is_tactic:
			for cell in available.cells:
				var candidate: Dictionary=order.duplicate(true); candidate.destination=cell.position.duplicate()
				var quote: Dictionary=Round.preview(battle,data,selected_id,candidate,sources.factors)
				if quote.ok: result.movement.append({"position":candidate.destination,"path":quote.path,"cost":quote.cost})
		else: result.movement=available.cells
	if not order.is_empty():
		if support:
			# 可协防圈仍以完整当前草案调用正式预览，不在表现层重算邻接或点数。
			for id in battle.units:
				var candidate: Dictionary=order.duplicate(true); candidate.target_id=id
				if Round.preview(battle,data,selected_id,candidate,sources.factors).ok: result.supportable.append(id)
			return result.duplicate(true)
		for id in battle.units:
			if battle.units[id].side==battle.units[selected_id].side: continue
			var attack: Dictionary=order.duplicate(true); attack.action="attack"; attack.tactic_id=""; attack.target_id=id
			if order.action=="tactic": attack.action="tactic"; attack.tactic_id=order.tactic_id
			if Round.preview(battle,data,selected_id,attack,sources.factors).ok: result.attackable.append(id)
	return result.duplicate(true)

## 仅公开指挥者本人的未关闭野战；命令提交仍独立经过原世界权限校验。
static func _available(state: Dictionary, id: String) -> bool:
	var battle: Dictionary=state.get("battles",{}).get(id,{})
	return battle.get("kind")=="military" and battle.get("phase")!="closed" and battle.get("controller_actor_id")==state.get("player_actor_id")

## 同半日先选未提交会战，再按原ID序选择其它未关闭会战。
static func _active(state: Dictionary) -> String:
	var ids: Array=state.get("battles",{}).keys(); ids.sort()
	for id in ids:
		var battle: Dictionary=state.battles[id]
		if _available(state,id) and battle.phase=="planning" and not battle.get("plan_submitted",false): return id
	for id in ids:
		if _available(state,id): return id
	return ""

## 五戳描述此次读取来源，不作为命令revision或新增授权epoch。
static func _identity(state: Dictionary, battle: Dictionary) -> Dictionary:
	return {"actor_id":state.player_actor_id,"revision":state.get("revision",0),"battle_id":battle.id,"round":battle.round,"phase":battle.phase}

## 复用原页面控制候选条件；布尔值只决定显示可编辑项，不构成授权。
static func _controllable(state: Dictionary, battle: Dictionary, id: String) -> bool:
	if battle.phase!="planning" or battle.controller_actor_id!=state.player_actor_id or not id in battle.command_unit_ids or not battle.units.has(id): return false
	var unit: Dictionary=battle.units[id]
	if unit.side!=battle.controller_side or unit.withdrawn or unit.fit.is_empty() or unit.morale<=0: return false
	return unit.commander_actor_id==battle.controller_actor_id if battle.command_scope=="unit" else unit.army_id==battle.command_army_id

## 只在原可控门内读取真实部队条件与已发生来源；缺初值或条件不补默认值，不暴露整事实、名册或训练支付条款。
static func _condition(state: Dictionary, data: Dictionary, id: String) -> Dictionary:
	var unit: Dictionary=state.get("military_units",{}).get(id,{})
	if not unit.get("training") is int or not unit.get("fatigue") is int: return {}
	var result: Dictionary={"training":unit.training,"fatigue":unit.fatigue,"source":{"kind":"military_unit","unit_id":id},"initial_source":{},"history_count":0,"history":[]}
	var garrison_id: String=unit.get("source_garrison_id","")
	for definition in data.get("garrisons",{}).get(garrison_id,{}).get("units",[]):
		if definition.get("id")==id and definition.get("initial_training") is int and definition.get("initial_fatigue") is int:
			result.initial_source={"garrison_id":garrison_id,"unit_id":id,"training":definition.initial_training,"fatigue":definition.initial_fatigue}
			break
	for source_id in state.get("facts",{}):
		var fact: Dictionary=state.facts[source_id]
		if fact.get("kind")=="military_training" and fact.get("terms",{}).get("unit_id")==id:
			for lesson in fact.get("lessons",[]):
				if lesson.tick>state.tick: continue
				var row: Dictionary={"kind":"military_training","source_id":source_id,"tick":lesson.tick,"supply_fact_id":lesson.supply_fact_id}
				for key in ["training_before","training_after","fatigue_before","fatigue_after"]: row[key]=lesson[key]
				result.history_count+=1; _remember_condition(result.history,row)
		elif fact.get("kind")=="military_march" and fact.get("rule")=="terrain_march_v1" and fact.get("units",{}).has(id) and fact.tick<=state.tick:
			var profile: Dictionary=fact.units[id]
			var row: Dictionary={"kind":"military_march","source_id":source_id,"tick":fact.tick,"supply_fact_id":fact.supply_fact_id,"training_before":profile.training,"training_after":profile.training,"fatigue_before":profile.fatigue_before,"fatigue_after":profile.fatigue_after,"rule":fact.rule,"action_id":fact.movement.action_id,"plan_id":fact.movement.plan_id}
			result.history_count+=1; _remember_condition(result.history,row)
	return result

## 返回最近六个真实课时/行军来源并显式保留总数；有界临时数组不复制全部世界历史，也不改变源顺序。
static func _remember_condition(history: Array, row: Dictionary) -> void:
	history.append(row)
	history.sort_custom(func(a,b): return a.tick>b.tick if a.tick!=b.tick else (a.kind<b.kind if a.kind!=b.kind else a.source_id<b.source_id))
	if history.size()>CONDITION_HISTORY_LIMIT: history.pop_back()

## 无显式军团仍按原Location回退，仅本人统领活动军团才保留原补给报价。
static func _logistics(state: Dictionary, data: Dictionary, army_id: String) -> Dictionary:
	if army_id.is_empty(): army_id=Location.army_id(state,state.player_actor_id)
	var army: Dictionary=state.get("armies",{}).get(army_id,{})
	if army.get("commander_actor_id")!=state.player_actor_id: return {}
	return Assets.supply_preview(state,data,army_id).duplicate(true)

## 作者名称沿原宿主降级顺序，未知名称不猜测本地化内容。
static func _name(record: Dictionary, data: Dictionary) -> String:
	return record.get("display_name",data.texts.get(record.get("name_key",record.get("title_key","")),record.get("id","")))

## 深复制有限容器，不让消费方改动世界或目录字段。
static func _copy(value: Variant) -> Variant:
	return value.duplicate(true) if value is Dictionary or value is Array else value

## 五类目标显示真实达成条件和当前进度；退出触发区权限按阵营解释。
static func objective_text(current: Dictionary, objective: Dictionary) -> String:
	var prefix: String="我军" if objective.side==current.controller_side else "敌军"
	match objective.kind:
		"repel": return "击退对方全部作战部队。"
		"hold": return "%s守住标记地，连续 %d 轮（已守 %d 轮）。" % [prefix,objective.hold_rounds,current.objective_state.hold_rounds]
		"breakthrough": return "%s至少 %d 部由本方退路突破（已离场 %d 部）。" % [prefix,objective.required_units,_escaped_count(current,objective.side)]
		"escort": return "%s护送指定 %d 部完整离场；保护部队失去全部战力即失败。" % [prefix,current.objective_unit_ids.size()]
		"cover_retreat": return "%s掩护指定部队，至少 %d 部经退路离场（已离场 %d 部）。" % [prefix,objective.required_units,_escaped_count(current,objective.side)]
	return "未知作战目标，请检查内容定义。"

## 退出进度只统计目标阵营已经保存的离场事实，避免混入敌方撤退数量。
static func _escaped_count(current: Dictionary, side: int) -> int:
	var total: int=0
	for id in current.objective_state.escaped_unit_ids:
		if current.units[id].side==side: total+=1
	return total
