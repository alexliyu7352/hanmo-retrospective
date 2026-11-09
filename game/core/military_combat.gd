## 真实军团与六角会战的领域桥接：命令只保存计划，逐半日战损调用人产模块；位置、口粮和世界时钟仍归统一调度。
extends RefCounted
const Fortification=preload("res://core/domains/siege/fortification_context.gd")
const Round=preload("res://core/military_round.gd")
const Hex=preload("res://core/military_hex.gd")
const Assets=preload("res://core/military_assets.gd")
const Factory=preload("res://core/world_factory.gd")
const KINDS=["military_plan","military_close"]
const TERMINATIONS=["authority_lost","responsible_actor_lost","truce","cancelled","timeout"]
const GROUPS=["fit","wounded","dead","captured","deserted"]

## 只有永久接敌来源能建立会战；全部引用、出生格与权限先通过，再在私有世界候选中分配唯一会话编号。
static func create(state: Dictionary, data: Dictionary, request: Dictionary) -> Dictionary:
	if request.has("controller_actor_id") and not request.controller_actor_id is String: return _fail("InvalidMilitaryController")
	var source: Variant=state.get("facts",{}).get(request.get("source_id"))
	var checked: Dictionary=_contact(state,data,request,source,true)
	if not checked.ok: return checked
	var units: Dictionary={}; var armies: Array=[]
	for id in _sorted(request.unit_ids):
		var actual: Dictionary=state.military_units[id]; var army: Dictionary=state.armies[actual.army_id]
		if actual.fit.is_empty() or actual.morale<=0: return _fail("MilitaryUnitUnavailable",id)
		if not actual.get("mobilizing_action_id","").is_empty() or not actual.get("training_action_id","").is_empty(): return _fail("MilitaryUnitReserved",id)
		if not army.id in armies: armies.append(army.id)
		var unit: Dictionary={"unit_id":id,"army_id":army.id,"commander_actor_id":actual.commander_actor_id,"troop_type_id":actual.troop_type_id,"side":int(request.side_by_army[army.id]),"position":[0,0],"heading":0,"morale":actual.morale,"tactic_points":data.troop_types[actual.troop_type_id].tactic_capacity,"statuses":{},"withdrawn":false,"member_ids":actual.member_ids.duplicate()}
		for group in GROUPS: unit[group]=actual[group].duplicate()
		units[id]=unit
	var controller: String=request.get("controller_actor_id",state.player_actor_id)
	if controller!=state.player_actor_id and not controller.is_empty(): return _fail("MilitaryControllerMustBePlayer")
	var authority: Dictionary=_authority(state,data,units,controller)
	if not authority.ok:
		if request.has("controller_actor_id"): return authority
		authority=_authority(state,data,units,"")
	var participants: Array=_participants(state,units,authority.controller_actor_id)
	for battle in state.get("battles",{}).values():
		if battle.get("phase")=="closed": continue
		if battle.get("source_id")==request.source_id: return _fail("MilitaryContactAlreadyUsed",battle.id)
		for id in units:
			if battle.get("units",{}).has(id): return _fail("MilitaryUnitAlreadyEngaged",id)
		for person in participants:
			if person in battle.get("participant_actor_ids",[]) or battle.get("units",{}).has(person): return _fail("MilitaryActorAlreadyEngaged",person)
	for person in participants:
		if not state.actors[person].get("alive",false) or not state.actors[person].get("available",false): return _fail("MilitaryCommanderUnavailable",person)
		var action_id: String=state.actors[person].get("action_id","")
		if not action_id.is_empty():
			var action: Dictionary=state.actions.get(action_id,{})
			if action.get("kind")!="travel" or (action.get("actor_id")!=person and not person in action.get("participants",[])): return _fail("MilitaryActorBusy",person)
	checked=_spawns(units,data.battlefields[request.battlefield_id],data)
	if not checked.ok: return checked
	var tick: int=maxi(state.tick,int(source.tick))
	var battle: Dictionary={"id":"pending","kind":"military","battlefield_id":request.battlefield_id,"objective_id":request.objective.id,"objective_unit_ids":request.objective.unit_ids.duplicate(),"phase":"planning","round":1,"units":units,"ai_plans":{},"objective_state":{"hold_rounds":0,"escaped_unit_ids":[]},"rng":{"seed":"0","state":"0"},"last_events":[],"result":{},"created_tick":tick,"last_tick":tick,"completed_tick":-1,"source_id":request.source_id,"army_ids":_sorted(armies),"participant_actor_ids":participants,"plan_submitted":false,"pending_orders":{},"round_receipt_ids":[],"settlement_receipt_id":"","applied":false}
	battle.merge(authority,true); battle.erase("ok")
	checked=Round.validate(battle,data)
	if not checked.ok: return checked
	var sources: Dictionary=_damage_sources(state,data,battle)
	if not sources.ok: return sources
	var ai: Dictionary=Round.make_ai(battle,data,sources.factors)
	if not ai.ok: return ai
	var id: String=Factory.next_id(state,"battle"); battle.id=id
	var rng=RandomNumberGenerator.new(); rng.seed=int(state.rng.battle.seed)^int(id.hash())^int(request.source_id.hash()); battle.rng={"seed":str(rng.seed),"state":str(rng.state)}
	battle.ai_plans=ai.orders; state.battles[id]=battle; source.battle_id=id
	return {"ok":true,"details":{"battle_id":id,"participant_actor_ids":participants.duplicate()}}

