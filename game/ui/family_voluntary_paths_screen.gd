## 生前本人接受与最终换控分开；此页只消费原生有限来源与正式命令。
extends RefCounted
const REASONS={"VoluntaryAcceptedPlanRequired":"须先取得接续人真实接受，再从原方案办理。","VoluntaryPathOriginalStableReadRequired":"尚未取得原准备与责任来源。","FamilyConversationPending":"请先结束本人会面，再返回这份方案。","FamilyContractPending":"本人仍有未结受托事务。","FamilyPartyPending":"请先处理同行队伍。","FamilyMilitaryOrOfficePending":"当前军旅处境不能办理接续。","FamilyAlreadyRetired":"这位人物已经退隐，不能再次选为接续人。","ActorBusy":"本人仍在行动中。","ActorUnavailable":"当前伤病或处境不允许办理。","VoluntaryPathLivingActorRequired":"生前接续要求双方仍在世。","SuccessionAcceptedAssetsChanged":"原先同意的私钱或原批货物已变化，请重新商议。","SuccessionPlanAlreadyPending":"已有待办方案，可先完成或取消。","VoluntaryPathPendingPlanRequired":"这份方案已不再待办。","VoluntaryPathCurrentPlayerRequired":"只由当前本人办理。","VoluntaryDiscipleRelationshipEnded":"原师承已经结束，不能据此换控。","VoluntaryRelationDeathReadRequired":"缺少可核验的生命来源。"}
const LABELS={"family_voluntary_path_accept":"记录本人接受生前接续","family_voluntary_path_continue":"按原接受方案接续人生","family_voluntary_path_cancel":"取消我的接续方案","family_voluntary_path_withdraw":"记录对方本人撤回","family_agree_succession":"记录本人接受生前接续","family_continue_as":"按原接受方案接续人生","family_cancel_succession":"取消我的接续方案"}

static func _source(host) -> Dictionary:
	var context: Dictionary=host.kernel.command_context()
	var identity: Dictionary=host.kernel.source_identity()
	var exact: Dictionary={}
	for key in ["world_id","execution_epoch","revision","tick"]: exact[key]=identity.get(key)
	return {"ok":context.get("ok",false),"actor_id":context.get("actor_id",""),"kernel_id":host.kernel.get_instance_id(),"revision":context.get("revision",-1),"identity":exact}

static func _live(host_ref: WeakRef,panel_ref: WeakRef,source: Dictionary) -> bool:
	var host=host_ref.get_ref(); var panel: Control=panel_ref.get_ref()
	return is_instance_valid(host) and is_instance_valid(panel) and panel.is_inside_tree() and panel.is_visible_in_tree() and not panel.is_queued_for_deletion() and not host._advancing and source.get("ok",false) and _source(host)==source

static func _submit(host_ref: WeakRef,panel_ref: WeakRef,button_ref: WeakRef,status_ref: WeakRef,source: Dictionary,operation: Dictionary) -> void:
	var button: Button=button_ref.get_ref(); var status: Label=status_ref.get_ref()
	if not is_instance_valid(button) or not is_instance_valid(status): return
	if not _live(host_ref,panel_ref,source): button.disabled=true; status.text="当前人物或方案已经变化，请重新打开。"; return
	var host=host_ref.get_ref(); var panel: Control=panel_ref.get_ref()
	if button.disabled or panel.get_meta("voluntary_pending",false): return
	var command: String=operation.command; var params: Dictionary=operation.params.duplicate(true)
	if not LABELS.has(command): button.disabled=true; return
	var quote: Dictionary=host.kernel.preview(command,params)
	if not quote.get("ok",false) or quote.get("revision",-1)!=source.revision:
		button.disabled=true; status.text=REASONS.get(quote.get("code",""),"当前来源或准备条件不满足，请重新查看。"); return
	if command in ["family_voluntary_path_continue","family_continue_as"] and not button.get_meta("confirmed",false):
		button.set_meta("confirmed",true); button.text="确认接续，并仅交付原方案中的私产"; status.text="再次确认后，控制转到接续人自己的生活。只交付本方案列出的私钱与货物，原公职与其他责任保留。"; return
	if not _live(host_ref,panel_ref,source): return
	panel.set_meta("voluntary_pending",true); button.disabled=true; host._perform(command,params)

