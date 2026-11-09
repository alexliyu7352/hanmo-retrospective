## 本人明确金额的自愿垫款入口；上限只提示，不自动填满或代付私钱。
extends RefCounted
const META="task_personal_advance_draft"
const QUERY="task_personal_advances"
const COMMAND="task_personal_advance"

static func visible_here(host) -> bool:
	if not host._resident_mode: return false
	var draft: Dictionary=_draft(host)
	var page: Dictionary=host.kernel.demand_view(QUERY,{"cursor":draft.cursor})
	return page.get("ok",false) and page.get("source_identity")==draft.source and not page.get("rows",[]).is_empty()

static func _draft(host) -> Dictionary:
	var source: Dictionary=host.kernel.source_identity()
	var actor: String=host.kernel.command_context().get("actor_id","")
	var draft: Dictionary=host.get_meta(META,{})
	if draft.get("source")!=source or draft.get("actor_id")!=actor:
		draft={"source":source.duplicate(true),"actor_id":actor,"cursor":0,"amounts":{}}
		host.set_meta(META,draft.duplicate(true))
	return draft

## 所有预读进入原队列；无明确金额时不请求付款报价。
static func prepare(host) -> bool:
	if not host._resident_mode: return true
	var draft: Dictionary=_draft(host)
	var page: Dictionary=host.kernel.demand_view(QUERY,{"cursor":draft.cursor})
	if not page.get("ok",false): return page.get("code")!="NativeAsyncRequired"
	if page.get("source_identity")!=draft.source or page.get("observer_actor_id")!=draft.actor_id: return false
	var ready: bool=true
	for row in page.get("rows",[]):
		var amount: int=int(draft.amounts.get(row.task_id,0))
		if amount>0 and amount<=row.get("available_maximum",0) and row.get("can_advance",false):
			if host.kernel.preview(COMMAND,{"task_id":row.task_id,"amount":amount},draft.actor_id).get("code")=="NativeAsyncRequired": ready=false
	return ready

static func build(host, parent: Control) -> void:
	if not host._resident_mode: return
	var draft: Dictionary=_draft(host)
	var page: Dictionary=host.kernel.demand_view(QUERY,{"cursor":draft.cursor})
	if not page.get("ok",false):
		if page.get("code") not in ["TaskAdvanceProfileRequired","NativeAsyncRequired"]: host._text(parent,"垫款记录暂不能查看："+host._reason(page.get("code","Unknown")),15)
		return
	if page.get("source_identity")!=draft.source or page.get("observer_actor_id")!=draft.actor_id or page.get("rows",[]).is_empty(): return
	var box=VBoxContainer.new();box.name="TaskPersonalAdvances";parent.add_child(box)
	host._text(box,"受托经费与本人垫款",19)
	for row in page.rows:
		var item=VBoxContainer.new();item.name="TaskPersonalAdvanceRow";box.add_child(item)
		host._text(item,row.get("display_name","")+" · "+{"authorized":"原合同允许自愿垫付","advanced":"已有本人垫款","awaiting_repayment":"原责任待清算","repaid":"本金已实际归还"}.get(row.get("status",""),"核对记录"),17)
		host._text(item,"累计垫入%d钱，已归还%d钱，尚待归还%d钱。"%[row.get("paid",0),row.get("repaid",0),row.get("principal_due",0)],15)
		host._text(item,row.get("explanation",""),14)
		if not row.get("can_advance",false):
			host._text(item,"此时不能新增垫款："+host._reason(row.get("blocked_code","Unknown")),14)
			continue
		var maximum: int=int(row.get("available_maximum",0))
		var amount: int=int(draft.amounts.get(row.task_id,0))
		host._text(item,"本次最多可垫%d钱；请填写你自愿支付的金额。"%maximum,14)
		var spin=SpinBox.new();spin.name="TaskPersonalAdvanceAmount";spin.min_value=0;spin.max_value=maximum;spin.step=1;spin.value=min(amount,maximum);spin.editable=not host._advancing;item.add_child(spin)
		spin.value_changed.connect(_remember.bind(weakref(host),weakref(spin),row.task_id,draft.source.duplicate(true),draft.actor_id,host._scene_counter))
		var params: Dictionary={"task_id":row.task_id,"amount":amount}
		var quote: Dictionary={"ok":false,"code":"TaskAdvanceAmountRequired"}
		if amount>0 and amount<=maximum: quote=host.kernel.preview(COMMAND,params,draft.actor_id)
		var button: Button=host._button(item,"自愿垫入%d钱"%amount if amount>0 else "填写自愿垫付金额",COMMAND,row.task_id,func(): pass,host._advancing or not quote.get("ok",false),false)
		button.name="TaskPersonalAdvanceSubmit"
		button.pressed.connect(_submit.bind(weakref(host),weakref(button),params.duplicate(true),draft.source.duplicate(true),draft.actor_id,host._scene_counter))
		if amount>0 and not quote.get("ok",false): host._text(item,"正在核对付款条件…" if quote.get("code")=="NativeAsyncRequired" else "这笔垫款暂不能支付："+host._reason(quote.get("code","Unknown")),14)
	if draft.cursor>0: _page_button(host,box,"返回首批",0,draft)
	if page.has("next_cursor"): _page_button(host,box,"更多垫款记录",int(page.next_cursor),draft)

