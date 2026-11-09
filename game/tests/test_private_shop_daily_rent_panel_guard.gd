## 原选择、实际报价和本人再次确认各自必需；替身只验控件，不冒充真实日租资金通过。
extends SceneTree
const RentPanel=preload("res://ui/private_shop_daily_rent_panel.gd")
var checks: int=0
var failures: int=0
class KernelStub extends RefCounted:
	var source={"world_id":"world","execution_epoch":1,"revision":3,"tick":8}
	var actor="executor"
	var enabled=true
	var allowed=true
	var async_page=false
	var owner_history=false
	var terms={"from_wallet_id":"executor.wallet","to_wallet_id":"lessor.wallet","amount":2,"arrear_id":"local_due:10","from_tick":6,"through_tick":8,"effect":"只偿原欠期，不复业。"}
	func private_shop_daily_rent_enabled() -> bool: return enabled
	func source_identity() -> Dictionary: return source.duplicate(true)
	func command_context() -> Dictionary: return {"ok":true,"actor_id":actor,"revision":source.revision,"next_request_seq":9}
	func demand_view(_kind: String,args: Dictionary) -> Dictionary:
		return {"ok":not async_page,"code":"NativeAsyncRequired" if async_page else "","source_identity":source.duplicate(true),"observer_actor_id":actor,"shop_id":args.shop_id,"actions":[],"owner_history":owner_history,"history_cursor":args.get("cursor",0),"next_cursor":int(args.get("cursor",0))+16}
	func preview(_kind: String,_params: Dictionary,_actor: String) -> Dictionary:
		return {"ok":allowed,"revision":source.revision,"terms":terms.duplicate(true),"code":"" if allowed else "PrivateShopRentFullPaymentRequired"}
class HostStub extends Control:
	var kernel=KernelStub.new()
	var _resident_mode=true
	var _scene_counter=7
	var _advancing=false
	var _storage_intent={}
	var submitted=[]
	func _render() -> void: pass
	func _resident_submit_frozen(command: Dictionary) -> Dictionary:
		submitted.append(command.duplicate(true));return {"ok":true}
func _initialize() -> void: call_deferred("_run")
func _need(ok: bool,why: String) -> void:
	checks+=1
	if not ok: failures+=1;push_error(why)
func _run() -> void:
	var host=HostStub.new();root.add_child(host)
	var button=Button.new();host.add_child(button)
	var source=host.kernel.source_identity()
	RentPanel.show_shop(host,"private_shop:1","mandate:3","",source,"executor")
	_need(not host.has_meta(RentPanel.META),"half an executor source group cannot enter")
	host.kernel.enabled=false
	RentPanel.show_shop(host,"private_shop:1","mandate:3","notice:6",source,"executor")
	_need(not host.has_meta(RentPanel.META),"disabled capability cannot create a draft")
	host.kernel.enabled=true
	RentPanel.show_shop(host,"private_shop:1","mandate:3","notice:6",source,"executor")
	_need(host.get_meta(RentPanel.META).selected.is_empty(),"entry does not preselect personal payment")
	host.kernel.async_page=true;_need(not RentPanel.prepare(host),"async original source blocks readiness")
	host.kernel.async_page=false;_need(RentPanel.prepare(host),"same source can finish readiness")
	host.kernel.owner_history=true
	var draft: Dictionary=host.get_meta(RentPanel.META);draft.selected={"old":"review"};host.set_meta(RentPanel.META,draft)
	RentPanel._page(weakref(host),weakref(button),99,source,"executor",7)
	_need(host.get_meta(RentPanel.META).cursor==0,"arbitrary history cursor not offered by original page is rejected")
	RentPanel._page(weakref(host),weakref(button),16,source,"executor",7)
	_need(host.get_meta(RentPanel.META).cursor==16 and host.get_meta(RentPanel.META).selected.is_empty() and host.submitted.is_empty(),"actual next page clears stale payment review without command")
	host.kernel.owner_history=false
	RentPanel._page(weakref(host),weakref(button),0,source,"executor",7)
	_need(host.get_meta(RentPanel.META).cursor==16,"executor-limited page cannot turn into owner history")
	draft=host.get_meta(RentPanel.META);draft.cursor=0;host.set_meta(RentPanel.META,draft)
	var action={"available":true,"command":"private_shop_pay_rent_arrear","params":{"shop_id":"private_shop:1","assessment_fact_id":"private_shop_rent_assessed:9","payer_wallet_id":"executor.wallet","mandate_fact_id":"mandate:3","death_notice_binding_fact_id":"notice:6"},"quote":{"terms":host.kernel.terms.duplicate(true)}}
	var result=RentPanel._submit(weakref(host),weakref(button),action,source,"executor",7)
	_need(not result.ok and host.submitted.is_empty(),"unreviewed payment cannot submit")
	RentPanel._choose(weakref(host),weakref(button),action,source,"executor",7)
	_need(host.submitted.is_empty(),"review does not spend any money")
	host.kernel.terms.amount=4
	result=RentPanel._submit(weakref(host),weakref(button),action,source,"executor",7)
	_need(not result.ok and host.submitted.is_empty(),"changed price needs another explicit decision")
	host.kernel.terms.amount=2;host.kernel.allowed=false
	result=RentPanel._submit(weakref(host),weakref(button),action,source,"executor",7)
	_need(not result.ok and host.submitted.is_empty(),"actual money or authority refusal wins over old quote")
	host.kernel.allowed=true
	result=RentPanel._submit(weakref(host),weakref(button),action,source,"executor",7)
	_need(result.ok and host.submitted.size()==1 and host.submitted[0].params==action.params,"confirmed actual debt and original payer submit once")
	result=RentPanel._submit(weakref(host),weakref(button),action,source,"executor",7)
	_need(not result.ok and host.submitted.size()==1,"disabled confirmation cannot repeat")
	button.disabled=false;host.kernel.source.execution_epoch=2
	result=RentPanel._submit(weakref(host),weakref(button),action,source,"executor",7)
	_need(not result.ok and host.submitted.size()==1,"restored world rejects stale payment")
	host.kernel.source.execution_epoch=1;host.kernel.actor="beneficiary"
	result=RentPanel._submit(weakref(host),weakref(button),action,source,"executor",7)
	_need(not result.ok and host.submitted.size()==1,"another person cannot pay using executor decision")
	host.kernel.actor="executor";host._scene_counter=8
	result=RentPanel._submit(weakref(host),weakref(button),action,source,"executor",7)
	_need(not result.ok and host.submitted.size()==1,"closed scene rejects old callback")
	host._scene_counter=7;host._storage_intent={"loading":true}
	result=RentPanel._submit(weakref(host),weakref(button),action,source,"executor",7)
	_need(not result.ok and host.submitted.size()==1,"loading rejects old callback")
	host._storage_intent={};button.hide()
	result=RentPanel._submit(weakref(host),weakref(button),action,source,"executor",7)
	_need(not result.ok and host.submitted.size()==1,"hidden control rejects old callback")
	button.show();var dead=weakref(button);button.free()
	result=RentPanel._submit(weakref(host),dead,action,source,"executor",7)
	_need(not result.ok and host.submitted.size()==1,"freed control cannot spend")
	host.free();print("private daily rent panel: ",checks," checks, ",failures," failures");quit(0 if failures==0 else 1)
