## 旧地图画布的有限动态输入；只读本人位置、既有开闭及当前行程，不生成知闻或第二旅行真值。
extends RefCounted
const Values=preload("res://core/base/value_checks.gd")
const Location=preload("res://core/world_location.gd")
const MapProjection=preload("res://core/views/map_projection.gd")
const Graph=preload("res://core/route_graph.gd")

## 合法来源沿原算法投影；坏源明确拒绝，不用零坐标、默认地图或全部畅通恢复画面。
static func query(state: Dictionary, data: Dictionary) -> Dictionary:
	if state.is_empty(): return {"ok":false,"code":"NoWorld"}
	if not Values.fields(state,{"player_actor_id":TYPE_STRING,"scenario_id":TYPE_STRING,"revision":TYPE_INT,"tick":TYPE_INT,"actors":TYPE_DICTIONARY,"actions":TYPE_DICTIONARY,"flags":TYPE_DICTIONARY}) or not Values.whole(state.revision) or not Values.whole(state.tick): return _fail()
	if not _catalog_valid(data): return _fail()
	var observer: String=state.player_actor_id
	var actor: Variant=state.actors.get(observer)
	if not Values.fields(actor,{"location":TYPE_DICTIONARY,"action_id":TYPE_STRING}) or not _location_source_valid(state,actor.location,observer): return _fail()
	var scenario: Variant=data.scenarios.get(state.scenario_id)
	if not scenario is Dictionary or not scenario.get("world_map_id","") is String: return _fail()
	var map_id: String=scenario.get("world_map_id","")
	var world_map: Dictionary={}
	if not map_id.is_empty():
		var source: Variant=data.get("world_maps",{}).get(map_id)
		if not _map_valid(source,map_id): return _fail()
		world_map=source
	var location: Dictionary=Location.resolve(state,observer)
	if not _resolved_valid(location,data,map_id): return _fail()
	var overlay: Dictionary=_travel_overlay(state,data,actor)
	if not overlay.ok: return overlay
	var point: Vector2=MapProjection.actor_position(state,data,observer)
	var result: Dictionary={"ok":true,"revision":state.revision,"observer_id":observer,"world_map_id":map_id,"observer_position":[point.x,point.y],"route_open":{},"crossing_closed":{},"road_progress":{},"travel_overlay":overlay.value}
	if world_map.is_empty():
		for id in data.routes: result.route_open[id]=Graph.is_open(data.routes[id],state.tick)
	else:
		for cell in world_map.cells:
			if not cell.has("crossing") or not cell.has("blocked_flag"): continue
			var closed: Variant=state.flags.get(cell.blocked_flag,false)
			if not closed is bool: return _fail()
			result.crossing_closed[cell.blocked_flag]=closed
	# 截线保留原Canvas本人raw位置条件；跟随者只共享坐标，不自动获得领队截线。
	if actor.location.kind=="OnRoute":
		result.road_progress={"edge_id":actor.location.edge_id,"ratio":float(actor.location.progress)/(data.routes[actor.location.edge_id].duration_ticks*100)}
	return result

## 只验证画布确实读取的作者坐标与道路引用；类型先于比较，几何仍唯一调用原投影实现。
static func _catalog_valid(data: Dictionary) -> bool:
	if not Values.fields(data,{"settlements":TYPE_DICTIONARY,"routes":TYPE_DICTIONARY,"scenarios":TYPE_DICTIONARY}) or not data.get("world_maps",{}) is Dictionary: return false
	for id in data.settlements:
		var town: Variant=data.settlements[id]
		if not id is String or not Values.fields(town,{"id":TYPE_STRING}) or town.id!=id or not _point(town.get("map_xy")): return false
	for id in data.routes:
		var edge: Variant=data.routes[id]
		if not id is String or not Values.fields(edge,{"id":TYPE_STRING,"origin":TYPE_STRING,"destination":TYPE_STRING,"duration_ticks":TYPE_INT}) or edge.id!=id: return false
		if not data.settlements.has(edge.origin) or not data.settlements.has(edge.destination) or not Values.whole(edge.duration_ticks,1): return false
		if not Values.whole(edge.get("blocked_from_tick",0)) or not Values.whole(edge.get("blocked_until_tick",0)) or not edge.get("shape",[]) is Array: return false
		for point in edge.get("shape",[]):
			if not _point(point): return false
	return true

