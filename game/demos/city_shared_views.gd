## 同数据街区总览与局部镜头：只切换投影，保留全部邻楼、道路、地块及可逆显示会话。
extends "res://demos/city_building_parts_editor.gd"
var view_mode: String = "overview"
var _views_panel: PanelContainer
var _overview_button: Button
var _local_button: Button
var _return_camera: Dictionary = {}
var _return_mode: String = "overview"

## 保留原部件和布局功能，但初始收起编辑面板，让总览首先可阅读。
func _ready() -> void:
	super._ready()
	if not _editor_ready: return
	_state_panel.hide(); _update_view_ui(); _responsive()
	_status.text = "选择地块后可看真实局部；建筑、邻楼和道路仍来自同一草案。"

## 两个视图入口放在顶部空位，不再增加一排底栏命令。
func _build_interface() -> void:
	super._build_interface()
	_views_panel = PanelContainer.new(); _views_panel.name = "DistrictViews"; add_child(_views_panel)
	_views_panel.add_theme_stylebox_override("panel", _panel(Color("eee2c6f5")))
	var row: HBoxContainer = HBoxContainer.new(); row.add_theme_constant_override("separation", 8); _views_panel.add_child(row)
	_overview_button = _button(row, "街区总览", "OverviewView", show_overview)
	_local_button = _button(row, "地块局部", "LocalView", focus_selected)

## 窄窗下视图入口保留右侧独立空间，不挤占状态面板或覆盖底栏。
func _responsive() -> void:
	super._responsive()
	if _views_panel == null: return
	_views_panel.position = Vector2(maxf(350, size.x - 318), 116)

## 总览只重置镜头，不取消当前合法候选，也不改变选择和撤销历史。
func show_overview() -> void:
	if not _editor_ready or not active_place.is_empty(): return
	view_mode = "overview"; canvas.reset_camera(); _update_view_ui()
	_status.text = "已回街区总览；选中对象和当前草案保持不变。"

## 显式局部按钮围绕当前地块取景，选择别楼不会偷偷跳镜头。
func focus_selected() -> void:
	if not _editor_ready or not active_place.is_empty() or road_mode: return
	var row: Dictionary = canvas.find_instance(selected)
	if row.is_empty(): _status.text = "先选择一个场所或地块。"; return
	var center: Vector2 = Vector2(row.foot[0], row.foot[1]) + Vector2(0, -65)
	if canvas.set_camera(center, 1.8):
		view_mode = "local"; _update_view_ui()
		_status.text = "真实地块局部；邻楼和道路未移动，编辑仍写同一份草案。"

## 单楼展示继续保留为独立模式，进入前记录镜头，不能把孤立单楼称为真实近街景。
func enter_place(id: String) -> void:
	if not active_place.is_empty(): return
	_return_camera = canvas.camera_state(); _return_mode = view_mode; canvas.reset_camera()
	super.enter_place(id)
	if active_place.is_empty(): canvas.restore_camera(_return_camera)
	else:
		_title.text = _instance(id).name + " · 单楼展示"
		_subtitle.text = "独立展示模式 · " + id
	_update_view_ui()

## 单楼返回恢复原街区镜头和选中项，不从原文件重建另一份街区状态。
func return_to_district() -> void:
	var returning: bool = not active_place.is_empty()
	super.return_to_district()
	if returning:
		view_mode = _return_mode; canvas.restore_camera(_return_camera)
	_update_view_ui()

## 路点模式明确先回总览，保证所有声明节点在安全取景内；结束改路不自动跳回别处。
func toggle_road_mode() -> void:
	if not road_mode: show_overview()
	super.toggle_road_mode(); _update_view_ui()

## 草案确认或历史恢复不重置镜头，两个尺度仍指向同一份会话快照。
func _accept_session(message: String) -> void:
	super._accept_session(message); _update_view_ui()

## 模式切换只调整对应控件，局部与浏览没有额外世界移动命令。
func _toggle_editing() -> void:
	super._toggle_editing(); _update_view_ui()

## 标题明确区分总览、真实局部和独立单楼，禁用入口由当前模式决定。
func _update_view_ui() -> void:
	if _views_panel == null or not _editor_ready: return
	_views_panel.visible = active_place.is_empty()
	_overview_button.disabled = not active_place.is_empty() or view_mode == "overview"
	_local_button.disabled = not active_place.is_empty() or road_mode or selected.is_empty()
	if not active_place.is_empty(): return
	_title.text = "市井街段 · " + ("地块局部" if view_mode == "local" else "街区总览")
	_subtitle.text = "真实局部视窗 · 保留道路、地块与邻楼" if view_mode == "local" else "同一街区草案 · 总览与局部只切视窗"
