## 城市分层画布：共用等比变换、足点层序与像素命中；不写入布局源或任何世界状态。
extends Control
const Model = preload("res://ui/city_layout_model.gd")
signal place_requested(instance_id: String)
signal selection_changed(instance_id: String)
signal placement_dragged(instance_id: String, logical_position: Vector2, finished: bool)
var _descriptor: Dictionary = {}
var _registry: RefCounted
var _images: Dictionary = {}
var _labels: Array = []
var _selected: String = ""
var _pressed: String = ""
var _pointer_id: int = -1
var _generation: int = 0
var show_labels: bool = true
var editing: bool = false
var show_plots: bool = false
var _dragging: String = ""
var _drag_offset: Vector2 = Vector2.ZERO
var _preview: Dictionary = {}
var _road_preview: Dictionary = {}
var _camera: Dictionary = {}

## 保留鼠标和键盘访问；重排时标签与图像使用同一画布变换。
func _init() -> void:
	mouse_filter = Control.MOUSE_FILTER_STOP
	clip_contents = true
	texture_repeat = CanvasItem.TEXTURE_REPEAT_ENABLED
	resized.connect(_arrange_labels)

## 所有描述和图像先验证再发布，失败时前一帧与标签仍然有效。
func configure(document: Dictionary, layout: Dictionary, registry: RefCounted) -> Dictionary:
	var result: Dictionary = Model.resolve(document, layout, registry)
	if not result.ok: return result
	var images: Dictionary = {}
	for row in result.layers:
		for part in row.parts:
			if images.has(part.asset_id): continue
			var image: Image = registry.texture("backgrounds", part.asset_id).get_image()
			if image == null or image.is_empty(): return {"ok": false, "code": "MissingCityPixels"}
			if image.is_compressed() and image.decompress() != OK: return {"ok": false, "code": "CityTextureDecodeFailed"}
			images[part.asset_id] = image
	_descriptor = result.duplicate(true); _registry = registry; _images = images
	_preview = {}; _road_preview = {}
	_generation += 1; _pressed = ""; _pointer_id = -1; _dragging = ""
	if find_instance(_selected).is_empty(): _selected = ""
	_rebuild_labels(); queue_redraw()
	return {"ok": true}

## 返回独立绘制快照，调用者不能通过它改写当前画面。
func describe() -> Dictionary:
	return _descriptor.duplicate(true)

## 稳定实例ID用于进入与返回；资源替换不改变对象身份。
func find_instance(id: String) -> Dictionary:
	for row in _descriptor.get("layers", []):
		if row.id == id: return row.duplicate(true)
	return {}

## 选择仅改变描边与按钮强调，不让不存在或装饰对象成为可访问场所。
func select_instance(id: String) -> void:
	var row: Dictionary = find_instance(id)
	if not id.is_empty() and (row.is_empty() or not row.interactive): return
	_selected = id
	_update_label_styles(); queue_redraw(); selection_changed.emit(id)

## 由同一实例入口发信号，调用者决定进入独立展示还是未来的真实世界命令。
func activate_instance(id: String) -> bool:
	var row: Dictionary = find_instance(id)
	if row.is_empty() or not row.interactive: return false
	select_instance(id)
	if not editing: place_requested.emit(id)
	return true

## 候选只覆盖预览层；原描述与图像不变，取消后可立即恢复原像素。
func preview_instance(id: String, position: Vector2, valid: bool, access: Array = []) -> void:
	var row: Dictionary = find_instance(id)
	if row.is_empty(): return
	_preview = {"id": id, "offset": position - Vector2(row.foot[0], row.foot[1]), "valid": valid, "access": access.duplicate(true)}
	_road_preview = {}; _arrange_labels()

## 道路候选使用同一真实材质与宽度，只移动实际折线图层。
func preview_road(id: String, points: Array, valid: bool, access: Array = []) -> void:
	_road_preview = {"id": id, "points": points.duplicate(true), "valid": valid, "access": access.duplicate(true)}; _preview = {}; queue_redraw()

## 共用节点影响多条路时一起预览；非法形状只画候选线，原路面和门前连接保留。
func preview_roads(roads: Array, valid: bool, access: Array = [], primary: String = "") -> void:
	_road_preview = {"id": primary, "roads": roads.duplicate(true), "valid": valid, "access": access.duplicate(true)}; _preview = {}; queue_redraw()

