## 固定成人适配族运行渲染器：同画布部件、独立五官、材料限色与有界缓存。
extends RefCounted
const Session = preload("res://src/studio_session.gd")
const SIZE := Vector2i(1024, 1536)
var catalog: Dictionary = {}
var _validator: RefCounted
var _images: Dictionary = {}
var _cache: Dictionary = {}
var _cache_order: Array[String] = []
var _materials: Dictionary = {}
const CACHE_LIMIT := 12

## 先核实整套资源；缺失任何部件时拒绝启动，避免显示半个人物。
func configure(definition: Dictionary) -> Dictionary:
	var validator := Session.new()
	var checked: Dictionary = validator.configure(definition)
	if not checked.ok:
		return checked
	if not definition.get("assets") is Dictionary:
		return {"ok": false, "reason": "人物目录没有部件清单。"}
	var required := ["cap", "cap-mask"]
	if not definition.get("feature_sources") is Dictionary:
		return {"ok": false, "reason": "人物目录缺少独立五官语意映射。"}
	for feature in ["brows", "eyes", "nose", "mouth"]:
		var mapping = definition.feature_sources.get(feature)
		if not mapping is Array or mapping.size() != definition.fields[feature].options.size():
			return {"ok": false, "reason": "五官选件与资源映射数量不符：" + feature}
		for asset in mapping:
			if not asset is String or asset.is_empty():
				return {"ok": false, "reason": "五官资源映射缺失：" + feature}
			if not asset in required:
				required.append(asset)
	var profiles_checked := _validate_face_profiles(definition, required)
	if not profiles_checked.ok:
		return profiles_checked
	if definition.fields.beard.options.size() > 1:
		required.append("features-beard")
	for index in definition.fields.face.options.size():
		required.append("head-" + str(index))
		required.append("hair-" + str(index))
	for index in definition.fields.outfit.options.size():
		required.append("outfit-" + str(index))
	for id in required:
		if not definition.assets.has(id) or not definition.assets[id] is String:
			return {"ok": false, "reason": "人物目录缺少必要部件：" + id}
	var crop = definition.get("portrait_crop", [0, 0, SIZE.x, SIZE.y])
	if not crop is Array or crop.size() != 4:
		return {"ok": false, "reason": "完整人物安全裁切定义不合法。"}
	for value in crop:
		if not typeof(value) in [TYPE_INT, TYPE_FLOAT] or not is_finite(float(value)) or float(value) != floorf(float(value)):
			return {"ok": false, "reason": "完整人物裁切必须使用有限整数像素。"}
	if crop[0] < 0 or crop[1] < 0 or crop[2] <= 0 or crop[3] <= 0 or crop[0] + crop[2] > SIZE.x or crop[1] + crop[3] > SIZE.y or crop[2] * 768 != crop[3] * 512:
		return {"ok": false, "reason": "完整人物裁切必须在画布内并保持半身比例。"}
	var images := {}
	for id in definition.assets:
		var path: String = definition.assets[id]
		if not path.begins_with("res://assets/runtime/"):
			return {"ok": false, "reason": "部件路径不属于人物工坊。"}
		var image: Image
		if ResourceLoader.exists(path):
			var texture = load(path)
			if texture is Texture2D:
				image = texture.get_image()
		if image == null:
			image = Image.load_from_file(path)
		if image == null or image.get_size() != SIZE:
			return {"ok": false, "reason": "人物部件缺失或尺寸不符：" + id}
		if image.is_compressed() and image.decompress() != OK:
			return {"ok": false, "reason": "人物部件无法解码：" + id}
		image.convert(Image.FORMAT_RGBA8)
		images[id] = image
	catalog = definition.duplicate(true)
	_validator = validator
	_images = images
	_cache.clear()
	_cache_order.clear()
	_materials.clear()
	return {"ok": true}

