## 真实地形行军条件：只消费可信执行点给出的整数保留轨迹；同一军团半日一份来源与疲劳回执，不扣第二份钱粮或推进时钟。
extends RefCounted
const Values=preload("res://core/base/value_checks.gd")
const History=preload("res://core/domains/military/condition_history.gd")
const Factory=preload("res://core/world_factory.gd")
const Terrain=preload("res://core/world_terrain.gd")
const MODEL="terrain_march_v1"
const FACT_KIND="military_march"
const EXECUTION_FORMAT="HM-TERRAIN-EXECUTION/1"

## 仅当前有效剧本的显式选择启用；有训练初值不等于已授权行军生产，旧内容缺省保持原行为。
static func enabled(state: Dictionary, data: Dictionary) -> bool:
	var scenario: Variant=data.get("scenarios",{}).get(state.get("scenario_id"))
	return scenario is Dictionary and scenario.get("military_condition_model") is String and scenario.military_condition_model==MODEL

## 原执行器成功后在同一候选中一次封存来源和既有单位疲劳；所有可失败校验均先于ID分配与字段写入，历史重放绝不恢复旧profile。
static func apply_execution(state: Dictionary, data: Dictionary, execution: Dictionary, finish_tick: int, contact_source_id: String="", uncut_execution: Dictionary={}) -> Dictionary:
	if not enabled(state,data): return _unchanged()
	if not Values.whole(finish_tick,1) or finish_tick>state.tick+1: return _fail("MilitaryMarchClockMismatch")
	var checked: Dictionary=_execution(state,data,execution,finish_tick,contact_source_id)
	if not checked.ok: return checked
	var uncut: Dictionary=_uncut_source(state,data,execution,finish_tick,contact_source_id,uncut_execution)
	if not uncut.ok: return uncut
	var projected: Dictionary=validated_projections(state,data,maxi(state.tick,finish_tick))
	if not projected.ok: return projected
	for id in state.facts:
		var prior: Variant=state.facts[id]
		if not _is_receipt(prior): continue
		if prior.movement.action_id==execution.action_id and prior.tick==finish_tick:
			if prior.movement!=execution or prior.contact_source_id!=contact_source_id or prior.uncut_execution!=uncut_execution: return _fail("ConflictingMilitaryMarchExecution")
			return {"ok":true,"receipt_id":id,"already_completed":true}
	if finish_tick!=state.tick+1 or execution.source_tick!=state.tick: return _fail("MilitaryMarchClockMismatch")
	var live: Dictionary=_live_source(state,execution)
	if not live.ok: return live
	var army: Dictionary=live.army
	for fact in state.facts.values():
		if _is_receipt(fact) and fact.army_id==army.id and fact.tick==finish_tick: return _fail("ConflictingMilitaryMarchExecution")
	if not checked.positive: return _unchanged()
	var food: Dictionary=_find_supply(state,army.id,finish_tick)
	if not food.ok: return food
	var contact_time: Dictionary={}
	if not contact_source_id.is_empty():
		var contact: Variant=state.facts.get(contact_source_id)
		if not Values.fields(contact,{"time_num":TYPE_INT,"time_den":TYPE_INT}): return _fail("MilitaryMarchContactSourceMissing")
		contact_time={"num":contact.time_num,"den":contact.time_den}
	var units: Dictionary={}
	for unit_id in army.unit_ids:
		var unit: Variant=state.military_units.get(unit_id)
		if not Values.fields(unit,{"army_id":TYPE_STRING,"organization_id":TYPE_STRING,"training_action_id":TYPE_STRING,"mobilizing_action_id":TYPE_STRING,"training":TYPE_INT,"fatigue":TYPE_INT}) or unit.army_id!=army.id or unit.organization_id!=army.organization_id or not unit.training_action_id.is_empty() or not unit.mobilizing_action_id.is_empty(): return _fail("MilitaryMarchUnitUnavailable",str(unit_id))
		for battle in state.get("battles",{}).values():
			if battle is Dictionary and battle.get("phase")!="closed" and battle.get("units",{}) is Dictionary and battle.units.has(unit_id): return _fail("MilitaryMarchUnitEngaged",unit_id)
		var before: Dictionary=_profile_at(state,data,unit_id,projected.activities_by_unit.get(unit_id,[]),finish_tick)
		if not before.ok: return before
		if before.profile!={"training":unit.training,"fatigue":unit.fatigue}: return _fail("MilitaryMarchHistoryMismatch",unit_id)
		var after: Dictionary=History.project_profile(before.profile,"march")
		if not after.ok: return after
		units[unit_id]={"training":unit.training,"fatigue_before":unit.fatigue,"fatigue_after":after.profile.fatigue}
	var receipt: Dictionary={"kind":FACT_KIND,"rule":MODEL,"tick":finish_tick,"army_id":army.id,"formation_source":army.formation_source,"commander_actor_id":army.commander_actor_id,"supply_fact_id":food.id,"movement":execution.duplicate(true),"uncut_execution":uncut_execution.duplicate(true),"contact_source_id":contact_source_id,"contact_time":contact_time,"units":units}
	checked=_receipt(state,data,receipt,finish_tick)
	if not checked.ok: return checked
	checked=_continuity_for_new(state,data,receipt,finish_tick)
	if not checked.ok: return checked
	var id: String=Factory.next_id(state,"military")
	state.facts[id]=receipt
	for unit_id in units: state.military_units[unit_id].fatigue=units[unit_id].fatigue_after
	return {"ok":true,"receipt_id":id,"already_completed":false}

