## 地形行旅沿用真实旅行行动和人物位置；路线预算表示本半日通行能力，不另建世界或计时器。
extends RefCounted
const Terrain=preload("res://core/world_terrain.gd")
const Query=preload("res://core/terrain_query.gd")
const Location=preload("res://core/world_location.gd")
const Inventory=preload("res://core/inventory_rules.gd")
const Parties=preload("res://core/party_rules.gd")
const Factory=preload("res://core/world_factory.gd")

## 只从当前剧本选择其作者地图，未声明地形的旧内容继续采用既有道路行程。
static func map_id(state: Dictionary, data: Dictionary) -> String:
	return data.scenarios[state.scenario_id].get("world_map_id","")

## 解析原野连续点或据点入口；地点与同行者仍通过唯一位置解析器获取。
static func point(state: Dictionary, data: Dictionary, actor_id: String) -> Array:
	var location: Dictionary=Location.resolve(state,actor_id)
	if location.get("kind")=="Wilderness": return location.point.duplicate()
	if location.get("kind")=="Settlement": return data.settlements[location.settlement_id].map_xy.duplicate()
	return []

## 在任意合法点与据点之间规划完整剩余行程，载重只降低速度，预览不移动人物或扣费。
static func plan(state: Dictionary, data: Dictionary, actor_id: String, destination: Variant, via: Variant=[]) -> Dictionary:
	var id: String=map_id(state,data)
	if id.is_empty() or not data.get("world_maps",{}).has(id): return fail("UnknownWorldMap")
	var actor: Dictionary=state.actors[actor_id]
	var location: Dictionary=Location.resolve(state,actor_id)
	if not location.get("kind") in ["Settlement","Wilderness"]: return fail("InvalidDestination")
	if location.kind=="Settlement" and location.place_id!=data.settlements[location.settlement_id].entry_place: return fail("NotAtCityExit")
	for member in Location.members(state,actor_id):
		var participant: Dictionary=state.actors[member]
		if not participant.alive or not participant.available: return fail("ParticipantBusy")
		if not participant.action_id.is_empty():
			var current: Dictionary=state.actions[participant.action_id]
			if current.kind!="travel" or current.actor_id!=actor_id or not current.params.get("terrain",false): return fail("ParticipantBusy")
	if not via is Array or via.size()>8: return fail("InvalidWaypoints")
	var target: Dictionary=destination_point(data,destination)
	if not target.ok: return target
	var origin: Array=point(state,data,actor_id)
	if target.point==origin and via.is_empty(): return fail("AlreadyThere")
	var stops: Array=[]; var seen: Array=[]
	for stop in via:
		if not stop is String or not data.settlements.has(stop) or stop in seen or (destination is String and stop==destination): return fail("InvalidWaypoints")
		seen.append(stop); stops.append(data.settlements[stop].map_xy)
	stops.append(target.point)
	var map: Dictionary=data.world_maps[id]
	var profile: Dictionary=travel_profile(state,data,actor_id)
	if not profile.ok: return profile
	var mode: String=profile.mode; var load_weight: int=profile.load_weight
	var budget: int=maxi(1,map.budgets[mode]*100/(100+load_weight*4))
	var money: int=map.money_per_tick+Parties.extra_cost(state,actor_id)
	if Inventory.spendable(state.wallets[profile.payer_wallet_id])<money: return fail("InsufficientFunds")
	var points: Array=[origin]; var cost: int=0
	for stop in stops:
		var route: Dictionary=Terrain.plan(map,points[-1],stop,mode,state.flags)
		if not route.ok: return route
		points.append_array(route.points.slice(1)); cost+=route.cost
	if points.size()<2: return fail("AlreadyThere")
	var credit: int=0
	if not actor.action_id.is_empty(): credit=_saved_credit(state.actions[actor.action_id].params)
	var duration: int=maxi(1,(maxi(0,cost-credit)+budget-1)/budget)
	return {"ok":true,"duration":duration,"cost":duration*money,"terrain":true,"map_id":id,"points":points,"destination":target.destination,"origin":location.get("origin",location.get("settlement_id","")),"mode":mode,"budget":budget,"starting_load":load_weight,"payer_wallet_id":profile.payer_wallet_id,"money_per_tick":money,"fatigue_per_tick":map.fatigue_per_tick}

