## 显式本人回应的真实Main入口；复用原候选恢复与队列屏障，不创建第二任务账或代办履约。
extends "res://tests/test_interaction_tasks_main.gd"

## 读取真正可见Label核回访说明，不能拿原页JSON当作已渲染文字。
func response_labels(node: Node) -> Array[String]:
	var result: Array[String]=[]
	if node is Label and node.is_visible_in_tree(): result.append(node.text)
	for child in node.get_children(): result.append_array(response_labels(child))
	return result

## 只认当前原生行中的本人委托动作，不靠按钮文案猜命令。
func response_action(main: Node, offer: String, kind: String) -> Button:
	for control in actions(main):
		if not control.is_visible_in_tree() or control.is_queued_for_deletion(): continue
		var action: Dictionary=control.get_meta("task_action")
		if action.get("command")==kind and action.get("params",{}).get("offer_id")==offer: return control
	return null

## 银行必须由本轮正式回应Session产生；旧包/旧profile不能通过手工改JSON顶替。
func _run() -> void:
	var bank_path: String="";var expected_path: String=""
	for arg in OS.get_cmdline_user_args():
		if arg.begins_with("--bank="): bank_path=arg.trim_prefix("--bank=")
		if arg.begins_with("--expected-page="): expected_path=arg.trim_prefix("--expected-page=")
		if arg.begins_with("--output="): output_path=arg.trim_prefix("--output=")
	if not check(ClassDB.class_exists("HanmoResidentBridge") and FileAccess.file_exists(bank_path) and FileAccess.file_exists(expected_path),"新回应扩展与原生银行就绪"): quit(1);return
	var bytes: PackedByteArray=FileAccess.get_file_as_bytes(bank_path)
	var parsed: Dictionary=StrictJSON.parse_text(FileAccess.get_file_as_string(expected_path))
	var expected: Variant=parsed.get("value")
	if not check(expected is Dictionary and expected.get("rows",[]).size()==2,"真实本人页保留两份已知委托"): quit(1);return
	var response_rows: int=0
	for row in expected.rows:
		if row.has("response_text"):
			response_rows+=1
			check(row.id=="contacts::offer.letter" and row.status=="known","原银行只记录家书的本人暂不承接")
	if not check(response_rows==1,"输入没有预造另一份委托的回应"): quit(1);return
	evidence.bank_sha256=FileAccess.get_sha256(bank_path);evidence.expected_page_sha256=FileAccess.get_sha256(expected_path)
	evidence.scope="formal Main response button; same native world; no delivery or escort replay"
	var main=Main.new();root.add_child(main);main.size=Vector2(1280,800)
	var prepared: Dictionary=main._resident_prepare_load_candidate(bytes)
	if not check(prepared.get("ok",false) and prepared.get("restored",false),"新profile原银行完整准入："+str(prepared)): await finish(main);return
	var adopted: Dictionary=main._resident_adopt_load_candidate()
	if not check(adopted.get("ok",false),"Main采用真正已恢复候选："+str(adopted)): await finish(main);return
	main._resident_frame_pending=true;main._advancing=true
	if not await settled(main): await finish(main);return
	main.kernel.command_completed.connect(observed_command)
	var initial: Dictionary=main.kernel.source_identity().duplicate(true)
	var before: PackedByteArray=await checkpoint(main)
	var close_map: Button=button(main,"map_close")
	if close_map!=null:
		close_map.pressed.emit()
		if not await settled(main): await finish(main);return
	var toggle: Button=button(main,"interaction_tasks_toggle")
	if not check(toggle!=null and not toggle.disabled,"当前真人会面可核对已知条款"): await finish(main);return
	toggle.pressed.emit()
	if not await settled(main): await finish(main);return
	evidence.displayed=rows(main).duplicate(true)
	if not check(evidence.displayed==expected.rows,"真实Main两行与原native有限页逐值相等"): await finish(main);return
	var text_found: bool=false
	for label in response_labels(main):
		if "暂不承接" in label and "重新询问" in label: text_found=true
	check(text_found,"过去的本人回应在实际Label可读")
	var accept: Button=response_action(main,"contacts::offer.letter","accept_offer")
	var decline: Button=response_action(main,"contacts::offer.letter","decline_offer")
	if not check(accept!=null and not accept.disabled and decline!=null and not decline.disabled,"原重新受领和显式回应各有已报价按钮"): await finish(main);return
	var after_read: PackedByteArray=await checkpoint(main)
	check(commands.is_empty() and before==after_read,"展开和报价是纯读，没有自动代答")
	var old_callback: Callable=Callable()
	for connection in decline.get_signal_connection_list("pressed"):
		if connection.callable.get_method()=="_request": old_callback=connection.callable
	if not check(old_callback.is_valid(),"保存实际生产请求回调"): await finish(main);return
	decline.pressed.emit()
	if not await settled(main): await finish(main);return
	check(commands.size()==1 and commands[0].kind=="decline_offer" and commands[0].result.get("ok",false),"真实按钮只提交一次具名本人回应")
	var current: Dictionary=main.kernel.source_identity().duplicate(true)
	check(current.get("tick")==initial.get("tick") and current.get("revision")==int(initial.revision)+1,"回应不推进半日且只有一次提交")
	toggle=button(main,"interaction_tasks_toggle")
	if not check(toggle!=null and not toggle.disabled,"提交后新来源条款入口可用"): await finish(main);return
	if rows(main).is_empty():
		toggle.pressed.emit()
		if not await settled(main): await finish(main);return
	check(rows(main)==expected.rows,"重复回应保原首次来源与另一委托状态")
	old_callback.call()
	if not await settled(main): await finish(main);return
	check(commands.size()==1 and main.kernel.source_identity()==current,"原旧来源回调失效且不重复提交")
	var final_bank: PackedByteArray=await checkpoint(main)
	check(not final_bank.is_empty() and FileAccess.get_file_as_bytes(bank_path)==bytes,"原输入银行未改写，当前真正世界可checkpoint")
	if not output_path.is_empty() and not final_bank.is_empty():
		var saved=FileAccess.open(output_path+".hmcb",FileAccess.WRITE)
		if check(saved!=null,"保存本次实际Main提交后的新银行"): saved.store_buffer(final_bank)
	evidence.initial_identity=initial;evidence.final_identity=current;evidence.after_rows=rows(main).duplicate(true)
	await finish(main)
