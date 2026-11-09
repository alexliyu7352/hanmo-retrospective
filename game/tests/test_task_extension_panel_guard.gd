## 仅验证原办理回调的来源、明确确认与一次提交；替身命令不冒真实合同或GUI验收。
extends SceneTree
const ExtensionPanel=preload("res://ui/task_extension_panel.gd")
var checks: int=0
var failures: int=0

class KernelStub extends RefCounted:
	var source: Dictionary={"world_id":"world","execution_epoch":1,"revision":3,"tick":1}
	var actor: String="worker"
	var allowed: bool=true
	func source_identity() -> Dictionary: return source.duplicate(true)
	func command_context() -> Dictionary: return {"ok":true,"actor_id":actor,"revision":source.revision,"next_request_seq":9}
	func preview(_kind: String, _params: Dictionary, _actor: String) -> Dictionary:
		return {"ok":allowed,"revision":source.revision,"code":"" if allowed else "TaskExtensionOriginalDeadlinePassed"}

class HostStub extends Control:
	var kernel=KernelStub.new()
	var _resident_mode: bool=true
	var _scene_counter: int=7
	var _advancing: bool=false
	var _storage_intent: Dictionary={}
	var _message: String=""
	var submitted: Array=[]
	func _render() -> void: pass
	func _reason(code: String) -> String: return code
	func _resident_submit_frozen(command: Dictionary) -> Dictionary:
		submitted.append(command.duplicate(true));return {"ok":true}

func _initialize() -> void: call_deferred("_run")

## 失败单独报具体控制边界；不通过调高等待或吞ERROR获得成功。
func _need(value: bool, reason: String) -> void:
	checks+=1
	if not value: failures+=1;push_error(reason)

func _run() -> void:
	var host=HostStub.new();root.add_child(host)
	_need(ExtensionPanel._reason(host,"TaskExtensionOriginalDeadlinePassed").contains("原约定期限"),"missed original deadline stays explicit")
	_need(ExtensionPanel._reason(host,"TaskExtensionProviderDeclarationRequired").contains("原委托人"),"consent must come from original actual issuer")
	_need(ExtensionPanel._reason(host,"UnknownSourceCorruption")=="UnknownSourceCorruption","unknown cold errors stay visible")
	var button=Button.new();host.add_child(button)
	var source: Dictionary=host.kernel.source_identity()
	var action: Dictionary={"command":"task_request_extension","params":{"task_id":"task:4","policy_id":"extension_policy"},"available":true,"text":"本人接受","summary":"原合同冻结条款"}
	ExtensionPanel._draft(host)
	var result: Dictionary=ExtensionPanel._submit(weakref(host),weakref(button),action,source,"worker",7)
	_need(not result.get("ok",false) and host.submitted.is_empty(),"without explicit review selection no command is submitted")
	ExtensionPanel._choose(weakref(host),weakref(button),action,source,"worker",7)
	_need(host.submitted.is_empty(),"review selection itself never accepts private extension")
	result=ExtensionPanel._submit(weakref(host),weakref(button),action,source,"worker",7)
	_need(result.get("ok",false) and host.submitted.size()==1,"explicit confirmation submits exactly one original command")
	_need(host.submitted[0]=={"kind":"task_request_extension","params":{"task_id":"task:4","policy_id":"extension_policy"},"actor_id":"worker","request_seq":9,"expected_revision":3},"submitted payload cannot edit deadline or original issuer identity")
	result=ExtensionPanel._submit(weakref(host),weakref(button),action,source,"worker",7)
	_need(not result.get("ok",false) and host.submitted.size()==1,"same confirmed button cannot submit twice")
	button.disabled=false;host.kernel.source.execution_epoch=2
	result=ExtensionPanel._submit(weakref(host),weakref(button),action,source,"worker",7)
	_need(not result.get("ok",false) and host.submitted.size()==1,"restore epoch invalidates old confirmation")
	host.kernel.source.execution_epoch=1;host.kernel.actor="employer"
	result=ExtensionPanel._submit(weakref(host),weakref(button),action,source,"worker",7)
	_need(not result.get("ok",false) and host.submitted.size()==1,"changing controlled actor cannot sign another actor consent")
	host.kernel.actor="worker";host._scene_counter=8
	result=ExtensionPanel._submit(weakref(host),weakref(button),action,source,"worker",7)
	_need(not result.get("ok",false) and host.submitted.size()==1,"old closed scene cannot submit")
	host._scene_counter=7;host.kernel.allowed=false
	result=ExtensionPanel._submit(weakref(host),weakref(button),action,source,"worker",7)
	_need(not result.get("ok",false) and host.submitted.size()==1,"fresh original business refusal wins over old review")
	host.kernel.allowed=true;button.hide()
	result=ExtensionPanel._submit(weakref(host),weakref(button),action,source,"worker",7)
	_need(not result.get("ok",false) and host.submitted.size()==1,"hidden old control cannot sign")
	button.show();var dead_ref=weakref(button);button.free()
	result=ExtensionPanel._submit(weakref(host),dead_ref,action,source,"worker",7)
	_need(not result.get("ok",false) and host.submitted.size()==1,"destroyed control is safely ignored")
	var foreign=Button.new();host.add_child(foreign)
	var wrong: Dictionary={"command":"task_shop_order_cancel","params":{"task_id":"task:4"},"available":true}
	ExtensionPanel._choose(weakref(host),weakref(foreign),wrong,source,"worker",7)
	result=ExtensionPanel._submit(weakref(host),weakref(foreign),wrong,source,"worker",7)
	_need(not result.get("ok",false) and host.submitted.size()==1,"extension panel cannot issue another business command")
	host.free();print("private extension panel necessary callback guards: ",checks," checks, ",failures," failures")
	quit(0 if failures==0 else 1)
