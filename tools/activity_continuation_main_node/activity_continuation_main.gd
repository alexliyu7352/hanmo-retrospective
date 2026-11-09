## 三类已接受行动的实际Main节点；每个消费者从原物理银行独立标题恢复，不注入行动或世界。
extends SceneTree
const MainScene=preload("res://main.tscn")
const Strict=preload("res://core/strict_json.gd")
const Study=preload("res://tests/test_study_segments_player_flow.gd")
const Life=preload("res://tests/test_regional_life_player_flow.gd")
var main
var rows: Array=[]
var failures: Array=[]
var receipts: Array=[]
var case_root: String=""
var output: String=""
var binding_path: String=""
var phase: String=""
var action_case: String=""
var binding: Dictionary={}
var profile: String=""
var directory: String=""

## 只有明确启动阶段才运行，Godot check-only不会安装世界。
func _initialize() -> void: call_deferred("run")

## 失败标签与原始结果保留，不通过改玩法输入或隐藏重试获得通过。
func check(value: bool, label: String) -> bool:
	rows.append({"stage":label,"ok":value})
	if not value: failures.append(label); printerr("FAIL ",label)
	return value

## 节点证据只写调用方新目录；物理银行始终由产品Storage写入。
func write_text(name: String, value: String) -> bool:
	var file=FileAccess.open(output.path_join(name),FileAccess.WRITE)
	if file==null: return check(false,"write_"+name)
	file.store_string(value); file.close(); return true

## 保留世界整数和字典顺序，不将解析后的浮点数字当成游戏证据。
func write_json(name: String, value: Variant) -> bool:
	return write_text(name,JSON.stringify(value,"",false,true)+"\n")

## 读回绑定与前一进程证据只供断言，绝不把这些JSON当成restore输入。
func read_json(path: String) -> Dictionary:
	var parsed: Dictionary=Strict.parse_text(FileAccess.get_file_as_string(path))
	if not check(parsed.get("ok",false) and parsed.get("value") is Dictionary,"read_"+path.get_file()): return {}
	return parsed.value

## 一次稳定边界须排空Main读、原生票据和保存入口；帧数不会赠送世界半日。
func settled(host) -> bool:
	var deadline: int=Time.get_ticks_usec()+45000000
	var stable: int=0
	while Time.get_ticks_usec()<deadline and failures.is_empty():
		if host._started and host._resident_mode and not host._advancing and not host._resident_frame_pending and host._storage_intent.is_empty() and host._resident_pending_slot_mode.is_empty() and not host.kernel.is_busy() and not host.kernel.has_pending_reads() and not host._resident_adapter.is_busy() and host._resident_command.is_empty() and host._resident_quote.is_empty() and host._resident_short_action.is_empty():
			stable+=1
			if stable>=3: return check(host._resident_page_error.is_empty(),"stable_page")
		else: stable=0
		await process_frame
	write_json("idle-failure.json",{"message":host._message,"page_error":host._resident_page_error})
	return check(false,"stable_boundary_timeout")

## 原按钮依语义定位；有限预算的确认不能借直接调用adapter或改变Timer节奏替代。
func click(kind: String, target: String="") -> bool:
	return await Life.click(self,main,kind,target)

## 唯一许可结束后才返回；预算、原行动结束或阻断均由原core回执提供。
func stopped() -> bool:
	var deadline: int=Time.get_ticks_usec()+30000000
	while Time.get_ticks_usec()<deadline:
		if not main._resident_adapter.auto_running and not main._resident_adapter.is_busy(): return await settled(main)
		await process_frame
	return check(false,"finite_run_timeout")

## 使用可见整数控件填写本次预算；不修改已接受run和世界钱包。
func budget(half_days: int, money: int) -> bool:
	if not await settled(main): return false
	var ticks: SpinBox=main.find_child("ActivityContinuationHalfDays",true,false)
	var cost: SpinBox=main.find_child("ActivityContinuationMoney",true,false)
	if not check(ticks!=null and cost!=null and ticks.is_visible_in_tree() and cost.is_visible_in_tree(),"actual_budget_controls"): return false
	ticks.value=half_days; cost.value=money
	return await click("activity_begin")

