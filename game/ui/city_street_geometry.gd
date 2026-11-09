## 街段的独立道路、地块与入口显示几何；不提供寻路、产权、通行或建设权限。
extends RefCounted

## 旧布局可省略街段信息；声明后要求真实纹理、稳定ID与合法几何，失败不补造道路。
static func resolve(document: Dictionary, layout: Dictionary, registry: RefCounted, buildings: Array) -> Dictionary:
	if not layout.has("roads") and not layout.has("plots"): return {"ok": true, "roads": [], "plots": [], "access": []}
	if not layout.get("roads") is Array or not layout.get("plots") is Array: return _fail("InvalidCityStreet")
	if layout.roads.size() > 16 or layout.plots.size() > 64: return _fail("CityStreetLimit")
	var roads: Array = []; var by_road: Dictionary = {}; var plots: Array = []; var by_plot: Dictionary = {}
	for source in layout.roads:
		if not source is Dictionary or not source.get("id") is String or source.id.is_empty() or by_road.has(source.id): return _fail("DuplicateCityRoad")
		if not source.get("points") is Array or source.points.size() < 2 or source.points.size() > 16: return _fail("InvalidCityRoad")
		if not _number(source.get("width")) or source.width < 20 or source.width > 220: return _fail("InvalidCityRoadWidth")
		var style: Dictionary = document.get("road_styles", {}).get(source.get("style", ""), {})
		var asset: String = str(style.get("asset_id", ""))
		if asset.is_empty() or registry.texture("backgrounds", asset) == null: return _fail("MissingCityRoadTexture")
		var basis: Variant = style.get("uv_basis", [[1, 0], [0, 1]])
		if not basis is Array or basis.size() != 2 or not _point(basis[0]) or not _point(basis[1]): return _fail("InvalidCityRoadMaterial")
		var transform: Transform2D = Transform2D(Vector2(basis[0][0], basis[0][1]), Vector2(basis[1][0], basis[1][1]), Vector2.ZERO)
		if absf(transform.determinant()) < 0.01 or not _number(style.get("opacity", 1.0)) or style.get("opacity", 1.0) <= 0 or style.get("opacity", 1.0) > 1: return _fail("InvalidCityRoadMaterial")
		var points: PackedVector2Array = PackedVector2Array()
		for point in source.points:
			if not _point(point): return _fail("InvalidCityRoadPoint")
			var p: Vector2 = Vector2(point[0], point[1])
			if p.x < 0 or p.y < 0 or p.x > document.canvas[0] or p.y > document.canvas[1]: return _fail("CityRoadOutsideCanvas")
			if not points.is_empty() and points[-1].distance_to(p) < 1: return _fail("InvalidCityRoadPoint")
			points.append(p)
		var geometry: Array = []
		for polygon in Geometry2D.offset_polyline(points, float(source.width) * 0.5, Geometry2D.JOIN_ROUND, Geometry2D.END_ROUND): geometry.append(_array(polygon))
		if geometry.is_empty(): return _fail("InvalidCityRoad")
		var road: Dictionary = {"id": source.id, "name": str(source.get("name", source.id)), "points": source.points.duplicate(true), "width": source.width,
			"asset_id": asset, "tile_size": clampf(float(style.get("tile_size", 256)), 64, 1024), "polygons": geometry,
			"uv_basis": basis.duplicate(true), "opacity": style.get("opacity", 1.0), "projected_access": style.has("uv_basis")}
		if source.has("node_ids"): road.node_ids = source.node_ids.duplicate() if source.node_ids is Array else source.node_ids
		roads.append(road); by_road[road.id] = road
	var topology: Dictionary = _validate_nodes(roads)
	if not topology.ok: return topology
	for source in layout.plots:
		if not source is Dictionary or not source.get("id") is String or source.id.is_empty() or by_plot.has(source.id): return _fail("DuplicateCityPlot")
		if not source.get("polygon") is Array or source.polygon.size() != 4 or not by_road.has(source.get("road_id")): return _fail("InvalidCityPlot")
		for point in source.polygon:
			if not _point(point) or point[0] < 0 or point[1] < 0 or point[0] > document.canvas[0] or point[1] > document.canvas[1]: return _fail("InvalidCityPlot")
		var polygon: PackedVector2Array = _polygon(source.polygon)
		if Geometry2D.convex_hull(polygon).size() != 5: return _fail("InvalidCityPlot")
		for previous in plots:
			if not Geometry2D.intersect_polygons(polygon, _polygon(previous.polygon)).is_empty(): return _fail("CityPlotsOverlap")
		var plot: Dictionary = source.duplicate(true); plot.occupant_id = ""; plot.place_id = ""
		plots.append(plot); by_plot[plot.id] = plot
	for building in buildings:
		if not building.interactive or not building.get("occupies_ground", true): continue
		for road in roads:
			for polygon in road.polygons:
				if not Geometry2D.intersect_polygons(_polygon(building.footprint), _polygon(polygon)).is_empty(): return _fail("CityRoadCrossesBuilding")
	var access: Array = []
	for building in buildings:
		if not building.interactive: continue
		var plot_id: String = str(building.get("plot_id", ""))
		if not by_plot.has(plot_id): return _fail("CityPlotRequired")
		var plot: Dictionary = by_plot[plot_id]
		if not plot.place_id.is_empty(): return _fail("CityPlotOccupied")
		for point in building.footprint:
			if not Geometry2D.is_point_in_polygon(Vector2(point[0], point[1]), _polygon(plot.polygon)): return _fail("CityBuildingOutsidePlot")
		plot.place_id = building.id
		if not building.get("occupies_ground", true):
			var center: Vector2 = Vector2.ZERO
			for point in plot.polygon: center += Vector2(point[0], point[1])
			center /= float(plot.polygon.size()); building.label = [center.x, center.y]
			continue
		plot.occupant_id = building.id
		var entry: Vector2 = Vector2(building.entry[0], building.entry[1])
		var road: Dictionary = by_road[plot.road_id]
		var nearest: Vector2 = _nearest(entry, road.points)
		if road.projected_access:
			var aligned: Variant = _aligned_entry(entry, road.points, Vector2(road.uv_basis[1][0], road.uv_basis[1][1]))
			if aligned == null: return _fail("CityEntryFacingAway")
			nearest = aligned
		if entry.distance_to(nearest) > float(road.width) * 0.5 + 200: return _fail("CityEntryTooFarFromRoad")
		for other in buildings:
			if not other.interactive or not other.get("occupies_ground", true): continue
			if other.id != building.id and _crosses(entry, nearest, other.footprint): return _fail("CityEntryObstructed")
		access.append({"instance_id": building.id, "plot_id": plot_id, "road_id": road.id, "points": [building.entry.duplicate(), [nearest.x, nearest.y]], "asset_id": road.asset_id, "tile_size": road.tile_size, "uv_basis": road.uv_basis.duplicate(true), "opacity": road.opacity})
	return {"ok": true, "roads": roads, "plots": plots, "access": access}