## 路面从同一候选取坐标，不让相连道路使用不同帧的节点位置。
func _road_points(road: Dictionary) -> Array:
	if _road_preview.has("roads"):
		if not _road_preview.valid: return road.points
		for candidate in _road_preview.roads:
			if candidate.id == road.id: return candidate.points
	return _road_preview.points if _road_preview.get("id") == road.id and _road_preview.has("points") else road.points

## 关闭预览立即恢复原层，不重选对象或改变布局数据。
func clear_preview() -> void:
	_preview = {}; _road_preview = {}; _dragging = ""; _pressed = ""; _pointer_id = -1; _arrange_labels()

## 视窗只改变投影，布局与层描述不变；中心和放大倍率都是有限的显示值。
func set_camera(center: Vector2, zoom: float) -> bool:
	if not center.is_finite() or not is_finite(zoom) or zoom < 1.0 or zoom > 3.0 or _descriptor.is_empty(): return false
	_camera = {"center": center, "zoom": zoom}; _arrange_labels(); return true

## 总览恢复完整画布，不改变选中场所、候选或撤销历史。
func reset_camera() -> void:
	_camera = {}; _arrange_labels()

## 保存视窗意图用于单楼返回；实际中心会依当前窗口尺寸约束在画布内。
func camera_state() -> Dictionary:
	return _camera.duplicate(true)

## 恢复已知视窗只接受本控件保存的有限参数，坏记录不破坏当前画面。
func restore_camera(state: Dictionary) -> bool:
	if state.is_empty(): reset_camera(); return true
	if not state.get("center") is Vector2 or not (typeof(state.get("zoom")) in [TYPE_INT, TYPE_FLOAT]): return false
	return set_camera(state.center, state.zoom)

## 全景与局部共用等比变换，局部中心夹在可绘地面内，不挪动建筑或另造近景布局。
func canvas_transform() -> Transform2D:
	if _descriptor.is_empty(): return Transform2D.IDENTITY
	var extent: Vector2 = Vector2(_descriptor.canvas[0], _descriptor.canvas[1])
	var factor: float = minf(size.x / extent.x, size.y / extent.y) * float(_camera.get("zoom", 1.0))
	if factor <= 0: return Transform2D.IDENTITY
	var center: Vector2 = _camera.get("center", extent * 0.5)
	var half: Vector2 = size / (2.0 * factor)
	for axis in range(2):
		center[axis] = clampf(center[axis], half[axis], extent[axis] - half[axis]) if half[axis] * 2 < extent[axis] else extent[axis] * 0.5
	return Transform2D(Vector2(factor, 0), Vector2(0, factor), size * 0.5 - center * factor)

## 独立输入检查和宿主截图使用与实际绘图完全相同的坐标。
func logical_to_local(point: Vector2) -> Vector2:
	return canvas_transform() * point

## 层序倒查真实alpha；透明外框与浅投影不挡点击，实心树冠挡住后方屋面，纸签仍可访问。
func instance_at(point: Vector2) -> String:
	if _descriptor.is_empty() or not Rect2(Vector2.ZERO, size).has_point(point): return ""
	var logical: Vector2 = canvas_transform().affine_inverse() * point
	for index in range(_descriptor.layers.size() - 1, -1, -1):
		var row: Dictionary = _descriptor.layers[index]
		if not row.get("visible", true): continue
		for part_index in range(row.parts.size() - 1, -1, -1):
			var part: Dictionary = row.parts[part_index]
			if not part.visible: continue
			var rect: Rect2 = _rect(part.rect)
			if not rect.has_point(logical): continue
			var source: Image = _images[part.asset_id]
			var pixel: Vector2i = Vector2i((logical - rect.position) / rect.size * Vector2(source.get_size()))
			if source.get_pixelv(pixel).a >= 0.70: return row.id if row.interactive else ""
	for plot in _descriptor.get("plots", []):
		if plot.occupant_id.is_empty() and not str(plot.get("place_id", "")).is_empty() and Geometry2D.is_point_in_polygon(logical, _polygon(plot.polygon)): return plot.place_id
	return ""