## 保存与候选只读校验同一永久来源，再生成供原Training折叠器使用的临时投影；不接受调用方validated标记或补造漏掉的历史。
static func validated_projections(state: Dictionary, data: Dictionary, through_tick: int) -> Dictionary:
	var receipts: Array=[]; var seen: Dictionary={}; var activities: Dictionary={}
	for id in state.get("facts",{}):
		var fact: Variant=state.facts[id]
		if not _is_receipt(fact): continue
		if not enabled(state,data): return _fail("MilitaryMarchNotEnabled")
		if not _instance(id,state.get("next_instance_id")): return _fail("InvalidMilitaryMarchId")
		var checked: Dictionary=_receipt(state,data,fact,through_tick)
		if not checked.ok: return checked
		var key: String=fact.army_id+":"+str(fact.tick)
		if seen.has(key): return _fail("DuplicateMilitaryMarchTick")
		seen[key]=true; receipts.append(fact)
		for unit_id in fact.units:
			if not activities.has(unit_id): activities[unit_id]=[]
			var row: Dictionary=fact.units[unit_id]
			activities[unit_id].append({"unit_id":unit_id,"tick":fact.tick,"kind":"march","source":{"kind":"executed_movement","id":id},"training_before":row.training,"training_after":row.training,"fatigue_before":row.fatigue_before,"fatigue_after":row.fatigue_after})
	var ordered: Dictionary=_continuity(state,receipts)
	if not ordered.ok: return ordered
	return {"ok":true,"activities_by_unit":activities}

