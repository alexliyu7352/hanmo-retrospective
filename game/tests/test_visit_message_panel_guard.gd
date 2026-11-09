## 拜帖本人确认与旧页面失效；替身只验回调，不冒正式World送达。
extends SceneTree
const Panel=preload("res://ui/visit_message_panel.gd")
var checks: int=0
var failures: int=0
class KernelStub extends RefCounted:
	var source: Dictionary={"world_id":"world","execution_epoch":1,"revision":3,"tick":2}
	var actor: String="sender"
	var allowed: bool=true
	var action: Dictionary={"command":"leave_visit_message","params":{"policy_id":"visit"},"available":true,"code":"","text":"确认托留"}
	func source_identity() -> Dictionary: return source.duplicate(true)
	func command_context() -> Dictionary: return {"ok":true,"actor_id":actor,"revision":source.revision,"next_request_seq":9}
	func demand_view(_query: String,_args: Dictionary) -> Dictionary:
		return {"ok":true,"source_identity":source.duplicate(true),"observer_actor_id":actor,"rows":[],"offers":[{"id":"visit","actions":[action.duplicate(true)] if allowed else []}]}
	func preview(_kind: String,_params: Dictionary,_actor: String) -> Dictionary: return {"ok":allowed,"revision":source.revision}
class HostStub extends Control:
	var kernel=KernelStub.new()
	var _scene_counter: int=7
	var _advancing: bool=false
	var _resident_mode: bool=true
	var _storage_intent: Dictionary={}
	var _message: String=""
	var submitted: Array=[]
	func _render() -> void: pass
	func _reason(code: String) -> String: return code
	func _resident_submit_frozen(command: Dictionary) -> Dictionary: submitted.append(command.duplicate(true));return {"ok":true}
func _initialize() -> void: call_deferred("_run")
func _need(value: bool,reason: String) -> void:
	checks+=1
	if not value: failures+=1;push_error(reason)
func _run() -> void:
	var host=HostStub.new();root.add_child(host);var button=Button.new();host.add_child(button)
	var source: Dictionary=host.kernel.source_identity();var action: Dictionary=host.kernel.action.duplicate(true)
	Panel._draft(host)
	var result: Dictionary=Panel._confirm(weakref(host),weakref(button),"visit",action,source,"sender",7)
	_need(not result.get("ok",false) and host.submitted.is_empty(),"no personal review no message")
	Panel._choose(weakref(host),weakref(button),"visit",source,"sender",7);_need(host.submitted.is_empty(),"review does not deposit")
	host.kernel.allowed=false;result=Panel._confirm(weakref(host),weakref(button),"visit",action,source,"sender",7)
	_need(not result.get("ok",false) and host.submitted.is_empty(),"changed original custodian source rejects")
	host.kernel.allowed=true;result=Panel._confirm(weakref(host),weakref(button),"visit",action,source,"sender",7)
	_need(result.get("ok",false) and host.submitted.size()==1,"same-source explicit confirmation submits once")
	_need(host.submitted[0].kind=="leave_visit_message" and host.submitted[0].params=={"policy_id":"visit"},"only original typed policy id, no invented facts or listener")
	result=Panel._confirm(weakref(host),weakref(button),"visit",action,source,"sender",7);_need(not result.get("ok",false) and host.submitted.size()==1,"consumed button cannot post twice")
	button.disabled=false;host.kernel.source.execution_epoch=2;result=Panel._confirm(weakref(host),weakref(button),"visit",action,source,"sender",7);_need(not result.get("ok",false),"restored world rejects old source")
	host.kernel.source.execution_epoch=1;host.kernel.actor="recipient";result=Panel._confirm(weakref(host),weakref(button),"visit",action,source,"sender",7);_need(not result.get("ok",false),"controller change cannot use sender permission")
	host.kernel.actor="sender";host._scene_counter=8;result=Panel._confirm(weakref(host),weakref(button),"visit",action,source,"sender",7);_need(not result.get("ok",false),"closed page callback rejects")
	host._scene_counter=7;button.hide();result=Panel._confirm(weakref(host),weakref(button),"visit",action,source,"sender",7);_need(not result.get("ok",false),"hidden control rejects")
	host.free();print("visit message callback guards: ",checks," checks, ",failures," failures");quit(0 if failures==0 else 1)
