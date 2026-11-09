## 有限本人照护接手页，只沿原生来源提交原ID；远方死亡、监护和旧粮处分权均不由UI推断。
extends RefCounted
const Guard=preload("res://ui/family_legacy_screen.gd")
const REASONS={"ChildCareReliefOriginalStopRequired":"原照护仍未明确中止或完成，不能直接替换。","ChildCareOriginalActionPending":"原照护正在进行，请先按本人权限处理原行动。","ChildCareReliefAlreadyActive":"孩子已有一份未结束的有限援助。","ChildCareReliefLivingParentRequired":"这一步需要在世原父母本人的真实决定。","ChildCareReliefOwnInventoryRequired":"援助只能来自提供者自己的私人粮食。","InsufficientChildCareReliefFood":"提供者本人可用私粮不足，不能挪用旧预约或公产。","ChildCareReliefActionPending":"本次照护尚在进行，不能直接撤回承诺。","ChildCareReliefNotAtPlace":"孩子和照护者须实际同处约定地点。","ChildCareReliefMinorRequired":"该人物已不适用儿童照护条款。"}

## 旧报价或异步回调不得跨换人/读档/页面关闭继续授权；两次确认仍须原生再核。
static func _submit(host_ref: WeakRef,panel_ref: WeakRef,button_ref: WeakRef,status_ref: WeakRef,source: Dictionary,kind: String,params: Dictionary) -> void:
	var button: Button=button_ref.get_ref(); var status: Label=status_ref.get_ref()
	if not is_instance_valid(button) or not is_instance_valid(status): return
	if not Guard._live(host_ref,panel_ref,source): button.disabled=true; status.text="人物或世界已改变，请重新打开照护接手。"; return
	var host=host_ref.get_ref(); var panel: Control=panel_ref.get_ref()
	if button.disabled or panel.get_meta("relief_pending",false): return
	var quote: Dictionary=host.kernel.preview(kind,params)
	if quote.get("code")=="NativeAsyncRequired": status.text="正在核对本人意愿、原责任与私粮，请稍候。"; return
	if not quote.get("ok",false) or quote.get("revision",-1)!=source.revision: status.text=REASONS.get(quote.get("code",""),"当前本人来源或实际条件尚不满足，请核对原记录。"); return
	var exact={"kind":kind,"params":params.duplicate(true)}
	if button.get_meta("confirmed",{})!=exact:
		button.set_meta("confirmed",exact); button.text="再次确认"
		if kind=="family_care_relief_accept": status.text="再次确认接受这份有限帮助，预约提供者自己的私粮；原出生责任仍在，儿童并未变更亲代或监护。"
		elif kind=="family_child_care_relief": status.text="再次确认本人投入一期照护时间，实际完成后才消耗约定的一期私粮。"
		elif kind=="family_care_relief_cancel": status.text="再次确认停止本次照护，不记已完成、不消费粮食；两份原承诺各自保留。"
		else: status.text="再次确认终止尚未提供的本份志愿帮助，未消费新粮仅解除提供者自己的预约；不释放原出生预约。"
		return
	if not Guard._live(host_ref,panel_ref,source): return
	panel.set_meta("relief_pending",true); button.disabled=true; host._perform(kind,params)

## 操作参数只来自本人有限页，不允许提交预留量、期数或死亡源替换原报价。
static func _operation(host,parent: Control,label: String,kind: String,params: Dictionary,available: bool,status: Label,host_ref: WeakRef,panel_ref: WeakRef,source: Dictionary) -> void:
	var binding={"button":null}
	var button: Button=host._button(parent,label,kind,"",func(): _submit(host_ref,panel_ref,binding.button,weakref(status),source,kind,params))
	button.name="ChildCareReliefAction"; binding.button=weakref(button); button.disabled=not source.ok or not available

## 有限期和私粮数量是本人听到的原条款，不显示未通知的远处死亡或他人照护详情。
static func _terms(host,parent: Control,t: Dictionary) -> void:
	host._text(parent,"约定每期%d半日，实际完成消耗%d份提供者私粮，最多%d期。"%[int(t.get("period_ticks",0)),int(t.get("food_per_period",0)),int(t.get("prepaid_periods",0))],17)