## 同一事实既绑定原军团/执行/供给，又给出有限整数条件变化；历史军团可已归营，不要求旧动作仍占用人物。
static func _receipt(state: Dictionary, data: Dictionary, fact: Variant, through_tick: int) -> Dictionary:
	var fields: Dictionary={"kind":TYPE_STRING,"rule":TYPE_STRING,"tick":TYPE_INT,"army_id":TYPE_STRING,"formation_source":TYPE_STRING,"commander_actor_id":TYPE_STRING,"supply_fact_id":TYPE_STRING,"movement":TYPE_DICTIONARY,"uncut_execution":TYPE_DICTIONARY,"contact_source_id":TYPE_STRING,"contact_time":TYPE_DICTIONARY,"units":TYPE_DICTIONARY}
	if not Values.fields(fact,fields) or fact.size()!=fields.size() or fact.kind!=FACT_KIND or fact.rule!=MODEL or not Values.whole(fact.tick,1,through_tick): return _fail("InvalidMilitaryMarchFact")
	var army: Variant=state.get("armies",{}).get(fact.army_id)
	if not Values.fields(army,{"id":TYPE_STRING,"formation_source":TYPE_STRING,"commander_actor_id":TYPE_STRING,"organization_id":TYPE_STRING,"garrison_id":TYPE_STRING,"order_id":TYPE_STRING,"unit_ids":TYPE_ARRAY}) or army.id!=fact.army_id or army.formation_source!=fact.formation_source or army.commander_actor_id!=fact.commander_actor_id or not _strings(army.unit_ids): return _fail("MilitaryMarchArmySourceMissing")
	var formed: Variant=state.facts.get(fact.formation_source)
	if not Values.fields(formed,{"kind":TYPE_STRING,"command":TYPE_STRING,"phase":TYPE_STRING,"success":TYPE_BOOL,"actor_id":TYPE_STRING,"completed_tick":TYPE_INT,"terms":TYPE_DICTIONARY,"result":TYPE_DICTIONARY}) or formed.kind!="military_action" or formed.command!="army_form" or formed.phase!="completed" or not formed.success or formed.actor_id!=army.commander_actor_id or not formed.result.get("army_id") is String or formed.result.army_id!=army.id: return _fail("MilitaryMarchFormationSourceMissing")
	if not Values.fields(formed.terms,{"unit_ids":TYPE_ARRAY,"organization_id":TYPE_STRING,"actor_id":TYPE_STRING,"order_id":TYPE_STRING,"ownership_id":TYPE_STRING}) or formed.terms.unit_ids!=army.unit_ids or formed.terms.organization_id!=army.organization_id or formed.terms.actor_id!=army.commander_actor_id or formed.terms.order_id!=army.order_id or not Values.whole(formed.completed_tick,0,fact.tick-1): return _fail("MilitaryMarchFormationMismatch")
	var moved: Dictionary=_execution(state,data,fact.movement,fact.tick,fact.contact_source_id)
	if not moved.ok: return moved
	var uncut: Dictionary=_uncut_source(state,data,fact.movement,fact.tick,fact.contact_source_id,fact.uncut_execution)
	if not uncut.ok: return uncut
	if fact.movement.action_start_tick<formed.completed_tick: return _fail("MilitaryMarchFormationMismatch")
	if not moved.positive or fact.movement.actor_id!=fact.commander_actor_id: return _fail("MilitaryMarchMovementRequired")
	var food: Dictionary=_supply(state,data,fact.supply_fact_id,army,fact.tick,formed.terms.ownership_id)
	if not food.ok: return food
	var contacted: Dictionary=_contact(state,fact,army)
	if not contacted.ok: return contacted
	if not _strings(fact.units.keys()) or _ids(fact.units)!=_sorted(army.unit_ids) or fact.units.is_empty(): return _fail("MilitaryMarchUnitSourceMismatch")
	for unit_id in fact.units:
		var initial: Dictionary=_initial(state,data,unit_id)
		if not initial.ok: return initial
		var row: Variant=fact.units[unit_id]
		if not Values.fields(row,{"training":TYPE_INT,"fatigue_before":TYPE_INT,"fatigue_after":TYPE_INT}) or row.size()!=3 or not Values.whole(row.training,0,100) or not Values.whole(row.fatigue_before,0,100): return _fail("InvalidMilitaryMarchProfile",str(unit_id))
		var expected: Dictionary=History.project_profile({"training":row.training,"fatigue":row.fatigue_before},"march")
		if row.fatigue_after!=expected.profile.fatigue: return _fail("InvalidMilitaryMarchProfile",str(unit_id))
	return {"ok":true}

