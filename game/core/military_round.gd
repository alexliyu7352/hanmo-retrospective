## 六角野战纯轮次：使用真实部队与互斥兵员分类，消费已保存 AI 命令；不创建兵员、扣军粮或改世界半日。
extends RefCounted
const Hex=preload("res://core/military_hex.gd")
const Support=preload("res://core/domains/military/military_support.gd")
const Damage=preload("res://core/domains/military/military_damage.gd")
const ACTIONS=["move","attack","defend","wait","tactic","withdraw"]
const GROUPS=["fit","wounded","dead","captured","deserted"]
const STATUS_NAMES=["inspired","suppressed","exposed","hindered"]

## 检查引用、名册与授权；初建时可先不填 AI，保存恢复时传 require_ai=true 核验整套已保存命令。
static func validate(battle: Dictionary, data: Dictionary, require_ai: bool=false) -> Dictionary:
	for table in ["battlefields","troop_types","military_objectives"]:
		if not data.get(table) is Dictionary: return _fail("MissingMilitaryDefinitions",table)
	if not data.battlefields.has(battle.get("battlefield_id")) or not data.military_objectives.has(battle.get("objective_id")): return _fail("UnknownMilitaryDefinition")
	if not data.battlefields[battle.battlefield_id] is Dictionary or not data.military_objectives[battle.objective_id] is Dictionary: return _fail("InvalidMilitaryDefinition")
	var board: Dictionary=data.battlefields[battle.battlefield_id]
	var checked: Dictionary=Hex.validate(board)
	if not checked.ok: return checked
	checked=Damage.validate_model(board.get("damage_model",{}))
	if not checked.ok: return checked
	var objective: Dictionary=data.military_objectives[battle.objective_id]
	checked=_objective_definition(objective,board)
	if not checked.ok: return checked
	if not battle.get("id") is String or battle.id.is_empty() or battle.get("kind")!="military" or not battle.get("phase") in ["planning","result"] or not Hex.whole(battle.get("round"),1,10000): return _fail("InvalidMilitaryBattle")
	if not battle.get("units") is Dictionary or battle.units.size()<2 or battle.units.size()>64: return _fail("InvalidMilitaryUnits")
	if not battle.get("ai_plans") is Dictionary or not battle.get("last_events") is Array or not battle.get("result") is Dictionary: return _fail("InvalidMilitarySession")
	var seen: Dictionary={}; var occupied: Dictionary={}; var sides: Array=[]; var commanders: Array=[]
	for id in _ids(battle.units):
		var unit: Variant=battle.units[id]
		if not id is String or not unit is Dictionary or unit.get("unit_id")!=id: return _fail("InvalidMilitaryUnit",str(id))
		for field in ["army_id","commander_actor_id","troop_type_id"]:
			if not unit.get(field) is String or unit[field].is_empty(): return _fail("InvalidMilitaryUnit",id+"/"+field)
		if unit.commander_actor_id in commanders: return _fail("DuplicateMilitaryCommander",id)
		commanders.append(unit.commander_actor_id)
		if not data.troop_types.has(unit.troop_type_id): return _fail("UnknownTroopType",id)
		if not data.troop_types[unit.troop_type_id] is Dictionary: return _fail("InvalidTroopType",id)
		var troop: Dictionary=data.troop_types[unit.troop_type_id]
		checked=_troop_definition(troop)
		if not checked.ok: return checked
		if not Hex.whole(unit.get("side"),0,1) or not Hex.whole(unit.get("heading"),0,5) or not Hex.whole(unit.get("morale"),0,100) or not Hex.whole(unit.get("tactic_points"),0,troop.tactic_capacity): return _fail("InvalidMilitaryStatus",id)
		if not Hex.point(unit.get("position")) or Hex.movement_cost(board,unit.position,troop)<0 or not unit.get("withdrawn") is bool: return _fail("InvalidMilitaryPosition",id)
		if not unit.get("statuses") is Dictionary: return _fail("InvalidMilitaryStatuses",id)
		for status in unit.statuses:
			if not status in STATUS_NAMES or not Hex.whole(unit.statuses[status],0,10008): return _fail("InvalidMilitaryStatusExpiry",id+"/"+str(status))
		if not unit.get("member_ids") is Array or unit.member_ids.is_empty() or unit.member_ids.size()>4096: return _fail("InvalidMilitaryRoster",id)
		var members: Dictionary={}; var classified: Dictionary={}
		for member in unit.member_ids:
			if not member is String or member.is_empty() or members.has(member) or seen.has(member): return _fail("DuplicateMilitaryMember",id)
			members[member]=true; seen[member]=true
		for group in GROUPS:
			if not unit.get(group) is Array: return _fail("InvalidMilitaryClassification",id+"/"+group)
			for member in unit[group]:
				if not member is String or not members.has(member) or classified.has(member): return _fail("MilitaryRosterNotConserved",id+"/"+group)
				classified[member]=true
		if classified.size()!=members.size(): return _fail("MilitaryRosterNotConserved",id)
		if not unit.side in sides: sides.append(unit.side)
		if not unit.fit.is_empty() and unit.morale==0: return _fail("MilitaryRoutNotApplied",id)
		if unit.withdrawn and (unit.fit.is_empty() or not _exit(board,unit.side,unit.position)): return _fail("InvalidMilitaryWithdrawal",id)
		if _active(unit):
			var position: String=Hex.key_of(unit.position)
			if occupied.has(position): return _fail("MilitaryUnitsOverlap",id)
			occupied[position]=true
	if sides.size()!=2: return _fail("MissingMilitarySide")
	for unit in battle.units.values():
		if seen.has(unit.commander_actor_id) and not unit.commander_actor_id in unit.member_ids: return _fail("MilitaryCommanderInOtherUnit",unit.unit_id)
	checked=_authority(battle)
	if not checked.ok: return checked
	if not battle.get("objective_unit_ids") is Array: return _fail("InvalidMilitaryObjectiveUnits")
	var goal_units: Array=[]
	for id in battle.objective_unit_ids:
		if not id is String or not battle.units.has(id) or id in goal_units or battle.units[id].side!=objective.side: return _fail("InvalidMilitaryObjectiveUnit",str(id))
		goal_units.append(id)
	if objective.kind in ["escort","cover_retreat"] and goal_units.is_empty(): return _fail("MissingMilitaryObjectiveUnits")
	if objective.kind in ["breakthrough","cover_retreat"]:
		var own_units: int=0
		for unit in battle.units.values():
			if unit.side==objective.side: own_units+=1
		if objective.required_units>own_units or (objective.kind=="cover_retreat" and objective.required_units<goal_units.size()): return _fail("ImpossibleMilitaryExitCount")
	var progress: Variant=battle.get("objective_state")
	if not progress is Dictionary or not Hex.whole(progress.get("hold_rounds"),0,10000) or not progress.get("escaped_unit_ids") is Array: return _fail("InvalidMilitaryObjectiveProgress")
	var escaped: Array=[]
	for id in progress.escaped_unit_ids:
		if not id is String or id in escaped or not battle.units.has(id) or not battle.units[id].withdrawn: return _fail("InvalidMilitaryEscapedUnit",str(id))
		escaped.append(id)
	for id in battle.units:
		if battle.units[id].withdrawn!=(id in escaped): return _fail("MilitaryExitFactMismatch",id)
	if not battle.get("rng") is Dictionary or not _decimal(battle.rng.get("seed")) or not _decimal(battle.rng.get("state")): return _fail("InvalidMilitaryRandomState")
	if battle.phase=="planning" and not battle.result.is_empty(): return _fail("PrematureMilitaryResult")
	if battle.phase=="result" and (not Hex.whole(battle.result.get("winner_side"),-1,1) or not battle.result.get("reason") is String): return _fail("InvalidMilitaryResult")
	if require_ai:
		if battle.phase=="result":
			if not battle.ai_plans.is_empty(): return _fail("UnexpectedMilitaryAIOrder")
		else:
			checked=_saved_ai(battle,data)
			if not checked.ok: return checked
	return {"ok":true}

