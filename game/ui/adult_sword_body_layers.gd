## 成人刀剑有限分层身体消费叶；仅接调用者纹理、已验证rig和既有只读姿态，不查规则或资源路径。
extends CanvasGroup
const Rig = preload("res://ui/adult_sword_body_rig.gd")
var _atlas: Texture2D
var _arm_atlas: Texture2D
var _shoulder_atlas: Texture2D
var _sample: Dictionary = {}
var _layers: Array = []
var _ink: AtlasCanvas

## 子画布独立绘制旧15件或有限整臂12层原图；父CanvasGroup合并后统一透明度，避免退离时关节重叠加深。
class AtlasCanvas extends Node2D:
	var atlas: Texture2D
	var arm_atlas: Texture2D
	var shoulder_atlas: Texture2D
	var layers: Array = []

	## 每件按唯一rig局部矩阵与裁片绘制，不在图层上逐件重复乘退离alpha。
	func _draw() -> void:
		if atlas == null: return
		for layer in layers:
			var key: String = layer.get("texture_key", "base")
			var source: Texture2D = shoulder_atlas if key == "shoulder" else (arm_atlas if key == "arms" else atlas)
			if source == null: continue
			draw_set_transform_matrix(layer.transform)
			for patch in layer.source_patches:
				draw_texture_rect_region(source, patch, Rect2(layer.region.position + patch.position, patch.size), Color.WHITE, false, true)
		draw_set_transform_matrix(Transform2D.IDENTITY)

## 建立只属于本样本的绘制组，不添加输入、身份、世界或时钟逻辑。
func _init() -> void:
	_ink = AtlasCanvas.new()
	_ink.name = "AtlasParts"
	add_child(_ink)


## 验证调用者提供的真实纹理尺寸后复用唯一rig采样；任何失败立即清空旧身体且保留诊断。
func configure(texture: Texture2D, definition: Dictionary, pose: Dictionary) -> Dictionary:
	if texture == null:
		clear_sample()
		return {"ok": false, "errors": ["MissingAdultSwordTexture"]}
	var validated: Dictionary = Rig.validate(definition, texture.get_size())
	if not validated.get("ok", false):
		clear_sample()
		return validated
	var sampled: Dictionary = Rig.sample(definition, pose)
	if not sampled.get("ok", false):
		clear_sample()
		return sampled
	return _install_sample(texture, null, sampled)

## 中性/出手沿用两张显式纹理；仅肩覆片可追加独立肩图，其他姿态由rig拒绝，不注册通用多材质。
func configure_full_arm(base_texture: Texture2D, arm_texture: Texture2D, base_definition: Dictionary, arm_definition: Dictionary, pose: Dictionary, shoulder_texture: Texture2D = null) -> Dictionary:
	if base_texture == null or arm_texture == null:
		clear_sample()
		return {"ok": false, "errors": ["MissingAdultFullArmTexture"]}
	var validated: Dictionary = Rig.validate_full_arm(base_definition, arm_definition, base_texture.get_size(), arm_texture.get_size(), shoulder_texture.get_size() if shoulder_texture != null else Vector2.ZERO)
	if not validated.get("ok", false):
		clear_sample()
		return validated
	var sampled: Dictionary = Rig.sample_full_arm(base_definition, arm_definition, pose)
	if not sampled.get("ok", false):
		clear_sample()
		return sampled
	return _install_sample(base_texture, arm_texture, sampled, shoulder_texture)

## 安装已验证姿态并稳定排序；可选shoulder严格限于肩覆片，不扩大其他部件资源来源。
func _install_sample(texture: Texture2D, arm_texture: Texture2D, sampled: Dictionary, shoulder_texture: Texture2D = null) -> Dictionary:
	var ordered: Array = []
	for id in sampled.parts:
		var layer: Dictionary = sampled.parts[id].duplicate(true)
		layer.id = id
		var texture_key: String = layer.get("texture_key", "base")
		if not texture_key in ["base", "arms", "shoulder"] or (texture_key == "arms" and arm_texture == null) or (texture_key == "shoulder" and (str(id) != "shoulder_collar" or shoulder_texture == null)):
			clear_sample()
			return {"ok": false, "errors": ["InvalidAdultBodyTextureKey"]}
		ordered.append(layer)
	ordered.sort_custom(_before)
	_atlas = texture
	_arm_atlas = arm_texture
	_shoulder_atlas = shoulder_texture
	_sample = sampled.duplicate(true)
	_layers = ordered
	_ink.atlas = texture
	_ink.arm_atlas = arm_texture
	_ink.shoulder_atlas = shoulder_texture
	_ink.layers = ordered
	self_modulate = Color(1.0, 1.0, 1.0, sampled.alpha)
	_ink.queue_redraw()
	return {"ok": true, "parts_count": ordered.size(), "body_position": sampled.body_position, "alpha": sampled.alpha}

