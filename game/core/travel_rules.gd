## 管理道路上的实际行程与消费；逻辑进度由世界半日推进，地图只读取位置。
extends RefCounted
const Events=preload("res://core/event_rules.gd")
const Values=preload("res://core/base/value_checks.gd")
const TerrainJourney=preload("res://core/terrain_journey.gd")
const Parties=preload("res://core/party_rules.gd")
const Location=preload("res://core/world_location.gd")
const Factory = preload("res://core/world_factory.gd")
const Inventory = preload("res://core/inventory_rules.gd")
const Graph = preload("res://core/route_graph.gd")
## 从真实据点入口规划行程，途经点属于同一计划，未确认时不扣钱。
static func plan(state: Dictionary, data: Dictionary, actor_id: String, destination: Variant, via: Variant = []) -> Dictionary:
	if not TerrainJourney.map_id(state,data).is_empty(): return TerrainJourney.plan(state,data,actor_id,destination,via)
	var actor: Dictionary = state.actors[actor_id]
	if actor.location.kind != "Settlement": return {"ok":false,"code":"InvalidDestination"}
	var origin: String = actor.location.settlement_id
	if actor.location.place_id != data.settlements[origin].entry_place: return {"ok":false,"code":"NotAtCityExit"}
	if not Parties.available(state,actor_id): return {"ok":false,"code":"ParticipantBusy"}
	if origin == destination: return {"ok":false,"code":"AlreadyThere"}
	var route: Dictionary = Graph.plan(data,origin,destination,state.tick,via)
	if not route.ok: return route
	if Inventory.spendable(state.wallets[actor.wallet_id]) < Graph.cost(data.routes[route.path[0]])+Parties.extra_cost(state,actor_id):
		return {"ok":false,"code":"InsufficientFunds"}
	route.cost += route.duration*Parties.extra_cost(state,actor_id)
	route.payer_wallet_id = actor.wallet_id
	return route
## 安装已确认的行程，只改变位置和行动，不预收尚未行走的费用。
static func begin(state: Dictionary, data: Dictionary, action: Dictionary, quote: Dictionary) -> void:
	if quote.get("terrain",false):
		TerrainJourney.begin(state,data,action,quote)
		return
	action.params.path = quote.path.duplicate()
	action.params.edge_index = 0
	action.params.returning = false
	action.params.paused = false
	action.params.reverse_first = false
	action.params.payer_wallet_id = quote.payer_wallet_id
	var edge: Dictionary = data.routes[quote.path[0]]
	state.actors[action.actor_id].location = {"kind":"OnRoute","edge_id":edge.id,"progress":0,"origin":edge.origin}
## 本地移动保留原Kernel写入先后：正式地点、清会话、入场事件；失败留给外层事务回滚。
static func move_local(state: Dictionary, data: Dictionary, actor_id: String, place_id: String) -> Dictionary:
	state.actors[actor_id].location=Factory.settlement_location(data,place_id)
	state.interactions.erase(actor_id)
	var entered: Dictionary=Events.on_enter(state,data,actor_id)
	if not entered.ok: return entered
	return {"ok":true}

## 查询下一半日是否可走；暂停、封路和缺钱分别返回原因，不冻结其他人物。
static func can_step(state: Dictionary, data: Dictionary, action: Dictionary, ignore_pause: bool = false) -> Dictionary:
	if action.params.get("terrain",false): return TerrainJourney.can_step(state,data,action,ignore_pause)
	for member in Location.members(state,action.actor_id):
		if not state.actors[member].alive or not state.actors[member].available: return {"ok":false,"code":"ParticipantBusy"}
	if action.params.get("paused",false) and not ignore_pause: return {"ok":false,"code":"TravelPaused"}
	var actor: Dictionary = state.actors[action.actor_id]
	var edge: Dictionary = data.routes[actor.location.edge_id]
	var boundary: bool = actor.location.progress == (0 if action.params.returning else edge.duration_ticks*100)
	if boundary:
		if action.params.returning and not action.params.get("reverse_first",false): return {"ok":true}
		if action.params.edge_index+1 >= action.params.path.size(): return {"ok":true}
		edge = data.routes[action.params.path[action.params.edge_index+1]]
		if not Graph.is_open(edge,state.tick): return {"ok":false,"code":"RoadBlocked"}
	elif not action.params.returning and not Graph.is_open(edge,state.tick):
		return {"ok":false,"code":"RoadBlocked"}
	if Inventory.spendable(state.wallets[action.params.payer_wallet_id]) < Graph.cost(edge)+Parties.extra_cost(state,action.actor_id):
		return {"ok":false,"code":"Stranded"}
	return {"ok":true}
