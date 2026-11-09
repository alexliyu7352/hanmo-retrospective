## 按同一半日的连续地形轨迹只读检测会面，保留各人的实际锚点与未走完的折线。
extends RefCounted
const RADIUS: float = 4.0
const TIME_DEN: int = 1000000000
const EPSILON: float = 0.000000000001

## 无行动停驻使用覆盖半日的零位移轨迹；保留实际位置，不生成行动、余程或可兑现预算。
static func stationary(actor_id: String, location: Dictionary) -> Dictionary:
	if actor_id.is_empty() or location.get("kind")!="Wilderness" or not location.get("map_id") is String or location.map_id.is_empty() or not location.get("origin") is String or location.origin.is_empty() or not _point(location.get("point")): return {}
	var point: Array=location.point.duplicate()
	return {"actor_id":actor_id,"action_id":"","map_id":location.map_id,"origin":location.origin,"stationary":true,"location":location.duplicate(true),"params":{"points":[point.duplicate()]},"segments":[{"from":point.duplicate(),"to":point.duplicate(),"start_cost":0,"end_cost":1,"point_index":0}],"budget":1}

## 检查轨迹完整覆盖半日且各段连续；拒绝损坏输入，不推断身份、不补造移动。
static func validate_trace(trace: Dictionary) -> Dictionary:
	for key in ["actor_id", "map_id", "origin"]:
		if not trace.get(key) is String or trace[key].is_empty(): return {"ok": false, "code": "InvalidTerrainTraceIdentity"}
	if not trace.get("action_id") is String or not trace.get("stationary",false) is bool: return {"ok":false,"code":"InvalidTerrainTraceIdentity"}
	if trace.get("stationary",false):
		if not trace.get("location") is Dictionary: return {"ok":false,"code":"InvalidStationaryTerrainTrace"}
		var expected: Dictionary=stationary(trace.actor_id,trace.location)
		return {"ok":true} if not expected.is_empty() and trace==expected else {"ok":false,"code":"InvalidStationaryTerrainTrace"}
	if trace.action_id.is_empty(): return {"ok":false,"code":"InvalidTerrainTraceIdentity"}
	# 本步预算允许加上此前未用的整数成本，显式上界仍保证双精度时间可区分。
	if typeof(trace.get("budget")) != TYPE_INT or trace.budget <= 0 or trace.budget > 4294967294: return {"ok": false, "code": "InvalidTerrainTraceBudget"}
	var params: Variant = trace.get("params")
	if not params is Dictionary or not params.get("points") is Array or params.points.is_empty(): return {"ok": false, "code": "InvalidTerrainTracePoints"}
	for point in params.points:
		if not _point(point): return {"ok": false, "code": "InvalidTerrainTracePoints"}
	if params.has("route_index") and (typeof(params.route_index) != TYPE_INT or params.route_index < 1): return {"ok": false, "code": "InvalidTerrainTracePointIndex"}
	if params.has("travel_credit") and (typeof(params.travel_credit) != TYPE_INT or params.travel_credit < 0 or params.travel_credit > 2147483647): return {"ok": false, "code": "InvalidTerrainTraceBudget"}
	if not params.get("encounter_id", "") is String or not params.get("met_actions", []) is Array: return {"ok": false, "code": "InvalidTerrainTraceEncounter"}
	for action in params.get("met_actions", []):
		if not action is String: return {"ok": false, "code": "InvalidTerrainTraceEncounter"}
	if not trace.get("segments") is Array or trace.segments.is_empty(): return {"ok": false, "code": "InvalidTerrainTraceSegments"}
	var previous_cost: float = 0.0; var previous_point: Array = []; var previous_index: int = -1
	for index in range(trace.segments.size()):
		var segment: Variant = trace.segments[index]
		if not segment is Dictionary or not _trace_point(segment.get("from")) or not _trace_point(segment.get("to")): return {"ok": false, "code": "InvalidTerrainTraceSegments"}
		if not _number(segment.get("start_cost")) or not _number(segment.get("end_cost")): return {"ok": false, "code": "InvalidTerrainTraceTime"}
		if segment.start_cost != previous_cost or segment.end_cost <= segment.start_cost or segment.end_cost > trace.budget: return {"ok": false, "code": "InvalidTerrainTraceTime"}
		if not previous_point.is_empty() and not _same_point(segment.from, previous_point): return {"ok": false, "code": "DiscontinuousTerrainTrace"}
		var point_index: Variant = segment.get("point_index", index)
		if typeof(point_index) != TYPE_INT or point_index < previous_index or point_index < 0 or point_index > params.points.size(): return {"ok": false, "code": "InvalidTerrainTracePointIndex"}
		if point_index == params.points.size() and (not _same_point(segment.from, segment.to) or not _same_point(segment.to, params.points[-1])): return {"ok": false, "code": "InvalidTerrainTraceFinishedTail"}
		previous_cost = segment.end_cost; previous_point = segment.to; previous_index = point_index
	if previous_cost != trace.budget: return {"ok": false, "code": "IncompleteTerrainTrace"}
	return {"ok": true}

