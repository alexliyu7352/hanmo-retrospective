## 独立人物工坊的真实 Godot 操作界面；所有按钮只修改本次会话，不接主游戏。
extends Control
const Session = preload("res://src/fitting_session.gd")
const Fittings = preload("res://src/fitting_catalog.gd")
const Renderer = preload("res://src/portrait_renderer.gd")
const Canvas = preload("res://src/portrait_canvas.gd")
const Samples = preload("res://src/representative_catalog.gd")
var session := Session.new()
var renderer := Renderer.new()
var _renderers: Array[RefCounted] = []
var _scope_label: Label
var catalog: Dictionary = {}
var portrait: Image
var preview: Control
var avatars: Array[Control] = []
var status: Label
var summary: Label
var _buttons: Dictionary = {}
var _undo: Button
var _redo: Button
var _mode: int = 0
var _random_counter: int = 20261006
var _pending: bool = false
var _sample_catalog: Dictionary = {}
var _sample_overlay: Control
var _guide_overlay: Control
var _return_focus: Control

## 先校验目录与资源再显示交互；失败保留明确错误，不展示拼装不全的人物。
func _ready() -> void:
	var registry: Dictionary = Fittings.read_registry()
	catalog = registry.get("catalog", {})
	var initialized: Dictionary = session.configure(catalog) if registry.ok else registry
	if initialized.ok:
		for fitting in catalog.fittings:
			var candidate := Renderer.new()
			initialized = candidate.configure(fitting.definition)
			if not initialized.ok:
				break
			_renderers.append(candidate)
	if initialized.ok:
		_sample_catalog = Samples.read(catalog)
	_build_ui()
	if not initialized.ok:
		status.text = initialized.reason
		return
	session.changed.connect(_refresh)
	_refresh()