## 当前许可和整世界原件并列保存，冷导出仅作证据不回填世界。
func record(label: String) -> bool:
	var exported: Dictionary=main.kernel.transport().debug_export_state()
	if not check(exported.get("ok",false) and exported.get("json") is String,label+"_actual_world"): return false
	if not write_text(label+"-state.json",exported.json) or not write_json(label+"-source.json",main.kernel.source_identity()) or not write_json(label+"-continuation.json",main._activity_continuation_state()): return false
	if DisplayServer.get_name()!="headless":
		var source: Dictionary=main.kernel.source_identity()
		await process_frame; await RenderingServer.frame_post_draw
		var image: Image=root.get_texture().get_image()
		if not check(not image.is_empty() and image.save_png(output.path_join(label+".png"))==OK,label+"_actual_viewport"): return false
		return check(main.kernel.source_identity()==source,label+"_screenshot_read_only")
	return true

## 只读本节点隔离profile，消费者核全部原物理银行未变。
func bank_hashes() -> Dictionary:
	var values: Dictionary={}
	if DirAccess.dir_exists_absolute(directory):
		for name in DirAccess.get_files_at(directory): values[name]=FileAccess.get_sha256(directory.path_join(name))
	return values

## 实际Main回执只作为顺序证据，监听不会产生请求序号或新半日。
func command_completed(kind: String, result: Dictionary, before: Dictionary, after: Dictionary) -> void:
	receipts.append({"kind":kind,"result":result.duplicate(true),"before":before.duplicate(true),"after":after.duplicate(true)})

## Alpha原庄主先实际走入自庄，课程使用原120钱和原教师，不添加资产或接受记录。
func accept_original_action() -> bool:
	if action_case=="study":
		if not await Study.enroll(self,main,binding.cases.study): return false
		var pending: Dictionary=main.kernel.study_activity()
		var begin: Button=Life.button(main.page,"activity_begin")
		var manual: Button=main.find_child("StudyAdvance",true,false)
		if not check(pending.challenge_pending and pending.completed_lessons==0 and begin!=null and begin.disabled and manual!=null and not manual.disabled,"unanswered_manual_question_stops_auto_but_preserves_late_halfday"): return false
		if not await Study.click(self,main,"advance",{},false): return false
		if not check(main.kernel.study_activity().challenge_pending and main.kernel.study_activity().completed_lessons==1,"manual_late_question_keeps_original_input"): return false
		if not await Study.click(self,main,"study_cancel",{"action_id":pending.action_id}): return false
		if not await click("courses_open"): return false
		var duration: OptionButton=main.find_child("StudyDuration",true,false)
		if not check(duration!=null,"ordinary_duration_control"): return false
		duration.select(1); duration.item_selected.emit(1)
		if not await settled(main): return false
		if not await Study.click(self,main,"study_skill",{"skill_id":"rites","ticks":3,"challenge_mode":"automatic"},false): return false
		return check(main.kernel.study_activity().completed_lessons==1,"ordinary_automatic_original_first_halfday_preserved")
	if not await Study.start_selected(self,main,binding.cases[action_case]): return false
	if action_case=="rest":
		if main.journey_view.opened and not await click("map_close"): return false
		if not await click("move_local","west.estate") or not await click("family_open") or not await click("family_tab","rest"): return false
		return await click("long_rest","estate.riverside")
	if not main.journey_view.opened and not await click("map"): return false
	if not await click("map_select","village.east") or not await click("map_confirm"): return false
	return check(main.kernel.action_context().get("action_kind")=="travel","original_map_action_accepted")

## 观察原Timer已有回调之后的真实grant，再按原菜单；不延长worker、不注入事务或许可。
func request_save_on_grant(observation: Dictionary) -> void:
	if not check(main._resident_adapter._job.get("kind")=="grant","actual_inflight_halfday_before_save"): return
	var menu: Button=Life.button(main.page,"system_menu")
	if not check(menu!=null and not menu.disabled,"inflight_menu_enabled"): return
	menu.pressed.emit()
	var save: Button=Life.button(main.page,"save")
	if not check(save!=null and save.is_visible_in_tree() and not save.disabled,"inflight_save_enabled"): return
	save.pressed.emit(); observation.requested=true

