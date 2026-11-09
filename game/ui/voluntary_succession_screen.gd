## 自愿接续分为本人接受、退出会面后的交接确认；界面不改人物、财产或公职。
extends RefCounted
const REASONS={"FamilyConversationPending":"请先结束会面，再回到接续方案确认交接。","FamilyContractPending":"本人仍有未结受托事务，请先处理。","FamilyMilitaryOrOfficePending":"本人仍在实际军旅中，请先恢复可交接处境。","ActorBusy":"本人仍在行动中。","ActorUnavailable":"当前伤病或处境不允许办理。","SuccessionPlanAlreadyPending":"已有尚未完成的方案，可先办理或撤回。","SuccessionAcceptedAssetsChanged":"原先同意交接的财物已发生变化，请撤回后重新商议。","SuccessionRelationshipRequired":"须先建立真实合法的接续关系。","SuccessionRequiresAdult":"接续双方须已成年。","InsufficientFunds":"当前可支配私钱不足，不能动用已预留款项。","InsufficientCapacity":"接续人的随身容器无法容纳这批物资。"}

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

## 换控制人物须在显示原方案后再次确认；每次点击仍重验原正式报价。
static func _submit(host_ref: WeakRef,panel_ref: WeakRef,button_ref: WeakRef,status_ref: WeakRef,source: Dictionary,operation: Dictionary) -> void:
	var button: Button=button_ref.get_ref(); var status: Label=status_ref.get_ref()
	if not is_instance_valid(button) or not is_instance_valid(status): return
	if not _live(host_ref,panel_ref,source): button.disabled=true; status.text="当前人物或方案已经变化，请重新打开。"; return
	var host=host_ref.get_ref(); var panel: Control=panel_ref.get_ref()
	if button.disabled or panel.get_meta("succession_pending",false): return
	var command: String=operation.command; var params: Dictionary=operation.params.duplicate(true)
	var quote: Dictionary=host.kernel.preview(command,params)
	if not quote.get("ok",false) or quote.get("revision",-1)!=source.revision:
		button.disabled=true; status.text=REASONS.get(quote.get("code",""),"当前方案不能办理，请重新查看条件。"); return
	if command=="family_continue_as" and not button.get_meta("confirmed",false):
		button.set_meta("confirmed",true); button.text="确认交接，继续此人的人生"; status.text="再次确认后，将交付方案中列出的私钱物资，并继续操控接续人。前代仍留在世界。"; return
	if not _live(host_ref,panel_ref,source): return
	panel.set_meta("succession_pending",true); button.disabled=true
	host._perform(command,params)

## 财物只显示原方案列项，公产、产业产权与未选私产不混作自动承接。
static func _assets(host,parent: Control,personal: Dictionary) -> void:
	host._text(parent,"私钱 · %d钱"%int(personal.get("money",0)),18)
	var amounts: Dictionary={}
	for lot in personal.get("lots",personal.get("goods",[])): amounts[lot.item_id]=int(amounts.get(lot.item_id,0))+int(lot.quantity)
	for item_id in amounts:
		var label: String=host._name(host._data.items[item_id]) if host._data.items.has(item_id) else item_id
		host._text(parent,"%s × %d"%[label,amounts[item_id]],17)
	if amounts.is_empty(): host._text(parent,"未选择随身物资。",17)

## 本人卡片显示接续者自己的处境，成长、关系与职责不会由前代抄入。
static func _person(host,parent: Control,person: Dictionary) -> void:
	var age: int=int(person.get("age_years",-1))
	host._text(parent,"%s · %s"%[person.get("name","人物"),"年龄不详" if age<0 else "%d岁"%age],22)
	host._text(parent,"体力 %d · 疲劳 %d · 伤势 %d"%[person.get("hp",0),person.get("fatigue",0),person.get("injury",0)],17)
	if not person.get("action_kind","").is_empty(): host._text(parent,"正在办理："+person.action_kind,17)

## 每个动作独立引用按钮，但同页只允许一项命令进入待处理状态。
static func _button(host,parent: Control,operation: Dictionary,source: Dictionary,host_ref: WeakRef,panel_ref: WeakRef,status: Label) -> void:
	var labels: Dictionary={"family_agree_succession":"同意这份接续方案","family_continue_as":"查看并确认交接","family_cancel_succession":"撤回这份方案"}
	var binding: Dictionary={"button":null}; var status_ref: WeakRef=weakref(status)
	var button: Button=host._button(parent,labels.get(operation.command,"办理"),operation.command,"",func(): _submit(host_ref,panel_ref,binding.button,status_ref,source,operation))
	binding.button=weakref(button); button.name="SuccessionAction"; button.disabled=not source.ok or not operation.get("available",false)
	if not operation.get("available",false): host._text(parent,REASONS.get(operation.get("code",""),"当前条件不能办理这一步。"),16)

