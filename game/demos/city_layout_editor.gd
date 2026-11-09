## 街段作者操作样板：真实拖放及道路层预览只产生内存草案，不提供世界建设入口。
extends "res://demos/city_layout_lab.gd"
const Session = preload("res://ui/city_layout_session.gd")
var session: RefCounted
var _edit_button: Button
var _confirm_edit: Button
var _cancel_edit: Button
var _undo_edit: Button
var _redo_edit: Button
var _road_edit: Button
var _remove_edit: Button
var _reset_button: Button
var _editor_ready: bool = false
const REASONS = {"CityPlotRequired": "没有可用地块", "CityPlotOccupied": "目标地块已有建筑", "CityBuildingOutsidePlot": "建筑占地超出地块", "CityBuildingFootprintOverlap": "建筑占地相交", "CityRoadCrossesBuilding": "道路穿过建筑占地", "CityEntryObstructed": "入口连接被另一栋建筑阻挡", "CityEntryTooFarFromRoad": "入口离所属道路太远", "CityAnchorOutsideCanvas": "建筑移出画布", "CityRoadOutsideCanvas": "道路移出画布"}

## 复用样板装载与注册表，只替换本片的独立作者布局文件。
func _document_path() -> String:
	return "res://data/city-layout/editable_street.json"

## 基础画面建立后开启新编辑会话，旧浏览控件不产生额外写入入口。
func _ready() -> void:
	super._ready()
	if canvas == null or not canvas.describe().get("ok", false): return
	session = Session.new()
	var result: Dictionary = session.start(document, layout, registry)
	if not result.ok: _fatal(str(result)); return
	_editor_ready = true
	canvas.editing = true; canvas.show_plots = true
	canvas.placement_dragged.connect(_dragged)
	canvas.selection_changed.connect(_selected_instance)
	_sync_buttons(); _responsive()
	_status.text = "拖动建筑到空地块，绿框表示可确定，红框说明冲突。放下后仍可取消。"
	_subtitle.text = "六个稳定地块 · 独立道路层 · 编辑只在内存中"

## 工作栏只开放本片的事务入口，原演示的固定移位和直接删除按钮不混入编辑历史。
func _build_interface() -> void:
	super._build_interface()
	for child in _toolbar.get_children():
		if child == _back: continue
		_toolbar.remove_child(child); child.queue_free()
	_edit_button = _button(_toolbar, "切换浏览", "EditMode", _toggle_editing)
	_road_edit = _button(_toolbar, "道路偏移", "PreviewRoad", preview_road_shift)
	_remove_edit = _button(_toolbar, "移除预览", "PreviewRemove", preview_removal)
	_confirm_edit = _button(_toolbar, "确定", "ConfirmEdit", confirm_edit)
	_cancel_edit = _button(_toolbar, "取消", "CancelEdit", cancel_edit)
	_undo_edit = _button(_toolbar, "撤销", "UndoEdit", undo_edit)
	_redo_edit = _button(_toolbar, "重做", "RedoEdit", redo_edit)
	_reset_button = _button(_toolbar, "恢复原布局", "ResetEdit", _reset_edit)

## 桌面与窄窗按工作栏实际列数保留按钮，所有操作仍有44像素逻辑高度。
func _responsive() -> void:
	super._responsive()
	var footer: Control = get_node_or_null("CityFooter")
	if footer == null: return
	footer.offset_top = -168 if size.x < 1000 else -116

## 拖动只更新候选纹理和冲突提示；画布与会话使用同一足点坐标。
func _dragged(id: String, position: Vector2, finished: bool) -> void:
	if not _editor_ready or not canvas.editing: return
	selected = id
	var result: Dictionary = session.preview_move(id, position)
	canvas.preview_instance(id, position, result.ok, result.get("access", []))
	_status.text = ("可以确定；取消会回到原位置。" if result.ok else "不能放置：" + REASONS.get(result.code, str(result.code))) if finished else ("拖动中 · 可放置" if result.ok else "拖动中 · " + REASONS.get(result.code, str(result.code)))
	_sync_buttons()

## 纸签只选择对象，编辑中不意外进入近景或提交尚未确定的候选。
func _selected_instance(id: String) -> void:
	if _editor_ready and id != selected and session.status().pending:
		session.cancel(); canvas.clear_preview(); canvas.configure(document, session.layout(), registry)
		_status.text = "切换对象已取消上一份未确定预览。"; _sync_buttons()
	selected = id

## 一个道路偏移产生真实图层预览，仍须确定；再次操作从已接受布局产生候选。
func preview_road_shift() -> void:
	if not _editor_ready or not active_place.is_empty() or not canvas.editing: return
	var result: Dictionary = session.preview_road("road.north", Vector2(0, -18))
	canvas.preview_roads(session.pending().get("roads", []), result.ok, result.get("access", []), "road.north")
	_status.text = "道路纹理已作候选偏移；确定或取消。" if result.ok else "道路候选冲突：" + REASONS.get(result.code, str(result.code))
	_sync_buttons()