## 预览只验当前真实授权与整套合法命令；已经提交的计划不能用第二个提交覆盖本半日意图。
static func preview(state: Dictionary, data: Dictionary, actor_id: String, kind: String, params: Dictionary) -> Dictionary:
	if not kind in KINDS: return _fail("UnknownMilitaryCommand")
	var battle: Dictionary=state.get("battles",{}).get(params.get("battle_id"),{})
	if battle.get("kind")!="military": return _fail("UnknownMilitaryBattle")
	if battle.controller_actor_id!=actor_id or actor_id!=state.player_actor_id: return _fail("MilitaryCommanderUnauthorized",battle.id)
	if kind=="military_close":
		if battle.phase!="result" or not battle.applied or not state.facts.has(battle.id): return _fail("MilitaryResultNotSettled",battle.id)
		return {"ok":true,"battle_id":battle.id}
	if battle.phase!="planning": return _fail("MilitaryBattleNotPlanning",battle.id)
	if battle.plan_submitted: return _fail("MilitaryPlanAlreadySubmitted",battle.id)
	if not params.get("orders") is Dictionary: return _fail("InvalidMilitaryOrders",battle.id)
	var authority: Dictionary=_authority(state,data,battle.units,actor_id)
	if not authority.ok: return authority
	if not _same_authority(battle,authority): return _fail("MilitaryAuthorityChanged",battle.id)
	var checked: Dictionary=Round.validate_orders(battle,data,actor_id,params.orders)
	if not checked.ok: checked.battle_id=battle.id; return checked
	return {"ok":true,"battle_id":battle.id,"orders":checked.orders}

## 提交仅冻结计划，关闭仅释放已经落账的结果回顾；调用者统一决定是否推进世界半日。
static func apply(state: Dictionary, data: Dictionary, actor_id: String, kind: String, params: Dictionary) -> Dictionary:
	var quote: Dictionary=preview(state,data,actor_id,kind,params)
	if not quote.ok: return quote
	var battle: Dictionary=state.battles[quote.battle_id]
	if kind=="military_close": battle.phase="closed"
	else: battle.pending_orders=quote.orders.duplicate(true); battle.plan_submitted=true
	return {"ok":true,"details":{"battle_id":battle.id,"plan_submitted":battle.plan_submitted}}

## 半日任何写入之前检查全部当前战场，缺少玩家计划时返回具体会话供界面定位，不先扣其他军团口粮。
static func preview_step(state: Dictionary, data: Dictionary, finish_tick: int) -> Dictionary:
	return _preflight(state,data,finish_tick,false)

## 各军已计本半日口粮后消费全部旧会战一次，立即把真实伤亡落到人产；返回真人变化供根层统一写生命与位置。
static func step(state: Dictionary, data: Dictionary, finish_tick: int) -> Dictionary:
	var checked: Dictionary=_preflight(state,data,finish_tick,true)
	if not checked.ok: return checked
	var completed: Array=[]; var changed: Array=[]; var rounds: Array=[]
	for id in checked.battle_ids:
		var battle: Dictionary=state.battles[id]; var current: Dictionary=battle.duplicate(true)
		for unit_id in current.units: current.units[unit_id].morale=state.military_units[unit_id].morale
		var sources: Dictionary=_damage_sources(state,data,current)
		if not sources.ok: return sources
		var resolved: Dictionary=Round.resolve(current,data,current.controller_actor_id,current.pending_orders if current.plan_submitted else {},sources.factors)
		if not resolved.ok: resolved.battle_id=id; return resolved
		var next: Dictionary=resolved.battle; next.last_tick=finish_tick; next.plan_submitted=false; next.pending_orders={}
		var settled: Dictionary=_settle(state,data,next,finish_tick,battle.last_tick)
		if not settled.ok: return settled
		changed.append_array(settled.actor_changes); rounds.append(settled.round_fact_id)
		if next.phase=="result":
			completed.append(id)
			if next.controller_actor_id.is_empty(): next.phase="closed"
		state.battles[id]=next
	return {"ok":true,"completed_battle_ids":completed,"round_fact_ids":rounds,"actor_changes":changed}

