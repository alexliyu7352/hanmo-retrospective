## 将真实道路进度投影为画面坐标；浮点插值只用于显示，绝不反写世界位置。
extends RefCounted
const Location = preload("res://core/world_location.gd")
## 拼接道路端点和作者定义的中间折线，逻辑路程仍由道路时耗决定。
static func points(data: Dictionary, edge_id: String) -> PackedVector2Array:
	var edge: Dictionary = data.routes[edge_id]
	var result = PackedVector2Array()
	var a: Array = data.settlements[edge.origin].map_xy
	result.append(Vector2(a[0],a[1]))
	for xy in edge.get("shape",[]): result.append(Vector2(xy[0],xy[1]))
	var b: Array = data.settlements[edge.destination].map_xy
	result.append(Vector2(b[0],b[1]))
	return result
## 按折线弧长分配显示进度；重复坐标不产生除零，也不改变规则长度。
static func along(line: PackedVector2Array, ratio: float) -> Vector2:
	if line.is_empty(): return Vector2.ZERO
	var total: float = 0.0
	for i in range(1,line.size()): total += line[i-1].distance_to(line[i])
	var left: float = clampf(ratio,0.0,1.0)*total
	for i in range(1,line.size()):
		var length: float = line[i-1].distance_to(line[i])
		if length > 0.0 and left <= length: return line[i-1].lerp(line[i],left/length)
		left -= length
	return line[-1]
## 读取唯一位置：据点取入口坐标，路上取已保存的边与进度。
static func actor_position(state: Dictionary, data: Dictionary, actor_id: String) -> Vector2:
	var location: Dictionary = Location.resolve(state,actor_id)
	if location.kind=="Wilderness": return Vector2(location.point[0],location.point[1])
	if location.kind == "OnRoute":
		var edge: Dictionary = data.routes[location.edge_id]
		return along(points(data,edge.id),float(location.progress)/(edge.duration_ticks*100))
	var xy: Array = data.settlements[location.settlement_id].map_xy
	return Vector2(xy[0],xy[1])

## 从当前弧长位置截取仅供显示的余程；反向路线保留折点顺序，零长边安全跳过。
static func remaining(line: PackedVector2Array, ratio: float, reverse: bool) -> PackedVector2Array:
	if line.is_empty(): return PackedVector2Array()
	var total: float=0.0
	var distances: Array[float]=[0.0]
	for i in range(1,line.size()):
		total+=line[i-1].distance_to(line[i]); distances.append(total)
	var at: float=clampf(ratio,0.0,1.0)*total
	var result=PackedVector2Array([along(line,ratio)])
	if reverse:
		for i in range(line.size()-1,-1,-1):
			if distances[i]<at and not line[i].is_equal_approx(result[-1]): result.append(line[i])
	else:
		for i in range(line.size()):
			if distances[i]>at and not line[i].is_equal_approx(result[-1]): result.append(line[i])
	return result
