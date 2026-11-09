## 军团沿同一连续地形轨迹实际接敌；无行动停驻者只有真实位置，不生成旅行或另计半日。
extends RefCounted
const Journey=preload("res://core/terrain_journey.gd")
const Contact=preload("res://core/terrain_contact.gd")
const Terrain=preload("res://core/world_terrain.gd")
const Factory=preload("res://core/world_factory.gd")
const Combat=preload("res://core/military_combat.gd")
const March=preload("res://core/domains/military/march_conditions.gd")

## 同时读取真实旅行与无行动的原野停驻；无权、已在战和双方都不动时不会反复建立战斗。
static func detect(state: Dictionary, data: Dictionary, hold: Array) -> Array:
	var map_id: String=Journey.map_id(state,data)
	if map_id.is_empty() or not data.get("world_maps",{}).has(map_id): return []
	var traces: Array=[]; var armies: Dictionary={}
	for id in _ids(state.get("armies",{})):
		var army: Dictionary=state.armies[id]
		if army.status!="active" or army.commander_actor_id in hold or _engaged(state,army) or not _fit(state,army): continue
		var person: Dictionary=state.actors.get(army.commander_actor_id,{})
		if not person.get("alive",false) or not person.get("available",false): continue
		var trace: Dictionary={}
		if person.action_id.is_empty(): trace=Contact.stationary(person.id,person.location)
		else:
			var action: Dictionary=state.actions.get(person.action_id,{})
			if action.get("kind")!="travel" or action.get("actor_id")!=person.id or not action.params.get("terrain",false): continue
			trace=Journey.motion(state,data,action,hold)
		if trace.is_empty() or trace.map_id!=map_id or not Contact.validate_trace(trace).ok: continue
		if armies.has(person.id): return []
		traces.append(trace); armies[person.id]=army
	var entrances: Dictionary=_entrances(data,map_id); var candidates: Array=[]
	for a in range(traces.size()):
		for b in range(a+1,traces.size()):
			if not hostile(state,data,armies[traces[a].actor_id],armies[traces[b].actor_id]) or _already_met(state,armies,traces[a],traces[b]): continue
			var hit: Dictionary=Contact.intersection(traces[a],traces[b],entrances)
			if hit.is_empty() or not _anchors_legal(state,data,hit): continue
			hit.source_tick=state.tick; hit.motions={traces[a].actor_id:traces[a].duplicate(true),traces[b].actor_id:traces[b].duplicate(true)}
			hit.army_ids={hit.a:armies[hit.a].id,hit.b:armies[hit.b].id}
			candidates.append(hit)
	candidates.sort_custom(Contact.before)
	var result: Array=[]; var used: Array=[]
	for hit in candidates:
		if hit.a in used or hit.b in used: continue
		result.append(hit); used.append(hit.a); used.append(hit.b)
	return result

## 只有执行中的真实征讨军令指向对方辖地才敌对，补给或普通异组织来客不自动开战。
static func hostile(state: Dictionary, data: Dictionary, a: Dictionary, b: Dictionary) -> bool:
	return not _aggressor(state,data,a,b).is_empty()

## 征讨来源同时决定场地与攻守次序，不能因人物ID排序把供给军令当成进攻来源。
static func _aggressor(state: Dictionary, data: Dictionary, a: Dictionary, b: Dictionary) -> String:
	if a.organization_id==b.organization_id: return ""
	for pair in [[a,b],[b,a]]:
		var campaign: Dictionary=state.get("campaigns",{}).get(pair[0].get("order_id",""),{})
		var definition: Dictionary=data.get("military_orders",{}).get(campaign.get("template_id",""),{})
		var target: Dictionary=data.get("strongholds",{}).get(definition.get("target_stronghold_id",""),{})
		var owner: String=state.get("strongholds",{}).get(target.get("id",""),{}).get("owner_organization_id",data.get("garrisons",{}).get(target.get("garrison_id",""),{}).get("organization_id",""))
		if campaign.get("army_id")==pair[0].id and campaign.get("status") in ["marching","operating"] and definition.get("objective",{}).get("kind")=="occupy" and owner==pair[1].organization_id: return pair[0].id
	return ""

## 当前旅行已与同一停驻军团接战时读取永久来源去重；不借虚构action填入旅程记录。
static func _already_met(state: Dictionary, armies: Dictionary, a: Dictionary, b: Dictionary) -> bool:
	if not a.get("stationary",false) and not b.get("stationary",false): return false
	var moving: Dictionary=b if a.get("stationary",false) else a
	var still: Dictionary=a if a.get("stationary",false) else b
	if moving.action_id.is_empty(): return false
	for fact in state.facts.values():
		if not fact is Dictionary or fact.get("kind")!="military_contact": continue
		var sources: Variant=fact.get("motion_sources",{})
		if not sources is Dictionary: continue
		if sources.get(armies[moving.actor_id].id,{}).get("action_id")==moving.action_id and sources.has(armies[still.actor_id].id): return true
	return false