## 军令失效或停战在稳定边界结束会话，保留已经逐轮落账的人数；根层继续实际返程，不撤销伤亡或移动人物。
static func terminate(state: Dictionary, data: Dictionary, battle_id: String, reason: String, finish_tick: int=-1) -> Dictionary:
	if not reason in TERMINATIONS: return _fail("UnknownMilitaryTermination")
	var battle: Dictionary=state.get("battles",{}).get(battle_id,{})
	if battle.get("kind")!="military": return _fail("UnknownMilitaryBattle",battle_id)
	if battle.phase=="closed": return {"ok":true,"already_completed":true,"actor_changes":[],"completed_battle_ids":[]}
	if battle.phase=="result":
		if not battle.applied or not state.facts.has(battle.settlement_receipt_id): return _fail("MilitaryResultNotSettled",battle_id)
		battle.phase="closed"; return {"ok":true,"actor_changes":[],"completed_battle_ids":[battle_id]}
	if battle.phase!="planning": return _fail("MilitaryBattleNotPlanning",battle_id)
	var tick: int=maxi(state.tick,battle.last_tick) if finish_tick<0 else finish_tick
	if tick<battle.last_tick or not tick in [state.tick,state.tick+1]: return _fail("MilitaryTerminationBoundary",battle_id)
	var next: Dictionary=battle.duplicate(true); next.phase="result"; next.result={"winner_side":-1,"reason":reason}; next.ai_plans={}; next.plan_submitted=false; next.pending_orders={}
	for id in next.units: next.units[id].morale=state.military_units[id].morale
	var settled: Dictionary=_settle(state,data,next,tick,tick)
	if not settled.ok: return settled
	next.phase="closed"; state.battles[battle_id]=next
	return {"ok":true,"actor_changes":settled.actor_changes,"completed_battle_ids":[battle_id]}

## 轮间与终局保存同时核验真实编组和永久落账来源；旧个人会话由各自领域检查，不在此重解释。
static func validate_state(state: Dictionary, data: Dictionary) -> Dictionary:
	if not state.get("battles") is Dictionary: return _fail("MissingMilitaryBattles")
	var occupied: Dictionary={}; var sources: Array=[]
	for id in _ids(state.battles):
		var battle: Variant=state.battles[id]
		if not battle is Dictionary: return _fail("InvalidMilitarySession",str(id))
		if battle.get("kind")!="military": continue
		var checked: Dictionary=_validate_battle(state,data,battle,false)
		if not checked.ok: return checked
		if battle.id!=id or battle.source_id in sources: return _fail("DuplicateMilitarySource",str(id))
		sources.append(battle.source_id)
		if battle.phase!="closed":
			for unit_id in battle.units:
				if occupied.has(unit_id): return _fail("MilitaryUnitAlreadyEngaged",unit_id)
				occupied[unit_id]=id
			for person in battle.participant_actor_ids:
				if occupied.has(person): return _fail("MilitaryActorAlreadyEngaged",person)
				occupied[person]=id
	return {"ok":true}

## 世界调度共享预检，供给之后只允许由本半日永久口粮账解释的士气变化；失败不执行任何场次。
static func _preflight(state: Dictionary, data: Dictionary, finish_tick: int, supplied: bool) -> Dictionary:
	if finish_tick!=state.tick+1: return _fail("MilitaryInvalidStep")
	var ids: Array=[]
	for id in _ids(state.get("battles",{})):
		var battle: Dictionary=state.battles[id]
		if battle.get("kind")!="military" or battle.phase!="planning" or battle.created_tick>state.tick: continue
		if battle.last_tick==finish_tick: continue
		if battle.last_tick!=state.tick: return _fail("MilitarySkippedStep",id)
		var checked: Dictionary=_validate_battle(state,data,battle,supplied)
		if not checked.ok: return checked
		var needed: bool=false
		for unit_id in battle.command_unit_ids:
			if not battle.units[unit_id].fit.is_empty() and not battle.units[unit_id].withdrawn: needed=true
		if needed and not battle.plan_submitted: return _fail("MilitaryPlanRequired",id)
		if battle.plan_submitted:
			checked=Round.validate_orders(battle,data,battle.controller_actor_id,battle.pending_orders)
			if not checked.ok: checked.battle_id=id; return checked
		ids.append(id)
	return {"ok":true,"battle_ids":ids}

