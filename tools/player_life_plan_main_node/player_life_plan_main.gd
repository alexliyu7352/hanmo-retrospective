## 真实青川生活安排的单一Main节点；只按原按钮，物理银行由原系统菜单生产和恢复。
extends "../activity_continuation_main_node/activity_continuation_main.gd"
const Flow=preload("res://tests/test_dynamic_world_player_flow.gd")
const P="society::"
var ids: Dictionary={}
var contract: Dictionary={}

## 预算已提交但尚未核对新页时不算稳定；沿原等待排空，不暗发许可或越过启动交接。
func settled(host) -> bool:
	if not await super.settled(host): return false
	var deadline: int=Time.get_ticks_usec()+45000000
	while Time.get_ticks_usec()<deadline:
		if host._player_plan_pending_start.is_empty(): return await super.settled(host)
		await process_frame
	return check(false,"player_plan_budget_handoff_timeout")

## 同语义菜单项可能隐藏；只找当前可见且尚未释放的实际控件，不因DFS先遇隐藏项而误报入口缺失。
func visible_button(node: Node, kind: String, target: String="") -> Button:
	if node is Button and not node.is_queued_for_deletion() and node.is_inside_tree() and node.is_visible_in_tree() and node.get_meta("command_kind","")==kind and (target.is_empty() or node.get_meta("target_id","")==target): return node
	for child in node.get_children():
		var found: Button=visible_button(child,kind,target)
		if found!=null: return found
	return null

## 优先点击原可见入口；确无可见计划入口时才先按真实菜单，再选择菜单中的同一功能。
func click(kind: String, target: String="") -> bool:
	if not await settled(main): return false
	var current: Button=visible_button(main.page,kind,target)
	if current==null and kind=="player_plan_open":
		var menu: Button=visible_button(main.page,"system_menu")
		if not check(menu!=null and not menu.disabled,"actual_menu_for_plan_entry"): return false
		menu.pressed.emit()
		if not await settled(main): return false
		current=visible_button(main.page,kind,target)
	if not check(current!=null and not current.disabled,"visible_click_"+kind+"_"+target): return false
	rows.append({"stage":"actual_visible_control","command":kind,"target":target,"node_path":str(current.get_path()),"source":main.kernel.source_identity()})
	current.pressed.emit()
	return check(await settled(main),"visible_settled_"+kind)

## 只读同源有限计划页；调用方先开正常手记入口，不以冷世界导出生成可选目标。
func plan() -> Dictionary:
	return main.kernel.demand_view("player_plan")

## 来源按钮按其typed参数定位，整个节点不调用通用submit或直接设置世界事实。
func source(kind: String, params: Dictionary, node: Node=null) -> Control:
	if node==null: node=main.page
	if node.get_meta("player_plan_kind","")==kind and node.get_meta("player_plan_params",{})==params and node.get_meta("player_plan_source",false): return node as Control
	for child in node.get_children():
		var found: Control=source(kind,params,child)
		if found!=null: return found
	return null

## 勾选始终来自实际默认关闭的控件，报价读完后重取活按钮，不跳过同源Session。
func append(kind: String, params: Dictionary, authorized: bool=false) -> bool:
	if not await settled(main): return false
	var entry: Control=source(kind,params)
	if not check(entry!=null and entry.is_visible_in_tree(),"actual_source_"+kind): return false
	if kind in ["work","course"]:
		var consent: CheckBox=entry.find_child("PlayerPlanPreauthorize",true,false)
		if not check(consent!=null and not consent.button_pressed,"new_step_preauthorization_defaults_false"): return false
		if authorized:
			consent.button_pressed=true
			if not await settled(main): return false
			entry=source(kind,params)
			if not check(entry!=null,"source_preserved_after_explicit_consent"): return false
	var add: Button=entry.find_child("PlayerPlanAdd",true,false)
	if not check(add!=null and not add.disabled,"add_actual_"+kind): return false
	var before: Dictionary=main.kernel.transport().debug_export_state()
	add.pressed.emit()
	if not await settled(main): return false
	var after: Dictionary=main.kernel.transport().debug_export_state()
	var a: Dictionary=Strict.parse_text(before.json).value; var b: Dictionary=Strict.parse_text(after.json).value
	return check(a.wallets==b.wallets and a.inventories==b.inventories and a.actions==b.actions,"planning_never_reserves_or_starts_"+kind)

