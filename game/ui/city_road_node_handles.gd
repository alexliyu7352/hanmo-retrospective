## 有限路点编辑叠层：只把屏幕拖放转换为作者节点坐标，不维护另一个路网或建设状态。
extends Control
signal node_dragged(id: String, position: Vector2, finished: bool)
var canvas: Control
var nodes: Dictionary = {}
var _dragging: String = ""
var _pointer: int = -1
var _valid: bool = true

## 模式开启时接管画面内拖放，顶部和底部操作仍由外层UI处理。
func _init() -> void:
	mouse_filter = Control.MOUSE_FILTER_STOP
	clip_contents = true

## 节点位置取自当前接受或候选路段，共用节点只画一个手柄。
func configure(roads: Array, valid: bool = true) -> void:
	nodes.clear(); _valid = valid
	for road in roads:
		for index in range(road.get("node_ids", []).size()):
			var id: String = road.node_ids[index]
			if not nodes.has(id): nodes[id] = {"position": Vector2(road.points[index][0], road.points[index][1]), "uses": 0}
			nodes[id].uses += 1
	queue_redraw()

## 取消或模式切换终止旧指针，迟到抬起不会生成新的候选。
func cancel_drag() -> void:
	_dragging = ""; _pointer = -1

## 画布在路点模式缩进安全取景区，手柄和输入显式跨同一控件坐标系。
func project(point: Vector2) -> Vector2:
	return canvas.global_position - global_position + canvas.logical_to_local(point)

## 命中按屏幕44像素直径计算，窗口缩小也保留可操作大小。
func node_at(point: Vector2) -> String:
	if canvas == null: return ""
	var closest: String = ""; var distance: float = 22.0
	for id in nodes:
		var current: float = point.distance_to(project(nodes[id].position))
		if current <= distance: closest = id; distance = current
	return closest

## 鼠标或单触点共用一次拖动，放下仍不确定任何显示修改。
func _gui_input(event: InputEvent) -> void:
	if event is InputEventMouseButton and event.button_index == MOUSE_BUTTON_LEFT:
		if event.pressed:
			_dragging = node_at(event.position); _pointer = -2
		elif _pointer == -2:
			_emit_drag(event.position, true); cancel_drag()
	elif event is InputEventMouseMotion and _pointer == -2: _emit_drag(event.position, false)
	elif event is InputEventScreenTouch:
		if event.pressed and _pointer == -1: _dragging = node_at(event.position); _pointer = event.index
		elif not event.pressed and _pointer == event.index: _emit_drag(event.position, true); cancel_drag()
	elif event is InputEventScreenDrag and event.index == _pointer: _emit_drag(event.position, false)
	accept_event()

## 透视坐标只经现有等比画布逆变换，不额外旋转或拉伸道路。
func _emit_drag(point: Vector2, finished: bool) -> void:
	if _dragging.is_empty() or canvas == null: return
	node_dragged.emit(_dragging, canvas.canvas_transform().affine_inverse() * (point + global_position - canvas.global_position), finished)

## 圆点为单路节点，菱形为已声明连接点；图形和颜色共同区分，不依赖红绿。
func _draw() -> void:
	if canvas == null: return
	for id in nodes:
		var point: Vector2 = project(nodes[id].position)
		var color: Color = Color("285f66") if _valid else Color("ae4035")
		if nodes[id].uses > 1:
			var polygon: PackedVector2Array = PackedVector2Array([point + Vector2(0,-12), point + Vector2(12,0), point + Vector2(0,12), point + Vector2(-12,0)])
			draw_colored_polygon(polygon, Color("f1e5c5")); polygon.append(polygon[0]); draw_polyline(polygon, color, 3, true)
		else:
			draw_circle(point, 10, Color("f1e5c5")); draw_arc(point, 10, 0, TAU, 32, color, 3, true)
		draw_circle(point, 3, color)
