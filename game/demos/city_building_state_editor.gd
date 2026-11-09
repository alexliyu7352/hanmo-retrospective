## 同地块建筑状态工作台：已制作变体与外观替换统一进入既有可取消、可撤销内存事务。
extends "res://demos/city_layout_editor.gd"
const STATE_ORDER = ["empty", "foundation", "construction", "complete", "damaged", "repair"]
var _state_panel: PanelContainer
var _target_label: Label
var _state_choice: OptionButton
var _type_choice: OptionButton
var _reuse_note: Label

## 复用街段与注册表，初始即选粮铺供同一身份状态链核对。
func _ready() -> void:
	super._ready()
	if not _editor_ready: return
	selected = "market.grain"; canvas.select_instance(selected)
	_refresh_state_controls(); _state_panel.show(); _responsive()
	_status.text = "同一粮铺、同一地块；状态只改显示，选后仍需确定。"

## 状态面板与画面分离，不把状态名、工程进度或按钮烧进素材。
func _build_interface() -> void:
	super._build_interface()
	var toggle: Button = _button(_toolbar, "状态/外观", "ToggleStatePanel", _toggle_state_panel)
	toggle.custom_minimum_size.x = 110
	_state_panel = PanelContainer.new(); _state_panel.name = "BuildingStatePanel"; add_child(_state_panel)
	_state_panel.position = Vector2(24, 116); _state_panel.custom_minimum_size = Vector2(282, 0)
	_state_panel.add_theme_stylebox_override("panel", _panel(Color("eee2c6f5")))
	var body: VBoxContainer = VBoxContainer.new(); body.add_theme_constant_override("separation", 8); _state_panel.add_child(body)
	_target_label = _label("", 20); body.add_child(_target_label)
	body.add_child(_label("显示状态", 16))
	_state_choice = OptionButton.new(); _state_choice.name = "BuildingStateChoice"; _state_choice.custom_minimum_size = Vector2(250, 42)
	_state_choice.fit_to_longest_item = false; body.add_child(_state_choice); _state_choice.item_selected.connect(_choose_state)
	body.add_child(_label("同地块外观", 16))
	_type_choice = OptionButton.new(); _type_choice.name = "BuildingTypeChoice"; _type_choice.custom_minimum_size = Vector2(250, 42)
	_type_choice.fit_to_longest_item = false; body.add_child(_type_choice); _type_choice.item_selected.connect(_choose_type)
	_reuse_note = _label("", 15); _reuse_note.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	_reuse_note.custom_minimum_size.x = 250; body.add_child(_reuse_note)

## 根据真实按钮宽度计算行数，增加状态入口也不让窄窗按钮或说明溢出。
func _responsive() -> void:
	super._responsive()
	if _toolbar == null: return
	var used: float = 0; var rows: int = 1; var available: float = maxf(100, size.x - 72)
	for child in _toolbar.get_children():
		if not child is Button or not child.visible: continue
		var width: float = child.get_combined_minimum_size().x
		if used > 0 and used + 8 + width > available: rows += 1; used = 0
		used += width + (8 if used > 0 else 0)
	get_node("CityFooter").offset_top = -(54 + rows * 50 + (18 if size.x < 1000 else 0))

## 浏览和近景收起无效编辑命令；返回按钮显隐后立即按实际行数重排底栏。
func _sync_buttons() -> void:
	super._sync_buttons()
	if not _editor_ready: return
	var editing_allowed: bool = canvas.editing and active_place.is_empty()
	for button in [_road_edit, _remove_edit, _confirm_edit, _cancel_edit, _undo_edit, _redo_edit, _reset_button]:
		button.visible = editing_allowed
	var toggle: Button = find_child("ToggleStatePanel", true, false)
	if toggle != null: toggle.visible = editing_allowed
	_responsive()

## 有效候选可供预览，非法候选仍显示原记录，避免在下拉菜单里偷偷变更当前状态。
func _display_record() -> Dictionary:
	if not _editor_ready: return {}
	var draft: Dictionary = session.pending() if session.status().valid else session.layout()
	for row in draft.get("instances", []):
		if row.id == selected: return row.duplicate(true)
	return {}

