## 师承页只办理实际双方意愿，普通课程沿原入口；界面不改关系、技艺或控制人物。
extends RefCounted
const Guard=preload("res://ui/family_legacy_screen.gd")
const SKILLS={"archery":"弓术","cavalry":"骑术","commerce":"商术","debate":"辩才","infantry":"步战","martial":"武艺","negotiation":"交涉","rites":"礼学","tactics":"兵法","transport":"运输","naval":"水战","espionage":"谍报","fortification":"筑城","agriculture":"农政","medicine":"医术","forging":"锻造"}
const REASONS={"ApprenticeshipAdultRequired":"本次成年师承需要双方已满十八岁。","ActorAgeUnknown":"原生日来源尚未核清。","ApprenticeshipStatementNotCurrent":"请先亲自回到对方表示师承意愿的会面节点。","ApprenticeshipProposalPending":"已有这项师承提议，请另行答复。","ApprenticeshipAlreadyActive":"已有同一双方与技艺的师承记录。","ApprenticeshipProposalNotPending":"原提议已结束或当前不能办理。","ApprenticeshipUnavailable":"当前无法办理；未收到的对方消息不会自动公开。","ApprenticeshipDecisionMakerBusy":"请先处理本人正在进行的行动。","ApprenticeshipAvailabilitySourceRequired":"当时处境来源尚不能核清。"}

## 每次本人决定先预览，再明确确认；同一次身份/版本失效时旧按钮不提交。
static func _submit(host_ref: WeakRef,panel_ref: WeakRef,button_ref: WeakRef,status_ref: WeakRef,source: Dictionary,operation: Dictionary) -> void:
	var button: Button=button_ref.get_ref(); var status: Label=status_ref.get_ref()
	if not is_instance_valid(button) or not is_instance_valid(status): return
	if not Guard._live(host_ref,panel_ref,source): button.disabled=true; status.text="人物或世界已改变，请重新打开师承。"; return
	var host=host_ref.get_ref(); var panel: Control=panel_ref.get_ref()
	if button.disabled or panel.get_meta("apprenticeship_pending",false): return
	var kind: String=operation.get("kind",""); var params: Dictionary=operation.get("params",{}).duplicate(true)
	var quote: Dictionary=host.kernel.preview(kind,params)
	if quote.get("code")=="NativeAsyncRequired": status.text="正在核对本人和原会面，请稍后确认。"; return
	if not quote.get("ok",false) or quote.get("revision",-1)!=source.revision: status.text=REASONS.get(quote.get("code",""),"当前本人、原提议或处境不能办理，请重新核对。"); return
	var exact={"kind":kind,"params":params}
	if button.get_meta("confirmed",{})!=exact:
		button.set_meta("confirmed",exact); button.text="再次确认"
		if kind=="family_apprenticeship_receive": status.text="再次确认记录亲自听到的师承提议；这一步尚未接受。"
		elif kind=="family_apprenticeship_exit": status.text="再次确认本人退出这项师承；已经学到的本领、原课程与未结责任保留。"
		elif params.get("decision")=="accepted": status.text="再次确认本人接受这项师承；不转财物，不增加经验，也不接受继任。"
		elif params.get("decision")=="refused": status.text="再次确认本人拒绝这份提议，不形成师承关系。"
		else: status.text="再次确认撤回本人提出的原提议。"
		return
	if not Guard._live(host_ref,panel_ref,source): return
	panel.set_meta("apprenticeship_pending",true); button.disabled=true; host._perform(kind,params)

## 参数只来自本页原生有限操作，不提供任意人物或技艺的输入框。
static func _operation(host,parent: Control,label: String,operation: Dictionary,status: Label,host_ref: WeakRef,panel_ref: WeakRef,source: Dictionary) -> void:
	var binding={"button":null}; var exact: Dictionary=operation.duplicate(true)
	var button: Button=host._button(parent,label,exact.get("kind",""),"",func(): _submit(host_ref,panel_ref,binding.button,weakref(status),source,exact))
	binding.button=weakref(button); button.name="ApprenticeshipAction"; button.disabled=not source.ok or not exact.get("available",false)
	if not exact.get("available",false): host._text(parent,REASONS.get(exact.get("code",""),"当前不能办理，请核对本人来源。"),16)

