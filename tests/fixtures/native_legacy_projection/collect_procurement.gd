## 只复用已验采购实际命令前缀；覆盖输出钩子为原state/算法来源，不运行旧全门或新codec。
extends "res://tests/export_native_inventory_projection.gd"
const Capture=preload("res://tests/source_capture.gd")
var destination: String=""

## 保存真实采购前缀的源与独立旧答案，预期数值继续由父类原断言约束。
func capture_case(kernel,id: String,_label: String,requests: Array) -> bool:
	var before: Dictionary=kernel.save_context()
	var captured: Dictionary=Capture.capture(before,destination,id,"actual_kernel_commands",[PLAYER])
	if not check(captured.ok,"原Schema+独立答案 "+id+" "+str(captured.get("code",""))): return false
	for request in requests:
		check(Inventory.count(before.state.inventories[request.inventory_id],request.item_id,request.owners,request.reservation)==request.expected,"原Inventory.count "+id+"/"+request.query_id)
	if id=="actual_reporting":
		var answer: Dictionary=captured.answers[0]
		check(not answer.action_money_reservations.is_empty() and answer.action_lot_reservations.size()==1,"真实report同时预留报酬钱包与一批10份粮")
		print("ACTUAL_REPORTING=",JSON.stringify(answer))
	return check(kernel.save_context()==before,"活Kernel前后不变 "+id)

## 不调用父类其余Schema/native矩阵，仅执行授权短链和原预期。
func run() -> void:
	for arg in OS.get_cmdline_user_args():
		if arg.begins_with("--output="): destination=arg.trim_prefix("--output=")
	if destination.is_empty(): quit(2); return
	reachable_cases()
	print("M1_PROCUREMENT_SOURCES checks=",checks," failures=",failures)
	quit(0 if failures==0 else 1)
