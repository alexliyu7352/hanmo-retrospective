## 只展示当前教师的真实提议与本人有限履历；课时、付款和习得由原生同一候选提交。
extends RefCounted
const Guard=preload("res://ui/family_legacy_screen.gd")
const REASONS={"TacticLessonAlreadyLearned":"本人已经学会这项战法，无须重复付费。","TacticLessonTeacherKnowledgeRequired":"目前没有可核实的教师掌握来源。","TacticLessonParticipantUnavailable":"双方需先处理各自原行动，才能开始授课。","TacticLessonOfferNotCurrent":"请亲自交谈，回到教师本人提出授课的节点。","InsufficientFunds":"本人可用钱不足以预约本次全部学费。","InvalidTacticLessonPrerequisite":"真实基础技艺尚未达到要求；本半日刚学到的经验须下一半日才计入报名资格。","MilitaryOccupationHistoryIncomplete":"原军事占用历史尚不可证，当前不能办理。"}
const ENDINGS={"completed":"实际教学已完成","student_cancelled":"本人停止未教部分","student_dead":"学生死亡，余下授课终止","teacher_dead":"教师死亡，余下授课终止","student_unavailable":"学生因真实军事事件中断","teacher_unavailable":"教师因真实军事事件中断"}

static func _submit(host_ref: WeakRef,panel_ref: WeakRef,button_ref: WeakRef,status_ref: WeakRef,source: Dictionary,kind: String,params: Dictionary) -> void:
	var button: Button=button_ref.get_ref(); var status: Label=status_ref.get_ref()
	if not is_instance_valid(button) or not is_instance_valid(status): return
	if not Guard._live(host_ref,panel_ref,source): button.disabled=true; status.text="人物或世界已改变，请重新打开授课页。"; return
	var panel: Control=panel_ref.get_ref(); var host=host_ref.get_ref()
	if button.disabled or panel.get_meta("lesson_pending",false): return
	var quote: Dictionary=host.kernel.preview(kind,params)
	if quote.get("code")=="NativeAsyncRequired": status.text="正在核对原教师、课时与学费，请稍后确认。"; return
	if not quote.get("ok",false) or quote.get("revision",-1)!=source.revision: status.text=REASONS.get(quote.get("code",""),"当前原来源或处境不能办理，请重新核对。"); return
	var operation={"kind":kind,"params":params.duplicate(true)}
	if button.get_meta("confirmed",{})!=operation:
		button.set_meta("confirmed",operation); button.text="再次确认"
		status.text="确认本人报名：预约本页完整学费，每个真实半日才向教师付款，完成全部教学后才登记习得。" if kind=="tactic_lesson_start" else "确认停止原课程：已提供教学和已付费用保留，只释放余下未教预约。"
		return
	if not Guard._live(host_ref,panel_ref,source): return
	panel.set_meta("lesson_pending",true); button.disabled=true; host._perform(kind,params.duplicate(true))

static func _button(host,parent: Control,label: String,kind: String,params: Dictionary,enabled: bool,status: Label,host_ref: WeakRef,panel_ref: WeakRef,source: Dictionary) -> void:
	var binding={"button":null}; var saved=params.duplicate(true)
	var button: Button=host._button(parent,label,kind,"",func(): _submit(host_ref,panel_ref,binding.button,weakref(status),source,kind,saved))
	binding.button=weakref(button); button.name="TacticLessonDecision"; button.disabled=not source.ok or not enabled

