## 门面部件工作台：一栋粮铺的净楼体与布棚可逆组合，状态未适配时明确隐藏并禁选。
extends "res://demos/city_road_node_editor.gd"
var _part_tabs: TabBar
var _part_options: VBoxContainer
var _state_only: Array = []
var _part_view: bool = true

## 在同一街段和状态编辑器中开启门面页，不新造建筑身份或经营入口。
func _ready() -> void:
	super._ready()
	if not _editor_ready: return
	_part_tabs.current_tab = 1; _refresh_state_controls()
	find_child("ToggleStatePanel", true, false).text = "状态/部件"
	_status.text = "布棚与支杆是独立透明层；关闭可查看完整后方楼体。"

## 页签共享原面板空间，窄窗不因增加一个部件槽位继续向底栏堆叠。
func _build_interface() -> void:
	super._build_interface()
	var body: VBoxContainer = _state_panel.get_child(0)
	for index in range(1, 5): _state_only.append(body.get_child(index))
	_part_tabs = TabBar.new(); _part_tabs.add_tab("状态/外观"); _part_tabs.add_tab("门面部件")
	body.add_child(_part_tabs); body.move_child(_part_tabs, 1); _part_tabs.tab_changed.connect(_change_part_tab)
	_part_options = VBoxContainer.new(); body.add_child(_part_options); body.move_child(_part_options, body.get_child_count() - 2)

## 切页只改变控件可见性，不提交或取消当前候选。
func _change_part_tab(index: int) -> void:
	_part_view = index == 1; _refresh_state_controls()

## 选项来自同一建筑描述；保存的选择与当前是否可画分开，破损状态不会被完整布棚覆盖。
func _refresh_state_controls() -> void:
	super._refresh_state_controls()
	if _part_options == null: return
	for control in _state_only: control.visible = not _part_view
	_part_options.visible = _part_view
	for child in _part_options.get_children(): _part_options.remove_child(child); child.queue_free()
	var row: Dictionary = canvas.find_instance(selected) if canvas != null else {}
	for option in row.get("attachment_options", []):
		var check: CheckButton = CheckButton.new(); check.name = "Part_" + option.id
		check.text = option.name + ("（未适配）" if not option.supported else "")
		for color_name in ["font_color", "font_hover_color", "font_pressed_color", "font_focus_color"]: check.add_theme_color_override(color_name, Color("382f24"))
		check.add_theme_color_override("font_disabled_color", Color("756851"))
		check.custom_minimum_size.y = 44; check.set_pressed_no_signal(option.visible)
		check.disabled = not option.supported or not canvas.editing or not active_place.is_empty() or road_mode
		check.toggled.connect(_preview_part.bind(option.id)); _part_options.add_child(check)
	if _part_view:
		_reuse_note.text = "独立布棚仅适配完工；其他状态沿用原状态图，部件禁选。选择后仍需确定。" if not row.get("attachment_options", []).is_empty() else "这个建筑还没有制作独立门面部件。"
	elif row.get("state_note", "").is_empty():
		_reuse_note.text = "显示草案；选择后确定或取消。"

## 布棚开关进入原候选事务，取消和撤销恢复同一楼体而非换回烘焙布棚图。
func _preview_part(visible: bool, slot: String) -> void:
	if not _editor_ready or not canvas.editing or not active_place.is_empty() or road_mode: return
	var result: Dictionary = session.preview_attachment(selected, slot, visible)
	_show_state_candidate(result, "门面部件候选")
