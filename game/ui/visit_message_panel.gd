## 本人拜帖与当前接待节点，历史列表没有远方实时送达状态。
extends RefCounted
const QUERY="visit_messages"
const META="visit_message_review"

## 世界、人物或提交改变后旧选择失效；页游标只保留在同一观察身份内。
static func _draft(host) -> Dictionary:
	var source: Dictionary=host.kernel.source_identity()
	var actor: String=host.kernel.command_context().get("actor_id","")
	var draft: Dictionary=host.get_meta(META,{})
	if draft.get("source")!=source or draft.get("actor_id")!=actor:
		draft={"source":source.duplicate(true),"actor_id":actor,"selected":"","cursor":0,"offer_cursor":0}
		host.set_meta(META,draft.duplicate(true))
	return draft

static func _args(draft: Dictionary) -> Dictionary:
	return {"cursor":draft.cursor,"offer_cursor":draft.offer_cursor}

static func _same(page: Dictionary,draft: Dictionary) -> bool:
	return page.get("source_identity")==draft.source and page.get("observer_actor_id")==draft.actor_id

## 有限页和真实报价一起预读，缺失模块不冒空成功结果。
static func prepare(host) -> bool:
	if not host._resident_mode: return true
	var draft: Dictionary=_draft(host)
	var page: Dictionary=host.kernel.demand_view(QUERY,_args(draft))
	if not page.get("ok",false): return page.get("code")!="NativeAsyncRequired"
	if not _same(page,draft): return false
	var ready: bool=true
	for row in page.get("rows",[])+page.get("offers",[]):
		for action in row.get("actions",[]):
			if action.get("available",false) and host.kernel.preview(action.command,action.params,draft.actor_id).get("code")=="NativeAsyncRequired": ready=false
	return ready

## 场景侧栏只在本人有可见记录时出现，不把世界中其他人的拜帖数量泄露出来。
static func visible_here(host) -> bool:
	if not host._resident_mode: return false
	var draft: Dictionary=_draft(host)
	var page: Dictionary=host.kernel.demand_view(QUERY,_args(draft))
	return page.get("ok",false) and _same(page,draft) and (not page.get("rows",[]).is_empty() or not page.get("offers",[]).is_empty())

## 对话页和本人手记复用同一读口；不强开、关闭或交换任何会话参与人。
static func build(host,parent: Control) -> void:
	if not host._resident_mode: return
	var draft: Dictionary=_draft(host)
	var page: Dictionary=host.kernel.demand_view(QUERY,_args(draft))
	if not page.get("ok",false):
		if page.get("code") not in ["VisitMessageProfileRequired","NativeAsyncRequired"]: host._text(parent,"拜帖暂不能核对："+host._reason(page.get("code","Unknown")),15)
		return
	if not _same(page,draft): return
	host._text(parent,"本人拜帖",18)
	host._text(parent,"拜帖只表达曾来访、希望另约会面；不等于收件人答应，也不会遥知领取结果。",14)
	var rows: Array=page.get("offers",[])+page.get("rows",[])
	if rows.is_empty(): host._text(parent,"暂无本人已知拜帖；托留、领取与撤回须向原接待人当面办理。",15)
	for row in rows:
		var box=VBoxContainer.new();box.name="VisitMessageRow";parent.add_child(box)
		host._text(box,row.get("display_name",""),17)
		host._text(box,row.get("text",""),15)
		for action in row.get("actions",[]):
			var quote: Dictionary=host.kernel.preview(action.command,action.params,draft.actor_id)
			var enabled: bool=action.get("available",false) and quote.get("ok",false)
			var review: Button=host._button(box,action.get("text","核对拜帖"),"visit_message_review",row.id,func(): pass,host._advancing or not enabled,false)
			review.pressed.connect(_choose.bind(weakref(host),weakref(review),row.id,draft.source.duplicate(true),draft.actor_id,host._scene_counter))
			if not enabled: host._text(box,host._reason(quote.get("code",action.get("code","Unavailable"))),14)
			if draft.selected==row.id:
				host._text(box,"确认这项当面办理；不会扣钱、修改关系或自动约定会面。",14)
				var confirm: Button=host._button(box,"确认办理",action.command,row.id,func(): pass,host._advancing or not enabled,false)
				confirm.pressed.connect(_confirm.bind(weakref(host),weakref(confirm),row.id,action.duplicate(true),draft.source.duplicate(true),draft.actor_id,host._scene_counter))
				var cancel: Button=host._button(box,"暂不办理","visit_message_back",row.id,func(): pass,host._advancing,false)
				cancel.pressed.connect(_choose.bind(weakref(host),weakref(cancel),"",draft.source.duplicate(true),draft.actor_id,host._scene_counter))
	for key in ["cursor","offer_cursor"]:
		if int(draft[key])>0: _pager(host,parent,draft,key,maxi(0,int(draft[key])-16),"上一页")
		if page.has("next_"+key): _pager(host,parent,draft,key,int(page["next_"+key]),"更多拜帖" if key=="cursor" else "更多托留对象")

