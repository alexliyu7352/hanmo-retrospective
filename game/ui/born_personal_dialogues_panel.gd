## 眼前成年亲生本人沿真实出生模板开口；只开原会话，不代其答允。
extends RefCounted
const QUERY="born_personal_dialogues"
const META="born_personal_dialogues_draft"

## 错误提示不透露远方是否有回复，也不将未领取说成对方已知。
static func _reason(host, code: String) -> String:
	match code:
		"BornPersonalDialogueNotAvailable": return "须在原地点与实际成年本人当面交谈。"
		"BornPersonalDialogueBusy": return "请先结束当前行动或会话；不替本人关闭另一场交谈。"
		"BornPersonalDialoguePlayerRequired": return "须由当前本人明确开始交谈。"
		"StaleBornPersonalDialogueQuote": return "现场或来源已变化，请重新核对。"
	return host._reason(code)

## 草稿绑定完整来源和本人；切换世界或控制者后旧交谈请求不可复用。
static func _draft(host) -> Dictionary:
	var source: Dictionary=host.kernel.source_identity()
	var actor: String=host.kernel.command_context().get("actor_id","")
	var draft: Dictionary=host.get_meta(META,{})
	if draft.get("source")!=source or draft.get("actor_id")!=actor:
		draft={"source":source.duplicate(true),"actor_id":actor,"cursor":0,"selected":{}}
		host.set_meta(META,draft.duplicate(true))
	return draft


## 页面及按钮走原异步队列；当前原报价未通过时不能办理。
static func prepare(host) -> bool:
	if not host._resident_mode: return true
	var draft: Dictionary=_draft(host)
	var page: Dictionary=host.kernel.demand_view(QUERY,{"cursor":draft.cursor})
	if not page.get("ok",false): return page.get("code")!="NativeAsyncRequired"
	if page.get("source_identity")!=draft.source or page.get("observer_actor_id")!=draft.actor_id: return false
	var ready: bool=true
	for row in page.get("rows",[]):
		for action in row.get("actions",[]):
			if action.get("available",false) and host.kernel.preview(action.command,action.params,draft.actor_id).get("code")=="NativeAsyncRequired": ready=false
	return ready

static func visible_here(host) -> bool:
	if not host._resident_mode: return false
	var draft: Dictionary=_draft(host)
	var page: Dictionary=host.kernel.demand_view(QUERY,{"cursor":draft.cursor})
	return page.get("ok",false) and page.get("source_identity")==draft.source and page.get("observer_actor_id")==draft.actor_id and (not page.get("rows",[]).is_empty() or draft.cursor>0)

## 只呈现原场景已在场的本人，原会话开始后沿既有对白继续。
static func build(host, parent: Control) -> void:
	if not host._resident_mode: return
	var draft: Dictionary=_draft(host)
	var page: Dictionary=host.kernel.demand_view(QUERY,{"cursor":draft.cursor})
	if not page.get("ok",false):
		if page.get("code") not in ["BornPersonalDialogueProfileRequired","NativeAsyncRequired"]: host._text(parent,"本人私人交谈暂不能查看："+_reason(host,page.get("code","Unknown")),15)
		return
	if page.get("source_identity")!=draft.source or page.get("observer_actor_id")!=draft.actor_id: return
	var rows: Array=page.get("rows",[])
	if rows.is_empty() and draft.cursor==0: return
	var box=VBoxContainer.new();box.name="BornPersonalDialogues";parent.add_child(box)
	host._text(box,"本人私人交谈",19)
	for row in rows:
		var item=VBoxContainer.new();item.name="BornPersonalDialoguesRow";box.add_child(item)
		host._text(item,row.get("display_name","原本人私人交谈")+" · "+row.get("status_text",""),17)
		if not row.get("progress_text","").is_empty(): host._text(item,row.progress_text,14)
		for action in row.get("actions",[]):
			var quote: Dictionary={"ok":false,"code":action.get("code","Unavailable")}
			if action.get("available",false): quote=host.kernel.preview(action.command,action.params,draft.actor_id)
			var enabled: bool=action.get("available",false) and quote.get("ok",false)
			var button: Button=host._button(item,"核对："+action.get("text","办理"),"born_personal_dialogue_review",row.id,func(): pass,host._advancing or not enabled,false)
			button.pressed.connect(_choose.bind(weakref(host),weakref(button),action.duplicate(true),draft.source.duplicate(true),draft.actor_id,host._scene_counter))
			if not enabled and quote.get("code")!="NativeAsyncRequired": host._text(item,_reason(host,quote.get("code","Unavailable")),14)
			if draft.selected==action:
				host._text(item,action.get("summary",row.get("terms_text","")),15)
				var confirm: Button=host._button(item,"确认本人决定："+action.get("text","办理"),action.command,row.id,func(): pass,host._advancing or not enabled,false)
				confirm.name="BornPersonalDialoguesConfirm"
				confirm.pressed.connect(_submit.bind(weakref(host),weakref(confirm),action.duplicate(true),draft.source.duplicate(true),draft.actor_id,host._scene_counter))
				var cancel: Button=host._button(item,"返回核对，不提交","born_personal_dialogue_back",row.id,func(): pass,host._advancing,false)
				cancel.pressed.connect(_choose.bind(weakref(host),weakref(cancel),{},draft.source.duplicate(true),draft.actor_id,host._scene_counter))
	if draft.cursor>0: _page_button(host,box,"在场本人首批","cursor",0,draft)
	if page.has("next_cursor"): _page_button(host,box,"更多在场本人","cursor",int(page.next_cursor),draft)

## 销毁、换人、离开页面或来源变化后的旧回调不能替他人作答。
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
	if action.get("command") not in ["open_born_personal_dialogue"]: return {"ok":false,"code":"UnsupportedAction"}
	var context: Dictionary=host.kernel.command_context()
	if not context.get("ok",false) or context.get("actor_id")!=actor or context.get("revision")!=source.revision: return {"ok":false,"code":"StalePreview"}
	var quote: Dictionary=host.kernel.preview(action.command,action.params,actor)
	if not quote.get("ok",false): host._message="这次交谈暂不能开始："+_reason(host,quote.get("code","Unknown"));host._render();return quote
	if quote.get("revision")!=source.revision: return {"ok":false,"code":"StalePreview"}
	button.disabled=true
	return host._resident_submit_frozen({"kind":action.command,"params":action.params.duplicate(true),"actor_id":actor,"request_seq":context.next_request_seq,"expected_revision":source.revision})

static func _page_button(host, parent: Control, text: String, field: String, cursor: int, draft: Dictionary) -> void:
	var button: Button=host._button(parent,text,"born_personal_dialogue_page","",func(): pass,host._advancing)
	button.pressed.connect(_page.bind(weakref(host),weakref(button),field,cursor,draft.source.duplicate(true),draft.actor_id,host._scene_counter))

## 翻页只变有限查询游标，清掉尚未提交的确认草稿。
static func _page(host_ref: WeakRef, control_ref: WeakRef, field: String, cursor: int, source: Dictionary, actor: String, epoch: int) -> void:
	if not _live(host_ref,control_ref,source,actor,epoch) or field!="cursor": return
	var host=host_ref.get_ref();var draft: Dictionary=host.get_meta(META,{})
	var page: Dictionary=host.kernel.demand_view(QUERY,{"cursor":draft.get("cursor",0)})
	if not page.get("ok",false) or page.get("source_identity")!=source or page.get("observer_actor_id")!=actor or (cursor!=0 and cursor!=page.get("next_cursor",-1)): return
	draft[field]=cursor;draft.selected={};host.set_meta(META,draft.duplicate(true));host._render()
