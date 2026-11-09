## 行旅手记里的有限生活安排；世界真相、行动启动和预算采纳均由同一原生owner决定。
extends RefCounted
const Art=preload("res://ui/art_catalog.gd")
const Session=preload("res://ui/activity_session.gd")
const Controls=preload("res://ui/player_plan_controls.gd")
const LABELS={"move_local":"前往已知场所","travel":"行路","work":"普通短工","course":"普通课程","rest":"自宅休整","continue_action":"继续已接行动","task_reminder":"已接托付办理提醒"}
const PHASES={"queued":"尚未开始","active":"正在办理","awaiting_confirmation":"等待本人确认","completed":"已完成","blocked":"已停住"}
const ESTIMATE_SOURCES={"active_action_remaining":"正在办理的原行动余程","existing_action_remaining":"本人已接行动的剩余时间","captured_work_quote":"当面看过的原短工时长","captured_course_quote":"当面看过的原课程时长","own_rest_duration":"本人选定的自宅休整时长","known_local_move":"已知城内通路","current_travel_preview":"当前原路程报价","original_task_report":"原托付的实际办理要求","known_source":"本人已知的原来源","blocked_step":"当前受阻步骤","actual_result":"已经发生的原行动结果"}
const REASONS={"DurationUnknown":"所需时间尚不明确","PreviousStepUnknown":"前一步完成时间尚不明确","PlayerPlanActionReferenceMissing":"原行动已不存在，须本人核对","PlayerPlanActionPaused":"原行动已暂停，须先恢复","PlayerPlanManualInputRequired":"请先亲自处理当前题目、会面或战斗","PlayerPlanActionBlocked":"原行动当前受阻，完成时间尚不明确","PlayerPlanUnsupportedEstimate":"目前无法估计这一步的耗时","PlayerPlanProjectedOriginUnknown":"前段行路后的实际起点尚不明确","PlayerPlanTravelPreviewUnavailable":"当前无法取得原路程估计","PlayerPlanCapturedQuoteMissing":"原报价不完整，须回到原入口查看","PlayerPlanExistingActionTimingUnknown":"原行动须先处理，不能估计它在后续安排中的完成时刻","PlayerPlanTaskCompletionUnknown":"托付仍须本人实际办理和报告，完成时间尚不明确","PlayerPlanTaskNeedsConfirmation":"请在原入口亲自办理这件托付","PlayerPlanStepAuthorizationRequired":"这一步尚未获得本人开始许可","PlayerPlanTermsChanged":"原条款已改变，请回原入口查看并重新确认","PlayerPlanInteractionPending":"请先处理并结束当前交谈","PlayerPlanMembershipDecisionRequired":"有任职邀请待本人决定","PlayerPlanBudgetRequired":"请先确认本次半日与支出上限","PlayerPlanTimeBudgetExceeded":"本次半日预算已用完","PlayerPlanSpendBudgetExceeded":"本次支出预算不足","PlayerPlanPaused":"已暂停生活安排","PlayerPlanManualCommand":"本人已手动办理，请重新确认后续安排","PlayerPlanRestored":"已恢复存档，请确认条件和预算后继续","PlayerPlanTravelChanged":"原行程已改变，请本人核对后续安排","PlayerPlanCompleted":"这些安排已办完","StudyCancelled":"原课程已结束","RestCancelled":"原休整已结束","EncounterPending":"途中事情待本人处理","BattlePending":"请先处理当前战斗","SiegePending":"请先处理当前军议"}
const COMMITMENT_STATES={"active":"正在办理","ready":"可到原入口交付","settling":"原责任仍待清算","paused":"已暂停"}