## 个别命令预览复用实际射界、控制区和杀伤区间；不读取 AI 草案或消耗会话随机数。
static func preview(battle: Dictionary, data: Dictionary, unit_id: String, order: Dictionary, source_factors_by_unit: Dictionary={}) -> Dictionary:
	var sources: Dictionary=_validate_sources(battle,source_factors_by_unit)
	if not sources.ok: return sources
	var checked: Dictionary=validate(battle,data)
	if not checked.ok: return checked
	if battle.phase!="planning": return _fail("MilitaryBattleNotPlanning")
	return _order(battle,data,unit_id,order,source_factors_by_unit)

## 提供同源普通或已知战法移动圈；先查点数，具体目标/动作仍由preview复核，不消耗资源。
static func movement_options(battle: Dictionary, data: Dictionary, unit_id: String, tactic_id: String="") -> Dictionary:
	var checked: Dictionary=validate(battle,data)
	if not checked.ok: return checked
	if battle.phase!="planning": return _fail("MilitaryBattleNotPlanning")
	if not battle.units.has(unit_id) or not _active(battle.units[unit_id]): return _fail("MilitaryUnitUnavailable",unit_id)
	var bonus: int=0; var kind: String=""
	if not tactic_id.is_empty():
		var unit: Dictionary=battle.units[unit_id]
		var tactic: Dictionary=_tactic(data.troop_types[unit.troop_type_id],tactic_id)
		if tactic.is_empty(): return _fail("MilitaryTacticNotKnown",unit_id)
		if unit.tactic_points<tactic.point_cost: return _fail("NotEnoughMilitaryTacticPoints",unit_id)
		if tactic.kind in ["rally","volley","support"]: return {"ok":true,"cells":[{"position":unit.position.duplicate(),"path":[unit.position.duplicate()],"cost":0}]}
		bonus=tactic.move_bonus; kind=tactic.kind
	return _movement_space(battle,data,unit_id,bonus,kind)

## 玩家只可完整提交当前授权集的活跃部队；己方未获授权的部队与敌方一样由保存的 AI 指令驱动。
static func validate_orders(battle: Dictionary, data: Dictionary, issuer_actor_id: String, orders: Dictionary) -> Dictionary:
	var checked: Dictionary=validate(battle,data)
	if not checked.ok: return checked
	if battle.phase!="planning": return _fail("MilitaryBattleNotPlanning")
	if issuer_actor_id!=battle.controller_actor_id: return _fail("MilitaryCommanderUnauthorized")
	var normalized: Dictionary={}
	for id in _ids(orders):
		if not id is String or not id in battle.command_unit_ids: return _fail("MilitaryUnitUnauthorized",str(id))
		if not orders[id] is Dictionary: return _fail("InvalidMilitaryOrder",id)
		checked=_order(battle,data,id,orders[id])
		if not checked.ok: return checked
		normalized[id]=checked.order
	for id in battle.command_unit_ids:
		if _active(battle.units[id]) and not normalized.has(id): return _fail("MissingMilitaryUnitOrder",id)
	return {"ok":true,"orders":normalized}

## 仅从已提交局面选择合法 AI 命令；远方会战允许空控制者，此时双方全部走同一选择与结算规则。
static func make_ai(battle: Dictionary, data: Dictionary, source_factors_by_unit: Dictionary={}) -> Dictionary:
	var sources: Dictionary=_validate_sources(battle,source_factors_by_unit)
	if not sources.ok: return sources
	var checked: Dictionary=validate(battle,data)
	if not checked.ok: return checked
	if battle.phase!="planning": return _fail("MilitaryBattleNotPlanning")
	var orders: Dictionary={}
	for id in _ids(battle.units):
		if id in battle.command_unit_ids or not _active(battle.units[id]): continue
		orders[id]=_ai_order(battle,data,id,source_factors_by_unit)
		checked=_order(battle,data,id,orders[id])
		if not checked.ok: return _fail("MilitaryAICannotPlan",id+"/"+checked.code)
	return {"ok":true,"orders":orders}