## 原六课时选择让预计完成超过短托付期限，资格和收费仍取正式课程源。
func capture_course(authorized: bool) -> bool:
	if not await click("courses_open"): return false
	var duration: OptionButton=main.find_child("StudyDuration",true,false)
	if not check(duration!=null,"original_course_duration"): return false
	duration.select(4); duration.item_selected.emit(4)
	if not await settled(main): return false
	var contexts: Array=main.kernel.study_course_contexts()
	if not check(not contexts.is_empty(),"actual_local_course_provider"): return false
	var context: Dictionary=contexts[0].context.duplicate(true)
	if not await append("course",{"context":context,"skill_id":"rites","ticks":6,"challenge_mode":"automatic"},authorized): return false
	return await click("courses_close")

## 原粮行对白先显示工资，生活安排只能从这个真实当面工作入口产生。
func capture_work(authorized: bool) -> bool:
	var request: Dictionary={"role_id":P+"role.merchant"}
	var quote: Dictionary=await Flow.preview(self,main,"talk",request)
	if quote.get("code")=="ActorBusy":
		if not await wait_for_visible_provider(request): return false
	elif not check(quote.get("ok",false),"original_work_provider_available"): return false
	if not await click("talk",P+"role.merchant") or not await click("dialogue_next") or not await click("dialogue_choose","work"): return false
	var view: Dictionary=main.kernel.interaction_view()
	if not check(view.get("phase")=="action" and view.command.get("activity")=="work","original_work_confirmation_source"): return false
	if not await append("work",{"context":view.context.duplicate(true)},authorized): return false
	return await Life.close_conversation(self,main)

## 当面忙闲只容许本人再等一个实际半日；若仍忙或托付余期不足就保留停止证据，不改人物或无界等候。
func wait_for_visible_provider(request: Dictionary) -> bool:
	var before: Dictionary=main.kernel.source_identity()
	var observed: Dictionary={"source":before,"place_id":main.kernel.presentation_location().get("place_id",""),"request":request.duplicate(true),"reason":"ActorBusy","max_wait_half_days":1}
	rows.append({"stage":"visible_provider_busy_before_capture","observation":observed})
	if not check(observed.place_id==P+"place.grain" and not main._resident_adapter.auto_running,"provider_busy_is_local_and_stopped"): return false
	if not contract.is_empty() and before.tick>=contract.deadline_tick-2:
		write_json("provider-capture-blocked.json",observed); return check(false,"provider_busy_leaves_insufficient_original_task_window")
	if not await click("wait"): return false
	if not check(main.kernel.source_identity().tick==before.tick+1,"one_explicit_halfday_wait_for_original_provider"): return false
	var quote: Dictionary=await Flow.preview(self,main,"talk",request)
	if not quote.get("ok",false):
		observed.after_source=main.kernel.source_identity(); observed.current_quote=quote
		write_json("provider-capture-blocked.json",observed)
		return check(false,"original_provider_still_unavailable_after_bounded_wait")
	return true

## 调序使用画面中相邻移动按钮，不以函数直接覆盖计划顺序。
func move_step(step_id: String, target_index: int) -> bool:
	for attempt in range(8):
		var steps: Array=plan().get("steps",[]); var index: int=-1
		for i in range(steps.size()):
			if steps[i].step_id==step_id: index=i
		if index==target_index: return true
		if not check(index>=0,"real_step_before_reorder"): return false
		var order: Array=[]
		for row in steps: order.append(row.step_id)
		var other: int=index+(-1 if index>target_index else 1)
		order[index]=steps[other].step_id; order[other]=step_id
		if not await Study.click(self,main,"player_plan_reorder",{"step_ids":order}): return false
	return check(false,"finite_reorder_did_not_reach_target")

## 真实自宅源先记到末尾；捕获后重排仍不扣粮、不改变原所有权。
func capture_rest() -> bool:
	if not await click("family_open") or not await click("family_tab","rest"): return false
	if not await append("rest",{"estate_id":P+"estate.apprentice_home","duration":2}): return false
	return await click("family_close")

