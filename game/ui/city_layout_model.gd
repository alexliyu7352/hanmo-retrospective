## 独立城市表现数据：只解析布局与图像几何，不读取世界、存档或建设权限。
extends RefCounted
const Street = preload("res://ui/city_street_geometry.gd")
const STATE_LABELS = {"complete": "完工", "empty": "空地", "foundation": "基础", "construction": "施工", "damaged": "破损", "repair": "修缮"}

## 将一个显式布局解析为稳定绘制列表；坏引用、重名或越界时不返回半份画面。
static func resolve(document: Dictionary, layout: Dictionary, registry: RefCounted) -> Dictionary:
	if document.get("format_version") != 1 or not _point(document.get("canvas")) or not document.get("archetypes") is Dictionary:
		return _fail("InvalidCityDocument")
	if registry == null or not registry.has_method("texture") or not layout.get("instances") is Array:
		return _fail("InvalidCityLayout")
	if not layout.get("id") is String or not layout.get("name") is String or layout.instances.size() > 64:
		return _fail("InvalidCityLayout")
	var canvas: Vector2 = _vector(document.canvas)
	if canvas.x < 1 or canvas.y < 1 or canvas.x > 4096 or canvas.y > 4096:
		return _fail("InvalidCityCanvas")
	var background: Texture2D = registry.texture("backgrounds", str(document.get("background_id", "")))
	if background == null or str(document.get("background_id", "")).is_empty():
		return _fail("MissingCityBackground")
	var rows: Array = []
	var seen: Dictionary = {}
	for instance in layout.instances:
		if not instance is Dictionary or not instance.get("id") is String or instance.id.is_empty() or seen.has(instance.id):
			return _fail("DuplicateCityInstance")
		seen[instance.id] = true
		if not instance.get("name") is String or not instance.get("interactive") is bool or not _point(instance.get("position")):
			return _fail("InvalidCityInstance")
		if not _number(instance.get("width")) or instance.width < 40 or instance.width > 1400:
			return _fail("InvalidCityScale")
		var position: Vector2 = _vector(instance.position)
		if not Rect2(Vector2.ZERO, canvas).has_point(position): return _fail("CityAnchorOutsideCanvas")
		var archetype: Dictionary = document.archetypes.get(instance.get("archetype", ""), {})
		if archetype.is_empty(): return _fail("UnknownCityArchetype")
		for field in ["support", "entry", "label"]:
			if not _point(archetype.get(field)): return _fail("InvalidCityGeometry")
			var point: Vector2 = _vector(archetype[field])
			if point.x < 0 or point.y < 0 or point.x > 1 or point.y > 1: return _fail("InvalidCityGeometry")
		var state: String = str(instance.get("state", ""))
		var asset_id: String = str(archetype.get("asset_id", ""))
		var empty: bool = state == "empty" and archetype.get("supports_empty", false)
		var primary: Texture2D = registry.texture("backgrounds", asset_id) if not asset_id.is_empty() else null
		if state != "complete" and not empty:
			if not archetype.get("states", {}).has(state): return _fail("UnsupportedCityVisualState")
			asset_id = str(archetype.states[state])
		if asset_id.is_empty(): return _fail("MissingCityTexture")
		var texture: Texture2D = registry.texture("backgrounds", asset_id)
		if texture == null: return _fail("MissingCityTexture")
		if primary == null or texture.get_size() != primary.get_size(): return _fail("CityStateCanvasMismatch")
		var extent: Vector2 = texture.get_size() * float(instance.width) / texture.get_width()
		var rect: Rect2 = Rect2(position - extent * _vector(archetype.support), extent)
		if not rect.intersects(Rect2(Vector2.ZERO, canvas)): return _fail("CityImageOutsideCanvas")
		var footprint: Array = []
		if instance.interactive:
			var definition: Variant = archetype.get("footprint")
			if not definition is Array or definition.size() != 4: return _fail("MissingCityFootprint")
			for point in definition:
				if not _point(point) or point[0] < 0 or point[1] < 0 or point[0] > 1 or point[1] > 1:
					return _fail("InvalidCityFootprint")
				footprint.append(_array(rect.position + extent * _vector(point)))
			if not _convex(footprint): return _fail("InvalidCityFootprint")
			for previous in rows:
				if empty or not previous.interactive or not previous.get("occupies_ground", true): continue
				if not Geometry2D.intersect_polygons(_polygon(footprint), _polygon(previous.footprint)).is_empty():
					return {"ok": false, "code": "CityBuildingFootprintOverlap", "instances": [previous.id, instance.id]}
		var label_position: Array = archetype.get("state_label_positions", {}).get(state, archetype.label)
		if not _point(label_position): return _fail("InvalidCityGeometry")
		var parts: Dictionary = _display_parts(archetype, instance, state, asset_id, rect, position, registry)
		if not parts.ok: return parts
		rows.append({"id": instance.id, "name": instance.name, "archetype": instance.archetype, "state": state, "asset_id": parts.parts[0].asset_id, "parts": parts.parts, "attachment_options": parts.options, "plot_id": instance.get("plot_id", ""),
			"interactive": instance.interactive, "rect": [rect.position.x, rect.position.y, rect.size.x, rect.size.y],
			"foot": instance.position.duplicate(), "entry": _array(rect.position + extent * _vector(archetype.entry)),
			"label": _array(rect.position + extent * _vector(label_position)), "footprint": footprint, "visible": not empty, "occupies_ground": not empty,
			"state_label": STATE_LABELS.get(state, state), "state_note": archetype.get("state_notes", {}).get(state, ""), "entry_active": not empty})
	rows.sort_custom(func(a: Dictionary, b: Dictionary) -> bool:
		return a.foot[1] < b.foot[1] if a.foot[1] != b.foot[1] else a.id < b.id)
	var street: Dictionary = Street.resolve(document, layout, registry, rows)
	if not street.ok: return street
	return {"ok": true, "layout_id": layout.id, "name": layout.name, "canvas": document.canvas.duplicate(),
		"background_id": document.background_id, "layers": rows, "roads": street.roads, "plots": street.plots, "access": street.access}

