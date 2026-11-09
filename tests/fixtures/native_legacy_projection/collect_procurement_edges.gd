## 只新增原来源未捕获的零金额预留和交易已交割但action尚存窗口；不重跑既有18份来源。
extends "res://tests/export_native_inventory_projection.gd"
const Capture=preload("res://tests/source_capture.gd")
var destination: String=""

## 每个新增样本只取一次正式三元组，捕获后不再推进。
func record(kernel,id: String) -> void:
	var before: Dictionary=kernel.save_context(); var result: Dictionary=Capture.capture(before,destination,id,"actual_kernel_commands",[PLAYER])
	check(result.ok and kernel.save_context()==before,"新边界原Schema与只读 "+id)

## 明确复用原采购作者与已存在零预算作者，不改运行钱包或行动。
func run() -> void:
	for arg in OS.get_cmdline_user_args():
		if arg.begins_with("--output="): destination=arg.trim_prefix("--output=")
	for zero in [false,true]:
		var kernel=Kernel.new(); var opened: Dictionary=kernel.new_game("scenario.procurement","res://data/generated/zero_budget.hmcatalog.json" if zero else "res://data/generated/procurement.hmcatalog.json")
		if not check(opened.ok,"已存在采购作者新局"): continue
		if not move_to(kernel,"west.office") or not check(send(kernel,"inspect",{"prop_id":"notice.grain"}).ok,"实际告示"): continue
		if not check(send(kernel,"accept_offer",{"offer_id":"offer.grain","context":context(kernel,"role.office")}).ok,"当面受托"): continue
		if zero:
			var current: Dictionary=kernel.snapshot(); var action: String=current.actors[PLAYER].action_id
			check(current.wallets["wallet.office"].reserved.has(action) and current.wallets["wallet.office"].reserved[action]==0,"真实存在的零reservation不能省略")
			record(kernel,"zero_money_reservation"); continue
		if not finish_action(kernel) or not move_to(kernel,"west.street") or not move_to(kernel,"west.grain"): continue
		var task: Dictionary=kernel.snapshot().tasks.values()[0]
		var bought: Dictionary=send(kernel,"trade",{"shop_id":"shop.grain","item_id":"item.grain","quantity":13,"side":"buy","payer_wallet_id":task.budget_wallet_id,"context":context(kernel,"role.merchant")})
		if not check(bought.ok,"真实trade start即时交割"): continue
		var before: Dictionary=kernel.save_context(); var answer: Dictionary=Capture.old_answers(before,[PLAYER])[0]
		check(not answer.action_id.is_empty() and answer.action.kind=="trade" and answer.action_money_reservations.is_empty() and answer.action_lot_reservations.is_empty(),"交易行动尚在但无当前预留，不能凭action补造锁定")
		record(kernel,"trade_active_without_reservation")
	print("M1_PROCUREMENT_EDGES checks=",checks," failures=",failures)
	quit(0 if failures==0 else 1)
