## 实际Main导航与页面装配的窄验收；查询探针只供边界输入，不冒充原生银行或世界回放。
extends SceneTree
const Main = preload("res://ui/main.gd")

## 使用真实Main入口，仅把异步绘制调度计数化，显式装配仍调用正式Screen与读取计划。
class EdgeMain extends "res://ui/main.gd":
	var renders: int = 0
	## 最小舞台避免启动标题、音频或存储；所有导航函数沿正式Main继承。
	func _ready() -> void:
		set_process(false)
		add_child(journey_view)
		add_child(_ui_font_scale)
		add_child(storage)
		page = VBoxContainer.new()
		add_child(page)
		page.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
		var font = SystemFont.new()
		font.font_names = PackedStringArray(["Noto Sans CJK SC", "DejaVu Sans"])
		theme = Art.theme(font)
	## 只记录真实入口请求的重绘次数；不把探针当作后台原生进程。
	func _render() -> void:
		renders += 1
	## 音频节点未入树，结束时单独释放其已分配子节点。
	func release_audio() -> void:
		_ambience.music.free()
		_ambience.effect.free()
		_ambience.free()

## 返回严格有限DTO与身份；不持有世界模拟，命令与整态调用只计失败证据。
class Probe extends RefCounted:
	var source: Dictionary = {"world_id":"0123456789abcdef0123456789abcdef", "execution_epoch":3, "revision":17, "tick":22}
	var actor: String = "actor:one"
	var display_name: String = "当前本人甲"
	var battle: String = ""
	var failure: String = ""
	var commands: int = 0
	var snapshots: int = 0
	## 复制同一当前来源，允许测试只改变epoch或当前人物制造迟到输入。
	func source_identity() -> Dictionary:
		return source.duplicate(true)
	## 只提供已发布人物与revision，与真正adapter使用的边界一致。
	func command_context(_actor: String = "") -> Dictionary:
		return {"ok":true, "actor_id":actor, "revision":source.revision}
	## 五维及状态作为显式夹具输入，姓名和源随探针当前身份变化。
	func self_profile() -> Dictionary:
		if not failure.is_empty(): return {"ok":false, "code":failure}
		var values: Array = []
		for id in ["leadership", "martial", "intellect", "administration", "charisma"]: values.append({"attribute_id":id, "value":41})
		return {"kind":"self_profile", "actor_id":actor, "display_name":display_name, "age_years":-1, "attributes":values, "skills":[],
			"roles":[{"role_id":"role:held", "place_id":"place:current", "organization_id":""}],
			"status":{"alive":true, "available":true, "hp":86, "fatigue":7, "injury":2, "action_kind":"study_skill"},
			"source":{"world_id":source.world_id, "execution_epoch":source.execution_epoch, "revision":source.revision}}
	## 注入个人战信号验证既有入口拒绝，不生成虚拟战斗。
	func personal_battle_view() -> Dictionary:
		return {"battle_id":battle}
	## 本项不涉及军战，边界明确无活跃页。
	func military_battle_page() -> Dictionary:
		return {"active":false}
	## 本项不涉及围城，边界明确无活跃页。
	func siege_page() -> Dictionary:
		return {"siege_id":""}
	## 正式读取计划在本人档案前只需手动窗口标志。
	func demand_view(_kind: String) -> Dictionary:
		return {"ok":true}
	## 场所只供真实Screen选择背景，不参与规则或移动。
	func presentation_location() -> Dictionary:
		return {"kind":"Settlement", "place_id":"place:current"}
	## 顶栏来自与本人页相同的当前人物字段。
	func hud_view(_battle: String = "") -> Dictionary:
		return {"day_index":11, "half_day":0, "display_name":display_name, "balance":12, "hp":86}
	## 如果本人读页触及命令，该计数必须让最终断言失败。
	func submit(_command: Dictionary) -> Dictionary:
		commands += 1
		return {"ok":false, "code":"UnexpectedCommand"}
	## 如果本人读页退回整态，该计数必须让最终断言失败。
	func snapshot() -> Dictionary:
		snapshots += 1
		return {}

var checks: int = 0
var failures: int = 0

## 按帧延后，使真实容器与已排队节点释放稳定。
func _init() -> void:
	call_deferred("_run")

## 所有断言保留失败原因和非零退出码。
func _check(condition: bool, description: String) -> void:
	checks += 1
	if not condition:
		failures += 1
		printerr("FAIL: " + description)

## 只等待Godot布局帧，不运行原生worker或世界时钟。
func _settle() -> void:
	for frame in range(5): await process_frame

## 显式调用正式准备与装配，测试没有复制Main的源校验或Screen分支。
func _compose(main: EdgeMain) -> Control:
	_check(Main.ResidentPageRequests.prepare(main), "正式读取计划完成本人投影")
	Main.Screen.render(main)
	await _settle()
	return main.find_child("SelfProfileScreen", true, false)