## 相同深度以稳定部件ID破平局；不依赖字典插入顺序改变遮挡。
static func _before(left: Dictionary, right: Dictionary) -> bool:
	return str(left.id) < str(right.id) if left.z == right.z else left.z < right.z

## 返回真实atlas组合的独立透明像素证据；此CPU合成不宣称Viewport或原生窗口验收。
func image_copy(canvas: Vector2i = Vector2i(768, 768), foot_origin: Vector2 = Vector2(300.0, 620.0), display_scale: float = 1.0) -> Dictionary:
	if _atlas == null or _sample.is_empty():
		return {"ok": false, "code": "MissingBodySample"}
	if not is_finite(display_scale) or display_scale <= 0.0:
		return {"ok": false, "code": "InvalidBodyDisplayScale"}
	var layers: Array = _layers.duplicate(true)
	var scale_transform: Transform2D = Transform2D(0.0, Vector2.ONE * display_scale, 0.0, Vector2.ZERO)
	for layer in layers: layer.transform = scale_transform * layer.transform
	return _compose_sources(_atlas.get_image(), _arm_atlas.get_image() if _arm_atlas != null else null, layers, canvas, foot_origin, float(_sample.alpha), _shoulder_atlas.get_image() if _shoulder_atlas != null else null)

## 返回副本供宿主核对已采样局部变换；节点位置和播放时钟始终由原宿主管理。
func describe() -> Dictionary:
	return _sample.duplicate(true)

## 清空失败或过期帧，不保留上一个人的身体；不改写节点脚点、身份或时钟。
func clear_sample() -> void:
	_atlas = null
	_arm_atlas = null
	_shoulder_atlas = null
	_sample.clear()
	_layers.clear()
	_ink.atlas = null
	_ink.arm_atlas = null
	_ink.shoulder_atlas = null
	_ink.layers.clear()
	self_modulate = Color.WHITE
	_ink.queue_redraw()

## CPU证据只变换真实atlas像素；与画布消费同一裁框和矩阵，不造替代人物或假Viewport回执。
static func compose_pixels(source: Image, layers: Array, canvas: Vector2i, origin: Vector2, opacity: float = 1.0) -> Dictionary:
	return _compose_sources(source, null, layers, canvas, origin, opacity)

## 两张实际源图和可选独立肩图按同一全局z顺序组合；肩图只允许shoulder_collar层取样。
static func compose_full_arm_pixels(base_source: Image, arm_source: Image, layers: Array, canvas: Vector2i, origin: Vector2, opacity: float = 1.0, shoulder_source: Image = null) -> Dictionary:
	if arm_source == null or arm_source.is_empty(): return {"ok": false, "code": "MissingAdultFullArmImage"}
	return _compose_sources(base_source, arm_source, layers, canvas, origin, opacity, shoulder_source)