## 弱引用先判活，再核完整来源、本人和场景代数；销毁后的旧控件不能改草稿或付款。
static func _live(host_ref: WeakRef, control_ref: WeakRef, source: Dictionary, actor: String, epoch: int) -> bool:
	var host=host_ref.get_ref();var control=control_ref.get_ref()
	if not is_instance_valid(host) or host.is_queued_for_deletion() or not host.is_inside_tree(): return false
	if not is_instance_valid(control) or not control is Control or control.is_queued_for_deletion() or not control.is_inside_tree() or not control.is_visible_in_tree(): return false
	if host._advancing or not host._storage_intent.is_empty() or host._scene_counter!=epoch: return false
	return source==host.kernel.source_identity() and actor==host.kernel.command_context().get("actor_id") and host.get_meta(META,{}).get("source")==source

static func _remember(value: float, host_ref: WeakRef, spin_ref: WeakRef, task: String, source: Dictionary, actor: String, epoch: int) -> void:
	if not _live(host_ref,spin_ref,source,actor,epoch): return
	var host=host_ref.get_ref();var draft: Dictionary=host.get_meta(META,{})
	draft.amounts[task]=int(value);host.set_meta(META,draft.duplicate(true));host._render()

static func _submit(host_ref: WeakRef, button_ref: WeakRef, params: Dictionary, source: Dictionary, actor: String, epoch: int) -> Dictionary:
	if not _live(host_ref,button_ref,source,actor,epoch): return {"ok":false,"code":"StaleSceneRequest"}
	var host=host_ref.get_ref();var button=button_ref.get_ref()
	if button.disabled or params.get("amount",0)<=0 or int(host.get_meta(META,{}).get("amounts",{}).get(params.task_id,0))!=params.amount: return {"ok":false,"code":"StalePreview"}
	var context: Dictionary=host.kernel.command_context()
	if not context.get("ok",false) or context.get("actor_id")!=actor or context.get("revision")!=source.revision: return {"ok":false,"code":"StalePreview"}
	var quote: Dictionary=host.kernel.preview(COMMAND,params,actor)
	if not quote.get("ok",false): host._message="这笔垫款暂不能支付："+host._reason(quote.get("code","Unknown"));host._render();return quote
	if quote.get("revision")!=source.revision: return {"ok":false,"code":"StalePreview"}
	button.disabled=true
	return host._resident_submit_frozen({"kind":COMMAND,"params":params.duplicate(true),"actor_id":actor,"request_seq":context.next_request_seq,"expected_revision":source.revision})

static func _page_button(host, parent: Control, text: String, cursor: int, draft: Dictionary) -> void:
	var button: Button=host._button(parent,text,"task_advance_page","",func(): pass,host._advancing)
	button.pressed.connect(_page.bind(weakref(host),weakref(button),cursor,draft.source.duplicate(true),draft.actor_id,host._scene_counter))

static func _page(host_ref: WeakRef, button_ref: WeakRef, cursor: int, source: Dictionary, actor: String, epoch: int) -> void:
	if not _live(host_ref,button_ref,source,actor,epoch): return
	var host=host_ref.get_ref();var draft: Dictionary=host.get_meta(META,{})
	draft.cursor=cursor;host.set_meta(META,draft.duplicate(true));host._render()
