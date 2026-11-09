## 世界地形的只读通行与寻路；连续整数坐标是权威位置，内部导航格不构成地图棋盘或独立时钟。
extends RefCounted
const TERRAIN: Array = ["plain", "road", "forest", "mountain", "water"]
const CROSSINGS: Array = ["ford", "bridge", "pass"]
const MODES: Array = ["foot", "army", "boat"]
const DEFAULT_COSTS: Dictionary = {
	"foot": {"plain": 10, "road": 8, "forest": 18, "mountain": 0, "water": 0, "ford": 26, "bridge": 8, "pass": 18},
	"army": {"plain": 16, "road": 12, "forest": 32, "mountain": 0, "water": 0, "ford": 42, "bridge": 12, "pass": 26},
	"boat": {"plain": 0, "road": 0, "forest": 0, "mountain": 0, "water": 8, "ford": 8, "bridge": 8, "pass": 0}
}
const MAX_COST: int = 2147483647
const EPSILON: float = 0.000000001

## 检查扁平逐行地形定义及可选成本；地图尺寸不合法、渡口放在陆地或成本为负均明确拒绝。
static func validate_map(map: Dictionary) -> Dictionary:
	for key in ["width", "height", "cell_size"]:
		if not _whole(map.get(key), 1, 100000): return _fail("InvalidMapDimensions")
	if map.width % map.cell_size != 0 or map.height % map.cell_size != 0 or map.cell_size < 2: return _fail("InvalidMapDimensions")
	var count: int = (map.width / map.cell_size) * (map.height / map.cell_size)
	if count > 65536 or not map.get("cells") is Array or map.cells.size() != count: return _fail("InvalidMapCells")
	for index in range(count):
		var cell: Variant = map.cells[index]
		if not cell is Dictionary or not cell.get("terrain") in TERRAIN: return {"ok": false, "code": "InvalidTerrain", "cell_index": index}
		if cell.has("crossing"):
			if not cell.crossing in CROSSINGS: return {"ok": false, "code": "InvalidCrossing", "cell_index": index}
			if cell.crossing == "pass" and cell.terrain != "mountain" or cell.crossing != "pass" and cell.terrain != "water": return {"ok": false, "code": "InvalidCrossingTerrain", "cell_index": index}
		if cell.has("blocked_flag") and not _blocked_flag(cell.blocked_flag): return {"ok": false, "code": "InvalidBlockedFlag", "cell_index": index}
	if not map.get("costs", {}) is Dictionary: return _fail("InvalidTravelCosts")
	for mode in map.get("costs", {}):
		if not mode in MODES or not map.costs[mode] is Dictionary: return _fail("InvalidTravelCosts")
		for terrain in map.costs[mode]:
			if not terrain in DEFAULT_COSTS[mode] or not _whole(map.costs[mode][terrain], 0, 10000): return _fail("InvalidTravelCosts")
	return {"ok": true}

## 查询任意整数点能否驻留；边界接触的每块地形均须可通过，不能贴着关闭关口或水岸擦边穿行。
static func point_valid(map: Dictionary, point: Array, mode: String = "foot", flags: Dictionary = {}) -> bool:
	if not _context(map, mode, flags).ok or not _inside(map, point): return false
	return _rate_at(map, point[0], point[1], mode, flags) > 0

## 积分直线实际经过的地形成本；cost是通行消耗而非钱或半日，任何阻断、擦角或越界使整段失败。
static func segment_cost(map: Dictionary, from: Array, to: Array, mode: String = "foot", flags: Dictionary = {}) -> Dictionary:
	var valid: Dictionary = _context(map, mode, flags)
	if not valid.ok: return valid
	return _segment(map, from, to, mode, flags)

## 返回同一积分产生的成本变化段；中间格界交点可为小数，只用于半日轨迹，持久起止点仍须整数。
static func segment_profile(map: Dictionary, from: Array, to: Array, mode: String = "foot", flags: Dictionary = {}) -> Dictionary:
	var valid: Dictionary = _context(map, mode, flags)
	if not valid.ok: return valid
	return _segment(map, from, to, mode, flags, true)

