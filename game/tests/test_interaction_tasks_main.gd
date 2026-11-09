## 原生已受领两约经正式Main候选恢复；只读条款和首次取消确认，不重跑业务或冒存档槽。
extends SceneTree
const Main=preload("res://ui/main.gd")
const StrictJSON=preload("res://core/strict_json.gd")
var checks: int=0
var failures: int=0
var commands: Array=[]
var evidence: Dictionary={}
var output_path: String=""

## 扩展与Viewport就绪后才进入实际Main。
func _init() -> void: call_deferred("_run")

## 每项失败保留正文与原来源，不以空页或未安装状态冒通过。
func check(value: bool,label: String) -> bool:
	checks+=1
	if not value: failures+=1;printerr("FAIL: ",label)
	return value

## 等原生同队列完成正常页屏障，不注入额外世界或帮玩家推进半日。
func settled(main: Node) -> bool:
	var deadline: int=Time.get_ticks_msec()+30000
	while Time.get_ticks_msec()<deadline:
		if main._started and main._resident_mode and main._storage_intent.is_empty() and not main._advancing and not main._resident_frame_pending and not main.kernel.is_busy() and not main._resident_adapter.is_busy() and main._resident_command.is_empty() and main._resident_quote.is_empty():
			await process_frame
			if not main._resident_frame_pending and not main.kernel.is_busy(): return true
		await process_frame
	return check(false,"原任务页队列超时："+main._message)

## 只查找当前可见生产按钮；静态文本不是可提交入口。
func button(node: Node,kind: String) -> Button:
	if node is Button and node.is_visible_in_tree() and not node.is_queued_for_deletion() and node.get_meta("command_kind","")==kind: return node
	for child in node.get_children():
		var found: Button=button(child,kind)
		if found!=null: return found
	return null

## 任务行与动作按原控件元数据逐行读取，不重新构造业务条件。
func rows(node: Node) -> Array:
	var result: Array=[]
	if node.has_meta("task_row"): result.append(node.get_meta("task_row"))
	for child in node.get_children(): result.append_array(rows(child))
	return result

## 只选择原任务按钮，避免把关闭、翻页或对白选项误当任务命令。
func actions(node: Node) -> Array:
	var result: Array=[]
	if node is Button and node.has_meta("task_action"): result.append(node)
	for child in node.get_children(): result.append_array(actions(child))
	return result

## 原命令完成事件只作计数；本门不以回调重放或补交命令。
func observed_command(kind: String,result: Dictionary,source: Dictionary,current: Dictionary) -> void:
	commands.append({"kind":kind,"result":result.duplicate(true),"source":source.duplicate(true),"current":current.duplicate(true)})

## 同一原worker的冷检查点用于核纯读，绝不写UI磁盘存档槽。
func checkpoint(main: Node) -> PackedByteArray:
	var source: Dictionary=main.kernel.source_identity()
	var transport=main.kernel.transport()
	var accepted: Dictionary=transport.enqueue_live_operation({"kind":"checkpoint","source":source,"payload":{}})
	if not check(accepted.get("ok",false),"原worker接纳检查点纯读"): return PackedByteArray()
	var deadline: int=Time.get_ticks_msec()+30000
	while Time.get_ticks_msec()<deadline:
		var reply: Dictionary=transport.poll_live_operation(accepted.reference)
		if not reply.get("ready",false) and reply.get("ok",false): await process_frame;continue
		if not check(reply.get("ok",false) and reply.get("tag")=="checkpoint" and reply.get("source_identity")==source and reply.get("current_identity")==source and reply.get("result",{}).get("ok",false),"检查点回执与当前完整来源一致"): return PackedByteArray()
		return reply.result.get("checkpoint_bytes",PackedByteArray())
	check(false,"原worker检查点超时");return PackedByteArray()

## 退出时保留实际页和失败，不删原输入bank或把进程零退出替代检查结果。
func finish(main: Node) -> void:
	evidence.checks=checks;evidence.failures=failures;evidence.commands=commands;evidence.gui="NOT_TESTED";evidence.ui_save_slot="NOT_TESTED"
	if not output_path.is_empty():
		var file=FileAccess.open(output_path,FileAccess.WRITE)
		if file!=null: file.store_string(JSON.stringify(evidence,"\t",false,true)+"\n")
	if is_instance_valid(main): main.queue_free()
	await process_frame
	print("INTERACTION_TASKS_MAIN_CHECKS=",checks," FAILURES=",failures," SOURCE=FORMAL_CONTACTS_NATIVE_BANK GUI=NOT_TESTED UI_SLOT=NOT_TESTED")
	quit(0 if failures==0 else 1)

