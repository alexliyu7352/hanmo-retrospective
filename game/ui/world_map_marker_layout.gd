## 按屏幕命中范围收纳重叠地点，代表点始终使用真实地点坐标，不生成聚落。
extends RefCounted
const CELL_SIZE: float=128.0
const VILLAGE_NAME_ZOOM: float=2.0

## 概览收起未选村名，放大或关注该地点时恢复；未知类型保可读名字，不猜行政等级。
static func shows_name(kind: String, zoom: float, priority: int) -> bool:
	return priority>0 or kind!="village" or zoom>=VILLAGE_NAME_ZOOM

## 先保留选点、途经与所在地点，再收纳相交标记；屏外地点仍可由检索到达。
static func arrange(rows: Array, viewport: Rect2) -> Dictionary:
	var ordered: Array=rows.duplicate()
	ordered.sort_custom(func(a: Dictionary,b: Dictionary):
		if a.priority!=b.priority: return a.priority>b.priority
		var a_city: bool=a.get("kind","")=="city"
		var b_city: bool=b.get("kind","")=="city"
		return a_city if a_city!=b_city else a.id<b.id)
	var groups: Dictionary={}
	var cells: Dictionary={}
	var owner: Dictionary={}
	for row in ordered:
		var hit: Rect2=row.hit
		if not viewport.intersects(hit): continue
		var keys: Array=_cells(hit)
		var nearby: Dictionary={}
		for key in keys:
			for id in cells.get(key,[]): nearby[id]=true
		var representative: String=""
		var nearest: float=INF
		for id in nearby:
			if not groups[id].hit.intersects(hit): continue
			var distance: float=groups[id].point.distance_squared_to(row.point)
			if distance<nearest or (distance==nearest and id<representative):
				representative=id; nearest=distance
		if representative.is_empty():
			representative=row.id
			groups[representative]={"ids":[],"hit":hit,"point":row.point}
			for key in keys:
				if not cells.has(key): cells[key]=[]
				cells[key].append(representative)
		groups[representative].ids.append(row.id)
		owner[row.id]=representative
	for group in groups.values(): group.ids.sort()
	return {"groups":groups,"owner":owner}

## 固定屏幕网格仅加速邻近候选查找，与世界坐标、路线和通行规则无关。
static func _cells(rect: Rect2) -> Array:
	var result: Array=[]
	for y in range(floori(rect.position.y/CELL_SIZE),floori(rect.end.y/CELL_SIZE)+1):
		for x in range(floori(rect.position.x/CELL_SIZE),floori(rect.end.x/CELL_SIZE)+1): result.append(Vector2i(x,y))
	return result