## 从真实当前位置重新规划，保留精确起止点；不读旧路线、据点名称、时钟或随机流，不替调用者移动人物。
static func plan(map: Dictionary, from: Array, to: Array, mode: String = "foot", flags: Dictionary = {}) -> Dictionary:
	var valid: Dictionary = _context(map, mode, flags)
	if not valid.ok: return valid
	if not _inside(map, from) or _rate_at(map, from[0], from[1], mode, flags) == 0: return _fail("InvalidOrigin")
	if not _inside(map, to) or _rate_at(map, to[0], to[1], mode, flags) == 0: return _fail("InvalidDestination")
	if from == to: return {"ok": true, "points": [from.duplicate()], "segment_costs": [], "cost": 0}
	var origin: int = map.cells.size(); var destination: int = origin + 1
	var distances: Dictionary = {origin: 0}; var previous: Dictionary = {}; var visited: Dictionary = {}
	var minimum: int = _minimum_rate(map, mode)
	var heap: Array = []
	_push(heap, {"node": origin, "cost": 0, "estimate": 0})
	while not heap.is_empty():
		var current: Dictionary = _pop(heap)
		var id: int = current.node
		if visited.has(id) or current.cost != distances[id]: continue
		if id == destination:
			var ids: Array = [id]
			while id != origin: id = previous[id]; ids.push_front(id)
			var points: Array = []
			for node in ids:
				var point: Array = _node_point(map, node, from, to)
				if points.is_empty() or point != points[-1]: points.append(point)
			return _path_result(map, _simplify(points), mode, flags)
		visited[id] = true
		var point: Array = _node_point(map, id, from, to)
		var neighbors: Array = _neighbors(map, point, id == origin)
		if id == origin or _near_cell(map, point, to): neighbors.append(destination)
		for next in neighbors:
			if visited.has(next): continue
			var end: Array = _node_point(map, next, from, to)
			var segment: Dictionary = _segment(map, point, end, mode, flags)
			if not segment.ok: continue
			var cost: int = distances[id] + segment.cost
			if cost > MAX_COST or distances.has(next) and cost >= distances[next]: continue
			distances[next] = cost; previous[next] = id
			var dx: float = float(end[0] - to[0]); var dy: float = float(end[1] - to[1])
			_push(heap, {"node": next, "cost": cost, "estimate": cost + floori(sqrt(dx * dx + dy * dy) * minimum)})
	return _fail("NoRoute")

## 统一公共查询的地图、旅行模式及旗标边界；未知模式和非布尔旗标不能悄悄变成步行或开放。
static func _context(map: Dictionary, mode: String, flags: Dictionary) -> Dictionary:
	var valid: Dictionary = validate_map(map)
	if not valid.ok: return valid
	if not mode in MODES: return _fail("InvalidTravelMode")
	for key in flags:
		if not key is String or not flags[key] is bool: return _fail("InvalidTerrainFlags")
	return {"ok": true}

## 检查半开地图边界；width/height本身属于图外，不把错误终点夹回地图。
static func _inside(map: Dictionary, point: Array) -> bool:
	return point.size() == 2 and _whole(point[0], 0, map.width - 1) and _whole(point[1], 0, map.height - 1)

## 按当前格与模式解释通行；水上旅行仍留在河道，陆行只能经渡口或桥过水，可选成本不改变此边界。
static func _cell_rate(map: Dictionary, index: int, mode: String, flags: Dictionary) -> int:
	var cell: Dictionary = map.cells[index]
	if flags.get(cell.get("blocked_flag", ""), false): return 0
	var terrain: String = cell.terrain
	if mode == "boat":
		if terrain != "water": return 0
	else:
		if terrain == "water" and not cell.get("crossing") in ["ford", "bridge"]: return 0
		terrain = cell.get("crossing", terrain)
	return map.get("costs", {}).get(mode, {}).get(terrain, DEFAULT_COSTS[mode][terrain])

## 对恰好落在格界的点取相邻地形中最严格成本；图外邻格不参与，使地图最外沿仍可正常行走。
static func _rate_at(map: Dictionary, x: float, y: float, mode: String, flags: Dictionary) -> int:
	var column: int = floori(x / map.cell_size); var row: int = floori(y / map.cell_size)
	if absf(x / map.cell_size - roundf(x / map.cell_size)) < EPSILON: column = roundi(x / map.cell_size)
	if absf(y / map.cell_size - roundf(y / map.cell_size)) < EPSILON: row = roundi(y / map.cell_size)
	var columns: Array = [column]; var rows: Array = [row]
	if column > 0 and absf(x / map.cell_size - column) < EPSILON: columns.append(column - 1)
	if row > 0 and absf(y / map.cell_size - row) < EPSILON: rows.append(row - 1)
	var width: int = map.width / map.cell_size; var height: int = map.height / map.cell_size
	var result: int = 0
	for cy in rows:
		for cx in columns:
			if cx < 0 or cy < 0 or cx >= width or cy >= height: return 0
			var rate: int = _cell_rate(map, cy * width + cx, mode, flags)
			if rate == 0: return 0
			result = maxi(result, rate)
	return result