## 原子消费玩家与已保存 AI 命令，先同步移动再同时计伤；失败保留全部输入，不替换坏 AI 或重抽本轮。
static func resolve(battle: Dictionary, data: Dictionary, issuer_actor_id: String, orders: Dictionary, source_factors_by_unit: Dictionary={}) -> Dictionary:
	var sources: Dictionary=_validate_sources(battle,source_factors_by_unit)
	if not sources.ok: return sources
	var checked: Dictionary=validate_orders(battle,data,issuer_actor_id,orders)
	if not checked.ok: return checked
	var plans: Dictionary=checked.orders
	checked=_saved_ai(battle,data)
	if not checked.ok: return checked
	plans.merge(checked.orders)
	var next: Dictionary=battle.duplicate(true); next.last_events=[]
	var paths: Dictionary={}
	for id in plans: paths[id]=_order(battle,data,id,plans[id]).path
	var moved_steps: Dictionary=_move(next,data,plans,paths)
	var guards: Dictionary={}; var incoming_damage: Dictionary={}
	for id in _ids(plans):
		var unit: Dictionary=next.units[id]; var plan: Dictionary=plans[id]
		var troop: Dictionary=data.troop_types[unit.troop_type_id]
		unit.heading=plan.heading
		if plan.action in ["wait","defend"]:
			unit.tactic_points=mini(troop.tactic_capacity,unit.tactic_points+(3 if plan.action=="wait" else 2))
			unit.morale=mini(100,unit.morale+(1 if plan.action=="wait" else 3))
			if plan.action=="defend": guards[id]=25 if troop.category=="infantry" else 15
		elif plan.action=="tactic":
			var tactic: Dictionary=_tactic(troop,plan.tactic_id)
			unit.tactic_points-=tactic.point_cost
			if tactic.kind=="rally":
				for friend_id in _ids(next.units):
					var friend: Dictionary=next.units[friend_id]
					if _active(friend) and friend.side==unit.side and Hex.distance(friend.position,unit.position)<=1:
						friend.morale=mini(100,friend.morale+tactic.morale_effect)
						friend.statuses.inspired=maxi(friend.statuses.get("inspired",0),next.round+tactic.status_rounds-1)
						next.last_events.append({"kind":"rally","unit_id":id,"target_id":friend_id,"morale":friend.morale})
			elif tactic.kind=="charge": unit.statuses.exposed=next.round+tactic.status_rounds
			elif tactic.kind=="feint": incoming_damage[id]=11000
	var casualties: Dictionary={}; var morale_loss: Dictionary={}; var suppression: Dictionary={}; var hindrance: Dictionary={}
	var support_guards: Dictionary=Support.assignments(next,data,plans)
	var support_consumed: Dictionary={}
	var rng=RandomNumberGenerator.new(); rng.seed=int(next.rng.seed); rng.state=int(next.rng.state)
	for id in _ids(plans):
		var plan: Dictionary=plans[id]
		var tactic: Dictionary=_tactic(data.troop_types[next.units[id].troop_type_id],plan.tactic_id)
		if plan.action!="attack" and not (plan.action=="tactic" and tactic.kind in ["charge","volley","flank","suppress"]): continue
		var attack: Dictionary=_attack(next,data,id,plan.target_id,tactic,guards.get(plan.target_id,0),[],moved_steps.get(id,0),incoming_damage.get(plan.target_id,10000),source_factors_by_unit)
		var event: Dictionary={"kind":"attack","unit_id":id,"target_id":plan.target_id,"tactic_id":plan.tactic_id,"casualties":0}
		if not attack.ok: event.code=attack.code; next.last_events.append(event); continue
		var weighted: int=attack.potential*attack.hit_chance
		var count: int=weighted/100
		if rng.randi_range(1,100)<=weighted%100: count+=1
		var overkill: bool=count>0 and casualties.get(plan.target_id,0)>=next.units[plan.target_id].fit.size()
		count=mini(count,maxi(0,next.units[plan.target_id].fit.size()-casualties.get(plan.target_id,0)))
		var allocation: Dictionary=Support.allocate(next,support_guards,support_consumed,casualties,plan.target_id,count)
		if allocation.consumed: support_consumed[plan.target_id]=true
		var target_count: int=allocation.target_casualties
		casualties[plan.target_id]=casualties.get(plan.target_id,0)+target_count
		if count>0:
			morale_loss[plan.target_id]=morale_loss.get(plan.target_id,0)+target_count*100/maxi(1,next.units[plan.target_id].fit.size())+attack.flank+tactic.get("morale_effect",0)
			if tactic.get("kind")=="volley": suppression[plan.target_id]=maxi(suppression.get(plan.target_id,0),next.round+tactic.status_rounds)
			if tactic.get("kind")=="suppress": hindrance[plan.target_id]=next.round+1
		event.casualties=target_count; event.hit_chance=attack.hit_chance; event.code="Hit" if count>0 else ("AlreadyAccounted" if overkill else "Miss"); next.last_events.append(event)
		if allocation.supporter_casualties>0:
			var supporter: String=allocation.supporter_id; var share: int=allocation.supporter_casualties
			casualties[supporter]=casualties.get(supporter,0)+share
			morale_loss[supporter]=morale_loss.get(supporter,0)+share*100/maxi(1,next.units[supporter].fit.size())
			next.last_events.append({"kind":"support","unit_id":supporter,"target_id":plan.target_id,"attacker_id":id,"casualties":share,"original_casualties":count})
	next.rng.state=str(rng.state)
	for id in _ids(casualties):
		var unit: Dictionary=next.units[id]; var fit: Array=unit.fit.duplicate(); fit.sort()
		for index in range(mini(casualties[id],fit.size())):
			var member: String=fit[index]; unit.fit.erase(member)
			if index%3==0: unit.dead.append(member)
			else: unit.wounded.append(member)
		unit.morale=maxi(0,unit.morale-morale_loss.get(id,0))
		if suppression.has(id): unit.statuses.suppressed=suppression[id]
		if hindrance.has(id): unit.statuses.hindered=hindrance[id]
	_route(next)
	for id in _ids(plans):
		var unit: Dictionary=next.units[id]
		if plans[id].action=="withdraw" and _active(unit) and _exit(data.battlefields[next.battlefield_id],unit.side,unit.position):
			unit.withdrawn=true; next.objective_state.escaped_unit_ids.append(id)
			next.last_events.append({"kind":"withdraw","unit_id":id,"position":unit.position.duplicate()})
	next.objective_state.escaped_unit_ids.sort()
	var outcome: Dictionary=_outcome(next,data)
	if not outcome.is_empty():
		next.phase="result"; next.result=outcome; next.ai_plans={}
		next.last_events.append({"kind":"result","winner_side":outcome.winner_side,"reason":outcome.reason})
	else:
		next.round+=1
		for unit in next.units.values():
			for status in unit.statuses.keys():
				if unit.statuses[status]<next.round: unit.statuses.erase(status)
		var ai: Dictionary=make_ai(next,data,source_factors_by_unit)
		if not ai.ok: return ai
		next.ai_plans=ai.orders
	checked=validate(next,data)
	if not checked.ok: return checked
	return {"ok":true,"battle":next}