## 保存入口先等稳定画面，原在途半日可完成或撤销；下一份许可不能再启动。
func save_during_grant() -> bool:
	var observation: Dictionary={"requested":false}
	main._resident_adapter._timer.timeout.connect(request_save_on_grant.bind(observation),CONNECT_ONE_SHOT)
	var deadline: int=Time.get_ticks_usec()+10000000
	while Time.get_ticks_usec()<deadline and not observation.requested and failures.is_empty(): await process_frame
	if not check(observation.requested,"original_timer_triggered_save_request"): return false
	if not await Life._wait_slot_window(self,main): return false
	var stable: Dictionary=main.kernel.source_identity()
	if not check(not main._resident_adapter.auto_running and not main._resident_adapter.is_busy() and not main.kernel.is_busy() and not main._advancing,"slot_opens_only_after_inflight_stable_boundary"): return false
	if not Life._confirm_slot(self,main,"manual-1") or not await settled(main): return false
	return check(main.last_storage_result.get("ok",false) and main.kernel.source_identity()==stable,"actual_bank_matches_stable_inflight_result")

## 每类先实际用完一个有限预算；修业另核费用不足的候选完全没有提交。
func produce() -> bool:
	if not check(not DirAccess.dir_exists_absolute(directory),"fresh_profile") or not await accept_original_action(): return false
	main.kernel.command_completed.connect(command_completed)
	var start: Dictionary=main.kernel.source_identity()
	var action: String=main.kernel.action_context().action_id
	var count: int=2 if action_case=="rest" else 1
	var money: int=30 if action_case=="study" else 100 if action_case=="travel" else 0
	if not await budget(count,money) or not await stopped(): return false
	if not check(main.kernel.source_identity().tick==start.tick+count and main.kernel.action_context().action_id==action and not main._resident_adapter.auto_running and not main._rest_auto,"finite_actual_halfdays_preserve_original_action"): return false
	if not check(main._activity_continuation_state().get("stop")=="budget_exhausted","native_budget_stop"): return false
	if not await record("budget-stopped"): return false
	if action_case=="study":
		var before: Dictionary=main.kernel.transport().debug_export_state()
		var sequence: int=main.kernel.command_context().next_request_seq
		if not await budget(1,29) or not await stopped(): return false
		if not check(main._activity_continuation_state().get("stop")=="money_budget_exhausted" and main.kernel.transport().debug_export_state()==before and main.kernel.command_context().next_request_seq==sequence,"money_rejection_zero_world_payment_experience_or_sequence"): return false
	# 再选明确的新预算，begin停驻后从原菜单保存，尚未授予的半日不能偷跑。
	if not await budget(count,money): return false
	var saved_source: Dictionary=main.kernel.source_identity()
	var saved: bool=await save_during_grant() if action_case=="rest" else await Life.save_via_menu(self,main,"manual-1")
	if not saved or not await record("saved"): return false
	var continuation: Dictionary=main._activity_continuation_state()
	var delta: int=main.kernel.source_identity().tick-saved_source.tick
	var remaining: int=continuation.get("max_half_days",0)-continuation.get("committed_half_days",0)
	if not check(not main._resident_adapter.auto_running and delta in ([0,1] if action_case=="rest" else [0]) and remaining==count-delta and continuation.get("committed_half_days",0)==delta,"saved_remaining_budget_counts_only_actual_commits"): return false
	var manifest: Dictionary={"format":"HM-ACTIVITY-CONTINUATION-MAIN/1","producer_pid":OS.get_process_id(),"case":action_case,"profile":profile,"directory":directory,"binding_sha256":FileAccess.get_sha256(binding_path),"banks":bank_hashes(),"source":main.kernel.source_identity(),"continuation":continuation,"remaining":remaining,"action_id":action}
	return write_json("save-manifest.json",manifest)

