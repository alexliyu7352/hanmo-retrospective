## 已冻结源码上的真实Main边界门；仅正常恢复已有银行并提交一轮，其余操作只消费公开回放接口。
extends "../formal_appearance_main_node/main_entry.gd"
const BoundaryProbe=preload("source_boundaries.gd")

var playback_world: String=""
var playback_source: Dictionary={}
var playback_context: Dictionary={}
var playback_receipts: String=""
var witnesses: Array=[]
var stale_callbacks: int=0
var planning_page: Dictionary={}
var settled_page: Dictionary={}
var expected_reload_generation: int=-1

## 当前页必须真实挂树且公开帧接口存在；不以测试替身补齐缺失产品能力。
func battle_view():
	var view=main.page.find_child("PersonalBattleScreen",true,false)
	if not check(view!=null and view.is_inside_tree() and view.is_visible_in_tree() and view.has_method("playback_frame"),"live_personal_playback_page"): return null
	return view

## 仅读取生产Screen的公开帧快照；无效或缺接口直接失败。
func frame() -> Dictionary:
	var view=battle_view()
	return {} if view==null else view.playback_frame().duplicate(true)

## 使用冻结后核实的真实控件元数据；暂停/播放与倍速均沿原pressed信号。
func playback_press(action: String) -> bool:
	var pair: Array=binding.controls[action]
	return press(main.page,str(pair[0]),str(pair[1]))

## 从原worker取得有效typed checkpoint；证据从不回装世界或写入产品槽。
func checkpoint(label: String) -> bool:
	var source: Dictionary=main.kernel.source_identity()
	var bridge=main.kernel.transport()
	var accepted: Dictionary=bridge.enqueue_live_operation({"kind":"checkpoint","source":source,"payload":{}})
	if not check(accepted.get("ok",false),label+"_checkpoint_accepted"): return false
	var deadline: int=Time.get_ticks_usec()+30000000
	while Time.get_ticks_usec()<deadline:
		var reply: Dictionary=bridge.poll_live_operation(accepted.reference)
		if not reply.get("ok",false): return check(false,label+"_checkpoint_poll")
		if reply.get("ready",false):
			var result: Dictionary=reply.get("result",{})
			if not check(result.get("ok",false) and result.get("checkpoint_bytes") is PackedByteArray and reply.get("current_identity")==source,label+"_real_checkpoint"): return false
			var path: String=output.path_join(label+".hmcheckpoint")
			var file=FileAccess.open(path,FileAccess.WRITE)
			if file==null: return check(false,label+"_checkpoint_write")
			file.store_buffer(result.checkpoint_bytes); file.close()
			return check(FileAccess.get_sha256(path)==result.get("validation",{}).get("candidate_sha256"),label+"_checkpoint_hash")
		await process_frame
	return check(false,label+"_checkpoint_timeout")

## 比较完整原文、RNG、全部命令序号与真实信号回执字节；任何UI动作都不能推进世界。
func unchanged(label: String) -> bool:
	var raw: String=world()
	var actual: Variant=JSON.parse_string(raw)
	var baseline: Variant=JSON.parse_string(playback_world)
	var same: bool=raw==playback_world and main.kernel.source_identity()==playback_source and main.kernel.command_context()==playback_context and encode(commands)==playback_receipts
	var detail: Dictionary={"label":label,"frame":frame(),"world_sha256":raw.sha256_text(),"receipts_sha256":encode(commands).sha256_text(),"same_bytes":same}
	witnesses.append(detail); write("playback-witnesses.json",encode(witnesses))
	if not check(actual is Dictionary and baseline is Dictionary,label+"_world_objects"): return false
	return check(same,label+"_world_source_context_receipts_exact") and check(actual.rng==baseline.rng and actual.request_sequences==baseline.request_sequences and actual.tick==baseline.tick,label+"_rng_sequences_tick_exact") and check(not main._resident_adapter.auto_running,label+"_no_auto_permission")

## 槽窗口可在标题或活战斗中使用；不复制原消费者仅允许标题的断言。
func confirm_slot() -> bool:
	var deadline: int=Time.get_ticks_usec()+30000000
	while Time.get_ticks_usec()<deadline:
		if is_instance_valid(main._slot_dialog) and main._slot_dialog.visible:
			if expected_reload_generation>=0:
				var current=battle_view()
				if current==null or not arm_stale_callbacks(current,"successful_load",expected_reload_generation): return false
			return press(main._slot_dialog,"slot_select","manual-1") and press(main._slot_dialog,"slot_confirm")
		await process_frame
	return check(false,"slot_dialog_timeout")