static func build(host,parent: Control,view: Dictionary,on_close: Callable,on_page: Callable) -> Control:
	var panel=PanelContainer.new(); panel.name="TacticLessonScreen"; parent.add_child(panel)
	panel.anchor_left=.04; panel.anchor_right=.96; panel.anchor_top=.12; panel.anchor_bottom=.96
	var column=VBoxContainer.new(); panel.add_child(column); column.add_theme_constant_override("separation",10)
	var source: Dictionary=Guard._source(host)
	if view.get("actor_id")!=source.actor_id or view.get("source_identity")!=source.identity: source.ok=false
	var host_ref=weakref(host); var panel_ref=weakref(panel)
	var header=HBoxContainer.new(); column.add_child(header); host._text(header,"战法实际授课",25).size_flags_horizontal=Control.SIZE_EXPAND_FILL
	host._button(header,"返回","tactic_lesson_close","",func():
		if Guard._live(host_ref,panel_ref,source) and on_close.is_valid(): panel_ref.get_ref().hide(); on_close.call())
	var status: Label=host._text(column,view.get("explanation",""),17)
	var scroll=ScrollContainer.new(); scroll.horizontal_scroll_mode=ScrollContainer.SCROLL_MODE_DISABLED; scroll.size_flags_vertical=Control.SIZE_EXPAND_FILL; column.add_child(scroll)
	var body=VBoxContainer.new(); body.size_flags_horizontal=Control.SIZE_EXPAND_FILL; body.add_theme_constant_override("separation",10); scroll.add_child(body)
	for row in view.get("offers",[]):
		host._text(body,"%s本人提出授课 · %s · %d半日 · 共%d钱"%[row.teacher_name,row.tactic_id,int(row.duration),int(row.total_fee)],19)
		_button(host,body,"本人报名","tactic_lesson_start",row.params,row.get("eligible",false),status,host_ref,panel_ref,source)
		if not row.get("eligible",false): host._text(body,REASONS.get(row.get("code",""),"当前条件不满足，请核对原会面。"),16)
	if view.get("active_course") is Dictionary:
		var active: Dictionary=view.active_course
		host._text(body,"正在学习%s · 已教%d/%d半日 · 已付%d钱 · 原预约余%d钱"%[active.tactic_id,int(active.provided_half_days),int(active.duration),int(active.fee_paid),int(active.remaining_fee)],19)
		host._text(body,"继续授课沿原行动推进；不得用本页按钮直接增加课时或战法。",17)
		_button(host,body,"停止未教部分","tactic_lesson_cancel",active.cancel_params,active.get("can_cancel",false),status,host_ref,panel_ref,source)
	elif view.get("offers",[]).is_empty(): host._text(body,"请先亲自找到真正能够教授战法的教师，并听到其本人授课提议。",18)
	for row in view.get("history",[]):
		body.add_child(HSeparator.new()); host._text(body,"%s · %s · 已教%d/%d半日"%[row.tactic_id,ENDINGS.get(row.reason,"原课程结束"),int(row.provided_half_days),int(row.duration)],18)
		host._text(body,"原教师%s · 实付%d钱 · 释放未教预约%d钱"%[row.teacher_name,int(row.fee_paid),int(row.fee_released)],17)
	var pager=HBoxContainer.new(); column.add_child(pager)
	for delta in [-1,1]:
		var target: int=int(view.get("page",0))+delta
		var button: Button=host._button(pager,"上一页" if delta<0 else "下一页","tactic_lesson_page","",func():
			if Guard._live(host_ref,panel_ref,source) and on_page.is_valid(): on_page.call(target))
		button.disabled=not source.ok or target<0 or target>62500 or target*16>=int(view.get("history_count",0))
	return panel

## 页面导航没有课程或资产写入。
static func selected(host) -> bool:
	return host._resident_mode and host.get_meta("tactic_lessons_open",false) and host.kernel.has_method("tactic_lessons_enabled") and host.kernel.tactic_lessons_enabled()
static func open(host) -> void:
	host.set_meta("tactic_lessons_open",true); host.set_meta("tactic_lessons_page",0); host._render()
static func _return(host_ref: WeakRef) -> void:
	var host=host_ref.get_ref()
	if is_instance_valid(host): host.set_meta("tactic_lessons_open",false); host._render()
static func render(host,parent: Control) -> void:
	var source: Dictionary=Guard._source(host)
	if host.get_meta("tactic_lessons_source",{})!=source: host.set_meta("tactic_lessons_source",source.duplicate(true)); host.set_meta("tactic_lessons_page",0)
	var ref: WeakRef=weakref(host); var view: Dictionary=host.kernel.tactic_lessons_page(int(host.get_meta("tactic_lessons_page",0)))
	build(host,parent,view,_return.bind(ref),func(page: int):
		if is_instance_valid(ref.get_ref()) and selected(ref.get_ref()) and not ref.get_ref()._advancing and page>=0 and page<=62500: ref.get_ref().set_meta("tactic_lessons_page",page); ref.get_ref()._render())
