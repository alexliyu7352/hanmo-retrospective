## 统一节点的真实Main课程暂停片；调用方提供同源普通自动课，不造教师、课费或自己的推进循环。
extends RefCounted

## 每个必要断言并入中央回执；失败保留真实页面和世界供诊断。
static func check(harness, value: bool, label: String) -> bool:
	harness.rows.append({"stage":"course_pause_"+label,"ok":value})
	if not value: harness.failures.append("course_pause_"+label)
	return value

## 从当前树找正式命令按钮；隐藏页面和旧节点不冒充当前可操作入口。
static func button(node: Node, kind: String) -> Button:
	if node is Button and node.get_meta("command_kind","")==kind: return node
	for child in node.get_children():
		var result: Button=button(child,kind)
		if result!=null: return result
	return null

## 原Kernel独立提交精确课程控制；主界面只按真实按钮，零时长控制不附送半日。
static func control(harness, main, baseline, name: String, kind: String, action_id: String) -> bool:
	var current: Button=main.find_child(name,true,false)
	if not check(harness,current!=null and not current.disabled and current.is_visible_in_tree(),kind+"_enabled"): return false
	var context: Dictionary=baseline.command_context()
	var submitted: Dictionary=baseline.submit({"kind":kind,"params":{"action_id":action_id},"actor_id":context.actor_id,"request_seq":context.next_request_seq,"expected_revision":context.revision})
	if not check(harness,submitted.get("ok",false),kind+"_original"): return false
	current.pressed.emit()
	if not check(harness,await harness.settled(main),kind+"_settled"): return false
	harness.record(main,"course_pause_"+kind,JSON.stringify(baseline.snapshot(),"",false,true))
	return check(harness,main.kernel.study_activity()==baseline.study_activity(),kind+"_finite_activity")

## 调用时应为原Main初次报名后的一个实际半日；手动题面和军事训练不属于本片。
static func run(harness, main, baseline) -> bool:
	if not check(harness,main._resident_mode and await harness.settled(main),"native_entry"): return false
	var activity: Dictionary=baseline.study_activity()
	if not check(harness,activity.get("active",false) and activity.kind=="study_skill" and activity.get("pause_supported",false) and not activity.paused and activity.completed_lessons==1 and activity.remaining_ticks>=1,"automatic_first_half_only"): return false
	var action_id: String=activity.action_id
	var before: Dictionary=baseline.snapshot()
	var frozen: Dictionary=before.facts[action_id].duplicate(true)
	var pause_button: Button=main.find_child("StudyPause",true,false)
	if not check(harness,pause_button!=null,"actual_pause_control"): return false
	var old_session=pause_button.get_parent().get_meta("activity_session")
	var old_ticket: int=pause_button.get_meta("activity_ticket")
	if not await control(harness,main,baseline,"StudyPause","study_pause",action_id): return false
	if not check(harness,baseline.snapshot().tick==before.tick and main.kernel.action_context().action_id.is_empty() and main.find_child("StudyActiveAction",true,false)==null,"released_and_returned_to_scene"): return false
	if not check(harness,old_session.activate(old_ticket).get("code")=="ActivitySessionClosed","stale_pause_button_rejected"): return false
	var wait: Button=button(main.page,"wait")
	if not check(harness,wait!=null and not wait.disabled and wait.is_visible_in_tree(),"normal_wait_available"): return false
	var context: Dictionary=baseline.command_context()
	if not check(harness,baseline.submit({"kind":"wait","params":{},"actor_id":context.actor_id,"request_seq":context.next_request_seq,"expected_revision":context.revision}).get("ok",false) and baseline.advance_one_tick().get("ok",false),"original_other_activity"): return false
	wait.pressed.emit()
	if not check(harness,await harness.settled(main),"other_activity_settled"): return false
	harness.record(main,"course_pause_other_activity",JSON.stringify(baseline.snapshot(),"",false,true))
	var current: Dictionary=baseline.snapshot()
	if not check(harness,current.facts[action_id].lessons==frozen.lessons and current.facts[action_id].terms==frozen.terms and current.rng==before.rng and current.wallets==before.wallets,"no_paused_lesson_fee_or_reroll"): return false
	var menu: Button=button(main.page,"system_menu")
	if not check(harness,menu!=null,"menu_exists"): return false
	menu.pressed.emit()
	var pending: Button=main.find_child("PausedStudyMenuOpen",true,false)
	if not check(harness,pending!=null and pending.is_visible_in_tree(),"paused_course_entry"): return false
	pending.pressed.emit()
	if not check(harness,await harness.settled(main),"paused_page_settled"): return false
	if not check(harness,main.find_child("StudyAdvance",true,false)==null and main.find_child("StudyPausedClose",true,false)!=null,"paused_page_has_no_lesson_advance"): return false
	if not await control(harness,main,baseline,"StudyResume","study_resume",action_id): return false
	if not check(harness,baseline.snapshot().tick==current.tick and main.kernel.study_activity().completed_lessons==frozen.lessons.size(),"resume_does_not_deliver_lesson"): return false
	if not await control(harness,main,baseline,"StudyPause","study_pause",action_id): return false
	if not await control(harness,main,baseline,"StudyCancel","study_cancel",action_id): return false
	current=baseline.snapshot()
	return check(harness,not current.actions.has(action_id) and current.facts[action_id].code=="StudyCancelled" and current.facts[action_id].lessons==frozen.lessons and main.find_child("StudyResume",true,false)==null,"terminal_cancel_once")