## 目标可为真实据点ID或地图整数点，返回统一的保存形状；不替未知目标创建地点。
static func destination_point(data: Dictionary, destination: Variant) -> Dictionary:
	if destination is String and data.settlements.has(destination): return {"ok":true,"point":data.settlements[destination].map_xy.duplicate(),"destination":destination}
	if destination is Array and destination.size()==2 and typeof(destination[0])==TYPE_INT and typeof(destination[1])==TYPE_INT: return {"ok":true,"point":destination.duplicate(),"destination":destination.duplicate()}
	return fail("InvalidDestination")

## 安装确认的原始路线并保存永久计划依据；改道建立新依据而不退款或重置当前位置。
static func begin(state: Dictionary, _data: Dictionary, action: Dictionary, quote: Dictionary) -> void:
	var id: String=Factory.next_id(state,"journey")
	var origin: Array=quote.points[0].duplicate()
	var credit: int=_saved_credit(action.params) if action.params.get("terrain",false) else 0
	state.facts[id]={"kind":"terrain_journey","actor_id":action.actor_id,"action_id":action.id,"tick":state.tick,"map_id":quote.map_id,"origin":origin,"destination":quote.destination,"mode":quote.mode,"budget":quote.budget,"starting_load":quote.starting_load,"points":quote.points.duplicate(true)}
	action.params={"terrain":true,"plan_id":id,"map_id":quote.map_id,"route_index":1,"points":quote.points.slice(1),"destination":quote.destination,"origin":quote.origin,"mode":quote.mode,"budget":quote.budget,"travel_credit":credit,"payer_wallet_id":quote.payer_wallet_id,"money_per_tick":quote.money_per_tick,"fatigue_per_tick":quote.fatigue_per_tick,"paused":false,"encounter_id":"","met_actions":action.params.get("met_actions",[]).duplicate()}
	action.remaining_ticks=maxi(1,quote.duration)
	state.actors[action.actor_id].location={"kind":"Wilderness","map_id":quote.map_id,"point":origin,"origin":quote.origin}

## 检查下一半日和当前剩余路线，关闭关口时保持原位置与行程，要求明确改道或等候。
static func can_step(state: Dictionary, data: Dictionary, action: Dictionary, ignore_pause: bool=false, query: RefCounted=null) -> Dictionary:
	if action.params.get("paused",false) and not ignore_pause: return fail("TravelPaused")
	for member in Location.members(state,action.actor_id):
		if not state.actors[member].alive or not state.actors[member].available: return fail("ParticipantBusy")
	var params: Dictionary=action.params
	var map: Dictionary=data.world_maps[params.map_id]
	var from: Array=point(state,data,action.actor_id)
	if query==null: query=Query.new(map,params.mode,state.flags)
	if not query.point_valid(from): return fail("TerrainBlocked")
	var budget: int=params.budget+params.get("travel_credit",0)
	for target in params.points:
		var segment: Dictionary=query.segment_cost(from,target)
		if not segment.ok: return fail("TerrainBlocked")
		budget-=segment.cost
		if budget<=0: break
		from=target
	if Inventory.spendable(state.wallets[params.payer_wallet_id])<params.money_per_tick: return fail("Stranded")
	return {"ok":true}

