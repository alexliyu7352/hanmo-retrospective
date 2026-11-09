## 纯会话人物选件状态；拒绝非法组合，支持撤销和显式随机，不读写世界或存档。
extends RefCounted
signal changed
var catalog: Dictionary = {}
var _state: Dictionary = {}
var _entry: Dictionary = {}
var _undo: Array[Dictionary] = []
var _redo: Array[Dictionary] = []
var _locks: Dictionary = {}
const HISTORY_LIMIT := 48

## 检查目录和起始草案；失败时保留旧会话，不静默回落到其他人物。
func configure(definition: Dictionary, initial: Dictionary = {}) -> Dictionary:
	if not definition.get("defaults") is Dictionary or not definition.get("fields") is Dictionary:
		return {"ok": false, "reason": "人物目录不完整。"}
	var previous := catalog
	catalog = definition.duplicate(true)
	for field in catalog.defaults:
		catalog.defaults[field] = int(catalog.defaults[field])
	var candidate: Dictionary = catalog.defaults.duplicate(true) if initial.is_empty() else initial.duplicate(true)
	var check := validate(candidate)
	if not check.ok:
		catalog = previous
		return check
	_state = candidate
	_entry = candidate.duplicate(true)
	_undo.clear()
	_redo.clear()
	_locks.clear()
	changed.emit()
	return {"ok": true}

## 返回独立草案，调用者无法绕过校验直接修改会话。
func state() -> Dictionary:
	return _state.duplicate(true)

## 逐项检查类型、候选和兼容关系；错误不会修改其他组选件。
func validate(candidate: Dictionary) -> Dictionary:
	if candidate.size() != catalog.get("defaults", {}).size():
		return {"ok": false, "reason": "选件字段不完整。"}
	for field in catalog.get("defaults", {}):
		if not candidate.has(field) or not typeof(candidate[field]) == TYPE_INT:
			return {"ok": false, "reason": "选件类型不正确。"}
		if candidate[field] < 0 or candidate[field] >= catalog.fields[field].options.size():
			return {"ok": false, "reason": "没有这项人物部件。"}
	for rule in catalog.get("compatibility", []):
		if _matches(candidate, rule.when) and not _matches(candidate, rule.requires):
			return {"ok": false, "reason": rule.reason}
	return {"ok": true}

## 根据稳定字段选择一项，未知字段与不相容项直接拒绝。
func choose(field: String, index: int) -> Dictionary:
	if not _state.has(field):
		return {"ok": false, "reason": "未知人物选件。"}
	var candidate := state()
	candidate[field] = index
	return apply(candidate)

## 原子接受完整草案，重复选择不新增撤销步骤。
func apply(candidate: Dictionary) -> Dictionary:
	var result := validate(candidate)
	if not result.ok:
		return result
	if candidate == _state:
		return {"ok": true, "changed": false}
	_undo.append(state())
	if _undo.size() > HISTORY_LIMIT:
		_undo.pop_front()
	_redo.clear()
	_state = candidate.duplicate(true)
	changed.emit()
	return {"ok": true, "changed": true}

## 锁定只影响之后的显式随机，手动选件仍由用户控制。
func set_locked(field: String, locked: bool) -> void:
	if _state.has(field):
		_locks[field] = locked

## 只读查询选件锁，不泄露可变内部字典。
func is_locked(field: String) -> bool:
	return _locks.get(field, false)

## 显式种子随机只搜索兼容候选，保留所有锁定字段，不使用世界随机源。
func randomize_selection(seed_value: int) -> Dictionary:
	var all_locked := true
	for field in _state:
		all_locked = all_locked and is_locked(field)
	if all_locked:
		return {"ok": true, "changed": false, "reason": "全部选件已锁定，人物保持不变。"}
	var rng := RandomNumberGenerator.new()
	rng.seed = seed_value
	for attempt in 256:
		var candidate := state()
		for field in candidate:
			if not is_locked(field):
				candidate[field] = rng.randi_range(0, catalog.fields[field].options.size() - 1)
		if validate(candidate).ok:
			return apply(candidate)
	return {"ok": false, "reason": "当前锁定条件下未找到新组合，已保留原人物。"}

## 当前组恢复到进入时选件，不动其他组；兼容冲突明确拒绝。
func reset_group(group_id: String) -> Dictionary:
	var candidate := state()
	for group in catalog.groups:
		if group.id == group_id:
			for field in group.fields:
				candidate[field] = _entry[field]
			return apply(candidate)
	return {"ok": false, "reason": "未知选件分组。"}

## 恢复进入工坊时整套草案，仍可撤销这次恢复。
func restore_entry() -> Dictionary:
	return apply(_entry)

## 撤销恰好一次成功修改，不产生重新随机或隐式替换。
func undo() -> bool:
	if _undo.is_empty():
		return false
	_redo.append(state())
	_state = _undo.pop_back()
	changed.emit()
	return true

## 重做最近撤销结果；之后的手动编辑会清空这条重做分支。
func redo() -> bool:
	if _redo.is_empty():
		return false
	_undo.append(state())
	_state = _redo.pop_back()
	changed.emit()
	return true

## 返回历史可用性供按钮刷新，不暴露历史草案。
func history() -> Dictionary:
	return {"undo": not _undo.is_empty(), "redo": not _redo.is_empty()}

## 条件必须全部相等才匹配，用于读取声明式兼容规则。
func _matches(candidate: Dictionary, condition: Dictionary) -> bool:
	for field in condition:
		if candidate.get(field) != condition[field]:
			return false
	return true