## 执行记录只接受真实整数端点和原计划后缀；数学只核本步保留前缀，不重新走旅行或用后来旗标重寻路。
static func _execution(state: Dictionary, data: Dictionary, execution: Variant, finish_tick: int, contact_id: String) -> Dictionary:
	var fields: Dictionary={"format":TYPE_STRING,"actor_id":TYPE_STRING,"action_id":TYPE_STRING,"action_start_tick":TYPE_INT,"source_tick":TYPE_INT,"plan_id":TYPE_STRING,"map_id":TYPE_STRING,"from_point":TYPE_ARRAY,"to_point":TYPE_ARRAY,"route_index_before":TYPE_INT,"route_index_after":TYPE_INT,"travel_credit_before":TYPE_INT,"travel_credit_after":TYPE_INT,"result_location":TYPE_DICTIONARY}
	if not Values.fields(execution,fields) or execution.size()!=fields.size() or execution.format!=EXECUTION_FORMAT or not _point(execution.from_point) or not _point(execution.to_point) or not Values.whole(execution.source_tick,0) or execution.source_tick!=finish_tick-1 or not Values.whole(execution.action_start_tick,0,execution.source_tick) or not Values.whole(execution.travel_credit_before) or not Values.whole(execution.travel_credit_after): return _fail("InvalidMilitaryMarchExecution")
	var scenario: Variant=data.get("scenarios",{}).get(state.get("scenario_id"))
	if not scenario is Dictionary or not scenario.get("world_map_id") is String or scenario.world_map_id!=execution.map_id or not data.get("world_maps",{}).get(execution.map_id) is Dictionary: return _fail("MilitaryMarchTerrainRequired")
	var plan: Variant=state.get("facts",{}).get(execution.plan_id)
	if not Values.fields(plan,{"kind":TYPE_STRING,"actor_id":TYPE_STRING,"action_id":TYPE_STRING,"tick":TYPE_INT,"map_id":TYPE_STRING,"mode":TYPE_STRING,"budget":TYPE_INT,"starting_load":TYPE_INT,"origin":TYPE_ARRAY,"points":TYPE_ARRAY}) or plan.kind!="terrain_journey" or plan.actor_id!=execution.actor_id or plan.action_id!=execution.action_id or plan.map_id!=execution.map_id or plan.mode!="army" or not Values.whole(plan.tick,execution.action_start_tick,execution.source_tick) or not Values.whole(plan.budget,1) or plan.points.size()<2 or not _point(plan.origin): return _fail("MilitaryMarchPlanSourceMissing")
	var map: Dictionary=data.world_maps[execution.map_id]
	var budgets: Variant=map.get("budgets")
	if not budgets is Dictionary or not Values.whole(budgets.get("army"),1) or not Values.whole(plan.starting_load): return _fail("MilitaryMarchPlanBudgetMismatch")
	# 仅重核原TerrainJourney的冻结负载与作者预算公式，不用后来库存重算负载，也不启动第二次旅行。
	if plan.budget!=maxi(1,budgets.army*100/(100+plan.starting_load*4)): return _fail("MilitaryMarchPlanBudgetMismatch")
	if execution.action_start_tick==execution.source_tick and execution.travel_credit_before!=0: return _fail("MilitaryMarchCreditMismatch")
	for point in plan.points:
		if not _point(point): return _fail("InvalidMilitaryMarchPlan")
	if plan.origin!=plan.points[0] or not Values.whole(execution.route_index_before,1,plan.points.size()-1) or not Values.whole(execution.route_index_after,execution.route_index_before,plan.points.size()): return _fail("MilitaryMarchPlanIndexMismatch")
	var prefix: Array=[execution.from_point]
	for index in range(execution.route_index_before,execution.route_index_after): prefix.append(plan.points[index])
	if execution.route_index_after==plan.points.size():
		if execution.to_point!=plan.points[-1]: return _fail("MilitaryMarchEndpointMismatch")
	elif not _on_leg(execution.to_point,prefix[-1],plan.points[execution.route_index_after]): return _fail("MilitaryMarchEndpointMismatch")
	prefix.append(execution.to_point)
	var positive: bool=false; var cost: int=0
	for index in range(1,prefix.size()):
		if prefix[index-1]==prefix[index]: continue
		positive=true
		# 接敌整数锚点不是另一条实际行走直线；预算改由同收据保留的真整步来源严格证明。
		if contact_id.is_empty():
			var segment: Dictionary=Terrain.segment_cost(data.world_maps[execution.map_id],prefix[index-1],prefix[index],"army",{})
			if not segment.ok: return _fail("MilitaryMarchPathMismatch")
			cost+=segment.cost
	if contact_id.is_empty() and (cost>plan.budget+execution.travel_credit_before or execution.travel_credit_after>plan.budget+execution.travel_credit_before): return _fail("MilitaryMarchBudgetMismatch")
	if contact_id.is_empty() and execution.route_index_after<plan.points.size() and cost+execution.travel_credit_after!=plan.budget+execution.travel_credit_before: return _fail("MilitaryMarchCreditMismatch")
	var location: Dictionary=execution.result_location
	if location.get("kind") is String and location.kind=="Wilderness":
		if not Values.fields(location,{"map_id":TYPE_STRING,"origin":TYPE_STRING,"point":TYPE_ARRAY}) or location.size()!=4 or location.map_id!=execution.map_id or location.point!=execution.to_point or not data.get("settlements",{}).has(location.origin): return _fail("MilitaryMarchLocationMismatch")
		if execution.route_index_after==plan.points.size() and (not plan.get("destination") is Array or plan.destination!=execution.to_point): return _fail("MilitaryMarchArrivalMismatch")
	elif location.get("kind") is String and location.kind=="Settlement":
		if not Values.fields(location,{"settlement_id":TYPE_STRING,"place_id":TYPE_STRING}) or location.size()!=3 or not plan.get("destination") is String or plan.destination!=location.settlement_id or execution.route_index_after!=plan.points.size(): return _fail("MilitaryMarchArrivalMismatch")
		var place: Variant=data.get("settlements",{}).get(location.settlement_id)
		if not Values.fields(place,{"entry_place":TYPE_STRING,"map_xy":TYPE_ARRAY}) or place.entry_place!=location.place_id or place.map_xy!=execution.to_point: return _fail("MilitaryMarchArrivalMismatch")
	else: return _fail("MilitaryMarchLocationMismatch")
	if execution.route_index_after==plan.points.size() and execution.travel_credit_after!=0: return _fail("MilitaryMarchArrivalMismatch")
	if not contact_id.is_empty() and (location.kind!="Wilderness" or execution.travel_credit_after!=0 or execution.route_index_after==plan.points.size()): return _fail("MilitaryMarchContactMismatch")
	return {"ok":true,"positive":positive,"plan":plan}