## 对有效轨迹按首次时刻分配会面；重复身份整组拒绝，每个人在同一半日最多参加一场。
static func detect(traces: Array, entrances: Dictionary = {}) -> Array:
	var valid: Array = []; var people: Dictionary = {}; var actions: Dictionary = {}
	for trace in traces:
		if not trace is Dictionary or not validate_trace(trace).ok: continue
		valid.append(trace)
		people[trace.actor_id] = people.get(trace.actor_id, 0) + 1
		if not trace.action_id.is_empty(): actions[trace.action_id] = actions.get(trace.action_id, 0) + 1
	var candidates: Array = []
	for i in range(valid.size()):
		if people[valid[i].actor_id] > 1 or actions.get(valid[i].action_id,0) > 1: continue
		for j in range(i + 1, valid.size()):
			if people[valid[j].actor_id] > 1 or actions.get(valid[j].action_id,0) > 1: continue
			var hit: Dictionary = _intersection(valid[i], valid[j], entrances)
			if not hit.is_empty(): candidates.append(hit)
	candidates.sort_custom(before)
	var selected: Array = []; var occupied: Dictionary = {}
	for hit in candidates:
		if occupied.has(hit.a) or occupied.has(hit.b): continue
		selected.append(hit); occupied[hit.a] = true; occupied[hit.b] = true
	return selected

## 查询两名旅人的最早有效接触；入口表为地图ID到整数圆心数组，不读取世界或消耗预算。
static func intersection(a: Dictionary, b: Dictionary, entrances: Dictionary = {}) -> Dictionary:
	if not validate_trace(a).ok or not validate_trace(b).ok: return {}
	return _intersection(a, b, entrances)

## 固定分母记录无理数交圆时刻的十亿分之一近似，规范人物ID保证读序不改变选择。
static func before(a: Dictionary, b: Dictionary) -> bool:
	var left: int = a.time_num * b.time_den; var right: int = b.time_num * a.time_den
	if left != right: return left < right
	return a.b < b.b if a.a == b.a else a.a < b.a

## 规范人物次序后计算同步相对运动；已经会面、不同地图及双方全程不动均不生成会话。
static func _intersection(first: Dictionary, second: Dictionary, entrances: Dictionary) -> Dictionary:
	var a: Dictionary = first if first.actor_id < second.actor_id else second
	var b: Dictionary = second if first.actor_id < second.actor_id else first
	if a.actor_id == b.actor_id or (not a.action_id.is_empty() and a.action_id == b.action_id) or a.map_id != b.map_id: return {}
	if not a.params.get("encounter_id", "").is_empty() or not b.params.get("encounter_id", "").is_empty(): return {}
	if a.action_id in b.params.get("met_actions", []) or b.action_id in a.params.get("met_actions", []): return {}
	if not _moves(a) and not _moves(b): return {}
	var centers: Variant = entrances.get(a.map_id, [])
	if not centers is Array: return {}
	for center in centers:
		if not _point(center): return {}
	var best: Dictionary = {}
	for ai in range(a.segments.size()):
		var sa: Dictionary = _segment(a, ai)
		if sa.point_index >= a.params.points.size(): continue
		for bi in range(b.segments.size()):
			var sb: Dictionary = _segment(b, bi)
			if sb.point_index >= b.params.points.size(): continue
			var start: float = maxf(sa.start, sb.start); var end: float = minf(sa.end, sb.end)
			if start > end: continue
			var pa: Array = _position(sa, start); var pb: Array = _position(sb, start)
			var interval: Array = _inside([pa[0] - pb[0], pa[1] - pb[1]], [sa.vx - sb.vx, sa.vy - sb.vy], end - start)
			if interval.is_empty(): continue
			var time: float = _outside_entrances(sa, sb, start + interval[0], start + interval[1], centers)
			if time < 0.0: continue
			var anchor_a: Dictionary = _anchor(a, sa, time); var anchor_b: Dictionary = _anchor(b, sb, time)
			# 完成最终折点的时刻和静止尾段都不能重新挂起已经结束的行动。
			if anchor_a.is_empty() or anchor_b.is_empty(): continue
			var hit: Dictionary = {"a": a.actor_id, "b": b.actor_id, "time_num": roundi(time * TIME_DEN), "time_den": TIME_DEN,
				"anchors": {a.actor_id: anchor_a.location, b.actor_id: anchor_b.location},
				"traces": {a.actor_id: anchor_a.trace, b.actor_id: anchor_b.trace}}
			if best.is_empty() or before(hit, best): best = hit
	return best

## 用双精度标量求两次移动之间的时间区间，避免单精度Vector2在大地图坐标处丢失细节。
static func _segment(trace: Dictionary, index: int) -> Dictionary:
	var source: Dictionary = trace.segments[index]
	var start: float = float(source.start_cost) / trace.budget
	var end: float = float(source.end_cost) / trace.budget
	return {"from": source.from, "to": source.to, "start": start, "end": end,
		"vx": float(source.to[0] - source.from[0]) / (end - start),
		"vy": float(source.to[1] - source.from[1]) / (end - start), "point_index": source.get("point_index", index)}