## 单个命令必须在公开起点局面合法，实际共同移动后射界仍会再查；命令不承诺敌人停在原处。
static func _order(battle: Dictionary, data: Dictionary, id: String, order: Dictionary, source_factors_by_unit: Dictionary={}) -> Dictionary:
	if not battle.units.has(id) or not _active(battle.units[id]): return _fail("MilitaryUnitUnavailable",id)
	for key in order:
		if not key in ["action","destination","heading","target_id","tactic_id"]: return _fail("UnexpectedMilitaryOrderField",id+"/"+str(key))
	if not order.get("action") in ACTIONS or not Hex.point(order.get("destination")) or not Hex.whole(order.get("heading"),0,5): return _fail("InvalidMilitaryOrder",id)
	if not order.get("target_id","") is String or not order.get("tactic_id","") is String: return _fail("InvalidMilitaryOrderTarget",id)
	var normalized: Dictionary={"action":order.action,"destination":order.destination.duplicate(),"heading":order.heading,"target_id":order.get("target_id",""),"tactic_id":order.get("tactic_id","")}
	var unit: Dictionary=battle.units[id]; var troop: Dictionary=data.troop_types[unit.troop_type_id]
	var tactic: Dictionary={}
	if normalized.action=="tactic":
		tactic=_tactic(troop,normalized.tactic_id)
		if tactic.is_empty(): return _fail("MilitaryTacticNotKnown",id)
		if unit.tactic_points<tactic.point_cost: return _fail("NotEnoughMilitaryTacticPoints",id)
	elif not normalized.tactic_id.is_empty(): return _fail("UnexpectedMilitaryTactic",id)
	var attacks: bool=normalized.action=="attack" or tactic.get("kind") in ["charge","volley","flank","suppress"]
	var supports: bool=tactic.get("kind")=="support"
	if not attacks and not supports and not normalized.target_id.is_empty(): return _fail("UnexpectedMilitaryTarget",id)
	if (normalized.action in ["wait","defend"] or tactic.get("kind") in ["rally","volley","support"]) and normalized.destination!=unit.position: return _fail("MilitaryActionRequiresHold",id)
	if normalized.action=="withdraw":
		if not data.military_objectives[battle.objective_id].allow_withdrawal: return _fail("MilitaryWithdrawalForbidden",id)
		if not _exit(data.battlefields[battle.battlefield_id],unit.side,normalized.destination): return _fail("MilitaryExitRequired",id)
	var result: Dictionary={"ok":true,"order":normalized}
	if tactic.get("kind")=="feint":
		if not _retreat_destination(unit,normalized.destination): return _fail("MilitaryFeintNeedsRetreat",id)
		result.incoming_damage_bp=11000
	if supports:
		var supported: Dictionary=Support.preview(battle,id,normalized.target_id)
		if not supported.ok: return _fail(supported.code,id)
		result.support_share_bp=supported.share_bp
	var movement: Dictionary={}
	if tactic.get("kind")=="flank":
		movement=_movement(battle,data,id,normalized.destination)
		if not movement.ok: return _fail(movement.code,id)
	if attacks:
		var attack: Dictionary=_attack(battle,data,id,normalized.target_id,tactic,0,normalized.destination,movement.get("path",[]).size()-1,10000,source_factors_by_unit)
		if not attack.ok: return _fail(attack.code,id)
		result.hit_chance=attack.hit_chance; result.casualties_min=attack.potential*attack.hit_chance/100
		result.casualties_max=mini(battle.units[normalized.target_id].fit.size(),(attack.potential*attack.hit_chance+99)/100)
	if movement.is_empty(): movement=_movement(battle,data,id,normalized.destination,tactic.get("move_bonus",0),tactic.get("kind",""))
	if not movement.ok: return _fail(movement.code,id)
	if tactic.get("kind")=="charge" and movement.cost<1: return _fail("MilitaryChargeNeedsMovement",id)
	result.path=movement.path; result.cost=movement.cost
	return result

## 对方仍占据的格不可穿越；进入任一敌邻格即停步，压制状态真实减少本轮移动预算。
static func _movement(battle: Dictionary, data: Dictionary, id: String, destination: Array, bonus: int=0, tactic_kind: String="") -> Dictionary:
	var space: Dictionary=_movement_space(battle,data,id,bonus,tactic_kind)
	if not space.ok: return space
	for entry in space.cells:
		if entry.position==destination: return {"ok":true,"path":entry.path,"cost":entry.cost}
	return _fail("HexDestinationUnreachable")

## 同一次搜索返回全部可达点，AI 复用移动圈而不对场外每个目标重复规划一遍。
static func _movement_space(battle: Dictionary, data: Dictionary, id: String, bonus: int=0, tactic_kind: String="") -> Dictionary:
	var unit: Dictionary=battle.units[id]; var troop: Dictionary=data.troop_types[unit.troop_type_id]
	var occupied: Array=[]; var zones: Array=[]
	for other_id in battle.units:
		var other: Dictionary=battle.units[other_id]
		if other_id==id or not _active(other): continue
		occupied.append(other.position)
		if other.side!=unit.side and tactic_kind!="feint": zones.append_array(Hex.neighbors(other.position))
	var budget: int=maxi(0,troop.movement+bonus-(1 if _status(unit,"suppressed",battle.round) or _status(unit,"hindered",battle.round) else 0))
	var space: Dictionary=Hex.reachable(data.battlefields[battle.battlefield_id],unit.position,troop,budget,occupied,zones)
	if space.ok and tactic_kind=="feint": space.cells=space.cells.filter(func(entry): return _retreat_destination(unit,entry.position))
	return space

## BA07后撤半面是明确实施约定：按轮初确认朝向取后方三方向，拒原地；本次新朝向不能改变资格。
static func _retreat_destination(unit: Dictionary, destination: Array) -> bool:
	if destination==unit.position: return false
	var difference: int=absi(unit.heading-Hex.direction(unit.position,destination))
	return mini(difference,6-difference)>=2