## 删除尚未开始的安排只清本人备忘；实际原托付、已托管的钱货和责任均保持不变。
func check_clear_preserves_commitment() -> bool:
	if not await capture_rest() or not await click("player_plan_open"): return false
	var before: Dictionary=Strict.parse_text(main.kernel.transport().debug_export_state().json).value
	if not await Study.click(self,main,"player_plan_clear",{}): return false
	var after: Dictionary=Strict.parse_text(main.kernel.transport().debug_export_state().json).value
	for key in ["wallets","inventories","actions","tasks","facts"]:
		if not check(before.get(key)==after.get(key),"clear_unstarted_keeps_original_"+key): return false
	if not check(plan().steps.is_empty() and plan().commitments.any(func(row): return row.kind=="task" and row.source_id==contract.task_id),"deleting_intent_preserves_accepted_obligation"): return false
	return await click("player_plan_close")

## 本人看到冲突后显式接受风险，预算控件从零钱起；本函数不替世界放宽每步价格。
func budget(half_days: int, money: int) -> bool:
	if not await settled(main): return false
	var ticks: SpinBox=main.find_child("PlayerPlanHalfDays",true,false)
	var spend: SpinBox=main.find_child("PlayerPlanMoney",true,false)
	if not check(ticks!=null and spend!=null and ticks.is_visible_in_tree(),"actual_plan_budget_controls"): return false
	ticks.value=half_days; spend.value=money
	var risk: CheckBox=main.find_child("PlayerPlanKeepRisk",true,false)
	if risk!=null and risk.is_visible_in_tree(): risk.button_pressed=true
	return await click("player_plan_begin")

## 记录有限计划及原世界，截图仅为实际视口证据，不以headless代替画面复核。
func record(label: String) -> bool:
	var exported: Dictionary=main.kernel.transport().debug_export_state()
	if not check(exported.get("ok",false),label+"_world"): return false
	if not write_text(label+"-state.json",exported.json) or not write_json(label+"-plan.json",plan()) or not write_json(label+"-source.json",main.kernel.source_identity()): return false
	if DisplayServer.get_name()!="headless":
		var stable: Dictionary=main.kernel.source_identity(); await process_frame; await RenderingServer.frame_post_draw
		var screenshot: Image=root.get_texture().get_image()
		return check(not screenshot.is_empty() and screenshot.save_png(output.path_join(label+".png"))==OK and main.kernel.source_identity()==stable,label+"_real_viewport")
	return true

## 原本人合同先正式承接，随后只在实际来源页收集课程、工作、休整和通路。
func prepare_life_route() -> bool:
	if not await Life.start_selected(self,main,ids): return false
	if not await click("dynamic_open"): return false
	if not check(main.kernel.source_identity().tick==0 and Flow.rows(main.page).any(func(row): return row.get("kind")=="opportunity" and row.get("action",{}).get("command")=="accept_offer"),"initial_public_need_is_already_visible_without_wait"): return false
	contract=await Flow.accept_contract(self,main,ids.scenario_id)
	if contract.is_empty() or not await click("dynamic_close"): return false
	if not await check_clear_preserves_commitment(): return false
	if not await capture_rest(): return false
	if not await append("move_local",{"place_id":P+"place.school"}) or not await click("move_local",P+"place.school"): return false
	if not await capture_course(true): return false
	if not await append("move_local",{"place_id":P+"place.courtyard"}) or not await click("move_local",P+"place.courtyard"): return false
	if not await append("move_local",{"place_id":P+"place.grain"}) or not await click("move_local",P+"place.grain"): return false
	if not await capture_work(false): return false
	if not await append("move_local",{"place_id":P+"place.courtyard"}) or not await click("move_local",P+"place.courtyard"): return false
	if not await click("player_plan_open"): return false
	if not await append("task_reminder",{"task_id":contract.task_id,"place_id":P+"place.courtyard"}): return false
	var steps: Array=plan().steps
	if not check(steps.size()==8,"eight_real_typed_steps"): return false
	var rest_id: String=steps[0].step_id; var task_id: String=steps[-1].step_id
	var visible: Array=main.find_children("PlayerPlanConflict","Label",true,false)
	if not check(not visible.is_empty() and plan().conflicts.any(func(row): return row.get("known_conflict",false)),"course_completion_not_arrival_conflicts_with_original_deadline"): return false
	if not await record("known-completion-conflict"): return false
	if not await move_step(rest_id,7) or not await move_step(task_id,0): return false
	if not await record("reordered-reminder-first"): return false
	if not await budget(10,180) or not await stopped(): return false
	if not check(not main._resident_adapter.auto_running and main.kernel.source_identity().tick<contract.deadline_tick,"task_reminder_stops_without_autofulfillment"): return false
	if not await record("task-reminder-stopped"): return false
	return true