## 接敌只保留同一步真实执行的整数前缀；完整来源仍走普通预算守恒，裁切不得换行动、起点或越过原步终点。
static func _uncut_source(state: Dictionary, data: Dictionary, movement: Dictionary, tick: int, contact_id: String, uncut: Dictionary) -> Dictionary:
	if contact_id.is_empty(): return {"ok":true} if uncut.is_empty() else _fail("MilitaryMarchUncutExecutionMismatch")
	var checked: Dictionary=_execution(state,data,uncut,tick,"")
	if not checked.ok: return checked
	if not checked.positive: return _fail("MilitaryMarchUncutExecutionMismatch")
	for key in ["format","actor_id","action_id","action_start_tick","source_tick","plan_id","map_id","from_point","route_index_before","travel_credit_before"]:
		if movement[key]!=uncut[key]: return _fail("MilitaryMarchUncutExecutionMismatch")
	if movement.route_index_after>uncut.route_index_after: return _fail("MilitaryMarchUncutExecutionMismatch")
	var plan: Dictionary=checked.plan
	var origin: Array=movement.from_point if movement.route_index_after==movement.route_index_before else plan.points[movement.route_index_after-1]
	var target: Array=uncut.to_point if movement.route_index_after==uncut.route_index_after else plan.points[movement.route_index_after]
	if not _on_leg(movement.to_point,origin,target): return _fail("MilitaryMarchUncutExecutionMismatch")
	return {"ok":true}