## 新进程必须独立标题恢复原银行，展示同一剩余额度且不自动启动；随后本人选自动或逐半日。
func consume() -> bool:
	var producer: String=case_root.path_join("producer")
	var manifest: Dictionary=read_json(producer.path_join("save-manifest.json"))
	if not check(bank_hashes()==manifest.banks,"original_physical_banks"): return false
	if not await Life.continue_empty_title(self,main,binding.cases[action_case],"manual-1",false): return false
	if action_case=="rest":
		if main.journey_view.opened and not await click("map_close"): return false
		if not await click("family_open"): return false
	var restored: Dictionary=main._activity_continuation_state()
	if not check(not main._resident_adapter.auto_running and restored.get("stop")=="restored" and restored.get("max_half_days",0)-restored.get("committed_half_days",0)==manifest.remaining and main.kernel.transport().debug_export_state().json==FileAccess.get_file_as_string(producer.path_join("saved-state.json")),"fresh_title_exact_world_and_stopped_remaining_budget"): return false
	var before: Dictionary=main.kernel.source_identity()
	await create_timer(1.4).timeout
	if not check(main.kernel.source_identity()==before and not main._resident_adapter.auto_running,"restored_focus_time_never_restarts_permission"): return false
	if not await record("restored"): return false
	if phase=="consume-auto":
		if not await click("activity_resume") or not await stopped(): return false
	else:
		for step in range(manifest.remaining):
			if not await click("advance"): return false
	if not check(main.kernel.source_identity().tick==before.tick+manifest.remaining,"explicit_continuation_exact_remaining_actual_halfdays"): return false
	if not await record("completed") or not check(bank_hashes()==manifest.banks,"consumers_never_rewrite_original_banks"): return false
	if phase=="consume-manual":
		if not check(FileAccess.file_exists(case_root.path_join("consume-auto/completed-state.json")),"automatic_reference_available_after_independent_manual_completion"): return false
		return check(FileAccess.get_file_as_string(output.path_join("completed-state.json"))==FileAccess.get_file_as_string(case_root.path_join("consume-auto/completed-state.json")),"same_manual_halfday_sequence_exact_whole_world")
	return true

## 最终入口须绑定中央受审源码、真实库和已有唯一Main stage；准备与解析不算玩法执行。
func run() -> void:
	for arg in OS.get_cmdline_user_args():
		if arg.begins_with("--phase="): phase=arg.trim_prefix("--phase=")
		elif arg.begins_with("--case="): action_case=arg.trim_prefix("--case=")
		elif arg.begins_with("--case-root="): case_root=arg.trim_prefix("--case-root=").simplify_path().trim_suffix("/")
		elif arg.begins_with("--binding="): binding_path=arg.trim_prefix("--binding=")
	if phase not in ["produce","consume-auto","consume-manual"] or action_case not in ["travel","rest","study"] or not case_root.is_absolute_path(): quit(2); return
	output=case_root.path_join("producer" if phase=="produce" else phase)
	if not DirAccess.dir_exists_absolute(output) or FileAccess.file_exists(output.path_join("result.json")): quit(2); return
	binding=read_json(binding_path)
	if not check(binding.get("native_artifact_status")=="BOUND_REVIEWED_ARTIFACT_NOT_EXECUTED" and binding.get("source_commit")==binding.get("reviewed_source_commit") and FileAccess.get_sha256(binding.native_extension.path)==binding.native_extension.sha256,"actual_reviewed_library"): quit(2); return
	profile="bounded-activity-"+case_root.sha256_text().left(12)
	ProjectSettings.set_setting("application/config/storage_profile",profile)
	directory=ProjectSettings.globalize_path("user://profiles/"+profile+"/saves")
	if not check(directory.begins_with(case_root+"/") and ClassDB.class_exists("HanmoResidentBridge"),"owned_profile_real_native"): quit(2); return
	root.size=Vector2i(1280,900); main=MainScene.instantiate(); root.add_child(main)
	var passed: bool=await produce() if phase=="produce" else await consume()
	write_json("receipts.json",receipts)
	write_json("result.json",{"ok":passed and failures.is_empty(),"phase":phase,"case":action_case,"pid":OS.get_process_id(),"source_commit":binding.source_commit,"native_source_commit":binding.native_source_commit,"profile":profile,"rows":rows,"failures":failures,"page_error":main._resident_page_error,"message":main._message,"actual_main_scene":true,"actual_native_bridge":true,"gui_pixels_accepted":false})
	main.queue_free(); await process_frame
	print("ACTIVITY_CONTINUATION_MAIN ",action_case," ",phase," OK=",passed and failures.is_empty()); quit(0 if passed and failures.is_empty() else 1)
