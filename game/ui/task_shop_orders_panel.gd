## 店单本人办理只交原Task命令；接受预约原客户款，实际当面交货后才确认收款。
extends RefCounted
const QUERY="task_shop_orders"
const META="task_shop_orders_draft"

## 已知办理拒绝给下一步说明；未知冷档/规则码仍交统一诊断，不翻译成成功。
static func _reason(host, code: String) -> String:
	match code:
		"TaskShopOrderStockMissing": return "原店库存中可交付的货物不足，请先补齐店里的货。"
		"TaskShopOrderClientFundsMissing": return "客户当前可用的钱不足以预约整份订单价款，暂不能受领。"
		"TaskShopOrderDeadlinePassed": return "剩余期限不足以完成一个半日的交付，可取消尚未交货的订单。"
		"TaskShopOrderDeliveryAlreadyActive", "TaskShopOrderActionStillActive": return "这份店单正在当面交付，请先完成当前半日。"
		"TaskShopOrderClientDeclarationRequired", "PrivateShopOrderClientDeclarationRequired", "ProviderDeclarationRequired", "AuthoredNodeSourceRequired": return "请与原客户当面交谈，停留在对方说明这份订单的原话处再办理。"
		"TaskShopOrderAssetAuthorityMismatch": return "原店或客户的钱包、库存权限已不符合这份订单，当前不能交货。"
		"TaskShopOrderCancellationUnavailable": return "只有本人空闲且这份订单尚未交付时，才可取消。"
		"StaleTaskShopOrderQuote", "StaleProviderDeclarationSource", "StaleAuthoredNodeSource": return "当前人物、会面或时刻已经变化，请重新核对这份店单。"
	return host._reason(code)

## 草稿绑定完整来源和本人，换世界、时刻或人物后不保留旧签约按钮。
static func _draft(host) -> Dictionary:
	var source: Dictionary=host.kernel.source_identity()
	var actor: String=host.kernel.command_context().get("actor_id","")
	var draft: Dictionary=host.get_meta(META,{})
	if draft.get("source")!=source or draft.get("actor_id")!=actor:
		draft={"source":source.duplicate(true),"actor_id":actor,"cursor":0,"order_id":"","selected":{}}
		host.set_meta(META,draft.duplicate(true))
	return draft


## 店页仅把本人已选原订单交来；旧页、旧世界或旧人物不能打开新的接受草稿。
static func show_order(host, order_id: String, source: Dictionary, actor: String) -> bool:
	if not host._resident_mode or order_id.is_empty() or host._advancing: return false
	if host.kernel.source_identity()!=source or host.kernel.command_context().get("actor_id")!=actor: return false
	var draft: Dictionary=_draft(host)
	draft.order_id=order_id;draft.selected={};host.set_meta(META,draft.duplicate(true));host._render()
	return true

## 页面及按钮走原异步队列；当前原报价未通过时不能办理。
static func prepare(host) -> bool:
	if not host._resident_mode: return true
	var draft: Dictionary=_draft(host)
	var page: Dictionary=host.kernel.demand_view(QUERY,{"cursor":draft.cursor,"order_id":draft.order_id})
	if not page.get("ok",false): return page.get("code")!="NativeAsyncRequired"
	if page.get("source_identity")!=draft.source or page.get("observer_actor_id")!=draft.actor_id: return false
	var ready: bool=true
	for row in page.get("rows",[])+page.get("offers",[]):
		for action in row.get("actions",[]):
			if action.get("available",false) and host.kernel.preview(action.command,action.params,draft.actor_id).get("code")=="NativeAsyncRequired": ready=false
	return ready

static func visible_here(host) -> bool:
	if not host._resident_mode: return false
	var draft: Dictionary=_draft(host)
	var page: Dictionary=host.kernel.demand_view(QUERY,{"cursor":draft.cursor,"order_id":draft.order_id})
	return page.get("ok",false) and page.get("source_identity")==draft.source and page.get("observer_actor_id")==draft.actor_id and (not page.get("rows",[]).is_empty() or not page.get("offers",[]).is_empty() or draft.cursor>0 or not draft.order_id.is_empty())