## 当前写入必须仍是原统帅的真实travel占用，已执行后的参数和位置精确相符；不把路线计划或UI参数当执行成功。
static func _live_source(state: Dictionary, execution: Dictionary) -> Dictionary:
	var actor: Variant=state.get("actors",{}).get(execution.actor_id)
	var action: Variant=state.get("actions",{}).get(execution.action_id)
	if not Values.fields(actor,{"alive":TYPE_BOOL,"available":TYPE_BOOL,"action_id":TYPE_STRING,"location":TYPE_DICTIONARY}) or not actor.alive or not actor.available or actor.action_id!=execution.action_id or actor.location!=execution.result_location: return _fail("MilitaryMarchActorChanged")
	if not Values.fields(action,{"id":TYPE_STRING,"actor_id":TYPE_STRING,"kind":TYPE_STRING,"start_tick":TYPE_INT,"params":TYPE_DICTIONARY}) or action.id!=execution.action_id or action.kind!="travel" or action.actor_id!=execution.actor_id or action.start_tick!=execution.action_start_tick: return _fail("MilitaryMarchActionChanged")
	var p: Dictionary=action.params
	if not Values.fields(p,{"terrain":TYPE_BOOL,"plan_id":TYPE_STRING,"map_id":TYPE_STRING,"mode":TYPE_STRING,"route_index":TYPE_INT,"travel_credit":TYPE_INT,"points":TYPE_ARRAY,"payer_wallet_id":TYPE_STRING}) or not p.terrain or p.plan_id!=execution.plan_id or p.map_id!=execution.map_id or p.mode!="army" or p.route_index!=execution.route_index_after or p.travel_credit!=execution.travel_credit_after or p.points!=state.facts[execution.plan_id].points.slice(execution.route_index_after): return _fail("MilitaryMarchActionChanged")
	var found: Dictionary={}
	for value in state.get("armies",{}).values():
		if not Values.fields(value,{"status":TYPE_STRING,"commander_actor_id":TYPE_STRING}) or value.status!="active" or value.commander_actor_id!=execution.actor_id: continue
		if not found.is_empty(): return _fail("MilitaryMarchArmyAmbiguous")
		found=value
	if not Values.fields(found,{"id":TYPE_STRING,"wallet_id":TYPE_STRING,"unit_ids":TYPE_ARRAY,"member_actor_ids":TYPE_ARRAY,"formation_source":TYPE_STRING,"organization_id":TYPE_STRING}) or found.wallet_id!=p.payer_wallet_id or not _strings(found.unit_ids) or found.unit_ids.is_empty() or not execution.actor_id in found.member_actor_ids: return _fail("MilitaryMarchArmyUnavailable")
	return {"ok":true,"army":found}

## 供给只引用本军该半日原唯一账，不重扣库存；缺粮仍可是真行军，但不在这里加另一份缺粮疲劳。
static func _find_supply(state: Dictionary, army_id: String, tick: int) -> Dictionary:
	var found: String=""
	for id in state.facts:
		var fact: Variant=state.facts[id]
		if not Values.fields(fact,{"kind":TYPE_STRING,"army_id":TYPE_STRING,"tick":TYPE_INT}) or fact.kind!="military_supply" or fact.army_id!=army_id or fact.tick!=tick: continue
		if not found.is_empty(): return _fail("MilitaryMarchSupplyAmbiguous")
		found=id
	return {"ok":true,"id":found} if not found.is_empty() else _fail("MilitaryMarchSupplyRequired")

## 历史供给按原组织、定额及唯一时刻核算，统帅必须是当时实际供养者，不要求后来的兵员分类保持不变。
static func _supply(state: Dictionary, data: Dictionary, id: String, army: Dictionary, tick: int, ownership: String) -> Dictionary:
	var fact: Variant=state.facts.get(id)
	var camp: Variant=data.get("garrisons",{}).get(army.garrison_id)
	if not Values.fields(camp,{"food_item_id":TYPE_STRING,"ration_per_person":TYPE_INT}) or not Values.fields(fact,{"kind":TYPE_STRING,"army_id":TYPE_STRING,"garrison_id":TYPE_STRING,"organization_id":TYPE_STRING,"ownership_id":TYPE_STRING,"tick":TYPE_INT,"item_id":TYPE_STRING,"mouth_ids":TYPE_ARRAY,"required":TYPE_INT,"consumed":TYPE_INT,"shortage":TYPE_INT}): return _fail("MilitaryMarchSupplySourceMissing")
	if fact.kind!="military_supply" or fact.army_id!=army.id or not fact.garrison_id.is_empty() or fact.organization_id!=army.organization_id or fact.ownership_id!=ownership or fact.tick!=tick or fact.item_id!=camp.food_item_id or not _strings(fact.mouth_ids) or not army.commander_actor_id in fact.mouth_ids or not Values.whole(fact.required) or fact.required!=fact.mouth_ids.size()*camp.ration_per_person or not Values.whole(fact.consumed,0,fact.required) or fact.shortage!=fact.required-fact.consumed: return _fail("MilitaryMarchSupplyMismatch")
	var unique: Dictionary=_find_supply(state,army.id,tick)
	return {"ok":true} if unique.ok and unique.id==id else _fail("MilitaryMarchSupplyAmbiguous")

