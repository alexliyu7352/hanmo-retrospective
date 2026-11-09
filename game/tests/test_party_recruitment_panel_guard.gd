## 本人确认与对方原答复分离的必要回调门；替身不是完整World或画面验收。
extends SceneTree
const Recruit=preload("res://ui/party_recruitment_panel.gd")
var checks: int=0
var failures: int=0
class KernelStub extends RefCounted:
	var source: Dictionary={"world_id":"world","execution_epoch":1,"revision":3,"tick":2}
	var actor: String="leader"
	var decision: String="accept"
	var cost: int=6
	var action: Dictionary={"command":"join_party","params":{"actor_id":"companion","context":{"place_id":"yard","provider_actor_id":"companion","role_id":"guide"}},"available":true}
	func source_identity() -> Dictionary: return source.duplicate(true)
	func command_context() -> Dictionary: return {"ok":true,"actor_id":actor,"revision":source.revision,"next_request_seq":9}
	func demand_view(_query: String, _args: Dictionary) -> Dictionary:
		return {"ok":true,"source_identity":source.duplicate(true),"observer_actor_id":actor,"rows":[{"declaration_id":"yes","decision":decision,"cost_per_tick":cost,"actions":[action.duplicate(true)]}]}
	func preview(_kind: String, _params: Dictionary, _actor: String) -> Dictionary: return {"ok":true,"revision":source.revision,"cost_per_tick":cost}
class HostStub extends Control:
	var kernel=KernelStub.new()
	var _scene_counter: int=7
	var _advancing: bool=false
	var _storage_intent: Dictionary={}
	var _message: String=""
	var submitted: Array=[]
	func _render() -> void: pass
	func _reason(code: String) -> String: return code
	func _resident_submit_frozen(command: Dictionary) -> Dictionary: submitted.append(command.duplicate(true));return {"ok":true}
func _initialize() -> void: call_deferred("_run")
func _need(value: bool, reason: String) -> void:
	checks+=1
	if not value: failures+=1;push_error(reason)
func _run() -> void:
	var host=HostStub.new();root.add_child(host);var button=Button.new();host.add_child(button)
	var source: Dictionary=host.kernel.source_identity();var action: Dictionary=host.kernel.action.duplicate(true)
	Recruit._draft(host)
	var result: Dictionary=Recruit._confirm(weakref(host),weakref(button),"yes",action,6,source,"leader",7)
	_need(not result.get("ok",false) and host.submitted.is_empty(),"without personal review no joining")
	Recruit._choose(weakref(host),weakref(button),"yes",source,"leader",7);_need(host.submitted.is_empty(),"review is not command or NPC consent")
	host.kernel.decision="decline";result=Recruit._confirm(weakref(host),weakref(button),"yes",action,6,source,"leader",7)
	_need(not result.get("ok",false) and host.submitted.is_empty(),"changed NPC refusal cannot be overridden by old yes button")
	host.kernel.decision="accept";host.kernel.cost=8;result=Recruit._confirm(weakref(host),weakref(button),"yes",action,6,source,"leader",7)
	_need(not result.get("ok",false) and host.submitted.is_empty(),"changed fee requires new review")
	host.kernel.cost=6;result=Recruit._confirm(weakref(host),weakref(button),"yes",action,6,source,"leader",7)
	_need(result.get("ok",false) and host.submitted.size()==1,"explicit same-source review submits original join once")
	_need(host.submitted[0].kind=="join_party" and host.submitted[0].params==action.params and not host.submitted[0].params.has("decision"),"original actor/context only, no NPC choice or fee setter")
	result=Recruit._confirm(weakref(host),weakref(button),"yes",action,6,source,"leader",7);_need(not result.get("ok",false) and host.submitted.size()==1,"same confirmed control cannot join twice")
	button.disabled=false;host.kernel.source.execution_epoch=2;result=Recruit._confirm(weakref(host),weakref(button),"yes",action,6,source,"leader",7);_need(not result.get("ok",false),"restore invalidates old willingness")
	host.kernel.source.execution_epoch=1;host.kernel.actor="other";result=Recruit._confirm(weakref(host),weakref(button),"yes",action,6,source,"leader",7);_need(not result.get("ok",false),"new controller cannot use old leader consent")
	host.kernel.actor="leader";host._scene_counter=8;result=Recruit._confirm(weakref(host),weakref(button),"yes",action,6,source,"leader",7);_need(not result.get("ok",false),"closed conversation control invalid")
	host._scene_counter=7;button.hide();result=Recruit._confirm(weakref(host),weakref(button),"yes",action,6,source,"leader",7);_need(not result.get("ok",false),"hidden old control cannot join")
	host.free();print("party recruitment callback guards: ",checks," checks, ",failures," failures");quit(0 if failures==0 else 1)
