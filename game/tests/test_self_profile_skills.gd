## 本人技艺控件的固定输入窄门；检查原数值、导航和迟到回调，不冒充原生世界或图形验收。
extends SceneTree

const Skills = preload("res://ui/self_profile_skills.gd")
var failures: int = 0
var checks: int = 0
var navigation: Array = []

## 控件需要进入树后才允许导航，故延后执行固定输入检查。
func _init() -> void:
	call_deferred("run_checks")

## 断言保留实际失败并以非零退出，不把可加载等同于功能通过。
func check(condition: bool, label: String) -> void:
	checks += 1
	if not condition:
		failures += 1
		printerr("FAIL: " + label)

## 原生等级由输入负责；故意使用不相符的经验证明控件不会重算或补创建默认值。
func run_checks() -> void:
	var holder = Control.new()
	root.add_child(holder)
	var component = Skills.new()
	holder.add_child(component)
	component.ordinary_skills_requested.connect(func(id: String): navigation.append(["ordinary", id]))
	component.martial_requested.connect(func(): navigation.append(["martial", ""]))
	var view: Dictionary = {"ok": true, "skills": [
		{"skill_id": "martial", "xp": 37, "level": 4},
		{"skill_id": "rites", "xp": 0, "level": 0},
		{"skill_id": "commerce", "xp": 1200, "level": 4}]}
	var before: Dictionary = view.duplicate(true)
	component.present(view)
	var ordinary: Button = component.find_child("SelfProfileSkill_martial", true, false)
	var martial: Button = component.find_child("SelfProfileMartialOpen", true, false)
	check(ordinary.text == "武艺\n经验 37 · 等级 4", "显示输入经验和原生等级且复用既有名称")
	check(component.find_child("SelfProfileSkill_rites", true, false).text.contains("经验 0 · 等级 0"), "真实零值保留")
	check(component.find_child("SelfProfileSkill_commerce", true, false).text.contains("经验 1200 · 等级 4"), "满级经验原样显示")
	check(component.find_children("*", "Button", true, false).size() == 4, "只生成实际技艺导航和原武学入口")
	check(component.find_children("*", "ScrollContainer", true, false).is_empty(), "沿父页面滚动而不嵌套滚动区")
	ordinary.pressed.emit()
	martial.pressed.emit()
	check(navigation == [["ordinary", "martial"], ["martial", ""]], "两种原页面导航只发各自信号")
	check(view == before, "展示和点击不改投影输入")
	component.present({"ok": true, "skills": []})
	ordinary.pressed.emit()
	martial.pressed.emit()
	check(navigation.size() == 2, "重绘后迟到按钮不导航")
	check(component.find_child("SelfProfileSkillsEmpty", true, false) != null, "无普通技艺能力是合法空状态")
	check(component.find_children("*", "Button", true, false).size() == 1, "合法空状态仍可进入已有武学页")
	component.present({"ok": false, "skills": view.skills})
	check(component.find_child("SelfProfileSkillsUnavailable", true, false) != null, "读取失败明确不可用")
	check(component.find_children("*", "Button", true, false).is_empty(), "读取失败不沿旧记录导航")
	component.present({"ok": true, "skills": [{"skill_id": "rites", "xp": null, "level": -1}]})
	check(component.find_child("SelfProfileSkill_rites", true, false).text.contains("经验未记录 · 等级未定"), "缺值不补零或默认等级")
	component.present(view)
	martial = component.find_child("SelfProfileMartialOpen", true, false)
	holder.remove_child(component)
	martial.pressed.emit()
	check(navigation.size() == 2, "退出树后的旧导航失效")
	holder.add_child(component)
	holder.queue_free()
	martial.pressed.emit()
	check(navigation.size() == 2, "父页面排队关闭后拒绝迟到导航")
	await process_frame
	print("SELF_PROFILE_SKILLS_CHECKS=%d FAILURES=%d GUI=false" % [checks, failures])
	quit(0 if failures == 0 else 1)