## 接敌回执绑定原动作、冻结计划、保留锚点及同一整数时刻；停驻或时间零不能冒充正位移来源。
static func _contact(state: Dictionary, fact: Dictionary, army: Dictionary) -> Dictionary:
	if fact.contact_source_id.is_empty(): return {"ok":true} if fact.contact_time.is_empty() else _fail("MilitaryMarchContactMismatch")
	var contact: Variant=state.facts.get(fact.contact_source_id)
	if not Values.fields(contact,{"kind":TYPE_STRING,"tick":TYPE_INT,"motion_sources":TYPE_DICTIONARY,"army_order_ids":TYPE_DICTIONARY,"army_locations":TYPE_DICTIONARY,"time_num":TYPE_INT,"time_den":TYPE_INT}) or contact.kind!="military_contact" or contact.tick!=fact.tick or not Values.whole(contact.time_den,1,1000000000) or not Values.whole(contact.time_num,1,contact.time_den): return _fail("MilitaryMarchContactSourceMissing")
	if not Values.fields(fact.contact_time,{"num":TYPE_INT,"den":TYPE_INT}) or fact.contact_time.size()!=2 or fact.contact_time!={"num":contact.time_num,"den":contact.time_den}: return _fail("MilitaryMarchContactTimeMismatch")
	var source: Variant=contact.motion_sources.get(army.id)
	if not source is Dictionary or not Values.whole(source.get("route_index"),1) or source.route_index!=fact.movement.route_index_after: return _fail("MilitaryMarchContactPrefixMismatch")
	if not Values.fields(source,{"actor_id":TYPE_STRING,"action_id":TYPE_STRING,"plan_id":TYPE_STRING,"stationary":TYPE_BOOL,"location":TYPE_DICTIONARY}) or source.stationary or source.actor_id!=fact.commander_actor_id or source.action_id!=fact.movement.action_id or source.plan_id!=fact.movement.plan_id or source.location!=fact.movement.result_location or not contact.army_locations.get(army.id) is Dictionary or contact.army_locations[army.id]!=source.location or not contact.army_order_ids.get(army.id) is String or contact.army_order_ids[army.id]!=army.order_id: return _fail("MilitaryMarchContactMismatch")
	return {"ok":true}

## 原课时仍引用唯一训练事实，不复制训练数值来源；这里只对本次受影响单位按候选半日边界折叠，不调用稳定世界总校验。
static func _profile_at(state: Dictionary, data: Dictionary, unit_id: String, activities: Array, tick: int) -> Dictionary:
	var initial: Dictionary=_initial(state,data,unit_id)
	if not initial.ok: return initial
	var training: Array=[]
	for id in state.facts:
		var fact: Variant=state.facts[id]
		if Values.fields(fact,{"kind":TYPE_STRING,"terms":TYPE_DICTIONARY}) and fact.kind=="military_training" and fact.terms.get("unit_id") is String and fact.terms.unit_id==unit_id: training.append({"action_id":id,"fact":fact})
	return History.fold(unit_id,initial.profile,training,activities,tick)

## 作者初值必须确实属于该部源驻军，当前条件仍只有原单位字段；没有初值就拒绝，不初始化50/0。
static func _initial(state: Dictionary, data: Dictionary, unit_id: String) -> Dictionary:
	var unit: Variant=state.get("military_units",{}).get(unit_id)
	if not Values.fields(unit,{"source_garrison_id":TYPE_STRING,"id":TYPE_STRING}) or unit.id!=unit_id: return _fail("MilitaryMarchProfileRequired",unit_id)
	for definition in data.get("garrisons",{}).get(unit.source_garrison_id,{}).get("units",[]):
		if definition is Dictionary and definition.get("id") is String and definition.id==unit_id:
			if Values.whole(definition.get("initial_training"),0,100) and Values.whole(definition.get("initial_fatigue"),0,100): return {"ok":true,"profile":{"training":definition.initial_training,"fatigue":definition.initial_fatigue}}
	return _fail("MilitaryMarchProfileRequired",unit_id)

## 新回执与原永久轨迹一起只读核接续；尚未提交的临时回执不先写进facts来验证自身。
static func _continuity_for_new(state: Dictionary, _data: Dictionary, fresh: Dictionary, tick: int) -> Dictionary:
	var receipts: Array=[]
	for fact in state.facts.values():
		if _is_receipt(fact) and fact.tick<=tick: receipts.append(fact)
	receipts.append(fresh)
	return _continuity(state,receipts)