## 可选逐脸映射必须完整且局限于原画布；只补充候选资源清单，不改变现有状态或缓存。
func _validate_face_profiles(definition: Dictionary, required: Array) -> Dictionary:
	if not definition.has("face_profiles"):
		return {"ok": true}
	var profiles = definition.face_profiles
	if not profiles is Array or profiles.size() != definition.fields.face.options.size():
		return {"ok": false, "reason": "脸型作者配置与脸型选件数量不符。"}
	for face in profiles.size():
		var profile = profiles[face]
		if not profile is Dictionary or not profile.get("feature_sources") is Dictionary or not profile.get("regions") is Dictionary:
			return {"ok": false, "reason": "脸型作者配置缺少独立五官来源或定位：" + str(face)}
		for feature in ["brows", "eyes", "nose", "mouth"]:
			var label: String = str(face) + "/" + feature
			var mapping = profile.feature_sources.get(feature)
			if not mapping is Array or mapping.size() != definition.fields[feature].options.size():
				return {"ok": false, "reason": "脸型五官选件与资源映射数量不符：" + label}
			for asset in mapping:
				if not asset is String or asset.is_empty():
					return {"ok": false, "reason": "脸型五官资源映射缺失：" + label}
				if not asset in required:
					required.append(asset)
			var regions = profile.regions.get(feature)
			if not regions is Array or regions.is_empty():
				return {"ok": false, "reason": "脸型五官定位区域缺失：" + label}
			for region in regions:
				if not region is Array or region.size() != 4:
					return {"ok": false, "reason": "脸型五官定位必须是中心与半径四元组：" + label}
				for value in region:
					if not typeof(value) in [TYPE_INT, TYPE_FLOAT] or not is_finite(float(value)):
						return {"ok": false, "reason": "脸型五官定位必须使用有限数值：" + label}
				var center_x := float(region[0])
				var center_y := float(region[1])
				var radius_x := float(region[2])
				var radius_y := float(region[3])
				var radius := Vector2(region[2], region[3])
				if radius.x <= 0.0 or radius.y <= 0.0 or center_x - radius_x < 0.0 or center_y - radius_y < 0.0 or center_x + radius_x > SIZE.x * 0.5 or center_y + radius_y > SIZE.y * 0.5:
					return {"ok": false, "reason": "脸型五官定位必须完整位于512乘768源坐标内且半径为正：" + label}
	return {"ok": true}

## 输出独立合成像素；非法草案或缺资源不复用另一套人物冒充成功。
func render(selection: Dictionary) -> Dictionary:
	if _validator == null:
		return {"ok": false, "reason": "人物资源尚未准备好。"}
	var checked: Dictionary = _validator.validate(selection)
	if not checked.ok:
		return checked
	var key := _state_key(selection)
	if _cache.has(key):
		return {"ok": true, "image": _cache[key].duplicate(), "cached": true}
	var head: Image = _images["head-" + str(selection.face)].duplicate()
	var feature_sources: Dictionary = catalog.feature_sources
	var feature_regions: Dictionary = catalog.regions
	if catalog.has("face_profiles"):
		var profile: Dictionary = catalog.face_profiles[selection.face]
		feature_sources = profile.feature_sources
		feature_regions = profile.regions
	for feature in ["brows", "eyes", "nose", "mouth"]:
		_apply_feature(head, _images[feature_sources[feature][selection[feature]]], feature_regions[feature])
	if selection.beard == 1:
		_apply_feature(head, _images["features-beard"], catalog.regions.beard)
	_skin_tint(head, selection.skin)
	var hair: Image
	if selection.hair == 1:
		var empty := Image.create(SIZE.x, SIZE.y, false, Image.FORMAT_RGBA8)
		head.blit_rect_mask(empty, _images["cap-mask"], Rect2i(Vector2i.ZERO, SIZE), Vector2i.ZERO)
		hair = _material("cap", selection.skin, selection.hair_color)
	else:
		hair = _material("hair-" + str(selection.face), 0, selection.hair_color)
	if selection.beard == 1 and selection.hair_color != 0:
		_beard_tint(head, selection.hair_color)
	var outfit := _outfit(selection.outfit, selection.cloth_color)
	var output := Image.create(SIZE.x, SIZE.y, false, Image.FORMAT_RGBA8)
	output.blend_rect(outfit, Rect2i(Vector2i.ZERO, SIZE), Vector2i.ZERO)
	output.blend_rect(head, Rect2i(Vector2i.ZERO, SIZE), Vector2i.ZERO)
	output.blend_rect(hair, Rect2i(Vector2i.ZERO, SIZE), Vector2i.ZERO)
	var framing: Array = catalog.get("portrait_crop", [0, 0, SIZE.x, SIZE.y])
	output = output.get_region(Rect2i(int(framing[0]), int(framing[1]), int(framing[2]), int(framing[3])))
	output.resize(512, 768, Image.INTERPOLATE_LANCZOS)
	_cache[key] = output
	_cache_order.append(key)
	if _cache_order.size() > CACHE_LIMIT:
		_cache.erase(_cache_order.pop_front())
	return {"ok": true, "image": output.duplicate(), "cached": false}