## 节点是有限作者连接，不推导交通图；只有明确共用同ID的位置可以形成路口。
static func _validate_nodes(roads: Array) -> Dictionary:
	var positions: Dictionary = {}; var declared: bool = false
	for road in roads:
		if not road.has("node_ids"): continue
		declared = true
		if not road.node_ids is Array or road.node_ids.size() != road.points.size(): return _fail("InvalidCityRoadNodes")
		var used: Array = []
		for index in range(road.node_ids.size()):
			var id: Variant = road.node_ids[index]; var point: Vector2 = Vector2(road.points[index][0], road.points[index][1])
			if not id is String or id.is_empty() or id in used: return _fail("InvalidCityRoadNodes")
			if positions.has(id) and positions[id].distance_to(point) > 0.01: return _fail("CityRoadJunctionSplit")
			positions[id] = point; used.append(id)
	if not declared: return {"ok": true}
	for road in roads:
		if not road.has("node_ids"): return _fail("InvalidCityRoadNodes")
	var segments: Array = []
	for road in roads:
		for index in range(road.points.size() - 1):
			var a: Vector2 = Vector2(road.points[index][0], road.points[index][1]); var b: Vector2 = Vector2(road.points[index + 1][0], road.points[index + 1][1])
			if a.distance_to(b) < 12: return _fail("CityRoadSegmentTooShort")
			for prior in segments:
				var contacts: Array = _segment_contacts(a, b, prior.a, prior.b)
				if contacts.size() > 1: return _fail("CityRoadSegmentOverlap")
				if contacts.is_empty(): continue
				var allowed: bool = false
				for id in [road.node_ids[index], road.node_ids[index + 1]]:
					if id in prior.ids and positions[id].distance_to(contacts[0]) < 0.05: allowed = true
				if not allowed: return _fail("CityUnboundRoadCrossing")
			segments.append({"a": a, "b": b, "ids": [road.node_ids[index], road.node_ids[index + 1]]})
	return {"ok": true}

