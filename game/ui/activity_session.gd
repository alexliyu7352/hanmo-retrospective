## 课程与小游戏输入会话：冻结原页面和报价，只转交既有Kernel；不写世界时间、奖励或费用。
extends RefCounted
var _host: WeakRef
var _root: WeakRef
var _identity: Dictionary={}
var _tickets: Dictionary={}
var _next: int=0
var _closed: bool=false

## 会话挂在独立功能块，移出节点树立即失效；普通重绘不会复用旧按钮。
static func create(host, root: Control):
	var session=new()
	session._host=weakref(host); session._root=weakref(root)
	session._identity=session._current_identity(host)
	root.set_meta("activity_session",session)
	root.tree_exiting.connect(session.close)
	return session

## 世界身份包含真实Kernel实例、内容摘要与页面代次；同人物同revision的新世界也不复用旧输入。
func _current_identity(host) -> Dictionary:
	var identity: Dictionary=host.kernel.activity_identity()
	identity.started=host._started
	identity.scene_epoch=host._scene_counter
	identity.kernel_instance=host.kernel.get_instance_id()
	return identity

## 关闭只销毁界面候选，不取消真实行动或重新生成小游戏。
func close() -> void:
	_closed=true; _tickets.clear()

## 显示过的页面、人物、内容与版本须仍相同；拒绝借当前revision重发旧按钮。
func guard() -> Dictionary:
	var host=_host.get_ref(); var root=_root.get_ref()
	if _closed or not is_instance_valid(host) or not is_instance_valid(root) or root.is_queued_for_deletion() or not root.is_inside_tree() or not root.is_visible_in_tree() or not host.is_ancestor_of(root): return {"ok":false,"code":"ActivitySessionClosed"}
	if not host._started or host._advancing or not host._storage_intent.is_empty(): return {"ok":false,"code":"ActorBusy"}
	if _current_identity(host)!=_identity: return {"ok":false,"code":"StaleActivitySession"}
	return {"ok":true}

## 每个按钮保存完整参数与正式报价；失败理由来自规则，不靠按钮文案推断资格。
func command(parent: Control, label: String, kind: String, params: Dictionary, completed: Callable=Callable()) -> Button:
	var host=_host.get_ref(); _next+=1; var id: int=_next
	var quote: Dictionary=host.kernel.preview(kind,params.duplicate(true),_identity.actor_id)
	_tickets[id]={"kind":kind,"params":params.duplicate(true),"quote":quote.duplicate(true),"completed":completed,"used":false}
	var button: Button=host._button(parent,label,kind,params.get("action_id",params.get("quote_id",params.get("skill_id",params.get("unit_id","")))),activate.bind(id),not quote.ok or host._advancing)
	button.set_meta("activity_ticket",id); button.set_meta("activity_params",params.duplicate(true)); button.set_meta("activity_quote",quote.duplicate(true))
	if not quote.ok:
		button.tooltip_text=reason(host,quote)
		host._text(parent,reason(host,quote),15).set_meta("activity_rejection",quote.get("code","Unknown"))
	return button

## 提交前再核原报价；一次点击即消费该界面ticket，失败后须重绘取得新候选。
func activate(id: int) -> Dictionary:
	var valid: Dictionary=guard()
	if not valid.ok: return valid
	if not _tickets.has(id) or _tickets[id].used: return {"ok":false,"code":"ActivityInputAlreadyUsed"}
	var ticket: Dictionary=_tickets[id]; ticket.used=true
	var host=_host.get_ref(); var quote: Dictionary=host.kernel.preview(ticket.kind,ticket.params.duplicate(true),_identity.actor_id)
	if not quote.ok: return _reject(host,quote)
	if JSON.stringify(quote,"",true)!=JSON.stringify(ticket.quote,"",true): return _reject(host,{"ok":false,"code":"StaleActivityQuote"})
	valid=guard()
	if not valid.ok: return _reject(host,valid)
	var context: Dictionary=host.kernel.command_context(_identity.actor_id)
	if not context.ok: return _reject(host,context)
	if host.get("_resident_mode")==true:
		return host._resident_submit_frozen({"kind":ticket.kind,"params":ticket.params.duplicate(true),"actor_id":_identity.actor_id,"expected_revision":_identity.revision,"request_seq":context.next_request_seq},ticket.completed)
	var result: Dictionary=host.kernel.submit({"kind":ticket.kind,"params":ticket.params.duplicate(true),"actor_id":_identity.actor_id,"expected_revision":_identity.revision,"request_seq":context.next_request_seq})
	if result.ok and ticket.completed.is_valid(): ticket.completed.call(result.duplicate(true))
	host._apply_command_result(ticket.kind,result)
	return result

## 延迟拒绝仍显示明确原因；不把旧请求交给会取得新revision的通用_perform。
func _reject(host, result: Dictionary) -> Dictionary:
	host._message=reason(host,result); host._render()
	return result

## 保留原本地化文本及准确规则代码，尚无译文的拒绝也能定位。
static func reason(host, result: Dictionary) -> String:
	var code: String=str(result.get("code","Unknown"))
	return host._reason(code)+"（"+code+"）"
