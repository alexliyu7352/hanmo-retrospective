## 三个真实面板的有限控件门：真实Button信号与host/kernel替身，不冒Native事务或整场景试玩。
extends SceneTree

const PANELS=[
	preload("res://ui/task_personal_acceptances_panel.gd"),
	preload("res://ui/task_personal_messages_panel.gd"),
	preload("res://ui/task_personal_reports_panel.gd"),
]
const COMMANDS=["task_personal_message_accept","task_personal_message_deliver","task_personal_message_report"]
const LABELS=["接下送信委托（半日）","当面交信（半日）","返报并结酬（半日）"]
var checks: int=0
var failures: int=0

class KernelMock extends RefCounted:
	var source: Dictionary={"world_id":"original","execution_epoch":1,"revision":7,"tick":3}
	var actor: String="carrier"
	var sequence: int=12
	var context_ok: bool=true
	var context_revision: int=-1
	var quote_revision: int=-1
	var allowed: bool=true
	var rejection: String="TaskDeadlineExpired"
	var action: Dictionary={}
	var preview_calls: int=0
	var during_preview: Callable=Callable()
	var terms: String="委托预算：7；原报酬：11；原期限：20半日。"
	func source_identity() -> Dictionary: return source.duplicate(true)
	func command_context() -> Dictionary:
		return {"ok":context_ok,"actor_id":actor,"revision":source.revision if context_revision<0 else context_revision,"next_request_seq":sequence}
	func demand_view(_query: String, _args: Dictionary) -> Dictionary:
		return {"ok":true,"enabled":true,"source_identity":source.duplicate(true),"observer_actor_id":actor,
			"rows":[{"id":"original-task","display_name":"送给秦伯的信","status_text":"本人待办理","terms_text":terms,"actions":[action.duplicate(true)]}]}
	func preview(_kind: String, _params: Dictionary, _actor: String) -> Dictionary:
		preview_calls+=1
		var reply: Dictionary={"ok":allowed,"revision":source.revision if quote_revision<0 else quote_revision,"code":"" if allowed else rejection}
		var hook: Callable=during_preview
		during_preview=Callable()
		if hook.is_valid(): hook.call()
		return reply

class HostMock extends Control:
	var kernel=KernelMock.new()
	var _resident_mode: bool=true
	var _scene_counter: int=5
	var _advancing: bool=false
	var _storage_intent: Dictionary={}
	var _message: String=""
	var submitted: Array=[]
	var renders: int=0
	func _reason(code: String) -> String: return code
	func _render() -> void:
		renders+=1
		_scene_counter+=1
	func _text(parent: Control, text: String, _size: int) -> void:
		var label=Label.new();label.text=text;parent.add_child(label)
	func _button(parent: Control, text: String, _kind: String, _id: String, callback: Callable, disabled: bool=false, _primary: bool=false) -> Button:
		var button=Button.new();button.text=text;button.disabled=disabled;parent.add_child(button)
		button.pressed.connect(callback)
		return button
	func _resident_submit_frozen(command: Dictionary) -> Dictionary:
		if _advancing or not _storage_intent.is_empty(): return {"ok":false,"code":"ActorBusy"}
		submitted.append(command.duplicate(true));_advancing=true
		return {"ok":true,"pending":true}

func _initialize() -> void: call_deferred("_run")

## 每条失败报实际边界；断言数量只代表本控件门，不能代替真实款物办理。
func _need(ok: bool, reason: String) -> void:
	checks+=1
	if not ok: failures+=1;push_error(reason)

func _buttons(node: Node) -> Array:
	var result: Array=[]
	if node is Button: result.append(node)
	for child in node.get_children(): result.append_array(_buttons(child))
	return result

func _labels(node: Node) -> Array:
	var result: Array=[]
	if node is Label: result.append(node.text)
	for child in node.get_children(): result.append_array(_labels(child))
	return result

## 必须由产品build建立控件及pressed绑定，不能直接调用提交函数代替正常一击路径。
func _case(index: int) -> Dictionary:
	var host=HostMock.new();root.add_child(host)
	host.kernel.action={"command":COMMANDS[index],"params":{"offer_id":"original-offer"} if index==0 else {"task_id":"original-task"},"available":true,"summary":"双方实际会面，办理一半日。","text":"原办理"}
	PANELS[index].build(host,host)
	var buttons: Array=_buttons(host)
	_need(buttons.size()==1,"panel %d has exactly one explicit business button, no duplicate review/confirm" % index)
	return {"host":host,"button":buttons[0] if not buttons.is_empty() else null,"index":index}

func _press(case: Dictionary) -> void:
	if is_instance_valid(case.button): case.button.pressed.emit()

