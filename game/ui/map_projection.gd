## 保留旧UI投影路径、常量和静态入口；算法唯一位于core/views，转发不复制世界或重算第二份几何。
extends RefCounted
const Core=preload("res://core/views/map_projection.gd")
const Location=preload("res://core/world_location.gd")

## 兼容转发：拼接道路端点和作者定义的中间折线，逻辑路程仍由道路时耗决定。
static func points(data: Dictionary, edge_id: String) -> PackedVector2Array:
	return Core.points(data,edge_id)

## 兼容转发：按折线弧长分配显示进度；重复坐标不产生除零，也不改变规则长度。
static func along(line: PackedVector2Array, ratio: float) -> Vector2:
	return Core.along(line,ratio)

## 兼容转发：读取唯一位置：据点取入口坐标，路上取已保存的边与进度。
static func actor_position(state: Dictionary, data: Dictionary, actor_id: String) -> Vector2:
	return Core.actor_position(state,data,actor_id)

## 兼容转发：从当前弧长位置截取仅供显示的余程；反向路线保留折点顺序，零长边安全跳过。
static func remaining(line: PackedVector2Array, ratio: float, reverse: bool) -> PackedVector2Array:
	return Core.remaining(line,ratio,reverse)
