## 本人档案的只读技艺区；只展示投影数值，导航由外层接回原技艺与武学页面。
extends VBoxContainer

const StudyScreen = preload("res://ui/study_screen.gd")

signal ordinary_skills_requested(skill_id: String)
signal martial_requested()

var _generation: int = 0

## 每次替换整块内容并使旧按钮失效；不查询世界、推算等级或提交学习命令。
func present(view: Dictionary) -> void:
	_generation += 1
	for child in get_children():
		remove_child(child)
		child.queue_free()
	name = "SelfProfileSkills"
	size_flags_horizontal = Control.SIZE_EXPAND_FILL
	add_theme_constant_override("separation", 10)
	_text(self, "普通技艺", 24).name = "SelfProfileSkillsHeading"
	if not view.get("ok", true):
		_text(self, "本人技艺暂不可用", 18).name = "SelfProfileSkillsUnavailable"
		return
	var rows = VBoxContainer.new()
	rows.name = "SelfProfileSkillRows"
	rows.add_theme_constant_override("separation", 6)
	add_child(rows)
	for row in view.get("skills", []):
		_skill(rows, row)
	if rows.get_child_count() == 0:
		_text(rows, "当前未提供普通技艺记录。", 18).name = "SelfProfileSkillsEmpty"
	var martial = _button(self, "查看武学与行囊", "SelfProfileMartialOpen")
	martial.tooltip_text = "前往原武学页面查看已学招式、心法与熟练度。"
	var generation: int = _generation
	martial.pressed.connect(func():
		if _current(martial, generation): martial_requested.emit())

## 技艺名称沿既有课程词汇，经验与等级逐字使用原生投影；缺值保持未知。
func _skill(parent: Control, row: Dictionary) -> void:
	var skill_id: String = row.get("skill_id", "")
	var skill_name: String = StudyScreen.SKILL_NAMES.get(skill_id, "技艺未记")
	var xp: Variant = row.get("xp")
	var level: Variant = row.get("level")
	var progress: String = "经验未记录 · 等级未定"
	if xp is int and level is int and xp >= 0 and level >= 0:
		progress = "经验 %d · 等级 %d" % [xp, level]
	var button = _button(parent, skill_name + "\n" + progress, "SelfProfileSkill_" + skill_id)
	button.tooltip_text = "查看这项普通技艺及其原有学习记录。"
	button.set_meta("skill_id", skill_id)
	button.disabled = skill_id.is_empty()
	var generation: int = _generation
	button.pressed.connect(func():
		if _current(button, generation) and not button.disabled:
			ordinary_skills_requested.emit(skill_id))

## 继承本人档案的主题与宽度，只保留自然纵向内容，不新建嵌套滚动区。
func _button(parent: Control, words: String, node_name: String) -> Button:
	var button = Button.new()
	button.name = node_name
	button.text = words
	button.alignment = HORIZONTAL_ALIGNMENT_LEFT
	button.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	button.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	parent.add_child(button)
	return button

## 普通文字随父容器折行；不固化技艺数量或把未知记录补成创建默认值。
func _text(parent: Control, words: String, font_size: int) -> Label:
	var label = Label.new()
	label.text = words
	label.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	label.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	label.add_theme_font_size_override("font_size", font_size)
	parent.add_child(label)
	return label

## 重绘、退出树或排队释放后拒绝迟到导航；旧控件不能借新档案重新打开页面。
func _current(button: Button, generation: int) -> bool:
	if generation != _generation or not is_inside_tree() or not is_instance_valid(button) or not button.is_inside_tree(): return false
	var ancestor: Node = button
	while ancestor != null:
		if ancestor.is_queued_for_deletion(): return false
		ancestor = ancestor.get_parent()
	return true
