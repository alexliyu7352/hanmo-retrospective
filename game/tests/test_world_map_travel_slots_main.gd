## 原关渡世界返程途中经真实新磁盘槽保存；另进程标题冷读继续，不注入位置或旅行状态。
extends "test_world_map_company_formed_main.gd"
var run_mode: String="produce"
var evidence_dir: String=""
var physical_saved: bool=false
var physical_restored: bool=false

## 各进程只新增证据，不覆盖首次失败或既存存档。
func artifact(name: String,text: String) -> bool:
	var path: String=evidence_dir.path_join(name)
	if not check(not FileAccess.file_exists(path),"原件路径尚不存在："+name):return false
	var file=FileAccess.open(path,FileAccess.WRITE)
	if not check(file!=null,"可写新原件："+name):return false
	file.store_string(text);file.close();return true

## 调试导出只用于逐值核存储前后世界，从不作为恢复输入。
func record_state(main: Node,name: String) -> Dictionary:
	var result: Dictionary=main.kernel.transport().debug_export_state()
	if not check(result.get("ok",false) and result.get("json") is String,"原世界可纯读导出："+name):return {}
	if not artifact(name,result.json):return {}
	var parsed: Dictionary=StrictJSON.parse_text(result.json)
	return parsed.get("value",{})

## 标题状态没有现行World，单独等待原磁盘候选窗口。
func slot_window(main: Node) -> bool:
	var deadline: int=Time.get_ticks_msec()+30000
	while Time.get_ticks_msec()<deadline:
		if is_instance_valid(main._slot_dialog) and main._slot_dialog.visible and main._storage_intent.is_empty():return true
		await process_frame
	return check(false,"原槽窗口超时："+main._message)

## 新隔离目录只选真实manual-1新槽，冷读仍由原标题确认办理。
func choose_slot(main: Node,saving: bool) -> bool:
	if not await slot_window(main):return false
	var choice: Button=target_button(main,"slot_select","manual-1")
	if not check(choice!=null and not choice.disabled,"原磁盘manual-1候选可用"):return false
	choice.pressed.emit()
	if saving and not check(main._slot_choice.get("new",false),"不覆盖任何既有槽"):return false
	var confirm: Button=target_button(main,"slot_confirm")
	if not check(confirm!=null and not confirm.disabled,"原选槽确认可用"):return false
	confirm.pressed.emit()
	if not await settled(main):return false
	return check(main._selected_slot=="manual-1" and main._save_generation>=1 and main._message in ["已保存当前旅途。","已恢复此前的旅途。"],"原物理槽事务成功："+main._message)

## 完成或失败均留下世界身份、命令及UI槽边界，GUI仍未测试。
func finish(main: Node) -> void:
	evidence.mode=run_mode;evidence.checks=checks;evidence.failures=failures;evidence.commands=commands
	evidence.physical_saved=physical_saved;evidence.physical_restored=physical_restored;evidence.gui="NOT_TESTED"
	evidence.user_data=ProjectSettings.globalize_path("user://")
	if is_instance_valid(main) and main._started:
		evidence.identity=main.kernel.source_identity();evidence.sidebar=main.kernel.journey_sidebar();evidence.map=main.kernel.map_view()
	if not evidence_dir.is_empty():artifact(run_mode+"-result.json",JSON.stringify(evidence,"\t",false,true)+"\n")
	if is_instance_valid(main):main.queue_free()
	await process_frame
	print("MAP_TRAVEL_SLOTS_MAIN ",run_mode," CHECKS=",checks," FAILURES=",failures," SAVED=",physical_saved," RESTORED=",physical_restored," GUI=NOT_TESTED")
	quit(0 if failures==0 else 1)

## 只续当前原行程；遇到原生自然待决即保原件停止，不能自动回应。
func advance_once(main: Node) -> bool:
	var side: Dictionary=main.kernel.journey_sidebar()
	if not check(side.travel.active and side.get("encounter",{}).is_empty(),"原行程在途且没有待决相逢"):return false
	return await press(main,"advance")