## 共同移动按地形代价分确定微步；争抢即停，只返回本轮实际六角步数供侧击复核，不保存额外状态。
static func _move(battle: Dictionary, data: Dictionary, plans: Dictionary, paths: Dictionary) -> Dictionary:
	var cursors: Dictionary={}; var due: Dictionary={}; var stopped: Dictionary={}; var moved_steps: Dictionary={}
	var board: Dictionary=data.battlefields[battle.battlefield_id]
	for id in plans:
		cursors[id]=1; moved_steps[id]=0
		if paths[id].size()>1: due[id]=Hex.movement_cost(board,paths[id][1],data.troop_types[battle.units[id].troop_type_id])
	for step in range(1,25):
		var proposals: Dictionary={}; var destinations: Dictionary={}; var occupied: Dictionary={}
		for id in battle.units:
			if _active(battle.units[id]): occupied[Hex.key_of(battle.units[id].position)]=id
		for id in _ids(due):
			if due[id]!=step or stopped.has(id): continue
			var position: Array=paths[id][cursors[id]]; var key: String=Hex.key_of(position)
			proposals[id]=position
			if not destinations.has(key): destinations[key]=[]
			destinations[key].append(id)
		for id in _ids(proposals):
			var position: Array=proposals[id]; var key: String=Hex.key_of(position)
			if occupied.has(key) or destinations[key].size()>1:
				stopped[id]=true; battle.last_events.append({"kind":"move","unit_id":id,"code":"MilitaryMoveBlocked","position":battle.units[id].position.duplicate()}); continue
			battle.units[id].position=position.duplicate(); cursors[id]+=1; moved_steps[id]+=1
			if cursors[id]<paths[id].size(): due[id]=step+Hex.movement_cost(board,paths[id][cursors[id]],data.troop_types[battle.units[id].troop_type_id])
			else: due.erase(id)
	for id in _ids(plans):
		if not stopped.has(id): battle.last_events.append({"kind":"move","unit_id":id,"code":"Moved","position":battle.units[id].position.duplicate()})
	return moved_steps

## 实际命中与预览共享射程、地形、朝向和士气；区间不读取随机数，弓的近身限制不会变成伤害换皮。
static func _attack(battle: Dictionary, data: Dictionary, id: String, target_id: String, tactic: Dictionary={}, guard: int=0, from_position: Array=[], moved_steps: int=0, incoming_damage_bp: int=10000, source_factors_by_unit: Dictionary={}) -> Dictionary:
	if not battle.units.has(target_id) or not _active(battle.units[target_id]): return _fail("MilitaryTargetUnavailable")
	var unit: Dictionary=battle.units[id]; var target: Dictionary=battle.units[target_id]
	if unit.side==target.side: return _fail("MilitaryFriendlyTarget")
	var troop: Dictionary=data.troop_types[unit.troop_type_id]; var defense: Dictionary=data.troop_types[target.troop_type_id]
	var board: Dictionary=data.battlefields[battle.battlefield_id]
	var position: Array=unit.position if from_position.is_empty() else from_position
	var distance: int=Hex.distance(position,target.position)
	var close_combat: bool=troop.category=="archer" and distance==1 and tactic.is_empty()
	if (distance<troop.minimum_range and not close_combat) or distance>troop.maximum_range+(1 if tactic.get("kind")=="volley" else 0): return _fail("MilitaryTargetOutOfRange")
	if troop.category=="archer" and distance==1 and tactic.get("kind") in ["volley","suppress"]: return _fail("MilitaryTargetOutOfRange")
	if not Hex.line_of_sight(board,position,target.position): return _fail("MilitarySightBlocked")
	var facing: int=absi(target.heading-Hex.direction(target.position,position)); facing=mini(facing,6-facing)
	if tactic.get("kind")=="flank":
		if moved_steps<2: return _fail("MilitaryFlankNeedsMovement")
		if facing<2: return _fail("MilitaryFlankNeedsSideOrRear")
	var attacker: Dictionary={"fit_count":unit.fit.size(),"attack":troop.attack,"accuracy":troop.accuracy,"morale":unit.morale,"inspired":_status(unit,"inspired",battle.round),"suppressed":_status(unit,"suppressed",battle.round),"cavalry_in_forest":troop.category=="cavalry" and Hex.cell_at(board,position).terrain=="forest"}
	var defender: Dictionary={"fit_count":target.fit.size(),"defense":defense.defense,"guard":guard,"cover":Hex.cover(board,target.position),"exposed":_status(target,"exposed",battle.round),"terrain":Hex.cell_at(board,target.position).terrain}
	var context: Dictionary={"facing":facing,"height_advantage":Hex.height(board,position)>Hex.height(board,target.position),"ranged":distance>1,"close_combat":close_combat,"power_bp":tactic.get("power_bp",10000),"accuracy_bonus":tactic.get("accuracy_bonus",0),"incoming_damage_bp":incoming_damage_bp}
	return Damage.quote(attacker,defender,context,board.get("damage_model",{}),source_factors_by_unit.get(id,{}))

## 士气归零是溃退，按实际接敌程度转被俘或逃散；五类兵员只作转移，绝不一律记成战死。
static func _route(battle: Dictionary) -> void:
	var routed: Array=[]
	for id in _ids(battle.units):
		var unit: Dictionary=battle.units[id]
		if not unit.withdrawn and not unit.fit.is_empty() and unit.morale<=0: routed.append(id)
	for id in routed:
		var unit: Dictionary=battle.units[id]; var enemies: int=0; var captor: String=""
		for other_id in _ids(battle.units):
			var other: Dictionary=battle.units[other_id]
			if not other_id in routed and _active(other) and other.side!=unit.side and Hex.distance(unit.position,other.position)<=1:
				enemies+=1
				if captor.is_empty(): captor=other_id
		var fit: Array=unit.fit.duplicate(); fit.sort()
		var capture_count: int=fit.size() if enemies>=2 else (fit.size()/2 if enemies==1 else 0)
		var captured_ids: Array=[]
		for index in range(fit.size()):
			if index<capture_count: unit.captured.append(fit[index]); captured_ids.append(fit[index])
			else: unit.deserted.append(fit[index])
		unit.fit=[]
		var captured_now: int=capture_count
		if enemies>=2: captured_now+=unit.wounded.size(); captured_ids.append_array(unit.wounded); unit.captured.append_array(unit.wounded); unit.wounded=[]
		battle.last_events.append({"kind":"rout","unit_id":id,"captured":captured_now,"deserted":fit.size()-capture_count,"captured_ids":captured_ids,"captor_unit_id":captor if captured_now>0 else "","captor_army_id":battle.units[captor].army_id if captured_now>0 else ""})