## 校验当前本人、失败重建、同revision换epoch、换人及旧节点晚到事件。
func _run() -> void:
	root.size = Vector2i(800, 600)
	root.content_scale_size = Vector2i(800, 600)
	var main = EdgeMain.new()
	root.add_child(main)
	main.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	var probe = Probe.new()
	main.kernel = probe
	main._resident_mode = true; main._started = true
	main._data = {"places":{"place:current":{"visual_tag":"city"}}, "roles":{"role:held":{"name_key":"role.name"}}, "texts":{"role.name":"当前实际职事"}}
	main._rest_auto = true; main.journey_view.auto_running = true
	main._open_self_profile()
	_check(main._self_profile_open and not main._rest_auto and not main.journey_view.auto_running, "真实打开只停止界面自动推进")
	var panel: Control = await _compose(main)
	_check(panel != null, "正式Screen装配本人档案")
	_check(panel.find_child("SelfProfileName", true, false).text == probe.display_name, "姓名与当前DTO相符")
	_check(panel.find_child("SelfProfileRoles", true, false).text == "当前职事：当前实际职事", "职事使用同目录名称")
	_check(panel.find_child("SelfProfileAge", true, false).text == "年龄未记录", "未知年龄不使用作者模板")
	_check(Rect2(Vector2.ZERO, Vector2(800, 600)).encloses(panel.get_global_rect()), "实际Main的800×600本人页不超出窗口")
	print("PROFILE_MAIN_PANEL_RECT=", panel.get_global_rect(), " STAGE=", panel.get_parent().size)
	var source: Dictionary = main._self_profile_view.source.duplicate(true)
	var renders: int = main.renders
	main._navigate_self_profile("unavailable_target", "", source)
	_check(main.renders == renders + 1 and main._self_profile_open, "未知目标拒绝后请求重绘")
	panel = await _compose(main)
	var stale_close: Button = panel.find_child("SelfProfileClose", true, false)
	var stale_skills: Control = panel.find_child("SelfProfileSkills", true, false)
	probe.source.execution_epoch += 1
	renders = main.renders
	stale_skills.martial_requested.emit()
	_check(main.renders == renders + 1 and main._self_profile_view.is_empty() and main._self_profile_open and not main._growth_open, "同revision换epoch拒绝旧导航并请求重建")
	_check(Main.ResidentPageRequests.prepare(main), "过期源后正式读取计划重新取得本人")
	Main.Screen.render(main)
	renders = main.renders
	_check(is_instance_valid(stale_close) and is_instance_valid(stale_skills), "旧节点在帧末释放前确实仍存在")
	stale_close.pressed.emit()
	stale_skills.ordinary_skills_requested.emit("rites")
	_check(main.renders == renders and main._self_profile_open and not main._ordinary_skills_open, "重建后旧页事件不能打开或关闭新页")
	await _settle()
	panel = main.find_child("SelfProfileScreen", true, false)
	probe.actor = "actor:two"; probe.display_name = "当前本人乙"
	renders = main.renders
	main._navigate_self_profile("ordinary_skills", "rites", main._self_profile_view.source)
	_check(main.renders == renders + 1 and main._self_profile_open and main._self_profile_view.is_empty(), "同源下当前人物不符也拒绝导航")
	panel = await _compose(main)
	_check(panel.find_child("SelfProfileName", true, false).text == "当前本人乙", "重开显示新的当前人物")
	probe.failure = "UnsupportedNativeCapability"
	panel = await _compose(main)
	_check(panel.find_child("SelfProfileUnavailable", true, false) != null and panel.find_child("SelfProfileName", true, false) == null, "读取拒绝重建为无旧人物的失败页")
	panel.find_child("SelfProfileClose", true, false).pressed.emit()
	_check(not main._self_profile_open and main._self_profile_view.is_empty(), "失败页仍可沿真实Main返回")
	probe.failure = ""
	main._open_self_profile(); panel = await _compose(main)
	renders = main.renders
	probe.battle = "battle:blocked"
	main._navigate_self_profile("ordinary_skills", "rites", main._self_profile_view.source)
	_check(main._self_profile_open and not main._ordinary_skills_open and main.renders == renders + 1, "已有入口拒绝目标时仍请求重绘")
	probe.battle = ""
	main._close_self_profile()
	_check(probe.commands == 0 and probe.snapshots == 0, "打开关闭和拒绝路径不调用世界命令或整态")
	main.release_audio(); main.queue_free(); await _settle()
	print("SELF_PROFILE_MAIN_EDGE_CHECKS=%d FAILURES=%d HEADLESS=%s" % [checks, failures, DisplayServer.get_name() == "headless"])
	quit(0 if failures == 0 else 1)