## 只计算本半日公开轨迹；上步零碎预算参与本步移动，暂停不积攒，末段只选能继续原路线的整数落点。
static func motion(state: Dictionary, data: Dictionary, action: Dictionary, hold: Array=[]) -> Dictionary:
	var params: Dictionary=action.params
	var start: Array=point(state,data,action.actor_id)
	var map: Dictionary=data.world_maps[params.map_id]
	var query=Query.new(map,params.mode,state.flags)
	var moving: bool=not action.actor_id in hold and can_step(state,data,action,false,query).ok
	var credit: int=params.get("travel_credit",0)
	var budget: int=params.budget+credit
	var segments: Array=[]; var consumed: int=0; var reached: int=0; var here: Array=start.duplicate()
	if moving:
		while reached<params.points.size():
			var available: int=budget-consumed
			if available<=0: break
			var target: Array=params.points[reached]
			var cost: int=query.segment_cost(here,target).cost
			if cost==0: reached+=1; continue
			var landing: Array=target.duplicate() if cost<=available else _partial_point(query,here,target,available)
			if landing==here: break
			var profile: Dictionary=query.segment_profile(here,landing)
			var spent: int=profile.cost
			for span in profile.spans:
				segments.append({"from":span.from.duplicate(),"to":span.to.duplicate(),"start_cost":consumed+span.start_cost,"end_cost":consumed+span.end_cost,"point_index":reached})
			# 积分成本向上取整的不足一单位时间在原地消耗，不能把它变成额外位移。
			if profile.raw_cost<float(spent):
				segments.append({"from":landing.duplicate(),"to":landing.duplicate(),"start_cost":consumed+profile.raw_cost,"end_cost":consumed+spent,"point_index":reached+1 if landing==target else reached})
			consumed+=spent; here=landing
			if here==target: reached+=1
	if consumed<budget: segments.append({"from":here.duplicate(),"to":here.duplicate(),"start_cost":consumed,"end_cost":budget,"point_index":reached})
	var end_credit: int=(budget-consumed if reached<params.points.size() else 0) if moving else credit
	return {"actor_id":action.actor_id,"action_id":action.id,"map_id":params.map_id,"origin":params.origin,"params":params.duplicate(true),"segments":segments,"budget":budget,"moving":moving,"end":here,"end_credit":end_credit,"reached_points":reached}

## 寻找预算内可续行的整数落点，同时检查前后两段；取整失效时保留预算待后续半日继续。
static func _partial_point(query: RefCounted, from: Array, to: Array, budget: int) -> Array:
	var low: int=0; var high: int=1000000; var result: Array=from.duplicate()
	for _iteration in range(20):
		var amount: int=(low+high+1)/2
		var candidate: Array=[from[0]+int((to[0]-from[0])*amount/1000000),from[1]+int((to[1]-from[1])*amount/1000000)]
		var queried: Dictionary=query.segment_cost(from,candidate)
		if queried.ok and queried.cost<=budget and query.segment_cost(candidate,to).ok: result=candidate; low=amount
		else: high=amount-1
		if low>=high: break
	return result