## 鼠标与触摸必须在同一对象按下并抬起；拖离或换布局不触发旧访问。
func _gui_input(event: InputEvent) -> void:
	if editing:
		_editor_input(event)
		return
	if event is InputEventMouseButton and event.button_index == MOUSE_BUTTON_LEFT:
		if event.pressed and _pointer_id == -1: _pressed = instance_at(event.position); _pointer_id = -2
		elif not event.pressed and _pointer_id == -2:
			var id: String = instance_at(event.position)
			if not _pressed.is_empty() and id == _pressed: activate_instance(id)
			_pressed = ""; _pointer_id = -1
		accept_event()
	elif event is InputEventScreenTouch:
		if event.pressed and _pointer_id == -1: _pressed = instance_at(event.position); _pointer_id = event.index
		elif not event.pressed and _pointer_id == event.index:
			var id: String = instance_at(event.position)
			if not _pressed.is_empty() and id == _pressed: activate_instance(id)
			_pressed = ""; _pointer_id = -1
		accept_event()

## 编辑模式保留足点相对鼠标的偏移，拖动结束仍只是待确认预览。
func _editor_input(event: InputEvent) -> void:
	if event is InputEventMouseButton and event.button_index == MOUSE_BUTTON_LEFT:
		if event.pressed:
			var id: String = instance_at(event.position)
			if not id.is_empty():
				select_instance(id); _dragging = id
				var row: Dictionary = find_instance(id)
				_drag_offset = Vector2(row.foot[0], row.foot[1]) - canvas_transform().affine_inverse() * event.position
		elif not _dragging.is_empty():
			var id: String = _dragging; _dragging = ""
			placement_dragged.emit(id, canvas_transform().affine_inverse() * event.position + _drag_offset, true)
		accept_event()
	elif event is InputEventMouseMotion and not _dragging.is_empty():
		placement_dragged.emit(_dragging, canvas_transform().affine_inverse() * event.position + _drag_offset, false)
		accept_event()
	elif event is InputEventScreenTouch:
		if event.pressed and _pointer_id == -1:
			var id: String = instance_at(event.position)
			if not id.is_empty():
				select_instance(id); _dragging = id; _pointer_id = event.index
				var row: Dictionary = find_instance(id)
				_drag_offset = Vector2(row.foot[0], row.foot[1]) - canvas_transform().affine_inverse() * event.position
		elif not event.pressed and _pointer_id == event.index:
			var id: String = _dragging; _dragging = ""; _pointer_id = -1
			if not id.is_empty(): placement_dragged.emit(id, canvas_transform().affine_inverse() * event.position + _drag_offset, true)
		accept_event()
	elif event is InputEventScreenDrag and event.index == _pointer_id and not _dragging.is_empty():
		placement_dragged.emit(_dragging, canvas_transform().affine_inverse() * event.position + _drag_offset, false)
		accept_event()

## 地面始终完整绘制，建筑与树只按共同足点排序，不在源图里烘焙另一栋建筑。
func _draw() -> void:
	if _descriptor.is_empty(): return
	var transform: Transform2D = canvas_transform()
	draw_set_transform_matrix(transform)
	draw_texture_rect(_registry.texture("backgrounds", _descriptor.background_id), Rect2(Vector2.ZERO, Vector2(_descriptor.canvas[0], _descriptor.canvas[1])), false)
	_draw_street_surfaces()
	if show_plots:
		for plot in _descriptor.get("plots", []):
			var polygon: PackedVector2Array = _polygon(plot.polygon)
			var color: Color = Color("8db39d") if plot.occupant_id.is_empty() else Color("c2aa74")
			draw_colored_polygon(polygon, Color(color, 0.10))
			polygon.append(polygon[0]); draw_polyline(polygon, color, 2, true)
	for row in _descriptor.layers:
		if _preview.get("id") == row.id or not row.get("visible", true): continue
		_draw_building_parts(row)
	if show_plots:
		for plot in _descriptor.get("plots", []):
			if not plot.occupant_id.is_empty() or not str(plot.get("place_id", "")).is_empty(): continue
			var center: Vector2 = Vector2.ZERO
			for point in plot.polygon: center += Vector2(point[0], point[1])
			center /= float(plot.polygon.size())
			var text: String = str(plot.get("name", plot.id)) + " · 空地"
			var font: Font = get_theme_default_font()
			var extent: Vector2 = font.get_string_size(text, HORIZONTAL_ALIGNMENT_LEFT, -1, 24)
			draw_rect(Rect2(center - Vector2(extent.x * 0.5 + 10, 20), Vector2(extent.x + 20, 38)), Color("eee2c6dd"))
			draw_string(font, center + Vector2(-extent.x * 0.5, 7), text, HORIZONTAL_ALIGNMENT_LEFT, -1, 24, Color("365848"))
	if not _preview.is_empty():
		var row: Dictionary = find_instance(_preview.id)
		var rect: Rect2 = _rect(row.rect); rect.position += _preview.offset
		if row.get("visible", true): _draw_building_parts(row, _preview.offset, Color(1, 1, 1, 0.74))
		var footprint: PackedVector2Array = _polygon(row.footprint)
		for index in range(footprint.size()): footprint[index] += _preview.offset
		var color: Color = Color("398666") if _preview.valid else Color("b43731")
		draw_colored_polygon(footprint, Color(color, 0.15)); footprint.append(footprint[0]); draw_polyline(footprint, color, 3, true)
	if not _selected.is_empty():
		var row: Dictionary = find_instance(_selected)
		if not row.is_empty() and row.get("entry_active", true):
			var entry: Vector2 = Vector2(row.entry[0], row.entry[1])
			if _preview.get("id") == row.id: entry += _preview.offset
			draw_arc(entry, 17, 0, TAU, 36, Color("b55338"), 3, true)
			draw_circle(entry, 5, Color("f3dfaf"))
	draw_set_transform_matrix(Transform2D.IDENTITY)