## 保护失败优先于其他成绩，守点与运送不会因敌人暂退便自动完成；互溃给平局而非偏袒玩家。
static func _outcome(battle: Dictionary, data: Dictionary) -> Dictionary:
	var objective: Dictionary=data.military_objectives[battle.objective_id]
	var side: int=objective.side; var enemy: int=1-side
	var counts: Array=[0,0]; var escaped: int=0
	for unit in battle.units.values():
		if _active(unit): counts[unit.side]+=1
		if unit.withdrawn and unit.side==side: escaped+=1
	if objective.kind in ["escort","cover_retreat"]:
		var delivered: bool=true
		for id in battle.objective_unit_ids:
			var unit: Dictionary=battle.units[id]
			if not unit.withdrawn and unit.fit.is_empty(): return {"winner_side":enemy,"reason":"ProtectedMilitaryUnitLost"}
			if not unit.withdrawn: delivered=false
		if delivered and (objective.kind=="escort" or escaped>=objective.required_units): return {"winner_side":side,"reason":"EscortDelivered" if objective.kind=="escort" else "RetreatCovered"}
	if objective.kind=="breakthrough" and escaped>=objective.required_units: return {"winner_side":side,"reason":"MilitaryBreakthrough"}
	if objective.kind=="hold":
		var held: bool=false; var contested: bool=false
		for unit in battle.units.values():
			if not _active(unit): continue
			if unit.side==side and unit.position==objective.point: held=true
			if unit.side!=side and Hex.distance(unit.position,objective.point)<=1: contested=true
		battle.objective_state.hold_rounds=battle.objective_state.hold_rounds+1 if held and not contested else 0
		if battle.objective_state.hold_rounds>=objective.hold_rounds: return {"winner_side":side,"reason":"MilitaryPointHeld"}
	if counts[side]==0:
		return {"winner_side":-1 if counts[enemy]==0 else enemy,"reason":"MutualMilitaryRout" if counts[enemy]==0 else "MilitarySideRepelled"}
	if objective.kind=="repel" and counts[enemy]==0: return {"winner_side":side,"reason":"EnemyMilitaryRepelled"}
	if battle.round>=objective.max_rounds: return {"winner_side":-1 if objective.kind=="repel" else enemy,"reason":"MilitaryObjectiveTimeout"}
	return {}

## AI先顾士气与护送目标，再选公开伤害/目标推进；压制仅按原单格20分估值，不读未提交计划。
static func _ai_order(battle: Dictionary, data: Dictionary, id: String, source_factors_by_unit: Dictionary={}) -> Dictionary:
	var unit: Dictionary=battle.units[id]; var troop: Dictionary=data.troop_types[unit.troop_type_id]
	var objective: Dictionary=data.military_objectives[battle.objective_id]
	var board: Dictionary=data.battlefields[battle.battlefield_id]
	var idle: Dictionary=_plan(unit.position,unit.heading,"defend")
	if unit.morale<60:
		for tactic in troop.tactics:
			if tactic.kind=="rally":
				var rally: Dictionary=_plan(unit.position,unit.heading,"tactic","",tactic.id)
				if _order(battle,data,id,rally).ok: return rally
	var leaving: bool=objective.allow_withdrawal and (unit.morale<25 or (unit.side==objective.side and (objective.kind=="breakthrough" or id in battle.objective_unit_ids or (objective.kind=="cover_retreat" and _subjects_escaped(battle)))))
	var goal: Array=[]
	if leaving:
		for entry in board.exits:
			if entry.side==unit.side and (goal.is_empty() or Hex.distance(unit.position,entry.position)<Hex.distance(unit.position,goal)): goal=entry.position
		if not goal.is_empty() and unit.position==goal: return _plan(unit.position,unit.heading,"withdraw")
	if goal.is_empty() and objective.kind=="hold": goal=objective.point
	var enemies: Array=[]
	for enemy_id in _ids(battle.units):
		if _active(battle.units[enemy_id]) and battle.units[enemy_id].side!=unit.side: enemies.append(enemy_id)
	if goal.is_empty():
		for enemy_id in enemies:
			var position: Array=battle.units[enemy_id].position
			if goal.is_empty() or Hex.distance(unit.position,position)<Hex.distance(unit.position,goal): goal=position
	var moves: Dictionary={}; var positions: Dictionary={}
	var normal_space: Dictionary=_movement_space(battle,data,id)
	for entry in normal_space.cells:
		var key: String=Hex.key_of(entry.position)
		moves[key]=entry; positions[key]=entry.position
	for tactic in troop.tactics:
		if tactic.kind!="charge" or unit.tactic_points<tactic.point_cost: continue
		for entry in _movement_space(battle,data,id,tactic.move_bonus).cells: positions[Hex.key_of(entry.position)]=entry.position
	var best: Dictionary=idle; var best_score: int=-2147483647
	for key in _ids(positions):
		var position: Array=positions[key]
		var movement: Dictionary=moves.get(key,{})
		if not movement.is_empty():
			var score: int=-movement.cost if goal.is_empty() else -Hex.distance(position,goal)*20-movement.cost
			if score>best_score:
				best_score=score; best=_plan(position,unit.heading,"move" if position!=unit.position else "defend")
				if leaving and _exit(board,unit.side,position): best.action="withdraw"; best_score+=1000
		if leaving: continue
		for target_id in enemies:
			var facing: int=Hex.direction(position,battle.units[target_id].position)
			var candidates: Array=[_plan(position,facing,"attack",target_id)]
			for tactic in troop.tactics:
				if tactic.kind in ["charge","volley","flank","suppress"]: candidates.append(_plan(position,facing,"tactic",target_id,tactic.id))
			for candidate in candidates:
				var quote: Dictionary=_order(battle,data,id,candidate,source_factors_by_unit)
				if not quote.ok: continue
				var score: int=100+quote.casualties_min*30+quote.hit_chance/5-quote.cost
				if _tactic(troop,candidate.tactic_id).get("kind")=="suppress":
					var target: Dictionary=battle.units[target_id]
					if data.troop_types[target.troop_type_id].movement>0 and not _status(target,"hindered",battle.round+1) and not _status(target,"suppressed",battle.round+1): score+=20*quote.hit_chance/100
				if score>best_score: best_score=score; best=candidate
	for kind in ["march","feint"]:
		var maneuver: Dictionary=_movement_tactic_candidate(battle,data,id,goal,kind)
		if maneuver.ok and maneuver.score>best_score: best_score=maneuver.score; best=maneuver.order
	if not leaving:
		var support: Dictionary=_support_candidate(battle,data,id,enemies,source_factors_by_unit)
		if support.ok and support.score>best_score: best=support.order
	return best

