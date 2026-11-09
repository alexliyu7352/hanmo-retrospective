## 本人档案叶的控件级验收；只用有限投影检查值、布局与导航，不冒充真实世界或图形试玩。
extends SceneTree
const Screen = preload("res://ui/self_profile_screen.gd")

## 宿主仅有既有文字与按钮接口，故界面若读取Kernel或提交命令会直接报错。
class Host extends Control:
	## 文本继承主题并自动换行，与正式Main保持相同的控件约束。
	func _text(parent: Node, text: String, font_size: int = 20) -> Label:
		var label = Label.new()
		label.text = text
		label.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
		label.add_theme_font_size_override("font_size", font_size)
		parent.add_child(label)
		return label
	## 按钮保留语义标签，仅连接调用者传入的导航回调。
	func _button(parent: Node, text: String, kind: String, target: String, callback: Callable) -> Button:
		var button = Button.new()
		button.text = text
		button.custom_minimum_size = Vector2(90, 40)
		button.set_meta("command_kind", kind)
		button.set_meta("target_id", target)
		button.pressed.connect(callback)
		parent.add_child(button)
		return button

var checks: int = 0
var failures: int = 0
var closes: int = 0
var navigations: Array = []

## 延后运行以便真实Godot容器在SceneTree中完成布局。
func _init() -> void:
	call_deferred("_run")

## 明确记录每项断言，失败时最终返回非零退出码。
func _check(condition: bool, message: String) -> void:
	checks += 1
	if not condition:
		failures += 1
		printerr("FAIL: " + message)

## 等待有限帧让尺寸、滚动条和换行稳定，不开启世界时间。
func _settle() -> void:
	for frame in range(5): await process_frame

## 构造显式命名的控件夹具；数值只是边界输入，绝不用于产品人物默认值。
func _view() -> Dictionary:
	var skills: Array = []
	for skill_id in ["infantry", "cavalry", "archery", "martial", "tactics", "commerce", "negotiation", "rites", "debate", "transport"]:
		skills.append({"skill_id":skill_id, "xp":17, "level":1})
	return {"kind":"self_profile", "actor_id":"actor:fixture", "display_name":"本人档案控件夹具", "age_years":37,
		"attributes":[{"attribute_id":"leadership", "value":11}, {"attribute_id":"martial", "value":22}, {"attribute_id":"intellect", "value":33}, {"attribute_id":"administration", "value":44}, {"attribute_id":"charisma", "value":55}],
		"roles":[{"role_id":"role:fixture", "place_id":"place:fixture", "organization_id":"organization:fixture"}], "role_names":{"role:fixture":"夹具职事"},
		"status":{"alive":true, "available":false, "hp":73, "fatigue":19, "injury":6, "action_kind":"study_skill"}, "skills":skills,
		"source":{"world_id":"world:fixture", "execution_epoch":7, "revision":29}}

## 每次创建独立宿主，避免前一页的主题或节点残留掩盖重复开启缺陷。
func _host() -> Host:
	var host = Host.new()
	var font = SystemFont.new()
	font.font_names = PackedStringArray(["Noto Sans CJK SC", "WenQuanYi Zen Hei", "DejaVu Sans"])
	host.theme = Screen.Art.theme(font)
	root.add_child(host)
	host.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	return host

## 收集返回回调次数，不在测试宿主实现任何世界行为。
func _closed() -> void:
	closes += 1

## 收集宿主收到的页面名、选项和读取身份，验证源副本未被控件改写。
func _navigate(target: String, selected_id: String, source: Dictionary) -> void:
	navigations.append({"target":target, "selected_id":selected_id, "source":source})

