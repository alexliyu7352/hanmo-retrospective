## 正式三族有限合成叶；算法沿character_studio/src/portrait_renderer.gd，根适配不改独立工坊。
## 仅支持目录冻结的束髻、无须、暖肤与素衣，1024基底合成一次后派生整对头像/半身。
extends RefCounted
const Composition = preload("res://ui/adult_appearance_composition.gd")
const SIZE = Vector2i(1024, 1536)
const FEATURES = ["brows", "eyes", "nose", "mouth"]
const CACHE_LIMIT = 12
static var _pairs: Dictionary = {}
static var _order: Array[String] = []

## 全部资源、状态和裁框先验证，合成完成才返回整对；任一失败不发布单边结果。
static func render_pair(appearance_view: Variant, manifest: Dictionary = {}, resource_base: String = "res://") -> Dictionary:
	var resolved: Dictionary = Composition.resolve(appearance_view, manifest, resource_base)
	if not resolved.ok: return resolved
	var definition: Dictionary = resolved.definition
	var selected: Dictionary = resolved.selection
	if not definition.get("face_profiles") is Array or selected.face >= definition.face_profiles.size(): return _fail("MissingAdultCompositionFaceProfile")
	var profile: Variant = definition.face_profiles[selected.face]
	if not profile is Dictionary or not profile.get("feature_sources") is Dictionary or not profile.get("regions") is Dictionary or not definition.get("assets") is Dictionary: return _fail("InvalidAdultCompositionFaceProfile")
	var ids: Array[String] = ["head-" + str(selected.face), "hair-" + str(selected.face), "outfit-" + str(selected.outfit)]
	for feature in FEATURES:
		var sources: Variant = profile.feature_sources.get(feature)
		var semantics: Variant = definition.choice_semantics.get(feature)
		if not sources is Array or not semantics is Array or sources.size() != semantics.size() or selected[feature] >= sources.size(): return _fail("MissingAdultCompositionFeature")
		var id: Variant = sources[selected[feature]]
		if not id is String or id.is_empty(): return _fail("MissingAdultCompositionFeature")
		if not id in ids: ids.append(id)
		if not _regions_valid(profile.regions.get(feature)): return _fail("InvalidAdultCompositionRegions")
	var framing: Variant = definition.get("portrait_crop", [0, 0, SIZE.x, SIZE.y])
	var avatar_crop: Variant = definition.get("avatar_crop")
	if not _crop_valid(framing, SIZE) or framing[2] * 768 != framing[3] * 512: return _fail("InvalidAdultCompositionCrop")
	if not _crop_valid(avatar_crop, Vector2i(512, 768)) or avatar_crop[2] != avatar_crop[3]: return _fail("InvalidAdultCompositionCrop")
	var sources: Dictionary = {}
	var signatures: Array = []
	for id in ids:
		var path: Variant = definition.assets.get(id)
		if not path is String or not path.begins_with("res://assets/runtime/") or ".." in path or "\\" in path: return _fail("InvalidAdultCompositionAssetPath")
		path = str(resolved.registry.resource_root) + path.trim_prefix("res://")
		if not Composition.safe_resource_path(path): return _fail("InvalidAdultCompositionAssetPath")
		var snapshot: Dictionary = _snapshot(path)
		if not snapshot.ok: return snapshot
		sources[id] = snapshot
		signatures.append([path, snapshot.digest])
	var key: String = JSON.stringify([resolved.record, definition, resolved.registry, signatures], "", true, true).sha256_text()
	if _pairs.has(key): return _copy_pair(_pairs[key])
	var images: Dictionary = {}
	for id in sources:
		var decoded: Dictionary = _image(sources[id])
		if not decoded.ok: return decoded
		images[id] = decoded.image
	var head: Image = images[ids[0]].duplicate()
	for feature in FEATURES:
		_apply_feature(head, images[profile.feature_sources[feature][selected[feature]]], profile.regions[feature])
	var portrait: Image = Image.create(SIZE.x, SIZE.y, false, Image.FORMAT_RGBA8)
	portrait.blend_rect(images[ids[2]], Rect2i(Vector2i.ZERO, SIZE), Vector2i.ZERO)
	portrait.blend_rect(head, Rect2i(Vector2i.ZERO, SIZE), Vector2i.ZERO)
	portrait.blend_rect(images[ids[1]], Rect2i(Vector2i.ZERO, SIZE), Vector2i.ZERO)
	portrait = portrait.get_region(Rect2i(int(framing[0]), int(framing[1]), int(framing[2]), int(framing[3])))
	portrait.resize(512, 768, Image.INTERPOLATE_LANCZOS)
	var avatar: Image = portrait.get_region(Rect2i(int(avatar_crop[0]), int(avatar_crop[1]), int(avatar_crop[2]), int(avatar_crop[3])))
	var pair: Dictionary = {"ok": true, "half_body": portrait, "avatar": avatar, "avatar_crop": avatar_crop.duplicate(), "record_id": resolved.record.id, "fitting_id": resolved.fitting_id}
	_pairs[key] = pair
	_order.append(key)
	if _order.size() > CACHE_LIMIT: _pairs.erase(_order.pop_front())
	return _copy_pair(pair)