## 急行/佯退只用公开目标和原距离/代价评分；同好普通动作优先，最省点战法抵出口也不自动撤离。
static func _movement_tactic_candidate(battle: Dictionary, data: Dictionary, id: String, goal: Array, kind: String) -> Dictionary:
	if goal.is_empty(): return {"ok":false}
	var unit: Dictionary=battle.units[id]; var troop: Dictionary=data.troop_types[unit.troop_type_id]
	var selected: Dictionary={}
	for tactic in troop.tactics:
		if tactic.kind!=kind or tactic.point_cost>unit.tactic_points: continue
		if selected.is_empty() or tactic.point_cost<selected.point_cost or (tactic.point_cost==selected.point_cost and tactic.id<selected.id): selected=tactic
	if selected.is_empty(): return {"ok":false}
	var result: Dictionary={"ok":false}; var best_score: int=-2147483647
	for entry in _movement_space(battle,data,id,selected.move_bonus,kind).cells:
		var score: int=-Hex.distance(entry.position,goal)*20-entry.cost
		if score<=best_score: continue
		var order: Dictionary=_plan(entry.position,unit.heading,"tactic","",selected.id)
		if not _order(battle,data,id,order).ok: continue
		best_score=score; result={"ok":true,"order":order,"score":score}
	return result

## 协防只按公开轮初射界估计一次直接威胁，复用每名减员30分的原评分；不读取敌我保存计划或玩家草案。
static func _support_candidate(battle: Dictionary, data: Dictionary, id: String, enemies: Array, source_factors_by_unit: Dictionary={}) -> Dictionary:
	var unit: Dictionary=battle.units[id]; var troop: Dictionary=data.troop_types[unit.troop_type_id]
	var selected: Dictionary={}
	for tactic in troop.tactics:
		if tactic.kind!="support" or tactic.point_cost>unit.tactic_points: continue
		if selected.is_empty() or tactic.point_cost<selected.point_cost or (tactic.point_cost==selected.point_cost and tactic.id<selected.id): selected=tactic
	if selected.is_empty(): return {"ok":false}
	var result: Dictionary={"ok":false}; var best_score: int=-1
	for target_id in _ids(battle.units):
		if not Support.preview(battle,id,target_id).ok: continue
		var expected: int=0
		for enemy_id in enemies:
			var attack: Dictionary=_attack(battle,data,enemy_id,target_id,{},0,[],0,10000,source_factors_by_unit)
			if attack.ok: expected=maxi(expected,attack.potential*attack.hit_chance/100)
		var share: int=mini(unit.fit.size(),expected*2000/10000)
		if share<=0: continue
		var order: Dictionary=_plan(unit.position,unit.heading,"tactic",target_id,selected.id)
		if not _order(battle,data,id,order).ok: continue
		var score: int=100+share*30
		if score>best_score:
			best_score=score; result={"ok":true,"order":order,"score":score}
	return result

## 战法只属于兵种；未知类别和未实现效果拒绝，不能偷偷把个人功法当作军队战法。
static func _troop_definition(troop: Dictionary) -> Dictionary:
	if not troop.get("id") is String or not troop.get("category") in ["infantry","archer","cavalry"]: return _fail("InvalidTroopType")
	for field in ["movement","minimum_range","maximum_range","tactic_capacity","attack","defense","accuracy"]:
		if not Hex.whole(troop.get(field),0,100): return _fail("InvalidTroopValue",troop.id+"/"+field)
	if troop.movement>16 or troop.maximum_range>16 or troop.minimum_range<1 or troop.minimum_range>troop.maximum_range or troop.attack<1: return _fail("InvalidTroopRange",troop.id)
	if not troop.get("movement_costs") is Dictionary or not troop.get("tactics") is Array: return _fail("InvalidTroopCapabilities",troop.id)
	for terrain in Hex.TERRAINS:
		if not Hex.whole(troop.movement_costs.get(terrain),0,24): return _fail("InvalidTroopMovementCost",troop.id+"/"+terrain)
	var ids: Array=[]
	for tactic in troop.tactics:
		if not tactic is Dictionary or not tactic.get("id") is String or tactic.id.is_empty() or tactic.id in ids or not tactic.get("kind") in ["rally","charge","volley","support","march","flank","feint","suppress"]: return _fail("InvalidMilitaryTactic",troop.id)
		ids.append(tactic.id)
		if tactic.kind in ["volley","suppress"] and troop.category!="archer": return _fail("MilitaryTacticTroopMismatch",troop.id)
		if tactic.kind=="charge" and troop.category=="archer": return _fail("MilitaryTacticTroopMismatch",troop.id)
		if tactic.kind=="flank" and troop.category!="cavalry": return _fail("MilitaryTacticTroopMismatch",troop.id)
		if tactic.kind=="support" and troop.category!="infantry": return _fail("MilitaryTacticTroopMismatch",troop.id)
		for field in ["point_cost","morale_effect","move_bonus","status_rounds"]:
			if not Hex.whole(tactic.get(field),0,100): return _fail("InvalidMilitaryTacticValue",tactic.id+"/"+field)
		if tactic.point_cost<1 or tactic.point_cost>troop.tactic_capacity or tactic.move_bonus>8 or troop.movement+tactic.move_bonus>24 or tactic.status_rounds<1 or tactic.status_rounds>8: return _fail("InvalidMilitaryTacticCost",tactic.id)
		if not Hex.whole(tactic.get("power_bp"),1000,30000) or not Hex.whole(tactic.get("accuracy_bonus"),-100,100): return _fail("InvalidMilitaryTacticPower",tactic.id)
		if tactic.kind=="rally" and (tactic.power_bp!=10000 or tactic.accuracy_bonus!=0 or tactic.move_bonus!=0): return _fail("UnsupportedRallyParameter",tactic.id)
		if tactic.kind=="volley" and tactic.move_bonus!=0: return _fail("UnsupportedVolleyMovement",tactic.id)
		if tactic.kind=="suppress" and (tactic.power_bp!=8000 or tactic.accuracy_bonus!=0 or tactic.move_bonus!=0 or tactic.morale_effect!=0 or tactic.status_rounds!=1): return _fail("UnsupportedSuppressParameter",tactic.id)
		if tactic.kind=="feint" and (tactic.power_bp!=10000 or tactic.accuracy_bonus!=0 or tactic.move_bonus!=0 or tactic.morale_effect!=0 or tactic.status_rounds!=1): return _fail("UnsupportedFeintParameter",tactic.id)
		if tactic.kind=="flank" and (tactic.power_bp!=12500 or tactic.accuracy_bonus!=0 or tactic.move_bonus!=0 or tactic.morale_effect!=0 or tactic.status_rounds!=1): return _fail("UnsupportedFlankParameter",tactic.id)
		if tactic.kind=="march" and (tactic.power_bp!=10000 or tactic.accuracy_bonus!=0 or tactic.move_bonus!=2 or tactic.morale_effect!=0 or tactic.status_rounds!=1): return _fail("UnsupportedMarchParameter",tactic.id)
		if tactic.kind=="support" and (tactic.power_bp!=10000 or tactic.accuracy_bonus!=0 or tactic.move_bonus!=0 or tactic.morale_effect!=0 or tactic.status_rounds!=1): return _fail("UnsupportedSupportParameter",tactic.id)
	return {"ok":true}

