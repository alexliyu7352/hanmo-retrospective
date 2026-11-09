## 三族成人的有限组合目录；只解析既有九槽与衣着，不写世界、身份或随机状态。
extends RefCounted
const Appearance = preload("res://core/domains/identity/actor_appearance.gd")
const Accepted = preload("res://ui/character_appearance_adapter.gd")
const Strict = preload("res://core/strict_json.gd")
const CATALOG_PATH = "res://data/adult_appearance_compositions.json"
const POLICY = "fixed_actor_appearance_composition_v1"
const FEATURES = ["brows", "eyes", "nose", "mouth"]
const REGISTRY_KEY = "adult_appearance_composition"

## 返回完整192条语义元组的独立副本；末项非法也不泄漏前缀。
static func records() -> Array[Dictionary]:
	var loaded: Dictionary = _read_catalog()
	var result: Array[Dictionary] = []
	if loaded.ok: result.assign(loaded.catalog.records.duplicate(true))
	return result

## 目录可用性独立于世界与预览控件，错误携带稳定诊断码。
static func status() -> Dictionary:
	var loaded: Dictionary = _read_catalog()
	return {"ok": true} if loaded.ok else loaded

## 按已接受骨相公开独立眉眼鼻口；只给稳定语义ID，不公开内部素材路径。
static func options_for_face(face_shape: String) -> Dictionary:
	var loaded: Dictionary = _read_catalog()
	if not loaded.ok: return loaded
	for face in loaded.catalog.faces:
		if face.id == face_shape:
			return {"ok": true, "options": loaded.catalog.options.duplicate(true), "fitting_id": face.fitting_id}
	return _fail("UnmappedAdultAppearance")

## 完整公开元组精确匹配后按本族choice_semantics反查索引；不把偶然同索引当作同选件。
static func resolve(appearance_view: Variant, manifest: Dictionary = {}, resource_base: String = "res://") -> Dictionary:
	var visible: Dictionary = Appearance.validate_visible(appearance_view)
	if not visible.ok: return visible
	if appearance_view.legacy: return _fail("UnsupportedAdultAppearanceLegacy")
	var loaded: Dictionary = _read_catalog()
	if not loaded.ok: return loaded
	var selected: Dictionary = {}
	for record in loaded.catalog.records:
		if record.base_features == appearance_view.base_features and record.attire_style == appearance_view.attire_style:
			selected = record
			break
	if selected.is_empty(): return _fail("UnmappedAdultAppearance")
	var roots: Dictionary = registry_entry(manifest, resource_base)
	if not roots.ok: return roots
	var registered: Dictionary = _read_json(roots.entry.catalog)
	if not registered.ok: return registered
	if registered.value != loaded.catalog: return _fail("MismatchedAdultCompositionCatalog")
	var origins: Dictionary = _source_bindings(loaded.catalog, roots.entry)
	if not origins.ok: return origins
	var family: Dictionary = {}
	var face: Dictionary = {}
	for entry in loaded.catalog.families:
		if entry.id == selected.fitting_id: family = entry
	for entry in loaded.catalog.faces:
		if entry.id == selected.base_features.face_shape: face = entry
	var parsed: Dictionary = _read_json(str(roots.entry.resource_root) + str(family.catalog))
	if not parsed.ok: return parsed
	var definition: Dictionary = parsed.value
	if not definition.get("defaults") is Dictionary or not definition.get("choice_semantics") is Dictionary or not definition.get("fields") is Dictionary:
		return _fail("InvalidAdultCompositionFitting")
	for field in definition.defaults:
		if not definition.fields.get(field) is Dictionary or not definition.fields[field].get("options") is Array or definition.fields[field].options.is_empty() or typeof(definition.defaults[field]) != TYPE_INT: return _fail("InvalidAdultCompositionFitting")
	if not _exact(definition.defaults, ["face", "brows", "eyes", "nose", "mouth", "hair", "beard", "skin", "hair_color", "outfit", "cloth_color"]): return _fail("InvalidAdultCompositionFitting")
	if not definition.get("compatibility", []) is Array: return _fail("InvalidAdultCompositionFitting")
	for rule in definition.get("compatibility", []):
		if not rule is Dictionary or not rule.get("when") is Dictionary or not rule.get("requires") is Dictionary or not rule.get("reason") is String: return _fail("InvalidAdultCompositionFitting")
		for condition in [rule.when, rule.requires]:
			for field in condition:
				if not definition.defaults.has(field) or typeof(condition[field]) != TYPE_INT: return _fail("InvalidAdultCompositionFitting")
	var selection: Dictionary = definition.defaults.duplicate(true)
	selection.face = face.face_index
	var semantics: Dictionary = {"brows": selected.base_features.brows, "eyes": selected.base_features.eyes, "nose": selected.base_features.nose, "mouth": selected.base_features.mouth, "hair": "bun", "beard": "none", "skin": "warm", "hair_color": "black", "outfit": "plain", "cloth_color": "original"}
	for field in semantics:
		var options: Variant = definition.choice_semantics.get(field)
		if not options is Array or options.count(semantics[field]) != 1: return _fail("MissingAdultCompositionSemantic")
		selection[field] = options.find(semantics[field])
	for sample in origins.samples:
		if sample.id != face.id: continue
		var expected: Dictionary = {}
		for record in Accepted.records():
			if record.id == face.id: expected = record.base_features
		for field in FEATURES:
			var source_index: Variant = sample.selection.get(field)
			if typeof(source_index) != TYPE_INT or source_index < 0 or source_index >= definition.choice_semantics[field].size() or definition.choice_semantics[field][source_index] != expected.get(field): return _fail("InvalidAdultCompositionSourceBinding")
	var session_script: Script = load(roots.entry.session)
	if session_script == null or not session_script.can_instantiate(): return _fail("MissingAdultCompositionSession")
	var session: RefCounted = session_script.new()
	var checked: Dictionary = session.configure(definition, selection)
	if not checked.get("ok", false): return _fail("InvalidAdultCompositionSelection")
	return {"ok": true, "record": selected.duplicate(true), "fitting_id": selected.fitting_id, "selection": selection, "definition": definition, "registry": roots.entry}

