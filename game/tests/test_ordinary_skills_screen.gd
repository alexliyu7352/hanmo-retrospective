## 真实Main菜单和普通技艺读页窄验收；可用已安装青川1.3跑实际课程，默认Alpha验证通用入口。
extends SceneTree
const Main=preload("res://ui/main.gd")
## 计数只包裹原Kernel边界并保留同一后端；不代替真实世界、命令或查询结果。
class ReadProbe extends "res://core/world_kernel.gd":
	var snapshot_calls: int=0
	var command_calls: int=0
	var blocked_route: String=""
	## 只为入口拒绝门提供正在战斗的信号；正常玩家流程仍返回真实查询。
	func personal_battle_view() -> Dictionary:
		return {"battle_id":"blocked"} if blocked_route=="personal" else super.personal_battle_view()
	## 本测试信号不冒充会战规则或实际战场页面。
	func military_battle_page(id: String="") -> Dictionary:
		return {"active":true} if blocked_route=="military" else super.military_battle_page(id)
	## 本测试信号只核围城期间不能把总览标志延后带出战场。
	func siege_page() -> Dictionary:
		return {"siege_id":"blocked"} if blocked_route=="siege" else super.siege_page()
	## 记录整态读取，返回原实现供既有场所控件正常使用。
	func snapshot() -> Dictionary:
		snapshot_calls+=1; return super.snapshot()
	## 记录命令，所有提交仍经正式Kernel事务。
	func submit(command: Dictionary) -> Dictionary:
		command_calls+=1; return super.submit(command)
var main
var checks: int=0
var failures: int=0
var capture_directory: String=""

## 延后建立真实窗口；截图位置由本次验收显式传入。
func _init() -> void:
	for arg in OS.get_cmdline_user_args():
		if arg.begins_with("--capture="): capture_directory=arg.trim_prefix("--capture=")
	call_deferred("run_checks")

## 保留断言与非零失败状态，不能把控件存在当作世界命令通过。
func check(value: bool, label: String) -> bool:
	checks+=1
	if not value: failures+=1; printerr("FAIL: ",label)
	return value

## 让正式递延命令、重绘和布局完成，不另开自动世界时钟。
func settle() -> void:
	for frame in range(6): await process_frame

## 每次从活界面查找节点，防止测试误点重绘前的控件。
func control(name: String) -> Control:
	return main.find_child(name,true,false)

## 使用既有按钮语义查真实入口，不调用测试专用产品方法。
func kind_button(kind: String) -> Button:
	for button in main.find_children("*","Button",true,false):
		if button.get_meta("command_kind","")==kind and not button.disabled: return button
	return null

## 图形验收发送真实鼠标事件；无图形仅验证同一按钮回调，禁用控件不得强行执行。
func click(button: Button) -> void:
	if not check(button!=null and not button.disabled,"真实按钮可操作"): return
	if DisplayServer.get_name()=="headless": button.pressed.emit(); await settle(); return
	var ancestor: Node=button.get_parent()
	while ancestor!=null:
		if ancestor is ScrollContainer: ancestor.ensure_control_visible(button)
		ancestor=ancestor.get_parent()
	await settle()
	var point: Vector2=button.get_global_rect().get_center()
	var motion=InputEventMouseMotion.new(); motion.position=point; motion.global_position=point; Input.parse_input_event(motion)
	var down=InputEventMouseButton.new(); down.button_index=MOUSE_BUTTON_LEFT; down.pressed=true; down.position=point; down.global_position=point; Input.parse_input_event(down)
	await process_frame
	var up=InputEventMouseButton.new(); up.button_index=MOUSE_BUTTON_LEFT; up.position=point; up.global_position=point; Input.parse_input_event(up)
	await settle()

## 截图取真实视口；无图形运行不伪造图像或声明视觉验收。
func capture(label: String) -> void:
	if capture_directory.is_empty() or DisplayServer.get_name()=="headless": return
	await RenderingServer.frame_post_draw
	DirAccess.make_dir_recursive_absolute(capture_directory)
	check(root.get_texture().get_image().save_png(capture_directory.path_join(label+".png"))==OK,"保存实际界面 "+label)