## 托付依原货单和实际交付办理，计划提醒不能代买货、交付或领酬。
func handle_original_task() -> bool:
	if not await click("player_plan_close") or not await click("move_local",P+"place.grain"): return false
	var trade: Dictionary={"merchant_role":P+"role.merchant","merchant":P+"actor.merchant","shop":P+"shop.grain","item":P+"item.millet"}
	if not await Flow.purchase(self,main,trade,contract): return false
	if not await click("move_local",P+"place.courtyard") or not await click("dynamic_open"): return false
	if not await Flow.report_contract(self,main,contract) or not await click("dynamic_close"): return false
	if not await click("player_plan_open"): return false
	# 原报告真实成功才更新提醒；这只是观察原结果，不能代交付、领奖或重新授予后续预算。
	var reminders: Array=plan().steps.filter(func(row): return row.kind=="task_reminder" and row.params.task_id==contract.task_id)
	return check(reminders.size()==1 and reminders[0].phase=="completed" and not main._resident_adapter.auto_running,"original_successful_report_completes_only_its_reminder_and_stays_stopped")

## 正常局在真实课程执行一课后保存；未授权短工保留为后续本人确认停点。
func produce() -> bool:
	if not check(not DirAccess.dir_exists_absolute(directory),"fresh_plan_profile"): return false
	if action_case=="busy": return await produce_busy()
	if not await prepare_life_route() or not await handle_original_task(): return false
	main.kernel.command_completed.connect(command_completed)
	if not await budget(1,30) or not await stopped(): return false
	if not check(main.kernel.action_context().get("action_kind")=="study_skill" and main.kernel.study_activity().completed_lessons==1,"plan_started_same_course_and_one_real_lesson"): return false
	if not await Life.save_via_menu(self,main,"manual-1") or not await record("saved"): return false
	return write_json("save-manifest.json",{"format":"HM-PLAYER-LIFE-PLAN-MAIN/1","producer_pid":OS.get_process_id(),"case":action_case,"profile":profile,"directory":directory,"binding_sha256":FileAccess.get_sha256(binding_path),"banks":bank_hashes(),"source":main.kernel.source_identity(),"plan":plan(),"action_id":main.kernel.action_context().action_id})

## 人忙分支先看过同一短工，再由原NPC实际谋生占用店家；停止不能换人或吞掉原价变化。
func produce_busy() -> bool:
	if not await Life.start_selected(self,main,ids): return false
	if not await append("move_local",{"place_id":P+"place.grain"}) or not await click("move_local",P+"place.grain") or not await capture_work(true): return false
	if not await click("move_local",P+"place.courtyard") or not await click("wait") or not await click("player_plan_open"): return false
	var before: Dictionary=main.kernel.transport().debug_export_state()
	if not await budget(2,0) or not await stopped(): return false
	var work: Dictionary=plan().steps[1]
	if not check(work.phase in ["blocked","awaiting_confirmation"] and work.get("action_id","").is_empty(),"real_provider_busy_prevents_new_action"): return false
	var a: Dictionary=Strict.parse_text(before.json).value; var b: Dictionary=Strict.parse_text(main.kernel.transport().debug_export_state().json).value
	if not check(a.wallets==b.wallets,"blocked_work_never_pays_or_reserves"): return false
	if not await record("provider-busy-stopped"): return false
	if not check(work.get("stop_reason")=="ActorBusy","busy_stop_keeps_original_domain_reason"): return false
	return await retry_busy_work_once(work)

