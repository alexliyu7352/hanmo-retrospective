## 正式作者世界的Main生产入口；只用当前可见控件提交配置与撤回，不改世界或授测试权限。
extends SceneTree
const Main=preload("res://ui/main.gd")
const EconomicPanel=preload("res://ui/economic_world_panel.gd")
var checks: int=0
var failures: int=0

## 等正式扩展和Viewport注册后才安装同一生产Main。
func _init() -> void: call_deferred("run")

## 失败保留当前来源与真实错误，不把空控件视作通过。
func check(value: bool, label: String) -> bool:
	checks+=1
	if not value: failures+=1; printerr("FAIL: ",label)
	return value

## 等原生工作队列及Main读屏障结束；本函数不补虚构查询或时钟。
func settled(main: Node) -> bool:
	var deadline: int=Time.get_ticks_msec()+30000
	while Time.get_ticks_msec()<deadline:
		if main._started and main._resident_mode and main._storage_intent.is_empty() and not main._advancing and not main._resident_frame_pending and not main.kernel.is_busy() and not main._resident_adapter.is_busy() and main._resident_command.is_empty() and main._resident_quote.is_empty():
			await process_frame
			if not main._resident_frame_pending and not main.kernel.is_busy(): return true
		await process_frame
	return check(false,"Main原生队列未完成："+main._message)

## 只选择可见的正式command_kind入口，不直接调用经济领域写函数。
func button(parent: Node, kind: String) -> Button:
	if parent is Button and parent.is_visible_in_tree() and not parent.is_queued_for_deletion() and parent.get_meta("command_kind","")==kind: return parent
	for child in parent.get_children():
		var found: Button=button(child,kind)
		if found!=null: return found
	return null

## 当前原生身份和实际草稿由页面决定，不能沿旧page硬填authorities。
func choose_office(main: Node) -> bool:
	if not main._dynamic_open:
		main._open_dynamic_world()
		if not await settled(main): return false
	var page: Button=main.find_child("DynamicEconomyPage",true,false)
	if not check(page!=null,"身份更新后仍可进入原经济页"): return false
	if not page.disabled:
		page.pressed.emit()
		if not await settled(main): return false
	var choose: Button=button(main,"economic_authority")
	if not check(choose!=null and not choose.disabled,"真实Office公务身份可选择"): return false
	choose.pressed.emit()
	return await settled(main)

## 释放本次Main后保留进程日志；不改已有槽位或保存银行。
func finish(main: Node) -> void:
	if is_instance_valid(main): main.queue_free()
	await process_frame
	print("ECONOMIC_MANAGEMENT_MAIN_CHECKS=",checks," FAILURES=",failures," SOURCE=FORMAL_AUTHOR_NATIVE_MAIN GUI=NOT_TESTED UI_SLOT_BANK=NOT_TESTED")
	quit(0 if failures==0 else 1)

## 一次真实配置、撤回与返回形成可玩入口；周期产消和税款守恒交同源Session门。
func run() -> void:
	if not check(ClassDB.class_exists("HanmoResidentBridge"),"生产resident扩展已注册"): quit(1); return
	var catalog: String=""
	for arg in OS.get_cmdline_user_args():
		if arg.begins_with("--catalog="): catalog=arg.trim_prefix("--catalog=")
	if not check(not catalog.is_empty(),"提供实际prepared目录"): quit(1); return
	var main=Main.new();root.add_child(main);main.size=Vector2(1280,800)
	var installed: Dictionary=main.start_content("civicecon::scenario.qingchuan_political_economy",catalog,{},"society::actor.city_steward")
	if not check(installed.get("ok",false),"Main安装新正式情景："+str(installed)) or not await settled(main): await finish(main); return
	var initial: Dictionary=main.kernel.source_identity()
	main._open_dynamic_world()
	if not await settled(main): await finish(main); return
	var page: Button=main.find_child("DynamicEconomyPage",true,false)
	if not check(page!=null and not page.disabled,"原地方页提供生产与收支入口"): await finish(main); return
	page.pressed.emit()
	if not await settled(main) or not await choose_office(main): await finish(main); return
	check(main.kernel.source_identity()==initial,"进入与选择真实公务账不推进世界")
	check(main.find_child("EconomicWorldSection",true,false)!=null and main.find_child("FiscalSettlementSection",true,false)!=null,"生产与税账共用实际经济页")
	var spin: SpinBox=main.find_child("ProductionWorkers",true,false)
	var submit: Button=main.find_child("ProductionConfigure",true,false)
	if not check(spin!=null and submit!=null and spin.value>0 and not submit.disabled,"实际生产单位有合法配置控件"): await finish(main); return
	spin.value=floor(spin.value/2.0)
	check(main.kernel.source_identity()==initial,"改人数只写本页草稿")
	submit.pressed.emit()
	if not await settled(main): await finish(main); return
	var configured: Dictionary=main.kernel.source_identity()
	if not check(configured.world_id==initial.world_id and configured.execution_epoch==initial.execution_epoch and configured.tick==initial.tick and configured.revision==initial.revision+1,"配置真实成功一次且不提前生产："+main._message): await finish(main); return
	if not await choose_office(main): await finish(main); return
	var cancel: Button=main.find_child("ProductionCancel",true,false)
	if not check(cancel!=null and not cancel.disabled,"原生已安排计划提供实际撤回入口"): await finish(main); return
	cancel.pressed.emit()
	if not await settled(main): await finish(main); return
	var cancelled: Dictionary=main.kernel.source_identity()
	check(cancelled.revision==configured.revision+1 and cancelled.tick==configured.tick,"撤回只解除未来责任，不推进账期")
	if not await choose_office(main): await finish(main); return
	check(main.find_child("ProductionCancel",true,false)==null,"撤回后的正式页不再呈现未生效计划")
	var close: Button=main.find_child("DynamicWorldClose",true,false)
	if not check(close!=null,"原地方页仍提供返回场所"): await finish(main); return
	close.pressed.emit()
	if not await settled(main): await finish(main); return
	check(not main._dynamic_open and main.kernel.source_identity()==cancelled,"返回场所不额外提交命令")
	await finish(main)