## 原动作起时不变且被替换计划不能复活；后续正位移承接终点与连续半日余量，间隙不猜补暂停或零位移信用。
static func _continuity(state: Dictionary, receipts: Array) -> Dictionary:
	receipts.sort_custom(func(a,b): return a.tick<b.tick)
	var previous: Dictionary={}; var plans: Dictionary={}
	for fact in receipts:
		var movement: Dictionary=fact.movement; var plan: Dictionary=state.facts[movement.plan_id]
		var key: String=fact.army_id+":"+movement.action_id
		var origin: Array=plan.origin; var first_index: int=1
		if plans.has(movement.plan_id) and (not previous.has(key) or previous[key].movement.plan_id!=movement.plan_id): return _fail("MilitaryMarchPlanReused")
		if previous.has(key):
			var prior: Dictionary=previous[key]
			if prior.movement.action_start_tick!=movement.action_start_tick: return _fail("MilitaryMarchActionStartMismatch")
			if prior.movement.to_point!=movement.from_point: return _fail("MilitaryMarchContinuityMismatch")
			if prior.movement.plan_id==movement.plan_id: origin=prior.movement.to_point; first_index=prior.movement.route_index_after
			if prior.tick==movement.source_tick and prior.movement.travel_credit_after!=movement.travel_credit_before: return _fail("MilitaryMarchCreditMismatch")
		if not plans.has(movement.plan_id) or (previous.has(key) and previous[key].movement.plan_id==movement.plan_id):
			if origin!=movement.from_point or movement.route_index_before<first_index: return _fail("MilitaryMarchContinuityMismatch")
			for index in range(first_index,movement.route_index_before):
				if plan.points[index]!=origin: return _fail("MilitaryMarchContinuityMismatch")
		plans[movement.plan_id]=true; previous[key]=fact
	return {"ok":true}

## 局部整数落点允许原旅行取整产生的一格误差，但不能向目标相反方向或越过未声明路点。
static func _on_leg(point: Array, start: Array, target: Array) -> bool:
	if point[0]<mini(start[0],target[0]) or point[0]>maxi(start[0],target[0]) or point[1]<mini(start[1],target[1]) or point[1]>maxi(start[1],target[1]): return false
	var dx: int=target[0]-start[0]; var dy: int=target[1]-start[1]
	return absi((point[0]-start[0])*dy-(point[1]-start[1])*dx)<=absi(dx)+absi(dy)

## 坐标必须是原世界持久整数范围，不接收浮点或布尔作为位移证据。
static func _point(value: Variant) -> bool:
	return value is Array and value.size()==2 and Values.whole(value[0],0,100000) and Values.whole(value[1],0,100000)

## 稳定身份列表只验真实字符串与唯一性，不排序调用者数据。
static func _strings(value: Variant) -> bool:
	if not value is Array: return false
	var seen: Dictionary={}
	for id in value:
		if not id is String or id.is_empty() or seen.has(id): return false
		seen[id]=true
	return true

## 即使kind被改坏，保留本规则标记的回执也必须进严格检查，不能默默跳过。
static func _is_receipt(value: Variant) -> bool:
	return value is Dictionary and ((value.get("kind") is String and value.kind==FACT_KIND) or (value.get("rule") is String and value.rule==MODEL))

## 回执沿原military实例空间分配，读档不接受自行发明的无界编号。
static func _instance(id: Variant, next_id: Variant) -> bool:
	if not id is String or not id.begins_with("military:") or not Values.whole(next_id,1): return false
	var number: String=id.substr(9)
	return number.is_valid_int() and str(number.to_int())==number and number.to_int()>0 and number.to_int()<next_id

## 稳定排序只用于本次验证，不改变名册或来源字典的顺序。
static func _ids(value: Dictionary) -> Array:
	return _sorted(value.keys())

## 返回独立排序副本，避免纯查询修改永久军团清单。
static func _sorted(value: Array) -> Array:
	var result: Array=value.duplicate(); result.sort(); return result

## 未启用或确实无正位移时不分配ID、不写空事实、不改变任何profile。
static func _unchanged() -> Dictionary:
	return {"ok":true,"receipt_id":"","already_completed":false}

## 每个坏来源都返回结构化错误，交原Kernel丢弃候选而非吞错补账。
static func _fail(code: String, path: String="") -> Dictionary:
	return {"ok":false,"code":code,"path":path}
