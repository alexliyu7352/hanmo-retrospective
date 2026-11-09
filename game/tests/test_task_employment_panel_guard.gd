## 仅验证原办理回调的来源、明确确认与一次提交；替身命令不冒真实合同或GUI验收。
extends SceneTree
const EmploymentPanel=preload("res://ui/task_employment_panel.gd")
var checks: int=0
var failures: int=0

class KernelStub extends RefCounted:
	var source: Dictionary={"world_id":"world","execution_epoch":1,"revision":3,"tick":1}
	var actor: String="worker"
	var allowed: bool=true
	func source_identity() -> Dictionary: return source.duplicate(true)
	func command_context() -> Dictionary: return {"ok":true,"actor_id":actor,"revision":source.revision,"next_request_seq":9}
	func preview(_kind: String, _params: Dictionary, _actor: String) -> Dictionary:
		return {"ok":allowed,"revision":source.revision,"code":"" if allowed else "EmploymentWorkerOccupied"}

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
	var button=Button.new();host.add_child(button)
	var source: Dictionary=host.kernel.source_identity()
	var action: Dictionary={"command":"estate_employment_decide","params":{"contract_id":"employment:1","decision":"accept"},"available":true,"text":"本人接受","summary":"原合同冻结条款"}
	EmploymentPanel._draft(host)
	var result: Dictionary=EmploymentPanel._submit(weakref(host),weakref(button),action,source,"worker",7)
	_need(not result.get("ok",false) and host.submitted.is_empty(),"without explicit review selection no command is submitted")
	EmploymentPanel._choose(weakref(host),weakref(button),action,source,"worker",7)
	_need(host.submitted.is_empty(),"review selection itself never accepts employment")
	result=EmploymentPanel._submit(weakref(host),weakref(button),action,source,"worker",7)
	_need(result.get("ok",false) and host.submitted.size()==1,"explicit confirmation submits exactly one original command")
	_need(host.submitted[0]=={"kind":"estate_employment_decide","params":{"contract_id":"employment:1","decision":"accept"},"actor_id":"worker","request_seq":9,"expected_revision":3},"submitted payload cannot edit wages wallets or worker identity")
	result=EmploymentPanel._submit(weakref(host),weakref(button),action,source,"worker",7)
	_need(not result.get("ok",false) and host.submitted.size()==1,"same confirmed button cannot submit twice")
	button.disabled=false;host.kernel.source.execution_epoch=2
	result=EmploymentPanel._submit(weakref(host),weakref(button),action,source,"worker",7)
	_need(not result.get("ok",false) and host.submitted.size()==1,"restore epoch invalidates old confirmation")
	host.kernel.source.execution_epoch=1;host.kernel.actor="employer"
	result=EmploymentPanel._submit(weakref(host),weakref(button),action,source,"worker",7)
	_need(not result.get("ok",false) and host.submitted.size()==1,"changing controlled actor cannot sign old worker consent")
	host.kernel.actor="worker";host._scene_counter=8
	result=EmploymentPanel._submit(weakref(host),weakref(button),action,source,"worker",7)
	_need(not result.get("ok",false) and host.submitted.size()==1,"old closed scene cannot submit")
	host._scene_counter=7;host.kernel.allowed=false
	result=EmploymentPanel._submit(weakref(host),weakref(button),action,source,"worker",7)
	_need(not result.get("ok",false) and host.submitted.size()==1,"fresh original business refusal wins over old review")
	host.kernel.allowed=true;button.hide()
	result=EmploymentPanel._submit(weakref(host),weakref(button),action,source,"worker",7)
	_need(not result.get("ok",false) and host.submitted.size()==1,"hidden old control cannot sign")
	button.show();var dead_ref=weakref(button);button.free()
	result=EmploymentPanel._submit(weakref(host),dead_ref,action,source,"worker",7)
	_need(not result.get("ok",false) and host.submitted.size()==1,"destroyed control is safely ignored")
	var request_button=Button.new();host.add_child(request_button)
	var request: Dictionary={"command":"estate_employment_request_offer","params":{"policy_id":"farm_job"},"available":true,"text":"询问雇主","summary":"原作者邀约，尚未本人接受"}
	EmploymentPanel._choose(weakref(host),weakref(request_button),request,source,"worker",7)
	_need(host.submitted.size()==1,"asking NPC employer still requires explicit confirmation")
	result=EmploymentPanel._submit(weakref(host),weakref(request_button),request,source,"worker",7)
	_need(result.get("ok",false) and host.submitted.size()==2 and host.submitted[1].kind=="estate_employment_request_offer" and host.submitted[1].params=={"policy_id":"farm_job"},"confirmed inquiry submits only original policy, never acceptance or employer money fields")
	host.free();print("employment panel necessary callback guards: ",checks," checks, ",failures," failures")
	quit(0 if failures==0 else 1)