## 临时校验跟随引用，避免原Location解析器遇到坏容器；不枚举或投影无关人物。
static func _location_source_valid(state: Dictionary, location: Dictionary, observer: String) -> bool:
	if not location.get("kind") is String: return false
	if location.kind=="Following":
		if not location.get("party_id") is String or not state.get("parties",{}) is Dictionary: return false
		var party: Variant=state.get("parties",{}).get(location.party_id)
		if not Values.fields(party,{"leader_id":TYPE_STRING,"members":TYPE_ARRAY}): return false
	elif location.kind=="InArmy":
		if not location.get("army_id") is String or not state.get("armies",{}) is Dictionary: return false
		var army: Variant=state.get("armies",{}).get(location.army_id)
		if not Values.fields(army,{"status":TYPE_STRING,"commander_actor_id":TYPE_STRING,"member_actor_ids":TYPE_ARRAY}) or not army.get("prisoner_ids",[]) is Array: return false
	var resolved: Dictionary=Location.resolve(state,observer)
	return not resolved.is_empty()

## 实际坐标必须可由既有地点、道路或当前地形图解释；不修补丢失的必要引用。
static func _resolved_valid(location: Dictionary, data: Dictionary, map_id: String) -> bool:
	if not location.get("kind") is String: return false
	match location.kind:
		"Settlement": return location.get("settlement_id") is String and data.settlements.has(location.settlement_id)
		"OnRoute": return location.get("edge_id") is String and data.routes.has(location.edge_id) and Values.whole(location.get("progress"))
		"Wilderness": return location.get("map_id") is String and not map_id.is_empty() and location.map_id==map_id and _point(location.get("point"),true)
	return false

## 仅校验当前地图的绘图输入；不读取未选地图或给所有世界旗标建立投影。
static func _map_valid(value: Variant, map_id: String) -> bool:
	if not Values.fields(value,{"id":TYPE_STRING,"width":TYPE_INT,"height":TYPE_INT,"cell_size":TYPE_INT,"cells":TYPE_ARRAY}) or value.id!=map_id: return false
	if not Values.whole(value.width,1) or not Values.whole(value.height,1) or not Values.whole(value.cell_size,1): return false
	if value.width%value.cell_size!=0 or value.height%value.cell_size!=0 or value.cells.size()!=(value.width/value.cell_size)*(value.height/value.cell_size): return false
	for cell in value.cells:
		if not cell is Dictionary: return false
		if not cell.has("crossing") or not cell.has("blocked_flag"): continue
		if not cell.crossing is String or not cell.crossing in ["ford","bridge","pass"] or not cell.blocked_flag is String: return false
	return true

## 只摘原当前travel高亮字段；不重新寻路、裁剪地形计划或读取报价/账目。
static func _travel_overlay(state: Dictionary, data: Dictionary, actor: Dictionary) -> Dictionary:
	if actor.action_id.is_empty(): return {"ok":true,"value":{}}
	var action: Variant=state.actions.get(actor.action_id)
	if not Values.fields(action,{"kind":TYPE_STRING}): return _fail()
	if action.kind!="travel": return {"ok":true,"value":{}}
	if not Values.fields(action,{"params":TYPE_DICTIONARY}) or not action.params.get("terrain",false) is bool: return _fail()
	var params: Dictionary=action.params
	if params.get("terrain",false):
		if actor.location.kind!="Wilderness" or not _point(actor.location.get("point"),true) or not params.get("points") is Array: return _fail()
		for point in params.points:
			if not _point(point,true): return _fail()
		return {"ok":true,"value":{"kind":"terrain","points":[actor.location.point.duplicate()]+params.points.duplicate(true)}}
	if not Values.fields(params,{"path":TYPE_ARRAY,"edge_index":TYPE_INT,"returning":TYPE_BOOL}) or not Values.whole(params.edge_index,0,params.path.size()): return _fail()
	for id in params.path:
		if not id is String or not data.routes.has(id): return _fail()
	return {"ok":true,"value":{"kind":"road","path":params.path.slice(params.edge_index),"returning":params.returning}}

## 画面坐标接受原有限数值；真实地形路径仍使用原整数点，不隐式取整。
static func _point(value: Variant, integer_only: bool=false) -> bool:
	if not value is Array or value.size()!=2: return false
	for coordinate in value:
		if typeof(coordinate)!=TYPE_INT and (integer_only or typeof(coordinate)!=TYPE_FLOAT): return false
		if not is_finite(float(coordinate)): return false
	return true

## 新有限门面的坏来源错误，不声称旧Canvas任意坏字典已有相同失败协议。
static func _fail() -> Dictionary: return {"ok":false,"code":"InvalidMapViewSource"}
