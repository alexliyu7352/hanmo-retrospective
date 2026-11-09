@tool
## 只绘制CharacterRenderDescriptor的透明分层控件；CPU合成可在无窗口测试且不写世界状态。
extends Control
const AdultComposition = preload("res://ui/adult_appearance_composition.gd")
const AdultRenderer = preload("res://ui/adult_appearance_renderer.gd")
var _texture: Texture2D
var _descriptor: Dictionary = {}

## 人物图层始终是只读显示，即使首次配置失败也不能拦截宿主按钮或地图输入。
func _init() -> void:
	mouse_filter = Control.MOUSE_FILTER_IGNORE

## 候选全部合成成功后才替换当前画面；清单变更或缺纹理保留上一帧并返回诊断。
func configure(view_model: Dictionary, registry: RefCounted) -> Dictionary:
	if not view_model.get("ok", false) or not view_model.get("layers") is Array or not view_model.get("resolution") is Array: return _fail("InvalidRenderDescriptor")
	if registry == null or not registry.has_method("describe") or not registry.has_method("texture"): return _fail("InvalidPresentationRegistry")
	var manifest: Dictionary = registry.describe()
	if JSON.stringify(manifest, "", true, true).sha256_text() != view_model.get("registry_digest"): return _fail("StaleRenderDescriptor")
	var resolution: Array = view_model.resolution
	if resolution.size() != 2 or typeof(resolution[0]) != TYPE_INT or typeof(resolution[1]) != TYPE_INT or resolution[0] < 1 or resolution[1] < 1 or resolution[0] > 1024 or resolution[1] > 1024: return _fail("InvalidPortraitResolution")
	var resource_base: String = registry.resource_root() if registry.has_method("resource_root") else "res://"
	var checked: Dictionary = _validate_descriptor(view_model, manifest, resource_base)
	if not checked.ok: return checked
	if view_model == _descriptor and _texture != null and not view_model.has("adult_composition"): return {"ok": true}
	var result: Dictionary = _compose(view_model, registry)
	if not result.ok: return result
	_texture = ImageTexture.create_from_image(result.image)
	_descriptor = view_model.duplicate(true)
	queue_redraw()
	return {"ok": true}

## 拒绝篡改或损坏描述再解包几何；只接受当前唯一清单登记的组件/专绘投影。
func _validate_descriptor(descriptor: Dictionary, manifest: Dictionary, resource_base: String = "res://") -> Dictionary:
	if descriptor.has("adult_composition"):
		return _validate_adult_composition(descriptor, manifest, resource_base)
	var canvas: Variant = descriptor.get("logical_canvas")
	var crop: Variant = descriptor.get("crop")
	if not canvas is Array or canvas.size() != 2: return _fail("InvalidRenderCanvas")
	for dimension in canvas:
		if typeof(dimension) != TYPE_INT or dimension < 1 or dimension > 8192: return _fail("InvalidRenderCanvas")
	if not crop is Array or crop.size() != 4: return _fail("InvalidRenderCrop")
	for value in crop:
		if not typeof(value) in [TYPE_INT, TYPE_FLOAT] or not is_finite(float(value)): return _fail("InvalidRenderCrop")
	if crop[0] < 0 or crop[1] < 0 or crop[2] <= 0 or crop[3] <= 0: return _fail("InvalidRenderCrop")
	# Vector2交集使用单精度，允许边界浮点舍入；明显越界仍然拒绝。
	for axis in [0, 1]:
		var end: float = crop[axis] + crop[axis + 2]
		if end > canvas[axis] and not is_equal_approx(end, float(canvas[axis])): return _fail("InvalidRenderCrop")
	if descriptor.layers.size() > 64: return _fail("InvalidRenderLayer")
	var seen: Array = []
	for layer in descriptor.layers:
		if not layer is Dictionary or not layer.get("id") is String or layer.id in seen: return _fail("InvalidRenderLayer")
		seen.append(layer.id)
		if layer.get("slot") == "legacy":
			if layer.size() != 3 or layer.id != "legacy" or not layer.get("asset_id") is String or not layer.asset_id.begins_with("portraits/") or not manifest.portraits.has(layer.asset_id.trim_prefix("portraits/")): return _fail("InvalidRenderLayer")
			continue
		var expected: Dictionary = {}
		if layer.get("slot") == "dedicated":
			for profile in manifest.get("character_profiles", {}).values():
				for item in profile.overrides:
					if item.id == layer.id and item.asset_id == layer.get("asset_id") and item.geometry == layer.get("geometry"):
						expected = {"id": item.id, "slot": "dedicated", "asset_id": item.asset_id, "anchor": item.geometry.support_point, "z_index": 0, "after": [], "geometry": item.geometry}
		else:
			var component: Dictionary = manifest.get("components", {}).get(layer.id, {})
			if not component.is_empty():
				expected = {"id": component.id, "slot": component.slot, "asset_id": component.asset_id, "anchor": component.anchor, "z_index": component.z_index, "after": component.after, "geometry": component.geometry}
		if expected.is_empty() or layer != expected or layer.geometry.logical_canvas != canvas: return _fail("InvalidRenderLayer")
		for dependency in layer.after + layer.geometry.mask_ids:
			if not dependency in seen or dependency == layer.id: return _fail("InvalidRenderOrder")
	return {"ok": true}