## 以真实容器组织半身、实际尺寸头像和分组选件，窄屏保留滚动操作。
func _build_ui() -> void:
	var theme := Theme.new()
	var font := SystemFont.new()
	font.font_names = PackedStringArray(["Noto Sans CJK SC", "Noto Sans CJK TC"])
	theme.default_font = font
	theme.default_font_size = 18
	theme.set_color("font_color", "Label", Color("e8e0d0"))
	theme.set_color("font_color", "Button", Color("e8e0d0"))
	theme.set_color("font_disabled_color", "Button", Color("8c918c"))
	for kind in ["normal", "hover", "pressed", "disabled", "focus"]:
		var style := StyleBoxFlat.new()
		style.bg_color = Color("243537") if kind == "normal" else Color("385455")
		if kind == "pressed":
			style.bg_color = Color("72513c")
		if kind == "disabled":
			style.bg_color = Color("1d292a")
		style.border_color = Color("9d825c") if kind == "pressed" else Color("445958")
		style.set_border_width_all(1)
		style.set_corner_radius_all(5)
		style.content_margin_left = 14
		style.content_margin_right = 14
		style.content_margin_top = 10
		style.content_margin_bottom = 10
		theme.set_stylebox(kind, "Button", style)
	self.theme = theme
	var background := ColorRect.new()
	background.color = Color("142323")
	background.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	add_child(background)
	var margin := MarginContainer.new()
	margin.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	for edge in ["left", "right", "top", "bottom"]:
		margin.add_theme_constant_override("margin_" + edge, 22)
	add_child(margin)
	var root := VBoxContainer.new()
	root.add_theme_constant_override("separation", 14)
	margin.add_child(root)
	var heading := HBoxContainer.new()
	root.add_child(heading)
	var title := Label.new()
	title.text = "人物工坊"
	title.add_theme_font_size_override("font_size", 32)
	heading.add_child(title)
	var subtitle := Label.new()
	subtitle.text = "  捏脸 · 衣冠 · 同人预览"
	subtitle.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	heading.add_child(subtitle)
	var badge := Label.new()
	_scope_label = badge
	badge.text = catalog.get("scope_label", "资源未就绪")
	badge.add_theme_font_size_override("font_size", 16)
	heading.add_child(badge)
	var sample_entry := _button(heading, "代表样本", "OpenSamples", _open_samples)
	sample_entry.disabled = not _sample_catalog.get("ok", false)
	sample_entry.tooltip_text = _sample_catalog.get("reason", "显式载入整组代表，可撤销；锁仅控制随机。")
	_button(heading, "操作说明", "OpenGuide", _open_guide)
	var body := HBoxContainer.new()
	body.size_flags_vertical = Control.SIZE_EXPAND_FILL
	body.add_theme_constant_override("separation", 24)
	root.add_child(body)
	var left := VBoxContainer.new()
	left.custom_minimum_size.x = 580
	left.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	body.add_child(left)
	var visual := HBoxContainer.new()
	visual.size_flags_vertical = Control.SIZE_EXPAND_FILL
	visual.add_theme_constant_override("separation", 12)
	left.add_child(visual)
	preview = Canvas.new()
	preview.name = "MainPortrait"
	preview.custom_minimum_size = Vector2(400, 600)
	preview.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	preview.size_flags_vertical = Control.SIZE_EXPAND_FILL
	visual.add_child(preview)
	var side := VBoxContainer.new()
	side.custom_minimum_size.x = 110
	visual.add_child(side)
	for dimension in [88, 96]:
		var label := Label.new()
		label.text = str(dimension) + " px 头像"
		label.add_theme_font_size_override("font_size", 14)
		side.add_child(label)
		var thumb := Canvas.new()
		thumb.name = "Avatar" + str(dimension)
		thumb.custom_minimum_size = Vector2(dimension, dimension)
		thumb.size_flags_horizontal = Control.SIZE_SHRINK_BEGIN
		side.add_child(thumb)
		avatars.append(thumb)
	var note := Label.new()
	note.text = "同一半身\n直接裁切\n\n头像不拉伸\n部件不拖拽"
	note.add_theme_font_size_override("font_size", 14)
	side.add_child(note)
	var view_row := HBoxContainer.new()
	left.add_child(view_row)
	_button(view_row, "半身", "ViewHalf", _set_mode.bind(0))
	_button(view_row, "头像", "ViewAvatar", _set_mode.bind(1))
	_button(view_row, "浅底", "BackdropLight", _set_backdrop.bind(0))
	_button(view_row, "深底", "BackdropDark", _set_backdrop.bind(1))
	_button(view_row, "棋盘", "BackdropGrid", _set_backdrop.bind(2))
	summary = Label.new()
	summary.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	summary.custom_minimum_size.y = 48
	summary.add_theme_font_size_override("font_size", 16)
	left.add_child(summary)
	var right := VBoxContainer.new()
	right.custom_minimum_size.x = 545
	right.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	body.add_child(right)
	var scroll := ScrollContainer.new()
	scroll.name = "ChoiceScroll"
	scroll.follow_focus = true
	scroll.size_flags_vertical = Control.SIZE_EXPAND_FILL
	scroll.horizontal_scroll_mode = ScrollContainer.SCROLL_MODE_DISABLED
	right.add_child(scroll)
	var options := VBoxContainer.new()
	options.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	options.add_theme_constant_override("separation", 12)
	scroll.add_child(options)
	for group in catalog.get("groups", []):
		var section := HBoxContainer.new()
		options.add_child(section)
		var label := Label.new()
		label.text = group.label
		label.add_theme_font_size_override("font_size", 21)
		label.size_flags_horizontal = Control.SIZE_EXPAND_FILL
		section.add_child(label)
		_button(section, "重置本组", "Reset_" + group.id, _reset_group.bind(group.id))
		for field in group.fields:
			_make_field(options, field)
		var separator := HSeparator.new()
		options.add_child(separator)
	var hint := Label.new()
	hint.text = "锁定会保留这项随机结果。灰色选项可悬停查看适配限制。"
	hint.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	hint.add_theme_font_size_override("font_size", 15)
	right.add_child(hint)
	status = Label.new()
	status.name = "Status"
	status.text = "正在准备人物部件…"
	status.custom_minimum_size.y = 52
	status.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	right.add_child(status)
	var actions := HFlowContainer.new()
	actions.add_theme_constant_override("h_separation", 8)
	right.add_child(actions)
	_undo = _button(actions, "撤销", "Undo", _undo_pressed)
	_redo = _button(actions, "重做", "Redo", _redo_pressed)
	_button(actions, "随机未锁定项", "Randomize", _random_pressed)
	_button(actions, "恢复进入时", "RestoreEntry", _restore_pressed)
	var footer := Label.new()
	footer.text = "独立试装 · 当前选择只在本次工坊中变化 · 完整头颈与衣领适配"
	footer.add_theme_font_size_override("font_size", 14)
	root.add_child(footer)
	_build_entry_overlays()

