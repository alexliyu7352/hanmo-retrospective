## 多比例族会话沿用原子历史与锁定，切族只允许已适配选件，缺件不会暗换。
extends "res://src/studio_session.gd"
const LocalSession = preload("res://src/studio_session.gd")
var _validators: Array[RefCounted] = []

## 先分别验证各族目录，再建立带比例族编号的单一会话草案。
func configure(definition: Dictionary, initial: Dictionary = {}) -> Dictionary:
	if not definition.get("fittings") is Array:
		return {"ok": false, "reason": "缺少人物比例族。"}
	var previous := _validators
	var candidates: Array[RefCounted] = []
	for fitting in definition.fittings:
		var validator := LocalSession.new()
		var result: Dictionary = validator.configure(fitting.definition)
		if not result.ok:
			return result
		candidates.append(validator)
	_validators = candidates
	var result := super.configure(definition, initial)
	if not result.ok:
		_validators = previous
	return result

## 使用目标族自己的合法范围检查同一草案；失败时旧历史与所有选值保持不动。
func validate(candidate: Dictionary) -> Dictionary:
	if not candidate.get("rig") is int or candidate.rig < 0 or candidate.rig >= _validators.size():
		return {"ok": false, "reason": "没有这个人物比例族。"}
	var local: Dictionary = candidate.duplicate(true)
	local.erase("rig")
	var validator: RefCounted = _validators[candidate.rig]
	for field in local:
		if validator.catalog.fields.has(field) and local[field] is int and local[field] >= validator.catalog.fields[field].options.size():
			return {"ok": false, "reason": "「" + str(catalog.fittings[candidate.rig].label) + "」尚未制作这款" + str(validator.catalog.fields[field].label) + "；现有选择已保留。"}
	return validator.validate(local)

## 读取目标族部件名称和数量，首族的鼻口定位与脸部比例不会混入另一族。
func field_definition(field: String, candidate: Dictionary = {}) -> Dictionary:
	var current := state() if candidate.is_empty() else candidate
	if field == "rig":
		return catalog.fields.rig
	return _validators[current.rig].catalog.fields[field]

## 给局部渲染器的草案只移除族编号，不改任何外貌选择。
func local_state() -> Dictionary:
	var local := state()
	local.erase("rig")
	return local

## 返回稳定语意而非显示文案或偶然素材顺序，供跨族不暗换特征的验收。
func semantic_state() -> Dictionary:
	var current := state()
	var result := {}
	var definition: Dictionary = _validators[current.rig].catalog
	for field in definition.choice_semantics:
		result[field] = definition.choice_semantics[field][current[field]]
	return result
