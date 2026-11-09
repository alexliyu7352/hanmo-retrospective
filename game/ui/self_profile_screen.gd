## 本人档案的只读表现叶；只显示宿主已取得的当前人物投影，导航不提交世界命令。
extends RefCounted
const Art = preload("res://ui/art_catalog.gd")
const Skills = preload("res://ui/self_profile_skills.gd")
const ATTRIBUTE_NAMES = {"leadership":"统率", "martial":"武艺", "intellect":"智识", "administration":"政务", "charisma":"风仪"}
const ACTION_NAMES = {"travel":"行旅", "long_rest":"休整", "study_skill":"修业", "learn_skill":"学习武学", "practice_skill":"修炼武学", "teach_skill":"传授武学"}

## 固定表头与单层有限滚动容纳本人信息；读取失败保留返回入口，不回退到人物模板。
static func build(host, parent: Control, view: Dictionary, on_close: Callable, on_navigate: Callable) -> Control:
	var panel = PanelContainer.new()
	panel.name = "SelfProfileScreen"
	parent.add_child(panel)
	panel.anchor_left = .025; panel.anchor_right = .975
	panel.anchor_top = .115; panel.anchor_bottom = .975
	panel.add_theme_stylebox_override("panel", Art.ink())
	var column = VBoxContainer.new()
	column.add_theme_constant_override("separation", 12)
	panel.add_child(column)
	var header = HBoxContainer.new()
	header.add_theme_constant_override("separation", 16)
	column.add_child(header)
	var title: Label = host._text(header, "本人档案", 28)
	title.name = "SelfProfileHeading"
	title.autowrap_mode = TextServer.AUTOWRAP_OFF
	title.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	title.size_flags_vertical = Control.SIZE_SHRINK_CENTER
	var close: Button = host._button(header, "返回", "self_profile_close", "", func(): _close(panel, on_close))
	close.name = "SelfProfileClose"
	close.size_flags_vertical = Control.SIZE_SHRINK_CENTER
	host._text(column, "查看本人信息不消耗时间。", 16).name = "SelfProfileReadOnly"
	column.add_child(HSeparator.new())
	var scroll = ScrollContainer.new()
	scroll.name = "SelfProfileScroll"
	scroll.size_flags_vertical = Control.SIZE_EXPAND_FILL
	scroll.horizontal_scroll_mode = ScrollContainer.SCROLL_MODE_DISABLED
	column.add_child(scroll)
	var content = VBoxContainer.new()
	content.name = "SelfProfileContent"
	content.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	content.add_theme_constant_override("separation", 16)
	scroll.add_child(content)
	if view.get("ok", true) == false or view.get("kind", "") != "self_profile" or not view.get("actor_id") is String or view.actor_id.is_empty():
		var unavailable: Label = host._text(content, "本人信息暂不可用，请返回后重试。", 20)
		unavailable.name = "SelfProfileUnavailable"
		unavailable.tooltip_text = str(view.get("code", ""))
		return panel
	panel.set_meta("actor_id", view.actor_id)
	panel.set_meta("source", view.get("source", {}).duplicate(true))
	_identity(host, content, view)
	_attributes(host, content, view)
	_status(host, content, view)
	var skills = Skills.new()
	skills.name = "SelfProfileSkills"
	content.add_child(skills)
	skills.present(view)
	var source: Dictionary = view.get("source", {}).duplicate(true)
	skills.ordinary_skills_requested.connect(func(skill_id: String): _navigate(panel, on_navigate, "ordinary_skills", skill_id, source))
	skills.martial_requested.connect(func(): _navigate(panel, on_navigate, "martial_skills", "", source))
	return panel

## 姓名、周岁与职事来自同一当前人物；未记年龄和无职事都据投影明确说明。
static func _identity(host, parent: Control, view: Dictionary) -> void:
	var card = _paper(parent, "SelfProfileIdentity")
	var name_value: String = str(view.get("display_name", ""))
	host._text(card, name_value if not name_value.is_empty() else "姓名未记录", 30).name = "SelfProfileName"
	var age: Variant = view.get("age_years")
	var age_text: String = "%d 岁" % age if age is int and age >= 0 else "年龄未记录"
	host._text(card, age_text, 20).name = "SelfProfileAge"
	var roles: Variant = view.get("roles")
	var labels: Dictionary = view.get("role_names", {})
	var names: PackedStringArray = []
	if roles is Array:
		for role in roles:
			if role is Dictionary and role.get("role_id") is String:
				names.append(str(labels.get(role.role_id, role.role_id)))
	var role_text: String = "、".join(names) if not names.is_empty() else "无" if roles is Array else "未记录"
	host._text(card, "当前职事：" + role_text, 18).name = "SelfProfileRoles"