## 永久接敌消息冻结真实军令、军团位置与节点来源；读档时历史消息保留，活动会话仍须符合当前在场事实。
static func _contact(state: Dictionary, data: Dictionary, request: Dictionary, source: Variant, live: bool) -> Dictionary:
	for field in request:
		if not field in ["source_id","battlefield_id","unit_ids","side_by_army","objective","controller_actor_id"]: return _fail("UnexpectedMilitaryContactField")
	for key in ["source_id","battlefield_id"]:
		if not request.get(key) is String: return _fail("InvalidMilitaryContact")
	if not source is Dictionary or source.get("kind")!="military_contact" or not Hex.whole(source.get("tick"),0,state.tick+1): return _fail("MilitaryContactSourceRequired")
	if not _unique(request.get("unit_ids")) or request.unit_ids.size()<2 or not request.get("side_by_army") is Dictionary or not request.get("objective") is Dictionary: return _fail("InvalidMilitaryContact")
	if not request.objective.get("id") is String or not _unique(request.objective.get("unit_ids")) or not data.get("military_objectives",{}).has(request.objective.id) or not data.get("battlefields",{}).has(request.battlefield_id): return _fail("UnknownMilitaryDefinition")
	if _ids(request.objective)!=["id","unit_ids"] or data.battlefields[request.battlefield_id].get("objective_id")!=request.objective.id: return _fail("MilitaryBattlefieldObjectiveMismatch")
	for field in ["battlefield_id","unit_ids","side_by_army","objective"]:
		if source.get(field)!=request[field]: return _fail("MilitaryContactMismatch")
	if not source.get("army_order_ids") is Dictionary or not source.get("army_locations") is Dictionary: return _fail("MilitaryContactArmySourceMissing")
	if live and source.has("battle_id"): return _fail("MilitaryContactAlreadyUsed",str(source.battle_id))
	var armies: Array=[]; var sides: Array=[]
	for id in request.unit_ids:
		var unit: Variant=state.get("military_units",{}).get(id)
		if not unit is Dictionary or not data.get("troop_types",{}).has(unit.get("troop_type_id")): return _fail("UnknownMilitaryUnit",id)
		if not state.actors.has(unit.get("commander_actor_id")) or not _unique(unit.get("member_ids")): return _fail("MilitaryActualRosterMissing",id)
		for group in GROUPS:
			if not _unique(unit.get(group)): return _fail("MilitaryActualRosterMissing",id)
		var army_id: String=unit.get("army_id","") if live else _source_army(state,source,id)
		var army: Dictionary=state.get("armies",{}).get(army_id,{})
		if army.is_empty() or not request.side_by_army.has(army_id) or not Hex.whole(request.side_by_army[army_id],0,1): return _fail("MilitaryContactArmyMismatch",id)
		if not army_id in armies: armies.append(army_id)
		if not int(request.side_by_army[army_id]) in sides: sides.append(int(request.side_by_army[army_id]))
		var campaign: Dictionary=state.get("campaigns",{}).get(army.get("order_id"),{})
		var formation: Dictionary=state.facts.get(army.get("formation_source"),{})
		if campaign.is_empty() or source.army_order_ids.get(army_id)!=campaign.id or campaign.get("army_id")!=army_id or formation.get("kind")!="military_action" or formation.get("command")!="army_form" or formation.get("phase")!="completed" or formation.get("success")!=true or formation.get("result",{}).get("army_id")!=army_id or not id in formation.get("terms",{}).get("unit_ids",[]): return _fail("MilitaryArmyFormationSourceMissing",id)
		if live:
			if army.get("status")!="active" or not campaign.get("status") in ["mustered","marching","operating","returning"] or not id in army.unit_ids: return _fail("MilitaryArmyUnavailable",id)
			if state.actors.get(army.get("commander_actor_id"),{}).get("location")!=source.army_locations.get(army_id): return _fail("MilitaryContactPositionChanged",army_id)
	if _sorted(armies)!=_ids(request.side_by_army) or _ids(source.army_order_ids)!=_sorted(armies) or _ids(source.army_locations)!=_sorted(armies) or sides.size()!=2: return _fail("MilitaryContactSidesMismatch")
	for army_id in armies:
		var location: Variant=source.army_locations[army_id]
		if not location is Dictionary or not location.get("kind") in ["Settlement","Wilderness"]: return _fail("MilitaryContactLocationRequired",army_id)
	for a in armies:
		for b in armies:
			if request.side_by_army[a]!=request.side_by_army[b] and state.armies[a].organization_id==state.armies[b].organization_id: return _fail("MilitaryContactOrganizationConflict")
	if source.has("siege_id") or source.has("node_id"):
		var siege: Dictionary=state.get("sieges",{}).get(source.get("siege_id"),{})
		if siege.is_empty() or not source.get("node_id") is String: return _fail("MilitarySiegeSourceMissing")
		if live:
			var pending: Dictionary=siege.get("pending_nodes",{}).get(source.node_id,{})
			if pending.get("source_id")!=request.source_id or pending.get("battlefield_id")!=request.battlefield_id or _sorted(pending.get("unit_ids",[]))!=_sorted(request.unit_ids): return _fail("MilitarySiegeSourceMismatch")
	return {"ok":true}

