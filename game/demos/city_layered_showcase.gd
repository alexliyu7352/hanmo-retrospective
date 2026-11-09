## 完整独立城市试玩入口：组织已有工作台和布局对照，保留本次确认草案，不接世界或存档。
extends Control
const Workshop = preload("res://demos/city_shared_views.gd")
const Comparison = preload("res://demos/city_layout_lab.gd")
var workshop: Control
var comparison: Control
var current: Control
var active_section: String = "workshop"
var _menu_button: Button
var _overlay: Control
var _overlay_panel: PanelContainer
var _overlay_title: Label
var _overlay_text: Label
var _section_buttons: Array = []
var _help: bool = false

## 默认进入可直接浏览的当前街区；编辑需用户明确开启。
func _ready() -> void:
	get_window().title = "汉末行记 · 独立城市试玩"
	set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	workshop = Workshop.new(); workshop.name = "DistrictWorkshop"; add_child(workshop)
	if not workshop._editor_ready: return
	current = workshop; theme = workshop.theme
	workshop._toggle_editing(); workshop._state_panel.hide()
	workshop._status.text = "点建筑看单楼，右上查看真实局部；要改布局、状态或布棚，请先点“编辑布局”。"
	_build_shell(); resized.connect(_responsive); _responsive()

## 入口按钮覆盖原品牌区，操作菜单为真正模态层，背景不能被点穿或处理快捷键。
func _build_shell() -> void:
	_menu_button = _button("试玩入口 ▾", "ShowcaseMenu", toggle_menu)
	_menu_button.position = Vector2(32, 25); _menu_button.custom_minimum_size = Vector2(146, 64)
	_menu_button.add_theme_font_size_override("font_size", 18); add_child(_menu_button)
	_overlay = Control.new(); _overlay.name = "ShowcaseOverlay"; add_child(_overlay)
	_overlay.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT); _overlay.mouse_filter = Control.MOUSE_FILTER_STOP
	var shade: ColorRect = ColorRect.new(); shade.color = Color("0b191966"); shade.mouse_filter = Control.MOUSE_FILTER_IGNORE
	_overlay.add_child(shade); shade.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	_overlay_panel = PanelContainer.new(); _overlay.add_child(_overlay_panel)
	_overlay_panel.add_theme_stylebox_override("panel", _paper())
	_overlay_panel.minimum_size_changed.connect(_responsive)
	var body: VBoxContainer = VBoxContainer.new(); body.add_theme_constant_override("separation", 10); _overlay_panel.add_child(body)
	_overlay_title = Label.new(); _overlay_title.add_theme_font_size_override("font_size", 26); body.add_child(_overlay_title)
	_overlay_text = Label.new(); _overlay_text.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	_overlay_text.add_theme_font_size_override("font_size", 18); body.add_child(_overlay_text)
	for label in [_overlay_title, _overlay_text]: label.add_theme_color_override("font_color", Color("382f24"))
	for item in [["当前街区工作台", "SelectWorkshop", "workshop"], ["两种布局对照", "SelectComparison", "comparison"]]:
		var button: Button = _button(item[0], item[1], show_section.bind(item[2])); body.add_child(button); _section_buttons.append(button)
	var help_button: Button = _button("说明与完成边界", "ShowcaseHelp", show_help); body.add_child(help_button); _section_buttons.append(help_button)
	body.add_child(_button("继续试玩", "CloseShowcaseOverlay", close_overlay))
	_overlay.hide()

## 菜单打开期间暂停当前子场景的输入处理，关闭后恢复原镜头和候选。
func toggle_menu() -> void:
	if _overlay.visible: close_overlay(); return
	_help = false
	_overlay_title.text = "独立城市试玩"
	_overlay_text.text = "切换样片会取消未确定预览；已确认草案在本次运行中保留。两布局对照不会覆盖工作台。"
	for button in _section_buttons: button.show()
	current.process_mode = Node.PROCESS_MODE_DISABLED; _overlay.show(); _responsive()

