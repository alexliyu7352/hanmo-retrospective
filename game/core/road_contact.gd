## 以实际行进轨迹检测道路会面；只读生成候选，不依赖画面帧或随机抽取。
extends RefCounted
const Location=preload("res://core/world_location.gd")
const TerrainJourney=preload("res://core/terrain_journey.gd")
const TerrainContact=preload("res://core/terrain_contact.gd")
const Terrain=preload("res://core/world_terrain.gd")
const Travel=preload("res://core/travel_rules.gd")
## 先计算所有旅行的半日轨迹，再按接触时刻和人物ID分配互不争用的会面。
static func detect(state: Dictionary, data: Dictionary, hold: Array = []) -> Array:
	# 旧内容没有声明路遇，不因升级运行核心而突然增加拦路会话。
	if not "road_encounters" in data.get("required_capabilities",[]): return []
	var traces: Array=[]; var terrain_traces: Array=[]
	var ids: Array=state.actions.keys(); ids.sort()
	for id in ids:
		var action: Dictionary=state.actions[id]
		if not Location.army_id(state,action.actor_id).is_empty(): continue
		if action.kind!="travel" or not action.params.get("encounter_id","").is_empty(): continue
		if action.params.get("terrain",false):
			terrain_traces.append(TerrainJourney.motion(state,data,action,hold))
			continue
		var trace: Dictionary=motion(state,data,action,hold)
		if not trace.is_empty(): traces.append(trace)
	var candidates: Array=[]
	var entrances: Dictionary={}
	var map_id: String=TerrainJourney.map_id(state,data)
	if not map_id.is_empty():
		entrances[map_id]=[]
		for place in data.settlements.values(): entrances[map_id].append(place.map_xy)
	for hit in TerrainContact.detect(terrain_traces,entrances):
		var legal: bool=true
		for person in [hit.a,hit.b]:
			var anchor: Dictionary=hit.anchors[person]
			var params: Dictionary=hit.traces[person].params
			if not Terrain.point_valid(data.world_maps[anchor.map_id],anchor.point,params.mode,state.flags): legal=false
			if params.points.is_empty() or not Terrain.segment_cost(data.world_maps[anchor.map_id],anchor.point,params.points[0],params.mode,{}).ok: legal=false
		if legal: candidates.append(hit)
	for i in range(traces.size()):
		for j in range(i+1,traces.size()):
			var a: Dictionary=traces[i]; var b: Dictionary=traces[j]
			if b.action_id in state.actions[a.action_id].params.get("met_actions",[]) or a.action_id in state.actions[b.action_id].params.get("met_actions",[]): continue
			var contact: Dictionary=intersection(a,b,data)
			if not contact.is_empty(): candidates.append(contact)
	candidates.sort_custom(before)
	var selected: Array=[]; var occupied: Dictionary={}
	for candidate in candidates:
		if occupied.has(candidate.a) or occupied.has(candidate.b): continue
		selected.append(candidate); occupied[candidate.a]=true; occupied[candidate.b]=true
	return selected
## 比较有理数时刻，不让容器顺序或浮点误差决定先遇到谁。
static func before(a: Dictionary, b: Dictionary) -> bool:
	var left: int=a.time_num*b.time_den; var right: int=b.time_num*a.time_den
	return (a.a+":"+a.b)<(b.a+":"+b.b) if left==right else left<right
## 在隔离快照预演同一TravelRules，处理端点转入下一边以及暂停人物的静止轨迹。
static func motion(state: Dictionary, data: Dictionary, action: Dictionary, hold: Array) -> Dictionary:
	var actor: Dictionary=state.actors[action.actor_id]
	if actor.location.kind!="OnRoute" or not actor.alive or not actor.available: return {}
	var moving: bool=not action.actor_id in hold and Travel.can_step(state,data,action).ok
	var probe: Dictionary=state.duplicate(true)
	var work: Dictionary=probe.actions[action.id]
	if moving and Travel.finish_leg(probe,data,work): return {}
	var start: Dictionary=probe.actors[action.actor_id].location.duplicate(true)
	if start.kind!="OnRoute": return {}
	var params: Dictionary=work.params.duplicate(true)
	var end_progress: int=start.progress
	if moving:
		var result: Dictionary=Travel.step(probe,data,work)
		if not result.ok: return {}
		var final: Dictionary=probe.actors[action.actor_id].location
		if final.get("edge_id")==start.edge_id: end_progress=final.progress
		else: end_progress=0 if params.returning else data.routes[start.edge_id].duration_ticks*100
	return {"actor_id":actor.id,"action_id":action.id,"edge_id":start.edge_id,"start":start.progress,"end":end_progress,"moving":moving,"params":params,"origin":start.origin}
## 统一正反方向的完整道路折线，拒绝仅端点相同但途中不同的路线。
static func line(data: Dictionary, edge: Dictionary) -> Array:
	var points: Array=[data.settlements[edge.origin].map_xy]
	points.append_array(edge.get("shape",[])); points.append(data.settlements[edge.destination].map_xy)
	if edge.origin>edge.destination: points.reverse()
	return points
## 返回同一路段的交会锚点；据点端点继续由普通城内交互处理。
static func intersection(a: Dictionary, b: Dictionary, data: Dictionary) -> Dictionary:
	if not a.moving and not b.moving: return {}
	var ea: Dictionary=data.routes[a.edge_id]; var eb: Dictionary=data.routes[b.edge_id]
	if not ((ea.origin==eb.origin and ea.destination==eb.destination) or (ea.origin==eb.destination and ea.destination==eb.origin)): return {}
	if line(data,ea)!=line(data,eb): return {}
	var la: int=ea.duration_ticks*100; var lb: int=eb.duration_ticks*100
	var a0: int=a.start if ea.origin<ea.destination else la-a.start
	var a1: int=a.end if ea.origin<ea.destination else la-a.end
	var b0: int=b.start if eb.origin<eb.destination else lb-b.start
	var b1: int=b.end if eb.origin<eb.destination else lb-b.end
	var gap0: int=a0*lb-b0*la; var gap1: int=a1*lb-b1*la
	if gap0!=0 and gap1!=0 and (gap0<0)==(gap1<0): return {}
	var denominator: int=absi(gap0-gap1); var numerator: int=absi(gap0)
	if denominator==0 or numerator>denominator: return {}
	@warning_ignore("integer_division")
	var pa: int=(a.start*denominator+(a.end-a.start)*numerator)/denominator
	@warning_ignore("integer_division")
	var pb: int=(b.start*denominator+(b.end-b.start)*numerator)/denominator
	if pa<=0 or pa>=la or pb<=0 or pb>=lb: return {}
	var anchors: Dictionary={}
	anchors[a.actor_id]={"kind":"OnRoute","edge_id":a.edge_id,"origin":a.origin,"progress":pa}
	anchors[b.actor_id]={"kind":"OnRoute","edge_id":b.edge_id,"origin":b.origin,"progress":pb}
	var traces: Dictionary={a.actor_id:a,b.actor_id:b}
	var people: Array=[a.actor_id,b.actor_id]; people.sort()
	return {"a":people[0],"b":people[1],"time_num":numerator,"time_den":denominator,"anchors":anchors,"traces":traces}