## 同一建筑的主体和部件一起按父足点排序、移位和淡化，独立部件不成为新场所。
func _draw_building_parts(row: Dictionary, offset: Vector2 = Vector2.ZERO, color: Color = Color.WHITE) -> void:
	for part in row.parts:
		if not part.visible: continue
		var rect: Rect2 = _rect(part.rect); rect.position += offset
		draw_texture_rect(_registry.texture("backgrounds", part.asset_id), rect, false, color)

## 同材质道路与门前衔接先合并，只铺一遍磨损层，交叉处不产生重叠色块。
func _draw_street_surfaces() -> void:
	var groups: Dictionary = {}
	for road in _descriptor.get("roads", []):
		var points: Array = _road_points(road)
		_collect_surface(groups, road, points, float(road.width))
	var access_layers: Array = _road_preview.access if _road_preview.get("valid", false) else _descriptor.get("access", [])
	for access in access_layers:
		if access.instance_id != _preview.get("id"): _collect_surface(groups, access, access.points, 22)
	for access in _preview.get("access", []):
		if access.instance_id == _preview.id: _collect_surface(groups, access, access.points, 22)
	for group in groups.values():
		var polygons: Array = _merge_surfaces(group.polygons)
		var basis: Array = group.source.uv_basis
		var inverse: Transform2D = Transform2D(Vector2(basis[0][0], basis[0][1]), Vector2(basis[1][0], basis[1][1]), Vector2.ZERO).affine_inverse()
		# 等比例透明叠层的总不透明度固定，地面石纹保留，不再整条覆盖成砂色地毯。
		var alpha: float = 1.0 - pow(1.0 - float(group.source.opacity), 1.0 / 7.0)
		for inset in range(7):
			for outline in polygons:
				for polygon in Geometry2D.offset_polygon(outline, -float(inset), Geometry2D.JOIN_ROUND):
					var uv: PackedVector2Array = PackedVector2Array()
					for point in polygon: uv.append((inverse * point) / float(group.source.tile_size))
					draw_colored_polygon(polygon, Color(1, 1, 1, alpha), uv, _registry.texture("backgrounds", group.source.asset_id))
	if not _road_preview.is_empty():
		var previews: Array = _road_preview.get("roads", [{"points": _road_preview.get("points", [])}])
		for preview in previews:
			if preview.points.size() >= 2: draw_polyline(_polygon(preview.points), Color("4c997a") if _road_preview.valid else Color("ba463d"), 3, true)

## 纹理、透明度和地面轴共同决定材质组，不混用不同光照或不同绘画尺度。
func _collect_surface(groups: Dictionary, source: Dictionary, points: Array, width: float) -> void:
	var key: String = str([source.asset_id, source.tile_size, source.uv_basis, source.opacity])
	if not groups.has(key): groups[key] = {"source": source, "polygons": []}
	for polygon in Geometry2D.offset_polyline(_polygon(points), width * 0.5, Geometry2D.JOIN_ROUND, Geometry2D.END_ROUND): groups[key].polygons.append(polygon)

## 当前开放街段的连续多边形合并后再羽化；不相交的道路仍保持独立轮廓。
func _merge_surfaces(source: Array) -> Array:
	var merged: Array = []
	for polygon in source:
		var candidate: PackedVector2Array = polygon
		var index: int = 0
		while index < merged.size():
			var result: Array = Geometry2D.merge_polygons(candidate, merged[index])
			if result.size() == 1:
				candidate = result[0]; merged.remove_at(index); index = 0
			else: index += 1
		merged.append(candidate)
	return merged