## 当前选中待接订单与本人已受领责任分列；只显示真实冻结条款和原付款结果。
static func build(host, parent: Control) -> void:
	if not host._resident_mode: return
	var draft: Dictionary=_draft(host)
	var page: Dictionary=host.kernel.demand_view(QUERY,{"cursor":draft.cursor,"order_id":draft.order_id})
	if not page.get("ok",false):
		if page.get("code") not in ["TaskShopOrderProfileRequired","NativeAsyncRequired"]: host._text(parent,"店单责任暂不能查看："+_reason(host,page.get("code","Unknown")),15)
		return
	if page.get("source_identity")!=draft.source or page.get("observer_actor_id")!=draft.actor_id: return
	var rows: Array=page.get("rows",[])+page.get("offers",[])
	if rows.is_empty() and draft.cursor==0 and draft.order_id.is_empty(): return
	var box=VBoxContainer.new();box.name="TaskShopOrders";parent.add_child(box)
	host._text(box,"本人店单与交付",19)
	for row in rows:
		var item=VBoxContainer.new();item.name="TaskShopOrdersRow";box.add_child(item)
		host._text(item,row.get("display_name","店单约定")+" · "+row.get("status_text",""),17)
		host._text(item,row.get("terms_text",""),14)
		if not row.get("progress_text","").is_empty(): host._text(item,row.progress_text,14)
		if not row.get("last_failure_code","").is_empty(): host._text(item,"上次交付中断："+_reason(host,row.last_failure_code),14)
		for action in row.get("actions",[]):
			var quote: Dictionary={"ok":false,"code":action.get("code","Unavailable")}
			if action.get("available",false): quote=host.kernel.preview(action.command,action.params,draft.actor_id)
			var enabled: bool=action.get("available",false) and quote.get("ok",false)
			var button: Button=host._button(item,"核对："+action.get("text","办理"),"shop_order_review",row.id,func(): pass,host._advancing or not enabled,false)
			button.pressed.connect(_choose.bind(weakref(host),weakref(button),action.duplicate(true),draft.source.duplicate(true),draft.actor_id,host._scene_counter))
			if not enabled and quote.get("code")!="NativeAsyncRequired": host._text(item,_reason(host,quote.get("code","Unavailable")),14)
			if draft.selected==action:
				host._text(item,action.get("summary",row.get("terms_text","")),15)
				var confirm: Button=host._button(item,"确认本人决定："+action.get("text","办理"),action.command,row.id,func(): pass,host._advancing or not enabled,false)
				confirm.name="TaskShopOrdersConfirm"
				confirm.pressed.connect(_submit.bind(weakref(host),weakref(confirm),action.duplicate(true),draft.source.duplicate(true),draft.actor_id,host._scene_counter))
				var cancel: Button=host._button(item,"返回核对，不提交","shop_order_back",row.id,func(): pass,host._advancing,false)
				cancel.pressed.connect(_choose.bind(weakref(host),weakref(cancel),{},draft.source.duplicate(true),draft.actor_id,host._scene_counter))
	if draft.cursor>0: _page_button(host,box,"本人店单首批","cursor",0,draft)
	if page.has("next_cursor"): _page_button(host,box,"更多本人店单","cursor",int(page.next_cursor),draft)

## 销毁、换人、离开页面或来源变化后的旧回调不能重复受领、交货或取消。
static func _live(host_ref: WeakRef, control_ref: WeakRef, source: Dictionary, actor: String, epoch: int) -> bool:
	var host=host_ref.get_ref();var control=control_ref.get_ref()
	if not is_instance_valid(host) or host.is_queued_for_deletion() or not host.is_inside_tree(): return false
	if not is_instance_valid(control) or control.is_queued_for_deletion() or not control.is_inside_tree() or not control.is_visible_in_tree(): return false
	if host._advancing or not host._storage_intent.is_empty() or host._scene_counter!=epoch: return false
	return host.kernel.source_identity()==source and host.kernel.command_context().get("actor_id")==actor and host.get_meta(META,{}).get("source")==source

static func _choose(host_ref: WeakRef, control_ref: WeakRef, action: Dictionary, source: Dictionary, actor: String, epoch: int) -> void:
	if not _live(host_ref,control_ref,source,actor,epoch) or control_ref.get_ref().disabled: return
	var host=host_ref.get_ref();var draft: Dictionary=host.get_meta(META,{})
	draft.selected=action.duplicate(true);host.set_meta(META,draft.duplicate(true));host._render()

## 确认时再取原领域报价与请求序号；一个旧按钮至多提交一次原命令。
static func _submit(host_ref: WeakRef, control_ref: WeakRef, action: Dictionary, source: Dictionary, actor: String, epoch: int) -> Dictionary:
	if not _live(host_ref,control_ref,source,actor,epoch): return {"ok":false,"code":"StaleSceneRequest"}
	var host=host_ref.get_ref();var button=control_ref.get_ref()
	if button.disabled or host.get_meta(META,{}).get("selected")!=action: return {"ok":false,"code":"StalePreview"}
	if action.get("command") not in ["task_shop_order_accept","task_shop_order_deliver","task_shop_order_cancel"]: return {"ok":false,"code":"UnsupportedAction"}
	var context: Dictionary=host.kernel.command_context()
	if not context.get("ok",false) or context.get("actor_id")!=actor or context.get("revision")!=source.revision: return {"ok":false,"code":"StalePreview"}
	var quote: Dictionary=host.kernel.preview(action.command,action.params,actor)
	if not quote.get("ok",false): host._message="这项店单决定暂不能提交："+_reason(host,quote.get("code","Unknown"));host._render();return quote
	if quote.get("revision")!=source.revision: return {"ok":false,"code":"StalePreview"}
	button.disabled=true
	return host._resident_submit_frozen({"kind":action.command,"params":action.params.duplicate(true),"actor_id":actor,"request_seq":context.next_request_seq,"expected_revision":source.revision})

static func _page_button(host, parent: Control, text: String, field: String, cursor: int, draft: Dictionary) -> void:
	var button: Button=host._button(parent,text,"shop_order_page","",func(): pass,host._advancing)
	button.pressed.connect(_page.bind(weakref(host),weakref(button),field,cursor,draft.source.duplicate(true),draft.actor_id,host._scene_counter))

## 翻页只变有限查询游标，清掉尚未提交的确认草稿。
static func _page(host_ref: WeakRef, control_ref: WeakRef, field: String, cursor: int, source: Dictionary, actor: String, epoch: int) -> void:
	if not _live(host_ref,control_ref,source,actor,epoch) or field!="cursor": return
	var host=host_ref.get_ref();var draft: Dictionary=host.get_meta(META,{})
	draft[field]=cursor;draft.selected={};host.set_meta(META,draft.duplicate(true));host._render()
