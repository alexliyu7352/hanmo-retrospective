## 婚议只显示本人实际听到的意愿和原经历；听取、接受及解除分别明确确认。
extends RefCounted
const Guard=preload("res://ui/family_legacy_screen.gd")
const REASONS={"MarriageOriginalStatusUnknown":"原婚姻处境尚未证实，不能假定未婚。","MarriageExistingBond":"已有婚姻关系，不能办理新的关系。","MarriageCurrentStatusUnconfirmed":"本人当前婚姻处境尚未得到可靠确认。","MarriageKnownCloseRelation":"已知直系或收养手足关系不符合本次婚议条件。","MarriageRequiresAdult":"双方须已满十八岁。","MarriageAgeUnknown":"原年龄来源不足，暂不能办理。","MarriageStatementNotCurrent":"请回到对方明确表示意愿的原会面节点。","MarriageProposalAlreadyReceived":"已听取同一份提议，请在下方独立答复。","MarriageProposalAlreadyAnswered":"这份提议已有本人决定，不能重复办理。","MarriageAvailabilityHistoryRequired":"当事人的处境来源尚不能核清。","MarriageDissolutionBondMismatch":"原婚姻或双方解除条款不符合当前来源。"}

## 报价与第二次确认均绑定完整世界来源，旧回调不能跨人物或跨读档办理。
static func _submit(host_ref: WeakRef,panel_ref: WeakRef,button_ref: WeakRef,status_ref: WeakRef,source: Dictionary,kind: String,params: Dictionary) -> void:
	var button: Button=button_ref.get_ref(); var status: Label=status_ref.get_ref()
	if not is_instance_valid(button) or not is_instance_valid(status): return
	if not Guard._live(host_ref,panel_ref,source): button.disabled=true; status.text="人物或世界已改变，请重新打开婚议。"; return
	var host=host_ref.get_ref(); var panel: Control=panel_ref.get_ref()
	if button.disabled or panel.get_meta("marriage_pending",false): return
	var quote: Dictionary=host.kernel.preview(kind,params)
	if quote.get("code")=="NativeAsyncRequired": status.text="正在核对原双方意愿，请稍候再确认。"; return
	if not quote.get("ok",false) or quote.get("revision",-1)!=source.revision: status.text=REASONS.get(quote.get("code",""),"当前本人、原会面或双方处境不能办理。请核对来源。"); return
	var exact={"kind":kind,"params":params.duplicate(true)}
	if button.get_meta("confirmed",{})!=exact:
		button.set_meta("confirmed",exact); button.text="再次确认"
		if kind=="family_marriage_receive": status.text="再次确认记录亲自听到的提议；这一步尚未答应婚姻。"
		elif kind=="family_marriage_dissolve": status.text="再次确认双方解除这段个人关系；原私产、住处、公职和照护责任保持。"
		elif params.get("decision")=="accepted": status.text="再次确认本人愿意与对方建立个人婚姻关系；不转移财物，也不附带生育或迁居许可。"
		elif params.get("decision")=="refused": status.text="再次确认本人拒绝这份提议，对方不能代为接受。"
		else: status.text="再次确认撤回本人提出的尚未生效提议。"
		return
	if not Guard._live(host_ref,panel_ref,source): return
	panel.set_meta("marriage_pending",true); button.disabled=true; host._perform(kind,params)

## 每个操作只传原生领域认可的精确参数，界面不设置配偶字段。
static func _operation(host,parent: Control,text: String,kind: String,params: Dictionary,available: bool,status: Label,host_ref: WeakRef,panel_ref: WeakRef,source: Dictionary) -> void:
	var binding={"button":null}
	var button: Button=host._button(parent,text,kind,"",func(): _submit(host_ref,panel_ref,binding.button,weakref(status),source,kind,params))
	button.name="MarriageAction"; binding.button=weakref(button); button.disabled=not source.ok or not available

