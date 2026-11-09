## 校验道路会面、挂起行程与NPC路线状态，历史回执不充当仍可交谈的位置。
extends RefCounted
const Behavior=preload("res://core/domains/identity/actor_behavior.gd")
const Query=preload("res://core/terrain_query.gd")
const Contact=preload("res://core/road_contact.gd")
const Location=preload("res://core/world_location.gd")
## 在提交和读档边界验证全部新集合，不修补损坏身份或丢失的旅行。
static func validate(state: Dictionary, data: Dictionary) -> Dictionary:
	var encounters: Dictionary=validate_encounters(state,data)
	if not encounters.ok: return encounters
	return validate_itineraries(state,data)

## 会面叶保留两个容器的最早检查；行程内容仍由后续独立阶段核验。
static func validate_encounters(state: Dictionary, data: Dictionary) -> Dictionary:
	if not state.get("encounters") is Dictionary or not state.get("itineraries") is Dictionary: return fail("MissingRoadState")
	if not state.encounters.is_empty() and not "road_encounters" in data.get("required_capabilities",[]): return fail("UnsupportedEncounters")
	var occupied: Dictionary={}
	var terrain_queries: Dictionary={}
	for id in state.encounters:
		var e: Variant=state.encounters[id]
		if not e is Dictionary or e.get("id")!=id or not e.get("status") in ["pending","resolved"]: return fail("InvalidEncounter")
		if not e.get("leaders") is Array or e.leaders.size()!=2 or e.leaders[0]==e.leaders[1] or not e.get("participants") is Array: return fail("InvalidEncounterMembers")
		if not e.get("anchors") is Dictionary or not e.get("action_ids") is Dictionary or not e.get("resume_paused") is Dictionary: return fail("InvalidEncounterAnchors")
		if e.anchors.size()!=2 or e.action_ids.size()!=2 or e.resume_paused.size()!=2 or not e.get("talked") is bool: return fail("InvalidEncounterShape")
		if not whole(e.get("created_tick"),0,state.tick) or not e.get("result_id") is String: return fail("InvalidEncounterTime")
		if e.status=="pending" and (e.get("resolved_tick")!=-1 or not e.result_id.is_empty()): return fail("InvalidEncounterResult")
		if e.status=="resolved" and (not whole(e.get("resolved_tick"),e.created_tick,state.tick) or e.result_id.is_empty()): return fail("InvalidEncounterResult")
		var expected: Array=[]
		for leader in e.leaders:
			if not leader is String or not state.actors.has(leader): return fail("UnknownEncounterActor")
			if not e.action_ids.get(leader) is String or not e.resume_paused.get(leader) is bool: return fail("InvalidEncounterAction")
			var anchor: Variant=e.anchors.get(leader)
			if not anchor is Dictionary: return fail("InvalidEncounterAnchor")
			if anchor.get("kind")=="Wilderness":
				if not anchor.get("map_id") is String or not data.get("world_maps",{}).has(anchor.map_id) or not anchor.get("point") is Array or not data.settlements.has(anchor.get("origin")): return fail("InvalidEncounterAnchor")
				if not terrain_queries.has(anchor.map_id): terrain_queries[anchor.map_id]=Query.new(data.world_maps[anchor.map_id],"foot",{})
				if not terrain_queries[anchor.map_id].point_valid(anchor.point): return fail("InvalidEncounterAnchor")
			elif anchor.get("kind")=="OnRoute":
				if not anchor.get("edge_id") is String or not data.routes.has(anchor.edge_id): return fail("InvalidEncounterAnchor")
				var edge: Dictionary=data.routes[anchor.edge_id]
				if anchor.get("origin")!=edge.origin or not whole(anchor.get("progress"),1,edge.duration_ticks*100-1): return fail("InvalidEncounterAnchor")
			else: return fail("InvalidEncounterAnchor")
			if e.status=="pending":
				var action: Variant=state.actions.get(e.action_ids[leader])
				if not action is Dictionary or action.kind!="travel" or action.actor_id!=leader: return fail("EncounterWithoutJourney")
				if action.params.get("encounter_id")!=id or not action.params.get("paused",false) or state.actors[leader].location!=anchor: return fail("EncounterJourneyMismatch")

				expected.append_array(Location.members(state,leader))
		if not anchors_meet(data,e.anchors[e.leaders[0]],e.anchors[e.leaders[1]]): return fail("EncounterAnchorsApart")
		if e.status=="pending" and e.participants!=expected: return fail("EncounterParticipantMismatch")
		var seen: Dictionary={}
		for person in e.participants:
			if not person is String or not state.actors.has(person) or seen.has(person): return fail("InvalidEncounterParticipant")
			seen[person]=true
			if e.status=="pending":
				if occupied.has(person): return fail("DoubleEncounterOccupation")
				occupied[person]=id
		for leader in e.leaders:
			if not seen.has(leader): return fail("MissingEncounterLeader")
	for action in state.actions.values():
		if action.kind!="travel": continue
		var id: Variant=action.params.get("encounter_id","")
		if not id is String: return fail("InvalidEncounterReference")
		if not id.is_empty() and occupied.get(action.actor_id)!=id: return fail("DanglingEncounter")
		var met: Variant=action.params.get("met_actions",[])
		if not met is Array or met.size()>4096: return fail("InvalidMetJourneys")
		for old in met:
			if not old is String or not old.begins_with("action:"): return fail("InvalidMetJourneys")
	return {"ok":true}
## 检查有限路线计划进度；未声明自动行旅的人物不能从存档取得隐藏AI脚本。
static func validate_itineraries(state: Dictionary, data: Dictionary) -> Dictionary:
	var required: Array=Behavior.scheduled_ids(state,data,"travel_plan")
	for id in required:
		var plan: Dictionary=Behavior.definition(state,data,id).travel_plan
		if not Behavior.valid_itinerary(state.itineraries.get(id),plan): return fail("InvalidItinerary")
	if state.itineraries.size()!=required.size(): return fail("UnexpectedItinerary")
	return {"ok":true}
## 拒绝布尔和小数冒充整数计数，不在读档时自行取整。
static func whole(value: Variant, low: int, high: int) -> bool:
	return typeof(value)==TYPE_INT and value>=low and value<=high
## 将错误交回原子提交边界，原内存世界和存档不受影响。
static func fail(code: String) -> Dictionary:
	return {"ok":false,"code":code}

## 两个锚点必须属于同一实际道路，并在整数进度量化误差内相接，不能远程说话。
static func anchors_meet(data: Dictionary, a: Dictionary, b: Dictionary) -> bool:
	if a.kind=="Wilderness" or b.kind=="Wilderness":
		if a.kind!=b.kind or a.map_id!=b.map_id: return false
		# 接触半径为4；两个连续锚点分别取整后，距离最多再增加sqrt(2)。
		return Vector2(a.point[0],a.point[1]).distance_squared_to(Vector2(b.point[0],b.point[1]))<=36
	var ea: Dictionary=data.routes[a.edge_id]; var eb: Dictionary=data.routes[b.edge_id]
	if not ((ea.origin==eb.origin and ea.destination==eb.destination) or (ea.origin==eb.destination and ea.destination==eb.origin)): return false
	if Contact.line(data,ea)!=Contact.line(data,eb): return false
	var la: int=ea.duration_ticks*100; var lb: int=eb.duration_ticks*100
	var pa: int=a.progress if ea.origin<ea.destination else la-a.progress
	var pb: int=b.progress if eb.origin<eb.destination else lb-b.progress
	return absi(pa*lb-pb*la)<la+lb