## 复用手记入口与场景纸面，最多八行；打开页面不安排时间、不锁人也不接受新责任。
static func build(host, stage: Control, view: Dictionary) -> void:
	var panel=PanelContainer.new(); panel.name="PlayerPlanScreen"; stage.add_child(panel)
	panel.anchor_left=.025; panel.anchor_right=.975; panel.anchor_top=.115; panel.anchor_bottom=.975
	panel.add_theme_stylebox_override("panel",Art.ink())
	var column=VBoxContainer.new(); column.add_theme_constant_override("separation",10); panel.add_child(column)
	var heading=HBoxContainer.new(); column.add_child(heading)
	var title: Label=host._text(heading,"行旅手记 · 生活安排",27); title.size_flags_horizontal=Control.SIZE_EXPAND_FILL
	host._button(heading,"返回场所","player_plan_close","",host._close_player_plan).name="PlayerPlanClose"
	host._text(column,"只安排自己已知的下一段生活，最多八步。尚未开始的安排不预留钱粮、不预约提供人。",16)
	if not view.get("enabled",false): host._text(column,"本行旅未启用生活安排。",18); return
	var scroll=ScrollContainer.new(); scroll.name="PlayerPlanScroll"; scroll.size_flags_vertical=Control.SIZE_EXPAND_FILL; scroll.horizontal_scroll_mode=ScrollContainer.SCROLL_MODE_DISABLED; column.add_child(scroll)
	var body=VBoxContainer.new(); body.size_flags_horizontal=Control.SIZE_EXPAND_FILL; body.add_theme_constant_override("separation",12); scroll.add_child(body)
	var steps: Array=view.get("steps",[])
	host._text(body,"按当前已知条件预计；提供人不会因此等候，途中阻碍与待办输入会改变完成时刻。",16).name="PlayerPlanConditionalEstimate"
	if not view.get("stop_reason","").is_empty(): host._text(body,_reason(host,view.stop_reason),17).name="PlayerPlanStopReason"
	if steps.is_empty(): host._text(body,"还没有安排。在场所、短工或课程入口，将本人看过的目标记到这里。",18)
	for index in range(steps.size()): _step(host,body,steps,index)
	_conflicts(host,body,view.get("conflicts",[]))
	_commitments(host,body,view.get("commitments",[]))
	var footer=VBoxContainer.new(); footer.add_theme_constant_override("separation",8); column.add_child(footer)
	var session=Session.create(host,footer)
	if not steps.is_empty():
		session.command(footer,"暂停生活安排","player_plan_pause",{}).name="PlayerPlanPause"
		session.command(footer,"删除未开始的安排","player_plan_clear",{}).name="PlayerPlanClear"
		host._text(footer,"删除安排只停止以后要做的事。已经接受的课程、行程与托付仍由原行动处理。",15)
		_budget(host,footer,view)

## 每步显示原来源与估计完成边界，顺序编辑只发送稳定ID，不能把抵达当成任务履约。
static func _step(host, parent: Control, steps: Array, index: int) -> void:
	var row: Dictionary=steps[index]
	var box=VBoxContainer.new(); box.name="PlayerPlanStep"; box.set_meta("step_id",row.get("step_id","")); parent.add_child(box)
	var kind: String=row.get("kind",""); var params: Dictionary=row.get("params",{})
	host._text(box,"%d. %s · %s"%[index+1,LABELS.get(kind,"待核对的安排"),PHASES.get(row.get("phase",""),"待核对")],20)
	var place: String=params.get("place_id",params.get("context",{}).get("place_id",""))
	if not place.is_empty(): host._text(box,Controls.known_name(host,"places",place),17)
	if kind=="travel":
		host._text(box,"目的地："+Controls.known_name(host,"settlements",params.get("destination","")),17)
		var via: PackedStringArray=[]
		for id in params.get("via",[]): via.append(Controls.known_name(host,"settlements",id))
		if not via.is_empty(): host._text(box,"途经："+" → ".join(via),16)
	if kind=="rest": host._text(box,Controls.known_name(host,"estates",params.get("estate_id",""))+" · 已选%d个半日"%params.get("duration",0),17)
	if kind=="task_reminder": host._text(box,"仅记到场与办理目标；交付、报告和领奖仍须本人在原入口办理。",16)
	if kind in ["work","course"]:
		Controls.terms_text(host,box,row.get("terms",{}),params)
		host._text(box,"已逐步允许按原条款开始" if row.get("authorized",false) else "开始前停下，等待本人确认",15)
		if row.get("phase") not in ["active","completed"]: Controls.authorization(host,box,row)
	var estimate: String=("实际完成：" if row.get("estimate_source")=="actual_result" else "预计完成：")+_time(row.get("estimated_completion_tick"))
	if not row.get("unknown_reason","").is_empty(): estimate+=" · "+_reason(host,row.unknown_reason)
	host._text(box,estimate,17).name="PlayerPlanEstimatedCompletion"
	if not row.get("estimate_source","").is_empty(): host._text(box,"依据："+ESTIMATE_SOURCES.get(row.estimate_source,"本人已知条件，具体耗时仍待核对"),15)
	if not row.get("stop_reason","").is_empty(): host._text(box,"停止原因："+_reason(host,row.stop_reason),16)
	var actions=HBoxContainer.new(); box.add_child(actions); var session=Session.create(host,actions)
	if row.get("phase") not in ["active","completed"]:
		for delta in [-1,1]:
			var other: int=index+delta
			if other<0 or other>=steps.size() or steps[other].get("phase") in ["active","completed"]: continue
			var ids: Array=[]
			for item in steps: ids.append(item.step_id)
			var held: Variant=ids[index]; ids[index]=ids[other]; ids[other]=held
			session.command(actions,"提前一步" if delta<0 else "推后一步","player_plan_reorder",{"step_ids":ids})
		session.command(actions,"删除此安排","player_plan_remove",{"step_id":row.step_id}).name="PlayerPlanRemove"
	box.add_child(HSeparator.new())