## 头像始终从同一已合成半身的正方形安全区派生，禁止非等比挤压。
func avatar(portrait: Image, dimension: int) -> Image:
	if not dimension in [88, 96, 310]:
		return null
	var crop: Array = catalog.avatar_crop
	var image := portrait.get_region(Rect2i(int(crop[0]), int(crop[1]), int(crop[2]), int(crop[3])))
	image.resize(dimension, dimension, Image.INTERPOLATE_LANCZOS)
	return image

## 在固定形状区域替换部件，alpha与外围皮肤保持基底，不引入矩形肤色接缝。
func _apply_feature(target: Image, edited: Image, regions: Array) -> void:
	for values in regions:
		var center := Vector2(values[0], values[1]) * 2.0
		var radius := Vector2(values[2], values[3]) * 2.0
		for y in range(maxi(0, floori(center.y - radius.y)), mini(SIZE.y, ceili(center.y + radius.y + 1))):
			for x in range(maxi(0, floori(center.x - radius.x)), mini(SIZE.x, ceili(center.x + radius.x + 1))):
				var distance := ((Vector2(x, y) - center) / radius).length()
				var weight := 1.0 - smoothstep(0.72, 1.0, distance)
				if weight <= 0.0:
					continue
				var old := target.get_pixel(x, y)
				var color := old.lerp(edited.get_pixel(x, y), weight)
				color.a = old.a
				target.set_pixel(x, y, color)

## 肤色只作用于温暖肤质区域，完整头颈统一处理并保留原画明暗。
func _skin_tint(image: Image, palette: int) -> void:
	if palette == 0:
		return
	var scale := Vector3(0.88, 0.82, 0.77) if palette == 1 else Vector3(1.055, 1.065, 1.075)
	var area: Array = catalog.get("skin_rect", [172, 130, 202, 266])
	for y in range(int(area[1] * 2), int((area[1] + area[3]) * 2)):
		for x in range(int(area[0] * 2), int((area[0] + area[2]) * 2)):
			var color := image.get_pixel(x, y)
			if color.a < 0.05 or not _is_skin(color):
				continue
			color.r = clampf(color.r * scale.x, 0.0, 1.0)
			color.g = clampf(color.g * scale.y, 0.0, 1.0)
			color.b = clampf(color.b * scale.z, 0.0, 1.0)
			image.set_pixel(x, y, color)

## 判断肤质限定区颜色，内衣和深发色不跟随肤色漂移。
func _is_skin(color: Color) -> bool:
	return color.r > 0.32 and color.r > color.g * 1.13 and color.g > color.b * 1.12

## 已受约束的发冠材料缓存；巾帻布料不当成头发染灰。
func _material(id: String, skin: int, hair_color: int) -> Image:
	var key := id + ":" + str(skin) + ":" + str(hair_color)
	if _materials.has(key):
		return _materials[key]
	var image: Image = _images[id].duplicate()
	if id == "cap":
		_skin_tint(image, skin)
	if hair_color != 0:
		for y in int(catalog.get("hair_tint_height", 290) * 2):
			for x in SIZE.x:
				var color := image.get_pixel(x, y)
				if color.a < 0.05:
					continue
				if id == "cap":
					if catalog.get("cap_tint_policy", "top_left_bounds") == "cool_cloth":
						if color.b > color.r * 1.01:
							continue
					else:
						var bounds: Array = catalog.get("cap_tint_bounds", [165, 171])
						if y < bounds[1] * 2 or x < bounds[0] * 2:
							continue
				var skin_weight := smoothstep(0.23, 0.40, color.r) * smoothstep(0.07, 0.14, color.r - color.g) * smoothstep(0.02, 0.07, color.g - color.b)
				var ribbon_weight := smoothstep(0.10, 0.20, color.r - color.g)
				image.set_pixel(x, y, color.lerp(_hair_tone(color, hair_color), (1.0 - skin_weight) * (1.0 - ribbon_weight)))
	_materials[key] = image
	return image