## 图层几何转换不另作坐标缩放，编辑预览与命中共用画布变换。
func _polygon(points: Array) -> PackedVector2Array:
	var result: PackedVector2Array = PackedVector2Array()
	for point in points: result.append(Vector2(point[0], point[1]))
	return result

## 标签是同对象的等价访问入口；创建代次防止旧按钮访问新布局中的同名节点。
func _rebuild_labels() -> void:
	for label in _labels:
		remove_child(label); label.queue_free()
	_labels.clear()
	for row in _descriptor.layers:
		if not row.interactive: continue
		var button: Button = Button.new()
		button.text = row.name + (" · " + str(row.get("state_label", row.state)) if row.state != "complete" else "")
		button.name = "Place_" + row.id.replace(".", "_")
		button.set_meta("instance_id", row.id)
		button.tooltip_text = "查看" + row.name + (" · " + str(row.state_note) if not str(row.get("state_note", "")).is_empty() else "")
		button.custom_minimum_size = Vector2(100, 44)
		button.pressed.connect(_label_pressed.bind(row.id, _generation))
		button.gui_input.connect(_label_input.bind(button, _generation))
		add_child(button); _labels.append(button)
	_update_label_styles(); _arrange_labels()

## 每次重排重新计算标签屏幕位置，字牌保留44像素命中区，避免窄窗缩成不可点文字。
func _arrange_labels() -> void:
	if _descriptor.is_empty(): return
	var occupied: Array[Rect2] = []
	for button in _labels:
		var row: Dictionary = find_instance(str(button.get_meta("instance_id")))
		var point: Vector2 = logical_to_local(Vector2(row.label[0], row.label[1]))
		if _preview.get("id") == row.id: point = logical_to_local(Vector2(row.label[0], row.label[1]) + _preview.offset)
		if not _camera.is_empty() and not Rect2(Vector2.ZERO, size).has_point(point):
			button.visible = false; continue
		var extent: Vector2 = button.get_combined_minimum_size()
		var rect: Rect2 = Rect2(point - extent * 0.5, extent)
		rect.position.x = clampf(rect.position.x, 4, maxf(4, size.x - extent.x - 4))
		rect.position.y = clampf(rect.position.y, 4, maxf(4, size.y - extent.y - 4))
		for previous in occupied:
			if previous.grow(3).intersects(rect): rect.position.y = minf(previous.end.y + 5, size.y - extent.y - 4)
		occupied.append(rect); button.position = rect.position; button.size = rect.size; button.visible = show_labels
	queue_redraw()

## 选中、悬停和键盘焦点以颜色加边框表达，不改变场所名单和身份。
func _update_label_styles() -> void:
	for button in _labels:
		var selected: bool = button.get_meta("instance_id") == _selected
		for state in ["normal", "hover", "pressed", "focus"]:
			var style: StyleBoxFlat = StyleBoxFlat.new()
			style.bg_color = Color("214e55") if selected or state == "hover" else Color("efe3c7")
			style.border_color = Color("b58b47"); style.set_border_width_all(2); style.set_corner_radius_all(2)
			style.content_margin_left = 15; style.content_margin_right = 15
			button.add_theme_stylebox_override(state, style)
		button.add_theme_color_override("font_color", Color("fff1cf") if selected else Color("382f24"))
		button.add_theme_color_override("font_hover_color", Color("fff1cf"))
		button.add_theme_font_size_override("font_size", 21)

## 标签回调只接受当前创建代次；已关闭或替换的节点没有后续作用。
func _label_pressed(id: String, generation: int) -> void:
	if generation == _generation and is_inside_tree(): activate_instance(id)

## 编辑时纸签也可拖动同一建筑；按钮局部坐标换回画布，避免标签挡住屋面拖放入口。
func _label_input(event: InputEvent, button: Button, generation: int) -> void:
	if not editing or generation != _generation or not is_instance_valid(button): return
	if event is InputEventMouseButton or event is InputEventMouseMotion or event is InputEventScreenTouch or event is InputEventScreenDrag:
		var translated: InputEvent = event.duplicate()
		translated.set("position", event.get("position") + button.position)
		_editor_input(translated)

## 描述矩形只由已验证解析结果调用，不另造一套几何规则。
func _rect(value: Array) -> Rect2:
	return Rect2(value[0], value[1], value[2], value[3])