## 只接受唯一表现清单显式登记的工坊根；路径必须完整落在该根，不接受目录穿越。
static func registry_entry(manifest: Dictionary = {}, resource_base: String = "res://") -> Dictionary:
	if manifest.is_empty():
		var read: Dictionary = _read_json("res://data/presentation.json")
		if not read.ok: return read
		manifest = read.value
	var entry: Variant = manifest.get(REGISTRY_KEY)
	if not entry is Dictionary or not _exact(entry, ["catalog", "resource_root", "fittings", "representatives", "renderer", "session"]): return _fail("MissingAdultCompositionRegistry")
	if not entry.catalog is String or not entry.resource_root is String: return _fail("InvalidAdultCompositionRoot")
	var prefix: String = "res://"
	if entry.resource_root != "res://character_studio/":
		var pattern: RegEx = RegEx.new()
		pattern.compile("^res://assets/art/packs/[A-Za-z0-9][A-Za-z0-9_.-]*/character_studio/$")
		if pattern.search(entry.resource_root) == null or ".." in entry.resource_root: return _fail("InvalidAdultCompositionRoot")
		prefix = entry.resource_root.trim_suffix("character_studio/")
	if entry.catalog != prefix + "data/adult_appearance_compositions.json": return _fail("InvalidAdultCompositionRoot")
	if entry.renderer != "res://ui/adult_appearance_renderer.gd": return _fail("InvalidAdultCompositionRoot")
	for field in ["fittings", "representatives"]:
		if not entry[field] is String or not entry[field].begins_with(entry.resource_root) or ".." in entry[field] or "\\" in entry[field]: return _fail("InvalidAdultCompositionRoot")
	if entry.fittings != entry.resource_root + "fittings.json" or entry.representatives != entry.resource_root + "representatives.json" or entry.session != "res://character_studio/src/studio_session.gd": return _fail("InvalidAdultCompositionRoot")
	if resource_base != "res://":
		var pattern: RegEx = RegEx.new()
		pattern.compile("^res://data/generated/content-sets/[0-9a-f]{64}/$")
		if pattern.search(resource_base) == null: return _fail("InvalidAdultCompositionRoot")
	var actual: Dictionary = entry.duplicate(true)
	for field in ["catalog", "resource_root", "fittings", "representatives"]:
		actual[field] = resource_base + entry[field].trim_prefix("res://")
		if not safe_resource_path(actual[field]): return _fail("InvalidAdultCompositionRoot")
	return {"ok": true, "entry": actual}

