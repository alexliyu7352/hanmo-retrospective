## 分段普通课程的真实Main流程片；原作者、原控件与物理槽由同一联合节点使用，不注入课程或题面。
extends RefCounted
const Life=preload("res://tests/test_regional_life_player_flow.gd")
const Strict=preload("res://core/strict_json.gd")

## 所有失败进入调用方的原回执，不能以跳过按钮或替换人物继续。
static func check(harness, value: bool, label: String) -> bool:
	harness.rows.append({"stage":"study_segments_"+label,"ok":value})
	if not value: harness.failures.append("study_segments_"+label)
	return value

## 冷导出只作为节点证据，绝不再喂入世界；产品页面仍消费有限原查询。
static func state(harness, main) -> Dictionary:
	var exported: Dictionary=main.kernel.transport().debug_export_state()
	if not check(harness,exported.get("ok",false) and exported.get("json") is String,"actual_world_export"): return {}
	var decoded: Dictionary=Strict.parse_text(exported.json)
	if not check(harness,decoded.get("ok",false) and decoded.get("value") is Dictionary,"strict_world_evidence"): return {}
	return decoded.value

## 只查找本次可见活按钮，参数必须来自当前Session元数据。
static func action_button(node: Node, kind: String, expected: Dictionary={}) -> Button:
	if node is Button and not node.is_queued_for_deletion() and node.is_visible_in_tree() and node.get_meta("command_kind","")==kind:
		var actual: Dictionary=node.get_meta("activity_params",{})
		var matching: bool=true
		for key in expected: matching=matching and actual.get(key)==expected[key]
		if matching: return node
	for child in node.get_children():
		var found: Button=action_button(child,kind,expected)
		if found!=null: return found
	return null

## 原按钮信号提交课程命令，零时命令完成后核真正世界tick没有变化。
static func click(harness, main, kind: String, expected: Dictionary={}, zero_time: bool=true) -> bool:
	if not await harness.settled(main): return false
	var current: Button=action_button(main.page,kind,expected)
	if not check(harness,current!=null and not current.disabled,"actual_button_"+kind): return false
	var before: Dictionary=main.kernel.source_identity()
	current.pressed.emit()
	if not check(harness,await harness.settled(main),"settled_"+kind): return false
	return check(harness,main.kernel.source_identity().tick==before.tick+(0 if zero_time else 1),"actual_time_"+kind)

## 新标题先选明确包锁和原人物；固定作者外貌只按真实确认按钮，不能传测试生成外貌。
static func start_selected(harness, main, ids: Dictionary) -> bool:
	if not check(harness,not main._started and not main._resident_mode,"empty_title"): return false
	var start: Button=Life.button(main.page,"start")
	if not check(harness,start!=null and not start.disabled,"normal_start"): return false
	start.pressed.emit()
	if not check(harness,is_instance_valid(main._scenario_dialog) and main._scenario_dialog.visible,"real_scenario_picker"): return false
	var selected: Button=Life._scenario_button(main._scenario_dialog,ids)
	if not check(harness,selected!=null and not selected.disabled,"exact_author_selection"): return false
	selected.pressed.emit()
	if is_instance_valid(main._appearance_picker):
		var confirm: Button=null
		for current in main._appearance_picker.find_children("*","Button",true,false):
			if current.get_meta("appearance_action","")=="confirm": confirm=current
		if not check(harness,confirm!=null and not confirm.disabled,"fixed_author_appearance_confirmation"): return false
		confirm.pressed.emit()
	return check(harness,await harness.settled(main) and main._resident_mode and main.kernel.content().get("pack_set_digest")==ids.pack_set_digest and main.kernel.command_context().actor_id==ids.player_actor_id,"real_selected_world")

## 原120钱足够初学三课90钱，直接走讲舍普通课程入口；不额外做工或改钱包。
static func enroll(harness, main, ids: Dictionary) -> bool:
	if not await start_selected(harness,main,ids): return false
	if not await Life.click(harness,main,"move_local","qingstudy::place.school"): return false
	if not await Life.click(harness,main,"courses_open"): return false
	var duration: OptionButton=main.find_child("StudyDuration",true,false)
	if not check(harness,duration!=null and duration.item_count==5,"normal_course_duration"): return false
	duration.select(1); duration.item_selected.emit(1)
	if not await harness.settled(main): return false
	return await click(harness,main,"study_skill",{"skill_id":"rites","ticks":3,"challenge_mode":"manual"})

