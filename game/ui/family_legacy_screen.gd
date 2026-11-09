## 独立条件控制许可由候选本人接受或撤回；死亡后仅用户确认选择，不代理签字或交付遗产。
extends RefCounted
const REASONS={"CollateralControlLivingPartiesRequired":"认可和新许可须双方仍在世。","CollateralControlRecognitionAfterDecision":"须先由双方明确认可旁支关系，再单独办理控制许可。","CollateralControlProviderDeclarationRequired":"请听取对方本人当前的明确声明。","CollateralControlRecognizedPartiesMismatch":"这份认可不是当前两人的原关系。","CollateralLegacyRecognitionBeforeDeathRequired":"旁支认可须在原真实死因发生前完成。","BiologicalControlDirectParentRequired":"须有真实直接亲代来源，收养或同姓不能代替亲生关系。","BiologicalControlBornSourceRequired":"请在已成年子代本人的真实私人会面中办理。","BiologicalControlAdultRequired":"双方须在作出决定时已经成年。","BiologicalLegacyAdultRequired":"接续候选须已经成年。","BiologicalLegacyLivingSuccessorRequired":"候选须仍在世。","BiologicalControlLivingPartiesRequired":"新许可只能在双方仍在世时由本人作出。","BiologicalControlCandidateSayRequired":"请等待候选本人的接受或撤回声明，玩家选项不能代签。","BiologicalControlDecisionBindingMismatch":"当前声明与原许可条款不符，请重新查看本人节点。","DiscipleControlRelationshipRequired":"须先完成双方本人接受的师承。拜师不代替条件许可。","DiscipleControlRelationshipEnded":"原师承已结束，不能新立许可；仍可在本人撤回节点撤回原许可。","DiscipleControlRelationshipAmbiguous":"有多份同技艺关系，来源须明确，不能自动选择最近一份。","DiscipleLegacyRelationshipEnded":"原师承在接续前已结束，不能据此选择。","DiscipleLegacyRelationshipBeforeDeathRequired":"这份师承不是在原真实死因发生前有效建立的。","DiscipleLegacyAdultRequired":"候选须已经成年。","DiscipleLegacyLivingSuccessorRequired":"候选当前须在世。","LegacySuccessorOccupied":"候选正处于战斗、围城或羁押，暂不能选择。","MilitaryOccupationHistoryIncomplete":"候选的旧军事进退来源不完整，暂不能核验接续资格。","LegacyPlayableSourceRequired":"缺少候选原处境来源，暂不能选择。","LegacyConsentAlreadyClosed":"这份许可已经撤回或使用。","LegacyControlLivingPartiesRequired":"只有双方仍在世时才能办理新的条件许可。","FamilyDeathSourceUnavailable":"尚无可核验的生命终点来源。","FamilyConversationPending":"请先结束会面，再回到接续方案确认交接。","FamilyContractPending":"本人仍有未结受托事务，请先处理。","FamilyMilitaryOrOfficePending":"本人仍在实际军旅中，请先恢复可交接处境。","ActorBusy":"本人仍在行动中。","ActorUnavailable":"当前伤病或处境不允许办理。","LegacyPlanAlreadyPending":"已有尚未完成的方案，可先办理或撤回。","LegacyAcceptedAssetsChanged":"原先同意交接的财物已发生变化，请撤回后重新商议。","LegacyRelationshipRequired":"须先建立真实合法的接续关系。","LegacyRequiresAdult":"接续双方须已成年。","InsufficientFunds":"当前可支配私钱不足，不能动用已预留款项。","InsufficientCapacity":"接续人的随身容器无法容纳这批物资。"}

## 当前世界与人物绑定所有回调，冷恢复、切人、关页及换代次后旧按钮失效。
static func _source(host) -> Dictionary:
	var context: Dictionary=host.kernel.command_context()
	var identity: Dictionary=host.kernel.source_identity()
	var exact: Dictionary={}
	for key in ["world_id","execution_epoch","revision","tick"]: exact[key]=identity.get(key)
	return {"ok":context.get("ok",false),"actor_id":context.get("actor_id",""),"kernel_id":host.kernel.get_instance_id(),"revision":context.get("revision",-1),"identity":exact}

## 关闭或正在结算的页面不可使用旧报价；WeakRef不延长已删除UI的生命。
static func _live(host_ref: WeakRef,panel_ref: WeakRef,source: Dictionary) -> bool:
	var host=host_ref.get_ref(); var panel: Control=panel_ref.get_ref()
	return is_instance_valid(host) and is_instance_valid(panel) and panel.is_inside_tree() and panel.is_visible_in_tree() and not panel.is_queued_for_deletion() and not host._advancing and source.get("ok",false) and _source(host)==source

