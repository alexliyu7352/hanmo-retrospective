## 原Main、真实resident worker与原GD规则对照；只走一段实际行旅，不跑规模矩阵。
extends SceneTree
const Main=preload("res://ui/main.gd")
const Kernel=preload("res://core/world_kernel.gd")
var checks: int=0
var failures: int=0
var boundary_index: int=0
var evidence_directory: String=""
func check(value: bool,label: String) -> bool:
	checks+=1
	if not value: failures+=1; push_error(label)
	return value
func _initialize() -> void: call_deferred("_run")
func _idle(main: Node) -> bool:
	return main._storage_intent.is_empty() and not main._advancing and not main._resident_frame_pending and not main.kernel.is_busy() and not main._resident_adapter.is_busy() and main._resident_command.is_empty() and main._resident_quote.is_empty()
func wait_idle(main: Node) -> bool:
	var deadline: int=Time.get_ticks_usec()+30000000
	while Time.get_ticks_usec()<deadline:
		if _idle(main): return true
		await process_frame
	return false
func wait_slot_window(main: Node) -> bool:
	var deadline: int=Time.get_ticks_usec()+15000000
	while Time.get_ticks_usec()<deadline:
		if is_instance_valid(main._slot_dialog) and main._slot_dialog.visible: return true
		await process_frame
	return false
func state_matches(main: Node, legacy: RefCounted) -> bool:
	var actual: Dictionary=main.kernel.transport().debug_export_state()
	if not actual.get("ok",false): return false
	var expected: String=JSON.stringify(legacy.snapshot(),"",false,true)
	# Godot JSON.parse把整数变成FLOAT，不能拿解析后的Dictionary直接比较原typed世界。
	# 原native序列化与原GD完整序列化字节相等，含数值标签和所有字典/数组顺序。
	boundary_index+=1
	if not evidence_directory.is_empty():
		FileAccess.open(evidence_directory.path_join("boundary-%d-expected.json"%boundary_index),FileAccess.WRITE).store_string(expected)
		FileAccess.open(evidence_directory.path_join("boundary-%d-actual.json"%boundary_index),FileAccess.WRITE).store_string(actual.json)
	print("MAIN_BOUNDARY=",boundary_index," EXACT_BYTES=",actual.json==expected," EXPECTED_SHA=",expected.sha256_text()," ACTUAL_SHA=",str(actual.json).sha256_text())
	return actual.json==expected
func submit_legacy(legacy: RefCounted, kind: String, params: Dictionary={}) -> Dictionary:
	var c: Dictionary=legacy.command_context()
	return legacy.submit({"kind":kind,"params":params,"actor_id":c.actor_id,"request_seq":c.next_request_seq,"expected_revision":c.revision})
func _run() -> void:
	for arg in OS.get_cmdline_user_args():
		if arg.begins_with("--evidence="): evidence_directory=arg.trim_prefix("--evidence=")
	if not evidence_directory.is_empty(): DirAccess.make_dir_recursive_absolute(evidence_directory)
	ProjectSettings.set_setting("development/native_resident_world_enabled",true)
	ProjectSettings.set_setting("application/config/storage_profile","resident-main-test-"+str(Time.get_ticks_usec()))
	if not check(ClassDB.class_exists("HanmoResidentBridge"),"真实resident扩展已注册"): quit(1); return
	var catalog: String="res://data/generated/alpha_life.hmcatalog.json"
	var legacy=Kernel.new(); var setup: Dictionary=legacy.new_game("scenario.alpha_life",catalog)
	if not check(setup.get("ok",false),"原GD真实Alpha世界建立"): quit(1); return
	var main=Main.new(); root.add_child(main)
	var installed: Dictionary=main.start_content("scenario.alpha_life",catalog)
	if not check(installed.get("ok",false),"原Main一次冷安装进入resident模式"): main.queue_free(); quit(1); return
	if not check(await wait_idle(main),"完整有限frame已发布"): main.queue_free(); quit(1); return
	check(main._resident_mode and main.find_child("WorldMapCanvas",true,false)!=null,"原Main实际地图控件已装配")
	check(state_matches(main,legacy),"初始完整世界与原GD相等")
	main._map_select("village.east")
	if not check(await wait_idle(main),"选点后真实纯报价和原Canvas完成"): main.queue_free(); quit(1); return
	check(main._map_quote().get("ok",false),"真实路线报价可确认")
	main._map_confirm()
	var started: Dictionary=submit_legacy(legacy,"travel",{"destination":"village.east","via":[]})
	check(started.get("ok",false),"原GD启程成功")
	if not check(await wait_idle(main),"原Main启程回执和新frame完成"): main.queue_free(); quit(1); return
	check(state_matches(main,legacy),"Main启程只经过同一core且完整世界精确")
	var before: int=main.kernel.source_identity().tick
	main._toggle_auto_travel()
	var deadline: int=Time.get_ticks_usec()+15000000
	while main.kernel.source_identity().tick==before and Time.get_ticks_usec()<deadline: await process_frame
	main._stop_journey_auto()
	check(await wait_idle(main),"真实1.2秒许可完成后阅读取消已排空")
	var advanced: Dictionary=submit_legacy(legacy,"advance")
	check(advanced.get("ok",false) and main.kernel.source_identity().tick==before+1,"实际仅提交一个世界半日")
	check(state_matches(main,legacy),"真实Main自动半日保留完整原世界变化")
	var frozen: Dictionary=main.kernel.transport().debug_export_state()
	main.journey_view.step_requested.emit(); main._auto_step()
	await create_timer(.05).timeout
	check(main.kernel.transport().debug_export_state()==frozen,"旧Main Timer回调不能暗中补半日")
	check(not main._resident_adapter.auto_running,"阅读后许可已终止")
	var saved_identity: Dictionary=main.kernel.source_identity()
	main._save()
	check(await wait_slot_window(main),"原Main保存槽窗口由真实物理列表打开")
	if is_instance_valid(main._slot_dialog):
		main._select_slot({"slot":"manual-1","generation":0,"ok":true,"new":true},main._slot_dialog,main._slot_epoch)
		main._confirm_slot(main._slot_dialog,main._slot_epoch)
		check(await wait_idle(main) and main.last_storage_result.get("ok",false) and main._save_generation==1,"原Main真实冷验证后提升第一银行并回读")
		check(main.kernel.source_identity()==saved_identity,"保存不改变原生身份、tick或revision")
		main._load()
		check(await wait_slot_window(main),"原Main加载同一真实resident槽")
		if is_instance_valid(main._slot_dialog):
			main._select_slot({"slot":"manual-1","generation":1,"ok":true,"new":false},main._slot_dialog,main._slot_epoch)
			main._confirm_slot(main._slot_dialog,main._slot_epoch)
			check(await wait_idle(main) and main.last_storage_result.get("ok",false),"原Main接收本次成功restore的跨epoch完成")
			var restored: Dictionary=main.kernel.source_identity()
			check(restored.world_id==saved_identity.world_id and restored.revision==saved_identity.revision and restored.tick==saved_identity.tick and restored.execution_epoch>saved_identity.execution_epoch,"恢复持久身份不变且执行epoch更新")
			check(state_matches(main,legacy),"原Main物理槽恢复后完整世界仍与原GD精确")
	main.queue_free(); await process_frame
	print("RESIDENT_MAIN_JOURNEY_CHECKS=",checks," FAILURES=",failures," GUI_PIXELS_ACCEPTED=false")
	quit(0 if failures==0 else 1)
