## 管理表现资源ID与缓存，不保存人物、任务或周目状态。
## 完整校验后才切换清单；失败保留旧资源，供编辑器继续预览。
extends RefCounted
const Strict = preload("res://core/strict_json.gd")
const Contract = preload("res://ui/presentation_contract.gd")
const Shape = preload("res://core/content_shape.gd")
const SCHEMA_PATH = "res://data/presentation.schema.json"
var _data: Dictionary = {}
var _textures: Dictionary = {}
var _assets: Dictionary = {}
var _resource_root: String = "res://"

## 读取限定大小的清单并返回结构化结果，失败不会清空当前数据。
func load_from_path(path: String, resource_root: String="res://") -> Dictionary:
	var parsed: Dictionary = _read_json(path)
	if not parsed.ok: return parsed
	return configure(parsed.value,resource_root)

## 校验结构、默认引用和真实纹理；全部通过后一次性替换数据与缓存。
func configure(candidate: Dictionary, resource_root: String="res://") -> Dictionary:
	var bounded: Dictionary = _bounded(candidate)
	if not bounded.ok: return bounded
	# 先界定原始候选，避免JSON.stringify把非有限数或对象悄悄改为其他值。
	var text: String = JSON.stringify(candidate)
	if text.to_utf8_buffer().size() > 1048576: return _error("", "ManifestTooLarge")
	var normalized: Dictionary = Strict.parse_text(text)
	if not normalized.ok: return normalized
	candidate = normalized.value
	var schema: Dictionary = _read_json(SCHEMA_PATH)
	if not schema.ok: return schema
	var keys: Dictionary = _key_check(candidate)
	if not keys.ok: return keys
	var checked: Dictionary = Shape.validate(candidate, schema.value, schema.value, candidate)
	if not checked.ok: return checked
	for group in ["components", "character_profiles", "scene_templates"]:
		if candidate.version == 1 and candidate.has(group): return _error("/" + group, "VersionField")
		if candidate.version == 2 and not candidate.has(group): return _error("/" + group, "RequiredGroup")
		if candidate.get(group, {}).size() > (512 if group == "components" else 128): return _error("/" + group, "TooManyRecords")
	var loaded: Dictionary = {}
	var assets: Dictionary = {}
	var decoded: Dictionary = {}
	for group in ["backgrounds", "portraits"]:
		if candidate[group].size() > 512: return _error("/" + group, "TooManyAssets")
		for id in candidate[group]:
			var path: String = candidate[group][id].path
			var location: String = "/" + group + "/" + Contract.segment(id) + "/path"
			var safe: Dictionary = _asset_path(path,resource_root)
			if not safe.ok: return _error(location, safe.code)
			if not decoded.has(path):
				var image_result: Dictionary = _decode(_asset_location(path,resource_root))
				if not image_result.ok: return _error(location, image_result.code)
				decoded[path] = image_result.bounds
				loaded[path] = image_result.texture
			assets[group + "/" + id] = decoded[path]
	checked = Contract.validate(candidate, assets)
	if not checked.ok: return checked
	_data = candidate.duplicate(true)
	_textures = loaded
	_assets = assets
	_resource_root = resource_root
	return {"ok": true}

## 限制候选深度、节点与类型；StringName键仅为现有脚本字典赋值兼容。
func _bounded(value: Variant) -> Dictionary:
	var todo: Array = [[value, 0]]
	var count: int = 0
	while not todo.is_empty():
		var pair: Array = todo.pop_back()
		var node: Variant = pair[0]
		var depth: int = pair[1]
		count += 1
		if depth > 32 or count > 200000: return _error("", "ManifestLimits")
		if node is Dictionary:
			if node.size() > 512: return _error("", "ManifestLimits")
			for key in node:
				if not (key is String or key is StringName) or str(key).length() > 16384: return _error("", "ManifestType")
				todo.append([node[key], depth + 1])
		elif node is Array:
			for entry in node: todo.append([entry, depth + 1])
		elif node is String:
			if node.length() > 16384: return _error("", "ManifestLimits")
		elif typeof(node) in [TYPE_INT, TYPE_FLOAT]:
			if not is_finite(float(node)) or abs(node) > 2147483647: return _error("", "InvalidNumber")
		elif node != null and not node is bool: return _error("", "ManifestType")
	return {"ok": true}

## 提前拒绝Schema不允许的分隔键，字段诊断保留JSON指针转义。
func _key_check(value: Variant) -> Dictionary:
	var todo: Array = [[value, ""]]
	while not todo.is_empty():
		var entry: Array = todo.pop_back()
		var node: Variant = entry[0]
		var path: String = entry[1]
		if node is Dictionary:
			for key in node:
				if "/" in key or "~" in key: return _error(path + "/" + Contract.segment(key), "InvalidKey")
				todo.append([node[key], path + "/" + key])
		elif node is Array:
			for n in range(node.size()): todo.append([node[n], path + "/" + str(n)])
	return {"ok": true}

