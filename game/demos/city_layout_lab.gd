## 独立城市分层样板：只维护可丢弃显示草案，不实例化Kernel、不访问存档、不提交经营命令。
extends Control
const Canvas = preload("res://ui/city_layout_canvas.gd")
const Model = preload("res://ui/city_layout_model.gd")
const Registry = preload("res://ui/presentation_registry.gd")
var document: Dictionary = {}
var registry: RefCounted
var canvas: Control
var layout: Dictionary = {}
var layout_index: int = 0
var active_place: String = ""
var selected: String = "market.grain"
var _title: Label
var _subtitle: Label
var _status: Label
var _toolbar: HFlowContainer
var _tools: PanelContainer
var _choices: OptionButton
var _back: Button

## F8保存当前真实Viewport，文件只落在启动器指定的证据目录；不修改布局或游戏存档。
func _unhandled_key_input(event: InputEvent) -> void:
	if event is InputEventKey and event.pressed and not event.echo and event.keycode == KEY_F8:
		var folder: String = OS.get_environment("HANMO_CITY_OUTPUT")
		if folder.is_empty() or DisplayServer.get_name() == "headless": return
		await RenderingServer.frame_post_draw
		var image: Image = get_viewport().get_texture().get_image()
		var path: String = folder.path_join("city-" + str(Time.get_unix_time_from_system()).replace(".", "-") + ".png")
		var result: Error = image.save_png(path)
		print("CITY_VIEWPORT_CAPTURE=", path, " RESULT=", result)

## 独立入口可从原工程直接运行，单独加载同一清单中当前样板引用的资源。
func _ready() -> void:
	get_window().content_scale_mode = Window.CONTENT_SCALE_MODE_DISABLED
	get_window().content_scale_size = Vector2i.ZERO
	get_window().min_size = Vector2i(760, 600)
	set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	var font: SystemFont = SystemFont.new()
	font.font_names = PackedStringArray(["Noto Serif CJK SC", "Noto Sans CJK SC"])
	theme = Theme.new(); theme.default_font = font; theme.default_font_size = 20
	var parsed: Variant = JSON.parse_string(FileAccess.get_file_as_string(_document_path()))
	if not parsed is Dictionary: _fatal("布局文件无法读取"); return
	document = parsed
	var loaded: Dictionary = _load_registry()
	if not loaded.ok: _fatal("资源未通过验证：" + str(loaded)); return
	canvas = Canvas.new(); canvas.name = "CityLayoutCanvas"; add_child(canvas)
	canvas.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	canvas.place_requested.connect(enter_place)
	_build_interface()
	resized.connect(_responsive)
	_responsive()
	choose_layout(0)

## 独立街段编辑器可选择自己的作者布局，仍复用同一注册表和绘制模块。
func _document_path() -> String:
	return "res://data/city-layout/market.json"

## 资源路径仍以presentation.json为唯一来源；为独立运行仅过滤无关旧资产，不改源清单。
func _load_registry() -> Dictionary:
	var source: Variant = JSON.parse_string(FileAccess.get_file_as_string("res://data/presentation.json"))
	if not source is Dictionary: return {"ok": false, "code": "InvalidPresentationManifest"}
	var candidate: Dictionary = {"version": 2, "default_background": source.default_background, "default_portrait": source.default_portrait,
		"backgrounds": {}, "portraits": {}, "components": {}, "character_profiles": {}, "scene_templates": {}}
	var requested: Array = Model.required_backgrounds(document)
	requested.append(source.default_background)
	for id in requested:
		if not source.backgrounds.has(id): return {"ok": false, "code": "MissingCityCatalogEntry", "id": id}
		candidate.backgrounds[id] = source.backgrounds[id].duplicate(true)
	candidate.portraits[source.default_portrait] = source.portraits[source.default_portrait].duplicate(true)
	registry = Registry.new()
	return registry.configure(candidate)