## 从唯一资源清单提取独立样板真正引用的资源；不引入第二张路径表或私有白名单。
static func required_backgrounds(document: Dictionary) -> Array:
	var ids: Array = [document.get("background_id", "")]
	for archetype in document.get("archetypes", {}).values():
		ids.append(archetype.get("asset_id", ""))
		ids.append_array(archetype.get("states", {}).values())
		for display in archetype.get("display_states", {}).values(): ids.append(display.get("body_asset_id", ""))
		for attachment in archetype.get("attachments", {}).values(): ids.append(attachment.get("asset_id", ""))
	for style in document.get("road_styles", {}).values(): ids.append(style.get("asset_id", ""))
	var unique: Array = []
	for id in ids:
		if id is String and not id.is_empty() and not id in unique: unique.append(id)
	return unique

## 建筑部件采用作者固定挂接点和统一等比尺寸；未适配状态只隐藏，不以完整部件修复坏楼。
static func _display_parts(archetype: Dictionary, instance: Dictionary, state: String, asset_id: String, rect: Rect2, foot: Vector2, registry: RefCounted) -> Dictionary:
	var display: Variant = archetype.get("display_states", {}).get(state, {})
	var catalog: Variant = archetype.get("attachments", {})
	var choices: Variant = instance.get("attachments", {})
	if not display is Dictionary or not catalog is Dictionary or not choices is Dictionary: return _fail("InvalidCityAttachments")
	var selected: Variant = display.get("attachments", [])
	if not selected is Array: return _fail("InvalidCityAttachments")
	for id in selected:
		if not id is String or not catalog.has(id): return _fail("UnknownCityAttachment")
	for id in choices:
		if not catalog.has(id) or not choices[id] is bool: return _fail("UnknownCityAttachment")
	var body: String = str(display.get("body_asset_id", asset_id))
	var texture: Texture2D = registry.texture("backgrounds", body)
	if texture == null: return _fail("MissingCityTexture")
	if texture.get_size() != registry.texture("backgrounds", asset_id).get_size(): return _fail("CityStateCanvasMismatch")
	var factor: Variant = display.get("body_scale", 1.0)
	if not _number(factor) or factor < 0.75 or factor > 1.25: return _fail("InvalidCityPartScale")
	var body_rect: Rect2 = Rect2(foot + (rect.position - foot) * factor, rect.size * factor)
	var parts: Array = [{"id": "body", "asset_id": body, "rect": [body_rect.position.x, body_rect.position.y, body_rect.size.x, body_rect.size.y], "visible": state != "empty"}]
	var options: Array = []
	for id in catalog:
		var definition: Variant = catalog[id]
		if not definition is Dictionary or not _point(definition.get("origin")) or not _number(definition.get("width")) or definition.width <= 0 or definition.width > 1: return _fail("InvalidCityAttachments")
		var origin: Vector2 = _vector(definition.origin)
		if origin.x < 0 or origin.y < 0 or origin.x > 1 or origin.y > 1: return _fail("InvalidCityAttachments")
		if not definition.get("default_visible", false) is bool: return _fail("InvalidCityAttachments")
		var requested: bool = choices.get(id, definition.get("default_visible", false))
		var supported: bool = id in selected and state != "empty"
		options.append({"id": id, "name": str(definition.get("name", id)), "supported": supported, "requested": requested, "visible": requested and supported})
		if not supported: continue
		var part_asset: String = str(definition.get("asset_id", "")); var part_texture: Texture2D = registry.texture("backgrounds", part_asset)
		if part_texture == null: return _fail("MissingCityTexture")
		var width: float = rect.size.x * float(definition.width)
		var size: Vector2 = part_texture.get_size() * width / part_texture.get_width()
		var position: Vector2 = rect.position + rect.size * origin
		parts.append({"id": id, "asset_id": part_asset, "rect": [position.x, position.y, size.x, size.y], "visible": requested})
	return {"ok": true, "parts": parts, "options": options}

