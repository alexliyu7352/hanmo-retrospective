## 正式成人代表的只读入口；整表通过后才按原九槽与衣着精确绑定，不保存第二份外貌真值。
extends RefCounted
const Appearance = preload("res://core/domains/identity/actor_appearance.gd")
const Strict = preload("res://core/strict_json.gd")
const PROFILE_ID = "regional.adult_representatives_v1"
const CATALOG_PATH = "res://data/accepted_adult_appearances.json"
const CATALOG_FIELDS = ["version", "scope", "age_domain", "records"]
const RECORD_FIELDS = ["id", "label", "base_features", "attire_style", "profile_id"]
const CATALOG_SCOPE = "qingchuan_adult_samples_v1"
const MAX_CATALOG_BYTES = 65536

## 每次返回完整且独立的十二组作者选择；目录失效只返回空表，诊断由status提供。
static func records() -> Array[Dictionary]:
	var loaded: Dictionary = _read_catalog()
	var output: Array[Dictionary] = []
	if loaded.ok: output.assign(loaded.records)
	return output

## 单独查询正式目录是否可用；不向调用者暴露部分通过的记录或静默修补。
static func status() -> Dictionary:
	var loaded: Dictionary = _read_catalog()
	return {"ok": true} if loaded.ok else loaded

## 原五字段协议先行校验；只有非legacy完整元组能得到登记绑定，缺图不改换默认人物。
static func resolve_profile_id(appearance_view: Variant) -> Dictionary:
	var visible: Dictionary = Appearance.validate_visible(appearance_view)
	if not visible.ok: return visible
	if appearance_view.legacy: return _fail("UnsupportedAdultAppearanceLegacy")
	var loaded: Dictionary = _read_catalog()
	if not loaded.ok: return loaded
	for record in loaded.records:
		if record.base_features == appearance_view.base_features and record.attire_style == appearance_view.attire_style:
			return {"ok": true, "profile_id": record.profile_id, "sample_id": record.id}
	return _fail("UnmappedAdultAppearance")

## 校验作者目录的整批边界与唯一身份；任何坏项都原子失败，成功记录仅为深复制。
static func validate_catalog(value: Variant) -> Dictionary:
	if not value is Dictionary or not _exact(value, CATALOG_FIELDS): return _fail("InvalidAdultAppearanceCatalogShape")
	if typeof(value.version) != TYPE_INT or value.version != 1: return _fail("UnsupportedAdultAppearanceCatalogVersion")
	if not value.scope is String or value.scope != CATALOG_SCOPE: return _fail("InvalidAdultAppearanceCatalogScope")
	var ages: Variant = value.age_domain
	if not ages is Array or ages.size() != 2 or typeof(ages[0]) != TYPE_INT or typeof(ages[1]) != TYPE_INT or ages[0] != 18 or ages[1] != 59:
		return _fail("InvalidAdultAppearanceAgeDomain")
	if not value.records is Array or value.records.size() != 12: return _fail("InvalidAdultAppearanceCatalogCount")
	var ids: Dictionary = {}
	var accepted: Array[Dictionary] = []
	for index in range(value.records.size()):
		var record: Variant = value.records[index]
		var location: String = "/records/" + str(index)
		if not record is Dictionary or not _exact(record, RECORD_FIELDS): return _fail("InvalidAdultAppearanceRecord", location)
		if not record.id is String or not record.label is String or record.label.is_empty() or record.label.length() > 80 or record.label != record.label.strip_edges():
			return _fail("InvalidAdultAppearanceRecord", location)
		var visible: Dictionary = Appearance.validate_visible({"format_version": 1, "generator_version": Appearance.VERSION, "base_features": record.base_features, "attire_style": record.attire_style, "legacy": false})
		if not visible.ok: return _fail("InvalidAdultAppearanceFeatures", location)
		if record.id != record.base_features.face_shape: return _fail("InvalidAdultAppearanceIdentity", location)
		if ids.has(record.id): return _fail("DuplicateAdultAppearanceId", location)
		if not record.profile_id is String or record.profile_id != "accepted_adult." + record.id: return _fail("InvalidAdultAppearanceBinding", location)
		ids[record.id] = true
		accepted.append(record.duplicate(true))
	return {"ok": true, "records": accepted}

## 有界读取唯一作者表并沿原严格JSON解析；缺失、重复键或整表失效只传播结构化诊断。
static func _read_catalog() -> Dictionary:
	var file: FileAccess = FileAccess.open(CATALOG_PATH, FileAccess.READ)
	if file == null: return _fail("MissingAdultAppearanceCatalog")
	if file.get_length() > MAX_CATALOG_BYTES: return _fail("AdultAppearanceCatalogTooLarge")
	var parsed: Dictionary = Strict.parse_text(file.get_as_text())
	if not parsed.ok: return {"ok": false, "code": "InvalidAdultAppearanceCatalogJSON", "cause": parsed.code}
	return validate_catalog(parsed.value)

## 只接受精确字段集合；不从额外字段推断资源、不补缺少的语义槽。
static func _exact(value: Dictionary, fields: Array) -> bool:
	if value.size() != fields.size(): return false
	for field in fields:
		if not value.has(field): return false
	return true

## 返回可传播的失败码与坏记录位置；预期调用错误不输出引擎异常或默认绑定。
static func _fail(code: String, location: String = "") -> Dictionary:
	var result: Dictionary = {"ok": false, "code": code}
	if not location.is_empty(): result.at = location
	return result