## 在给定有效线段上投影自己的位置；不会用对方或共同中点替换真实路线。
static func _position(segment: Dictionary, time: float) -> Array:
	var elapsed: float = clampf(time, segment.start, segment.end) - segment.start
	return [segment.from[0] + segment.vx * elapsed, segment.from[1] + segment.vy * elapsed]

## 解相对匀速运动进入半径四圆的区间，几何相交但时刻不同的轨迹不满足此查询。
static func _inside(offset: Array, velocity: Array, duration: float) -> Array:
	var speed_squared: float = velocity[0] * velocity[0] + velocity[1] * velocity[1]
	if speed_squared <= EPSILON:
		return [0.0, duration] if offset[0] * offset[0] + offset[1] * offset[1] <= RADIUS * RADIUS else []
	var closest: float = -(offset[0] * velocity[0] + offset[1] * velocity[1]) / speed_squared
	var x: float = offset[0] + velocity[0] * closest; var y: float = offset[1] + velocity[1] * closest
	var remainder: float = RADIUS * RADIUS - x * x - y * y
	if remainder < -EPSILON: return []
	var half_span: float = sqrt(maxf(0.0, remainder) / speed_squared)
	var enter: float = maxf(0.0, closest - half_span); var leave: float = minf(duration, closest + half_span)
	return [] if enter > leave + EPSILON else [minf(enter, duration), maxf(leave, 0.0)]

## 排除任一旅人位于据点入口半径内的时间；仍在接触距离内一起离城后可在城外正常相遇。
static func _outside_entrances(a: Dictionary, b: Dictionary, start: float, end: float, centers: Array) -> float:
	var blocked: Array = []
	for center in centers:
		for segment in [a, b]:
			var point: Array = _position(segment, start)
			var interval: Array = _inside([point[0] - center[0], point[1] - center[1]], [segment.vx, segment.vy], end - start)
			if not interval.is_empty(): blocked.append([start + interval[0], start + interval[1]])
	blocked.sort_custom(_interval_before)
	var candidate: float = start
	for interval in blocked:
		if interval[1] < candidate: continue
		if interval[0] > candidate: break
		candidate = interval[1] + 1.0 / TIME_DEN
	return candidate if candidate <= end else -1.0

## 入口排除区间按起点、终点排序，使入口定义顺序不会改变会面。
static func _interval_before(a: Array, b: Array) -> bool:
	return a[1] < b[1] if a[0] == b[0] else a[0] < b[0]

## 将连续位置各自量化为整数锚点，深复制余程；每人最多半格对角误差，不把人物吸到同一点。
static func _anchor(source: Dictionary, segment: Dictionary, time: float) -> Dictionary:
	if source.get("stationary",false): return {"location":source.location.duplicate(true),"trace":source.duplicate(true)}
	var index: int = segment.point_index
	var position: Array = _position(segment, time)
	if time >= segment.end - EPSILON and index < source.params.points.size() and _same_point(segment.to, source.params.points[index]): index += 1
	while index < source.params.points.size() and absf(position[0] - source.params.points[index][0]) < EPSILON and absf(position[1] - source.params.points[index][1]) < EPSILON: index += 1
	if index >= source.params.points.size(): return {}
	var copied: Dictionary = source.duplicate(true)
	copied.params.points = source.params.points.slice(index).duplicate(true)
	if copied.params.has("route_index"): copied.params.route_index += index
	# 会面已消费本半日；未走完的预算在截断时作废，不能在恢复行旅时再次兑现。
	copied.params.travel_credit = 0
	return {"location": {"kind": "Wilderness", "map_id": source.map_id, "point": [roundi(position[0]), roundi(position[1])], "origin": source.origin}, "trace": copied}

## 根据真实端点判断半日内是否发生位移，元数据moving不能凭空产生相遇。
static func _moves(trace: Dictionary) -> bool:
	for segment in trace.segments:
		if not _same_point(segment.from, segment.to): return true
	return false

## 连续世界仍以非负整数落点持久化，拒绝小数、布尔与无界坐标进入几何计算。
static func _point(value: Variant) -> bool:
	return value is Array and value.size() == 2 and typeof(value[0]) == TYPE_INT and typeof(value[1]) == TYPE_INT and value[0] >= 0 and value[1] >= 0 and value[0] <= 100000 and value[1] <= 100000

## 地形成本改变处的临时格界交点可为小数，保存锚点仍通过独立整数校验与取整输出。
static func _trace_point(value: Variant) -> bool:
	return value is Array and value.size() == 2 and _number(value[0]) and _number(value[1]) and value[0] >= 0 and value[1] >= 0 and value[0] <= 100000 and value[1] <= 100000

## 拒绝布尔、非数和无限值参与半日几何，有限小数只用于未持久化的轨迹计算。
static func _number(value: Variant) -> bool:
	return typeof(value) in [TYPE_INT, TYPE_FLOAT] and is_finite(float(value))

## 整数端点与其浮点格界副本按坐标比较，容器元素类型不同不能制造瞬移或虚假位移。
static func _same_point(a: Array, b: Array) -> bool:
	return absf(float(a[0]) - float(b[0])) < EPSILON and absf(float(a[1]) - float(b[1])) < EPSILON