## 安装只提交完整成功候选；旅程费与军粮必须已由本半日统一支付，失败不留下锚点或幽灵战场。
static func install(state: Dictionary, data: Dictionary, hit: Dictionary, executions: Dictionary={}) -> Dictionary:
	var checked: Dictionary=_validate_hit(state,data,hit)
	if not checked.ok: return checked
	if March.enabled(state,data):
		checked=_validate_executions(state,hit,executions)
		if not checked.ok: return checked
	var next: Dictionary=state.duplicate(true)
	var result: Dictionary=_install(next,data,hit,executions)
	if not result.ok: return result
	var keys: Array=["actors","actions","facts","battles","next_instance_id"]
	if March.enabled(state,data): keys.append("military_units")
	for key in keys: state[key]=next[key]
	return result

## 以已核验真实来源安装双方战场；只截断存在的旅行，静止者位置和空action完整保留。
static func _install(state: Dictionary, data: Dictionary, hit: Dictionary, executions: Dictionary={}) -> Dictionary:
	var first: Dictionary=state.armies[hit.army_ids[hit.a]]; var second: Dictionary=state.armies[hit.army_ids[hit.b]]
	var aggressor: String=_aggressor(state,data,first,second)
	if aggressor.is_empty(): return _fail("MilitaryContactNotHostile")
	var people: Array=[hit.a,hit.b] if first.id==aggressor else [hit.b,hit.a]
	var armies: Array=[]; var units: Array=[]; var sides: Dictionary={}; var orders: Dictionary={}; var locations: Dictionary={}; var sources: Dictionary={}
	for person in people:
		var army: Dictionary=state.armies[hit.army_ids[person]]; var trace: Dictionary=hit.traces[person]
		if not trace.get("stationary",false):
			var action: Dictionary=state.actions[trace.action_id]
			action.params=trace.params.duplicate(true)
			var peer: String=hit.b if person==hit.a else hit.a
			var peer_action: String=hit.traces[peer].action_id
			if not peer_action.is_empty() and not peer_action in action.params.met_actions: action.params.met_actions.append(peer_action)
			state.actors[person].location=hit.anchors[person].duplicate(true)
		sides[army.id]=armies.size(); armies.append(army.id); orders[army.id]=army.order_id
		locations[army.id]=state.actors[person].location.duplicate(true)
		sources[army.id]={"actor_id":person,"action_id":trace.action_id,"plan_id":trace.params.get("plan_id",""),"stationary":trace.get("stationary",false),"location":locations[army.id].duplicate(true)}
		# 新条件回执还须绑定真正截断的路点序号，回环同坐标不能伪装成后一个前缀。
		if March.enabled(state,data) and not trace.get("stationary",false): sources[army.id].route_index=trace.params.route_index
		for unit_id in army.unit_ids:
			if not state.military_units[unit_id].fit.is_empty(): units.append(unit_id)
	var campaign: Dictionary=state.campaigns[orders[armies[0]]]
	var target: Dictionary=data.strongholds[data.military_orders[campaign.template_id].target_stronghold_id]
	var topology: Dictionary=data.get("siege_topologies",{}).get(target.get("topology_id",""),{})
	var field: String=""
	for node in topology.get("nodes",[]):
		if node.id==topology.get("attacker_entry_id"): field=node.battlefield_id
	if not data.get("battlefields",{}).has(field): return _fail("MilitaryContactBattlefieldRequired")
	var objective: Dictionary={"id":data.battlefields[field].objective_id,"unit_ids":[]}
	var source: String=Factory.next_id(state,"contact")
	state.facts[source]={"kind":"military_contact","tick":state.tick+1,"battlefield_id":field,"unit_ids":units.duplicate(),"side_by_army":sides.duplicate(),"objective":objective.duplicate(true),"army_order_ids":orders,"army_locations":locations,"time_num":hit.time_num,"time_den":hit.time_den,"motion_sources":sources}
	if March.enabled(state,data):
		for person in people:
			if not _retained_execution_moves(state,hit,person): continue
			var execution: Dictionary=executions[person].duplicate(true)
			execution.to_point=hit.anchors[person].point.duplicate()
			execution.route_index_after=hit.traces[person].params.route_index
			execution.travel_credit_after=hit.traces[person].params.travel_credit
			execution.result_location=state.actors[person].location.duplicate(true)
			var marched: Dictionary=March.apply_execution(state,data,execution,state.tick+1,source,executions[person])
			if not marched.ok: return marched
	return Combat.create(state,data,{"source_id":source,"battlefield_id":field,"unit_ids":units,"side_by_army":sides,"objective":objective})