## 源工程拒绝任意路径段符号链接；导出包则只接受ResourceLoader的包内路径。
func _asset_path(path: String, resource_root: String="res://") -> Dictionary:
	if not path.begins_with("res://assets/art/"): return _error("", "AssetPath")
	if resource_root!="res://":
		var roots=RegEx.new();roots.compile("^res://data/generated/content-sets/[0-9a-f]{64}/(?![\\s\\S])")
		if roots.search(resource_root)==null: return _error("", "AssetPath")
	path=_asset_location(path,resource_root)
	var current: String = "res://"
	for part in path.trim_prefix("res://").split("/"):
		if part in ["", ".", ".."]: return _error("", "AssetPath")
		var directory = DirAccess.open(current)
		if directory != null and directory.is_link(part): return _error("", "AssetPath")
		current = current.path_join(part)
	if not ResourceLoader.exists(path) and not FileAccess.file_exists(path): return _error("", "MissingAsset")
	return {"ok": true}

## 保持清单逻辑路径和摘要不变，仅从受信内容集基址解析实际资源文件。
func _asset_location(path: String, resource_root: String) -> String:
	return path if resource_root=="res://" else resource_root.path_join(path.trim_prefix("res://"))

## 原文件可用时真实解码像素；原生包从导入纹理解码，沿用引擎的路径共享缓存。
func _decode(path: String) -> Dictionary:
	var image: Image
	if FileAccess.file_exists(path):
		var file = FileAccess.open(path, FileAccess.READ)
		if file == null: return _error("", "MissingAsset")
		if file.get_length() > 16777216: file.close(); return _error("", "AssetTooLarge")
		var raw: PackedByteArray = file.get_buffer(file.get_length()); file.close()
		# 已知截断PNG先返回有界诊断，合法候选仍须由真实解码器完整读取。
		if path.ends_with(".png") and (raw.size() < 33 or raw.slice(0, 8) != PackedByteArray([137,80,78,71,13,10,26,10]) or raw.slice(raw.size()-8, raw.size()-4).get_string_from_ascii() != "IEND"): return _error("", "AssetDecode")
		if path.get_extension() != "svg":
			var size: Array = Contract.raster_size(raw, path.get_extension())
			if size.is_empty() or size[0] <= 0 or size[1] <= 0: return _error("", "AssetDecode")
			if size[0] * size[1] > 16777216: return _error("", "AssetTooLarge")
		image = Image.new()
		var decode_result: Error = ERR_FILE_UNRECOGNIZED
		match path.get_extension():
			"png": decode_result = image.load_png_from_buffer(raw)
			"webp": decode_result = image.load_webp_from_buffer(raw)
			"jpg", "jpeg": decode_result = image.load_jpg_from_buffer(raw)
			"svg":
				if not Contract.svg_source_valid(raw): return _error("", "AssetDecode")
				decode_result = image.load_svg_from_buffer(raw)
		if decode_result != OK or image.is_empty(): return _error("", "AssetDecode")
	var texture: Texture2D
	if ResourceLoader.exists(path):
		var resource = load(path)
		if not resource is Texture2D: return _error("", "AssetType")
		texture = resource
	elif image != null:
		texture = ImageTexture.create_from_image(image)
	if texture == null: return _error("", "MissingAsset")
	if image == null: image = texture.get_image()
	if image == null or image.is_empty(): return _error("", "AssetDecode")
	if image.get_width() * image.get_height() > 16777216: return _error("", "AssetTooLarge")
	if path.ends_with(".svg"): return {"ok": true, "texture": texture, "bounds": [[], []]}
	var bounds: Rect2i = image.get_used_rect()
	var rect: Array = [] if not bounds.has_area() else [bounds.position.x, bounds.position.y, bounds.size.x, bounds.size.y]
	return {"ok": true, "texture": texture, "bounds": [[image.get_width(), image.get_height()], rect]}

## 公开组合验证仅使用当前已接受清单；拒绝的调用不会切换注册表或派生元数据。
func validate_composition(profile: Dictionary) -> Dictionary:
	var bounded: Dictionary = _bounded(profile)
	if not bounded.ok: return bounded
	if JSON.stringify(profile).to_utf8_buffer().size() > 1048576: return _error("", "ManifestTooLarge")
	var candidate: Dictionary = _data.duplicate(true)
	if candidate.is_empty(): return _error("", "RegistryEmpty")
	var normalized: Dictionary = Strict.parse_text(JSON.stringify(profile))
	if not normalized.ok: return normalized
	var schema: Dictionary = _read_json(SCHEMA_PATH)
	if not schema.ok: return schema
	var value: Dictionary = normalized.value
	var keys: Dictionary = _key_check(value)
	if not keys.ok: return keys
	var shape: Dictionary = Shape.validate(value, schema.value["$defs"].profile, schema.value, candidate)
	if not shape.ok: return shape
	candidate.get_or_add("character_profiles", {})[value.id] = value
	return Contract.validate(candidate, _assets)

## 返回独立副本，防止界面或工具直接修改已生效的注册数据。
func describe() -> Dictionary:
	return _data.duplicate(true)