## 每个候选是真实选件信号，锁定框只控制随机，不阻拦手动修改。
func _make_field(parent: Control, field: String) -> void:
	var row := HBoxContainer.new()
	row.add_theme_constant_override("separation", 7)
	parent.add_child(row)
	var label := Label.new()
	label.text = catalog.fields[field].label
	label.custom_minimum_size.x = 65
	row.add_child(label)
	var choices := HFlowContainer.new()
	choices.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	row.add_child(choices)
	_buttons[field] = []
	var button_group := ButtonGroup.new()
	button_group.allow_unpress = false
	for index in catalog.fields[field].options.size():
		var button := _button(choices, catalog.fields[field].options[index], "Choice_" + field + "_" + str(index), _choose.bind(field, index))
		button.toggle_mode = true
		button.button_group = button_group
		button.size_flags_horizontal = Control.SIZE_EXPAND_FILL
		_buttons[field].append(button)
	var lock := CheckBox.new()
	lock.name = "Lock_" + field
	lock.text = "锁"
	lock.tooltip_text = "随机时保留" + catalog.fields[field].label
	lock.toggled.connect(_lock_changed.bind(field))
	row.add_child(lock)

## 统一创建稳定命名按钮，便于键鼠使用与真控件回归。
func _button(parent: Control, text: String, id: String, action: Callable) -> Button:
	var button := Button.new()
	button.name = id
	button.text = text
	button.pressed.connect(action)
	parent.add_child(button)
	return button

## 收到合法状态后从各自选值重新合成，换脸型不会重置其他五官。
func _refresh() -> void:
	if _pending:
		return
	_pending = true
	var state: Dictionary = session.state()
	renderer = _renderers[state.rig]
	var active: Dictionary = catalog.fittings[state.rig].definition
	_scope_label.text = active.scope_label
	var result: Dictionary = renderer.render(session.local_state())
	if not result.ok:
		status.text = result.reason
		_pending = false
		return
	portrait = result.image
	_set_mode(_mode)
	for index in avatars.size():
		avatars[index].show_image(renderer.avatar(portrait, 88 if index == 0 else 96))
	var limitations: Array[String] = []
	for field in _buttons:
		for index in _buttons[field].size():
			var button: Button = _buttons[field][index]
			var field_info: Dictionary = session.field_definition(field)
			button.text = field_info.options[index] if index < field_info.options.size() else catalog.fields[field].options[index] + "（未适配）"
			var candidate: Dictionary = state.duplicate()
			candidate[field] = index
			var checked: Dictionary = session.validate(candidate)
			button.disabled = not checked.ok
			if not checked.ok and not str(checked.reason) in limitations:
				limitations.append("不可选：" + catalog.fields[field].label + "「" + (field_info.options[index] if index < field_info.options.size() else catalog.fields[field].options[index]) + "」。" + str(checked.reason))
			button.set_pressed_no_signal(state[field] == index)
			button.tooltip_text = checked.get("reason", "选择" + button.text)
	_undo.disabled = not session.history().undo
	_redo.disabled = not session.history().redo
	summary.text = "%s · %s · %s\n%s / %s / %s / %s" % [active.fields.face.options[state.face], active.fields.hair.options[state.hair], active.fields.outfit.options[state.outfit], active.fields.brows.options[state.brows], active.fields.eyes.options[state.eyes], active.fields.nose.options[state.nose], active.fields.mouth.options[state.mouth]]
	status.text = limitations[0] if not limitations.is_empty() else "每项立即预览。衣身与肤色保留原有光影，头像来自同一组合。"
	_pending = false

