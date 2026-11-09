## 完整姿态的单图显示叶；不拆肢体、不持规则对象、没有独立动画时钟。
extends Node2D
const Selector = preload("res://ui/adult_blade_pose_selector.gd")
var _definition: Dictionary = {}
var _textures: Dictionary = {}
var _mips: Dictionary = {}
var _current: Dictionary = {}
var _sprite: Sprite2D

## 只建立一个真实完整人物精灵，图层文字仍由宿主按原身份提供。
func _init() -> void:
	_sprite = Sprite2D.new()
	_sprite.centered = false
	_sprite.texture_filter = CanvasItem.TEXTURE_FILTER_LINEAR_WITH_MIPMAPS
	var premult_material: CanvasItemMaterial = CanvasItemMaterial.new()
	premult_material.blend_mode = CanvasItemMaterial.BLEND_MODE_PREMULT_ALPHA
	_sprite.material = premult_material
	add_child(_sprite)
	_clear_frame()

## 由宿主传入已读源图与完整候选；缺任一必需姿态或尺寸不符时整库拒绝。
func configure_sources(definition: Dictionary, images: Dictionary) -> Dictionary:
	_clear_frame()
	_definition = {}; _textures = {}; _mips = {}
	if definition.get("schema") != "adult_blade_pose_sources/1" or definition.get("family") != Selector.FAMILY: return {"ok": false, "reason": "UnsupportedSourceDefinition"}
	if not _number(definition.get("reference_height")) or definition.reference_height <= 0 or definition.reference_height > 8192 or floorf(definition.reference_height) != definition.reference_height or not definition.get("poses") is Dictionary: return {"ok": false, "reason": "InvalidSourceDefinition"}
	if definition.poses.size() != Selector.POSES.size(): return {"ok": false, "reason": "UnsupportedPoseCount"}
	for key in Selector.POSES:
		if not definition.poses.get(key) is Dictionary or not images.get(key) is Image: return {"ok": false, "reason": "MissingPose:" + key}
		var part: Dictionary = definition.poses[key]
		var source: Image = images[key]
		if source.is_empty() or source.get_format() != Image.FORMAT_RGBA8: return {"ok": false, "reason": "InvalidImage:" + key}
		if source.get_width() > 4096 or source.get_height() > 4096: return {"ok": false, "reason": "OversizedImage:" + key}
		if not part.get("rgba_sha256") is String or part.rgba_sha256 != pixel_sha256(source): return {"ok": false, "reason": "PixelHashMismatch:" + key}
		if not _point(part.get("size")) or Vector2(part.size[0], part.size[1]) != Vector2(source.get_size()): return {"ok": false, "reason": "SourceSizeMismatch:" + key}
		if not _point(part.get("ground_anchor")): return {"ok": false, "reason": "MissingGroundAnchor:" + key}
		var anchor: Vector2 = Vector2(part.ground_anchor[0], part.ground_anchor[1])
		if anchor.x < 0.0 or anchor.y < 0.0 or anchor.x > source.get_width() or anchor.y > source.get_height(): return {"ok": false, "reason": "AnchorOutsideImage:" + key}
		if not _number(part.get("uniform_scale", 1.0)) or part.get("uniform_scale", 1.0) <= 0.0 or part.get("uniform_scale", 1.0) > 4.0: return {"ok": false, "reason": "InvalidUniformScale:" + key}
	_definition = definition.duplicate(true)
	for key in Selector.POSES:
		var filtered: Image = images[key].duplicate()
		filtered.premultiply_alpha()
		filtered.generate_mipmaps()
		_textures[key] = ImageTexture.create_from_image(filtered)
		var levels: Array[Image] = []
		var data: PackedByteArray = filtered.get_data()
		for level in range(filtered.get_mipmap_count() + 1):
			var width: int = maxi(1, filtered.get_width() >> level)
			var height: int = maxi(1, filtered.get_height() >> level)
			var offset: int = filtered.get_mipmap_offset(level)
			levels.append(Image.create_from_data(width, height, false, Image.FORMAT_RGBA8, data.slice(offset, offset + width * height * 4)))
		_mips[key] = levels
	return {"ok": true, "poses_count": _textures.size()}