## 返回已通过清单装载的内容集基址；逻辑清单和身份摘要始终保持原样。
func resource_root() -> String:
	return _resource_root

## 枚举指定类别的ID，供编辑器引用选择器共用。
func ids(group: String) -> Array:
	return _data.get(group, {}).keys()

## 获取指定纹理；空ID采用清单默认值，拼错的非空ID不静默替换。
func texture(group: String, id: String) -> Texture2D:
	var fallback: String = "default_background" if group == "backgrounds" else "default_portrait"
	var key: String = id if not id.is_empty() else _data.get(fallback, "")
	var entry: Dictionary = _data.get(group, {}).get(key, {})
	return _textures.get(entry.get("path", ""))

## 将清单中的百分比转换成入口锚点；未配置入口时使用中间位置。
func entrance(id: String) -> Vector2:
	var point: Array = _data.get("backgrounds", {}).get(id, {}).get("entrance", [50, 45])
	return Vector2(point[0] / 100.0, point[1] / 100.0)

## 检查内容包中的表现引用，只读校验，不修改任务数据或存档摘要。
func validate_content(content: Dictionary) -> Dictionary:
	var parsed: Dictionary = _read_json("res://data/generated/content.schema.json")
	if not parsed.ok: return parsed
	var schema: Dictionary = parsed.value
	for table in schema["x-hanmo-tables"]:
		var resolved: Dictionary = _reference_rule(schema, schema.properties[table].items)
		if not resolved.ok: return resolved
		var rule: Dictionary = resolved.value
		for field in rule.get("properties", {}):
			resolved = _reference_rule(schema, rule.properties[field])
			if not resolved.ok: return resolved
			var property: Dictionary = resolved.value
			var group: String = property.get("x-hanmo-presentation", "")
			if group.is_empty() and property.get("type") == "object":
				var groups: Array = []
				for child in property.get("properties", {}).values():
					resolved = _reference_rule(schema, child)
					if not resolved.ok: return resolved
					var child_group: String = resolved.value.get("x-hanmo-presentation", "")
					if not child_group in groups: groups.append(child_group)
				if groups.size() == 1: group = groups[0]
			if group.is_empty(): continue
			var rows: Variant = content.get(table, {})
			var values: Array = rows.values() if rows is Dictionary else rows
			for record in values:
				if not record.has(field): continue
				var id: Variant = record[field]
				var path: String = "/" + table + "/" + Contract.segment(str(record.get("id", ""))) + "/" + field
				if group == "scene_templates" and property.get("type") == "object":
					if not id is Dictionary: return _error(path, "SceneScale")
					for view in id:
						var location: String = path + "/" + Contract.segment(str(view))
						if not view in ["world", "city", "interior"]: return _error(location, "SceneScale")
						var template_id: Variant = id[view]
						if not template_id is String or not template_id in ids(group): return _error(location, "UnknownPresentation")
						if _data[group][template_id].view_kind != view: return _error(location, "SceneScale")
					continue
				if id == "" and group in ["backgrounds", "portraits"]: continue
				if not id is String or not id in ids(group): return _error(path, "UnknownPresentation")
				if group == "scene_templates" and ((table == "world_maps") != (_data[group][id].view_kind == "world")): return _error(path, "SceneScale")
	return {"ok": true}

## 沿受信本地defs读取真实引用注解；坏链或循环明确失败，与作者Python验证保持同义。
func _reference_rule(schema: Dictionary, value: Variant) -> Dictionary:
	var seen: Array = []
	while value is Dictionary and value.has("$ref"):
		var reference: Variant = value["$ref"]
		if not reference is String or not reference.begins_with("#/$defs/") or reference in seen:
			return _error("res://data/generated/content.schema.json", "InvalidPresentationReferenceSchema")
		var key: String = reference.trim_prefix("#/$defs/")
		if not schema.get("$defs", {}).has(key): return _error("res://data/generated/content.schema.json", "InvalidPresentationReferenceSchema")
		seen.append(reference)
		value = schema["$defs"][key]
	if not value is Dictionary: return _error("res://data/generated/content.schema.json", "InvalidPresentationReferenceSchema")
	return {"ok": true, "value": value}

## 限制清单体积并用已有严格JSON解析器拒绝重复键等错误。
func _read_json(path: String) -> Dictionary:
	var file = FileAccess.open(path, FileAccess.READ)
	if file == null: return _error(path, "ManifestOpenFailed")
	if file.get_length() > 1048576:
		file.close(); return _error(path, "ManifestTooLarge")
	var text: String = file.get_as_text(); file.close()
	var parsed: Dictionary = Strict.parse_text(text)
	if not parsed.ok: return parsed
	if not parsed.value is Dictionary: return _error(path, "ManifestObjectExpected")
	return parsed

## 提供稳定错误码及字段路径，避免编辑器和运行时依赖异常文本。
func _error(path: String, code: String) -> Dictionary:
	return {"ok": false, "code": code, "path": path}