## 激活只改变玩家控制；再次点击前后都重验真实许可和死亡来源。
static func _submit(host_ref: WeakRef,panel_ref: WeakRef,button_ref: WeakRef,status_ref: WeakRef,source: Dictionary,operation: Dictionary) -> void:
	var button: Button=button_ref.get_ref(); var status: Label=status_ref.get_ref()
	if not is_instance_valid(button) or not is_instance_valid(status): return
	if not _live(host_ref,panel_ref,source): button.disabled=true; status.text="当前人物或方案已经变化，请重新打开。"; return
	var host=host_ref.get_ref(); var panel: Control=panel_ref.get_ref()
	if button.disabled or panel.get_meta("legacy_pending",false): return
	var command: String=operation.command; var params: Dictionary=operation.params.duplicate(true)
	var quote: Dictionary=host.kernel.preview(command,params)
	if not quote.get("ok",false) or quote.get("revision",-1)!=source.revision:
		button.disabled=true; status.text=REASONS.get(quote.get("code",""),"当前方案不能办理，请重新查看条件。"); return
	if command=="family_legacy_activate" and not button.get_meta("confirmed",false):
		button.set_meta("confirmed",true); button.text="确认选择此人，遗产仍待结"; status.text="再次确认只改变你所操控的人物。财物、公职和死亡消息知情均不因此转移。"; return
	if not _live(host_ref,panel_ref,source): return
	panel.set_meta("legacy_pending",true); button.disabled=true
	host._perform(command,params)

## 本人卡片显示接续者自己的处境，成长、关系与职责不会由前代抄入。
static func _person(host,parent: Control,person: Dictionary) -> void:
	var age: int=int(person.get("age_years",-1))
	host._text(parent,"%s · %s"%[person.get("name","人物"),"年龄不详" if age<0 else "%d岁"%age],22)

## 每个动作独立引用按钮，但同页只允许一项命令进入待处理状态。
static func _button(host,parent: Control,operation: Dictionary,source: Dictionary,host_ref: WeakRef,panel_ref: WeakRef,status: Label) -> void:
	var labels: Dictionary={"family_collateral_recognize":"本人确认双方的旁支认可","family_collateral_accept":"记录旁支本人另立控制许可","family_collateral_revoke":"记录旁支本人撤回许可","family_biological_accept":"记录子代本人的条件许可","family_biological_revoke":"记录子代本人撤回许可","family_legacy_accept":"记录本人的条件许可","family_legacy_activate":"选择继续此人的人生","family_legacy_revoke":"记录本人撤回许可"}
	var binding: Dictionary={"button":null}; var status_ref: WeakRef=weakref(status)
	var button: Button=host._button(parent,labels.get(operation.command,"办理"),operation.command,"",func(): _submit(host_ref,panel_ref,binding.button,status_ref,source,operation))
	binding.button=weakref(button); button.name="LegacyAction"; button.disabled=not source.ok or not operation.get("available",false)
	if not operation.get("available",false): host._text(parent,REASONS.get(operation.get("code",""),"当前条件不能办理这一步。"),16)

