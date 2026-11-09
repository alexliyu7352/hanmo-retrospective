@tool
## 新局与作者预览共用的外貌草案控件；只保存可丢弃选择，不持有或写入世界。
extends VBoxContainer
signal confirmed(draft: Dictionary)
signal cancelled
const Appearance = preload("res://core/domains/identity/actor_appearance.gd")
const Character = preload("res://ui/character_presentation.gd")
const Portrait = preload("res://ui/layered_portrait.gd")
const LABELS = {"face_shape": "脸型", "eyes": "眼睛", "brows": "眉形", "nose": "鼻形", "mouth": "口形", "hair": "发式", "beard": "胡须", "skin_palette": "肤色", "build": "体型"}
var _actor_id: String = ""
var _definition: Dictionary = {}
var _seed: String = ""
var _age: int = -1
var _registry: RefCounted
var _draft: Dictionary = {}
var _descriptor: Dictionary = {}
var _generation: int = 0
var _closed: bool = true
var _editable: bool = false
var _portrait: Control
var _status: Label
var _confirm: Button
var _choices: Dictionary = {}

## 每次配置建立独立代次；坏定义或草案失效全部旧回调，固定人物只读且确认不改其来源。
func configure(actor_id: String, actor_definition: Dictionary, generation_seed: String, age_years: int, registry: RefCounted, initial_draft: Dictionary = {}) -> Dictionary:
	_generation += 1; _closed = true
	_clear_controls()
	if not actor_definition.has("appearance_definition"): return {"ok": false, "code": "MissingAppearanceDefinition"}
	var prepared: Dictionary = Appearance.prepare(actor_id, generation_seed, actor_definition, initial_draft)
	if not prepared.ok: return prepared
	_actor_id = actor_id; _definition = actor_definition.duplicate(true); _seed = generation_seed
	_age = age_years; _registry = registry
	_draft = {"base_features": prepared.profile.base_features.duplicate(true), "attire_style": prepared.profile.attire_style}
	_editable = _definition.appearance_definition.kind == "generated" and _definition.get("origin") != "historical"
	var candidate: Dictionary = _preview(_draft)
	if not candidate.ok: return candidate
	_descriptor = candidate.descriptor
	_closed = false
	_build_controls()
	return {"ok": true}

## 返回草案深拷贝供宿主说明当前选择；不会直接提供可改写的内部状态。
func draft() -> Dictionary:
	return _draft.duplicate(true)

## 返回已接受的预览描述；资源降级另在面板可见诊断中说明。
func describe() -> Dictionary:
	return _descriptor.duplicate(true)

## 确认只发送一次经过同一prepare再次校验的选择；生成器和世界提交仍由宿主负责。
func confirm_selection() -> void:
	_confirm_generation(_generation)

## 取消与窗口关闭采用相同终结语义，不发合法草案也无需回滚任何世界。
func cancel_selection() -> void:
	_cancel_generation(_generation)

## 宿主关闭窗口时统一取消，随后到达的按钮回调全部失效。
func close() -> void:
	cancel_selection()

