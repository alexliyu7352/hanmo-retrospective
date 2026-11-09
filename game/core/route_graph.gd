## 只读道路规划：输入当前公开路况和途经点，返回确定路径；不修改时钟、钱包或随机流。
extends RefCounted
const DEFAULT_COST: int = 8
const DEFAULT_FATIGUE: int = 5
const PROGRESS_PER_TICK: int = 100
## 查询本时刻是否允许向前通过；退回已走路段由行程规则独立处理。
static func is_open(edge: Dictionary, tick: int) -> bool:
	return not (tick >= edge.get("blocked_from_tick",0) and tick < edge.get("blocked_until_tick",0))
## 读取内容提供的每半日费用，旧内容沿用原来的默认价格。
static func cost(edge: Dictionary) -> int:
	return edge.get("cost_per_tick",DEFAULT_COST)
## 读取道路疲劳增量，不能通过改变画面播放速度降低消耗。
static func fatigue(edge: Dictionary) -> int:
	return edge.get("fatigue_per_tick",DEFAULT_FATIGUE)
## 按顺序连接途经点；任一段不可达时拒绝整个计划，最多允许八个不同途经点。
static func plan(data: Dictionary, origin: String, destination: Variant, tick: int, via: Variant = []) -> Dictionary:
	if not destination is String or not data.settlements.has(origin) or not data.settlements.has(destination):
		return {"ok":false,"code":"InvalidDestination"}
	if not via is Array or via.size() > 8: return {"ok":false,"code":"InvalidWaypoints"}
	var seen: Array = [origin,destination]
	for id in via:
		if not id is String or not data.settlements.has(id) or id in seen: return {"ok":false,"code":"InvalidWaypoints"}
		seen.append(id)
	var stops: Array = via.duplicate(); stops.append(destination)
	var result: Dictionary = {"ok":true,"path":[],"duration":0,"cost":0}
	var from: String = origin
	for target in stops:
		var segment: Dictionary = _shortest(data,from,target,tick)
		if not segment.ok: return segment
		result.path.append_array(segment.path)
		result.duration += segment.duration
		result.cost += segment.cost
		from = target
	if result.duration > 100000 or result.cost > 2147483647: return {"ok":false,"code":"JourneyTooLong"}
	return result
## 使用稳定的最短时耗路径；同耗时按据点与道路ID顺序选择，封锁边不预判未来开通。
static func _shortest(data: Dictionary, origin: String, destination: String, tick: int) -> Dictionary:
	var distances: Dictionary = {origin:0}
	var previous: Dictionary = {}
	var visited: Dictionary = {}
	var edges: Array = data.routes.keys(); edges.sort()
	while true:
		var current: String = ""
		var minimum: int = 2147483647
		var candidates: Array = distances.keys(); candidates.sort()
		for node in candidates:
			if not visited.has(node) and distances[node] < minimum:
				current = node; minimum = distances[node]
		if current.is_empty(): return {"ok":false,"code":"NoRoute"}
		if current == destination: break
		visited[current] = true
		for id in edges:
			var edge: Dictionary = data.routes[id]
			if edge.origin != current or not is_open(edge,tick): continue
			var distance: int = minimum + edge.duration_ticks
			if not distances.has(edge.destination) or distance < distances[edge.destination]:
				distances[edge.destination] = distance
				previous[edge.destination] = id
	var path: Array = []
	var current: String = destination
	var total: int = 0
	while current != origin:
		var id: String = previous[current]
		var edge: Dictionary = data.routes[id]
		path.push_front(id)
		total += cost(edge)*edge.duration_ticks
		current = edge.origin
	return {"ok":true,"path":path,"duration":distances[destination],"cost":total}
