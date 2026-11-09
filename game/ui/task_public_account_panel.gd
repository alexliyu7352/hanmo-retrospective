## 当前本人动态公账委托的履责状态；办理按钮仍从原任务入口取权，不在此补造支付动作。
extends RefCounted
const QUERY="task_public_account_execution"
const META="task_public_account_page"

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
	return page.get("ok",false) and page.get("source_identity")==draft.source and (not page.get("rows",[]).is_empty() or page.has("next_cursor") or draft.cursor>0)

static func build(host, parent: Control) -> void:
	if not host._resident_mode: return
	var draft: Dictionary=_draft(host)
	var page: Dictionary=host.kernel.demand_view(QUERY,{"cursor":draft.cursor})
	if not page.get("ok",false):
		if page.get("code") not in ["TaskTakeoverExecutionProfileRequired","NativeAsyncRequired"]: host._text(parent,"公账履责状态暂不能查看："+host._reason(page.get("code","Unknown")),15)
		return
	if page.get("source_identity")!=draft.source or page.get("observer_actor_id")!=draft.actor_id: return
	for row in page.get("rows",[]):
		var box=VBoxContainer.new();box.name="TaskPublicExecutionRow";parent.add_child(box)
		var executable: bool=row.get("may_progress",false) and row.get("may_settle",false)
		host._text(box,row.get("display_name","")+" · "+("已有当前履责许可" if executable else "原责任待合法办理"),17)
		host._text(box,row.get("explanation",""),14)
		if not executable: host._text(box,"暂停原因："+host._reason(row.get("blocked_code","TaskPoliticalExecutionSuspended")),15)
		if row.get("task_status","") in ["success","cancelled","failed"]: host._text(box,"原任务已经按其实际结果清算。",14)
	if draft.cursor>0: _button(host,parent,"返回首批",0,draft)
	if page.has("next_cursor"): _button(host,parent,"更多公账委托",int(page.next_cursor),draft)

## 翻页只改本人当前画面；来源改变、关闭或控件销毁后的回调全部失效。
static func _button(host, parent: Control, text: String, cursor: int, draft: Dictionary) -> void:
	var button: Button=host._button(parent,text,"task_public_account_page","",func(): pass,host._advancing)
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