## 停车后本人只等一个已知短工半日，再从当面原报价决定是否重新确认预算；仍忙时保留终态而不绕过。
func retry_busy_work_once(work: Dictionary) -> bool:
	if not await click("player_plan_close"): return false
	var before: Dictionary=main.kernel.source_identity()
	if not await click("wait"): return false
	if not check(main.kernel.source_identity().tick==before.tick+1 and not main._resident_adapter.auto_running,"busy_route_explicit_wait_never_resumes_old_permission"): return false
	var quote: Dictionary=await Flow.preview(self,main,"service",{"activity":"work","context":work.params.context.duplicate(true)})
	if not await click("player_plan_open"): return false
	rows.append({"stage":"same_provider_after_one_known_halfday","source":main.kernel.source_identity(),"quote":quote.duplicate(true),"step_id":work.step_id})
	if quote.get("code")=="ActorBusy": return await record("provider-still-busy-after-one-halfday")
	if not check(quote.get("ok",false),"same_provider_terms_can_be_reviewed_after_wait"): return false
	if not await budget(1,0) or not await stopped(): return false
	var current: Dictionary=plan().steps[1]
	if current.phase in ["blocked","awaiting_confirmation"]:
		if not check(current.step_id==work.step_id and current.get("action_id","").is_empty() and current.get("stop_reason")=="ActorBusy","retry_keeps_same_busy_unaccepted_step"): return false
		return await record("provider-retry-stopped")
	if not check(current.step_id==work.step_id and current.phase=="completed" and not current.get("action_id","").is_empty(),"explicit_new_budget_completes_original_work_once"): return false
	return await record("provider-work-completed-after-explicit-retry")

## 新进程从原银行恢复，静置不重启；本人重新选预算后继续原课程直到未授权工作停住。
func consume() -> bool:
	if not check(action_case=="funded","only_saved_funded_route_has_consumer"): return false
	var manifest: Dictionary=read_json(case_root.path_join("producer/save-manifest.json"))
	if not check(bank_hashes()==manifest.banks,"same_physical_plan_bank"): return false
	# 活动课程页优先于恢复后地图标志；按实际计划菜单进入，不假定隐藏map_close存在。
	if not await Life.continue_empty_title(self,main,ids,"manual-1",false): return false
	if not await click("player_plan_open"): return false
	if not check(not main._resident_adapter.auto_running and main.kernel.transport().debug_export_state().json==FileAccess.get_file_as_string(case_root.path_join("producer/saved-state.json")),"fresh_title_exact_plan_world"): return false
	var stable: Dictionary=main.kernel.source_identity(); await create_timer(1.4).timeout
	if not check(main.kernel.source_identity()==stable and not main._resident_adapter.auto_running,"restored_plan_requires_explicit_permission"): return false
	if not await record("restored"): return false
	if phase=="consume-auto":
		if not await budget(8,150) or not await stopped(): return false
	else:
		# 相同已接受课程仍走原每课advance；计划观察原完成事实，不重复开课收费。
		if not await click("player_plan_close"): return false
		for index in range(5):
			if not await click("advance"): return false
		if not await click("player_plan_open") or not await budget(3,0) or not await stopped(): return false
	var pending: Array=plan().steps.filter(func(row): return row.kind=="work")
	if not check(pending.size()==1 and not pending[0].authorized and pending[0].phase in ["awaiting_confirmation","blocked"],"unapproved_original_work_stops_at_source"): return false
	if not await record("work-confirmation-stopped") or not check(bank_hashes()==manifest.banks,"consumer_preserves_original_banks"): return false
	var row: Control=null
	for candidate in main.find_children("*","Control",true,false):
		if candidate.get_meta("step_id","")==pending[0].step_id: row=candidate
	if not check(row!=null,"actual_unapproved_work_row"): return false
	var consent: CheckBox=row.find_child("PlayerPlanStepPreauthorize",true,false)
	if not check(consent!=null and not consent.button_pressed,"remaining_work_permission_still_unchecked"): return false
	consent.button_pressed=true
	if not await settled(main) or not await Study.click(self,main,"player_plan_authorize",{"step_id":pending[0].step_id,"authorized":true}): return false
	if not await budget(3,0) or not await stopped(): return false
	if not check(plan().steps.all(func(item): return item.phase=="completed") and main.kernel.presentation_location().get("place_id")==P+"place.courtyard" and main.kernel.action_context().get("action_id","").is_empty(),"real_work_and_owned_rest_finish_at_home"): return false
	if not await record("completed") or not check(bank_hashes()==manifest.banks,"completed_consumer_preserves_original_banks"): return false
	if phase=="consume-manual":
		var reference: Dictionary=read_json(case_root.path_join(str(get_meta("automatic_output","consume-auto"))+"/completed-state.json"))
		var actual: Dictionary=Strict.parse_text(main.kernel.transport().debug_export_state().json).value
		for key in ["wallets","inventories","actors","actions","facts"]:
			if not check(actual.get(key)==reference.get(key),"same_original_domain_after_manual_course_"+key): return false
	return true