## 原接敌证明已确认真实整步；再把显式捕获绑定到同一写入结果，暂停/滞留一侧没有票据仍可正常接敌。
static func _validate_executions(state: Dictionary, hit: Dictionary, executions: Dictionary) -> Dictionary:
	for person in [hit.a,hit.b]:
		var record: Variant=executions.get(person)
		if record==null:
			if _retained_execution_moves(state,hit,person): return _fail("MilitaryMarchExecutionRequired")
			continue
		var motion: Dictionary=hit.motions[person]
		if not record is Dictionary or motion.get("stationary",false): return _fail("MilitaryMarchExecutionMismatch")
		var action: Dictionary=state.actions[motion.action_id]; var p: Dictionary=motion.params
		var start: Array=motion.segments[0].from
		var expected: Dictionary={"format":"HM-TERRAIN-EXECUTION/1","actor_id":person,"action_id":action.id,"action_start_tick":action.start_tick,"source_tick":state.tick,"plan_id":p.plan_id,"map_id":motion.map_id,"from_point":[roundi(start[0]),roundi(start[1])],"to_point":motion.end,"route_index_before":p.route_index,"route_index_after":p.route_index+motion.reached_points,"travel_credit_before":p.get("travel_credit",0),"travel_credit_after":motion.end_credit,"result_location":state.actors[person].location}
		if record!=expected: return _fail("MilitaryMarchExecutionMismatch")
		for key in expected:
			if typeof(record.get(key))!=typeof(expected[key]): return _fail("MilitaryMarchExecutionMismatch")
	return {"ok":true}

## 只取接敌保留下来的整数前缀；time零、原地预算与量化回原点都不凭moving标记增加部队疲劳。
static func _retained_execution_moves(state: Dictionary, hit: Dictionary, person: String) -> bool:
	if hit.time_num==0: return false
	var motion: Dictionary=hit.motions[person]
	if motion.get("stationary",false): return false
	var start: Array=motion.segments[0].from
	var plan: Dictionary=state.facts[motion.params.plan_id]
	var crossed: Array=plan.points.slice(motion.params.route_index,hit.traces[person].params.route_index)
	return Journey.retained_movement([roundi(start[0]),roundi(start[1])],crossed,hit.anchors[person].point)

## 重新计算同源相交并比对真实军团、行旅依据及实际本步终点，拒绝过期观察和伪造停驻位置。
static func _validate_hit(state: Dictionary, data: Dictionary, hit: Dictionary) -> Dictionary:
	if hit.get("source_tick")!=state.tick or not hit.get("a") is String or not hit.get("b") is String or hit.a==hit.b or not hit.get("motions") is Dictionary or not hit.get("army_ids") is Dictionary: return _fail("InvalidMilitaryContactMotion")
	var armies: Dictionary={}
	for person in [hit.a,hit.b]:
		var army: Dictionary=state.armies.get(hit.army_ids.get(person,""),{})
		var actor: Dictionary=state.actors.get(person,{})
		var motion: Variant=hit.motions.get(person,{})
		if not motion is Dictionary: return _fail("InvalidMilitaryContactMotion")
		if army.get("status")!="active" or army.get("commander_actor_id")!=person or not actor.get("alive",false) or not actor.get("available",false) or _engaged(state,army) or not _fit(state,army): return _fail("MilitaryContactArmyUnavailable")
		if army.get("last_supply_tick")!=state.tick+1: return _fail("MilitaryContactHalfDayRequired")
		if not Contact.validate_trace(motion).ok or motion.actor_id!=person: return _fail("InvalidMilitaryContactMotion")
		armies[person]=army
		if motion.get("stationary",false):
			if not actor.action_id.is_empty() or actor.location!=motion.location: return _fail("MilitaryStationarySourceChanged")
		else:
			var valid: Dictionary=_travel_source(state,data,actor,motion)
			if not valid.ok: return valid
	var actual: Dictionary=Contact.intersection(hit.motions[hit.a],hit.motions[hit.b],_entrances(data,Journey.map_id(state,data)))
	if actual.is_empty(): return _fail("MilitaryContactGeometryMismatch")
	for key in ["a","b","time_num","time_den","anchors","traces"]:
		if actual[key]!=hit.get(key): return _fail("MilitaryContactGeometryMismatch")
	if not _anchors_legal(state,data,actual) or _already_met(state,armies,hit.motions[hit.a],hit.motions[hit.b]): return _fail("MilitaryContactAlreadyUsed")
	return {"ok":true}

