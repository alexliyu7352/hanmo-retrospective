## 仅验证原办理回调的来源、明确确认与一次提交；替身命令不冒真实合同或GUI验收。
extends SceneTree
const ShopPanel=preload("res://ui/private_shop_panel.gd")
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
	_need(ShopPanel.TABS.get("ledger")=="经营明细","ledger is reachable from original owned-shop navigation")
	var pending_text: String=ShopPanel._active_expenses_text({"unclosed_rent_paid":2,"unclosed_wage_paid":1})
	_need(pending_text.contains("租金 2") and pending_text.contains("工钱 1") and pending_text.contains("已计入") and pending_text.contains("不再重复"),"unclosed actual expenses are already in native profit, not a second UI deduction")
	var capital_text: String=ShopPanel._ledger_text({"record_kind":"capital","tick":4,"amount":100})
	_need(capital_text.contains("100") and capital_text.contains("不计经营收入"),"capital text does not become sales or profit")
	var stock_text: String=ShopPanel._ledger_text({"record_kind":"stock","tick":4,"item_id":"item.grain","quantity":3,"purchase_cash":10,"cost_observed_tick":8,"sold_quantity":2,"recognized_cost":6,"unrecognized_quantity":1,"unrecognized_cost":4,"trade_fact_id":"life_activity:17"})
	_need(stock_text.contains("10/3") and stock_text.contains("半日 8") and stock_text.contains("已售 2") and stock_text.contains("确认成本 6") and stock_text.contains("1 件共 4") and stock_text.contains("关店原物归还") and stock_text.contains("life_activity:17"),"purchase cost progress keeps exact basis and does not label returned goods as sales or remaining inventory")
	var remote_stock: String=ShopPanel._ledger_text({"record_kind":"stock","tick":4,"item_id":"item.grain","quantity":3,"purchase_cash":10,"trade_fact_id":"life_activity:17"})
	_need(remote_stock.contains("10 钱") and remote_stock.contains("尚未获知") and not remote_stock.contains("已售 0"),"original remote purchase stays known without inventing zero sales or stock")
	var day_text: String=ShopPanel._ledger_text({"record_kind":"retail","tick":5,"completed":false,"reason":"cancelled","quantity":0,"income":0,"cost_of_goods":0,"rent_paid":2,"wage_paid":2,"net_income":-4})
	_need(day_text.contains("营业中断") and day_text.contains("-4") and day_text.contains("已付工钱 2"),"interruption retains actual paid rent and earned wage")
	var order_text: String=ShopPanel._ledger_text({"record_kind":"order","tick":6,"outcome":"delivered","income":40,"cost_of_goods":28,"net_income":12,"task_id":"task:9","task_settlement_id":"settlement:14"})
	_need(order_text.contains("12") and order_text.contains("settlement:14"),"order text uses exact settlement and native realized net")
	var decision_text: String=ShopPanel._ledger_text({"record_kind":"staff_decision","tick":7,"decision":"dismissed"})
	_need(decision_text.contains("本人已明确辞退") and decision_text.contains("不是今日远方状态"),"personal decision remains a historical document instead of current remote status")
	var wage_text: String=ShopPanel._ledger_text({"record_kind":"staff_wage","tick":7,"attendance_index":1,"closing_fraction":true,"earned_cash":1,"paid_cash":1,"operation_id":"action:10","arrear_id":"local_due:11","payment_id":"local_payment:12"})
	_need(wage_text.contains("中断后结清已赚分数") and wage_text.contains("已付 1") and wage_text.contains("local_due:11") and wage_text.contains("local_payment:12"),"wage text exposes actual earned fraction and original two accounting sources")
	var closed_text: String=ShopPanel._ledger_text({"record_kind":"closing","tick":7,"returned_cash":123,"returned_quantity":3})
	_need(closed_text.contains("123") and closed_text.contains("不另计出售价值"),"closing actual remainder is not new profit")
	var host=HostStub.new();root.add_child(host)
	_need(ShopPanel._reason(host,"PrivateShopThreeClientsRequired").contains("同一客户"),"qualification reason explains distinct original clients")
	_need(ShopPanel._reason(host,"PrivateShopInsufficientOpeningFunds").contains("900") and ShopPanel._reason(host,"PrivateShopInsufficientOpeningFunds").contains("300"),"opening money reason keeps fee and capital distinct")
	_need(ShopPanel._reason(host,"PrivateShopOrderOutstanding").contains("取消"),"closing points to original order resolution")
	_need(ShopPanel._reason(host,"PrivateShopCommerceSourceMismatch")=="PrivateShopCommerceSourceMismatch", "bad source is not explained away as insufficient skill")
	var button=Button.new();host.add_child(button)
	var source: Dictionary=host.kernel.source_identity()
	var action: Dictionary={"command":"private_shop_fund","params":{"shop_id":"private_shop:1","amount":100},"available":true,"text":"本人接受","summary":"原合同冻结条款"}
	ShopPanel._draft(host)
	var result: Dictionary=ShopPanel._submit(weakref(host),weakref(button),action,source,"worker",7)
	_need(not result.get("ok",false) and host.submitted.is_empty(),"without explicit review selection no command is submitted")
	ShopPanel._choose(weakref(host),weakref(button),action,source,"worker",7)
	_need(host.submitted.is_empty(),"review selection itself never accepts employment")
	result=ShopPanel._submit(weakref(host),weakref(button),action,source,"worker",7)
	_need(result.get("ok",false) and host.submitted.size()==1,"explicit confirmation submits exactly one original command")
	_need(host.submitted[0]=={"kind":"private_shop_fund","params":{"shop_id":"private_shop:1","amount":100},"actor_id":"worker","request_seq":9,"expected_revision":3},"submitted payload cannot edit wages wallets or worker identity")
	result=ShopPanel._submit(weakref(host),weakref(button),action,source,"worker",7)
	_need(not result.get("ok",false) and host.submitted.size()==1,"same confirmed button cannot submit twice")
	button.disabled=false;host.kernel.source.execution_epoch=2
	result=ShopPanel._submit(weakref(host),weakref(button),action,source,"worker",7)
	_need(not result.get("ok",false) and host.submitted.size()==1,"restore epoch invalidates old confirmation")
	host.kernel.source.execution_epoch=1;host.kernel.actor="employer"
	result=ShopPanel._submit(weakref(host),weakref(button),action,source,"worker",7)
	_need(not result.get("ok",false) and host.submitted.size()==1,"changing controlled actor cannot sign old worker consent")
	host.kernel.actor="worker";host._scene_counter=8
	result=ShopPanel._submit(weakref(host),weakref(button),action,source,"worker",7)
	_need(not result.get("ok",false) and host.submitted.size()==1,"old closed scene cannot submit")
	host._scene_counter=7;host.kernel.allowed=false
	result=ShopPanel._submit(weakref(host),weakref(button),action,source,"worker",7)
	_need(not result.get("ok",false) and host.submitted.size()==1,"fresh original business refusal wins over old review")
	host.kernel.allowed=true;button.hide()
	result=ShopPanel._submit(weakref(host),weakref(button),action,source,"worker",7)
	_need(not result.get("ok",false) and host.submitted.size()==1,"hidden old control cannot sign")
	button.show();var dead_ref=weakref(button);button.free()
	result=ShopPanel._submit(weakref(host),dead_ref,action,source,"worker",7)
	_need(not result.get("ok",false) and host.submitted.size()==1,"destroyed control is safely ignored")
	var wage_button=Button.new();host.add_child(wage_button)
	ShopPanel._page(weakref(host),weakref(wage_button),"staff_ledger","",0,source,"worker",7,"private_shop_staff:9")
	_need(host.get_meta(ShopPanel.META).contract_id=="private_shop_staff:9" and ShopPanel._args(host.get_meta(ShopPanel.META)).contract_id=="private_shop_staff:9" and host.submitted.size()==1,"wage navigation preserves exact contract and submits no command")
	ShopPanel._page(weakref(host),weakref(wage_button),"staff","",0,source,"worker",7)
	_need(host.get_meta(ShopPanel.META).contract_id=="" and host.get_meta(ShopPanel.META).selected.is_empty(),"leaving wage history clears contract and prior money confirmation")
	var request_button=Button.new();host.add_child(request_button)
	var request: Dictionary={"command":"private_shop_request_order","params":{"shop_id":"private_shop:1","policy_id":"order.policy"},"available":true,"text":"询问雇主","summary":"原作者邀约，尚未本人接受"}
	ShopPanel._choose(weakref(host),weakref(request_button),request,source,"worker",7)
	_need(host.submitted.size()==1,"asking NPC employer still requires explicit confirmation")
	result=ShopPanel._submit(weakref(host),weakref(request_button),request,source,"worker",7)
	_need(result.get("ok",false) and host.submitted.size()==2 and host.submitted[1].kind=="private_shop_request_order" and host.submitted[1].params=={"shop_id":"private_shop:1","policy_id":"order.policy"},"confirmed inquiry submits only original policy, never acceptance or employer money fields")
	host.free();print("private shop panel necessary callback guards: ",checks," checks, ",failures," failures")
	quit(0 if failures==0 else 1)
