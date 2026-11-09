## 生育意愿、有限资助与实际照护分开展示；界面只提交原生认可的本人命令。
extends RefCounted
const Guard=preload("res://ui/family_legacy_screen.gd")
const REASONS={"BirthLivingDecisionMakerRequired":"本人当前不能办理新的决定。","BirthPersonalDecisionRequired":"这一步必须由原当事人本人决定。","BirthProposalAlreadyPending":"已经听取这份提议，请在原记录答复。","BirthProposalAlreadyAnswered":"这份提议已有决定。","BirthArrangementAlreadyActive":"双方仍有未结的生育或有限照护安排。","BirthCareReleaseSourceRequired":"已有儿童时不能用撤回计划免除原照护责任。","BirthParticipantsNotAtCarePlace":"须在原照护地点具备真实条件后才能继续。","ChildCareArrangementUnavailable":"本次安排尚不能开始照护，或已有照护进行中。","ChildCareNotAtSamePlace":"照护者和儿童须实际同处原地点。","ChildCareOriginalCaregiverRequired":"只有原本人承诺的照护者可以办理。","BirthCareSupplyInsufficient":"本人可用私粮不足，已预留、公产和代管物品不能挪用。","BirthArrangementSourceNotCurrent":"请回到双方明确讨论生育和照护的原会面节点。"}

## 两次确认绑定完整世界读戳；换人、读档或旧异步结果不会继承之前的按钮授权。
static func _submit(host_ref: WeakRef,panel_ref: WeakRef,button_ref: WeakRef,status_ref: WeakRef,source: Dictionary,kind: String,params: Dictionary) -> void:
	var button: Button=button_ref.get_ref(); var status: Label=status_ref.get_ref()
	if not is_instance_valid(button) or not is_instance_valid(status): return
	if not Guard._live(host_ref,panel_ref,source): button.disabled=true; status.text="世界或人物已改变，请重新打开生育照护。"; return
	var host=host_ref.get_ref(); var panel: Control=panel_ref.get_ref()
	if button.disabled or panel.get_meta("birth_pending",false): return
	var quote: Dictionary=host.kernel.preview(kind,params)
	if quote.get("code")=="NativeAsyncRequired": status.text="正在核对双方原意愿与本人责任，请稍候。"; return
	if not quote.get("ok",false) or quote.get("revision",-1)!=source.revision: status.text=REASONS.get(quote.get("code",""),"当前原意愿、本人处境或照护资源尚未满足。请核对来源。"); return
	var exact={"kind":kind,"params":params.duplicate(true)}
	if button.get_meta("confirmed",{})!=exact:
		button.set_meta("confirmed",exact); button.text="再次确认"
		if kind=="family_birth_receive": status.text="再次确认记录亲自听到的生育与有限照护提议；尚未接受，也不生成儿童。"
		elif kind=="family_birth_reply" and params.get("decision")=="accepted": status.text="再次确认本人愿意接受，预约约定的私人粮食。出生仍须到期并满足原条件，每次照护还要实际完成。"
		elif kind=="family_child_care": status.text="再次确认亲自投入这段照护时间；完成后才消耗约定的一期原口粮。"
		elif kind=="family_child_care_cancel": status.text="再次确认停止本次照护，不记已喂养；原有限照护责任和剩余私粮预约仍在。"
		elif kind=="family_birth_withdraw": status.text="再次确认撤回尚未出生的安排，释放原剩余预约；不能用此删除已出生人物。"
		else: status.text="再次确认本人对原提议的决定。"
		return
	if not Guard._live(host_ref,panel_ref,source): return
	panel.set_meta("birth_pending",true); button.disabled=true; host._perform(kind,params)

## 参数仅来自有限本人页的原ID，不能把出生/禀赋/钱粮字段写回世界。
static func _operation(host,parent: Control,label: String,kind: String,params: Dictionary,available: bool,status: Label,host_ref: WeakRef,panel_ref: WeakRef,source: Dictionary) -> void:
	var binding={"button":null}
	var button: Button=host._button(parent,label,kind,"",func(): _submit(host_ref,panel_ref,binding.button,weakref(status),source,kind,params))
	button.name="BirthCareAction"; binding.button=weakref(button); button.disabled=not source.ok or not available

## 只显示有限原条款，不把预付粮食或全局死亡状态改写成已照护、已知消息。
static func _terms(host,parent: Control,value: Dictionary) -> void:
	host._text(parent,"原约定：到期需%d半日；每期照护%d半日，消耗%d份私粮，共预付%d期。"%[int(value.get("birth_duration_ticks",0)),int(value.get("care_period_ticks",0)),int(value.get("food_per_period",0)),int(value.get("prepaid_care_periods",0))],17)