## 在两种要求尺寸核有限滚动，随后验证缺失数据、失败、换人和所有迟到导航。
func _run() -> void:
	for resolution in [Vector2i(800, 600), Vector2i(1280, 720)]:
		root.size = resolution
		root.content_scale_size = resolution
		var host = _host()
		var view: Dictionary = _view()
		var before: Dictionary = view.duplicate(true)
		var panel: Control = Screen.build(host, host, view, _closed, _navigate)
		await _settle()
		_check(panel.find_child("SelfProfileName", true, false).text == view.display_name, "本人实际姓名")
		_check(panel.find_child("SelfProfileAge", true, false).text == "37 岁", "本人实际周岁")
		_check(panel.find_child("SelfProfileRoles", true, false).text == "当前职事：夹具职事", "职事沿同一投影作者名称")
		for row in view.attributes:
			_check(panel.find_child("SelfProfileAttribute_" + row.attribute_id, true, false).text == str(row.value), "五维原值 " + row.attribute_id)
		for key in ["hp", "fatigue", "injury"]:
			_check(panel.find_child("SelfProfileStatus_" + key, true, false).text.ends_with(str(view.status[key])), "当前状态原值 " + key)
		_check(panel.find_child("SelfProfileAvailability", true, false).text == "在世 · 不可行动", "活着与可行动分别显示")
		var scroll: ScrollContainer = panel.find_child("SelfProfileScroll", true, false)
		var content: Control = panel.find_child("SelfProfileContent", true, false)
		var close: Button = panel.find_child("SelfProfileClose", true, false)
		_check(scroll.size.y > 100 and scroll.size.y < resolution.y, "有限滚动视口 " + str(resolution))
		_check(content.size.y > scroll.size.y, "完整内容超出视口时滚动 " + str(resolution))
		_check(content.size.x <= scroll.size.x + 1, "内容无水平溢出 " + str(resolution))
		_check(Rect2(Vector2.ZERO, Vector2(resolution)).encloses(panel.get_global_rect()), "整页保持在窗口内 " + str(resolution))
		var close_position: Vector2 = close.global_position
		scroll.scroll_vertical = 100000
		await _settle()
		_check(scroll.scroll_vertical > 0 and close.global_position == close_position, "末项可达且返回固定 " + str(resolution))
		_check(view == before, "构造和滚动不改投影")
		close.pressed.emit(); close.pressed.emit()
		_check(closes == (1 if resolution.x == 800 else 2), "重复返回只通知一次")
		host.queue_free()
		await _settle()
	await _edge_cases()
	print("SELF_PROFILE_SCREEN_CHECKS=%d FAILURES=%d HEADLESS=%s" % [checks, failures, DisplayServer.get_name() == "headless"])
	quit(0 if failures == 0 else 1)

## 缺失年龄不猜测，失败不泄露旧值，导航只带当前读取源且旧页不能重开。
func _edge_cases() -> void:
	var host = _host()
	var view: Dictionary = _view()
	view.age_years = -1
	view.display_name = "另一位当前人物".repeat(8)
	var panel: Control = Screen.build(host, host, view, _closed, _navigate)
	await _settle()
	_check(panel.find_child("SelfProfileAge", true, false).text == "年龄未记录", "缺失年龄不猜测")
	_check(panel.find_child("SelfProfileName", true, false).text == view.display_name, "再次开启按传入当前人物显示")
	_check(panel.find_child("SelfProfileName", true, false).size.x <= panel.size.x, "较长姓名仍按现有宽度折行")
	_check(panel.find_child("SelfProfileAction", true, false).text == "正在进行：修业", "当前行动使用已有行为词汇")
	var skills: Control = panel.find_child("SelfProfileSkills", true, false)
	view.source.revision = 999
	skills.ordinary_skills_requested.emit("rites")
	skills.martial_requested.emit()
	_check(navigations.size() == 1 and navigations[0].target == "ordinary_skills" and navigations[0].selected_id == "rites", "普通技艺导航与重复输入隔离")
	_check(navigations[0].source.revision == 29, "导航携带读取时的源副本")
	host.queue_free()
	await _settle()
	host = _host()
	panel = Screen.build(host, host, _view(), _closed, _navigate)
	await _settle()
	skills = panel.find_child("SelfProfileSkills", true, false)
	skills.martial_requested.emit()
	_check(navigations.size() == 2 and navigations[1].target == "martial_skills" and navigations[1].selected_id.is_empty(), "武学导航交给既有页面")
	host.queue_free()
	await _settle()
	host = _host()
	panel = Screen.build(host, host, _view(), _closed, _navigate)
	await _settle()
	skills = panel.find_child("SelfProfileSkills", true, false)
	var close: Button = panel.find_child("SelfProfileClose", true, false)
	host.queue_free()
	close.pressed.emit()
	skills.ordinary_skills_requested.emit("martial")
	_check(closes == 2 and navigations.size() == 2, "祖先已排队销毁时迟到输入无效")
	await _settle()
	host = _host()
	panel = Screen.build(host, host, {"ok":false, "ready":false, "code":"StaleSelfProfile"}, _closed, _navigate)
	await _settle()
	_check(panel.find_child("SelfProfileUnavailable", true, false) != null, "失败显示不可用")
	_check(panel.find_child("SelfProfileName", true, false) == null and panel.find_child("SelfProfileSkills", true, false) == null, "失败不展示默认人物与技艺")
	panel.find_child("SelfProfileClose", true, false).pressed.emit()
	_check(closes == 3, "失败仍可返回")
	host.queue_free()
	await _settle()