## 有限页展示当前明确邀约及本人计划；历史不提供重复执行按钮。
static func build(host,parent: Control,view: Dictionary,on_close: Callable,on_page: Callable) -> Control:
	var panel=PanelContainer.new(); panel.name="VoluntarySuccessionScreen"; parent.add_child(panel)
	panel.anchor_left=.04; panel.anchor_right=.96; panel.anchor_top=.12; panel.anchor_bottom=.96
	var column=VBoxContainer.new(); column.add_theme_constant_override("separation",12); panel.add_child(column)
	var source: Dictionary=_source(host)
	if view.get("actor_id")!=source.actor_id or view.get("source_identity")!=source.identity: source.ok=false
	var host_ref: WeakRef=weakref(host); var panel_ref: WeakRef=weakref(panel)
	var header=HBoxContainer.new(); column.add_child(header); host._text(header,"接续方案",25).size_flags_horizontal=Control.SIZE_EXPAND_FILL
	host._button(header,"返回家事","succession_close","",func():
		var current: Control=panel_ref.get_ref()
		if is_instance_valid(current) and current.is_inside_tree() and current.is_visible_in_tree() and not current.is_queued_for_deletion() and on_close.is_valid(): current.hide(); on_close.call()).name="SuccessionClose"
	var status: Label=host._text(column,"先由双方同意具体方案，再结束会面办理交接。所学、关系、公职和未选财物保留原来归属。",17); status.name="SuccessionStatus"
	var scroll=ScrollContainer.new(); scroll.size_flags_vertical=Control.SIZE_EXPAND_FILL; scroll.horizontal_scroll_mode=ScrollContainer.SCROLL_MODE_DISABLED; column.add_child(scroll)
	var body=VBoxContainer.new(); body.size_flags_horizontal=Control.SIZE_EXPAND_FILL; body.add_theme_constant_override("separation",12); scroll.add_child(body)
	for request in view.get("requests",[]):
		_person(host,body,request.successor); _assets(host,body,request.personal); _button(host,body,request.accept_action,source,host_ref,panel_ref,status)
	for plan in view.get("plans",[]):
		body.add_child(HSeparator.new()); _person(host,body,plan.successor)
		host._text(body,"%s的方案 · %s"%[plan.predecessor.name,{"accepted":"双方已同意，尚未交接","cancelled":"已撤回","completed":"已完成交接"}.get(plan.state,plan.state)],18)
		_assets(host,body,plan.personal)
		for operation in plan.get("actions",[]): _button(host,body,operation,source,host_ref,panel_ref,status)
	if view.get("requests",[]).is_empty() and view.get("plans",[]).is_empty(): host._text(body,"当前没有已当面接受的接续方案。收养、拜师或同族本身不表示愿意接续。",18)
	var pages=HBoxContainer.new(); column.add_child(pages); var page: int=int(view.get("page",0)); var total: int=maxi(int(view.get("request_count",0)),int(view.get("plan_count",0)))
	for delta in [-1,1]:
		var next_page: int=page+delta
		var button: Button=host._button(pages,"上一页" if delta<0 else "下一页","succession_page","",func():
			if next_page>=0 and next_page*16<total and _live(host_ref,panel_ref,source) and on_page.is_valid(): on_page.call(next_page))
		button.disabled=not source.ok or next_page<0 or next_page*16>=total
	return panel

## 仅已注册的原生情景显示此页，旧家庭profile不凭空启用新命令。
static func selected(host) -> bool:
	return host._resident_mode and host.get_meta("family_tab","family")=="voluntary_succession" and host.kernel.has_method("voluntary_succession_enabled") and host.kernel.voluntary_succession_enabled()

## 返回只改本页导航，不取消已接受方案或提交接续。
static func _return(host_ref: WeakRef) -> void:
	var host=host_ref.get_ref()
	if not is_instance_valid(host) or not host._family_open: return
	host.set_meta("family_tab","family"); host._render()

## 翻页重取有限当前来源，旧页面回调不能继续操作。
static func _page(host_ref: WeakRef,value: int) -> void:
	var host=host_ref.get_ref()
	if not is_instance_valid(host) or not selected(host) or host._advancing or value<0 or value>255: return
	host.set_meta("succession_page",value); host._render()

## 数据只来自正式有限查询，UI不请求完整世界状态。
static func render(host,parent: Control) -> void:
	var host_ref: WeakRef=weakref(host)
	build(host,parent,host.kernel.voluntary_succession_page(int(host.get_meta("succession_page",0))),_return.bind(host_ref),func(value: int): _page(host_ref,value))
