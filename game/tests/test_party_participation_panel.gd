## 随行控件只核来源、异步就绪与真实释放后的回调；不模拟Party规则或正式World。
extends SceneTree
const PartyPanel=preload("res://ui/party_participation_panel.gd")
var checks: int=0

## 有限回执夹具只统计原报价与请求，不产生人物、队伍或世界时钟。
class Kernel extends RefCounted:
	var revision: int=7
	var epoch: int=2
	var mode: String="invitation"
	var read_code: String=""
	var quote_code: String=""
	var quotes: int=0
	## 完整世界身份包含执行epoch，旧读档按钮不能复用。
	func source_identity() -> Dictionary: return {"world_id":"fixture","execution_epoch":epoch,"revision":revision,"tick":4}
	## 动作码不同只切换请求，不伪造成员状态。
	func demand_view(_query: String,_args: Dictionary) -> Dictionary:
		if not read_code.is_empty(): return {"ok":false,"code":read_code}
		var action: Dictionary={"command":"party_accept" if mode=="invitation" else "party_leave","params":{"invitation_id":"invite"} if mode=="invitation" else {},"text":"接受随行邀请" if mode=="invitation" else "离开这支队伍","available":true}
		var actions: Array=[action]
		if mode=="membership":
			actions.append({"command":"wait","params":{},"text":"随队等候半日","available":true})
			actions.append({"command":"end_interaction","params":{},"text":"结束这次交谈","available":true})
		return {"ok":true,"source_identity":source_identity(),"observer_actor_id":"player","rows":[] if mode=="empty" else [{"kind":mode,"display_name":"行商","text":"当面提出","terms_text":"原约六钱","actions":actions}]}
	## 仅记录是否发起原报价；普通规则拒绝必须保持可见。
	func preview(_kind: String,_params: Dictionary,_actor: String) -> Dictionary:
		quotes+=1
		return {"ok":quote_code.is_empty(),"code":quote_code,"revision":revision}
	## 请求序号仍由正式命令上下文提供。
	func command_context() -> Dictionary: return {"ok":true,"actor_id":"player","revision":revision,"next_request_seq":8}

## 真实Control宿主沿正式按钮/提交口形状，不写任何第二业务实现。
class Host extends Control:
	var kernel=Kernel.new()
	var _resident_mode: bool=true
	var _advancing: bool=false
	var _storage_intent: Dictionary={}
	var _scene_counter: int=1
	var _message: String=""
	var requests: Array[Dictionary]=[]
	var paper: VBoxContainer
	## 保原拒绝码以核错误没有被吞成空页面。
	func _reason(code: String) -> String: return "原因："+code
	## 实际Label承载可读反馈。
	func _text(parent: Node,text: String,_size: int) -> Label:
		var label=Label.new();label.text=text;parent.add_child(label);return label
	## 实际Button持与Main相同的pressed连线。
	func _button(parent: Node,text: String,_kind: String,_target: String,callback: Callable,disabled: bool=false,_auto: bool=true) -> Button:
		var button=Button.new();button.text=text;button.disabled=disabled;parent.add_child(button);button.pressed.connect(callback);return button
	## 重绘移除旧控件，让原保存Callable真正面对释放对象。
	func _render() -> void:
		_scene_counter+=1
		if is_instance_valid(paper): remove_child(paper);paper.queue_free()
		paper=VBoxContainer.new();add_child(paper);PartyPanel.build(self,paper)
	## 唯一提交口只留下请求原文供核身份和参数。
	func _resident_submit_frozen(request: Dictionary) -> Dictionary:
		requests.append(request.duplicate(true));return {"ok":true,"pending":true}

## 逐项失败直接退出，日志中的引擎ERROR另由运行包装核零。
func _check(value: bool, label: String) -> void:
	checks+=1
	if not value: push_error(label);quit(1);assert(value,label)