## 在端点衔接下一条道路；当前半日的剩余速度不跨边使用，也不额外收钱。
static func finish_leg(state: Dictionary, data: Dictionary, action: Dictionary) -> bool:
	var actor: Dictionary = state.actors[action.actor_id]
	var edge: Dictionary = data.routes[actor.location.edge_id]
	var endpoint: int = 0 if action.params.returning else edge.duration_ticks*100
	if actor.location.progress != endpoint: return false
	var reached: String = edge.origin if action.params.returning else edge.destination
	var legacy_return: bool = action.params.returning and not action.params.get("reverse_first",false)
	if legacy_return or action.params.edge_index+1 >= action.params.path.size():
		actor.location = Factory.settlement_location(data,data.settlements[reached].entry_place)
		return true
	var next: Dictionary = data.routes[action.params.path[action.params.edge_index+1]]
	if not Graph.is_open(next,state.tick): return false
	action.params.edge_index += 1
	action.params.returning = false
	actor.location = {"kind":"OnRoute","edge_id":next.id,"progress":0,"origin":next.origin}
	return false
## 执行已核半日；默认返回不变，显式执行捕获只支持地形分支，费用和人物疲劳仍唯一结算。
static func step(state: Dictionary, data: Dictionary, action: Dictionary, capture_execution: bool=false) -> Dictionary:
	if action.params.get("terrain",false): return TerrainJourney.step(state,data,action,capture_execution)
	if capture_execution: return {"ok":false,"code":"TerrainMarchRequired"}
	var allowed: Dictionary = can_step(state,data,action)
	if not allowed.ok: return allowed
	if finish_leg(state,data,action): return {"ok":true,"completed":true}
	var actor: Dictionary = state.actors[action.actor_id]
	var edge: Dictionary = data.routes[actor.location.edge_id]
	state.wallets[action.params.payer_wallet_id].balance -= Graph.cost(edge)+Parties.extra_cost(state,action.actor_id)
	for member in Location.members(state,action.actor_id):
		state.actors[member].fatigue=mini(100,state.actors[member].fatigue+Graph.fatigue(edge))
	action.remaining_ticks = maxi(1,action.remaining_ticks-1)
	if action.params.returning:
		actor.location.progress = maxi(0,actor.location.progress-Graph.PROGRESS_PER_TICK)
	else:
		actor.location.progress = mini(edge.duration_ticks*100,actor.location.progress+Graph.PROGRESS_PER_TICK)
	return {"ok":true,"completed":finish_leg(state,data,action)}
## 从道路当前位置比较前进与退回方案；只替换剩余计划，不将人瞬移到城门。
static func replan(state: Dictionary, data: Dictionary, actor_id: String, destination: Variant, via: Variant = []) -> Dictionary:
	if state.actors[actor_id].location.kind=="Wilderness": return TerrainJourney.plan(state,data,actor_id,destination,via)
	if not destination is String or not data.settlements.has(destination): return {"ok":false,"code":"InvalidDestination"}
	if not via is Array or via.size()>8: return {"ok":false,"code":"InvalidWaypoints"}
	var seen: Array=[destination]
	for id in via:
		if not id is String or not data.settlements.has(id) or id in seen: return {"ok":false,"code":"InvalidWaypoints"}
		seen.append(id)
	var actor: Dictionary = state.actors[actor_id]
	var edge: Dictionary = data.routes[actor.location.edge_id]
	var best: Dictionary = {"ok":false,"code":"NoRoute"}
	for reverse in [false,true]:
		if not reverse and not Graph.is_open(edge,state.tick): continue
		var origin: String = edge.origin if reverse else edge.destination
		var remaining: Array=via.duplicate()
		if not remaining.is_empty() and remaining[0]==origin: remaining.pop_front()
		var route: Dictionary = Graph.plan(data,origin,destination,state.tick,remaining)
		if not route.ok: continue
		var points: int = actor.location.progress if reverse else edge.duration_ticks*100-actor.location.progress
		@warning_ignore("integer_division")
		var ticks: int = (points+99)/100
		route.duration += ticks
		route.cost += ticks*Graph.cost(edge)+route.duration*Parties.extra_cost(state,actor_id)
		route.path.push_front(edge.id)
		route.returning = reverse
		if not best.ok or route.duration < best.duration or (route.duration == best.duration and route.cost < best.cost): best = route
	return best