## 按全部网格交点切分直线而非按帧采样；交点也检查四周，长线和斜线都不能跳过窄障碍。
static func _segment(map: Dictionary, from: Array, to: Array, mode: String, flags: Dictionary, include_profile: bool = false) -> Dictionary:
	if not _inside(map, from) or _rate_at(map, from[0], from[1], mode, flags) == 0: return _fail("InvalidOrigin")
	if not _inside(map, to) or _rate_at(map, to[0], to[1], mode, flags) == 0: return _fail("InvalidDestination")
	var dx: float = float(to[0] - from[0]); var dy: float = float(to[1] - from[1])
	var length: float = sqrt(dx * dx + dy * dy)
	if length == 0: return {"ok": true, "cost": 0, "raw_cost": 0.0, "spans": []} if include_profile else {"ok": true, "cost": 0}
	var cuts: Array[float] = [0.0, 1.0]
	for axis in range(2):
		var difference: float = float(to[axis] - from[axis])
		if difference == 0: continue
		var first: int = floori(float(mini(from[axis], to[axis])) / map.cell_size) + 1
		var last: int = floori(float(maxi(from[axis], to[axis])) / map.cell_size)
		for boundary in range(first, last + 1):
			var portion: float = (boundary * map.cell_size - from[axis]) / difference
			if portion > EPSILON and portion < 1.0 - EPSILON: cuts.append(portion)
	cuts.sort()
	var total: float = 0.0; var spans: Array = []
	for index in range(cuts.size()):
		var at: float = cuts[index]
		if _rate_at(map, from[0] + dx * at, from[1] + dy * at, mode, flags) == 0: return _fail("SegmentBlocked")
		if index == 0 or at - cuts[index - 1] < EPSILON: continue
		var middle: float = (at + cuts[index - 1]) / 2.0
		var rate: int = _rate_at(map, from[0] + dx * middle, from[1] + dy * middle, mode, flags)
		if rate == 0: return _fail("SegmentBlocked")
		var before: float = total
		total += length * (at - cuts[index - 1]) * rate
		if include_profile:
			var end: Array = [from[0] + dx * at, from[1] + dy * at]
			if not spans.is_empty() and spans[-1].rate == rate:
				spans[-1].to = end; spans[-1].end_cost = total
			else:
				var start: Array = spans[-1].to.duplicate() if not spans.is_empty() else from.duplicate()
				spans.append({"from": start, "to": end, "start_cost": before, "end_cost": total, "rate": rate})
	if total > MAX_COST: return _fail("RouteTooLong")
	var result: Dictionary = {"ok": true, "cost": ceili(total - EPSILON)}
	if include_profile:
		result.raw_cost = minf(total, float(result.cost)); result.spans = spans
		if not spans.is_empty(): spans[-1].end_cost = result.raw_cost
	return result

## 导航节点使用格心，两个特殊节点直接引用原始起止点的副本；返回路径不会暴露输入数组引用。
static func _node_point(map: Dictionary, node: int, from: Array, to: Array) -> Array:
	if node == map.cells.size(): return from.duplicate()
	if node == map.cells.size() + 1: return to.duplicate()
	var width: int = map.width / map.cell_size
	return [(node % width) * map.cell_size + map.cell_size / 2, (node / width) * map.cell_size + map.cell_size / 2]

## 稳定枚举周边八格；起点另接本格格心，每条候选边仍经过完整直线通行核验。
static func _neighbors(map: Dictionary, point: Array, include_self: bool) -> Array:
	var width: int = map.width / map.cell_size; var height: int = map.height / map.cell_size
	var cx: int = point[0] / map.cell_size; var cy: int = point[1] / map.cell_size
	var result: Array = []
	for dy in range(-1, 2):
		for dx in range(-1, 2):
			if not include_self and dx == 0 and dy == 0: continue
			if cx + dx >= 0 and cx + dx < width and cy + dy >= 0 and cy + dy < height: result.append((cy + dy) * width + cx + dx)
	return result