## 只建立常规容器和真实选项按钮，窄视口由纵向滚动保留所有操作。
func _build_controls() -> void:
	name = "AppearancePicker"
	add_theme_constant_override("separation", 8)
	var heading = Label.new(); heading.text = "外貌草案" if _editable else "固定外貌预览"
	heading.add_theme_font_size_override("font_size", 24); add_child(heading)
	var scroll = ScrollContainer.new(); scroll.size_flags_vertical = Control.SIZE_EXPAND_FILL
	scroll.horizontal_scroll_mode = ScrollContainer.SCROLL_MODE_DISABLED; add_child(scroll)
	var body = VBoxContainer.new(); body.size_flags_horizontal = Control.SIZE_EXPAND_FILL; scroll.add_child(body)
	_portrait = Portrait.new(); _portrait.name = "AppearancePreview"; _portrait.custom_minimum_size = Vector2(160, 220); body.add_child(_portrait)
	var rendered: Dictionary = _portrait.configure(_descriptor, _registry)
	_portrait.set_meta("render_result", rendered)
	for feature in Appearance.FEATURES:
		var row = HBoxContainer.new(); body.add_child(row)
		var label = Label.new(); label.text = LABELS[feature]; label.custom_minimum_size.x = 52; row.add_child(label)
		var choice = OptionButton.new(); choice.size_flags_horizontal = Control.SIZE_EXPAND_FILL
		choice.fit_to_longest_item = false; choice.clip_text = true
		choice.set_meta("appearance_feature", feature); row.add_child(choice); _choices[feature] = choice
		choice.item_selected.connect(_select_feature.bind(feature, choice, _generation))
	var attire = Label.new(); attire.name = "AppearanceAttire"
	attire.text = "衣着：" + _draft.attire_style + "（采用作者定义，本轮不提供换装）"
	attire.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART; body.add_child(attire)
	_status = Label.new(); _status.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART; add_child(_status)
	var actions = HFlowContainer.new(); add_child(actions)
	_confirm = _action(actions, "确定", "confirm", _confirm_generation.bind(_generation))
	_action(actions, "取消", "cancel", _cancel_generation.bind(_generation))
	_action(actions, "关闭", "close", _cancel_generation.bind(_generation))
	_refresh_choices()
	_refresh_status(rendered)

## 操作按钮携带稳定语义，回调捕获创建代次而非追随复用后的新草案。
func _action(parent: Node, text: String, action: String, callback: Callable) -> Button:
	var control = Button.new(); control.text = text; control.custom_minimum_size.y = 36
	control.set_meta("appearance_action", action); control.pressed.connect(callback); parent.add_child(control)
	return control

## 从作者候选逐一检查当前年龄与配方；未映射语义明确禁用，不能冒称画面已经显示。
func _refresh_choices() -> void:
	for feature in Appearance.FEATURES:
		var control: OptionButton = _choices[feature]; control.clear()
		var values: Array = _definition.appearance_definition.candidates[feature].duplicate() if _editable else [_draft.base_features[feature]]
		values.sort()
		var available: int = 0
		for value in values:
			var candidate: Dictionary = _draft.duplicate(true); candidate.base_features[feature] = value
			var result: Dictionary = _preview(candidate)
			var mapped: bool = result.ok and _compatible(result.descriptor) and _feature_visible(feature, result.descriptor)
			var label: String = value + ("" if mapped else "（未映射可见部件）")
			control.add_item(label); var index: int = control.item_count - 1
			control.set_item_metadata(index, value); control.set_item_disabled(index, not mapped)
			if value == _draft.base_features[feature]: control.select(index)
			if mapped: available += 1
		control.disabled = not _editable or available < 2
		control.tooltip_text = "只改变当前草案；确定后由宿主提交" if not control.disabled else "固定作者值或当前没有多个已制作的相容部件"

## 只认可解析器实际采用的语义绑定；专绘覆盖与未采用的候选不算可见部件。
func _feature_visible(feature: String, descriptor: Dictionary) -> bool:
	var manifest: Dictionary = _registry.describe()
	for binding_id in descriptor.get("selected_binding_id", []):
		if not manifest.get("character_profiles", {}).has(binding_id): continue
		for variant in manifest.character_profiles[binding_id].age_variants:
			if variant.id != descriptor.age_band: continue
			for binding in variant.get("feature_bindings", []):
				if not binding.features.has(feature) or not ("feature:" + binding.id) in descriptor.selected_binding_id: continue
				for layer in descriptor.layers:
					if layer.id == binding.component_id: return true
	return false