## 历史军团可能已经归营，依永久编组名单恢复当时部队所属，不要求它永远留在旧军团。
static func _source_army(state: Dictionary, source: Dictionary, unit_id: String) -> String:
	for id in source.army_order_ids:
		var army: Dictionary=state.armies.get(id,{})
		if unit_id in state.facts.get(army.get("formation_source"),{}).get("terms",{}).get("unit_ids",[]): return id
	return ""

## 当前公职只授权真实统帅的本军；没有军级权限的真实部队指挥者仅能控制本人一部，其他同侧照常使用AI。
static func _authority(state: Dictionary, data: Dictionary, units: Dictionary, controller: String) -> Dictionary:
	var result: Dictionary={"ok":true,"controller_actor_id":controller,"controller_side":0,"command_scope":"unit","command_army_id":"","command_unit_ids":[],"authority_office_id":""}
	if controller.is_empty(): return result
	var actor: Dictionary=state.actors.get(controller,{})
	if actor.is_empty() or not actor.get("alive",false) or not actor.get("available",false): return _fail("MilitaryCommanderUnavailable")
	for id in _ids(units):
		var unit: Dictionary=units[id]; var army: Dictionary=state.armies[unit.army_id]
		var campaign: Dictionary=state.campaigns[army.order_id]
		if army.commander_actor_id!=controller: continue
		var office: Dictionary=Assets.ready_office(state,data,controller,campaign.office_id)
		if office.ok and office.command_scope=="army" and office.organization_id==army.organization_id:
			result.controller_side=unit.side; result.command_scope="army"; result.command_army_id=army.id; result.authority_office_id=campaign.office_id
			for owned in _ids(units):
				if units[owned].army_id==army.id: result.command_unit_ids.append(owned)
			return result
	for id in _ids(units):
		if units[id].commander_actor_id==controller:
			result.controller_side=units[id].side; result.command_unit_ids=[id]; return result
	return _fail("MilitaryCommanderNotPresent")

## 出生格按作者阵营与兵种通行实际匹配；可交换先前匹配以避免宽通行兵种占走唯一可用格。
static func _spawns(units: Dictionary, board: Dictionary, data: Dictionary) -> Dictionary:
	var checked: Dictionary=Hex.validate(board)
	if not checked.ok: return checked
	var assigned: Dictionary={}
	for id in _ids(units):
		if not _assign_spawn(id,units,board,data,assigned,[]): return _fail("MilitaryBattlefieldCapacity",id)
	for slot in assigned:
		var id: String=assigned[slot]; units[id].position=board.spawns[slot].position.duplicate()
		units[id].heading=0 if units[id].side==0 else 3
	return {"ok":true}

## 有界增广匹配只访问既有合法出生格，失败不把部队塞入障碍或自动扩建战场。
static func _assign_spawn(id: String, units: Dictionary, board: Dictionary, data: Dictionary, assigned: Dictionary, visited: Array) -> bool:
	for index in range(board.spawns.size()):
		var spawn: Dictionary=board.spawns[index]
		if index in visited or spawn.side!=units[id].side or Hex.movement_cost(board,spawn.position,data.troop_types[units[id].troop_type_id])<0: continue
		visited.append(index)
		if not assigned.has(index) or _assign_spawn(assigned[index],units,board,data,assigned,visited): assigned[index]=id; return true
	return false

## 参战真人仅取实际部队将领与仍随本部生存的成员，不能因打开会战获得遥远同伴的指挥权。
static func _participants(state: Dictionary, units: Dictionary, controller: String) -> Array:
	var result: Array=[]
	for unit in units.values():
		for id in [unit.commander_actor_id]+unit.fit+unit.wounded:
			if state.actors.has(id) and not id in result: result.append(id)
	if not controller.is_empty() and not controller in result: result.append(controller)
	return _sorted(result)