## 终点附近格心连接精确目的地，避免抵达时把人物吸附到格心或城门。
static func _near_cell(map: Dictionary, a: Array, b: Array) -> bool:
	return absi(a[0] / map.cell_size - b[0] / map.cell_size) <= 1 and absi(a[1] / map.cell_size - b[1] / map.cell_size) <= 1

## 启发式只用模式最小正成本，忽略障碍仍是下界；不会因图形或读取次数改变寻路结果。
static func _minimum_rate(map: Dictionary, mode: String) -> int:
	var result: int = 10000
	for terrain in DEFAULT_COSTS[mode]:
		var rate: int = map.get("costs", {}).get(mode, {}).get(terrain, DEFAULT_COSTS[mode][terrain])
		if rate > 0: result = mini(result, rate)
	return result

## 仅移除同一直线上多余格心，不做可能切入水域或跨越关口的任意视觉平滑。
static func _simplify(points: Array) -> Array:
	var result: Array = []
	for point in points:
		while result.size() >= 2:
			var a: Array = result[-2]; var b: Array = result[-1]
			var dx: int = b[0] - a[0]; var dy: int = b[1] - a[1]
			var ex: int = point[0] - b[0]; var ey: int = point[1] - b[1]
			if dx * ey != dy * ex or dx * ex + dy * ey < 0: break
			result.pop_back()
		result.append(point)
	return result

## 对最终折线逐段使用同源查询重新计价；每段向上取整，总成本始终等于可提交各段的成本之和。
static func _path_result(map: Dictionary, points: Array, mode: String, flags: Dictionary) -> Dictionary:
	var costs: Array = []; var total: int = 0
	for index in range(1, points.size()):
		var segment: Dictionary = _segment(map, points[index - 1], points[index], mode, flags)
		if not segment.ok: return segment
		costs.append(segment.cost); total += segment.cost
		if total > MAX_COST: return _fail("RouteTooLong")
	return {"ok": true, "points": points, "segment_costs": costs, "cost": total}

## 最小堆按预计成本、实际成本和节点编号稳定排序，同成本路线不依赖字典遍历偶然顺序。
static func _before(a: Dictionary, b: Dictionary) -> bool:
	if a.estimate != b.estimate: return a.estimate < b.estimate
	if a.cost != b.cost: return a.cost < b.cost
	return a.node < b.node

## 将候选插入本次查询私有堆，避免大地图每一步扫描所有已发现格。
static func _push(heap: Array, item: Dictionary) -> void:
	heap.append(item)
	var index: int = heap.size() - 1
	while index > 0:
		var parent: int = (index - 1) / 2
		if not _before(heap[index], heap[parent]): break
		var swap: Dictionary = heap[parent]; heap[parent] = heap[index]; heap[index] = swap; index = parent

## 取出最小候选并恢复堆；调用者保证非空，所有存储均属于本次纯查询。
static func _pop(heap: Array) -> Dictionary:
	var result: Dictionary = heap[0]; var last: Dictionary = heap.pop_back()
	if heap.is_empty(): return result
	heap[0] = last
	var index: int = 0
	while index * 2 + 1 < heap.size():
		var child: int = index * 2 + 1
		if child + 1 < heap.size() and _before(heap[child + 1], heap[child]): child += 1
		if not _before(heap[child], heap[index]): break
		var swap: Dictionary = heap[child]; heap[child] = heap[index]; heap[index] = swap; index = child
	return result

## 仅接受范围内真整数，不把浮点坐标或字符串悄悄取整后写入路线。
static func _whole(value: Variant, low: int, high: int) -> bool:
	return typeof(value) == TYPE_INT and value >= low and value <= high

## 所有失败均保留稳定代码，不构造半条已移动路线或修改调用者状态。
static func _fail(code: String) -> Dictionary:
	return {"ok": false, "code": code}

## 保留既有短旗标合同；仅合法限定静态ID可超过80字符，不能用长任意文本绕过边界。
static func _blocked_flag(value: Variant) -> bool:
	if not value is String or value.is_empty(): return false
	if not "::" in value: return value.length()<=80
	if value.length()>114: return false
	var qualified := RegEx.new()
	qualified.compile("^[a-z][a-z0-9_-]{0,31}::[a-z][a-z0-9_.-]{0,79}(?![\\s\\S])")
	return qualified.search(value)!=null
