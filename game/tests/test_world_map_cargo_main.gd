## 原生产携货检查点经Main正式候选恢复后核地图；不伪造货批或声称已验磁盘存档槽。
extends "test_world_map_presence_main.gd"
const StrictJSON=preload("res://core/strict_json.gd")
var evidence: Dictionary={}
var output_path: String=""

## 从真实控件取完整货批元数据和可见文字，数量不跨页合并。
func cargo_labels(parent: Node) -> Array:
	var rows: Array=[]
	if parent is Label and parent.has_meta("map_transport_row"):rows.append({"row":parent.get_meta("map_transport_row"),"text":parent.text})
	for child in parent.get_children():rows.append_array(cargo_labels(child))
	return rows

## 等同一原worker完成只读checkpoint；保留来源与纯读前后字节，不写存档槽。
func checkpoint(main: Node) -> PackedByteArray:
	var source: Dictionary=main.kernel.source_identity()
	var transport=main.kernel.transport()
	var accepted: Dictionary=transport.enqueue_live_operation({"kind":"checkpoint","source":source,"payload":{}})
	if not check(accepted.get("ok",false),"原worker接纳纯读检查点"):return PackedByteArray()
	var deadline: int=Time.get_ticks_msec()+30000
	while Time.get_ticks_msec()<deadline:
		var reply: Dictionary=transport.poll_live_operation(accepted.reference)
		if not reply.get("ready",false) and reply.get("ok",false):await process_frame;continue
		if not check(reply.get("ok",false) and reply.get("tag")=="checkpoint" and reply.get("source_identity")==source and reply.get("current_identity")==source and reply.get("result",{}).get("ok",false),"原检查点回执保持同源"):return PackedByteArray()
		return reply.result.get("checkpoint_bytes",PackedByteArray())
	check(false,"原worker检查点超时");return PackedByteArray()

## 原始运行页独立落证；读取与表现不写输入bank。
func finish(main: Node) -> void:
	evidence.checks=checks;evidence.failures=failures;evidence.gui="NOT_TESTED";evidence.ui_save_slot="NOT_TESTED"
	if not output_path.is_empty():
		var file=FileAccess.open(output_path,FileAccess.WRITE)
		if file!=null:file.store_string(JSON.stringify(evidence,"\t",false,true)+"\n")
	if is_instance_valid(main):main.queue_free()
	await process_frame
	print("MAP_CARGO_MAIN_CHECKS=",checks," FAILURES=",failures," SOURCE=FORMAL_S3_CHECKPOINT_NATIVE_MAIN GUI=NOT_TESTED UI_SLOT_BANK=NOT_TESTED")
	quit(0 if failures==0 else 1)

## 冷准入真实受托货批，地图展开/互斥/收起与迟到回调都不推进业务。
func _run() -> void:
	var bank_path: String="";var expected_path: String=""
	for arg in OS.get_cmdline_user_args():
		if arg.begins_with("--bank="):bank_path=arg.trim_prefix("--bank=")
		if arg.begins_with("--expected-page="):expected_path=arg.trim_prefix("--expected-page=")
		if arg.begins_with("--output="):output_path=arg.trim_prefix("--output=")
	if not check(ClassDB.class_exists("HanmoResidentBridge") and FileAccess.file_exists(bank_path) and FileAccess.file_exists(expected_path),"正式扩展及原生产携货输入就绪"):quit(1);return
	var bytes: PackedByteArray=FileAccess.get_file_as_bytes(bank_path)
	var parsed: Dictionary=StrictJSON.parse_text(FileAccess.get_file_as_string(expected_path))
	var expected: Variant=parsed.get("value")
	if not check(expected is Dictionary and expected.get("ok")==true and not expected.get("rows",[]).is_empty(),"期望页来自真实非空S3生产结果"):quit(1);return
	evidence.bank_sha256=FileAccess.get_sha256(bank_path);evidence.expected_page_sha256=FileAccess.get_sha256(expected_path)
	var main=Main.new();root.add_child(main);main.size=Vector2(1280,800)
	var prepared: Dictionary=main._resident_prepare_load_candidate(bytes)
	if not check(prepared.get("ok",false) and prepared.get("restored",false),"原Main目录匹配和完整原生checkpoint准入："+str(prepared)):await finish(main);return
	var adopted: Dictionary=main._resident_adopt_load_candidate()
	if not check(adopted.get("ok",false),"原Main采用已完整准入候选："+str(adopted)):await finish(main);return
	# 复用原加载成功后的帧刷新标志，不制造存档槽或替代世界状态。
	main._resident_frame_pending=true;main._advancing=true
	if not await settled(main):await finish(main);return
	var initial: Dictionary=main.kernel.source_identity()
	var before: PackedByteArray=await checkpoint(main)
	var draft: Dictionary=main.get_meta(Transport.META,{})
	evidence.draft=draft.duplicate(true);evidence.expected=expected;evidence.message=main._message
	if not check(draft.get("page",{}).get("ok")==true and draft.page.rows==expected.rows,"原Main读屏障恢复实际同一货批行"):await finish(main);return
	evidence.identity=initial;evidence.manifest=draft.page.duplicate(true)
	var canvas=main.find_child("WorldMapCanvas",true,false)
	var panel=main.find_child("WorldMapTransport",true,false)
	if not check(canvas!=null and panel!=null and canvas._transport_marker!=null and canvas._transport_marker.is_visible_in_tree(),"真实非空携货生成当前本人携行标记"):await finish(main);return
	check(not panel.visible,"携行清单默认收起")
	canvas._transport_marker.pressed.emit();await process_frame
	var labels: Array=cargo_labels(panel)
	var actual_rows: Array=[]
	for label in labels:actual_rows.append(label.row)
	check(panel.visible and actual_rows==draft.page.rows,"原标记展开的每行保原task与lot身份和数量")
	evidence.displayed=labels
	check(labels.size()==1 and labels[0].row.quantity==4 and labels[0].text.contains("粟米 × 4") and labels[0].text.contains("县仓补粮"),"实际县仓补粮四份粟米按原货批可读呈现")
	canvas._presence_marker.pressed.emit();await process_frame
	check(not panel.visible and main.find_child("WorldMapPresence",true,false).visible,"真实人物标记与携行清单互斥")
	canvas._transport_marker.pressed.emit();await process_frame
	var saved_open: Callable=canvas.get_signal_connection_list("transport_requested")[0].callable
	var close: Button=button(main,"map_close")
	if not check(close!=null,"实际粮店位置可返回场景"):await finish(main);return
	close.pressed.emit()
	if not await settled(main):await finish(main);return
	saved_open.call()
	check(not main.has_meta(Transport.META),"离图与已释放携行入口不会复活旧货批草稿")
	var after: PackedByteArray=await checkpoint(main)
	check(not before.is_empty() and before==after and main.kernel.source_identity()==initial,"全部地图观察与导航保持世界与checkpoint字节不变")
	check(FileAccess.get_file_as_bytes(bank_path)==bytes,"生产原bank未被本次读取改写")
	evidence.checkpoint_bytes_unchanged=before==after;evidence.commands=0
	await finish(main)
