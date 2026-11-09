## 有界的街段内存编辑事务：预览与确定分开，取消不改原草案，撤销重做不读写存档。
extends RefCounted
const Model = preload("res://ui/city_layout_model.gd")
const Street = preload("res://ui/city_street_geometry.gd")
const HISTORY_LIMIT = 32
var _document: Dictionary = {}
var _original: Dictionary = {}
var _layout: Dictionary = {}
var _pending: Dictionary = {}
var _validation: Dictionary = {}
var _undo: Array = []
var _redo: Array = []
var _registry: RefCounted

## 新会话先验证整个街段；失败不接纳半份草案或继承旧历史。
func start(document: Dictionary, layout: Dictionary, registry: RefCounted) -> Dictionary:
	var checked: Dictionary = Model.resolve(document, layout, registry)
	if not checked.ok: return checked
	_document = document.duplicate(true); _original = layout.duplicate(true); _layout = layout.duplicate(true); _registry = registry
	_pending = {}; _validation = {}; _undo.clear(); _redo.clear()
	return {"ok": true}

## 对外返回独立草案，预览候选不混入已经确定的编辑结果。
func layout() -> Dictionary:
	return _layout.duplicate(true)

## 拖放只生成候选；地块由足点选择，未知地块与已有占地均明确失败。
func preview_move(id: String, position: Vector2) -> Dictionary:
	var next: Dictionary = _layout.duplicate(true); var found: bool = false
	for instance in next.get("instances", []):
		if instance.id == id and instance.interactive:
			instance.position = [position.x, position.y]
			instance.plot_id = Street.plot_at(next, position); found = true
	if not found: return {"ok": false, "code": "UnknownCityInstance"}
	return _preview(next)

## 道路预览实际改动折线坐标，渲染层随候选变化；不以隐藏属性假装已经改路。
func preview_road(id: String, delta: Vector2) -> Dictionary:
	var next: Dictionary = _layout.duplicate(true); var found: bool = false; var moved: Array = []
	for road in next.get("roads", []):
		if road.id != id: continue
		for point in road.points: point[0] += delta.x; point[1] += delta.y
		moved = road.get("node_ids", []).duplicate(); found = true
	if not found: return {"ok": false, "code": "UnknownCityRoad"}
	for road in next.get("roads", []):
		if road.id == id: continue
		for index in range(road.get("node_ids", []).size()):
			if road.node_ids[index] in moved: road.points[index][0] += delta.x; road.points[index][1] += delta.y
	return _preview(next)

## 一个作者节点可能被多条道路共用，拖放同时更新全部引用，连接不会被拆成两条悬空线。
func preview_road_node(id: String, position: Vector2) -> Dictionary:
	var next: Dictionary = _layout.duplicate(true); var found: bool = false
	for road in next.get("roads", []):
		for index in range(road.get("node_ids", []).size()):
			if road.node_ids[index] == id: road.points[index] = [position.x, position.y]; found = true
	if not found: return {"ok": false, "code": "UnknownCityRoadNode"}
	return _preview(next)

## 移除对象保留独立地块与道路；该地块之后在画面中明确为空地。
func preview_remove(id: String) -> Dictionary:
	var next: Dictionary = _layout.duplicate(true)
	next.instances = next.instances.filter(func(row: Dictionary) -> bool: return row.id != id)
	if next.instances.size() == _layout.instances.size(): return {"ok": false, "code": "UnknownCityInstance"}
	return _preview(next)

## 状态只选择作者已制作的显示变体，空地也保留同一实例与地块，不推进任何工程。
func preview_state(id: String, state: String) -> Dictionary:
	var next: Dictionary = _layout.duplicate(true); var found: bool = false
	for instance in next.instances:
		if instance.id == id and instance.interactive: instance.state = state; found = true
	if not found: return {"ok": false, "code": "UnknownCityInstance"}
	return _preview(next)

## 同地块替换保留名称、ID和当前状态；没有该状态或占地不兼容时拒绝，不偷偷恢复完工。
func preview_replace(id: String, archetype: String) -> Dictionary:
	var next: Dictionary = _layout.duplicate(true); var found: bool = false
	for instance in next.instances:
		if instance.id == id and instance.interactive:
			if instance.archetype != archetype: instance.erase("attachments")
			instance.archetype = archetype; found = true
	if not found: return {"ok": false, "code": "UnknownCityInstance"}
	return _preview(next)

## 部件开关仅对当前状态已经适配的作者槽位开放；其他状态保持隐藏，不能借开关修好建筑。
func preview_attachment(id: String, slot: String, visible: bool) -> Dictionary:
	var next: Dictionary = _layout.duplicate(true)
	for instance in next.instances:
		if instance.id != id or not instance.interactive: continue
		var archetype: Dictionary = _document.archetypes[instance.archetype]
		if not slot in archetype.get("display_states", {}).get(instance.state, {}).get("attachments", []): return {"ok": false, "code": "UnsupportedCityAttachmentState"}
		var choices: Dictionary = instance.get("attachments", {}).duplicate(true); choices[slot] = visible; instance.attachments = choices
		return _preview(next)
	return {"ok": false, "code": "UnknownCityInstance"}

## 确定时重验全部几何；只有成功候选进入历史，重复确定不会重复产生记录。
func confirm() -> Dictionary:
	if _pending.is_empty(): return {"ok": false, "code": "NoCityEditPreview"}
	var checked: Dictionary = Model.resolve(_document, _pending, _registry)
	if not checked.ok: _validation = checked; return checked
	if _pending == _layout: cancel(); return {"ok": true, "changed": false}
	_undo.append(_layout.duplicate(true)); if _undo.size() > HISTORY_LIMIT: _undo.pop_front()
	_layout = _pending.duplicate(true); _redo.clear(); cancel()
	return {"ok": true, "changed": true}

## 取消只丢弃候选，原草案与历史不变；可重复调用。
func cancel() -> void:
	_pending = {}; _validation = {}

## 撤销先取消悬而未决的预览，再回到一个已接受快照，不跨越新会话。
func undo() -> bool:
	cancel()
	if _undo.is_empty(): return false
	_redo.append(_layout.duplicate(true)); _layout = _undo.pop_back()
	return true

## 重做仅恢复原分支中的快照，新的确定操作会清空旧重做分支。
func redo() -> bool:
	cancel()
	if _redo.is_empty(): return false
	_undo.append(_layout.duplicate(true)); _layout = _redo.pop_back()
	return true

## 恢复初始布局也是可撤销的内存操作，不调用游戏读档。
func reset_preview() -> Dictionary:
	return _preview(_original.duplicate(true))

## 界面依据当前有效状态启用按钮，不缓存另一代次的许可。
func status() -> Dictionary:
	return {"pending": not _pending.is_empty(), "valid": _validation.get("ok", false), "code": _validation.get("code", ""),
		"undo": not _undo.is_empty(), "redo": not _redo.is_empty(), "history_size": _undo.size()}

## 候选副本用于画出拟建层与冲突提示；调用者不能直接改写会话候选。
func pending() -> Dictionary:
	return _pending.duplicate(true)

## 无论成功与失败都保留候选供明确预览，但确定入口始终重新检查。
func _preview(candidate: Dictionary) -> Dictionary:
	_pending = candidate.duplicate(true)
	_validation = Model.resolve(_document, _pending, _registry)
	return _validation.duplicate(true)
