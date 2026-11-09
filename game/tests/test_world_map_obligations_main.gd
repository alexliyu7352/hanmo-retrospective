## S3真实受领银行经正式Main恢复；地图只查看本人相关回报地与原截止，不重演任务业务。
extends "test_world_map_cargo_main.gd"
const Obligations=preload("res://ui/world_map_obligations.gd")

## 只收集正式消费者放入控件的原任务行及玩家可读文字。
func obligation_labels(parent: Node) -> Array:
	var rows: Array=[]
	if parent is Label and parent.has_meta("map_route_obligation"):rows.append({"row":parent.get_meta("map_route_obligation"),"text":parent.text})
	for child in parent.get_children():rows.append_array(obligation_labels(child))
	return rows

## 结果保实际来源、页面和原checkpoint纯读比较；此门不测试OS鼠标或物理槽。
func finish(main: Node) -> void:
	evidence.checks=checks;evidence.failures=failures;evidence.world_commands=0;evidence.gui="NOT_TESTED";evidence.ui_save_slot="NOT_TESTED"
	if not output_path.is_empty():
		var file=FileAccess.open(output_path,FileAccess.WRITE)
		if file!=null:file.store_string(JSON.stringify(evidence,"\t",false,true)+"\n");file.close()
	if is_instance_valid(main):main.queue_free()
	await process_frame
	print("MAP_OBLIGATIONS_MAIN_CHECKS=",checks," FAILURES=",failures," SOURCE=FORMAL_S3_ACCEPTED_BANK WORLD_COMMANDS=0 GUI=NOT_TESTED")
	quit(0 if failures==0 else 1)

## 正常准入同包银行后只选地图目的城；跨城切换不能借旧页泄漏或误套期限。
func _run() -> void:
	var bank_path: String="";var page_path: String=""
	for arg in OS.get_cmdline_user_args():
		if arg.begins_with("--bank="):bank_path=arg.trim_prefix("--bank=")
		if arg.begins_with("--expected-page="):page_path=arg.trim_prefix("--expected-page=")
		if arg.begins_with("--output="):output_path=arg.trim_prefix("--output=")
	if not check(ClassDB.class_exists("HanmoResidentBridge") and FileAccess.file_exists(bank_path) and FileAccess.file_exists(page_path),"真实同包受领bank和原期限页就绪"):quit(1);return
	var expected: Dictionary=StrictJSON.parse_text(FileAccess.get_file_as_string(page_path)).get("value",{})
	if not check(expected.get("ok",false) and expected.get("rows",[]).size()==1,"期望是所属域真实非空本人页"):quit(1);return
	var bytes: PackedByteArray=FileAccess.get_file_as_bytes(bank_path);evidence.input_bank_sha256=FileAccess.get_sha256(bank_path);evidence.expected_page_sha256=FileAccess.get_sha256(page_path)
	var main=Main.new();root.add_child(main);main.size=Vector2(1280,800)
	var candidate: Dictionary=main._resident_prepare_load_candidate(bytes)
	if not check(candidate.get("ok",false) and candidate.get("restored",false),"原Main对真实受领bank完整准入") or not check(main._resident_adopt_load_candidate().get("ok",false),"采用原正式恢复候选"):await finish(main);return
	main._resident_frame_pending=true;main._advancing=true
	if not await settled(main):await finish(main);return
	var identity: Dictionary=main.kernel.source_identity();var before: PackedByteArray=await checkpoint(main)
	main._open_map()
	if not await settled(main):await finish(main);return
	var canvas=main.find_child("WorldMapCanvas",true,false)
	if not check(canvas!=null,"实际地图存在"):await finish(main);return
	canvas.destination_selected.emit(expected.destination_settlement_id)
	if not await settled(main):await finish(main);return
	var draft: Dictionary=main.get_meta(Obligations.META,{})
	var rows: Array=obligation_labels(main);evidence.related_page=draft.duplicate(true);evidence.related_labels=rows.duplicate(true)
	if not check(draft.get("page",{}).get("rows")==expected.rows and draft.get("source",{}).get("identity")==identity,"正常注册query以当前完整来源返回原本人合同期限行"):await finish(main);return
	check(rows.size()==1 and rows[0].row==expected.rows[0] and rows[0].text.contains("青川讲舍") and rows[0].text.contains("回报完成截止") and rows[0].text.contains("须先办妥原目标") and not rows[0].text.contains("交付完成截止"),"真实家书清楚区分先亲交目标与最终回报，不把抵达当履约")
	canvas=main.find_child("WorldMapCanvas",true,false);canvas.destination_selected.emit("society::settlement.sword_yard")
	if not await settled(main):await finish(main);return
	var other: Dictionary=main.get_meta(Obligations.META,{})
	check(other.get("page",{}).get("ok",false) and other.page.destination_settlement_id=="society::settlement.sword_yard" and other.page.rows.is_empty() and obligation_labels(main).is_empty(),"别城没有原已知回报岗时不沿用旧行或推断收件人当前位置")
	evidence.other_page=other.duplicate(true)
	canvas=main.find_child("WorldMapCanvas",true,false);canvas.destination_selected.emit(expected.destination_settlement_id)
	if not await settled(main):await finish(main);return
	check(main.get_meta(Obligations.META,{}).get("page",{}).get("rows")==expected.rows,"回选本人已知城邑仍返回同一原合同")
	var close: Button=button(main,"map_close")
	if not check(close!=null and not close.disabled,"当前城中地图可正常收起"):await finish(main);return
	close.pressed.emit()
	if not await settled(main):await finish(main);return
	check(not main.has_meta(Obligations.META),"离开地图释放这代期限页")
	check(main.kernel.source_identity()==identity and await checkpoint(main)==before,"查看、换城与收起未改变完整World或原生checkpoint")
	check(FileAccess.get_file_as_bytes(bank_path)==bytes,"S3唯一原银行逐字节不变")
	await finish(main)