## 每轮先记永久军事结果再由既有人产账本结算，新俘虏必须有本轮真实溃退事件明确指定的看管军团。
static func _settle(state: Dictionary, data: Dictionary, battle: Dictionary, tick: int, start_tick: int) -> Dictionary:
	var final: bool=battle.phase=="result"
	var id: String=battle.id if final else Factory.next_id(state,"battle_round")
	if state.facts.has(id): return _fail("MilitaryResultAlreadyApplied",battle.id)
	var captors: Dictionary={}
	for event in battle.last_events:
		if event.get("kind")!="rout": continue
		for person in event.get("captured_ids",[]):
			if not person in state.military_units[event.unit_id].captured: captors[person]=event.captor_army_id
	state.facts[id]={"kind":"military_battle","phase":"completed","tick":tick,"start_tick":start_tick,"session_id":battle.id,"round":battle.round if final else battle.round-1,"army_ids":battle.army_ids.duplicate(),"unit_ids":_ids(battle.units),"result":battle.result.duplicate(true),"source_id":battle.source_id if final else id,"contact_source_id":battle.source_id,"winner_side":battle.result.get("winner_side",-1),"final":final}
	var settled: Dictionary=Assets.settle_battle(state,data,id,battle.units,captors)
	if not settled.ok: return settled
	battle.round_receipt_ids.append(settled.receipt_id)
	if final: battle.applied=true; battle.completed_tick=tick; battle.settlement_receipt_id=settled.receipt_id
	return {"ok":true,"round_fact_id":id,"actor_changes":settled.actor_changes}

## 当前世界名册、保存命令和历史收据共同约束会话，读档不能复活死人或再次领取同一战果。
static func _validate_battle(state: Dictionary, data: Dictionary, battle: Dictionary, supplied: bool) -> Dictionary:
	for field in ["source_id","settlement_receipt_id","authority_office_id"]:
		if not battle.get(field) is String: return _fail("InvalidMilitarySession",str(battle.get("id","")))
	for field in ["army_ids","participant_actor_ids","round_receipt_ids"]:
		if not _unique(battle.get(field)): return _fail("InvalidMilitarySession",str(battle.get("id","")))
	if not battle.get("plan_submitted") is bool or not battle.get("applied") is bool or not battle.get("pending_orders") is Dictionary or not battle.get("phase") in ["planning","result","closed"]: return _fail("InvalidMilitarySession")
	if not Hex.whole(battle.get("created_tick"),0,state.tick) or not Hex.whole(battle.get("last_tick"),battle.created_tick,state.tick) or not Hex.whole(battle.get("completed_tick"),-1,state.tick): return _fail("InvalidMilitarySessionTime",battle.id)
	var copy: Dictionary=battle.duplicate(true)
	if copy.phase=="closed": copy.phase="result"
	var checked: Dictionary=Round.validate(copy,data,true)
	if not checked.ok: checked.battle_id=battle.id; return checked
	var source: Variant=state.facts.get(battle.source_id)
	var request: Dictionary={"source_id":battle.source_id,"battlefield_id":battle.battlefield_id,"unit_ids":source.get("unit_ids",[]) if source is Dictionary else [],"side_by_army":source.get("side_by_army",{}) if source is Dictionary else {},"objective":{"id":battle.objective_id,"unit_ids":battle.objective_unit_ids}}
	checked=_contact(state,data,request,source,false)
	if not checked.ok: return checked
	if source.get("battle_id")!=battle.id or _sorted(request.unit_ids)!=_ids(battle.units) or _ids(request.side_by_army)!=battle.army_ids: return _fail("MilitaryContactMismatch",battle.id)
	var people: Array=[]
	for id in battle.units:
		var unit: Dictionary=battle.units[id]
		if unit.army_id!=_source_army(state,source,id) or unit.side!=request.side_by_army[unit.army_id]: return _fail("MilitaryContactSidesMismatch",battle.id)
		for person in [unit.commander_actor_id]+unit.member_ids:
			if state.actors.has(person) and not person in people: people.append(person)
		if not unit.commander_actor_id in battle.participant_actor_ids: return _fail("MilitaryParticipantMissing",battle.id)
	for person in battle.participant_actor_ids:
		if not person in people and person!=battle.controller_actor_id: return _fail("MilitaryParticipantMismatch",battle.id)
	if not battle.controller_actor_id.is_empty() and not battle.controller_actor_id in battle.participant_actor_ids: return _fail("MilitaryParticipantMissing",battle.id)
	checked=_round_history(state,battle)
	if not checked.ok: return checked
	if battle.phase=="planning":
		if battle.applied or battle.completed_tick!=-1 or not battle.settlement_receipt_id.is_empty() or battle.round!=battle.round_receipt_ids.size()+1 or battle.last_tick!=state.tick or battle.last_tick-battle.created_tick!=battle.round_receipt_ids.size(): return _fail("InvalidMilitarySettlement",battle.id)
		var authority: Dictionary=_authority(state,data,battle.units,battle.controller_actor_id)
		if not authority.ok or not _same_authority(battle,authority): return _fail("MilitaryAuthorityChanged",battle.id)
		for id in battle.units:
			var unit: Dictionary=battle.units[id]; var actual: Dictionary=state.military_units[id]
			if actual.army_id!=unit.army_id or not state.armies[unit.army_id].status=="active" or actual.commander_actor_id!=unit.commander_actor_id or actual.member_ids!=unit.member_ids or actual.troop_type_id!=unit.troop_type_id: return _fail("MilitaryActualUnitChanged",id)
			for group in GROUPS:
				if actual[group]!=unit[group]: return _fail("MilitaryActualRosterChanged",id)
			if supplied:
				var food: Dictionary=_supply(state,unit.army_id,state.tick+1)
				if food.is_empty() or state.armies[unit.army_id].last_supply_tick!=state.tick+1: return _fail("MilitarySupplyRequired",battle.id)
				var expected: int=maxi(1,unit.morale-8) if food.shortage>0 and not unit.fit.is_empty() else unit.morale
				if actual.morale!=expected: return _fail("MilitarySupplyMoraleMismatch",id)
			elif actual.morale!=unit.morale: return _fail("MilitaryActualMoraleChanged",id)
			if state.actors[state.armies[unit.army_id].commander_actor_id].location!=source.army_locations[unit.army_id]: return _fail("MilitaryContactPositionChanged",battle.id)
	else:
		if not battle.applied or battle.completed_tick<0 or battle.round_receipt_ids.is_empty() or battle.round!=battle.round_receipt_ids.size() or battle.settlement_receipt_id!=battle.round_receipt_ids[-1] or state.facts.get(battle.id,{}).get("source_id")!=battle.source_id: return _fail("MilitaryResultNotSettled",battle.id)
		var result: Dictionary=state.facts[battle.id]
		if result.get("phase")!="completed" or result.get("final")!=true or result.get("result")!=battle.result or result.get("winner_side")!=battle.result.winner_side or result.get("tick")!=battle.completed_tick: return _fail("MilitaryResultSourceMismatch",battle.id)
		var settled: Dictionary=state.facts[battle.settlement_receipt_id]
		if settled.get("battle_id")!=battle.id: return _fail("MilitaryResultSourceMismatch",battle.id)
		for id in battle.units:
			for group in GROUPS:
				if settled.units[id][group]!=battle.units[id][group]: return _fail("MilitaryResultRosterMismatch",id)
	if not battle.plan_submitted and not battle.pending_orders.is_empty(): return _fail("MilitaryUnexpectedPendingOrders",battle.id)
	if battle.phase!="planning" and battle.plan_submitted: return _fail("MilitaryUnexpectedPendingOrders",battle.id)
	if battle.plan_submitted:
		checked=Round.validate_orders(battle,data,battle.controller_actor_id,battle.pending_orders)
		if not checked.ok: return checked
	return {"ok":true}