## 独立目标只接受可实际执行的字段，出口型目标不能同时禁止撤离。
static func _objective_definition(objective: Dictionary, board: Dictionary) -> Dictionary:
	if not objective.get("kind") in ["repel","hold","breakthrough","escort","cover_retreat"] or not Hex.whole(objective.get("side"),0,1) or not Hex.whole(objective.get("max_rounds"),1,9999) or not objective.get("allow_withdrawal") is bool: return _fail("InvalidMilitaryObjective")
	if objective.kind=="hold":
		if not Hex.point(objective.get("point")) or Hex.cell_at(board,objective.point).is_empty() or not Hex.whole(objective.get("hold_rounds"),1,objective.max_rounds): return _fail("InvalidMilitaryHoldPoint")
	if objective.kind in ["breakthrough","escort","cover_retreat"]:
		if not objective.allow_withdrawal: return _fail("MilitaryObjectiveRequiresExit")
		var found: bool=false
		for entry in board.exits:
			if entry.side==objective.side: found=true
		if not found: return _fail("MilitaryObjectiveRequiresExit")
	if objective.kind in ["breakthrough","cover_retreat"] and not Hex.whole(objective.get("required_units"),1,64): return _fail("InvalidMilitaryExitCount")
	return {"ok":true}

## 指挥范围来自世界授权投影；部将只许本人一部，统帅只许指定军团内明确列出的同侧部队。
static func _authority(battle: Dictionary) -> Dictionary:
	if not battle.get("controller_actor_id") is String or not battle.get("command_army_id") is String or not battle.get("command_scope") in ["unit","army"] or not Hex.whole(battle.get("controller_side"),0,1) or not battle.get("command_unit_ids") is Array: return _fail("InvalidMilitaryAuthority")
	if battle.controller_actor_id.is_empty():
		return {"ok":true} if battle.command_unit_ids.is_empty() and battle.command_army_id.is_empty() else _fail("AutonomousMilitaryAuthority")
	if battle.command_unit_ids.is_empty() or (battle.command_scope=="unit" and battle.command_unit_ids.size()!=1) or (battle.command_scope=="army" and battle.command_army_id.is_empty()): return _fail("InvalidMilitaryCommandScope")
	var seen: Array=[]
	for id in battle.command_unit_ids:
		if not id is String or not battle.units.has(id) or id in seen or battle.units[id].side!=battle.controller_side: return _fail("MilitaryUnitUnauthorized",str(id))
		seen.append(id)
		if battle.command_scope=="unit" and battle.units[id].commander_actor_id!=battle.controller_actor_id: return _fail("MilitaryUnitUnauthorized",id)
		if battle.command_scope=="army" and battle.units[id].army_id!=battle.command_army_id: return _fail("MilitaryArmyUnauthorized",id)
	return {"ok":true}

## 保存命令必须恰好覆盖全部非受控活跃部队，坏命令不会在读取或结算时被悄悄替换。
static func _saved_ai(battle: Dictionary, data: Dictionary) -> Dictionary:
	var orders: Dictionary={}
	for id in _ids(battle.ai_plans):
		if not id is String or not battle.units.has(id) or id in battle.command_unit_ids or not _active(battle.units[id]): return _fail("UnexpectedMilitaryAIOrder",str(id))
	for id in _ids(battle.units):
		if id in battle.command_unit_ids or not _active(battle.units[id]): continue
		if not battle.ai_plans.get(id) is Dictionary: return _fail("MissingMilitaryAIOrder",id)
		var checked: Dictionary=_order(battle,data,id,battle.ai_plans[id])
		if not checked.ok: return _fail("InvalidMilitaryAIOrder",id+"/"+checked.code)
		orders[id]=checked.order
	return {"ok":true,"orders":orders}

## 取兵种已配置的战法，空 ID 仅表示普通命令而非默认战法。
static func _tactic(troop: Dictionary, id: String) -> Dictionary:
	for tactic in troop.tactics:
		if tactic.id==id: return tactic
	return {}

## 检查当前轮是否仍受短期战法影响，不通过 UI 动画计时失效。
static func _status(unit: Dictionary, kind: String, current_round: int) -> bool:
	return unit.statuses.get(kind,-1)>=current_round

## 活跃部队必须仍在战场、拥有可战人员且士气未溃；退出者保留自己的兵员分类。
static func _active(unit: Dictionary) -> bool:
	return not unit.withdrawn and not unit.fit.is_empty() and unit.morale>0

## 出口权限逐部按真实阵营判断，双方可配置同一撤离通道而不共享指挥权。
static func _exit(board: Dictionary, side: int, position: Array) -> bool:
	for entry in board.exits:
		if entry.side==side and entry.position==position: return true
	return false

## 掩护部队只有在全部保护对象已经离场时，才自行选择离开而非擅自放弃保护。
static func _subjects_escaped(battle: Dictionary) -> bool:
	for id in battle.objective_unit_ids:
		if not battle.units[id].withdrawn: return false
	return true

## 创建普通命令值，不将路径缓存或权威状态放进玩家可编辑的指令。
static func _plan(destination: Array, heading: int, action: String, target_id: String="", tactic_id: String="") -> Dictionary:
	return {"action":action,"destination":destination.duplicate(),"heading":heading,"target_id":target_id,"tactic_id":tactic_id}

## 固定 ID 顺序用于 AI、移动与命中结算，字典序列化顺序不改变重启后的结果。
static func _ids(values: Dictionary) -> Array:
	var ids: Array=values.keys(); ids.sort_custom(func(a, b): return str(a)<str(b)); return ids

## RNG 与既有世界采用相同十进制整数文本保存，不把无效文本静默变为零种子。
static func _decimal(value: Variant) -> bool:
	return value is String and value.is_valid_int() and str(int(value))==value

## 提供稳定失败代码与具体单位或定义字段，坏计划不会被自动改成等待。
static func _fail(code: String, path: String="") -> Dictionary:
	return {"ok":false,"code":code,"path":path}

## 只核受信来源投影的有限形状；不从作者参数、存档或全局状态猜墙加成。
static func _validate_sources(battle: Dictionary, sources: Dictionary) -> Dictionary:
	for id in sources:
		if not id is String or not battle.get("units",{}).has(id) or not Damage._sources(sources[id]): return _fail("InvalidMilitaryDamageSource",str(id))
	return {"ok":true}