## 选择失败显示具体原因，图像和全部原选件保持不变。
func _choose(field: String, index: int) -> void:
	var result: Dictionary = session.choose(field, index)
	if not result.ok:
		status.text = result.reason

## 当前组恢复的兼容冲突同样保留原草案。
func _reset_group(group_id: String) -> void:
	var result: Dictionary = session.reset_group(group_id)
	if not result.ok:
		status.text = result.reason

## 更新显式锁定状态，不重画或随机改变人物。
func _lock_changed(locked: bool, field: String) -> void:
	session.set_locked(field, locked)

## 半身与头像共用同一成品，不重算身份或产生新组合。
func _set_mode(mode: int) -> void:
	_mode = mode
	if portrait != null:
		preview.show_image(portrait if mode == 0 else renderer.avatar(portrait, 310))

## 全部预览一起换检查底色，人物像素保持原样。
func _set_backdrop(value: int) -> void:
	preview.set_backdrop(value)
	for item in avatars:
		item.set_backdrop(value)

## 撤销真实成功操作，空历史无副作用。
func _undo_pressed() -> void:
	session.undo()

## 重做上一次撤销，不重新执行随机。
func _redo_pressed() -> void:
	session.redo()

## 每次显式点击使用新种子；浏览、切视图和换底色不会随机。
func _random_pressed() -> void:
	_random_counter += 1
	var result: Dictionary = session.randomize_selection(_random_counter)
	if not result.ok or not result.get("changed", true):
		status.text = result.get("reason", "随机结果与当前人物相同，原选择已保留。")

## 恢复进入时草案，不涉及磁盘、主游戏存档或世界回滚。
func _restore_pressed() -> void:
	session.restore_entry()

## 在现有工坊内提供轻量覆盖层，不另造启动菜单，也不改变当前人物。
func _build_entry_overlays() -> void:
	var sample_body := _make_entry_overlay("代表样本 · 三族各四", "SampleOverlay")
	_sample_overlay = sample_body.get_meta("overlay")
	_entry_text(sample_body, "点击下方代表将显式替换整组外观，可一步撤销。锁仅控制随机，载入后原锁保持；不会自动写盘。")
	if _sample_catalog.get("ok", false):
		for fitting in catalog.fittings:
			var heading := Label.new()
			heading.text = fitting.label
			heading.add_theme_font_size_override("font_size", 20)
			sample_body.add_child(heading)
			var row := HBoxContainer.new()
			row.add_theme_constant_override("separation", 8)
			sample_body.add_child(row)
			for sample in _sample_catalog.samples:
				if sample.fitting_id == fitting.id:
					var button := _button(row, sample.label, "Sample_" + sample.id, _apply_sample.bind(sample.id))
					button.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	_entry_text(sample_body, "12名代表只覆盖已查看的成人样本，不代表任意组合都可辨。载入后仍可独立捏脸、换衣冠。")
	var guide_body := _make_entry_overlay("操作与阶段边界", "GuideOverlay")
	_guide_overlay = guide_body.get_meta("overlay")
	_entry_text(guide_body, "1. 选底稿与脸型，再分别改眉、眼、鼻、唇。也可从代表样本开始。\n2. 向下滚动换发冠、衣身及限定颜色；锁定仅影响随机。\n3. 半身与头像来自同一人物；浅底、深底、棋盘只供检查。\n4. 可撤销、重做、重置本组或恢复进入时。载入样本是整组手动替换，可一步撤销。")
	_entry_text(guide_body, "灰项会说明具体限制。短须仅适配男像基础脸型与平唇；女性没有这款须件。缺件会拒绝整次变化，不会自动换脸或剃须。")
	_entry_text(guide_body, "当前：三个成人比例族，各4名已取样代表，眉眼鼻口独立选件、两套衣身及受控颜色。\n尚未制作：儿童/老年比例、额外身形、持物姿态、大衣装库、战斗装备外观。灰发不是老年比例。\n本工坊不接主游戏或存档，不自动写盘；关闭后本次选择不会保存。")

