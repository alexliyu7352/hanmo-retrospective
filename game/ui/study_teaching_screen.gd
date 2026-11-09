## 普通授课页只消费当前学生明确请求与本人历史，支付和成长由原课程命令拥有。
extends RefCounted
const SKILLS={"infantry":"步兵","cavalry":"骑兵","archery":"弓术","martial":"武艺","tactics":"兵法","debate":"辩论","negotiation":"议价","rites":"礼法","commerce":"商术","transport":"运输","naval":"水战","espionage":"谍报","fortification":"筑城","agriculture":"农政","medicine":"医术","forging":"锻造"}

const REASONS={"InsufficientFunds":"学生的钱款不足以支付这门课程。","StudyTeacherNotQualified":"当前教师技艺尚不足以教授这位学生。","ActorBusy":"双方当前须先结束已有行动。","StudyTeachingRequestConsumed":"这次求教已经办理；新课程需要学生再次明确提出。","StudyAlreadyMastered":"学生已达到这项技艺的最高等级。","StudyCourseLevelLimit":"学生已达到这门课程可教授的上限。"}

static func _source(host) -> Dictionary:
	var command: Dictionary=host.kernel.command_context()
	var identity: Dictionary=host.kernel.source_identity()
	var exact: Dictionary={}
	for key in ["world_id","execution_epoch","revision","tick"]: exact[key]=identity.get(key)
	return {"ok":command.get("ok",false),"actor_id":command.get("actor_id",""),"kernel_id":host.kernel.get_instance_id(),"revision":command.get("revision",-1),"identity":exact}

static func _live(host_ref: WeakRef, panel_ref: WeakRef, source: Dictionary) -> bool:
	var host=host_ref.get_ref(); var panel: Control=panel_ref.get_ref()
	return is_instance_valid(host) and is_instance_valid(panel) and panel.is_inside_tree() and panel.is_visible_in_tree() and not panel.is_queued_for_deletion() and not host._advancing and source.get("ok",false) and _source(host)==source

static func _submit(host_ref: WeakRef, panel_ref: WeakRef, button_ref: WeakRef, status_ref: WeakRef, source: Dictionary, kind: String, params: Dictionary) -> void:
	var button: Button=button_ref.get_ref(); var status: Label=status_ref.get_ref()
	if not is_instance_valid(button) or not is_instance_valid(status): return
	if not _live(host_ref,panel_ref,source): button.disabled=true; status.text="人物或课程状态已变化，请重新打开。"; return
	var host=host_ref.get_ref(); var panel: Control=panel_ref.get_ref()
	if button.disabled or panel.get_meta("teaching_pending",false): return
	var quote: Dictionary=host.kernel.preview(kind,params)
	if not quote.get("ok",false) or quote.get("revision",-1)!=source.revision:
		button.disabled=true; status.text="当前条件不能办理，请刷新查看。"; return
	if not _live(host_ref,panel_ref,source): return
	panel.set_meta("teaching_pending",true); button.disabled=true
	host._perform(kind,params.duplicate(true))

static func _command(host, box: Control, panel: Control, status: Label, source: Dictionary, title: String, kind: String, params: Dictionary, name: String) -> Button:
	var binding: Dictionary={"button":null}
	var host_ref: WeakRef=weakref(host); var panel_ref: WeakRef=weakref(panel); var status_ref: WeakRef=weakref(status)
	var request: Dictionary=params.duplicate(true)
	var button: Button=host._button(box,title,kind,"",func(): _submit(host_ref,panel_ref,binding.button,status_ref,source,kind,request))
	binding.button=weakref(button); button.name=name
	return button