## 顶栏及底栏保持少量文字，建筑标签直接访问；检验控件默认收起以便观察完整画面。
func _build_interface() -> void:
	var top: PanelContainer = PanelContainer.new(); top.name = "CityHeader"
	add_child(top); top.set_anchors_and_offsets_preset(Control.PRESET_TOP_WIDE)
	top.offset_top = 16; top.offset_left = 24; top.offset_right = -24; top.mouse_filter = Control.MOUSE_FILTER_IGNORE
	top.add_theme_stylebox_override("panel", _panel(Color("efe3c7ed")))
	var columns: HBoxContainer = HBoxContainer.new(); top.add_child(columns)
	var identity: Label = _label("汉末行记\n城市分层样板", 18); columns.add_child(identity)
	identity.add_theme_color_override("font_color", Color("382f24"))
	var center: VBoxContainer = VBoxContainer.new(); center.size_flags_horizontal = Control.SIZE_EXPAND_FILL; columns.add_child(center)
	_title = _label("", 30); _title.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER; center.add_child(_title)
	_subtitle = _label("", 16); _subtitle.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER; center.add_child(_subtitle)
	var badge: Label = _label("独立显示\n未接世界与存档", 15); badge.horizontal_alignment = HORIZONTAL_ALIGNMENT_RIGHT; columns.add_child(badge)
	var footer: PanelContainer = PanelContainer.new(); footer.name = "CityFooter"; add_child(footer)
	footer.set_anchors_and_offsets_preset(Control.PRESET_BOTTOM_WIDE)
	footer.offset_left = 20; footer.offset_right = -20; footer.offset_top = -116; footer.offset_bottom = -14
	footer.add_theme_stylebox_override("panel", _panel(Color("efe3c7ed")))
	var stack: VBoxContainer = VBoxContainer.new(); footer.add_child(stack)
	_toolbar = HFlowContainer.new(); _toolbar.add_theme_constant_override("h_separation", 8); stack.add_child(_toolbar)
	_button(_toolbar, "长街市集", "LayoutLane", choose_layout.bind(0))
	_button(_toolbar, "围合市庭", "LayoutSquare", choose_layout.bind(1))
	_button(_toolbar, "表现检验", "ToggleTools", _toggle_tools)
	_back = _button(_toolbar, "返回街区", "ReturnToDistrict", return_to_district)
	_back.visible = false
	_status = _label("", 15); _status.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART; stack.add_child(_status)
	_tools = PanelContainer.new(); _tools.name = "VisualTools"; add_child(_tools)
	_tools.set_anchors_and_offsets_preset(Control.PRESET_BOTTOM_WIDE)
	_tools.offset_left = 20; _tools.offset_right = -20; _tools.offset_top = -275; _tools.offset_bottom = -125
	_tools.add_theme_stylebox_override("panel", _panel(Color("efe3c7f5"))); _tools.hide()
	var tools_stack: VBoxContainer = VBoxContainer.new(); _tools.add_child(tools_stack)
	tools_stack.add_child(_label("表现检验 · 只改本次显示，退出后丢弃", 18))
	var choices: HBoxContainer = HBoxContainer.new(); tools_stack.add_child(choices)
	choices.add_child(_label("对象", 18)); _choices = OptionButton.new(); _choices.custom_minimum_size = Vector2(200, 44); choices.add_child(_choices)
	_choices.item_selected.connect(_choose_instance)
	var actions: HFlowContainer = HFlowContainer.new(); actions.add_theme_constant_override("h_separation", 8); tools_stack.add_child(actions)
	_button(actions, "切换施工", "ChangeState", toggle_construction)
	_button(actions, "替换建筑", "ReplaceBuilding", replace_building)
	_button(actions, "建筑移位", "MoveBuilding", move_building)
	_button(actions, "移除建筑", "RemoveBuilding", remove_building)
	_button(actions, "只看地面", "GroundOnly", show_ground)
	_button(actions, "恢复布局", "RestoreLayout", restore_layout)

## 布局切换从作者原样复制，两个布局共享资源；不延续另一布局的临时删除或换型。
func choose_layout(index: int) -> void:
	if index < 0 or index >= document.layouts.size(): return
	layout_index = index; layout = document.layouts[index].duplicate(true)
	active_place = ""; _back.hide(); _tools.hide()
	_refresh_choices(); _present()
	_status.text = "点击建筑或纸签进入场所近景。所有建筑、树木与地面均为独立图层。"