## 固定宽度内容使用真实容器收拢高度，覆盖背景阻止误点下层按钮。
func _make_entry_overlay(title: String, id: String) -> VBoxContainer:
	var overlay := Control.new()
	overlay.name = id
	overlay.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	overlay.mouse_filter = Control.MOUSE_FILTER_STOP
	add_child(overlay)
	var shade := ColorRect.new()
	shade.color = Color(0.02, 0.04, 0.04, 0.78)
	shade.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	overlay.add_child(shade)
	var center := CenterContainer.new()
	center.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	overlay.add_child(center)
	var panel := PanelContainer.new()
	panel.name = id + "Panel"
	panel.custom_minimum_size.x = 820
	var style := StyleBoxFlat.new()
	style.bg_color = Color("203234")
	style.border_color = Color("9d825c")
	style.set_border_width_all(1)
	style.set_corner_radius_all(8)
	for edge in ["left", "right", "top", "bottom"]:
		style.set("content_margin_" + edge, 24.0)
	panel.add_theme_stylebox_override("panel", style)
	center.add_child(panel)
	var body := VBoxContainer.new()
	body.add_theme_constant_override("separation", 16)
	body.set_meta("overlay", overlay)
	panel.add_child(body)
	var heading := HBoxContainer.new()
	body.add_child(heading)
	var label := Label.new()
	label.text = title
	label.add_theme_font_size_override("font_size", 26)
	label.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	heading.add_child(label)
	_button(heading, "返回工坊", "Close" + id, _close_entry_overlay.bind(overlay))
	overlay.hide()
	return body

## 说明文字只在有限内容宽度内换行，不撑开窄窗或压住现有工作台。
func _entry_text(parent: Control, text: String) -> void:
	var label := Label.new()
	label.text = text
	label.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	label.custom_minimum_size.x = 772
	parent.add_child(label)

## 打开样本只浏览，不载入默认、不改变历史或锁。
func _open_samples() -> void:
	if _sample_catalog.get("ok", false):
		_open_entry_overlay(_sample_overlay)

## 操作边界随时可查，不阻断首次进入可操作工作台。
func _open_guide() -> void:
	_open_entry_overlay(_guide_overlay)

## 限定Tab焦点在当前覆盖层，关闭后回到原入口按钮。
func _open_entry_overlay(overlay: Control) -> void:
	_return_focus = get_viewport().gui_get_focus_owner()
	overlay.show()
	var buttons: Array[Node] = overlay.find_children("*", "Button", true, false)
	for index in buttons.size():
		buttons[index].focus_next = buttons[(index + 1) % buttons.size()].get_path()
		buttons[index].focus_previous = buttons[(index - 1 + buttons.size()) % buttons.size()].get_path()
	if not buttons.is_empty():
		buttons[0].grab_focus()

## 返回操作不产生会话变化，关闭只影响当前说明覆盖层。
func _close_entry_overlay(overlay: Control) -> void:
	overlay.hide()
	if is_instance_valid(_return_focus):
		_return_focus.grab_focus()

## 样本作为一次完整手动事务应用，保留锁且可一步撤销，不走随机或逐项切换。
func _apply_sample(id: String) -> void:
	for sample in _sample_catalog.get("samples", []):
		if sample.id == id:
			var result: Dictionary = session.apply(sample.state.duplicate(true))
			if result.ok:
				_close_entry_overlay(_sample_overlay)
				status.text = "已载入「" + str(sample.fitting_label) + " · " + str(sample.label) + "」整组外观；可撤销，原锁保持。"
			else:
				status.text = result.reason
			return
	status.text = "没有这个代表样本，原人物已保留。"

## Escape只关闭已打开的说明或样本，不触碰当前人物和撤销历史。
func _unhandled_key_input(event: InputEvent) -> void:
	if event.is_action_pressed("ui_cancel"):
		for overlay in [_sample_overlay, _guide_overlay]:
			if is_instance_valid(overlay) and overlay.visible:
				_close_entry_overlay(overlay)
				get_viewport().set_input_as_handled()
				return