## 两约同时显示、原拒绝可读、首次放弃仅确认、收起后旧回调失效，全程世界字节不变。
func _run() -> void:
	var bank_path: String="";var expected_path: String=""
	for arg in OS.get_cmdline_user_args():
		if arg.begins_with("--bank="): bank_path=arg.trim_prefix("--bank=")
		if arg.begins_with("--expected-page="): expected_path=arg.trim_prefix("--expected-page=")
		if arg.begins_with("--output="): output_path=arg.trim_prefix("--output=")
	if not check(ClassDB.class_exists("HanmoResidentBridge") and FileAccess.file_exists(bank_path) and FileAccess.file_exists(expected_path),"真实扩展与两约原bank就绪"): quit(1);return
	var bytes: PackedByteArray=FileAccess.get_file_as_bytes(bank_path)
	var parsed: Dictionary=StrictJSON.parse_text(FileAccess.get_file_as_string(expected_path))
	var expected: Variant=parsed.get("value")
	if not check(expected is Dictionary and expected.get("rows",[]).size()==2,"实际期望页有两份本人受领合同"): quit(1);return
	evidence.bank_sha256=FileAccess.get_sha256(bank_path);evidence.expected_page_sha256=FileAccess.get_sha256(expected_path)
	var main=Main.new();root.add_child(main);main.size=Vector2(1280,800)
	var prepared: Dictionary=main._resident_prepare_load_candidate(bytes)
	if not check(prepared.get("ok",false) and prepared.get("restored",false),"正式候选完整恢复："+str(prepared)): await finish(main);return
	var adopted: Dictionary=main._resident_adopt_load_candidate()
	if not check(adopted.get("ok",false),"正式Main采用已准入候选："+str(adopted)): await finish(main);return
	main._resident_frame_pending=true;main._advancing=true
	if not await settled(main): await finish(main);return
	main.kernel.command_completed.connect(observed_command)
	var initial: Dictionary=main.kernel.source_identity().duplicate(true)
	var before: PackedByteArray=await checkpoint(main)
	var map_close: Button=button(main,"map_close")
	if map_close!=null:
		map_close.pressed.emit()
		if not await settled(main): await finish(main);return
	var toggle: Button=button(main,"interaction_tasks_toggle")
	evidence.message=main._message;evidence.interaction=main.kernel.interaction_view().duplicate(true)
	if not check(toggle!=null and not toggle.disabled,"真实当面页提供两约条款入口"): await finish(main);return
	check(rows(main).is_empty(),"条款默认收起，不自动办理")
	toggle.pressed.emit()
	if not await settled(main): await finish(main);return
	var displayed: Array=rows(main);evidence.displayed=displayed.duplicate(true)
	if not check(displayed==expected.rows,"两约完整条款来自原native有限页"): await finish(main);return
	var controls: Array=actions(main);var reports: int=0;var cancel: Button=null
	for control in controls:
		var action: Dictionary=control.get_meta("task_action")
		if action.command=="report_offer": reports+=1;check(control.disabled and not action.available,"未亲交或未到达的原回报仍拒绝")
		if action.command=="cancel_task" and action.params.get("offer_id")=="contacts::offer.letter": cancel=control
	if not check(reports==2 and cancel!=null and not cancel.disabled,"两条原回报与一条合法放弃入口均已核报价"): await finish(main);return
	cancel.pressed.emit()
	if not await settled(main): await finish(main);return
	check(commands.is_empty() and main.get_meta("interaction_tasks_draft",{}).get("cancel_id")=="contacts::offer.letter","第一次放弃仅显示本地确认")
	var pending: Button=null
	for control in actions(main):
		if control.text=="确认放弃并清算": pending=control
	if not check(pending!=null and not pending.disabled,"真实第二步确认按钮已准备"): await finish(main);return
	var prior_callback: Callable=Callable()
	for connection in pending.get_signal_connection_list("pressed"):
		if connection.callable.get_method()=="_request": prior_callback=connection.callable
	if not check(prior_callback.is_valid(),"已保存真实任务请求回调而非跳过检查"): await finish(main);return
	toggle=button(main,"interaction_tasks_toggle")
	if not check(toggle!=null and not toggle.disabled,"确认期间仍可收起条款"): await finish(main);return
	toggle.pressed.emit()
	if not await settled(main): await finish(main);return
	check(rows(main).is_empty() and main.get_meta("interaction_tasks_draft",{}).get("cancel_id","").is_empty(),"收起丢弃未提交取消确认")
	prior_callback.call()
	var after: PackedByteArray=await checkpoint(main)
	check(not before.is_empty() and before==after and commands.is_empty() and main.kernel.source_identity()==initial,"两约阅读与未确认取消全程零命令且检查点不变")
	check(FileAccess.get_file_as_bytes(bank_path)==bytes,"原生产bank未改写")
	evidence.checkpoint_bytes_unchanged=before==after;evidence.identity=initial
	await finish(main)