## 有限页只消费同身份DTO；当前全局丧偶真值不会被当作人物已经获知的消息。
static func build(host,parent: Control,view: Dictionary,on_close: Callable,on_page: Callable) -> Control:
	var panel=PanelContainer.new(); panel.name="FamilyMarriageScreen"; parent.add_child(panel)
	panel.anchor_left=.04; panel.anchor_right=.96; panel.anchor_top=.12; panel.anchor_bottom=.96
	var column=VBoxContainer.new(); panel.add_child(column); column.add_theme_constant_override("separation",10)
	var source: Dictionary=Guard._source(host)
	if view.get("actor_id")!=source.actor_id or view.get("source_identity")!=source.identity: source.ok=false
	var host_ref: WeakRef=weakref(host); var panel_ref: WeakRef=weakref(panel)
	var title=HBoxContainer.new(); column.add_child(title); host._text(title,"本人婚议与关系经历",25).size_flags_horizontal=Control.SIZE_EXPAND_FILL
	host._button(title,"返回家事","marriage_close","",func():
		var current: Control=panel_ref.get_ref()
		if is_instance_valid(current) and current.is_inside_tree() and current.is_visible_in_tree() and on_close.is_valid(): current.hide(); on_close.call())
	var status: Label=host._text(column,view.get("explanation","婚议须双方当事人真实自愿。"),17)
	var scroll=ScrollContainer.new(); scroll.horizontal_scroll_mode=ScrollContainer.SCROLL_MODE_DISABLED; scroll.size_flags_vertical=Control.SIZE_EXPAND_FILL; column.add_child(scroll)
	var body=VBoxContainer.new(); body.size_flags_horizontal=Control.SIZE_EXPAND_FILL; scroll.add_child(body)
	for row in view.get("current_statements",[]):
		host._text(body,"%s当前亲口表达的意愿"%row.other.name,20)
		var params={"statement_id":row.statement_id}
		if row.command=="family_marriage_dissolve": params["marriage_id"]=row.marriage_id
		_operation(host,body,"听取这份婚议" if row.command=="family_marriage_receive" else "双方解除个人关系",row.command,params,row.get("available",false),status,host_ref,panel_ref,source)
		if not row.get("available",false): host._text(body,REASONS.get(row.get("code",""),"当前来源未满足办理条件。"),16)
	for row in view.get("proposals",[]):
		body.add_child(HSeparator.new()); host._text(body,"与%s的原提议 · 第%d半日"%[row.other.name,int(row.received_tick)],20)
		if not row.get("response","").is_empty(): host._text(body,{"accepted":"本人已接受","refused":"本人已拒绝","withdrawn":"提议人已撤回"}.get(row.response,"原决定已记录"),17)
		for choice in row.get("options",[]):
			_operation(host,body,{"accepted":"本人愿意接受","refused":"本人拒绝","withdrawn":"撤回本人提议"}.get(choice.decision,"本人答复"),"family_marriage_reply",{"proposal_id":row.proposal_id,"decision":choice.decision},choice.get("available",false),status,host_ref,panel_ref,source)
			if not choice.get("available",false): host._text(body,REASONS.get(choice.get("code",""),"请核对当前本人资格与原会面。"),16)
	for row in view.get("marriages",[]): host._text(body,"第%d半日与%s的婚姻记录%s"%[int(row.formed_tick),row.other.name," · 双方已解除" if row.status=="mutually_dissolved" else " · 原经历保留，远方消息另核"],17)
	if view.get("current_statements",[]).is_empty(): host._text(body,"请先实际交谈；未听到的对方意愿不会从目录提前显示。",17)
	var pager=HBoxContainer.new(); column.add_child(pager)
	var total: int=maxi(int(view.get("statement_count",0)),maxi(int(view.get("proposal_count",0)),int(view.get("bond_count",0))))
	for delta in [-1,1]:
		var target: int=int(view.get("page",0))+delta
		var button: Button=host._button(pager,"上一页" if delta<0 else "下一页","marriage_page","",func():
			if Guard._live(host_ref,panel_ref,source) and on_page.is_valid(): on_page.call(target))
		button.disabled=not source.ok or target<0 or target>4095 or target*16>=total
	return panel

## 新策略明确启用才显示独立页，不改变旧家庭模式。
static func selected(host) -> bool:
	return host._resident_mode and host.get_meta("family_tab","family")=="family_marriage" and host.kernel.has_method("family_marriage_enabled") and host.kernel.family_marriage_enabled()

## 关闭只取消当前待确认操作，不撤回既成关系或另一人的决定。
static func _return(host_ref: WeakRef) -> void:
	var host=host_ref.get_ref()
	if is_instance_valid(host) and host._family_open: host.set_meta("family_tab","family"); host._render()

## 换人物和恢复世界后重置页码，旧来源不能继续点击。
static func render(host,parent: Control) -> void:
	var source: Dictionary=Guard._source(host)
	if host.get_meta("marriage_navigation_source",{})!=source: host.set_meta("marriage_navigation_source",source.duplicate(true)); host.set_meta("marriage_page",0)
	var ref: WeakRef=weakref(host)
	var view: Dictionary=host.kernel.family_marriage_page(int(host.get_meta("marriage_page",0)))
	build(host,parent,view,_return.bind(ref),func(page: int):
		if is_instance_valid(ref.get_ref()) and selected(ref.get_ref()) and not ref.get_ref()._advancing and page>=0 and page<=4095: ref.get_ref().set_meta("marriage_page",page); ref.get_ref()._render())