## 删除在独立地块上预览为空地，地块与相邻道路均保留。
func preview_removal() -> void:
	if not _editor_ready or not active_place.is_empty() or not canvas.editing: return
	var result: Dictionary = session.preview_remove(selected)
	if result.ok:
		canvas.configure(document, session.pending(), registry)
		canvas.show_plots = true
	_status.text = "预览为空地；确定才移除建筑，取消会恢复。" if result.ok else "无法移除当前对象。"
	_sync_buttons()

## 确定只能接纳最后一份合法预览，坏候选保持原草案并解释首错。
func confirm_edit() -> void:
	if not _editor_ready or not canvas.editing or not active_place.is_empty(): return
	var result: Dictionary = session.confirm()
	if result.ok: _accept_session("已确定本次显示修改，可撤销。")
	else: _status.text = "未确定：" + REASONS.get(result.code, str(result.code))
	_sync_buttons()

## Esc与取消按钮共用同一回滚出口，恢复原图层、标签和入口。
func cancel_edit() -> void:
	if not _editor_ready or not canvas.editing or not active_place.is_empty(): return
	session.cancel(); _accept_session("已取消，恢复之前的草案。")

## 撤销一次已确定修改，不把尚在拖动的候选加入历史。
func undo_edit() -> void:
	if not _editor_ready or not canvas.editing or not active_place.is_empty(): return
	var restored: bool = session.undo()
	_accept_session("已撤销一次显示修改。" if restored else "已取消当前预览；没有更早的修改。")

## 重做仅恢复当前历史分支，新的确定会丢弃旧重做分支。
func redo_edit() -> void:
	if not _editor_ready or not canvas.editing or not active_place.is_empty(): return
	var restored: bool = session.redo()
	_accept_session("已重做一次显示修改。" if restored else "已取消当前预览；没有可重做的修改。")

## 恢复作者初始布局同样先显示预览，保留确定与取消选择。
func _reset_edit() -> void:
	if not _editor_ready or not canvas.editing or not active_place.is_empty(): return
	var result: Dictionary = session.reset_preview()
	if result.ok: canvas.clear_preview(); canvas.configure(document, session.pending(), registry)
	_status.text = "原布局预览；确定或取消。"; _sync_buttons()

## 切换浏览先明确取消未确定候选；浏览仍可进入同实例近景并返回。
func _toggle_editing() -> void:
	if not _editor_ready: return
	if not active_place.is_empty(): return_to_district()
	session.cancel(); canvas.clear_preview(); canvas.configure(document, session.layout(), registry)
	canvas.editing = not canvas.editing; canvas.show_plots = canvas.editing; canvas.queue_redraw()
	_edit_button.text = "切换浏览" if canvas.editing else "编辑布局"
	_status.text = "拖动建筑到空地块，放下后确定或取消。" if canvas.editing else "浏览当前已确定布局；点击建筑进入近景。"
	_sync_buttons()

## 回到原草案时保留浏览/编辑模式，不从原作者文件重新读取。
func _accept_session(message: String) -> void:
	canvas.clear_preview(); layout = session.layout()
	canvas.configure(document, layout, registry); canvas.select_instance(selected)
	_status.text = message; _sync_buttons()

## 按钮状态取自当前会话，不以画面颜色代替是否合法的判断。
func _sync_buttons() -> void:
	if not _editor_ready: return
	var state: Dictionary = session.status()
	var editing_allowed: bool = canvas.editing and active_place.is_empty()
	_confirm_edit.disabled = not editing_allowed or not state.pending or not state.valid
	_cancel_edit.disabled = not editing_allowed or not state.pending
	_undo_edit.disabled = not editing_allowed or not state.undo; _redo_edit.disabled = not editing_allowed or not state.redo
	_road_edit.disabled = not editing_allowed; _remove_edit.disabled = not editing_allowed; _reset_button.disabled = not editing_allowed

## 浏览近景时关闭编辑命令，避免画布回到街区但标题仍停在近景。
func enter_place(id: String) -> void:
	super.enter_place(id); _sync_buttons()

## 返回后恢复当前模式对应的操作，不重新装载原始布局。
func return_to_district() -> void:
	super.return_to_district(); _sync_buttons()

## Escape取消候选，Ctrl+Z/Y只在当前工作台操作历史；F8沿用真实Viewport截图出口。
func _unhandled_key_input(event: InputEvent) -> void:
	if event is InputEventKey and event.pressed and not event.echo:
		if event.keycode == KEY_ESCAPE:
			if not active_place.is_empty(): return_to_district()
			else: cancel_edit()
			get_viewport().set_input_as_handled(); return
		if _editor_ready and canvas.editing and active_place.is_empty():
			if event.ctrl_pressed and event.keycode == KEY_Z: undo_edit(); get_viewport().set_input_as_handled(); return
			if event.ctrl_pressed and event.keycode == KEY_Y: redo_edit(); get_viewport().set_input_as_handled(); return
	super._unhandled_key_input(event)

## 窗口关闭或离树后终结编辑入口，迟到按钮信号不会再次修改草案。
func _exit_tree() -> void:
	_editor_ready = false
	if session != null: session.cancel()