## 新组合描述须重新匹配完整九槽、已登记资源与族裁框；不能注入路径或任意裁切。
func _validate_adult_composition(descriptor: Dictionary, manifest: Dictionary, resource_base: String) -> Dictionary:
	if not descriptor.get("view_kind") in ["avatar", "half_body"] or descriptor.layers != [] or descriptor.get("logical_canvas") != [512, 768]: return _fail("InvalidRenderDescriptor")
	var selected: Dictionary = AdultComposition.resolve(descriptor.adult_composition, manifest, resource_base)
	if not selected.ok: return selected
	var crop: Array = selected.definition.get("avatar_crop", []) if descriptor.view_kind == "avatar" else [0, 0, 512, 768]
	if descriptor.get("crop") != crop or descriptor.get("body_basis") != selected.fitting_id or descriptor.get("selected_binding_id") != ["adult_composition:" + selected.record.id]: return _fail("InvalidRenderDescriptor")
	return {"ok": true}

## 返回独立描述供测试或宿主检查；不会暴露可变纹理或身份来源。
func describe() -> Dictionary:
	return _descriptor.duplicate(true)

## 返回独立像素供CPU验收；修改副本不会修改控件纹理。
func image_copy() -> Image:
	return _texture.get_image() if _texture != null else null

## 保持透明画布宽高比居中绘制；控件缩放不参与人物年龄或游戏权限。
func _draw() -> void:
	if _texture == null: return
	var scale: float = minf(size.x / _texture.get_width(), size.y / _texture.get_height())
	var extent: Vector2 = _texture.get_size() * scale
	draw_texture_rect(_texture, Rect2((size - extent) / 2.0, extent), false)

