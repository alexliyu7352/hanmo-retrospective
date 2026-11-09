## 街段统一工作台的有限路点模式：复用状态/建筑编辑及同一撤销历史，不增加模拟规则。
extends "res://demos/city_building_state_editor.gd"
const Handles = preload("res://ui/city_road_node_handles.gd")
var road_mode: bool = false
var road_handles: Control
const ROAD_REASONS = {"CityRoadSegmentTooShort": "相邻路点太近", "CityRoadSegmentOverlap": "路段折返重叠", "CityUnboundRoadCrossing": "未声明的道路穿越", "CityRoadJunctionSplit": "共用连接点不一致", "CityEntryFacingAway": "门前连接无法接入道路", "CityRoadOutsideCanvas": "路点超出画面", "InvalidCityRoadPoint": "路点重合或坐标无效"}

## 在现有画布与UI之间放操作叠层，复用道路按钮而不继续挤占底栏。
func _ready() -> void:
	super._ready()
	if not _editor_ready: return
	road_handles = Handles.new(); road_handles.name = "RoadNodeHandles"; road_handles.canvas = canvas
	add_child(road_handles); move_child(road_handles, 1); road_handles.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	road_handles.node_dragged.connect(_road_node_dragged); road_handles.hide()
	_road_edit.pressed.disconnect(preview_road_shift); _road_edit.pressed.connect(toggle_road_mode)
	_sync_buttons()

## 路点与建筑操作互斥；切模式先取消当前候选，原草案不被菜单悄悄提交。
func toggle_road_mode() -> void:
	if not _editor_ready or not canvas.editing or not active_place.is_empty(): return
	session.cancel(); canvas.clear_preview(); road_mode = not road_mode
	canvas.configure(document, session.layout(), registry); _state_panel.visible = not road_mode
	_refresh_road_handles(); _sync_buttons()
	_status.text = "拖动圆点改路，菱形共用连接随之移动；放下后确定或取消。" if road_mode else "已退出改路；可继续选择建筑与显示状态。"

## 合法候选同时绘制所有相连路段和门前连接；非法候选只加拒绝线，原路面保留。
func _road_node_dragged(id: String, position: Vector2, finished: bool) -> void:
	if not _editor_ready or not road_mode or not canvas.editing: return
	var result: Dictionary = session.preview_road_node(id, position)
	canvas.preview_roads(session.pending().get("roads", []), result.ok, result.get("access", []))
	road_handles.configure(session.pending().get("roads", []), result.ok)
	_status.text = ("路点候选可确定；取消恢复原路面和入口。" if finished else "拖动路点中；原草案尚未确定。") if result.ok else "不能确定：" + ROAD_REASONS.get(result.get("code", ""), REASONS.get(result.get("code", ""), str(result.get("code", ""))))
	_sync_buttons()

## 接受或撤销后从同一草案刷新手柄，不遗留前一次红框或共用点。
func _accept_session(message: String) -> void:
	super._accept_session(message); _refresh_road_handles()

## 统一模式状态同时约束按钮与手柄，近景无隐藏可写道路入口。
func _sync_buttons() -> void:
	super._sync_buttons()
	if not _editor_ready: return
	_road_edit.text = "结束改路" if road_mode else "道路节点"
	_remove_edit.disabled = _remove_edit.disabled or road_mode
	if road_mode: _remove_edit.hide()
	var toggle: Button = find_child("ToggleStatePanel", true, false)
	if toggle != null:
		toggle.disabled = road_mode
		if road_mode: toggle.hide()
	for button in _toolbar.get_children():
		if button is Button: button.custom_minimum_size.x = 72 if road_mode else (110 if button == toggle else 126)
	_responsive()

## 改路时整幅画布等比放入上下栏之间；不移动实际节点，边缘与下方手柄也可完整命中。
func _responsive() -> void:
	super._responsive()
	if canvas == null: return
	if road_mode:
		canvas.offset_left = 32; canvas.offset_right = -32
		canvas.offset_top = get_node("CityHeader").get_global_rect().end.y + 26
		canvas.offset_bottom = get_node("CityFooter").offset_top - 26
	else:
		canvas.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	if road_handles != null: road_handles.queue_redraw()

## 编辑结束立即清除当前拖动，恢复浏览时没有道路操作层覆盖点击。
func _toggle_editing() -> void:
	road_mode = false
	if road_handles != null: road_handles.cancel_drag(); road_handles.hide()
	super._toggle_editing()

## 候选同步不会切换建筑身份，手柄仅在显式路点模式里显示。
func _refresh_road_handles() -> void:
	if road_handles == null: return
	road_handles.cancel_drag(); road_handles.configure(session.layout().get("roads", [])); road_handles.visible = road_mode