## 结算真实半日；显式捕获只返回实际整数前缀，不制造军团权限、条件事实或第二次行旅。
static func step(state: Dictionary, data: Dictionary, action: Dictionary, capture_execution: bool=false) -> Dictionary:
	var allowed: Dictionary=can_step(state,data,action)
	if not allowed.ok: return allowed
	var trace: Dictionary=motion(state,data,action)
	var params: Dictionary=action.params
	var execution: Dictionary={}
	var from_point: Array=point(state,data,action.actor_id)
	if capture_execution and retained_movement(from_point,params.points.slice(0,trace.reached_points),trace.end):
		execution={"format":"HM-TERRAIN-EXECUTION/1","actor_id":action.actor_id,"action_id":action.id,"action_start_tick":action.start_tick,"source_tick":state.tick,"plan_id":params.plan_id,"map_id":params.map_id,"from_point":from_point.duplicate(),"to_point":trace.end.duplicate(),"route_index_before":params.route_index,"route_index_after":params.route_index+trace.reached_points,"travel_credit_before":params.get("travel_credit",0),"travel_credit_after":trace.end_credit}
	state.wallets[params.payer_wallet_id].balance-=params.money_per_tick
	for member in Location.members(state,action.actor_id): state.actors[member].fatigue=mini(100,state.actors[member].fatigue+params.fatigue_per_tick)
	state.actors[action.actor_id].location.point=trace.end.duplicate()
	params.points=params.points.slice(trace.reached_points)
	params.route_index+=trace.reached_points
	params.travel_credit=trace.end_credit
	var remaining: int=0; var here: Array=trace.end
	var base_query=Query.new(data.world_maps[params.map_id],params.mode,{})
	for target in params.points:
		# 远方临时封闭只影响能否继续，不能让尚未走到的余程失去可保存的基准时耗。
		remaining+=base_query.segment_cost(here,target).cost
		here=target
	action.remaining_ticks=maxi(1,(maxi(0,remaining-params.travel_credit)+params.budget-1)/params.budget)
	var finished: bool=params.points.is_empty()
	if finished and params.destination is String: state.actors[action.actor_id].location=Factory.settlement_location(data,data.settlements[params.destination].entry_place)
	var outcome: Dictionary={"ok":true,"completed":finished}
	if not execution.is_empty():
		execution.result_location=state.actors[action.actor_id].location.duplicate(true)
		outcome.execution=execution
	return outcome

## 只比较已保留的整数路点前缀；重复零长路点不计，绕行回到起点仍计，不借浮点轨迹或moving标记补位移。
static func retained_movement(from_point: Array, crossed_points: Array, to_point: Array) -> bool:
	var previous: Array=from_point
	for next in crossed_points:
		if next!=previous: return true
		previous=next
	return to_point!=previous

## 校验稳定行程与冻结依据；当前封路不损坏存档，但下次前进必须重新通过实际通行查询。
static func validate_action(state: Dictionary, data: Dictionary, action: Dictionary) -> Dictionary:
	var p: Dictionary=action.params
	for key in ["map_id","plan_id","origin","mode","payer_wallet_id"]:
		if not p.get(key) is String: return fail("InvalidTerrainJourney")
	if not data.get("world_maps",{}).has(p.map_id) or p.map_id!=map_id(state,data) or not data.settlements.has(p.origin) or not p.mode in Terrain.MODES: return fail("InvalidTerrainJourney")
	for key in ["budget","money_per_tick","fatigue_per_tick"]:
		if typeof(p.get(key))!=TYPE_INT or p[key]<(1 if key=="budget" else 0): return fail("InvalidTerrainJourney")
	if not p.has("destination"): return fail("InvalidTerrainJourney")
	if not p.get("paused") is bool or not p.get("points") is Array or p.points.is_empty(): return fail("InvalidTerrainJourney")
	var actor: Dictionary=state.actors[action.actor_id]
	var profile: Dictionary=travel_profile(state,data,actor.id)
	if not profile.ok or p.mode!=profile.mode or p.payer_wallet_id!=profile.payer_wallet_id or actor.location.get("map_id")!=p.map_id or actor.location.get("kind")!="Wilderness": return fail("InvalidTerrainJourneyActor")
	var fact: Dictionary=state.facts.get(p.plan_id,{})
	if fact.get("kind")!="terrain_journey" or fact.get("actor_id")!=actor.id or fact.get("action_id")!=action.id or fact.get("map_id")!=p.map_id or fact.get("budget")!=p.budget or fact.get("mode")!=p.mode or fact.get("destination")!=p.destination: return fail("TerrainJourneyFactMismatch")
	if typeof(fact.get("tick"))!=TYPE_INT or fact.tick<action.start_tick or fact.tick>state.tick or not fact.get("points") is Array or fact.points.size()<2 or fact.get("origin")!=fact.points[0]: return fail("TerrainJourneyFactMismatch")
	if typeof(p.get("route_index"))!=TYPE_INT or p.route_index<1 or p.route_index>=fact.points.size() or p.points!=fact.points.slice(p.route_index): return fail("TerrainJourneySuffixMismatch")
	var target: Dictionary=destination_point(data,p.destination)
	if not target.ok or p.points[-1]!=target.point: return fail("TerrainJourneyTargetMismatch")
	var map: Dictionary=data.world_maps[p.map_id]
	var base_query=Query.new(map,p.mode,{})
	if typeof(p.get("travel_credit"))!=TYPE_INT or p.travel_credit<0 or p.travel_credit>Terrain.MAX_COST: return fail("InvalidTerrainCredit")
	# 改道沿用行动开始时刻；整个行动可获得的最大预算为累积上界，不能读档凭空添加多年通行能力。
	var maximum_budget: int=0
	for value in map.budgets.values(): maximum_budget=maxi(maximum_budget,value)
	var elapsed: int=maxi(0,state.tick-action.start_tick)
	if p.travel_credit>mini(Terrain.MAX_COST,elapsed*maximum_budget): return fail("InvalidTerrainCredit")
	if p.money_per_tick!=map.money_per_tick+Parties.extra_cost(state,actor.id) or p.fatigue_per_tick!=map.fatigue_per_tick: return fail("TerrainJourneyCostMismatch")
	if typeof(fact.get("starting_load"))!=TYPE_INT or fact.starting_load<0 or p.budget!=maxi(1,map.budgets[p.mode]*100/(100+fact.starting_load*4)): return fail("TerrainJourneyBudgetMismatch")
	var here: Array=actor.location.point
	var original: Variant=fact.points[0]
	for next in fact.points.slice(1):
		if not original is Array or not next is Array or not base_query.segment_cost(original,next).ok: return fail("TerrainJourneyFactMismatch")
		original=next
	for next in p.points:
		if not next is Array or not base_query.segment_cost(here,next).ok: return fail("InvalidTerrainJourneyPath")
		here=next
	return {"ok":true}