## 固定作者椭圆内渐变替换五官，保留原头部alpha与外围肤质；与工坊公式逐项一致。
static func _apply_feature(target: Image, edited: Image, regions: Array) -> void:
	for values in regions:
		var center: Vector2 = Vector2(values[0], values[1]) * 2.0
		var radius: Vector2 = Vector2(values[2], values[3]) * 2.0
		for y in range(maxi(0, floori(center.y - radius.y)), mini(SIZE.y, ceili(center.y + radius.y + 1))):
			for x in range(maxi(0, floori(center.x - radius.x)), mini(SIZE.x, ceili(center.x + radius.x + 1))):
				var distance: float = ((Vector2(x, y) - center) / radius).length()
				var weight: float = 1.0 - smoothstep(0.72, 1.0, distance)
				if weight <= 0.0: continue
				var old: Color = target.get_pixel(x, y)
				var color: Color = old.lerp(edited.get_pixel(x, y), weight)
				color.a = old.a
				target.set_pixel(x, y, color)

## 每次包括缓存命中前读取真实源字节摘要；同秒截断和替换不能借mtime或ResourceLoader旧缓存通过。
static func _snapshot(path: String) -> Dictionary:
	var context: HashingContext = HashingContext.new()
	context.start(HashingContext.HASH_SHA256)
	if FileAccess.file_exists(path):
		var file: FileAccess = FileAccess.open(path, FileAccess.READ)
		if file == null or file.get_length() == 0 or file.get_length() > 33554432: return _fail("InvalidAdultCompositionAsset")
		var bytes: PackedByteArray = file.get_buffer(file.get_length())
		context.update(bytes)
		return {"ok": true, "bytes": bytes, "digest": context.finish().hex_encode()}
	if not ResourceLoader.exists(path): return _fail("MissingAdultCompositionAsset")
	var texture: Resource = ResourceLoader.load(path, "", ResourceLoader.CACHE_MODE_IGNORE)
	if not texture is Texture2D: return _fail("MissingAdultCompositionAsset")
	var image: Image = texture.get_image()
	if image == null or image.is_empty() or image.get_size() != SIZE: return _fail("InvalidAdultCompositionAsset")
	if image.is_compressed() and image.decompress() != OK: return _fail("AdultCompositionDecodeFailed")
	image.convert(Image.FORMAT_RGBA8)
	context.update(image.get_data())
	return {"ok": true, "image": image, "digest": context.finish().hex_encode()}

## 直接解码已取样的当前PNG字节；导出无源文件时使用强制重新读取的真实纹理，尺寸失败拒绝整对。
static func _image(snapshot: Dictionary) -> Dictionary:
	var image: Image
	if snapshot.has("bytes"):
		image = Image.new()
		if image.load_png_from_buffer(snapshot.bytes) != OK: return _fail("AdultCompositionDecodeFailed")
	else:
		image = snapshot.image
	if image == null or image.is_empty() or image.get_size() != SIZE: return _fail("InvalidAdultCompositionAsset")
	image = image.duplicate()
	if image.is_compressed() and image.decompress() != OK: return _fail("AdultCompositionDecodeFailed")
	image.convert(Image.FORMAT_RGBA8)
	return {"ok": true, "image": image}

## 所有五官定位必须在原作者512坐标内；异常数据不能令像素访问越界。
static func _regions_valid(value: Variant) -> bool:
	if not value is Array or value.is_empty(): return false
	for region in value:
		if not region is Array or region.size() != 4: return false
		for number in region:
			if not typeof(number) in [TYPE_INT, TYPE_FLOAT] or not is_finite(float(number)): return false
		if region[2] <= 0 or region[3] <= 0 or region[0] - region[2] < 0 or region[1] - region[3] < 0 or region[0] + region[2] > 512 or region[1] + region[3] > 768: return false
	return true

## 裁框只接受画布内正整数尺寸，输出始终保留源宽高比。
static func _crop_valid(value: Variant, size: Vector2i) -> bool:
	if not value is Array or value.size() != 4: return false
	for number in value:
		if typeof(number) != TYPE_INT: return false
	return value[0] >= 0 and value[1] >= 0 and value[2] > 0 and value[3] > 0 and value[0] + value[2] <= size.x and value[1] + value[3] <= size.y

## 缓存像素仅以独立副本交付，单个控件修改图片不会污染另一视图。
static func _copy_pair(pair: Dictionary) -> Dictionary:
	var result: Dictionary = pair.duplicate(true)
	result.half_body = pair.half_body.duplicate()
	result.avatar = pair.avatar.duplicate()
	return result

## 缺件或异常不返回任何一边图像，宿主可保留整对旧预览。
static func _fail(code: String) -> Dictionary:
	return {"ok": false, "code": code}