static func build(host, parent: Control, view: Dictionary, on_close: Callable, on_page: Callable) -> Control:
	var panel=PanelContainer.new(); panel.name="StudyTeachingScreen"; parent.add_child(panel)
	panel.anchor_left=.04; panel.anchor_right=.96; panel.anchor_top=.12; panel.anchor_bottom=.96
	var column=VBoxContainer.new(); column.add_theme_constant_override("separation",10); panel.add_child(column)
	var source: Dictionary=_source(host)
	source.ok=source.ok and view.get("actor_id")==source.actor_id and view.get("source_identity")==source.identity
	var host_ref: WeakRef=weakref(host); var panel_ref: WeakRef=weakref(panel)
	var header=HBoxContainer.new(); column.add_child(header)
	var title: Label=host._text(header,"授课",26); title.size_flags_horizontal=Control.SIZE_EXPAND_FILL
	var close: Button=host._button(header,"返回","study_teaching_close","",func():
		var current: Control=panel_ref.get_ref()
		if is_instance_valid(current) and current.is_inside_tree() and not current.is_queued_for_deletion() and on_close.is_valid(): current.hide(); on_close.call())
	close.name="StudyTeachingClose"
	var status: Label=host._text(column,"学生按本次约定用本人钱款付费；收入归实际课程指定的教师或组织。",16); status.name="StudyTeachingStatus"
	if not source.ok: status.text="课程来源已过期，请重新打开。"
	var scroll=ScrollContainer.new(); scroll.size_flags_vertical=Control.SIZE_EXPAND_FILL; scroll.horizontal_scroll_mode=ScrollContainer.SCROLL_MODE_DISABLED; column.add_child(scroll)
	var rows=VBoxContainer.new(); rows.size_flags_horizontal=Control.SIZE_EXPAND_FILL; rows.add_theme_constant_override("separation",12); scroll.add_child(rows)
	for item in view.get("requests",[]):
		var box=VBoxContainer.new(); rows.add_child(box)
		host._text(box,"%s求教%s · %d个半日"%[item.student_name,SKILLS.get(item.skill_id,item.skill_id),item.duration],21)
		if item.get("eligible",false): host._text(box,"本次学费%d钱；按当前条件，学员预计成长%d经验。"%[item.tuition,item.expected_xp],17)
		else: host._text(box,REASONS.get(item.get("code",""),"当前无法开课，请重新查看学生请求与教学资格。"),16)
		var accept: Button=_command(host,box,panel,status,source,"按本次条款授课","teach_study",item.params,"StudyTeachingAccept")
		accept.disabled=not source.ok or not item.get("eligible",false)
	if view.get("requests",[]).is_empty(): host._text(rows,"当前没有可办理的明确求教请求。",18)
	var active: Dictionary=view.get("active_course",{})
	if active.get("active",false):
		host._text(rows,"正在教授%s：%s，已上%d/%d课，实付%d钱。"%[active.student_name,SKILLS.get(active.skill_id,active.skill_id),active.completed_lessons,active.duration,active.fee_paid],20)
		var controls=HBoxContainer.new(); rows.add_child(controls)
		if not active.get("paused",false): _command(host,controls,panel,status,source,"继续一个实际半日","advance",{},"StudyTeachingAdvance").disabled=not source.ok
		if active.get("pause_supported",false):
			var kind: String="study_resume" if active.get("paused",false) else "study_pause"
			_command(host,controls,panel,status,source,"继续授课" if active.get("paused",false) else "暂停授课",kind,{"action_id":active.action_id},"StudyTeachingPause").disabled=not source.ok
		_command(host,controls,panel,status,source,"结束这门课程","study_cancel",{"action_id":active.action_id},"StudyTeachingCancel").disabled=not source.ok
	for item in view.get("history",[]):
		host._text(rows,"%s · %s：已上%d/%d课，实付%d钱，学员成长%d经验%s"%[item.student_name,SKILLS.get(item.skill_id,item.skill_id),item.completed_lessons,item.duration,item.fee_paid,item.xp_gained,"，课程完成" if item.success else "，课程已结束"],17)
	var paging=HBoxContainer.new(); column.add_child(paging)
	var page: int=int(view.get("page",0)); var total: int=maxi(int(view.get("request_count",0)),int(view.get("history_count",0)))
	for delta in [-1,1]:
		var next_page: int=page+delta
		var button: Button=host._button(paging,"上一页" if delta<0 else "下一页","study_teaching_page","",func():
			if next_page>=0 and next_page*16<total and _live(host_ref,panel_ref,source) and on_page.is_valid(): on_page.call(next_page))
		button.name="StudyTeachingPrevious" if delta<0 else "StudyTeachingNext"
		button.disabled=not source.ok or next_page<0 or next_page*16>=total
	return panel