## 运行只在中央精确绑定受审库与唯一stage以后；原prepare和静态检查不能解锁运行。
func run() -> void:
	for arg in OS.get_cmdline_user_args():
		if arg.begins_with("--phase="): phase=arg.trim_prefix("--phase=")
		elif arg.begins_with("--case="): action_case=arg.trim_prefix("--case=")
		elif arg.begins_with("--case-root="): case_root=arg.trim_prefix("--case-root=").simplify_path().trim_suffix("/")
		elif arg.begins_with("--binding="): binding_path=arg.trim_prefix("--binding=")
		elif arg.begins_with("--attempt="): set_meta("plan_node_attempt",arg.trim_prefix("--attempt="))
		elif arg.begins_with("--automatic-output="): set_meta("automatic_output",arg.trim_prefix("--automatic-output="))
	if phase not in ["produce","consume-auto","consume-manual"] or action_case not in ["funded","busy"] or not case_root.is_absolute_path(): quit(2); return
	var attempt: String=get_meta("plan_node_attempt","")
	if not attempt.is_empty() and (not attempt.is_valid_identifier() or phase=="produce"): quit(2); return
	output=case_root.path_join(("producer" if phase=="produce" else phase)+("-"+attempt if not attempt.is_empty() else ""))
	if not DirAccess.dir_exists_absolute(output) or FileAccess.file_exists(output.path_join("result.json")): quit(2); return
	binding=read_json(binding_path); ids=binding.cases[action_case]
	if not check(binding.get("native_artifact_status")=="BOUND_REVIEWED_ARTIFACT_NOT_EXECUTED" and binding.get("source_commit")==binding.get("reviewed_source_commit") and FileAccess.get_sha256(binding.native_extension.path)==binding.native_extension.sha256,"reviewed_plan_library"): quit(2); return
	profile="player-life-plan-"+case_root.sha256_text().left(12); ProjectSettings.set_setting("application/config/storage_profile",profile)
	directory=ProjectSettings.globalize_path("user://profiles/"+profile+"/saves")
	if not check(directory.begins_with(case_root+"/") and ClassDB.class_exists("HanmoResidentBridge"),"owned_profile_real_plan_native"): quit(2); return
	root.size=Vector2i(1280,900); main=MainScene.instantiate(); root.add_child(main)
	var passed: bool=await produce() if phase=="produce" else await consume()
	write_json("receipts.json",receipts)
	write_json("result.json",{"ok":passed and failures.is_empty(),"phase":phase,"case":action_case,"pid":OS.get_process_id(),"source_commit":binding.source_commit,"native_source_commit":binding.native_source_commit,"profile":profile,"rows":rows,"failures":failures,"page_error":main._resident_page_error,"message":main._message,"actual_main_scene":true,"actual_native_bridge":true,"gui_pixels_accepted":false})
	main.queue_free(); await process_frame
	print("PLAYER_LIFE_PLAN_MAIN ",action_case," ",phase," OK=",passed and failures.is_empty()); quit(0 if passed and failures.is_empty() else 1)