## 按统一逻辑画布、裁切与锚点逐层透明合成；遮罩只乘目标alpha而不单独画出。
func _compose(descriptor: Dictionary, registry: RefCounted) -> Dictionary:
	if descriptor.has("adult_composition"):
		var resource_base: String = registry.resource_root() if registry.has_method("resource_root") else "res://"
		return _compose_adult(descriptor, registry.describe(), resource_base)
	var resolution: Array = descriptor.resolution
	var output: Image = Image.create(resolution[0], resolution[1], false, Image.FORMAT_RGBA8)
	var prepared: Dictionary = {}
	for layer in descriptor.layers:
		if not layer is Dictionary or not layer.get("asset_id") is String: return _fail("InvalidRenderLayer")
		var parts: PackedStringArray = layer.asset_id.split("/")
		if parts.size() != 2 or not parts[0] in ["portraits", "backgrounds"] or parts[1].is_empty(): return _fail("InvalidRenderLayer")
		var texture: Texture2D = registry.texture(parts[0], parts[1])
		if texture == null: return _fail("MissingRenderTexture")
		var source: Image = texture.get_image()
		if source == null or source.is_empty(): return _fail("MissingRenderTexture")
		source = source.duplicate()
		# 导出/VRAM导入纹理的get_image可能仍压缩；先真实解码，不能把空白成功当画面。
		if source.is_compressed() and source.decompress() != OK: return _fail("RenderTextureDecodeFailed")
		if source.is_compressed() or source.is_empty(): return _fail("RenderTextureDecodeFailed")
		source.convert(Image.FORMAT_RGBA8)
		if layer.get("slot") == "legacy":
			var fit: float = minf(float(resolution[0]) / source.get_width(), float(resolution[1]) / source.get_height())
			source.resize(maxi(1, roundi(source.get_width() * fit)), maxi(1, roundi(source.get_height() * fit)), Image.INTERPOLATE_LANCZOS)
			output.blend_rect(source, Rect2i(Vector2i.ZERO, source.get_size()), Vector2i((resolution[0] - source.get_width()) / 2, (resolution[1] - source.get_height()) / 2))
			continue
		if not layer.get("geometry") is Dictionary: return _fail("InvalidRenderLayer")
		var geo: Dictionary = layer.geometry
		var crop: Array = descriptor.crop
		if crop.size() != 4 or crop[2] <= 0 or crop[3] <= 0: return _fail("InvalidRenderCrop")
		var canvas: Array = descriptor.logical_canvas
		var scale: float = minf(float(resolution[0]) / crop[2], float(resolution[1]) / crop[3])
		var offset: Vector2 = Vector2(canvas[0], canvas[1]) * (Vector2(layer.anchor[0], layer.anchor[1]) - Vector2(geo.support_point[0], geo.support_point[1])) / 100.0
		var drawn_size: Vector2i = Vector2i(maxi(1, roundi(canvas[0] * scale)), maxi(1, roundi(canvas[1] * scale)))
		if drawn_size.x > 8192 or drawn_size.y > 8192 or drawn_size.x * drawn_size.y > 16777216: return _fail("PortraitCropTooLarge")
		source.resize(drawn_size.x, drawn_size.y, Image.INTERPOLATE_NEAREST)
		var margin: Vector2 = (Vector2(resolution[0], resolution[1]) - Vector2(crop[2], crop[3]) * scale) / 2.0
		var position: Vector2 = margin + (offset - Vector2(crop[0], crop[1])) * scale
		var aligned: Image = Image.create(resolution[0], resolution[1], false, Image.FORMAT_RGBA8)
		aligned.blit_rect(source, Rect2i(Vector2i.ZERO, source.get_size()), Vector2i(roundi(position.x), roundi(position.y)))
		# 逻辑裁切外的信箱边必须透明，不能把原图剩余躯干画进头像边缘。
		var viewport: Rect2i = Rect2i(Vector2i(roundi(margin.x), roundi(margin.y)), Vector2i(roundi(crop[2] * scale), roundi(crop[3] * scale)))
		aligned.fill_rect(Rect2i(0, 0, resolution[0], viewport.position.y), Color.TRANSPARENT)
		aligned.fill_rect(Rect2i(0, viewport.end.y, resolution[0], resolution[1] - viewport.end.y), Color.TRANSPARENT)
		aligned.fill_rect(Rect2i(0, 0, viewport.position.x, resolution[1]), Color.TRANSPARENT)
		aligned.fill_rect(Rect2i(viewport.end.x, 0, resolution[0] - viewport.end.x, resolution[1]), Color.TRANSPARENT)
		for mask_id in geo.mask_ids:
			if not prepared.has(mask_id): return _fail("MissingRenderMask")
			var mask: Image = prepared[mask_id]
			for y in range(aligned.get_height()):
				for x in range(aligned.get_width()):
					var color: Color = aligned.get_pixel(x, y)
					color.a *= mask.get_pixel(x, y).a
					aligned.set_pixel(x, y, color)
		prepared[layer.id] = aligned
		if layer.slot != "mask": output.blend_rect(aligned, Rect2i(Vector2i.ZERO, aligned.get_size()), Vector2i.ZERO)
	return {"ok": true, "image": output}

## 已完成1024基底合成的整幅人物才统一缩放；头像直接使用同源族安全框，不放大96像素证据。
func _compose_adult(descriptor: Dictionary, manifest: Dictionary, resource_base: String) -> Dictionary:
	var pair: Dictionary = AdultRenderer.render_pair(descriptor.adult_composition, manifest, resource_base)
	if not pair.ok: return pair
	var source: Image = pair.avatar if descriptor.view_kind == "avatar" else pair.half_body
	var resolution: Array = descriptor.resolution
	var scale: float = minf(float(resolution[0]) / source.get_width(), float(resolution[1]) / source.get_height())
	source.resize(maxi(1, roundi(source.get_width() * scale)), maxi(1, roundi(source.get_height() * scale)), Image.INTERPOLATE_LANCZOS)
	var output: Image = Image.create(resolution[0], resolution[1], false, Image.FORMAT_RGBA8)
	output.blend_rect(source, Rect2i(Vector2i.ZERO, source.get_size()), Vector2i((resolution[0] - source.get_width()) / 2, (resolution[1] - source.get_height()) / 2))
	return {"ok": true, "image": output}

## 配置失败仅返回诊断，上一幅已接受画面与输入描述保持原样。
func _fail(code: String) -> Dictionary:
	return {"ok": false, "code": code}