## 按现有播放帧选完整图，投影足点由宿主给定；拒绝不会保留旧图。
func show_frame(binding: Dictionary, actor: Dictionary, frame: Dictionary, projected_foot: Vector2, logical_height: float = 112.0) -> Dictionary:
	_clear_frame()
	if _definition.is_empty() or not projected_foot.is_finite() or not is_finite(logical_height) or logical_height <= 0.0 or logical_height > 4096.0: return {"ok": false, "reason": "MissingSourcesOrInvalidProjection"}
	var selected: Dictionary = Selector.sample(binding, actor, frame)
	if not selected.ok: return selected
	var part: Dictionary = _definition.poses[selected.pose_key]
	var ratio: float = logical_height / float(_definition.reference_height) * float(part.get("uniform_scale", 1.0))
	var anchor: Vector2 = Vector2(part.ground_anchor[0], part.ground_anchor[1])
	_sprite.texture = _textures[selected.pose_key]
	_sprite.position = -anchor * ratio
	_sprite.scale = Vector2.ONE * ratio
	_sprite.modulate = Color(selected.alpha, selected.alpha, selected.alpha, selected.alpha)
	_sprite.visible = selected.alpha > 0.0
	position = projected_foot
	_current = selected.duplicate(true)
	_current["uniform_scale"] = ratio
	_current["ground_anchor"] = anchor
	return _current.duplicate(true)

## 只返回显示元数据深副本，不暴露源Image、节点或世界引用。
func describe() -> Dictionary:
	return _current.duplicate(true)

## 离线CPU采样当前完整原图；预乘双线性避免低alpha边色被当不透明像素混合。
func image_copy(canvas: Vector2i, foot_origin: Vector2) -> Dictionary:
	if _current.is_empty() or canvas.x <= 0 or canvas.y <= 0 or canvas.x > 8192 or canvas.y > 8192 or not foot_origin.is_finite(): return {"ok": false, "reason": "InvalidCapture"}
	var original_size: Vector2 = Vector2(_definition.poses[_current.pose_key].size[0], _definition.poses[_current.pose_key].size[1])
	var output: Image = Image.create(canvas.x, canvas.y, false, Image.FORMAT_RGBA8)
	output.fill(Color.TRANSPARENT)
	var ratio: float = _current.uniform_scale
	var level: int = clampi(floori(log(1.0 / ratio) / log(2.0)), 0, _mips[_current.pose_key].size() - 1) if ratio < 1.0 else 0
	var source: Image = _mips[_current.pose_key][level]
	var mip_ratio: Vector2 = Vector2(source.get_size()) / original_size
	var origin: Vector2 = foot_origin - _current.ground_anchor * ratio
	var start: Vector2i = Vector2i(origin.floor()).max(Vector2i.ZERO)
	var end: Vector2i = Vector2i((origin + original_size * ratio).ceil()).min(canvas)
	if _current.alpha > 0.0:
		for y in range(start.y, end.y):
			for x in range(start.x, end.x):
				var color: Color = _sample(source, (Vector2(x, y) + Vector2(0.5, 0.5) - origin) / ratio * mip_ratio - Vector2(0.5, 0.5))
				color.a *= _current.alpha
				output.set_pixel(x, y, color)
	return {"ok": true, "image": output, "evidence_kind": "godot_image_complete_pose", "viewport_capture": false}

## 对四个预乘缩小层像素插值再转回直alpha；越界透明，不清洗或回写原PNG。
static func _sample(source: Image, point: Vector2) -> Color:
	var left: int = floori(point.x); var top: int = floori(point.y)
	var fx: float = point.x - left; var fy: float = point.y - top
	var total: Color = Color(0, 0, 0, 0)
	for dy in 2:
		for dx in 2:
			var x: int = left + dx; var y: int = top + dy
			if x < 0 or y < 0 or x >= source.get_width() or y >= source.get_height(): continue
			var pixel: Color = source.get_pixel(x, y)
			var weight: float = (fx if dx else 1.0 - fx) * (fy if dy else 1.0 - fy)
			total += pixel * weight
	if total.a <= 0.0: return Color.TRANSPARENT
	return Color(total.r / total.a, total.g / total.a, total.b / total.a, total.a)

## 任何失败先清空可见帧，不把上一演员或上一动作残留给新绑定。
func _clear_frame() -> void:
	_current = {}
	_sprite.visible = false
	_sprite.texture = null

## 有限二维元数据不接受隐式字符串或布尔转换。
static func _point(value: Variant) -> bool:
	return value is Array and value.size() == 2 and _number(value[0]) and _number(value[1])

## 只接受有限整数/浮点，避免异常尺度传播到渲染器。
static func _number(value: Variant) -> bool:
	return (value is int or value is float) and is_finite(float(value))

## 像素摘要锁防止同尺寸假图或调用者替换Image；PNG文件摘要另由源读取器验证。
static func pixel_sha256(source: Image) -> String:
	var context: HashingContext = HashingContext.new()
	context.start(HashingContext.HASH_SHA256)
	context.update(source.get_data())
	return context.finish().hex_encode()