## 按钮建好后来源/忙锁改变：即使人为发出disabled信号也不许再提交旧请求。
func _before_click_guards(index: int) -> void:
	for boundary in ["busy","storage","scene","actor","revision","epoch","tick","hidden","queued","context","context_revision","sequence"]:
		var c: Dictionary=_case(index);var host=c.host;var button=c.button
		match boundary:
			"busy": host._advancing=true
			"storage": host._storage_intent={"kind":"load"}
			"scene": host._scene_counter+=1
			"actor": host.kernel.actor="other"
			"revision": host.kernel.source.revision+=1
			"epoch": host.kernel.source.execution_epoch+=1
			"tick": host.kernel.source.tick+=1
			"hidden": button.hide()
			"queued": button.queue_free()
			"context": host.kernel.context_ok=false
			"context_revision": host.kernel.context_revision=99
			"sequence": host.kernel.sequence=0
		_press(c)
		_need(host.submitted.is_empty(),"panel %d rejects %s before click" % [index,boundary])
		host.free()

## preview不是授权缓存：本次调用期间换人/换世界/取消页面必须使刚取回的报价失效。
func _during_preview_guards(index: int) -> void:
	for boundary in ["busy","storage","scene","actor","revision","epoch","tick","hidden","queued_button","queued_host","context","context_revision","sequence"]:
		var c: Dictionary=_case(index);var host=c.host;var button=c.button
		host.kernel.during_preview=func():
			match boundary:
				"busy": host._advancing=true
				"storage": host._storage_intent={"kind":"cancelled-scene"}
				"scene": host._scene_counter+=1
				"actor": host.kernel.actor="other"
				"revision": host.kernel.source.revision+=1
				"epoch": host.kernel.source.execution_epoch+=1
				"tick": host.kernel.source.tick+=1
				"hidden": button.hide()
				"queued_button": button.queue_free()
				"queued_host": host.queue_free()
				"context": host.kernel.context_ok=false
				"context_revision": host.kernel.context_revision=99
				"sequence": host.kernel.sequence+=1
		_press(c)
		_need(host.submitted.is_empty(),"panel %d rejects %s during original preview" % [index,boundary])
		host.free()

func _one_panel(index: int) -> void:
	var c: Dictionary=_case(index);var host=c.host;var button=c.button
	if not is_instance_valid(button): host.free();return
	_need(button.text==LABELS[index],"panel %d names the actual personal decision" % index)
	_need(host.submitted.is_empty(),"panel construction never accepts a task or changes the world")
	var shown: Array=_labels(host)
	_need(shown.has(host.kernel.terms),"budget, reward and original deadline remain visible before clicking")
	_need(shown.has(host.kernel.action.summary),"actual half-day and meeting terms remain visible without review step")
	_press(c)
	_need(host.submitted.size()==1,"one actual pressed signal submits once")
	_need(host.submitted[0]=={"kind":COMMANDS[index],"params":host.kernel.action.params,"actor_id":"carrier","request_seq":12,"expected_revision":7},"original actor/params/request/revision stay frozen")
	_need(button.disabled and host._advancing,"button and original host busy lock engage immediately")
	_press(c)
	_need(host.submitted.size()==1,"double click cannot duplicate the command")
	host._advancing=false
	_press(c)
	_need(host.submitted.size()==1,"same disabled control cannot resubmit after busy is cleared")
	host.free()

	c=_case(index);host=c.host;button=c.button
	host.kernel.during_preview=func(): button.pressed.emit()
	_press(c)
	_need(host.submitted.size()==1,"reentrant pressed during preview still submits exactly once")
	host.free()

	for rejection in ["TaskDeadlineExpired","TaskCancelled","NativeAsyncRequired"]:
		c=_case(index);host=c.host;host.kernel.allowed=false;host.kernel.rejection=rejection
		_press(c)
		_need(host.submitted.is_empty() and host._message.contains(rejection),"fresh %s is shown without submitting" % rejection)
		_need(host.renders==1,"new refusal refreshes the existing page once")
		host.free()

	c=_case(index);host=c.host;host.kernel.quote_revision=99
	_press(c);_need(host.submitted.is_empty(),"wrong quote revision cannot sign stale terms");host.free()

	c=_case(index);host=c.host;button=c.button
	var connections: Array=button.pressed.get_connections()
	var callback: Callable=connections.back().callable
	button.free();callback.call()
	_need(host.submitted.is_empty(),"callback retained after actual button destruction stays inert")
	host.free()

	c=_case(index);host=c.host;button=c.button
	connections=button.pressed.get_connections();callback=connections.back().callable
	var kernel=host.kernel;host.free();callback.call()
	_need(kernel.preview_calls==1,"callback retained after actual host destruction does not even preview")
	_before_click_guards(index)
	_during_preview_guards(index)

func _run() -> void:
	var version: Dictionary=Engine.get_version_info()
	if int(version.major)!=4 or int(version.minor)!=7 or int(version.patch)!=2:
		push_error("This control gate requires the fixed Godot 4.7.2 engine; another version is not acceptance evidence.")
		quit(1);return
	for index in range(PANELS.size()): _one_panel(index)
	print("personal message single-action controls: ",checks," checks, ",failures," failures; Native and full-scene execution deferred")
	if failures==0: print("HANMO_TASK_PANEL_MOCK_PASS")
	quit(0 if failures==0 else 1)