## 一课先按原基线实得，再答原第一轮并暂停；费用、疲劳、选择和成长均来自原领域。
static func start_to_pending(harness, main, ids: Dictionary) -> Dictionary:
	if not await enroll(harness,main,ids): return {}
	var before: Dictionary=state(harness,main)
	if before.is_empty(): return {}
	var activity: Dictionary=main.kernel.study_activity(); var id: String=activity.action_id
	if not check(harness,activity.get("settlement_policy")=="per_lesson_v1" and activity.completed_lessons==0 and activity.xp_gained==0 and activity.challenge_pending,"new_manual_policy_without_credit"): return {}
	if not await click(harness,main,"advance",{},false): return {}
	var taught: Dictionary=state(harness,main); var fact: Dictionary=taught.facts[id]
	var player: String=taught.player_actor_id
	if not check(harness,fact.lessons.size()==1 and taught.actors[player].skills.rites-before.actors[player].skills.rites==fact.lessons[0].xp and main.kernel.study_activity().xp_gained==fact.lessons[0].xp,"unanswered_first_lesson_credited_once"): return {}
	if not await click(harness,main,"study_challenge_choose",{"step_id":"register","option_id":"names"}): return {}
	if not await click(harness,main,"study_pause",{"action_id":id}): return {}
	var paused: Dictionary=state(harness,main)
	if not check(harness,paused.actors[player].skills==taught.actors[player].skills and paused.wallets==taught.wallets and paused.rng==taught.rng and paused.actions[id].participants.is_empty() and paused.actors[player].action_id.is_empty() and paused.actors[fact.terms.teacher_id].action_id.is_empty(),"unanswered_pause_releases_only_occupation"): return {}
	return {"action_id":id,"source":main.kernel.source_identity(),"state":paused,"slot":"manual-1"}

## 暂停课从原系统菜单找回，同一真实活动页恢复原教师，不另报名。
static func open_paused(harness, main, action_id: String) -> bool:
	if not await Life.click(harness,main,"system_menu"): return false
	var current: Button=main.find_child("PausedStudyMenuOpen",true,false)
	if not check(harness,current!=null and current.is_visible_in_tree() and not current.disabled,"paused_menu_entry"): return false
	current.pressed.emit()
	if not check(harness,await harness.settled(main),"paused_page_ready"): return false
	return check(harness,main.kernel.study_activity().action_id==action_id and main.kernel.study_activity().paused and main.find_child("StudyAdvance",true,false)==null,"paused_original_course_page")

## 原第二、三轮晚答只补已听课时差额，资金、疲劳、随机和时间保持；暂停保存固定已答实例。
static func continue_pending(harness, main, boundary: Dictionary) -> Dictionary:
	var id: String=boundary.action_id
	if not await open_paused(harness,main,id) or not await click(harness,main,"study_resume",{"action_id":id}): return {}
	var before: Dictionary=state(harness,main); var player: String=before.player_actor_id
	if not check(harness,before.facts[id].terms==boundary.state.facts[id].terms and before.facts[id].challenge==boundary.state.facts[id].challenge and before.rng==boundary.state.rng,"resume_original_teacher_question_and_seed"): return {}
	for step in ["report","close"]:
		if not await click(harness,main,"study_challenge_choose",{"step_id":step,"option_id":"names"}): return {}
	var answered: Dictionary=state(harness,main); var fact: Dictionary=answered.facts[id]
	var multiplier: int=fact.challenge.challenge.result.performance_bp
	@warning_ignore("integer_division")
	var expected: int=fact.lessons[0].xp*multiplier/10000
	if not check(harness,multiplier==12000 and fact.lessons.size()==1 and answered.actors[player].skills.rites-before.actors[player].skills.rites==expected-fact.lessons[0].xp and main.kernel.study_activity().xp_gained==expected,"late_original_score_exact_one_lesson_difference"): return {}
	if not check(harness,answered.tick==before.tick and answered.rng==before.rng and answered.wallets==before.wallets and answered.actors[player].fatigue==before.actors[player].fatigue and answered.actors[fact.terms.teacher_id].fatigue==before.actors[fact.terms.teacher_id].fatigue,"late_answer_is_zero_time_fee_fatigue_rng"): return {}
	if not await click(harness,main,"study_pause",{"action_id":id}): return {}
	return {"action_id":id,"source":main.kernel.source_identity(),"state":state(harness,main),"slot":"manual-2"}