## 冲突只是已知完成期限提醒；未知估计单独说明，不声称安全或替原责任改期。
static func _conflicts(host, parent: Control, conflicts: Array) -> void:
	if conflicts.is_empty(): return
	host._text(parent,"已知时限提醒",22)
	for row in conflicts:
		var text: String="完成时刻未知，不能判断能否按时办理" if row.get("unknown",false) else "预计完成晚于原时限" if row.get("known_conflict",false) else "按当前已知条件估计可及原时限"
		text+=" · 预计完成 "+_time(row.get("estimated_completion_tick"))+" · 原时限 "+_time(row.get("due_tick"))
		var label: Label=host._text(parent,text,17); label.name="PlayerPlanConflict"; label.set_meta("player_plan_conflict",row.duplicate(true))

## 托付列表只消费原任务owner给出的本人责任；可明确记一个提醒，不能接受或兑现托付。
static func _commitments(host, parent: Control, commitments: Array) -> void:
	if commitments.is_empty(): return
	host._text(parent,"本人已有责任",22)
	for row in commitments:
		var label: String=row.get("label","本人原托付")
		if row.get("kind")=="course": label=Controls.SKILL_NAMES.get(label,"原课程")
		var timing: String="预计完成 "+_time(row.get("estimated_completion_tick")) if row.get("kind")=="course" else "完成时限 "+_time(row.get("due_tick"))
		host._text(parent,label+" · "+COMMITMENT_STATES.get(row.get("status",""),"待核对")+" · "+timing,16)
		if row.get("kind")=="task" and row.has("place_id"):
			Controls.append(host,parent,"task_reminder",{"task_id":row.source_id,"place_id":row.place_id},"将这件托付记为办理提醒")

## 每次开始都显示独立半日和私人毛支出上限；收入不扩额，许可由既有有限驱动负责。
static func _budget(host, parent: Control, view: Dictionary) -> void:
	var root=VBoxContainer.new(); root.name="PlayerPlanBudget"; parent.add_child(root)
	var source: Dictionary=host.kernel.source_identity(); var epoch: int=host._scene_counter
	var state: Dictionary=view.get("budget",{})
	if not state.is_empty(): host._text(root,"原许可剩余 %d 个半日 · 最多再支出 %d 钱"%[maxi(0,state.get("max_additional_ticks",0)-state.get("used_ticks",0)),maxi(0,state.get("max_gross_spend",0)-state.get("gross_spend",0))],16)
	var ticks=SpinBox.new(); ticks.name="PlayerPlanHalfDays"; ticks.min_value=1; ticks.max_value=720; ticks.step=1; ticks.rounded=true; ticks.value=1
	var money=SpinBox.new(); money.name="PlayerPlanMoney"; money.min_value=0; money.max_value=2147483647; money.step=1; money.rounded=true; money.value=0
	host._text(root,"本次最多追加半日",15); root.add_child(ticks)
	host._text(root,"本次私人钱包毛支出上限（钱）",15); root.add_child(money)
	var risk=CheckBox.new(); risk.name="PlayerPlanKeepRisk"; risk.text="我已看过以上时限与未知项，仍保留当前顺序"; risk.button_pressed=false; root.add_child(risk)
	var needs_risk: bool=false
	for row in view.get("conflicts",[]): needs_risk=needs_risk or row.get("known_conflict",false) or row.get("unknown",false)
	risk.visible=needs_risk
	var session=Session.create(host,root)
	var start: Button=host._button(root,"确认本次预算并开始安排","player_plan_begin","",func():
		if not session.guard().get("ok",false) or (needs_risk and not risk.button_pressed): return
		host._begin_player_plan(int(ticks.value),int(money.value),source,epoch))
	start.name="PlayerPlanBegin"; start.disabled=needs_risk
	risk.toggled.connect(func(checked: bool):
		if session.guard().get("ok",false): start.disabled=needs_risk and not checked)
	host._text(root,"每步开始仍核原条件。手动题、重要决定和未授权新动作都会停下；存档后须本人再次确认。",15)

## 估计未知保持未知，绝不用第零半日冒充保证能够完成。
static func _time(value: Variant) -> String:
	if value==null or typeof(value)!=TYPE_INT or value<0: return "未知"
	return "第%d日%s（半日%d）"%[value/2+1,"上半日" if value%2==0 else "下半日",value]

## 有限已知原因使用玩家能理解的说法；新协议码只保诊断，不直接泄入玩法文字。
static func _reason(host, code: String) -> String:
	if REASONS.has(code): return REASONS[code]
	var original: String=host._reason(code)
	return original if not original.is_empty() and not original.contains(code) else "当前条件尚不明确，请到原入口核对"