## 页面关闭不撤回有效承诺；切页和迟到回调再次核同一人物与完整世界读戳。
static func build(host,parent: Control,view: Dictionary,on_close: Callable,on_page: Callable) -> Control:
	var panel=PanelContainer.new(); panel.name="ChildCareReliefScreen"; parent.add_child(panel)
	panel.anchor_left=.04; panel.anchor_right=.96; panel.anchor_top=.12; panel.anchor_bottom=.96
	var column=VBoxContainer.new(); panel.add_child(column); column.add_theme_constant_override("separation",10)
	var source: Dictionary=Guard._source(host)
	if view.get("actor_id")!=source.actor_id or view.get("source_identity")!=source.identity: source.ok=false
	var host_ref: WeakRef=weakref(host); var panel_ref: WeakRef=weakref(panel)
	var title=HBoxContainer.new(); column.add_child(title); host._text(title,"本人照护接手",25).size_flags_horizontal=Control.SIZE_EXPAND_FILL
	host._button(title,"返回家事","relief_close","",func():
		var current: Control=panel_ref.get_ref()
		if is_instance_valid(current) and current.is_inside_tree() and current.is_visible_in_tree() and on_close.is_valid(): current.hide(); on_close.call())
	var status: Label=host._text(column,view.get("explanation","本人真实同意后才能开始有限帮助。"),17)
	var scroll=ScrollContainer.new(); scroll.horizontal_scroll_mode=ScrollContainer.SCROLL_MODE_DISABLED; scroll.size_flags_vertical=Control.SIZE_EXPAND_FILL; column.add_child(scroll)
	var body=VBoxContainer.new(); body.size_flags_horizontal=Control.SIZE_EXPAND_FILL; scroll.add_child(body)
	for row in view.get("current_offers",[]):
		host._text(body,"%s亲口愿意照看%s"%[row.provider.name,row.child.name],20); _terms(host,body,row.terms)
		_operation(host,body,"本人接受有限帮助","family_care_relief_accept",{"offer_id":row.offer_id,"child_actor_id":row.child.actor_id},row.option.get("available",false),status,host_ref,panel_ref,source)
	for row in view.get("agreements",[]):
		body.add_child(HSeparator.new()); host._text(body,"%s · %s的有限照护"%[row.child.name,row.provider.name],20); _terms(host,body,row.terms)
		host._text(body,"本份志愿帮助已终结，原出生责任另行保留。" if row.status=="finite_help_closed" else "本人已记录这份有限帮助，不能据此推知远方履行详情。",17)
		if row.has("personally_completed_periods"): host._text(body,"本人实际完成%d期。"%int(row.personally_completed_periods),17)
		for option in row.get("options",[]):
			var params={"action_id":option.action_id} if option.kind=="family_care_relief_cancel" else {"accepted_id":row.accepted_id}
			_operation(host,body,{"family_care_relief_withdraw":"停止尚未提供的帮助","family_child_care_relief":"本人照护一期","family_care_relief_cancel":"停止本次照护"}.get(option.kind,"办理"),option.kind,params,option.get("available",false),status,host_ref,panel_ref,source)
			if not option.get("available",false): host._text(body,REASONS.get(option.get("code",""),"当前本人或实际条件尚不能办理。"),16)
	for row in view.get("care",[]): host._text(body,"%s · 第%d期 · %s"%[row.child.name,int(row.period_index),row.explanation],17)
	if view.get("current_offers",[]).is_empty(): host._text(body,"请先在真实会面中听到本人意愿；原照护没有具名中止来源时不会显示接手许可。",17)
	var pager=HBoxContainer.new(); column.add_child(pager)
	var total: int=maxi(int(view.get("offer_count",0)),maxi(int(view.get("agreement_count",0)),int(view.get("care_count",0))))
	for delta in [-1,1]:
		var target: int=int(view.get("page",0))+delta
		var button: Button=host._button(pager,"上一页" if delta<0 else "下一页","relief_page","",func():
			if Guard._live(host_ref,panel_ref,source) and on_page.is_valid(): on_page.call(target))
		button.disabled=not source.ok or target<0 or target>4095 or target*16>=total
	return panel

## 仅显式启用志愿接手模式才打开此页，旧家庭模式保持原入口。
static func selected(host) -> bool:
	return host._resident_mode and host.get_meta("family_tab","family")=="family_care_relief" and host.kernel.has_method("family_care_relief_enabled") and host.kernel.family_care_relief_enabled()

## 离开清除待确认界面，不写任何业务状态。
static func _return(host_ref: WeakRef) -> void:
	var host=host_ref.get_ref()
	if is_instance_valid(host) and host._family_open: host.set_meta("family_tab","family"); host._render()

## 每次新世界身份重新取有限页，不能跨读档复用旧的孩子或接手Action编号。
static func render(host,parent: Control) -> void:
	var source: Dictionary=Guard._source(host)
	if host.get_meta("care_relief_navigation_source",{})!=source: host.set_meta("care_relief_navigation_source",source.duplicate(true)); host.set_meta("care_relief_page",0)
	var ref: WeakRef=weakref(host)
	var view: Dictionary=host.kernel.family_care_relief_page(int(host.get_meta("care_relief_page",0)))
	build(host,parent,view,_return.bind(ref),func(page: int):
		if is_instance_valid(ref.get_ref()) and selected(ref.get_ref()) and not ref.get_ref()._advancing and page>=0 and page<=4095: ref.get_ref().set_meta("care_relief_page",page); ref.get_ref()._render())