## 有限页展示当前明确邀约及本人计划；历史不提供重复执行按钮。
static func build(host,parent: Control,view: Dictionary,on_close: Callable,on_page: Callable,terminal: bool=false) -> Control:
	var panel=PanelContainer.new(); panel.name="FamilyLegacyScreen"; parent.add_child(panel)
	panel.anchor_left=.04; panel.anchor_right=.96; panel.anchor_top=.12; panel.anchor_bottom=.96
	var column=VBoxContainer.new(); column.add_theme_constant_override("separation",12); panel.add_child(column)
	var source: Dictionary=_source(host)
	if view.get("actor_id")!=source.actor_id or view.get("source_identity")!=source.identity: source.ok=false
	var host_ref: WeakRef=weakref(host); var panel_ref: WeakRef=weakref(panel)
	var header=HBoxContainer.new(); column.add_child(header); host._text(header,"旁支认可与接续许可" if view.get("relation_basis","")=="collateral" else ("亲生子代接续许可" if view.get("relation_basis","")=="biological" else "人生接续许可"),25).size_flags_horizontal=Control.SIZE_EXPAND_FILL
	host._button(header,"返回标题" if terminal else "返回家事","succession_close","",func():
		var current: Control=panel_ref.get_ref()
		if is_instance_valid(current) and current.is_inside_tree() and current.is_visible_in_tree() and not current.is_queued_for_deletion() and on_close.is_valid(): current.hide(); on_close.call()).name="LegacyClose"
	var choices: Array=[["original","原收养与弟子许可"]]
	if host.kernel.has_method("family_biological_legacy_enabled") and host.kernel.family_biological_legacy_enabled(): choices.append(["biological","亲生子代许可"])
	if host.kernel.has_method("family_collateral_legacy_enabled") and host.kernel.family_collateral_legacy_enabled(): choices.append(["collateral","本人旁支认可与许可"])
	if choices.size()>1:
		var tabs=HBoxContainer.new(); column.add_child(tabs)
		for option in choices:
			var selected_kind: String=option[0]
			var tab: Button=host._button(tabs,option[1],"legacy_source_tab","",func():
				if _live(host_ref,panel_ref,source):
					var current=host_ref.get_ref(); current.set_meta("legacy_source_tab",selected_kind); current.set_meta("legacy_page",0); current._render())
			tab.disabled=String(view.get("relation_basis","original"))==selected_kind or (selected_kind=="original" and view.get("relation_basis","")=="")
	var status: Label=host._text(column,"候选本人可以在生前接受或撤回条件许可。前代死亡后，须再次选择才继续其原有人生；遗产另行办理。",17); status.name="LegacyStatus"
	var scroll=ScrollContainer.new(); scroll.size_flags_vertical=Control.SIZE_EXPAND_FILL; scroll.horizontal_scroll_mode=ScrollContainer.SCROLL_MODE_DISABLED; column.add_child(scroll)
	var body=VBoxContainer.new(); body.size_flags_horizontal=Control.SIZE_EXPAND_FILL; body.add_theme_constant_override("separation",12); scroll.add_child(body)
	for request in view.get("requests",[]):
		_person(host,body,request.get("successor",request.get("candidate",{})))
		if request.get("relation_basis","")=="disciple": host._text(body,"弟子关系只作候选依据；以下许可仍须本人独立接受。",16)
		_button(host,body,request.action,source,host_ref,panel_ref,status)
	for relation in view.get("relations",[]):
		body.add_child(HSeparator.new()); _person(host,body,relation.relative)
		host._text(body,"双方已明确认可旁支关系；本人控制许可仍须另行作出。",17)
		for operation in relation.get("actions",[]): _button(host,body,operation,source,host_ref,panel_ref,status)
	for plan in view.get("consents",[]):
		body.add_child(HSeparator.new()); _person(host,body,plan.successor)
		if plan.get("relation_basis","")=="disciple": host._text(body,"原弟子师承 · 不改血缘、技能、财物或公职",16)
		host._text(body,"条件许可 · %s"%{"accepted":"本人已接受，尚未启用","revoked":"本人已撤回","activated":"已经选择此人的人生"}.get(plan.status,plan.status),18)
		for operation in plan.get("actions",[]): _button(host,body,operation,source,host_ref,panel_ref,status)
	if view.get("requests",[]).is_empty() and view.get("consents",[]).is_empty() and view.get("relations",[]).is_empty(): host._text(body,"当前没有符合来源的条件许可。没有本人同意时，死亡终点仍可保存；合法亲属或师承关系本身不代替本人意愿。",18)
	if terminal:
		host._button(column,"保存此刻的世界","save","family_legacy",func():
			if _live(host_ref,panel_ref,source): host_ref.get_ref()._save())
	var pages=HBoxContainer.new(); column.add_child(pages); var page: int=int(view.get("page",0)); var total: int=maxi(int(view.get("relation_count",0)),maxi(int(view.get("request_count",0)),int(view.get("consent_count",0))))
	for delta in [-1,1]:
		var next_page: int=page+delta
		var button: Button=host._button(pages,"上一页" if delta<0 else "下一页","legacy_page","",func():
			if next_page>=0 and next_page*16<total and _live(host_ref,panel_ref,source) and on_page.is_valid(): on_page.call(next_page))
		button.disabled=not source.ok or next_page<0 or next_page*16>=total
	return panel

## 仅已注册的原生情景显示此页，旧家庭profile不凭空启用新命令。
static func selected(host) -> bool:
	return host._resident_mode and host.get_meta("family_tab","family")=="family_legacy" and host.kernel.has_method("family_legacy_enabled") and host.kernel.family_legacy_enabled()

## 返回只改本页导航，不取消已接受方案或提交接续。
static func _return(host_ref: WeakRef) -> void:
	var host=host_ref.get_ref()
	if not is_instance_valid(host) or not host._family_open: return
	host.set_meta("family_tab","family"); host._render()

## 翻页重取有限当前来源，旧页面回调不能继续操作。
static func _page(host_ref: WeakRef,value: int) -> void:
	var host=host_ref.get_ref()
	if not is_instance_valid(host) or not host._resident_mode or not host.kernel.family_legacy_enabled() or host._advancing or value<0 or value>4095: return
	host.set_meta("legacy_page",value); host._render()

## 只有显式亲生版本开放分栏；原收养和弟子许可仍可独立查看。
static func page(host) -> Dictionary:
	var collateral: bool=host.kernel.has_method("family_collateral_legacy_enabled") and host.kernel.family_collateral_legacy_enabled()
	var selected_kind: String=String(host.get_meta("legacy_source_tab","collateral" if collateral else "biological"))
	if collateral and selected_kind=="collateral": return host.kernel.family_collateral_page(int(host.get_meta("legacy_page",0)))
	var biological: bool=host.kernel.has_method("family_biological_legacy_enabled") and host.kernel.family_biological_legacy_enabled() and selected_kind=="biological"
	return host.kernel.family_biological_page(int(host.get_meta("legacy_page",0))) if biological else host.kernel.family_legacy_page(int(host.get_meta("legacy_page",0)))

## 数据只来自正式有限查询，UI不请求完整世界状态。
static func render(host,parent: Control,terminal: bool=false) -> void:
	var host_ref: WeakRef=weakref(host)
	var close: Callable=host._title_prompt if terminal else _return.bind(host_ref)
	build(host,parent,page(host),close,func(value: int): _page(host_ref,value),terminal)