## 公用采样保留base/arms旧调用并窄接肩图；不接收任意纹理字典，整体alpha只在全部图层完成后施加。
static func _compose_sources(base_source: Image, arm_source: Image, layers: Array, canvas: Vector2i, origin: Vector2, opacity: float, shoulder_source: Image = null) -> Dictionary:
	if base_source == null or base_source.is_empty() or canvas.x < 1 or canvas.y < 1 or canvas.x > 2048 or canvas.y > 2048:
		return {"ok": false, "code": "InvalidBodySampleImage"}
	if not is_finite(origin.x) or not is_finite(origin.y) or not is_finite(opacity):
		return {"ok": false, "code": "InvalidBodySampleImage"}
	var sources: Dictionary = {}
	for key in ["base", "arms", "shoulder"]:
		var image: Image = shoulder_source if key == "shoulder" else (base_source if key == "base" else arm_source)
		if image == null: continue
		if image.is_empty(): return {"ok": false, "code": "InvalidBodySampleImage"}
		image = image.duplicate()
		if image.is_compressed() and image.decompress() != OK:
			return {"ok": false, "code": "BodySampleDecodeFailed"}
		image.convert(Image.FORMAT_RGBA8)
		sources[key] = image
	var output: Image = Image.create(canvas.x, canvas.y, false, Image.FORMAT_RGBA8)
	for layer in layers:
		if not layer is Dictionary or not layer.get("region") is Rect2 or not layer.get("transform") is Transform2D:
			return {"ok": false, "code": "InvalidBodySampleLayer"}
		var texture_key: String = layer.get("texture_key", "base")
		if not sources.has(texture_key): return {"ok": false, "code": "MissingBodySampleTextureKey"}
		if texture_key == "shoulder" and layer.get("id") != "shoulder_collar": return {"ok": false, "code": "ShoulderTextureRestrictedToCollar"}
		var input: Image = sources[texture_key]
		var region: Rect2 = layer.region
		var transform: Transform2D = layer.transform
		transform.origin += origin
		if not transform.is_finite() or absf(transform.determinant()) < 0.000001 or not Rect2(Vector2.ZERO, input.get_size()).encloses(region) or not region.has_area():
			return {"ok": false, "code": "InvalidBodySampleLayer"}
		var patches: Array = layer.get("source_patches", [Rect2(Vector2.ZERO, region.size)])
		if patches.is_empty(): return {"ok": false, "code": "InvalidBodySamplePatch"}
		for patch in patches:
			if not patch is Rect2 or not patch.has_area() or not Rect2(Vector2.ZERO, region.size).encloses(patch):
				return {"ok": false, "code": "InvalidBodySamplePatch"}
		_blend_layer(output, input, region, transform, patches)
	if opacity < 1.0:
		var bytes: PackedByteArray = output.get_data()
		for index in range(3, bytes.size(), 4): bytes[index] = roundi(bytes[index] * clampf(opacity, 0.0, 1.0))
		output.set_data(canvas.x, canvas.y, false, Image.FORMAT_RGBA8, bytes)
	return {"ok": true, "image": output, "evidence_kind": "godot_image_atlas_transform", "viewport_capture": false}

## 逆映射同一部件的裁片并集，接边只合成一次；透明边缘采用预乘alpha插值，不制造裁片缝。
static func _blend_layer(output: Image, source: Image, region: Rect2, transform: Transform2D, patches: Array) -> void:
	var bounds: Rect2 = Rect2(transform * Vector2.ZERO, Vector2.ZERO)
	for corner in [Vector2(region.size.x, 0.0), region.size, Vector2(0.0, region.size.y)]:
		bounds = bounds.expand(transform * corner)
	var inverse: Transform2D = transform.affine_inverse()
	for y in range(maxi(0, floori(bounds.position.y)), mini(output.get_height(), ceili(bounds.end.y))):
		for x in range(maxi(0, floori(bounds.position.x)), mini(output.get_width(), ceili(bounds.end.x))):
			var local: Vector2 = inverse * Vector2(x + 0.5, y + 0.5)
			if local.x < 0.0 or local.y < 0.0 or local.x >= region.size.x or local.y >= region.size.y: continue
			var color: Color = _sample_bilinear(source, region, local, patches)
			if color.a <= 0.0: continue
			output.set_pixel(x, y, output.get_pixel(x, y).blend(color))

## 在当前裁框内读取四邻域并预乘插值，禁止跨格取色污染透明接缝。
static func _sample_bilinear(source: Image, region: Rect2, local: Vector2, patches: Array = []) -> Color:
	var point: Vector2 = local - Vector2(0.5, 0.5)
	var base: Vector2i = Vector2i(floori(point.x), floori(point.y))
	var fraction: Vector2 = point - Vector2(base)
	var accumulated: Color = Color(0.0, 0.0, 0.0, 0.0)
	for dy in range(2):
		for dx in range(2):
			var pos: Vector2i = base + Vector2i(dx, dy)
			if pos.x < 0 or pos.y < 0 or pos.x >= int(region.size.x) or pos.y >= int(region.size.y): continue
			if not patches.is_empty():
				var covered: bool = false
				for patch in patches:
					if patch.has_point(Vector2(pos) + Vector2(0.5, 0.5)): covered = true; break
				if not covered: continue
			var color: Color = source.get_pixel(int(region.position.x) + pos.x, int(region.position.y) + pos.y)
			var weight: float = (fraction.x if dx == 1 else 1.0 - fraction.x) * (fraction.y if dy == 1 else 1.0 - fraction.y)
			accumulated += Color(color.r * color.a, color.g * color.a, color.b * color.a, color.a) * weight
	if accumulated.a <= 0.0: return Color(0.0, 0.0, 0.0, 0.0)
	return Color(accumulated.r / accumulated.a, accumulated.g / accumulated.a, accumulated.b / accumulated.a, accumulated.a)