## 按原结算器的永久来源逐轮核对连续半日和终态；外部停止保留零攻击边界，不重放AI或修补缺失历史。
static func _round_history(state: Dictionary, battle: Dictionary) -> Dictionary:
	var previous_tick: int=battle.created_tick
	var sources: Dictionary={}
	var unit_ids: Array=_ids(battle.units)
	for index in range(battle.round_receipt_ids.size()):
		var receipt: Variant=state.facts.get(battle.round_receipt_ids[index])
		if not receipt is Dictionary or not receipt.get("kind") is String or receipt.kind!="military_casualties" or not receipt.get("battle_id") is String or not receipt.get("units") is Dictionary or _ids(receipt.units)!=unit_ids: return _fail("MilitarySettlementSourceMissing",battle.id)
		var source_id: String=receipt.battle_id
		var fact: Variant=state.facts.get(source_id)
		if sources.has(source_id) or not fact is Dictionary: return _fail("MilitarySettlementSourceMissing",battle.id)
		for field in ["kind","phase","session_id","contact_source_id","source_id"]:
			if not fact.get(field) is String: return _fail("MilitarySettlementSourceMissing",battle.id)
		if fact.kind!="military_battle" or fact.phase!="completed" or fact.session_id!=battle.id: return _fail("MilitarySettlementSourceMissing",battle.id)
		if not _unique(fact.get("army_ids")) or not _unique(fact.get("unit_ids")): return _fail("MilitarySettlementSourceMismatch",battle.id)
		sources[source_id]=true
		var final: bool=index==battle.round_receipt_ids.size()-1 and battle.phase!="planning"
		if not fact.get("final") is bool or fact.final!=final or not Hex.whole(fact.get("round"),index+1,index+1): return _fail("MilitarySettlementRoundMismatch",battle.id)
		if fact.get("army_ids")!=battle.army_ids or fact.get("unit_ids")!=unit_ids or fact.get("contact_source_id")!=battle.source_id or fact.get("source_id")!=(battle.source_id if final else source_id) or (final and source_id!=battle.id): return _fail("MilitarySettlementSourceMismatch",battle.id)
		if not fact.get("result") is Dictionary or fact.result!=(battle.result if final else {}) or not Hex.whole(fact.get("winner_side"),-1,1) or fact.winner_side!=(battle.result.get("winner_side",-1) if final else -1): return _fail("MilitarySettlementResultMismatch",battle.id)
		if not Hex.whole(fact.get("tick"),battle.created_tick,state.tick) or not Hex.whole(fact.get("start_tick"),battle.created_tick,fact.tick) or not Hex.whole(receipt.get("tick"),fact.tick,fact.tick): return _fail("MilitarySettlementTimeMismatch",battle.id)
		var terminated: bool=final and battle.result.get("reason") in TERMINATIONS
		if terminated:
			if fact.winner_side!=-1 or fact.start_tick!=fact.tick or not fact.tick in [previous_tick,previous_tick+1] or battle.last_tick!=previous_tick: return _fail("MilitarySettlementTimeMismatch",battle.id)
		elif fact.start_tick!=previous_tick or fact.tick!=previous_tick+1: return _fail("MilitarySettlementTimeMismatch",battle.id)
		elif index==battle.round_receipt_ids.size()-1 and battle.last_tick!=fact.tick: return _fail("MilitarySettlementTimeMismatch",battle.id)
		previous_tick=fact.tick
	return {"ok":true}