static func build(host,parent: Control,view: Dictionary,on_close: Callable,on_page: Callable) -> Control:
	var panel=PanelContainer.new(); panel.name="FamilyVoluntaryPathsScreen"; parent.add_child(panel)
	panel.anchor_left=.04; panel.anchor_right=.96; panel.anchor_top=.12; panel.anchor_bottom=.96
	var column=VBoxContainer.new(); column.add_theme_constant_override("separation",12); panel.add_child(column)
	var source: Dictionary=_source(host)
	if view.get("actor_id")!=source.actor_id or view.get("source_identity")!=source.identity: source.ok=false
	var host_ref: WeakRef=weakref(host); var panel_ref: WeakRef=weakref(panel)
	var header=HBoxContainer.new(); column.add_child(header); host._text(header,"生前自愿接续",25).size_flags_horizontal=Control.SIZE_EXPAND_FILL
	host._button(header,"返回家事","voluntary_close","",func():
		var current: Control=panel_ref.get_ref()
		if is_instance_valid(current) and current.is_inside_tree() and current.is_visible_in_tree() and not current.is_queued_for_deletion() and on_close.is_valid(): current.hide(); on_close.call()).name="VoluntaryClose"
	var status: Label=host._text(column,"亲属、弟子或旁支关系只是依据。须在对方本人的明确声明中记录接受，结束会面后再确认接续；本人也可撤回。",17); status.name="VoluntaryStatus"
	var scroll=ScrollContainer.new(); scroll.size_flags_vertical=Control.SIZE_EXPAND_FILL; scroll.horizontal_scroll_mode=ScrollContainer.SCROLL_MODE_DISABLED; column.add_child(scroll)
	var body=VBoxContainer.new(); body.size_flags_horizontal=Control.SIZE_EXPAND_FILL; body.add_theme_constant_override("separation",12); scroll.add_child(body)
	for row in view.get("rows",[]):
		body.add_child(HSeparator.new()); host._text(body,row.get("successor",{}).get("name","接续人"),22)
		if row.get("kind")=="plan": host._text(body,{"accepted":"本人已接受，尚未交接","completed":"已按原方案接续","cancelled":"原方案已取消"}.get(row.get("status",""),"原方案"),18)
		var personal: Dictionary=row.get("personal",{}); host._text(body,"原方案私钱：%d"%int(personal.get("money",0)),17)
		for lot in personal.get("goods",[]): host._text(body,"%s × %d"%[lot.get("item_id","物品"),int(lot.get("quantity",0))],16)
		for operation in row.get("actions",[]):
			if not LABELS.has(operation.get("command","")): continue
			var binding: Dictionary={"button":null}; var status_ref: WeakRef=weakref(status)
			var button: Button=host._button(body,LABELS[operation.command],operation.command,"",func(): _submit(host_ref,panel_ref,binding.button,status_ref,source,operation))
			binding.button=weakref(button); button.name="VoluntaryAction"; button.disabled=not source.ok or not operation.get("available",false)
			if not operation.get("available",false): host._text(body,REASONS.get(operation.get("code",""),"当前条件不能办理这一步。"),16)
	if view.get("rows",[]).is_empty(): host._text(body,"当前没有本人正在明确接受的条款或本人参与的方案。请通过真实私人会面商议。",18)
	var pages=HBoxContainer.new(); column.add_child(pages); var page: int=int(view.get("page",0)); var total: int=int(view.get("total",0))
	for delta in [-1,1]:
		var next_page: int=page+delta
		var button: Button=host._button(pages,"上一页" if delta<0 else "下一页","voluntary_page","",func():
			if next_page>=0 and next_page*16<total and _live(host_ref,panel_ref,source) and on_page.is_valid(): on_page.call(next_page))
		button.disabled=not source.ok or next_page<0 or next_page*16>=total
	return panel

static func selected(host) -> bool:
	return host._resident_mode and host.get_meta("family_tab","family")=="family_voluntary_paths" and host.kernel.has_method("family_voluntary_paths_enabled") and host.kernel.family_voluntary_paths_enabled()

static func page(host) -> Dictionary:
	return host.kernel.family_voluntary_paths_view(int(host.get_meta("voluntary_paths_page",0)))

## 离页只关闭显示，不取消当事人的原接受方案。
static func _return(host_ref: WeakRef) -> void:
	var host=host_ref.get_ref()
	if is_instance_valid(host) and host._family_open: host.set_meta("family_tab","family"); host._render()

## 每次人物/世界变化重新取本人有限页；旧页回调不授任何跨人控制。
static func render(host,parent: Control) -> void:
	var source: Dictionary=_source(host)
	if host.get_meta("voluntary_paths_navigation_source",{})!=source: host.set_meta("voluntary_paths_navigation_source",source.duplicate(true)); host.set_meta("voluntary_paths_page",0)
	var ref: WeakRef=weakref(host)
	build(host,parent,page(host),_return.bind(ref),func(number: int):
		if is_instance_valid(ref.get_ref()) and selected(ref.get_ref()) and not ref.get_ref()._advancing and number>=0 and number<=4095: ref.get_ref().set_meta("voluntary_paths_page",number); ref.get_ref()._render())