## 行旅必须仍引用真实冻结计划且已走完同源本步；回放原观察只用于核验，不再次扣钱、移动或加预算。
static func _travel_source(state: Dictionary, data: Dictionary, actor: Dictionary, motion: Dictionary) -> Dictionary:
	var action: Dictionary=state.actions.get(motion.action_id,{})
	if actor.action_id!=motion.action_id or action.get("kind")!="travel" or action.get("actor_id")!=actor.id or not action.params.get("terrain",false): return _fail("MilitaryTravelSourceChanged")
	var p: Dictionary=motion.params; var fact: Dictionary=state.facts.get(p.get("plan_id",""),{})
	if fact.get("kind")!="terrain_journey" or fact.get("actor_id")!=actor.id or fact.get("action_id")!=action.id or fact.get("map_id")!=motion.map_id or fact.get("mode")!=p.get("mode") or fact.get("budget")!=p.get("budget") or fact.get("destination")!=p.get("destination") or not fact.get("points") is Array or not p.get("route_index") is int: return _fail("MilitaryTravelPlanRequired")
	if p.route_index<1 or p.route_index>=fact.points.size() or p.points!=fact.points.slice(p.route_index): return _fail("MilitaryTravelPlanRequired")
	for key in ["plan_id","map_id","origin","mode","budget","payer_wallet_id","money_per_tick","fatigue_per_tick","paused","encounter_id","met_actions"]:
		if p.get(key)!=action.params.get(key): return _fail("MilitaryTravelSourceChanged")
	if Journey.point(state,data,actor.id)!=motion.get("end"): return _fail("MilitaryTravelStepRequired")
	if motion.get("moving",false):
		if action.params.points!=p.points.slice(motion.reached_points) or action.params.route_index!=p.route_index+motion.reached_points or action.params.travel_credit!=motion.end_credit: return _fail("MilitaryTravelStepRequired")
	elif action.params!=p: return _fail("MilitaryTravelSourceChanged")
	var before: Dictionary=state.duplicate(true); var start: Array=motion.segments[0].from
	if start[0]!=roundi(start[0]) or start[1]!=roundi(start[1]): return _fail("MilitaryTravelOriginMismatch")
	before.actors[actor.id].location={"kind":"Wilderness","map_id":motion.map_id,"point":[roundi(start[0]),roundi(start[1])],"origin":motion.origin}
	before.actions[action.id].params=p.duplicate(true)
	if motion.get("moving",false): before.wallets[p.payer_wallet_id].balance+=p.money_per_tick
	if Journey.motion(before,data,before.actions[action.id])!=motion: return _fail("MilitaryTravelGeometryMismatch")
	return {"ok":true}

## 锚点落地与余程仍走同一地形权限；静止者只有本人的合法军团落点，没有需虚构的下一路点。
static func _anchors_legal(state: Dictionary, data: Dictionary, hit: Dictionary) -> bool:
	for person in [hit.a,hit.b]:
		var trace: Dictionary=hit.traces[person]; var anchor: Dictionary=hit.anchors[person]
		var map: Dictionary=data.get("world_maps",{}).get(anchor.map_id,{})
		var mode: String="army" if trace.get("stationary",false) else trace.params.mode
		if not Terrain.point_valid(map,anchor.point,mode,state.flags): return false
		if not trace.get("stationary",false) and not Terrain.segment_cost(map,anchor.point,trace.params.points[0],mode,{}).ok: return false
	return true

## 入口圆排除共用连续几何，城内交互不被第二套近距判定截走。
static func _entrances(data: Dictionary, map_id: String) -> Dictionary:
	var points: Array=[]
	for place in data.settlements.values():
		if place.has("map_xy"): points.append(place.map_xy)
	return {map_id:points}

## 已在规划或结果界面的真实部队暂不参加另一接敌；关闭后仍按永久本次旅行来源去重。
static func _engaged(state: Dictionary, army: Dictionary) -> bool:
	for battle in state.get("battles",{}).values():
		if battle.get("phase")=="closed": continue
		for id in army.get("unit_ids",[]):
			if battle.get("units",{}).has(id): return true
	return false

## 至少一部有实际有效兵员才能接敌；不会只凭军团名称生成空军队。
static func _fit(state: Dictionary, army: Dictionary) -> bool:
	for id in army.get("unit_ids",[]):
		var unit: Dictionary=state.get("military_units",{}).get(id,{})
		if unit.get("army_id")==army.get("id") and not unit.get("fit",[]).is_empty() and unit.get("morale",0)>0: return true
	return false

## 来源表排序只用于稳定选择，不改变世界字典的写入顺序。
static func _ids(table: Dictionary) -> Array:
	var ids: Array=table.keys(); ids.sort(); return ids

## 接敌失败明确返回稳定代码，调用方不提交任何部分战场或位置。
static func _fail(code: String) -> Dictionary:
	return {"ok":false,"code":code}
