## 只读已验收代表目录；用稳定比例族ID还原完整草案，不读取或写入世界、主入口或存档。
extends RefCounted
const FittingSession = preload("res://src/fitting_session.gd")
const SOURCE_PATH := "res://representatives.json"
## 第一版只包含已验收的三个成人族及每族四种结构；扩充范围必须显式改版。
const VERSION_ONE_FITTINGS := ["adult_male_medium_v2", "adult_female_round_v1", "adult_male_long_v1"]
const FACES_PER_FITTING := 4

## 读取正式样本定义；解析失败或任意条目非法时拒绝整批，不跳过或补默认值。
static func read(catalog: Dictionary) -> Dictionary:
	if not FileAccess.file_exists(SOURCE_PATH):
		return _failure("已验收代表目录缺失。")
	var parser := JSON.new()
	if parser.parse(FileAccess.get_file_as_string(SOURCE_PATH)) != OK:
		return _failure("已验收代表目录无法解析。")
	return validate_definition(parser.data, catalog)

## 严格检查整批定义并委托既有会话验证完整状态；返回独立副本，来源和外部会话不变。
static func validate_definition(input: Variant, catalog: Dictionary) -> Dictionary:
	if not input is Dictionary or not _has_exact_keys(input, ["version", "samples"]):
		return _failure("已验收代表目录字段不完整或含未知字段。")
	if not _is_whole_number(input.version) or input.version != 1:
		return _failure("不支持这个已验收代表目录版本。")
	if not input.samples is Array or input.samples.is_empty():
		return _failure("已验收代表目录缺少样本。")
	if input.samples.size() != VERSION_ONE_FITTINGS.size() * FACES_PER_FITTING:
		return _failure("第一版已验收代表必须完整包含三族各四名，共十二名。")
	if not catalog.get("fittings") is Array or catalog.fittings.is_empty() or not _has_state_schema(catalog):
		return _failure("代表目录需要完整的运行时比例族目录。")
	var fitting_indices := {}
	for rig in catalog.fittings.size():
		var fitting = catalog.fittings[rig]
		if not fitting is Dictionary or not _is_label(fitting.get("id")) or not _is_label(fitting.get("label")):
			return _failure("运行时比例族标识或名称不合法。")
		if fitting_indices.has(fitting.id):
			return _failure("运行时比例族ID重复：" + fitting.id)
		if not fitting.get("definition") is Dictionary or not _has_state_schema(fitting.definition):
			return _failure("运行时比例族选件定义不完整：" + fitting.id)
		fitting_indices[fitting.id] = rig
	var validator := FittingSession.new()
	var configured: Dictionary = validator.configure(catalog)
	if not configured.ok:
		return _failure("运行时比例族无法验证代表：" + str(configured.get("reason", "")))
	var samples: Array[Dictionary] = []
	var sample_ids := {}
	var fitting_faces := {}
	for fitting_id in VERSION_ONE_FITTINGS:
		fitting_faces[fitting_id] = {}
	for entry in input.samples:
		if not entry is Dictionary or not _has_exact_keys(entry, ["id", "label", "fitting_id", "selection"]):
			return _failure("代表条目字段不完整或含未知字段。")
		if not _is_label(entry.id) or not _is_label(entry.label) or not _is_label(entry.fitting_id):
			return _failure("代表标识、名称或比例族ID不合法。")
		if sample_ids.has(entry.id):
			return _failure("代表ID重复：" + entry.id)
		if not fitting_indices.has(entry.fitting_id):
			return _failure("代表引用了未知比例族：" + entry.fitting_id)
		if not fitting_faces.has(entry.fitting_id):
			return _failure("第一版尚未验收这个代表比例族：" + entry.fitting_id)
		var rig: int = fitting_indices[entry.fitting_id]
		var fitting: Dictionary = catalog.fittings[rig]
		var definition: Dictionary = fitting.definition
		if not entry.selection is Dictionary or not _has_exact_keys(entry.selection, definition.defaults.keys()):
			return _failure("代表选件不完整或含未知字段：" + entry.id)
		var state := {}
		for field in entry.selection:
			var value = entry.selection[field]
			if not _is_whole_number(value) or value < 0 or value >= definition.fields[field].options.size():
				return _failure("代表选件必须是有效的整数索引：" + entry.id + "/" + str(field))
			state[field] = int(value)
		state.rig = rig
		var checked: Dictionary = validator.validate(state)
		if not checked.ok:
			return _failure("代表组合不合法：" + entry.id + "；" + str(checked.get("reason", "")))
		if fitting_faces[entry.fitting_id].has(state.face):
			return _failure("同一比例族的代表脸型重复：" + entry.id)
		fitting_faces[entry.fitting_id][state.face] = true
		sample_ids[entry.id] = true
		samples.append({"id": entry.id, "label": entry.label, "fitting_id": entry.fitting_id, "fitting_label": fitting.label, "state": state})
	for fitting_id in fitting_faces:
		if fitting_faces[fitting_id].size() != FACES_PER_FITTING:
			return _failure("已验收比例族必须各含四种不同脸型：" + fitting_id)
	return {"ok": true, "samples": samples, "reason": ""}

## 核对必要选件结构，避免把缺字段的外部族目录送进既有会话产生索引错误。
static func _has_state_schema(definition: Dictionary) -> bool:
	if not definition.get("defaults") is Dictionary or not definition.get("fields") is Dictionary or definition.defaults.is_empty():
		return false
	for field in definition.defaults:
		var field_definition = definition.fields.get(field)
		if not field_definition is Dictionary or not field_definition.get("options") is Array or field_definition.options.is_empty():
			return false
		if not _is_whole_number(definition.defaults[field]):
			return false
	return true

## 只接受完整且无额外字段的字典，不能用同样长度的未知字段替换缺项。
static func _has_exact_keys(value: Dictionary, keys: Array) -> bool:
	if value.size() != keys.size():
		return false
	for key in keys:
		if not value.has(key):
			return false
	return true

## JSON整数会解析为浮点数；只接受有限且无小数的数，禁止字符串、布尔值和截断。
static func _is_whole_number(value: Variant) -> bool:
	return value is int or (value is float and is_finite(value) and value == floor(value))

## 标识与名称必须是无首尾空白的非空文本，不自动修正作者数据。
static func _is_label(value: Variant) -> bool:
	return value is String and not value.is_empty() and value == value.strip_edges()

## 所有失败统一返回空代表数组，已通过的前缀不能泄漏给界面部分使用。
static func _failure(reason: String) -> Dictionary:
	var samples: Array[Dictionary] = []
	return {"ok": false, "samples": samples, "reason": reason}