## 浮点仅用于像素几何，拒绝非有限数；此模块不接触钱粮与资产数量。
static func _number(value: Variant) -> bool:
	return typeof(value) in [TYPE_INT, TYPE_FLOAT] and is_finite(float(value))

## 几何只接受两个有限数，避免布尔值或任意对象进入图形接口。
static func _point(value: Variant) -> bool:
	return value is Array and value.size() == 2 and _number(value[0]) and _number(value[1])

## JSON坐标转换只在验证后执行，不替缺失字段猜测原点。
static func _vector(value: Array) -> Vector2:
	return Vector2(value[0], value[1])

## 对外描述保持可序列化数组，不携带Godot资源或世界引用。
static func _array(value: Vector2) -> Array:
	return [value.x, value.y]

## 占地只约束显示素材的地面接触面，屋顶可以遮挡；不计算产权、通路或建设资格。
static func _polygon(points: Array) -> PackedVector2Array:
	var result: PackedVector2Array = PackedVector2Array()
	for point in points: result.append(_vector(point))
	return result

## 四边形必须保持同向凸轮廓，拒绝自交或退化声明，防止用坏占地掩盖嵌楼。
static func _convex(points: Array) -> bool:
	var direction: float = 0.0
	for index in range(4):
		var a: Vector2 = _vector(points[index])
		var b: Vector2 = _vector(points[(index + 1) % 4])
		var c: Vector2 = _vector(points[(index + 2) % 4])
		var turn: float = (b - a).cross(c - b)
		if absf(turn) < 1: return false
		if direction != 0.0 and signf(turn) != signf(direction): return false
		direction = turn
	return true

## 错误仅返回稳定原因，由宿主保留上一份合法布局。
static func _fail(code: String) -> Dictionary:
	return {"ok": false, "code": code}