## 点击必须仍属于当前可见原页；旧actor、读档epoch或已消费按钮都不能提交。
static func _live(host_ref: WeakRef,control_ref: WeakRef,source: Dictionary,actor: String,epoch: int) -> bool:
	var host=host_ref.get_ref();var control=control_ref.get_ref()
	if not is_instance_valid(host) or host.is_queued_for_deletion() or not host.is_inside_tree(): return false
	if not is_instance_valid(control) or control.is_queued_for_deletion() or not control.is_inside_tree() or not control.is_visible_in_tree() or control.disabled: return false
	if host._advancing or not host._storage_intent.is_empty() or host._scene_counter!=epoch: return false
	return host.kernel.source_identity()==source and host.kernel.command_context().get("actor_id")==actor and host.get_meta(META,{}).get("source")==source

static func _choose(host_ref: WeakRef,control_ref: WeakRef,selected: String,source: Dictionary,actor: String,epoch: int) -> void:
	if not _live(host_ref,control_ref,source,actor,epoch): return
	var host=host_ref.get_ref();var draft: Dictionary=host.get_meta(META,{})
	draft.selected=selected;host.set_meta(META,draft);host._render()

static func _pager(host,parent: Control,draft: Dictionary,key: String,value: int,text: String) -> void:
	var button: Button=host._button(parent,text,"visit_message_page",key+str(value),func(): pass,host._advancing,false)
	button.pressed.connect(_page.bind(weakref(host),weakref(button),key,value,draft.source.duplicate(true),draft.actor_id,host._scene_counter))

static func _page(host_ref: WeakRef,control_ref: WeakRef,key: String,value: int,source: Dictionary,actor: String,epoch: int) -> void:
	if not _live(host_ref,control_ref,source,actor,epoch) or key not in ["cursor","offer_cursor"] or value<0: return
	var host=host_ref.get_ref();var draft: Dictionary=host.get_meta(META,{})
	draft[key]=value;draft.selected="";host.set_meta(META,draft);host._render()

## 二次确认重新读同一有限行和原预览，命令只携policy或原帖ID。
static func _confirm(host_ref: WeakRef,control_ref: WeakRef,row_id: String,action: Dictionary,source: Dictionary,actor: String,epoch: int) -> Dictionary:
	if not _live(host_ref,control_ref,source,actor,epoch): return {"ok":false,"code":"StaleSceneRequest"}
	var host=host_ref.get_ref();var draft: Dictionary=host.get_meta(META,{})
	if draft.get("selected")!=row_id or action.get("command") not in ["leave_visit_message","receive_visit_message","withdraw_visit_message"]: return {"ok":false,"code":"StaleVisitMessageQuote"}
	var page: Dictionary=host.kernel.demand_view(QUERY,_args(draft))
	if not page.get("ok",false) or not _same(page,draft): return {"ok":false,"code":"StaleVisitMessageQuote"}
	var found: bool=false
	for row in page.get("rows",[])+page.get("offers",[]):
		if row.get("id")==row_id and action in row.get("actions",[]): found=true
	if not found: return {"ok":false,"code":"StaleVisitMessageQuote"}
	var context: Dictionary=host.kernel.command_context()
	if not context.get("ok",false) or context.get("revision")!=source.revision: return {"ok":false,"code":"StalePreview"}
	var quote: Dictionary=host.kernel.preview(action.command,action.params,actor)
	if not quote.get("ok",false): host._message="此时不能办理拜帖："+host._reason(quote.get("code","Unavailable"));host._render();return quote
	if quote.get("revision")!=source.revision: return {"ok":false,"code":"StaleVisitMessageQuote"}
	control_ref.get_ref().disabled=true
	return host._resident_submit_frozen({"kind":action.command,"params":action.params.duplicate(true),"actor_id":actor,"request_seq":context.next_request_seq,"expected_revision":source.revision})