## 安装改道结果并恢复行走；零距离端点可以直接完成，但不退已有消费。
static func replace_plan(state: Dictionary, data: Dictionary, action: Dictionary, route: Dictionary) -> bool:
	if route.get("terrain",false):
		TerrainJourney.begin(state,data,action,route)
		return false
	action.params.path = route.path.duplicate()
	action.params.edge_index = 0
	action.params.returning = route.returning
	action.params.reverse_first = route.returning
	action.params.paused = false
	action.remaining_ticks = maxi(1,route.duration)
	return finish_leg(state,data,action)
## 救助耗时由现有行动推进，只处理明确的个人货损，不扣受托公物。
static func finish_rescue(state: Dictionary, data: Dictionary, action: Dictionary) -> void:
	var actor: Dictionary = state.actors[action.actor_id]
	var inventory: Dictionary = state.inventories[actor.inventory_id]
	for lot in inventory.lots:
		if lot.owner == {"kind":"actor","id":actor.id}:
			@warning_ignore("integer_division")
			var loss: int = lot.quantity/10
			lot.quantity -= loss
	inventory.lots = inventory.lots.filter(func(lot): return lot.quantity>0)
	inventory.revision += 1
	actor.location = Factory.settlement_location(data,data.settlements[action.params.origin].entry_place)

## 查询下一次实际行走的费用；已到达最终端点时无需再收路费。
static func next_cost(state: Dictionary, data: Dictionary, action: Dictionary) -> int:
	if action.params.get("terrain",false): return action.params.money_per_tick
	var location: Dictionary=state.actors[action.actor_id].location
	var edge: Dictionary=data.routes[location.edge_id]
	var boundary: bool=location.progress==(0 if action.params.returning else edge.duration_ticks*100)
	if boundary:
		if action.params.edge_index+1>=action.params.path.size(): return 0
		edge=data.routes[action.params.path[action.params.edge_index+1]]
	return Graph.cost(edge)+Parties.extra_cost(state,action.actor_id)

## 旧旅行载荷与路线连续性检查原样保留，地形旅程仍交原领域校验。
static func validate_action(state: Dictionary, data: Dictionary, action: Dictionary) -> Dictionary:
	var params: Dictionary=action.params
	if params.get("terrain",false): return TerrainJourney.validate_action(state,data,action)
	if not Values.fields(params,{"path":TYPE_ARRAY,"edge_index":TYPE_INT,"payer_wallet_id":TYPE_STRING,"returning":TYPE_BOOL}) or params.path.is_empty() or not Values.whole(params.edge_index,0,params.path.size()-1) or params.payer_wallet_id!=state.actors[action.actor_id].wallet_id: return {"ok":false,"code":"InvalidTravelAction"}
	for optional in ["paused","reverse_first"]:
		if params.has(optional) and not params[optional] is bool: return {"ok":false,"code":"InvalidJourneyControl"}
	if params.get("reverse_first",false) and params.returning != (params.edge_index==0): return {"ok":false,"code":"InvalidReturnDirection"}
	var previous: String=""
	var leg_index: int=0
	for edge_id in params.path:
		if not edge_id is String or not data.routes.has(edge_id): return {"ok":false,"code":"InvalidTravelPath"}
		var edge: Dictionary=data.routes[edge_id]
		if not previous.is_empty() and previous!=edge.origin: return {"ok":false,"code":"DisconnectedTravelPath"}
		previous=edge.origin if leg_index==0 and params.get("reverse_first",false) else edge.destination
		leg_index+=1
	if state.actors[action.actor_id].location.get("edge_id")!=params.path[params.edge_index]: return {"ok":false,"code":"RouteActionMismatch"}
	return {"ok":true}

## 救援只核对既有起点、期限和实际位置，不产生新的行程。
static func validate_rescue(state: Dictionary, data: Dictionary, action: Dictionary) -> Dictionary:
	var params: Dictionary=action.params
	if not params.get("origin") is String or not data.settlements.has(params.origin) or action.remaining_ticks>2 or state.actors[action.actor_id].location.get("origin")!=params.origin: return {"ok":false,"code":"InvalidRescueAction"}
	return {"ok":true}