## 从实际菜单进入、换项、返回与再次开启；读页不新取snapshot，不提交任何命令。
func run_checks() -> void:
	main=Main.new(); root.add_child(main); main.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT); await settle()
	var fixture: Dictionary={"scenario_id":"scenario.alpha_life","catalog_path":"res://data/generated/alpha_life.hmcatalog.json"}
	if FileAccess.file_exists("res://tests/ordinary_skills_runtime.json"): fixture=JSON.parse_string(FileAccess.get_file_as_string("res://tests/ordinary_skills_runtime.json"))
	if not check(main.start_content(fixture.scenario_id,fixture.catalog_path).ok,"正式Main启动既有作者内容"): finish(); return
	await settle()
	var probe=ReadProbe.new(); probe._backend=main.kernel._backend; main.kernel=probe
	var before: Dictionary=probe.snapshot()
	await click(kind_button("system_menu"))
	probe.snapshot_calls=0; probe.command_calls=0
	await click(control("OrdinarySkillsMenuOpen"))
	if not check(control("OrdinarySkillsScreen")!=null,"实际菜单打开普通技艺页"): finish(); return
	check(control("OrdinarySkillsRows").get_child_count()==10 and control("OrdinarySkillsEmpty")!=null,"本人十项与无结课记录")
	await click(control("OrdinarySkill_rites")); check(control("OrdinarySkillTitle").text=="礼法","真实选择显示礼法")
	await click(control("OrdinarySkill_transport")); check(control("OrdinarySkillTitle").text=="运输","滚动末项可以切换")
	check(probe.snapshot_calls==0 and probe.command_calls==0,"打开与换项不新取snapshot或提交命令")
	check(probe.snapshot()==before,"所有读页操作保持整个世界、序号和时钟不变")
	await capture("01-overview")
	# 同步调用旧按钮模拟重复输入，关闭后旧节点不能把新页面再打开。
	var stale: Button=control("OrdinarySkill_rites")
	control("OrdinarySkillsClose").pressed.emit(); stale.pressed.emit(); await settle()
	check(control("OrdinarySkillsScreen")==null and not main._ordinary_skills_open,"返回后旧选择不重开页面")
	await click(control("OrdinarySkillsOpen")); await click(control("OrdinarySkillsClose"))
	check(probe.snapshot()==before,"重复开启与返回不改世界")
	if fixture.scenario_id=="qingstudy::scenario.study_routes": await course_flow()
	var gate_stage=Control.new(); main.add_child(gate_stage)
	main.Screen.hud(main,gate_stage,probe.hud_view(),false)
	var blocked_menu: Button=gate_stage.find_child("OrdinarySkillsMenuOpen",true,false)
	check(blocked_menu!=null and blocked_menu.disabled and blocked_menu.tooltip_text.contains("战斗"),"战斗HUD明确禁用读页入口并解释原因")
	gate_stage.free()
	for route in ["personal","military","siege"]:
		probe.blocked_route=route; main._open_ordinary_skills("martial")
		check(not main._ordinary_skills_open,"直接或迟到入口不把总览带出战场："+route)
	probe.blocked_route=""
	# 原生实验入口仅验证Main的拒绝边界；此处不安装原生后端，不冒充原生功能验收。
	var native_before: Dictionary=probe.snapshot(); probe.snapshot_calls=0; probe.command_calls=0
	main._resident_mode=true; main._open_ordinary_skills("martial")
	check(not main._ordinary_skills_open and main._message.contains("尚未接入"),"原生实验模式明确不可用，不伪造空页面")
	main._resident_mode=false; main._resident_frame_pending=false
	check(probe.snapshot_calls==0 and probe.command_calls==0 and probe.snapshot()==native_before,"未支持入口不退回snapshot或改写世界")
	finish()

## 青川第四章沿实际课程按钮开课、读未结经验、完成与主动取消；仅两门短课，不跑十技艺矩阵。
func course_flow() -> void:
	for ending in ["complete","cancel"]:
		await click(kind_button("courses_open"))
		await click(kind_button("study_skill"))
		var state: Dictionary=main.kernel.snapshot(); var actor_id: String=state.player_actor_id
		var action_id: String=state.actors[actor_id].action_id
		if not check(not action_id.is_empty(),"真实报名后仍有待结课半日"): return
		var xp: int=state.actors[actor_id].skills.martial
		await click(kind_button("system_menu")); await click(control("OrdinarySkillsMenuOpen")); await click(control("OrdinarySkill_martial"))
		var view: Dictionary=main.kernel.ordinary_skills("martial")
		check(view.course_records.size()==(0 if ending=="complete" else 1) and state.facts[action_id].phase=="started","进行中课程不冒充已获得成果")
		check(control("OrdinarySkillProgress").text.contains("经验 %d"%xp),"界面显示未结课前真实经验")
		await click(control("OrdinarySkillsClose"))
		await click(control("StudyAdvance" if ending=="complete" else "StudyCancel"))
		state=main.kernel.snapshot()
		check(state.actors[actor_id].action_id.is_empty(),"正式完成或取消释放课程")
		await click(control("OrdinarySkillsOpen")); await click(control("OrdinarySkill_martial"))
		view=main.kernel.ordinary_skills("martial")
		var record: Dictionary=view.course_records[0]
		check(record.action_id==action_id and record.success==(ending=="complete") and record.completed_lessons==(2 if ending=="complete" else 1),"面板读取这次真实完整或中断记录")
		check(record.xp_after==state.actors[actor_id].skills.martial and record.xp_gained>0,"实际成长与当前权威经验一致")
		check(control("OrdinarySkillRecords").get_child_count()==view.course_records.size(),"所有已结课来源可见")
		await capture("02-"+ending)
		await click(control("OrdinarySkillsClose"))

## 释放真实Main和声音节点后报告实际范围，图形与回调运行分开记录。
func finish() -> void:
	main.queue_free(); await settle()
	print("ORDINARY_SKILLS_SCREEN_CHECKS=%d FAILURES=%d GUI=%s"%[checks,failures,DisplayServer.get_name()!="headless"])
	quit(0 if failures==0 else 1)
