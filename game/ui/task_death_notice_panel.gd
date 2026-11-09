## 只显示本人真正收到且内容来源可核的死亡文书；不代签遗赠、继任或债务。
extends RefCounted
const QUERY="task_death_notices"
const META="task_death_notice_page"

static func _draft(host) -> Dictionary:
	var source: Dictionary=host.kernel.source_identity()
	var actor: String=host.kernel.command_context().get("actor_id","")
	var draft: Dictionary=host.get_meta(META,{})
	if draft.get("source")!=source or draft.get("actor_id")!=actor:
		draft={"source":source.duplicate(true),"actor_id":actor,"cursor":0}
		host.set_meta(META,draft.duplicate(true))
	return draft

## 未启用的新模块保原页面；异步未齐时等待原预读队列，不显示旧来源状态。
static func prepare(host) -> bool:
	if not host._resident_mode: return true
	var draft: Dictionary=_draft(host)
	var page: Dictionary=host.kernel.demand_view(QUERY,{"cursor":draft.cursor})
	if not page.get("ok",false): return page.get("code")!="NativeAsyncRequired"
	return page.get("source_identity")==draft.source and page.get("observer_actor_id")==draft.actor_id

static func visible_here(host) -> bool:
	if not host._resident_mode: return false
	var draft: Dictionary=_draft(host)
	var page: Dictionary=host.kernel.demand_view(QUERY,{"cursor":draft.cursor})
	return page.get("ok",false) and page.get("source_identity")==draft.source and page.get("observer_actor_id")==draft.actor_id and (not page.get("rows",[]).is_empty() or page.has("next_cursor") or draft.cursor>0)

static func build(host, parent: Control) -> void:
	if not host._resident_mode: return
	var draft: Dictionary=_draft(host)
	var page: Dictionary=host.kernel.demand_view(QUERY,{"cursor":draft.cursor})
	if not page.get("ok",false):
		if page.get("code") not in ["TaskMessageNoticeProfileRequired","NativeAsyncRequired"]: host._text(parent,"死亡文书暂不能查看："+host._reason(page.get("code","Unknown")),15)
		return
	if page.get("source_identity")!=draft.source or page.get("observer_actor_id")!=draft.actor_id: return
	for row in page.get("rows",[]):
		var box=VBoxContainer.new();box.name="TaskDeathNoticeRow";parent.add_child(box)
		host._text(box,"关于 "+row.get("decedent_name","")+" 的死亡文书",17)
		host._text(box,"亲历发信人："+row.get("sender_name","")+" · 实收：第 "+str(row.get("received_tick",0))+" 半日",14)
		host._text(box,row.get("explanation",""),14)
		if host.kernel.has_method("private_shop_liquidations_enabled") and host.kernel.private_shop_liquidations_enabled() and not str(row.get("binding_fact_id","")).is_empty(): _estate_button(host,box,str(row.binding_fact_id),draft)
	if draft.cursor>0: _button(host,parent,"返回首批",0,draft)
	if page.has("next_cursor"): _button(host,parent,"更多已收文书",int(page.next_cursor),draft)

## 翻页只改本人当前画面；来源改变、关闭或控件销毁后的回调全部失效。
static func _button(host, parent: Control, text: String, cursor: int, draft: Dictionary) -> void:
	var button: Button=host._button(parent,text,"task_death_notice_page","",func(): pass,host._advancing)
	button.pressed.connect(_page.bind(weakref(host),weakref(button),cursor,draft.source.duplicate(true),draft.actor_id,host._scene_counter))

static func _page(host_ref: WeakRef, button_ref: WeakRef, cursor: int, source: Dictionary, actor: String, epoch: int) -> void:
	var host=host_ref.get_ref();var button=button_ref.get_ref()
	if not is_instance_valid(host) or host.is_queued_for_deletion() or not host.is_inside_tree(): return
	if not is_instance_valid(button) or button.is_queued_for_deletion() or not button.is_inside_tree() or not button.is_visible_in_tree(): return
	if host._advancing or not host._storage_intent.is_empty() or host._scene_counter!=epoch: return
	if host.kernel.source_identity()!=source or host.kernel.command_context().get("actor_id")!=actor: return
	var draft: Dictionary=host.get_meta(META,{})
	if draft.get("source")!=source or draft.get("actor_id")!=actor: return
	draft.cursor=cursor;host.set_meta(META,draft.duplicate(true));host._render()

## 只将当前本人实收的精确binding带到限店页，不因文书自动授执行权或扫描全部店铺。
static func _estate_button(host, parent: Control, binding: String, draft: Dictionary) -> void:
	var button: Button=host._button(parent,"按这份文书核对限店委托","task_death_notice_estate","",func(): pass,host._advancing)
	button.pressed.connect(_estate.bind(weakref(host),weakref(button),binding,draft.source.duplicate(true),draft.actor_id,host._scene_counter))

## 保实际控件、来源、当前本人和场景寿命；旧页回调不能换用另一封文书。
static func _estate(host_ref: WeakRef, button_ref: WeakRef, binding: String, source: Dictionary, actor: String, epoch: int) -> void:
	var host=host_ref.get_ref();var button=button_ref.get_ref()
	if not is_instance_valid(host) or host.is_queued_for_deletion() or not host.is_inside_tree(): return
	if not is_instance_valid(button) or button.is_queued_for_deletion() or not button.is_inside_tree() or not button.is_visible_in_tree(): return
	if host._advancing or not host._storage_intent.is_empty() or host._scene_counter!=epoch: return
	if host.kernel.source_identity()!=source or host.kernel.command_context().get("actor_id")!=actor: return
	if not host.kernel.has_method("private_shop_liquidations_enabled") or not host.kernel.private_shop_liquidations_enabled(): return
	var draft: Dictionary=host.get_meta(META,{})
	if draft.get("source")!=source or draft.get("actor_id")!=actor: return
	var page: Dictionary=host.kernel.demand_view(QUERY,{"cursor":draft.cursor})
	if not page.get("ok",false) or page.get("source_identity")!=source or page.get("observer_actor_id")!=actor: return
	var found: bool=false
	for row in page.get("rows",[]):
		if row.get("binding_fact_id")==binding and row.get("recipient_actor_id")==actor: found=true;break
	if not found: return
	var panel=load("res://ui/private_shop_liquidations_panel.gd")
	panel.show_notice(host,binding,source,actor)