## 状态菜单显示未制作项及原因，修缮复用明确标注；候选来自作者定义。
func _refresh_state_controls() -> void:
	if _state_choice == null: return
	var record: Dictionary = _display_record()
	_state_choice.clear(); _type_choice.clear()
	if record.is_empty(): _target_label.text = "请选择已有场所"; _state_choice.disabled = true; _type_choice.disabled = true; return
	var archetype: Dictionary = document.archetypes[record.archetype]
	var plot_name: String = str(record.get("plot_id", ""))
	for plot in layout.get("plots", []):
		if plot.id == plot_name: plot_name = plot.get("name", plot_name)
	_target_label.text = record.name + " · " + plot_name
	for state in STATE_ORDER:
		var supported: bool = state == "complete" or (state == "empty" and archetype.get("supports_empty", false)) or archetype.get("states", {}).has(state)
		var note: String = str(archetype.get("state_notes", {}).get(state, ""))
		var label: String = Model.STATE_LABELS[state] + ("（未制作）" if not supported else ("（" + note + "）" if not note.is_empty() else ""))
		_state_choice.add_item(label); var index: int = _state_choice.item_count - 1
		_state_choice.set_item_metadata(index, state); _state_choice.set_item_disabled(index, not supported)
		if state == record.state: _state_choice.select(index)
	for key in document.archetypes:
		if not document.archetypes[key].has("footprint"): continue
		_type_choice.add_item(document.archetypes[key].get("display_name", key))
		var index: int = _type_choice.item_count - 1; _type_choice.set_item_metadata(index, key)
		if key == record.archetype: _type_choice.select(index)
	_reuse_note.text = "仅显示草案。缺失状态或不兼容占地会拒绝；没有费用、工期或所有权变化。"
	var note: String = str(archetype.get("state_notes", {}).get(record.state, ""))
	if not note.is_empty(): _reuse_note.text = "此状态" + note + "；不表示另有一张专用素材。"
	_state_choice.disabled = not canvas.editing or not active_place.is_empty()
	_type_choice.disabled = _state_choice.disabled

## 状态按钮先显示候选，不让菜单选择绕过确定或取消。
func preview_visual_state(state: String) -> Dictionary:
	if not _editor_ready or not canvas.editing or not active_place.is_empty(): return {"ok": false, "code": "CityEditorInactive"}
	var result: Dictionary = session.preview_state(selected, state)
	_show_state_candidate(result, "显示候选：" + Model.STATE_LABELS.get(state, state))
	return result

## 外观替换保留当前状态；缺少同状态素材时不会静默使用完工图。
func preview_building_type(archetype: String) -> Dictionary:
	if not _editor_ready or not canvas.editing or not active_place.is_empty(): return {"ok": false, "code": "CityEditorInactive"}
	var result: Dictionary = session.preview_replace(selected, archetype)
	_show_state_candidate(result, "同地块外观候选")
	return result

## 共用真实注册表和画布验证后呈现；失败恢复已接受布局并给出具体缺口。
func _show_state_candidate(result: Dictionary, message: String) -> void:
	canvas.clear_preview()
	if result.ok:
		canvas.configure(document, session.pending(), registry); canvas.select_instance(selected)
		_status.text = message + "；确定或取消。"
	else:
		canvas.configure(document, session.layout(), registry); canvas.select_instance(selected)
		_status.text = "保留原显示：" + {"UnsupportedCityVisualState": "该外观没有当前状态素材", "CityStateCanvasMismatch": "状态画布不兼容"}.get(result.get("code", ""), REASONS.get(result.get("code", ""), str(result.get("code", ""))))
	_refresh_state_controls(); _sync_buttons()

## 只接受当前菜单中的可用项，不因程序触发禁用选项而改写候选。
func _choose_state(index: int) -> void:
	if _state_choice.disabled or index < 0 or index >= _state_choice.item_count or _state_choice.is_item_disabled(index): return
	preview_visual_state(str(_state_choice.get_item_metadata(index)))

## 外观列表引用同一作者方案，不直接拼资源路径。
func _choose_type(index: int) -> void:
	if _type_choice.disabled or index < 0 or index >= _type_choice.item_count: return
	preview_building_type(str(_type_choice.get_item_metadata(index)))

## 对象切换后刷新同一场所的状态选项，上一候选仍遵守父类取消语义。
func _selected_instance(id: String) -> void:
	super._selected_instance(id); _refresh_state_controls()

## 确定、取消与历史恢复后同步菜单，防止菜单与已绘状态分离。
func _accept_session(message: String) -> void:
	super._accept_session(message); _refresh_state_controls()

## 面板只控制显示，不改变当前选中对象或原场所身份。
func _toggle_state_panel() -> void:
	if _state_panel != null: _state_panel.visible = not _state_panel.visible

## 浏览与近景不保留可写状态面板，避免把显示编辑理解为场所命令。
func _toggle_editing() -> void:
	super._toggle_editing(); _state_panel.visible = canvas.editing; _refresh_state_controls(); _responsive()

## 近景沿用同一人物无关建筑描述，面板退出焦点但状态不重置。
func enter_place(id: String) -> void:
	_state_panel.hide(); super.enter_place(id)