## 供给差只接受该军该半日已经保存的唯一真实口粮账，不能把交货或别军短粮套到本部。
static func _supply(state: Dictionary, army_id: String, tick: int) -> Dictionary:
	var found: Dictionary={}
	for fact in state.facts.values():
		if fact is Dictionary and fact.get("kind")=="military_supply" and fact.get("army_id")==army_id and fact.get("tick")==tick and fact.has("shortage"):
			if not found.is_empty(): return {}
			found=fact
	return found

## 明确比较固定授权字段，UI不能通过会话附加键扩大指挥范围。
static func _same_authority(battle: Dictionary, authority: Dictionary) -> bool:
	for field in ["controller_actor_id","controller_side","command_scope","command_army_id","command_unit_ids","authority_office_id"]:
		if battle.get(field)!=authority.get(field): return false
	return true

## 序列中的真实ID唯一且有类型，不以隐式字符串转换容忍损坏来源。
static func _unique(value: Variant) -> bool:
	if not value is Array: return false
	var seen: Array=[]
	for id in value:
		if not id is String or id.is_empty() or id in seen: return false
		seen.append(id)
	return true

## 稳定排序字典键供建局与多战场结算共用，不让插入顺序改变随机流或战果。
static func _ids(values: Dictionary) -> Array:
	return _sorted(values.keys())

## 返回排序副本，不改变调用方的名册和永久事实。
static func _sorted(values: Array) -> Array:
	var result: Array=values.duplicate(); result.sort(); return result

## 失败包含具体会话或人物标识，便于根层给出下一步操作，候选事务负责回滚写入。
static func _fail(code: String, id: String="") -> Dictionary:
	return {"ok":false,"code":code,"battle_id":id,"path":id}

## 有限军令页只读复用实际结算的当前来源；失败原样传出，不将坏凭据降为中性墙。
static func damage_sources(state: Dictionary, data: Dictionary, battle: Dictionary) -> Dictionary:
	return _damage_sources(state,data,battle)

## 每次当前会战结算先核原永久来源，再按现墙HP投影；历史结果验证不调用活动pending专用路径。
static func _damage_sources(state: Dictionary, data: Dictionary, battle: Dictionary) -> Dictionary:
	var source: Variant=state.get("facts",{}).get(battle.get("source_id"))
	if not source is Dictionary: return _fail("MilitaryContactSourceRequired")
	var request: Dictionary={"source_id":battle.source_id,"battlefield_id":battle.battlefield_id,"unit_ids":source.get("unit_ids",[]),"side_by_army":source.get("side_by_army",{}),"objective":{"id":battle.objective_id,"unit_ids":battle.objective_unit_ids}}
	var checked: Dictionary=_contact(state,data,request,source,false)
	if not checked.ok: return checked
	checked=Fortification.project(state,data,battle.source_id)
	if not checked.ok: return checked
	var factors: Dictionary={}
	for unit_id in battle.units:
		var factor: Dictionary=Fortification.factor(checked.context,unit_id,true)
		if not factor.ok: return factor
		if factor.applies: factors[unit_id]={"siege_wall_bp":factor.factor_bp}
	return {"ok":true,"factors":factors}