## 有限本人视图。取消只停止本次Action，关闭页面不撤销已生效的双方决定。
static func build(host,parent: Control,view: Dictionary,on_close: Callable,on_page: Callable) -> Control:
	var panel=PanelContainer.new(); panel.name="FamilyBirthScreen"; parent.add_child(panel)
	panel.anchor_left=.04; panel.anchor_right=.96; panel.anchor_top=.12; panel.anchor_bottom=.96
	var column=VBoxContainer.new(); panel.add_child(column); column.add_theme_constant_override("separation",10)
	var source: Dictionary=Guard._source(host)
	if view.get("actor_id")!=source.actor_id or view.get("source_identity")!=source.identity: source.ok=false
	var host_ref: WeakRef=weakref(host); var panel_ref: WeakRef=weakref(panel)
	var title=HBoxContainer.new(); column.add_child(title); host._text(title,"本人生育与有限照护",25).size_flags_horizontal=Control.SIZE_EXPAND_FILL
	host._button(title,"返回家事","birth_close","",func():
		var current: Control=panel_ref.get_ref()
		if is_instance_valid(current) and current.is_inside_tree() and current.is_visible_in_tree() and on_close.is_valid(): current.hide(); on_close.call())
	var status: Label=host._text(column,view.get("explanation","双方须有独立真实意愿。"),17)
	var scroll=ScrollContainer.new(); scroll.horizontal_scroll_mode=ScrollContainer.SCROLL_MODE_DISABLED; scroll.size_flags_vertical=Control.SIZE_EXPAND_FILL; column.add_child(scroll)
	var body=VBoxContainer.new(); body.size_flags_horizontal=Control.SIZE_EXPAND_FILL; scroll.add_child(body)
	for row in view.get("current_statements",[]):
		host._text(body,"%s亲口提出的安排"%row.other.name,20); _terms(host,body,row.terms)
		_operation(host,body,"听取原提议","family_birth_receive",{"arrangement_id":row.arrangement_id},row.option.get("available",false),status,host_ref,panel_ref,source)
	for row in view.get("proposals",[]):
		body.add_child(HSeparator.new()); host._text(body,"与%s的提议 · 第%d半日"%[row.other.name,int(row.received_tick)],20); _terms(host,body,row.terms)
		if not row.get("response","").is_empty(): host._text(body,{"accepted":"本人已接受原条款","refused":"本人已拒绝","withdrawn":"原提议已撤回"}.get(row.response,"原决定已记录"),17)
		for choice in row.get("options",[]):
			_operation(host,body,{"accepted":"本人愿意接受","refused":"本人拒绝","withdrawn":"撤回本人提议"}.get(choice.decision,"本人答复"),"family_birth_reply",{"proposal_id":row.proposal_id,"decision":choice.decision},choice.get("available",false),status,host_ref,panel_ref,source)
			if not choice.get("available",false): host._text(body,REASONS.get(choice.get("code",""),"当前资格或原资源尚未满足。"),16)
	for row in view.get("arrangements",[]):
		body.add_child(HSeparator.new()); host._text(body,{"finite_arrangement_closed":"原有限安排已终结","child_born":"本人亲历的出生","awaiting_original_conditions":"等待到期及原照护条件"}.get(row.status,"原安排"),20)
		if row.has("child"): host._text(body,"%s · 第%d半日出生"%[row.child.name,int(row.birth_tick)],18)
		if row.has("personally_completed_periods"): host._text(body,"本人实际完成%d期照护；这不是一生照护已完成的承诺。"%int(row.personally_completed_periods),17)
		for choice in row.get("options",[]):
			var params={"accepted_id":row.accepted_id}
			if choice.kind=="family_child_care_cancel": params={"action_id":choice.action_id}
			_operation(host,body,{"family_birth_withdraw":"撤回未出生安排","family_child_care":"亲自照护一期","family_child_care_cancel":"停止本次照护"}.get(choice.kind,"办理"),choice.kind,params,choice.get("available",false),status,host_ref,panel_ref,source)
			if not choice.get("available",false): host._text(body,REASONS.get(choice.get("code",""),"当前处境尚不能办理。"),16)
	for row in view.get("care",[]): host._text(body,"第%d期 · %s · %s"%[int(row.period_index),row.child.name,row.explanation],17)
	if view.get("current_statements",[]).is_empty(): host._text(body,"请先真实交谈。未听到的生育意愿不会从目录提前公开。",17)
	var pager=HBoxContainer.new(); column.add_child(pager)
	var total: int=maxi(maxi(int(view.get("statement_count",0)),int(view.get("proposal_count",0))),maxi(int(view.get("arrangement_count",0)),int(view.get("care_count",0))))
	for delta in [-1,1]:
		var target: int=int(view.get("page",0))+delta
		var button: Button=host._button(pager,"上一页" if delta<0 else "下一页","birth_page","",func():
			if Guard._live(host_ref,panel_ref,source) and on_page.is_valid(): on_page.call(target))
		button.disabled=not source.ok or target<0 or target>4095 or target*16>=total
	return panel

## 仅显式启用新生育模式才打开此页，旧家庭模式保持原入口。
static func selected(host) -> bool:
	return host._resident_mode and host.get_meta("family_tab","family")=="family_birth" and host.kernel.has_method("family_birth_enabled") and host.kernel.family_birth_enabled()

## 离开清除待确认界面，不写任何业务状态。
static func _return(host_ref: WeakRef) -> void:
	var host=host_ref.get_ref()
	if is_instance_valid(host) and host._family_open: host.set_meta("family_tab","family"); host._render()

## 每次新世界身份重新取有限页，不能跨读档复用旧的孩子或Action编号。
static func render(host,parent: Control) -> void:
	var source: Dictionary=Guard._source(host)
	if host.get_meta("birth_navigation_source",{})!=source: host.set_meta("birth_navigation_source",source.duplicate(true)); host.set_meta("birth_page",0)
	var ref: WeakRef=weakref(host)
	var view: Dictionary=host.kernel.family_birth_page(int(host.get_meta("birth_page",0)))
	build(host,parent,view,_return.bind(ref),func(page: int):
		if is_instance_valid(ref.get_ref()) and selected(ref.get_ref()) and not ref.get_ref()._advancing and page>=0 and page<=4095: ref.get_ref().set_meta("birth_page",page); ref.get_ref()._render())