## 五维只显示投影中的原值；不计算上限、加成或未定义的综合战力。
static func _attributes(host, parent: Control, view: Dictionary) -> void:
	var card = _paper(parent, "SelfProfileAttributes")
	host._text(card, "五维属性", 22)
	var values: Dictionary = {}
	for row in view.get("attributes", []):
		if row is Dictionary and row.get("attribute_id") is String:
			values[row.attribute_id] = row.get("value")
	var grid = GridContainer.new()
	grid.columns = 5
	grid.add_theme_constant_override("h_separation", 12)
	card.add_child(grid)
	for attribute_id in ATTRIBUTE_NAMES:
		var field = VBoxContainer.new()
		field.size_flags_horizontal = Control.SIZE_EXPAND_FILL
		grid.add_child(field)
		host._text(field, ATTRIBUTE_NAMES[attribute_id], 18)
		var value: Variant = values.get(attribute_id)
		var label: Label = host._text(field, str(value) if value is int else "未记录", 25)
		label.name = "SelfProfileAttribute_" + attribute_id

## 当前生命、疲劳、伤势与可行动状态逐项显示；没有行动时不生成继续半日或成长命令。
static func _status(host, parent: Control, view: Dictionary) -> void:
	var card = _paper(parent, "SelfProfileStatus")
	host._text(card, "当前状态", 22)
	var status: Dictionary = view.get("status", {})
	var life: String = "在世" if status.get("alive") == true else "已故" if status.get("alive") == false else "未记录"
	var available: String = "可行动" if status.get("available") == true else "不可行动" if status.get("available") == false else "行动状态未记录"
	host._text(card, life + " · " + available, 18).name = "SelfProfileAvailability"
	for pair in [["hp", "体力"], ["fatigue", "疲劳"], ["injury", "伤势"]]:
		var value: Variant = status.get(pair[0])
		host._text(card, pair[1] + "：" + (str(value) if value is int else "未记录"), 18).name = "SelfProfileStatus_" + pair[0]
	var action: String = str(status.get("action_kind", ""))
	if not action.is_empty():
		var action_label: Label = host._text(card, "正在进行：" + ACTION_NAMES.get(action, "行动进行中"), 18)
		action_label.name = "SelfProfileAction"
		action_label.tooltip_text = action
	elif status.has("action_kind"):
		host._text(card, "当前无进行中的行动。", 18).name = "SelfProfileAction"

## 纸面沿既有清单和字体主题，内容只做纵向伸展，最小高度不反推撑大视口。
static func _paper(parent: Control, node_name: String) -> VBoxContainer:
	var panel = PanelContainer.new()
	panel.name = node_name
	panel.set_meta("paper", true)
	panel.add_theme_stylebox_override("panel", Art.paper())
	parent.add_child(panel)
	var content = VBoxContainer.new()
	content.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	content.add_theme_constant_override("separation", 10)
	panel.add_child(content)
	return content

## 返回只通知宿主一次；旧页或其祖先已排队销毁后，迟到点击没有效力。
static func _close(panel: Control, callback: Callable) -> void:
	if not _live(panel) or not callback.is_valid(): return
	panel.set_meta("navigation_pending", true)
	callback.call()

## 技艺叶只导航既有页面并带回读取时的世界身份；宿主负责核对后打开，不能转为成长命令。
static func _navigate(panel: Control, callback: Callable, target: String, selected_id: String, source: Dictionary) -> void:
	if not _live(panel) or not callback.is_valid(): return
	panel.set_meta("navigation_pending", true)
	callback.call(target, selected_id, source.duplicate(true))

## 同时检查祖先销毁状态，避免宿主重绘和节点帧末释放之间残留可执行入口。
static func _live(panel: Control) -> bool:
	if not is_instance_valid(panel) or not panel.is_inside_tree() or panel.get_meta("navigation_pending", false): return false
	var current: Node = panel
	while current != null:
		if current.is_queued_for_deletion(): return false
		current = current.get_parent()
	return true