## 按真实控件树找业务Button，不把页文本当一次输入。
func _buttons(node: Node) -> Array:
	var result: Array=[]
	if node is Button and node.has_meta("party_action"): result.append(node)
	for child in node.get_children(): result.append_array(_buttons(child))
	return result

## 保存产品原绑定Callable，以核已free的host和button都安全拒绝。
func _request_callback(button: Button) -> Callable:
	for connection in button.pressed.get_connections():
		if connection.callable.get_method()=="_request": return connection.callable
	return Callable()

## 第一帧后实际点击，准备和渲染本身必须零提交。
func _initialize() -> void: call_deferred("_run")

## 同源确认、异步页、重复点击和释放旧回调是本门的完整范围。
func _run() -> void:
	var host=Host.new();root.add_child(host)
	host.kernel.read_code="DanglingParty";_check(PartyPanel.visible_here(host),"real query failure must remain visible");host._render();await process_frame
	_check(host.paper.get_child_count()==1 and host.paper.get_child(0).text.contains("DanglingParty") and host.requests.is_empty(),"query failure rendered with original reason and zero commands")
	host.kernel.read_code="PartyInvitationsUnavailable";_check(not PartyPanel.visible_here(host),"disabled old profile has no empty error section")
	host.kernel.read_code="NativeAsyncRequired";_check(not PartyPanel.prepare(host),"page not ready")
	host.kernel.read_code="";host.kernel.quote_code="NativeAsyncRequired";_check(not PartyPanel.prepare(host),"quote not ready")
	host.kernel.quote_code="ActorBusy";_check(PartyPanel.prepare(host),"rule rejection remains renderable");host._render();await process_frame
	_check(_buttons(host.paper).size()==1 and _buttons(host.paper)[0].disabled,"rule rejected button visible disabled")
	_check(host.requests.is_empty(),"prepare render do not submit")
	host.kernel.quote_code="";_check(PartyPanel.prepare(host),"all quotes ready");_check(PartyPanel.visible_here(host),"same source invitation has visible section");host._render();await process_frame
	var button: Button=_buttons(host.paper)[0];var call: Callable=_request_callback(button);_check(call.is_valid(),"original request callable captured")
	host.kernel.epoch+=1;_check(call.call().code=="StalePreview" and host.requests.is_empty(),"new epoch cannot submit old invitation")
	host.kernel.epoch-=1;button.pressed.emit();_check(host.requests.size()==1,"explicit accept submits once")
	_check(host.requests[0].kind=="party_accept" and host.requests[0].params=={"invitation_id":"invite"} and host.requests[0].expected_revision==7,"original bound command and revision")
	_check(not call.call().ok and host.requests.size()==1,"disabled duplicate cannot submit")
	host._render();await process_frame;button=_buttons(host.paper)[0];call=_request_callback(button)
	host._render();await process_frame;_check(not is_instance_valid(button),"previous button truly freed")
	_check(call.call().code=="StaleSceneRequest" and host.requests.size()==1,"freed button callback safe")
	host.kernel.mode="membership";host._render();await process_frame;button=_buttons(host.paper)[0];_check(button.text=="离开这支队伍","real membership offers own leave");_check(_buttons(host.paper).size()==3,"membership also shows original wait and conversation close")
	button.pressed.emit();_check(host.requests.size()==2 and host.requests[1].kind=="party_leave" and host.requests[1].params.is_empty(),"leave does not select another member")
	host._render();await process_frame;call=_request_callback(_buttons(host.paper)[0]);host.free();_check(call.call().code=="StaleSceneRequest","freed host callback safe")
	var empty=Host.new();root.add_child(empty);empty.kernel.mode="empty";empty._render();await process_frame;_check(_buttons(empty.paper).is_empty() and empty.requests.is_empty() and not PartyPanel.visible_here(empty),"no unseen invitation fallback or empty section");empty.free()
	print("party_participation_panel: checks=%d failures=0; controls only, no World"%checks);quit(0)