## 须色只调整局部深色生长线，嘴唇和周围皮肤不染成须色。
func _beard_tint(image: Image, palette: int) -> void:
	for values in catalog.regions.beard:
		var center := Vector2(values[0], values[1]) * 2.0
		var radius := Vector2(values[2], values[3]) * 2.0
		for y in range(int(center.y - radius.y), int(center.y + radius.y)):
			for x in range(int(center.x - radius.x), int(center.x + radius.x)):
				if ((Vector2(x, y) - center) / radius).length() > 0.82:
					continue
				var color := image.get_pixel(x, y)
				if color.r < 0.36 and color.a > 0.1:
					image.set_pixel(x, y, _hair_tone(color, palette))

## 两种有限发须调色保留原有高光，不支持自由虹彩或破坏材质的全图染色。
func _hair_tone(color: Color, palette: int) -> Color:
	if palette == 1:
		return Color(minf(1.0, color.r * 1.22), color.g * 1.02, color.b * 0.88, color.a)
	var level := color.r * 0.3 + color.g * 0.59 + color.b * 0.11
	return Color(minf(1.0, level * 1.30 + 0.11), minf(1.0, level * 1.30 + 0.10), minf(1.0, level * 1.30 + 0.08), color.a)

## 衣色读取每族自己的材料区，保护内领、腰带与皮具，不共享另一族身体坐标。
func _outfit(index: int, palette: int) -> Image:
	var key := "outfit:" + str(index) + ":" + str(palette)
	if _materials.has(key):
		return _materials[key]
	var image: Image = _images["outfit-" + str(index)].duplicate()
	var profile := _outfit_profile(index)
	var belt_polygon := _polygon(profile.belt)
	var collar_polygon := _polygon(profile.collar)
	if palette != 0:
		var factors: Array = profile.scales[palette - 1]
		var scale := Vector3(factors[0], factors[1], factors[2])
		for y in SIZE.y:
			for x in SIZE.x:
				var color := image.get_pixel(x, y)
				if color.a < 0.05:
					continue
				var logical_point := Vector2(x, y) * 0.5
				var fabric: bool = color.b > color.r * 1.03 if profile.mode == "cool" else true
				if profile.get("exclude_cool_materials", false) and (color.g >= color.r * 0.98 or color.b >= color.r * 0.98):
					fabric = false
				if profile.mode == "warm" or profile.get("protect_cool", false):
					var belt := Geometry2D.is_point_in_polygon(logical_point, belt_polygon)
					var collar := Geometry2D.is_point_in_polygon(logical_point, collar_polygon) or (color.r > 0.65 and color.g > 0.60 and color.b > 0.48)
					fabric = fabric and not belt and not collar
				if profile.has("strap"):
					var strap: Array = profile.strap
					var distance := logical_point.distance_to(Geometry2D.get_closest_point_to_segment(logical_point, Vector2(strap[0], strap[1]), Vector2(strap[2], strap[3])))
					fabric = fabric and distance > strap[4]
				if fabric:
					image.set_pixel(x, y, Color(clampf(color.r * scale.x, 0.0, 1.0), clampf(color.g * scale.y, 0.0, 1.0), clampf(color.b * scale.z, 0.0, 1.0), color.a))
	_materials[key] = image
	return image

## 首族保留已验收的材料参数；新增族显式提供自身定位，不能靠默认参数凑图。
func _outfit_profile(index: int) -> Dictionary:
	if catalog.has("outfit_tint"):
		return catalog.outfit_tint[index]
	var result := {"mode": "cool" if index == 0 else "warm", "scales": [[0.83, 1.10, 0.84], [1.25, 0.80, 0.74]] if index == 0 else [[0.82, 1.12, 0.90], [1.18, 0.82, 0.86]], "belt": [[114, 658], [395, 650], [413, 680], [406, 712], [376, 727], [377, 768], [337, 768], [349, 715], [114, 719]], "collar": [[171, 277], [194, 278], [226, 315], [268, 364], [291, 390], [305, 339], [309, 295], [322, 336], [331, 418], [307, 477], [286, 518], [289, 464], [274, 420], [228, 365], [192, 323], [171, 294]]}
	if index == 1:
		result.strap = [110, 345, 359, 647, 35]
	return result

## 作者区域顶点只转换数值表示，不执行脸部或衣身几何变形。
func _polygon(values: Array) -> PackedVector2Array:
	var points := PackedVector2Array()
	for value in values:
		points.append(Vector2(value[0], value[1]))
	return points

## 按目录字段构造缓存键，调用者字典顺序不影响结果。
func _state_key(selection: Dictionary) -> String:
	var parts := PackedStringArray()
	for field in catalog.defaults:
		parts.append(field + "=" + str(selection[field]))
	return "|".join(parts)