## 相交或共线重叠都要核验，避免未声明穿越被平行线分支漏掉。
static func _segment_contacts(a: Vector2, b: Vector2, c: Vector2, d: Vector2) -> Array:
	var result: Array = []
	var cross: Variant = Geometry2D.segment_intersects_segment(a, b, c, d)
	if cross != null: result.append(cross)
	for point in [a, b, c, d]:
		if point.distance_to(Geometry2D.get_closest_point_to_segment(point, a, b)) > 0.01 or point.distance_to(Geometry2D.get_closest_point_to_segment(point, c, d)) > 0.01: continue
		var unique: bool = true
		for previous in result:
			if point.distance_to(previous) < 0.05: unique = false
		if unique: result.append(point)
	return result

## 根据放下的足点查找作者地块；无地块时返回空，绝不隐式创建产权或新地址。
static func plot_at(layout: Dictionary, position: Vector2) -> String:
	for plot in layout.get("plots", []):
		if Geometry2D.is_point_in_polygon(position, _polygon(plot.polygon)): return plot.id
	return ""

## 入口连接只取显示道路上的最近点，不计算路程、计时或任何游戏路线。
static func _nearest(position: Vector2, points: Array) -> Vector2:
	var best: Vector2 = Vector2(points[0][0], points[0][1]); var distance: float = INF
	for index in range(points.size() - 1):
		var point: Vector2 = Geometry2D.get_closest_point_to_segment(position, Vector2(points[index][0], points[index][1]), Vector2(points[index + 1][0], points[index + 1][1]))
		if position.distance_squared_to(point) < distance: best = point; distance = position.distance_squared_to(point)
	return best

## 门前短路沿同一地面纵深轴与所属道路相接，避免屏幕垂直线破坏建筑基础透视。
static func _aligned_entry(position: Vector2, points: Array, direction: Vector2) -> Variant:
	var best: Variant = null; var distance: float = INF
	var end: Vector2 = position + direction.normalized() * 2048
	for index in range(points.size() - 1):
		var point: Variant = Geometry2D.segment_intersects_segment(position, end, Vector2(points[index][0], points[index][1]), Vector2(points[index + 1][0], points[index + 1][1]))
		if point != null and position.distance_squared_to(point) < distance: best = point; distance = position.distance_squared_to(point)
	return best

## 阻挡仅用于防止入口示意线穿入另一建筑，不把道路几何当战斗或行走碰撞。
static func _crosses(a: Vector2, b: Vector2, polygon: Array) -> bool:
	var points: PackedVector2Array = _polygon(polygon)
	if Geometry2D.is_point_in_polygon(a, points) or Geometry2D.is_point_in_polygon(b, points): return true
	for index in range(points.size()):
		if Geometry2D.segment_intersects_segment(a, b, points[index], points[(index + 1) % points.size()]) != null: return true
	return false

## 几何数组只接受有限数字，坏作者坐标不能进入引擎三角化。
static func _point(value: Variant) -> bool:
	return value is Array and value.size() == 2 and _number(value[0]) and _number(value[1])

## 显示长度使用有限浮点，不触及游戏资金或资源精度。
static func _number(value: Variant) -> bool:
	return typeof(value) in [TYPE_INT, TYPE_FLOAT] and is_finite(float(value))

## JSON与引擎多边形之间只转换已经检查的几何。
static func _polygon(points: Array) -> PackedVector2Array:
	var result: PackedVector2Array = PackedVector2Array()
	for point in points: result.append(Vector2(point[0], point[1]))
	return result

## 引擎输出保持可序列化，渲染描述不暴露资源对象。
static func _array(points: PackedVector2Array) -> Array:
	var result: Array = []
	for point in points: result.append([point.x, point.y])
	return result

## 失败由调用者保留原草案与像素，不自动修建道路或挪动建筑。
static func _fail(code: String) -> Dictionary:
	return {"ok": false, "code": code}