## 所有源路径只能在已批准根内逐段读取；工程符号链接不会绕过内容包真实资源边界。
static func safe_resource_path(path: String) -> bool:
	if not path.begins_with("res://") or "\\" in path: return false
	var current: String = "res://"
	for part in path.trim_prefix("res://").trim_suffix("/").split("/"):
		if part in ["", ".", ".."]: return false
		var directory: DirAccess = DirAccess.open(current)
		if directory != null and directory.is_link(part): return false
		current = current.path_join(part)
	return true

## 比例族入口与十二骨相绑定均核对工坊原目录，登记项实际参与解析，不能只写一个来源标签。
static func _source_bindings(catalog: Dictionary, entry: Dictionary) -> Dictionary:
	var fittings: Dictionary = _read_json(entry.fittings)
	var representatives: Dictionary = _read_json(entry.representatives)
	if not fittings.ok: return fittings
	if not representatives.ok: return representatives
	if not fittings.value.get("fittings") is Array or fittings.value.fittings.size() != 3 or not representatives.value.get("samples") is Array or representatives.value.samples.size() != 12: return _fail("InvalidAdultCompositionSourceBinding")
	for family in catalog.families:
		var matched: int = 0
		for source in fittings.value.fittings:
			if not source is Dictionary: return _fail("InvalidAdultCompositionSourceBinding")
			if source.get("id") == family.id and source.get("catalog") == "res://" + family.catalog: matched += 1
		if matched != 1: return _fail("InvalidAdultCompositionSourceBinding")
	for face in catalog.faces:
		var matched: int = 0
		for source in representatives.value.samples:
			if not source is Dictionary or not source.get("selection") is Dictionary: return _fail("InvalidAdultCompositionSourceBinding")
			if source.get("id") == face.id and source.get("fitting_id") == face.fitting_id and source.selection.get("face") == face.face_index: matched += 1
		if matched != 1: return _fail("InvalidAdultCompositionSourceBinding")
	return {"ok": true, "samples": representatives.value.samples}