## 改道只继承合法的已存预算，旧行动没有该字段时视为零；损坏存档仍由validate_action拒绝。
static func _saved_credit(params: Dictionary) -> int:
	var value: Variant=params.get("travel_credit",0)
	return value if typeof(value)==TYPE_INT and value>=0 and value<=Terrain.MAX_COST else 0

## 领域错误统一交给候选事务丢弃，保留实际原位置与消费历史。
static func fail(code: String) -> Dictionary:
	return {"ok":false,"code":code}

## 行军按实际兵员分担真实公私负载，军费只走本军公柜；个人与同行继续使用既有最大背负。
static func travel_profile(state: Dictionary, data: Dictionary, actor_id: String) -> Dictionary:
	var id: String=Location.army_id(state,actor_id)
	var weight: int=0
	if id.is_empty():
		for person in Location.members(state,actor_id): weight=maxi(weight,Inventory.weight(state.inventories[state.actors[person].inventory_id],data))
		return {"ok":true,"mode":"foot","load_weight":weight,"payer_wallet_id":state.actors[actor_id].wallet_id}
	var army: Dictionary=state.armies[id]
	if army.commander_actor_id!=actor_id: return fail("NotArmyCommander")
	var people: Dictionary={}
	for person in army.member_actor_ids:
		if state.actors[person].alive:
			people[person]=true
			weight+=Inventory.weight(state.inventories[state.actors[person].inventory_id],data)
	for unit_id in army.unit_ids:
		for person in state.military_units[unit_id].fit: people[person]=true
	if people.is_empty(): return fail("MilitaryUnitUnavailable")
	weight+=Inventory.weight(state.inventories[army.inventory_id],data)
	return {"ok":true,"mode":"army","load_weight":int((weight+people.size()-1)/people.size()),"payer_wallet_id":army.wallet_id}