## 原节点退出树时重放其真实按钮信号，检验旧页回调在销毁前已失效，不猜私有函数。
func arm_stale_callbacks(view: Node, label: String, expected_generation: int=-1) -> bool:
	var old_buttons: Array=[]
	for action in ["pause","speed_1","skip"]:
		var pair: Array=binding.controls[action]
		var button: Button=control(view,"command_kind",str(pair[0]),str(pair[1]))
		if not check(button!=null,label+"_old_control_"+action): return false
		old_buttons.append(button)
	view.tree_exiting.connect(func():
		var stored: String=encode(main.get_meta("personal_battle_playback_state",{}))
		var receipts: String=encode(commands)
		if expected_generation>=0:
			check(int(main.get_meta("personal_battle_generation",0))==expected_generation,label+"_callbacks_after_successful_adoption")
		for button in old_buttons:
			if is_instance_valid(button): button.pressed.emit(); stale_callbacks+=1
		check(encode(main.get_meta("personal_battle_playback_state",{}))==stored and encode(commands)==receipts,label+"_stale_callbacks_leave_ui_and_receipts")
	,CONNECT_ONE_SHOT)
	return true

## 等真实一轮新页出现立即暂停，避免冷checkpoint耗时吞掉短回放；只结算这一次。
func settle_one_round() -> bool:
	var view=battle_view()
	if view==null or not check(view.battle.phase=="planning" and view.battle.controller_id==binding.player_actor_id,"restored_actual_planning_controller"): return false
	planning_page=view.page_input.duplicate(true)
	if not write("actual-planning-page.json",encode(planning_page)): return false
	var battle_id: String=str(view.battle.id)
	var old_round: int=int(view.battle.round)
	var old_revision: int=int(view.page_input.revision)
	var old_world: String=world()
	var old_context: Dictionary=main.kernel.command_context().duplicate(true)
	if not press(main.page,"battle_action","defend") or not await settled(): return false
	if not check(world()==old_world and main.kernel.command_context()==old_context,"defend_draft_does_not_commit"): return false
	if not press(main.page,"battle_submit",battle_id): return false
	var deadline: int=Time.get_ticks_usec()+45000000
	var readiness: Array=[]
	while Time.get_ticks_usec()<deadline:
		var current=main.page.find_child("PersonalBattleScreen",true,false)
		if current!=null and int(current.page_input.revision)>old_revision and (int(current.battle.round)>old_round or current.battle.phase=="result") and current.has_method("playback_frame"):
			var current_frame: Dictionary=current.playback_frame()
			var pause_pair: Array=binding.controls.pause
			var pause_button: Button=control(main.page,"command_kind",str(pause_pair[0]),str(pause_pair[1]))
			if readiness.size()<32:
				readiness.append({"frame":current_frame,"pause_found":pause_button!=null,"pause_disabled":pause_button.disabled if pause_button!=null else true,"scene_epoch":main._scene_counter,"page_revision":current.page_input.revision,"advancing":main._advancing,"frame_pending":main._resident_frame_pending,"pending_reads":main.kernel.has_pending_reads(),"source":main.kernel.source_identity(),"context":main.kernel.command_context()})
				write("new-round-readiness.json",encode(readiness))
			if pause_button==null:
				await process_frame
				continue
			if not check(current_frame.get("active",false) and not pause_button.disabled,"real_new_round_starts_read_only_playback_with_live_controls"): return false
			if not playback_press("pause") or not await settled(): return false
			if not check(commands.size()==1 and commands[0].kind=="battle_plan" and commands[0].result.get("ok",false),"exactly_one_real_round_command"): return false
			var settled_view=battle_view()
			if settled_view==null: return false
			settled_page=settled_view.page_input.duplicate(true)
			return write("actual-round-page.json",encode(settled_page)) and write("commands.json",encode(commands))
		await process_frame
	return check(false,"one_round_new_page_timeout")

## 真实暂停、播放、1x/2x与普通重绘保持同一clip，再由跳过到核心终态。
func exercise_playback() -> bool:
	playback_world=world(); playback_source=main.kernel.source_identity().duplicate(true)
	playback_context=main.kernel.command_context().duplicate(true); playback_receipts=encode(commands)
	if not write("before-playback-world.json",playback_world) or not write("before-playback-source.json",encode(playback_source)) or not await checkpoint("before-playback"): return false
	var paused: Dictionary=frame()
	if not check(paused.get("paused",false) and paused.get("active",false),"paused_live_clip"): return false
	var submit: Button=control(main.page,"command_kind","battle_submit")
	if not check(submit!=null and submit.disabled,"next_round_disabled_during_playback"): return false
	await create_timer(0.12).timeout
	if not check(frame().get("elapsed")==paused.get("elapsed"),"pause_holds_ui_time") or not unchanged("paused"): return false
	if not playback_press("speed_2") or not check(frame().get("speed")==2.0 and frame().get("paused",false),"visible_speed_2_preserves_pause"): return false
	if not playback_press("play"): return false
	await process_frame; await process_frame
	if not playback_press("pause"): return false
	if not check(float(frame().get("elapsed",0))>float(paused.get("elapsed",0)),"play_advances_only_ui_time") or not unchanged("speed_2_play"): return false
	if not playback_press("speed_1") or not check(frame().get("speed")==1.0,"visible_speed_1"): return false
	var before_normal: Dictionary=frame()
	if not playback_press("play"): return false
	await process_frame; await process_frame
	if not playback_press("pause"): return false
	if not check(float(frame().get("elapsed",0))>float(before_normal.get("elapsed",0)),"normal_speed_advances_ui_time") or not unchanged("speed_1_play"): return false
	var before_redraw: Dictionary=frame()
	var view=battle_view()
	if view==null or not arm_stale_callbacks(view,"ordinary_redraw"): return false
	main._render()
	if not await settled(): return false
	var redrawn: Dictionary=frame()
	if not check(redrawn.get("paused",false) and redrawn.get("elapsed")==before_redraw.get("elapsed") and redrawn.get("speed")==before_redraw.get("speed"),"ordinary_redraw_resumes_same_paused_time"): return false
	if not unchanged("ordinary_redraw") or not playback_press("skip"): return false
	var skipped: Dictionary=frame()
	if not check(skipped.get("complete",false) and not skipped.get("active",true),"skip_reaches_actual_terminal_frame"): return false
	if not unchanged("skip_no_new_round_or_close"): return false
	submit=control(main.page,"command_kind","battle_submit")
	if not check(submit!=null and not submit.disabled,"skip_returns_next_round_to_player"): return false
	main._render()
	if not await settled(): return false
	if not check(frame().get("complete",false) and not frame().get("active",true),"completed_clip_does_not_replay_on_redraw"): return false
	if not unchanged("completed_redraw") or not await checkpoint("after-playback"): return false
	if not check(FileAccess.get_file_as_bytes(output.path_join("before-playback.hmcheckpoint"))==FileAccess.get_file_as_bytes(output.path_join("after-playback.hmcheckpoint")),"pure_playback_typed_checkpoint_exact_bytes"): return false
	return write("after-playback-world.json",world()) and write("after-playback-source.json",encode(main.kernel.source_identity()))

