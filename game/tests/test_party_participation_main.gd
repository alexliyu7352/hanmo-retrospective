## 正式随行银行进入Main的实际按钮门；接受/离队即时完成，等候只推进原世界一半日。
extends "res://tests/test_interaction_tasks_main.gd"

## 只读取当前可见的生产行，不能用本地期望JSON代替页面。
func party_rows(node: Node) -> Array:
	var result: Array=[]
	if node.has_meta("party_row") and node is Control and node.is_visible_in_tree(): result.append(node.get_meta("party_row"))
	for child in node.get_children(): result.append_array(party_rows(child))
	return result

## 原command元数据确定按钮来源，不凭显示文案拼新参数。
func party_button(node: Node,kind: String) -> Button:
	if node is Button and node.is_visible_in_tree() and not node.is_queued_for_deletion() and node.get_meta("party_action",{}).get("command")==kind: return node
	for child in node.get_children():
		var found: Button=party_button(child,kind)
		if found!=null: return found
	return null

## 所有点击都让真实Main接报价、提交、刷新与自动续时收尾；不直接调用领域命令。
func party_click(main: Node,kind: String,expected_tick_delta: int) -> bool:
	var control: Button=party_button(main,kind)
	if not check(control!=null and not control.disabled,"真实同行动作已报价："+kind): return false
	var before: Dictionary=main.kernel.source_identity().duplicate(true)
	var count: int=commands.size()
	control.pressed.emit()
	if not await settled(main): return false
	return check(commands.size()==count+1 and commands[-1].kind==kind and commands[-1].result.get("ok",false) and int(main.kernel.source_identity().tick)==int(before.tick)+expected_tick_delta,"原按钮只提交一次且无额外自动半日："+kind)

## 每次输入来自同一正式Session的自然邀请/成员银行；两个模式各自使用独立进程与XDG。
func _run() -> void:
	var bank_path: String="";var expected_path: String="";var mode: String="accept"
	for arg in OS.get_cmdline_user_args():
		if arg.begins_with("--bank="): bank_path=arg.trim_prefix("--bank=")
		if arg.begins_with("--expected-page="): expected_path=arg.trim_prefix("--expected-page=")
		if arg.begins_with("--mode="): mode=arg.trim_prefix("--mode=")
		if arg.begins_with("--output="): output_path=arg.trim_prefix("--output=")
	if not check(mode in ["accept","leave"] and ClassDB.class_exists("HanmoResidentBridge") and FileAccess.file_exists(bank_path) and FileAccess.file_exists(expected_path),"正式新ABI、自然银行与具名模式齐备"): quit(1);return
	var bytes: PackedByteArray=FileAccess.get_file_as_bytes(bank_path)
	var parsed: Dictionary=StrictJSON.parse_text(FileAccess.get_file_as_string(expected_path))
	var expected: Variant=parsed.get("value")
	if not check(expected is Dictionary and expected.get("rows",[]).size()==1,"真实有限同行页有一条本人来源"): quit(1);return
	if not check(expected.rows[0].kind==("invitation" if mode=="accept" else "membership"),"自然银行状态与所选玩家出口一致"): quit(1);return
	evidence.bank_sha256=FileAccess.get_sha256(bank_path);evidence.expected_page_sha256=FileAccess.get_sha256(expected_path);evidence.mode=mode
	evidence.scope="real Main party controls from formal native bank; no UI save slot"
	var main=Main.new();root.add_child(main);main.size=Vector2(1280,800)
	var prepared: Dictionary=main._resident_prepare_load_candidate(bytes)
	if not check(prepared.get("ok",false) and prepared.get("restored",false),"原银行完整准入："+str(prepared)): await finish(main);return
	var adopted: Dictionary=main._resident_adopt_load_candidate()
	if not check(adopted.get("ok",false),"采用原真实候选："+str(adopted)): await finish(main);return
	main._resident_frame_pending=true;main._advancing=true
	if not await settled(main): await finish(main);return
	main.kernel.command_completed.connect(observed_command)
	var initial: Dictionary=main.kernel.source_identity().duplicate(true)
	var before: PackedByteArray=await checkpoint(main)
	if mode=="accept":
		var close_map: Button=button(main,"map_close")
		if close_map!=null:
			close_map.pressed.emit()
			if not await settled(main): await finish(main);return
	if not check(party_rows(main)==expected.rows,"实际Main当前行逐值等于原native有限页"): await finish(main);return
	var after_read: PackedByteArray=await checkpoint(main)
	check(before==after_read and commands.is_empty(),"页面及原报价完全纯读")
	evidence.initial_identity=initial;evidence.initial_rows=party_rows(main).duplicate(true)
	if mode=="accept":
		var accept: Button=party_button(main,"party_accept")
		if not check(accept!=null,"真实接受按钮存在"): await finish(main);return
		var old_callback: Callable=Callable()
		for connection in accept.get_signal_connection_list("pressed"):
			if connection.callable.get_method()=="_request": old_callback=connection.callable
		if not check(old_callback.is_valid(),"保存原生产弱引用回调"): await finish(main);return
		if not await party_click(main,"party_accept",0): await finish(main);return
		check(party_rows(main).size()==1 and party_rows(main)[0].kind=="membership","真实接受后地图侧栏显示本人随行")
		old_callback.call()
		if not await settled(main): await finish(main);return
		check(commands.size()==1,"旧邀请回调不能重复接受")
		if not await party_click(main,"end_interaction",0): await finish(main);return
		if not await party_click(main,"wait",1): await finish(main);return
		evidence.after_wait_rows=party_rows(main).duplicate(true)
	else:
		if not await party_click(main,"party_leave",0): await finish(main);return
		check(party_rows(main).is_empty(),"本人离队后旧队伍行消失且不自动加入别队")
	var final_bank: PackedByteArray=await checkpoint(main)
	check(not final_bank.is_empty() and FileAccess.get_file_as_bytes(bank_path)==bytes,"实际结果可checkpoint且原输入银行未改")
	evidence.final_identity=main.kernel.source_identity().duplicate(true)
	if not output_path.is_empty() and not final_bank.is_empty():
		var saved=FileAccess.open(output_path+".bank",FileAccess.WRITE)
		if check(saved!=null,"保本次实际Main操作后银行"): saved.store_buffer(final_bank)
	await finish(main)