## 严格冻结三族十二骨相与每脸16组；旧样本元组必须全部存在且固定衣发肤身范围不变。
static func validate_catalog(value: Variant) -> Dictionary:
	if not value is Dictionary or not _exact(value, ["version", "scope", "age_domain", "families", "faces", "options", "records"]): return _fail("InvalidAdultCompositionCatalog")
	if typeof(value.version) != TYPE_INT or value.version != 1 or value.scope != "qingchuan_adult_compositions_v1": return _fail("InvalidAdultCompositionScope")
	if not value.age_domain is Array or value.age_domain.size() != 2 or typeof(value.age_domain[0]) != TYPE_INT or typeof(value.age_domain[1]) != TYPE_INT or value.age_domain != [18, 59]: return _fail("InvalidAdultCompositionScope")
	if not value.families is Array or value.families.size() != 3 or not value.faces is Array or value.faces.size() != 12 or not value.records is Array or value.records.size() != 192: return _fail("InvalidAdultCompositionCount")
	if not value.options is Dictionary or not _exact(value.options, FEATURES): return _fail("InvalidAdultCompositionOptions")
	var tokens: Dictionary = {}
	for field in FEATURES:
		if not value.options[field] is Array or value.options[field].size() != 2: return _fail("InvalidAdultCompositionOptions")
		tokens[field] = []
		for option in value.options[field]:
			if not option is Dictionary or not _exact(option, ["id", "label"]) or not option.id is String or not option.label is String or option.id.is_empty() or option.label.is_empty() or option.id in tokens[field]: return _fail("InvalidAdultCompositionOptions")
			tokens[field].append(option.id)
	var families: Dictionary = {}
	for family in value.families:
		if not family is Dictionary or not _exact(family, ["id", "label", "catalog"]) or not family.id is String or not family.label is String or not family.catalog is String: return _fail("InvalidAdultCompositionFitting")
		if families.has(family.id) or family.id.is_empty() or not family.catalog.begins_with("catalog_") or not family.catalog.ends_with(".json") or "/" in family.catalog or ".." in family.catalog: return _fail("InvalidAdultCompositionFitting")
		families[family.id] = []
	var accepted: Array[Dictionary] = Accepted.records()
	if accepted.size() != 12: return _fail("MissingAdultAppearanceCatalog")
	var faces: Dictionary = {}
	for face in value.faces:
		if not face is Dictionary or not _exact(face, ["id", "label", "fitting_id", "face_index"]) or not face.id is String or not face.label is String or not face.fitting_id is String or typeof(face.face_index) != TYPE_INT: return _fail("InvalidAdultCompositionFace")
		if faces.has(face.id) or not families.has(face.fitting_id) or face.face_index < 0 or face.face_index > 3 or face.face_index in families[face.fitting_id]: return _fail("InvalidAdultCompositionFace")
		var found: bool = false
		for sample in accepted:
			if sample.id == face.id: found = true
		if not found: return _fail("InvalidAdultCompositionFace")
		families[face.fitting_id].append(face.face_index)
		faces[face.id] = face
	var tuples: Dictionary = {}
	var counts: Dictionary = {}
	for record in value.records:
		if not record is Dictionary or not _exact(record, ["id", "label", "base_features", "attire_style", "fitting_id"]): return _fail("InvalidAdultCompositionRecord")
		var check: Dictionary = Appearance.validate_visible({"format_version": 1, "generator_version": Appearance.VERSION, "base_features": record.base_features, "attire_style": record.attire_style, "legacy": false})
		if not check.ok or not record.id is String or not record.label is String or not record.fitting_id is String: return _fail("InvalidAdultCompositionRecord")
		var base: Dictionary = record.base_features
		if not faces.has(base.face_shape) or record.fitting_id != faces[base.face_shape].fitting_id or record.attire_style != "plain" or base.hair != "bun" or base.beard != "none" or base.skin_palette != "warm" or base.build != "normal": return _fail("InvalidAdultCompositionScope")
		var id: String = base.face_shape
		for field in FEATURES:
			if not base[field] in tokens[field]: return _fail("InvalidAdultCompositionSemantic")
			id += "." + base[field]
		if record.id != id or tuples.has(id): return _fail("DuplicateAdultCompositionTuple")
		tuples[id] = true
		counts[base.face_shape] = int(counts.get(base.face_shape, 0)) + 1
	for face in faces:
		if counts.get(face) != 16: return _fail("InvalidAdultCompositionCount")
	for sample in accepted:
		var id: String = sample.id
		for field in FEATURES: id += "." + sample.base_features[field]
		if not tuples.has(id): return _fail("MissingAdultCompositionPreset")
	return {"ok": true, "catalog": value.duplicate(true)}

## 有界读取正式作者表并整批校验；不缓存会掩盖缺件或热更新失败的半成品。
static func _read_catalog() -> Dictionary:
	var parsed: Dictionary = _read_json(CATALOG_PATH)
	return validate_catalog(parsed.value) if parsed.ok else parsed

## 唯一严格JSON入口拒绝重复键、过大文件和非字典根。
static func _read_json(path: String) -> Dictionary:
	var file: FileAccess = FileAccess.open(path, FileAccess.READ)
	if file == null: return _fail("MissingAdultCompositionResource")
	if file.get_length() > 1048576: return _fail("AdultCompositionResourceTooLarge")
	var parsed: Dictionary = Strict.parse_text(file.get_as_text())
	if not parsed.ok or not parsed.value is Dictionary: return _fail("InvalidAdultCompositionJSON")
	return parsed

## 对作者合同采用精确字段集合，额外身份、颜色与路径槽不能悄然进入存档协议。
static func _exact(value: Dictionary, fields: Array) -> bool:
	if value.size() != fields.size(): return false
	for field in fields:
		if not value.has(field): return false
	return true

## 所有错误都不携带可绘制记录；宿主保留上一对成功图像。
static func _fail(code: String) -> Dictionary:
	return {"ok": false, "code": code}