## 进入稳定对象的独立近景，原街区草案和选中项完整保留；不假装执行世界移动。
func enter_place(id: String) -> void:
	if not active_place.is_empty(): return
	var instance: Dictionary = _instance(id)
	if instance.is_empty() or not instance.interactive: return
	selected = id; active_place = id; _tools.hide()
	var close: Dictionary = layout.duplicate(true); close.id += ".close"; close.name = instance.name + " · 场所近景"
	var focused: Dictionary = instance.duplicate(true); focused.position = [836, 810]; focused.width = 1200
	close.instances = [focused]
	close.erase("roads"); close.erase("plots")
	canvas.show_labels = false
	var result: Dictionary = canvas.configure(document, close, registry)
	if not result.ok: active_place = ""; _status.text = str(result.code); return
	_title.text = close.name; _subtitle.text = "同一建筑实例 · " + id; _back.show()
	_status.text = "独立场所外观展示。返回后保留原街区、建筑状态与所选入口。"

## 返回恢复原布局，不重新随机排列、不重置换型与施工显示，不访问存档。
func return_to_district() -> void:
	if active_place.is_empty(): return
	active_place = ""; _back.hide(); _present()
	_status.text = "已回到原街区与入口；本次显示修改保持不变。"

## 只对具有已绘制施工变体的建筑切换，缺素材时说明原因而不悄悄换建筑。
func toggle_construction() -> void:
	var instance: Dictionary = _instance(selected)
	if instance.is_empty(): return
	if not document.archetypes[instance.archetype].states.has("construction"):
		_status.text = "此建筑尚无施工变体；可选择粮铺或货栈检查。"; return
	var next: Dictionary = layout.duplicate(true)
	for row in next.instances:
		if row.id == selected: row.state = "complete" if row.state == "construction" else "construction"
	_apply(next, "同一地块切换施工外观；没有推进工程或改变钱粮。")

## 整栋替换保留稳定场所ID、入口位置和名称；近景与标签仍指向同一对象。
func replace_building() -> void:
	var instance: Dictionary = _instance(selected)
	if instance.is_empty(): return
	var next: Dictionary = layout.duplicate(true)
	var types: Array = ["tavern", "grainshop", "clothshop", "inn"]
	for row in next.instances:
		if row.id == selected: row.archetype = types[(types.find(row.archetype) + 1) % types.size()]; row.state = "complete"
	_apply(next, "仅替换建筑形体，场所ID和名称保留。")

## 移位在同一画布中进行；足点变化后遮挡和入口同步重算，不移动源图片的五官或结构。
func move_building() -> void:
	var instance: Dictionary = _instance(selected)
	if instance.is_empty(): return
	for position in [[885, 610], [770, 660], [1000, 650], [540, 500], [1100, 340]]:
		if instance.position == position: continue
		var next: Dictionary = layout.duplicate(true)
		for row in next.instances:
			if row.id == selected: row.position = position.duplicate()
		if Model.resolve(document, next, registry).ok:
			_apply(next, "已移到空置显示位置；地面占地不相交，遮挡与入口同步更新。")
			return
	_status.text = "当前检验空位都与已有建筑占地相交，已保留原布局。"

## 删除仅移除独立图层，完整底图始终存在；恢复布局可回到作者初始状态。
func remove_building() -> void:
	var next: Dictionary = layout.duplicate(true)
	next.instances = next.instances.filter(func(row: Dictionary) -> bool: return row.id != selected)
	_apply(next, "已移除独立建筑，原处露出完整地面。")

## 同时隐藏所有独立物件以检查底图，没有把建筑空洞补成另一张图。
func show_ground() -> void:
	var next: Dictionary = layout.duplicate(true); next.instances = []
	_apply(next, "完整地面：建筑与树木全部移除，使用的仍是同一张底图。")

## 恢复当前作者布局只清理本次可丢弃草案，不调用游戏的读档功能。
func restore_layout() -> void:
	choose_layout(layout_index)
	_status.text = "已恢复此布局的原始图层与位置。"