## 完成边界是用户可见说明，不把小市集称为整城或把本次内存称为存档。
func show_help() -> void:
	_help = true; current.process_mode = Node.PROCESS_MODE_DISABLED; _overlay.show()
	_overlay_title.text = "已完成与当前边界"
	_overlay_text.text = "使用：浏览时点建筑看单楼；右上切总览/真实局部。底栏“编辑布局”开启内存编辑，放下或选状态后仍须确定。\n\n已验证：独立地面、道路和建筑；两布局；可逆拖放与共享路点；粮铺六种显示状态；独立布棚；同数据跨尺度与返回。\n\n范围：当前小市集、四种建筑、单一朝向与日间。未完成整城、多街区完整资产、扩建合并布局及其他朝向。未接世界、经营建设或存档，退出后本次草案丢弃。"
	for button in _section_buttons: button.hide()
	_responsive()

## 关闭菜单不确定或取消候选；只有明确切样片才执行候选回滚。
func close_overlay() -> void:
	if _overlay == null: return
	_overlay.hide(); current.process_mode = Node.PROCESS_MODE_INHERIT

## 两个样片常驻但只有一个可见和处理输入，返回工作台保留已确认数据与视窗。
func show_section(section: String) -> void:
	if section not in ["workshop", "comparison"]: return
	if section == active_section: close_overlay(); return
	var cancelled: bool = current == workshop and workshop.session.status().pending
	if cancelled: workshop.cancel_edit()
	current.hide(); current.process_mode = Node.PROCESS_MODE_DISABLED
	if section == "comparison" and comparison == null:
		comparison = Comparison.new(); comparison.name = "LayoutComparison"; add_child(comparison); move_child(comparison, 0)
	current = workshop if section == "workshop" else comparison
	active_section = section; current.show(); close_overlay()
	current._status.text = ("未确定预览已取消；已确认工作台草案保留。 " if cancelled else "") + ("已回当前工作台，选择、镜头和已确认草案保留。" if current == workshop else "两布局对照是独立样片，不改工作台草案；左上返回工作台。")

## 模态面板按当前窗口宽度居中，文字与按钮不越过600像素窄窗。
func _responsive() -> void:
	if _overlay_panel == null: return
	_overlay_panel.custom_minimum_size.x = minf(640, size.x - 48)
	call_deferred("_place_overlay")

## 等容器完成文字换行后再定位，防止由旧高度计算出被裁切的说明页。
func _place_overlay() -> void:
	if _overlay_panel == null or not is_inside_tree(): return
	_overlay_panel.size = _overlay_panel.get_combined_minimum_size()
	_overlay_panel.position = Vector2((size.x - _overlay_panel.size.x) * 0.5, maxf(12, (size.y - _overlay_panel.size.y) * 0.5))

## F1打开入口；模态期间Esc只关闭菜单，不穿到被暂停的布局编辑器。
func _unhandled_key_input(event: InputEvent) -> void:
	if not event is InputEventKey or not event.pressed or event.echo: return
	if event.keycode == KEY_F1: toggle_menu(); get_viewport().set_input_as_handled()
	elif event.keycode == KEY_ESCAPE and _overlay.visible: close_overlay(); get_viewport().set_input_as_handled()

## 统一采用现有纸底深字与44像素命中，不增加新的视觉体系。
func _button(text: String, id: String, callback: Callable) -> Button:
	var button: Button = Button.new(); button.text = text; button.name = id; button.custom_minimum_size.y = 44
	for state in ["normal", "hover", "pressed", "focus"]: button.add_theme_stylebox_override(state, _paper())
	for color in ["font_color", "font_hover_color", "font_pressed_color", "font_focus_color"]: button.add_theme_color_override(color, Color("382f24"))
	button.pressed.connect(callback); return button

## 沿用已有纸色与旧铜边框，入口只负责组织而不生产新美术。
func _paper() -> StyleBoxFlat:
	var style: StyleBoxFlat = StyleBoxFlat.new(); style.bg_color = Color("eee2c6")
	style.border_color = Color("9b7b46"); style.set_border_width_all(1); style.set_corner_radius_all(3)
	style.content_margin_left = 16; style.content_margin_right = 16; style.content_margin_top = 10; style.content_margin_bottom = 10
	return style