## 正常菜单成功读回原银行后旧播放器不能复活，世界仍是原银行且没有第二次命令。
func reload_original() -> bool:
	var generation: int=int(main.get_meta("personal_battle_generation",0))
	var receipts: String=encode(commands)
	if not press(main.page,"system_menu") or not await settled(): return false
	expected_reload_generation=generation+1
	if not press(main.page,"load") or not await confirm_slot() or not await settled(): return false
	if not check(main.last_storage_result.get("ok",false) and int(main.get_meta("personal_battle_generation",0))==generation+1,"normal_menu_successful_load_advances_generation_once"): return false
	if not check(world()==FileAccess.get_file_as_string(binding.saved_world) and encode(commands)==receipts,"load_restores_original_world_without_command_replay"): return false
	var restored: Dictionary=frame()
	if not check(not restored.get("active",true),"cold_restored_page_has_no_previous_clip"): return false
	return check(stale_callbacks>=6,"real_stale_redraw_and_load_control_signals_observed") and write("loaded-frame.json",encode(restored)) and write("loaded-world.json",world())

## 空标题正常继续原银行，单轮真实事件后只读播放，最后成功读回原银行。
func consume() -> bool:
	if not press(main.page,"continue_dynamic") or not await confirm_slot() or not await settled(): return false
	if not check(main.last_storage_result.get("ok",false) and main.kernel.content().get("pack_set_digest")==binding.pack_set_digest,"normal_continue_exact_catalog"): return false
	if not check(world()==FileAccess.get_file_as_string(binding.saved_world),"copied_bank_original_world_exact_bytes"): return false
	if not check(not frame().get("active",true),"cold_continue_does_not_invent_motion"): return false
	main.kernel.command_completed.connect(observed_command)
	if binding.get("mode","playback")=="cold_reload": return await cold_reload_only()
	if not await settle_one_round() or not await exercise_playback(): return false
	var boundaries: Dictionary=BoundaryProbe.verify(self,planning_page,settled_page,main._data)
	if not write("source-boundaries.json",encode(boundaries)) or not check(boundaries.get("ok",false),"isolated_screen_source_boundaries"): return false
	if not unchanged("after_isolated_screen_boundaries"): return false
	return await reload_original()

## 单独补完0命令的真实冷恢复与同银行二次读档；不重新执行已经通过的战斗轮。
func cold_reload_only() -> bool:
	playback_world=world(); playback_source=main.kernel.source_identity().duplicate(true)
	playback_context=main.kernel.command_context().duplicate(true); playback_receipts=encode(commands)
	if not write("before-cold-reload-world.json",playback_world) or not await checkpoint("before-cold-reload"): return false
	var view=battle_view()
	if view==null or not arm_stale_callbacks(view,"cold_ordinary_redraw"): return false
	main._render()
	if not await settled() or not unchanged("cold_ordinary_redraw"): return false
	if not await reload_original() or not await checkpoint("after-cold-reload"): return false
	if not check(commands.is_empty() and FileAccess.get_file_as_bytes(output.path_join("before-cold-reload.hmcheckpoint"))==FileAccess.get_file_as_bytes(output.path_join("after-cold-reload.hmcheckpoint")),"cold_reload_zero_commands_and_exact_checkpoint"): return false
	return write("cold-reload-summary.json",encode({"ok":true,"scope":"actual_main_cold_restore_and_successful_reload_only","battle_commands":0,"stale_control_signals":stale_callbacks,"source":main.kernel.source_identity(),"load_generation":main.get_meta("personal_battle_generation",0)}))