## produce恢复既有终点后返向一段并停步保存；consume另一进程标题冷读再恢复一次半日。
func _run() -> void:
	var bank: String=""
	for arg in OS.get_cmdline_user_args():
		if arg.begins_with("--mode="):run_mode=arg.trim_prefix("--mode=")
		if arg.begins_with("--bank="):bank=arg.trim_prefix("--bank=")
		if arg.begins_with("--evidence="):evidence_dir=arg.trim_prefix("--evidence=")
	if not check(run_mode in ["produce","consume"] and DirAccess.dir_exists_absolute(evidence_dir),"明确独立证据目录与模式"):quit(1);return
	if not check(not OS.get_environment("XDG_DATA_HOME").is_empty() and ProjectSettings.globalize_path("user://").begins_with(OS.get_environment("XDG_DATA_HOME")),"真实存储位于本门独立XDG"):quit(1);return
	var main=Main.new();root.add_child(main);main.size=Vector2(1280,800);await process_frame
	if run_mode=="consume":
		var expected: Dictionary=StrictJSON.parse_text(FileAccess.get_file_as_string(evidence_dir.path_join("produce-paused.json"))).get("value",{})
		var title: Button=target_button(main,"continue_dynamic")
		if not check(not main._started and title!=null and not title.disabled,"另进程标题存在实际继续入口"):await finish(main);return
		title.pressed.emit()
		if not await choose_slot(main,false):await finish(main);return
		physical_restored=true
		var restored: Dictionary=record_state(main,"consume-restored.json")
		if not check(restored==expected,"完整冷世界保原行程进度、付费、位置与停步状态"):await finish(main);return
		main._open_map()
		if not await settled(main):await finish(main);return
		var side: Dictionary=main.kernel.journey_sidebar();var before: Dictionary=main.kernel.map_view();var identity: Dictionary=main.kernel.source_identity()
		if not check(side.travel.active and side.travel.paused,"真实地图冷读仍显示旅程停步"):await finish(main);return
		if not await press(main,"resume_travel") or not await advance_once(main):await finish(main);return
		check(main.kernel.source_identity().tick==identity.tick+1 and main.kernel.map_view().observer_position!=before.observer_position,"冷读后原继续控件只推进下一半日")
		record_state(main,"consume-advanced.json");await finish(main);return
	if not check(FileAccess.file_exists(bank),"原关渡实际终点bank存在"):await finish(main);return
	var bytes: PackedByteArray=FileAccess.get_file_as_bytes(bank);evidence.input_bank_sha256=FileAccess.get_sha256(bank)
	var candidate: Dictionary=main._resident_prepare_load_candidate(bytes)
	if not check(candidate.get("ok",false) and candidate.get("restored",false),"原终点bank完整准入") or not check(main._resident_adopt_load_candidate().get("ok",false),"原Main采用终点候选"):await finish(main);return
	main._resident_frame_pending=true;main._advancing=true
	if not await settled(main):await finish(main);return
	main._open_map()
	if not await settled(main):await finish(main);return
	var canvas=main.find_child("WorldMapCanvas",true,false);canvas.destination_selected.emit("society::settlement.qingchuan")
	if not await settled(main):await finish(main);return
	var quote: Dictionary=main._map_quote();evidence.quote=quote.duplicate(true)
	if not check(quote.get("ok",false) and quote.duration>1,"原返向报价允许保在途状态") or not await press(main,"map_confirm") or not await advance_once(main):await finish(main);return
	if not await press(main,"pause_travel"):await finish(main);return
	var paused: Dictionary=record_state(main,"produce-paused.json")
	if not check(main.kernel.journey_sidebar().travel.active and main.kernel.journey_sidebar().travel.paused,"原命令保在途停步状态"):await finish(main);return
	if not await press(main,"system_menu","",false):await finish(main);return
	var save: Button=target_button(main,"save")
	if not check(save!=null and not save.disabled,"原地图系统菜单保存可用"):await finish(main);return
	save.pressed.emit()
	if not await choose_slot(main,true):await finish(main);return
	physical_saved=true
	check(record_state(main,"produce-after-save.json")==paused,"物理保存未再扣费、推进或重置旅行")
	check(FileAccess.get_file_as_bytes(bank)==bytes,"既有关渡原bank逐字节未改")
	await finish(main)