## 选项失败保留旧草案与像素；重复或迟到的旧控件信号不重新开启已终结会话。
func _select_feature(index: int, feature: String, control: OptionButton, generation: int) -> void:
	if _closed or generation != _generation or not _editable or not is_instance_valid(control) or control != _choices.get(feature): return
	if control.disabled or index < 0 or index >= control.item_count or control.is_item_disabled(index): return
	var candidate: Dictionary = _draft.duplicate(true); candidate.base_features[feature] = control.get_item_metadata(index)
	var checked: Dictionary = _preview(candidate)
	if not checked.ok or not _compatible(checked.descriptor) or not _feature_visible(feature, checked.descriptor):
		_refresh_choices(); _status.text = "这项选择不可用，已保留之前的草案。"; return
	var rendered: Dictionary = _portrait.configure(checked.descriptor, _registry)
	if not rendered.ok:
		_refresh_choices(); _status.text = "预览未更新：" + str(rendered.code); return
	_draft = candidate; _descriptor = checked.descriptor
	_refresh_choices(); _refresh_status(rendered)

## 准备与预览严格复用领域合同和唯一解析器，不含独立配方、游戏时钟或随机源。
func _preview(selection: Dictionary) -> Dictionary:
	var prepared: Dictionary = Appearance.prepare(_actor_id, _seed, _definition, selection)
	if not prepared.ok: return prepared
	var actor_view: Dictionary = {"actor_id": _actor_id, "display_name": _definition.get("display_name", _actor_id), "presentation_profile_id": _definition.get("presentation_profile_id", ""), "portrait_kind": _definition.get("portrait_kind", "")}
	var descriptor: Dictionary = Character.preview({"actor_id": _actor_id, "actor_view": actor_view, "appearance": prepared.profile}, _age, {}, _registry)
	return {"ok": true, "descriptor": descriptor} if descriptor.ok else descriptor

## 有降级或没有人物像素层时不把生成草案标为可确认；固定人物仍可原样继续。
func _compatible(descriptor: Dictionary) -> bool:
	return descriptor.get("degradation", []).is_empty() and not descriptor.get("layers", []).is_empty()

## 明示表现缺口和固定限制；禁用确认不影响取消与关闭。
func _refresh_status(rendered: Dictionary) -> void:
	_confirm.disabled = not rendered.ok or (_editable and not _compatible(_descriptor))
	if not rendered.ok: _status.text = "预览不可用：" + str(rendered.get("code", ""))
	elif not _descriptor.degradation.is_empty(): _status.text = "当前为兼容预览：" + JSON.stringify(_descriptor.degradation)
	elif not _editable: _status.text = "此人物外貌由作者固定，不能在新局中改动。"
	else: _status.text = "仅已制作且相容的部件可选。标记未映射的语义保持原值；取消或关闭不保存。"

## 同代次只发一次合法结果；最后重验清单与像素，防止打开期间资源失效仍提交。
func _confirm_generation(generation: int) -> void:
	if _closed or generation != _generation: return
	var checked: Dictionary = _preview(_draft)
	if not checked.ok: _status.text = "无法确定：" + str(checked.get("code", "")); return
	if _editable and not _compatible(checked.descriptor): _status.text = "当前组合缺少相容表现，不能确定。"; return
	var rendered: Dictionary = _portrait.configure(checked.descriptor, _registry)
	if not rendered.ok: _refresh_status(rendered); return
	_closed = true; _confirm.disabled = true
	confirmed.emit(_draft.duplicate(true) if _editable else {})

## 先终结再通知宿主，宿主重建页面或重入取消都不会收到第二次结果。
func _cancel_generation(generation: int) -> void:
	if _closed or generation != _generation: return
	_closed = true; _confirm.disabled = true
	cancelled.emit()

## 重配时立即移除旧控件，延迟释放不让旧按钮留在当前可操作树中。
func _clear_controls() -> void:
	for child in get_children(): remove_child(child); child.queue_free()
	_choices.clear(); _portrait = null; _status = null; _confirm = null
	_draft = {}; _descriptor = {}

## 节点离树后即使宿主仍保留引用，也不能再确认或修改旧草案。
func _exit_tree() -> void:
	_closed = true