## 已答存档的剩余路线只再教一课并取消第三课，结束不重复经验且只释放未提供的费用。
static func continue_answered(harness, main, boundary: Dictionary) -> bool:
	var id: String=boundary.action_id
	if not await open_paused(harness,main,id) or not await click(harness,main,"study_resume",{"action_id":id}): return false
	var before: Dictionary=state(harness,main); var player: String=before.player_actor_id
	if not check(harness,before.facts[id].challenge==boundary.state.facts[id].challenge and before.actors[player].skills==boundary.state.actors[player].skills and before.rng==boundary.state.rng,"answered_restore_resume_no_recredit"): return false
	if not await click(harness,main,"advance",{},false): return false
	var taught: Dictionary=state(harness,main); var fact: Dictionary=taught.facts[id]
	@warning_ignore("integer_division")
	var expected: int=fact.lessons[1].xp*fact.challenge.challenge.result.performance_bp/10000
	if not check(harness,fact.lessons.size()==2 and taught.actors[player].skills.rites-before.actors[player].skills.rites==expected,"known_score_next_lesson_once"): return false
	if not await click(harness,main,"study_cancel",{"action_id":id}): return false
	var ended: Dictionary=state(harness,main)
	if not check(harness,not ended.actions.has(id) and ended.facts[id].code=="StudyCancelled" and ended.actors[player].skills==taught.actors[player].skills and ended.wallets[fact.terms.payer_wallet_id].balance==taught.wallets[fact.terms.payer_wallet_id].balance and not ended.wallets[fact.terms.payer_wallet_id].reserved.has(id) and ended.rng==taught.rng,"cancel_retains_two_lessons_and_releases_only_unused_reservation"): return false
	return await history(harness,main,id,ended)

## 原普通技艺菜单展示真实终态贡献和前后总经验；打开、换项不能再授课或发奖。
static func history(harness, main, action_id: String, ended: Dictionary) -> bool:
	var source: Dictionary=main.kernel.source_identity()
	if not await Life.click(harness,main,"system_menu") or not await Life.click(harness,main,"ordinary_skills_open") or not await Life.click(harness,main,"ordinary_skill","rites"): return false
	var view: Dictionary=main.kernel.ordinary_skills("rites")
	if not check(harness,view.get("settlement_policy")=="per_lesson_v1" and view.course_records.size()==1,"real_completed_course_history"): return false
	var record: Dictionary=view.course_records[0]
	var contribution: Label=main.find_child("OrdinaryCourseContribution",true,false)
	var total: Label=main.find_child("OrdinaryCourseTotalXP",true,false)
	if not check(harness,record.action_id==action_id and record.xp_after==ended.actors[ended.player_actor_id].skills.rites and contribution!=null and total!=null and contribution.text=="本课程经验 +%d · 实授 2 / 3 个半日"%record.xp_gained and total.text=="开课时总经验 %d · 结束时总经验 %d"%[record.xp_before,record.xp_after],"actual_history_separates_contribution_and_totals"): return false
	return check(harness,main.kernel.source_identity()==source and state(harness,main)==ended,"history_is_read_only")

## 空标题读取真实原槽后先核整个持久世界，再允许原题面继续，不从证据恢复假世界。
static func restore_boundary(harness, main, ids: Dictionary, boundary: Dictionary) -> bool:
	if not await Life.continue_empty_title(harness,main,ids,boundary.slot): return false
	var restored: Dictionary=state(harness,main)
	return check(harness,restored==boundary.state and main.kernel.source_identity().tick==boundary.source.tick and not main._resident_adapter.auto_running,"physical_fresh_title_exact_pending_course")
