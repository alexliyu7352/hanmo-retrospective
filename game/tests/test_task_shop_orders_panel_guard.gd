## 仅验证原办理回调的来源、明确确认与一次提交；替身命令不冒真实合同或GUI验收。
extends SceneTree
const ShopOrdersPanel=preload("res://ui/task_shop_orders_panel.gd")
var checks: int=0
var failures: int=0

class KernelStub extends RefCounted:
	var source: Dictionary={"world_id":"world","execution_epoch":1,"revision":3,"tick":1}
	var actor: String="worker"
	var allowed: bool=true
	func source_identity() -> Dictionary: return source.duplicate(true)
	func command_context() -> Dictionary: return {"ok":true,"actor_id":actor,"revision":source.revision,"next_request_seq":9}
	func preview(_kind: String, _params: Dictionary, _actor: String) -> Dictionary:
		return {"ok":allowed,"revision":source.revision,"code":"" if allowed else "TaskShopOrderActionStillActive"}

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
	_need(ShopOrdersPanel._reason(host,"TaskShopOrderStockMissing").contains("原店库存"),"missing shop goods gives actual restock instruction")
	_need(ShopOrdersPanel._reason(host,"ProviderDeclarationRequired").contains("原客户"),"declaration failure points to actual original customer")
	_need(ShopOrdersPanel._reason(host,"UnknownSourceCorruption")=="UnknownSourceCorruption","unknown source error stays visible and never becomes success")
	var button=Button.new();host.add_child(button)
	var source: Dictionary=host.kernel.source_identity()
	var action: Dictionary={"command":"task_shop_order_accept","params":{"order_id":"private_shop_order:1"},"available":true,"text":"本人接受","summary":"原合同冻结条款"}
	ShopOrdersPanel._draft(host)
	var result: Dictionary=ShopOrdersPanel._submit(weakref(host),weakref(button),action,source,"worker",7)
	_need(not result.get("ok",false) and host.submitted.is_empty(),"without explicit review selection no command is submitted")
	ShopOrdersPanel._choose(weakref(host),weakref(button),action,source,"worker",7)
	_need(host.submitted.is_empty(),"review selection itself never accepts shop order")
	result=ShopOrdersPanel._submit(weakref(host),weakref(button),action,source,"worker",7)
	_need(result.get("ok",false) and host.submitted.size()==1,"explicit confirmation submits exactly one original command")
	_need(host.submitted[0]=={"kind":"task_shop_order_accept","params":{"order_id":"private_shop_order:1"},"actor_id":"worker","request_seq":9,"expected_revision":3},"submitted payload cannot edit prices wallets or proprietor identity")
	result=ShopOrdersPanel._submit(weakref(host),weakref(button),action,source,"worker",7)
	_need(not result.get("ok",false) and host.submitted.size()==1,"same confirmed button cannot submit twice")
	button.disabled=false;host.kernel.source.execution_epoch=2
	result=ShopOrdersPanel._submit(weakref(host),weakref(button),action,source,"worker",7)
	_need(not result.get("ok",false) and host.submitted.size()==1,"restore epoch invalidates old confirmation")
	host.kernel.source.execution_epoch=1;host.kernel.actor="employer"
	result=ShopOrdersPanel._submit(weakref(host),weakref(button),action,source,"worker",7)
	_need(not result.get("ok",false) and host.submitted.size()==1,"changing controlled actor cannot sign old worker consent")
	host.kernel.actor="worker";host._scene_counter=8
	result=ShopOrdersPanel._submit(weakref(host),weakref(button),action,source,"worker",7)
	_need(not result.get("ok",false) and host.submitted.size()==1,"old closed scene cannot submit")
	host._scene_counter=7;host.kernel.allowed=false
	result=ShopOrdersPanel._submit(weakref(host),weakref(button),action,source,"worker",7)
	_need(not result.get("ok",false) and host.submitted.size()==1,"fresh original business refusal wins over old review")
	host.kernel.allowed=true;button.hide()
	result=ShopOrdersPanel._submit(weakref(host),weakref(button),action,source,"worker",7)
	_need(not result.get("ok",false) and host.submitted.size()==1,"hidden old control cannot sign")
	button.show();var dead_ref=weakref(button);button.free()
	result=ShopOrdersPanel._submit(weakref(host),dead_ref,action,source,"worker",7)
	_need(not result.get("ok",false) and host.submitted.size()==1,"destroyed control is safely ignored")
	_need(ShopOrdersPanel.show_order(host,"private_shop_order:8",source,"worker"),"same current actor can select original order for review")
	_need(host.get_meta(ShopOrdersPanel.META).order_id=="private_shop_order:8" and host.submitted.size()==1,"opening original order never accepts it")
	var stale: Dictionary=source.duplicate(true);stale.revision+=1
	_need(not ShopOrdersPanel.show_order(host,"private_shop_order:9",stale,"worker"),"stale shop selection cannot replace current review")
	_need(not ShopOrdersPanel.show_order(host,"private_shop_order:9",source,"another_actor"),"foreign actor cannot open proprietor order")
	for command in ["task_shop_order_deliver","task_shop_order_cancel"]:
		var task_button=Button.new();host.add_child(task_button)
		var request: Dictionary={"command":command,"params":{"task_id":"task:4"},"available":true,"text":"本人办理","summary":"原店单条款"}
		ShopOrdersPanel._choose(weakref(host),weakref(task_button),request,source,"worker",7)
		result=ShopOrdersPanel._submit(weakref(host),weakref(task_button),request,source,"worker",7)
		_need(result.get("ok",false) and host.submitted.back().kind==command and host.submitted.back().params=={"task_id":"task:4"},"formal delivery or cancellation submits only original Task ID")
	host.free();print("shop order panel necessary callback guards: ",checks," checks, ",failures," failures")
	quit(0 if failures==0 else 1)