## 独立历史页只显示真实原课程，未完成的预期经验不冒已得；最多消费原生每页十六条。
static func build(host,parent: Control,view: Dictionary,on_close: Callable,on_page: Callable,on_courses: Callable) -> Control:
	var panel=PanelContainer.new(); panel.name="ApprenticeshipScreen"; parent.add_child(panel)
	panel.anchor_left=.04; panel.anchor_right=.96; panel.anchor_top=.12; panel.anchor_bottom=.96
	var column=VBoxContainer.new(); column.add_theme_constant_override("separation",10); panel.add_child(column)
	var source: Dictionary=Guard._source(host)
	if view.get("actor_id")!=source.actor_id or view.get("source_identity")!=source.identity: source.ok=false
	var host_ref: WeakRef=weakref(host); var panel_ref: WeakRef=weakref(panel)
	var title=HBoxContainer.new(); column.add_child(title)
	host._text(title,"本人师承与实际课程",25).size_flags_horizontal=Control.SIZE_EXPAND_FILL
	host._button(title,"返回家事","apprenticeship_close","",func():
		if Guard._live(host_ref,panel_ref,source) and on_close.is_valid(): panel_ref.get_ref().hide(); on_close.call())
	var status: Label=host._text(column,view.get("explanation","师承需要双方真实自愿。"),17)
	var scroll=ScrollContainer.new(); scroll.horizontal_scroll_mode=ScrollContainer.SCROLL_MODE_DISABLED; scroll.size_flags_vertical=Control.SIZE_EXPAND_FILL; column.add_child(scroll)
	var body=VBoxContainer.new(); body.size_flags_horizontal=Control.SIZE_EXPAND_FILL; body.add_theme_constant_override("separation",10); scroll.add_child(body)
	for row in view.get("statements",[]):
		host._text(body,"%s与%s · %s师承意愿"%[row.teacher.name,row.student.name,SKILLS.get(row.skill_id,row.skill_id)],20)
		_operation(host,body,"听取这份提议",row.operation,status,host_ref,panel_ref,source)
	for row in view.get("proposals",[]):
		body.add_child(HSeparator.new()); host._text(body,"%s授%s · %s · 第%d半日原提议"%[row.teacher.name,row.student.name,SKILLS.get(row.skill_id,row.skill_id),int(row.received_tick)],19)
		host._text(body,{"accepted":"本人接受已经记录","refused":"本人的拒绝已记录","withdrawn":"本人撤回已记录"}.get(row.get("known_response",""),"尚无本人已获知的终态答复。"),17)
		for operation in row.get("operations",[]):
			var decision: String=operation.get("params",{}).get("decision","")
			_operation(host,body,{"accepted":"本人接受师承","refused":"本人拒绝","withdrawn":"撤回本人提议"}.get(decision,"本人答复"),operation,status,host_ref,panel_ref,source)
	for row in view.get("bonds",[]):
		body.add_child(HSeparator.new()); host._text(body,"%s与%s · %s · 第%d半日师承记录"%[row.teacher.name,row.student.name,SKILLS.get(row.skill_id,row.skill_id),int(row.accepted_tick)],20)
		if not row.get("known_exit_fact_id","").is_empty(): host._text(body,"本人已退出；原经历与已学成果保留。",17)
		var bond_id: String=row.bond_id
		var courses: Button=host._button(body,"查看原实际课程","apprenticeship_courses",bond_id,func():
			if Guard._live(host_ref,panel_ref,source) and on_courses.is_valid(): on_courses.call(bond_id))
		courses.disabled=not source.ok
		for operation in row.get("operations",[]): _operation(host,body,"本人退出这项师承",operation,status,host_ref,panel_ref,source)
	for row in view.get("rows",[]):
		host._text(body,"第%d半日开课 · 已上%d课 · 实得%d经验%s"%[int(row.start_tick),int(row.actual_lessons),int(row.credited_xp)," · 已结束" if row.phase=="completed" else " · 原课程仍在进行"],18)
	if not view.has("bond_id") and view.get("statements",[]).is_empty(): host._text(body,"请先在场所亲自交谈，达到对方明确表示师承意愿的节点。",17)
	var study: Button=host._button(column,"前往原普通课程","apprenticeship_original_study","",func():
		if Guard._live(host_ref,panel_ref,source) and host_ref.get_ref().has_method("_open_courses"): host_ref.get_ref()._open_courses())
	study.disabled=not source.ok
	var pager=HBoxContainer.new(); column.add_child(pager)
	var total: int=int(view.get("total",maxi(int(view.get("statement_count",0)),maxi(int(view.get("proposal_count",0)),int(view.get("bond_count",0))))))
	for delta in [-1,1]:
		var target: int=int(view.get("page",0))+delta
		var button: Button=host._button(pager,"上一页" if delta<0 else "下一页","apprenticeship_page","",func():
			if Guard._live(host_ref,panel_ref,source) and on_page.is_valid(): on_page.call(target))
		button.disabled=not source.ok or target<0 or target>4095 or target*16>=total
	return panel

## 显式新模块才进入本页，回到家事不会退出关系或取消课程。
static func selected(host) -> bool:
	return host._resident_mode and host.get_meta("family_tab","family")=="family_apprenticeship" and host.kernel.has_method("family_apprenticeship_enabled") and host.kernel.family_apprenticeship_enabled()
static func _return(host_ref: WeakRef) -> void:
	var host=host_ref.get_ref()
	if is_instance_valid(host) and host._family_open: host.set_meta("family_tab","family"); host.set_meta("apprenticeship_bond",""); host._render()

## 导航只持当前世界UI草案；换人/读档/版本变化重新读取，不续用上一世界的按钮。
static func render(host,parent: Control) -> void:
	var source: Dictionary=Guard._source(host)
	if host.get_meta("apprenticeship_navigation_source",{})!=source: host.set_meta("apprenticeship_navigation_source",source.duplicate(true)); host.set_meta("apprenticeship_page",0); host.set_meta("apprenticeship_bond","")
	var ref: WeakRef=weakref(host); var page: int=int(host.get_meta("apprenticeship_page",0)); var bond: String=host.get_meta("apprenticeship_bond","")
	var view: Dictionary=host.kernel.family_apprenticeship_page(page) if bond.is_empty() else host.kernel.family_apprenticeship_courses(bond,page)
	build(host,parent,view,_return.bind(ref),func(next_page: int):
		if is_instance_valid(ref.get_ref()) and selected(ref.get_ref()) and not ref.get_ref()._advancing and next_page>=0 and next_page<=4095: ref.get_ref().set_meta("apprenticeship_page",next_page); ref.get_ref()._render(),func(bond_id: String):
		if is_instance_valid(ref.get_ref()) and selected(ref.get_ref()) and not ref.get_ref()._advancing: ref.get_ref().set_meta("apprenticeship_bond",bond_id); ref.get_ref().set_meta("apprenticeship_page",0); ref.get_ref()._render())