## 候选先经真实画布验证，失败不替换已接受草案。
func _apply(candidate: Dictionary, message: String) -> void:
	if not active_place.is_empty(): return
	var result: Dictionary = canvas.configure(document, candidate, registry)
	if not result.ok: _status.text = "保留原布局：" + str(result.code); return
	layout = candidate.duplicate(true)
	_refresh_choices(); canvas.select_instance(selected); _status.text = message

## 统一呈现入口只负责图层显示，错误明确留在当前样板中。
func _present() -> void:
	canvas.show_labels = true
	var result: Dictionary = canvas.configure(document, layout, registry)
	if not result.ok: _status.text = "布局不可显示：" + str(result.code); return
	canvas.select_instance(selected)
	_title.text = layout.name; _subtitle.text = "独立地面 · 整栋建筑 · 遮挡与入口同源"

## 检验对象来自当前实例清单，不用资源列表冒充真实场所。
func _refresh_choices() -> void:
	_choices.clear()
	for instance in layout.instances:
		if not instance.interactive: continue
		_choices.add_item(instance.name); _choices.set_item_metadata(_choices.item_count - 1, instance.id)
		if instance.id == selected: _choices.select(_choices.item_count - 1)
	if _instance(selected).is_empty(): selected = str(_choices.get_item_metadata(0)) if _choices.item_count > 0 else ""

## 仅改变被检验对象，不自动进入近景或移除任何其他建筑。
func _choose_instance(index: int) -> void:
	selected = str(_choices.get_item_metadata(index)); canvas.select_instance(selected)

## 检验面板只在街区开放，近景不会误改被遮住的街区草案。
func _toggle_tools() -> void:
	if active_place.is_empty(): _tools.visible = not _tools.visible

## 窄窗按实际按钮行数抬高检验面板，保留全部操作；不依赖桌面固定分辨率。
func _responsive() -> void:
	if _tools == null: return
	var columns: int = maxi(1, int((size.x - 72) / 146.0))
	var rows: int = ceili(6.0 / mini(6, columns))
	_tools.offset_top = -125 - (105 + rows * 54)

## 返回实例副本，避免界面调用者持有可改写的显示源。
func _instance(id: String) -> Dictionary:
	for instance in layout.get("instances", []):
		if instance.id == id: return instance.duplicate(true)
	return {}

## 简洁纸色面板与旧铜细边服务于文字阅读，不把大面板盖在整个场景上。
func _panel(color: Color) -> StyleBoxFlat:
	var style: StyleBoxFlat = StyleBoxFlat.new(); style.bg_color = color
	style.border_color = Color("9b7b46"); style.set_border_width_all(1); style.set_corner_radius_all(3)
	style.content_margin_left = 16; style.content_margin_right = 16; style.content_margin_top = 10; style.content_margin_bottom = 10
	return style

## 统一44像素最小命中区，标签长短由容器处理而不缩小字号。
func _button(parent: Node, text: String, id: String, callback: Callable) -> Button:
	var button: Button = Button.new(); button.text = text; button.name = id
	button.custom_minimum_size = Vector2(126, 44); parent.add_child(button); button.pressed.connect(callback)
	button.add_theme_stylebox_override("normal", _panel(Color("eee0be")))
	button.add_theme_stylebox_override("hover", _panel(Color("d4c18d")))
	button.add_theme_stylebox_override("pressed", _panel(Color("bfd0c1")))
	button.add_theme_color_override("font_color", Color("382f24")); button.add_theme_color_override("font_hover_color", Color("382f24"))
	return button

## 标题与说明采用同一中文字体，不把文字烧进背景或建筑资源。
func _label(text: String, font_size: int) -> Label:
	var label: Label = Label.new(); label.text = text
	label.add_theme_font_size_override("font_size", font_size); label.add_theme_color_override("font_color", Color("382f24"))
	return label

## 启动错误保持可见，不用空白画面或旧默认背景假装成功。
func _fatal(message: String) -> void:
	var label: Label = _label(message, 22); add_child(label)
	label.position = Vector2(32, 32); printerr(message)
