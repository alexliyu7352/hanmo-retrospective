## 本人原文书跳转与二次确认边界；替身回调不冒Native清算通过。
extends SceneTree
const EstatePanel=preload("res://ui/private_shop_liquidations_panel.gd")
var checks: int=0
var failures: int=0
class KernelStub extends RefCounted:
	var source: Dictionary={"world_id":"world","execution_epoch":1,"revision":3,"tick":8}
	var actor: String="executor"
	var allowed: bool=true
	var async_page: bool=false
	var page_actor: String="executor"
	func demand_view(_kind: String,_args: Dictionary) -> Dictionary:
		return {"ok":not async_page,"code":"NativeAsyncRequired" if async_page else "","source_identity":source.duplicate(true),"observer_actor_id":page_actor,"rows":[]}
	var terms: Dictionary={"returned_cash":120,"return_wallet_id":"dead.wallet","return_inventory_id":"dead.inventory"}
	func source_identity() -> Dictionary: return source.duplicate(true)
	func command_context() -> Dictionary: return {"ok":true,"actor_id":actor,"revision":source.revision,"next_request_seq":9}
	func preview(_kind: String,_params: Dictionary,_actor: String) -> Dictionary:
		return {"ok":allowed,"revision":source.revision,"terms":terms.duplicate(true),"code":"" if allowed else "PrivateShopOrderOutstanding"}
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
func _need(value: bool,reason: String) -> void:
	checks+=1
	if not value: failures+=1;push_error(reason)
func _run() -> void:
	var host=HostStub.new();root.add_child(host)
	var button=Button.new();host.add_child(button)
	var source: Dictionary=host.kernel.source_identity()
	EstatePanel._draft(host)
	host.kernel.async_page=true
	_need(not EstatePanel.prepare(host),"async source keeps original page preparation pending")
	host.kernel.async_page=false;host.kernel.page_actor="other"
	_need(not EstatePanel.prepare(host),"wrong observer cannot complete page readiness")
	host.kernel.page_actor="executor"
	_need(EstatePanel.prepare(host),"fresh own source completes original readiness")
	EstatePanel.show_notice(host,"",source,"executor")
	_need(host.get_meta(EstatePanel.META).tab=="documents","empty notice does not enter estate tab")
	EstatePanel.show_notice(host,"task_death_notice_bound:12",source,"other")
	_need(host.get_meta(EstatePanel.META).tab=="documents","other observer cannot open estate details")
	EstatePanel.show_notice(host,"task_death_notice_bound:12",source,"executor")
	_need(host.get_meta(EstatePanel.META).tab=="estate" and host.get_meta(EstatePanel.META).notice_binding_fact_id=="task_death_notice_bound:12","selected original notice is the only estate entry parameter")
	var action: Dictionary={"available":true,"command":"private_shop_estate_close","params":{"shop_id":"private_shop:1","mandate_fact_id":"private_shop_executor:8","death_notice_binding_fact_id":"task_death_notice_bound:12"},"quote":{"terms":host.kernel.terms.duplicate(true)}}
	var result: Dictionary=EstatePanel._submit(weakref(host),weakref(button),action,source,"executor",7)
	_need(not result.get("ok",false) and host.submitted.is_empty(),"unreviewed closing cannot submit")
	EstatePanel._choose(weakref(host),weakref(button),action,source,"executor",7)
	_need(host.submitted.is_empty(),"review alone does not move original assets")
	host.kernel.terms.returned_cash=119
	result=EstatePanel._submit(weakref(host),weakref(button),action,source,"executor",7)
	_need(not result.get("ok",false) and host.submitted.is_empty(),"changed native terms require a new review")
	host.kernel.terms.returned_cash=120
	result=EstatePanel._submit(weakref(host),weakref(button),action,source,"executor",7)
	_need(result.get("ok",false) and host.submitted.size()==1,"fresh reviewed terms submit once")
	_need(host.submitted[0]=={"kind":action.command,"params":action.params,"actor_id":"executor","request_seq":9,"expected_revision":3},"payload contains authority sources but no alternate return wallet")
	result=EstatePanel._submit(weakref(host),weakref(button),action,source,"executor",7)
	_need(not result.get("ok",false) and host.submitted.size()==1,"same confirmation cannot repeat")
	button.disabled=false;host.kernel.source.execution_epoch=2
	result=EstatePanel._submit(weakref(host),weakref(button),action,source,"executor",7)
	_need(not result.get("ok",false) and host.submitted.size()==1,"restore epoch rejects old review")
	host.kernel.source.execution_epoch=1;host.kernel.actor="beneficiary"
	result=EstatePanel._submit(weakref(host),weakref(button),action,source,"executor",7)
	_need(not result.get("ok",false) and host.submitted.size()==1,"inheritor cannot reuse executor review")
	host.kernel.actor="executor";host._scene_counter=8
	result=EstatePanel._submit(weakref(host),weakref(button),action,source,"executor",7)
	_need(not result.get("ok",false) and host.submitted.size()==1,"closed scene cannot submit")
	host._scene_counter=7;host._storage_intent={"loading":true}
	result=EstatePanel._submit(weakref(host),weakref(button),action,source,"executor",7)
	_need(not result.get("ok",false) and host.submitted.size()==1,"restore in progress rejects stale control")
	host._storage_intent={};host.kernel.allowed=false
	result=EstatePanel._submit(weakref(host),weakref(button),action,source,"executor",7)
	_need(not result.get("ok",false) and host.submitted.size()==1 and host._message.contains("责任"),"real outstanding order refusal survives old quote")
	host.kernel.allowed=true;button.hide()
	result=EstatePanel._submit(weakref(host),weakref(button),action,source,"executor",7)
	_need(not result.get("ok",false) and host.submitted.size()==1,"hidden control cannot close")
	button.show();var dead_button=weakref(button);button.free()
	result=EstatePanel._submit(weakref(host),dead_button,action,source,"executor",7)
	_need(not result.get("ok",false) and host.submitted.size()==1,"destroyed control cannot submit")
	_need(EstatePanel._summary(action).contains("dead.wallet") and EstatePanel._summary(action).contains("执行人不取得"),"confirmation describes original-owner return rather than executor inheritance")
	_need(EstatePanel._reason(host,"BadSource")=="BadSource","unknown damaged source is not relabelled a normal refusal")
	var cancel_button=Button.new();host.add_child(cancel_button)
	host.kernel.terms={"released_reservation":40,"paid_price":0,"refunded_price":0,"scope":"只释放原客户预约，不支付或退款。"}
	var cancel: Dictionary={"available":true,"command":"task_shop_order_estate_cancel","params":{"shop_id":"private_shop:1","order_id":"private_shop_order:5","mandate_fact_id":"private_shop_executor:8","death_notice_binding_fact_id":"task_death_notice_bound:12"},"quote":{"terms":host.kernel.terms.duplicate(true)}}
	EstatePanel._choose(weakref(host),weakref(cancel_button),cancel,source,"executor",7)
	_need(host.submitted.size()==1,"reviewing one old order does not cancel all orders")
	result=EstatePanel._submit(weakref(host),weakref(cancel_button),cancel,source,"executor",7)
	_need(result.get("ok",false) and host.submitted.size()==2 and host.submitted[1].kind=="task_shop_order_estate_cancel" and host.submitted[1].params==cancel.params,"confirmed original order submits only four authority IDs")
	_need(EstatePanel._summary(cancel).contains("不支付或退款"),"unpaid cancellation is not described as a cash refund")
	host.free();print("private shop liquidation callback guards: ",checks," checks, ",failures," failures");quit(0 if failures==0 else 1)
