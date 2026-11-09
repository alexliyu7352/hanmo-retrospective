## 同一正式stage：真实两采购bank→原Main资格钩子→标题选人→真实槽→新进程冷读。
extends "./test_growth_cards_practice_main.gd"
const ACTOR_SCENARIO="actorlife::scenario.qingchuan_procurement_life"
const TARGET="society::actor.city_steward"
const UNLOCK_PATH="user://profiles/actor_unlock_v1.json"
const Strict=preload("res://core/strict_json.gd")
var source_bank: String=""
var source_proof: String=""
var profile_preserved: bool=false

func tagged(node: Node,key: String,value: String,actor: String="") -> Button:
	if node is Button and node.is_visible_in_tree() and not node.is_queued_for_deletion() and node.get_meta(key,"")==value and (actor.is_empty() or node.get_meta("player_actor_id","")==actor): return node
	for child in node.get_children():
		var found: Button=tagged(child,key,value,actor)
		if found!=null: return found
	return null

## 导出仅核选人财物与冷读，不向World写入导出值。
func strict_state(main: Node,name: String) -> Dictionary:
	var exported: Dictionary=main.kernel.transport().debug_export_state()
	if not check(exported.get("ok",false) and exported.get("json") is String,"实际世界导出 "+name): return {}
	var parsed: Dictionary=Strict.parse_text(exported.json)
	if not check(parsed.get("ok",false),"原生完整世界严格解析"): return {}
	if not artifact(name,exported.json): return {}
	return parsed.value

func choose_actor_screen(main: Node) -> bool:
	var start: Button=button(main,"start")
	if not check(start!=null and not start.disabled,"实际标题启程控件"): return false
	start.pressed.emit(); await process_frame
	var scenario: Button=button(main,"scenario_select",ACTOR_SCENARIO)
	if not check(scenario!=null and not scenario.disabled,"正式新作者人物卡情景"): return false
	scenario.pressed.emit(); await process_frame
	return check(not main._started and is_instance_valid(main._existing_actor_picker),"先显示原人物选择页，不先建世界")

func return_title(main: Node) -> bool:
	if main._cards_open and not await click(main,"growth_cards_close"): return false
	if not await click(main,"system_menu"): return false
	var title: Button=button(main,"title")
	if not check(title!=null and not title.disabled,"实际返回标题菜单"): return false
	title.pressed.emit(); await process_frame
	var confirmation: ConfirmationDialog
	for node in main.get_children():
		if node is ConfirmationDialog and node.visible and node.title=="返回标题": confirmation=node
	if not check(confirmation!=null,"原返回标题确认窗口"): return false
	confirmation.get_ok_button().pressed.emit(); await process_frame; await process_frame
	return check(not main._started,"标题退出当前游玩，不重写跨局来源")

func finish(main: Node) -> void:
	var result: Dictionary={"mode":practice_mode,"checks":checks,"failures":failures,"bank_saved":bank_saved,"bank_restored":bank_restored,"profile_preserved":profile_preserved,"source_bank":source_bank,"gui":"NOT_TESTED","user_data":ProjectSettings.globalize_path("user://")}
	if is_instance_valid(main):
		if main._started: result.identity=main.kernel.source_identity();result.message=main._message
		main.queue_free()
	await process_frame
	if not evidence_dir.is_empty(): artifact(practice_mode+"-actor-main-result.json",JSON.stringify(result,"\t"))
	print("ACTOR_CARD_MAIN ",practice_mode," ",checks," checks ",failures," failures; BANK_SAVED=",bank_saved," BANK_RESTORED=",bank_restored," GUI=NOT_TESTED")
	quit(1 if failures else 0)

func run() -> void:
	for arg in OS.get_cmdline_user_args():
		if arg.begins_with("--actor-card-mode="): practice_mode=arg.trim_prefix("--actor-card-mode=")
		if arg.begins_with("--actor-card-evidence="): evidence_dir=arg.trim_prefix("--actor-card-evidence=")
		if arg.begins_with("--actor-card-bank="): source_bank=arg.trim_prefix("--actor-card-bank=")
		if arg.begins_with("--actor-card-proof="): source_proof=arg.trim_prefix("--actor-card-proof=")
	if not check(practice_mode in ["produce","consume"] and not evidence_dir.is_empty() and DirAccess.dir_exists_absolute(evidence_dir),"明确模式与独占证据目录") or not check(not OS.get_environment("XDG_DATA_HOME").is_empty() and ProjectSettings.globalize_path("user://").begins_with(OS.get_environment("XDG_DATA_HOME")),"槽与资格文件位于本次独立XDG"): quit(1);return
	var main=Main.new();root.add_child(main);main.size=Vector2(1280,800);await process_frame
	if practice_mode=="consume":
		if not check(not FileAccess.file_exists(UNLOCK_PATH) and FileAccess.file_exists(UNLOCK_PATH+".preserved"),"跨局资格原件已保存旁置，当前文件缺席"): await finish(main);return
		var title: Button=button(main,"continue_dynamic")
		if not check(title!=null and not title.disabled,"新进程从正式标题继续槽"): await finish(main);return
		title.pressed.emit()
		if not await choose_bank(main,false): await finish(main);return
		bank_restored=true
		var actual: Dictionary=strict_state(main,"consume-selected-world.json")
		var expected: Dictionary=Strict.parse_text(FileAccess.get_file_as_string(evidence_dir.path_join("produce-selected-world.json"))).get("value",{})
		check(not expected.is_empty() and actual==expected and actual.get("player_actor_id")==TARGET,"缺外部资格仍冷恢复原选定人物完整世界")
		check(not FileAccess.file_exists(UNLOCK_PATH),"恢复不补造前世人物解锁来源")
		await finish(main);return
	if not check(not FileAccess.file_exists(UNLOCK_PATH),"首次人物资格文件缺席") or not await choose_actor_screen(main): await finish(main);return
	var locked: Button=tagged(main,"existing_actor_action","select",TARGET)
	if not check(locked!=null and locked.disabled,"尚未完成任务时县仓书佐真实控件锁定"): await finish(main);return
	var cancel: Button=tagged(main,"existing_actor_action","cancel")
	if not check(cancel!=null,"人物选择可返回章节"): await finish(main);return
	cancel.pressed.emit();await process_frame;await process_frame
	if not check(not main._started and FileAccess.file_exists(source_bank) and FileAccess.file_exists(source_proof),"只接同源Native实际两采购原件"): await finish(main);return
	var proof: Dictionary=Strict.parse_text(FileAccess.get_file_as_string(source_proof)).get("value",{})
	if not check(proof.get("cards",[]).size()==1 and proof.cards[0].get("completed_count")==2 and proof.cards[0].get("acquired",{}).get("tasks",[]).size()==2,"实际两条已结算来源证明"): await finish(main);return
	# 使用产品完整cold准入与采用入口；这里只排入原frame，不伪造槽成功或任务状态。
	var prepared: Dictionary=main._resident_prepare_load_candidate(FileAccess.get_file_as_bytes(source_bank))
	if not check(prepared.get("ok",false) and prepared.get("restored",false),"原生bank经正式目录匹配与完整冷准入："+str(prepared)): await finish(main);return
	var adopted: Dictionary=main._resident_adopt_load_candidate()
	if not check(adopted.get("ok",false),"正式控制器采用真实完成世界"): await finish(main);return
	main._resident_frame_pending=true;main._advancing=true
	if not await settled(main) or not await open_cards(main): await finish(main);return
	var view: Dictionary=main._cards_view().get("actor_cards",{})
	if not check(view.get("cards",[]).size()==1 and view.cards[0].get("completed_count")==2 and view.cards[0].get("unlocked",false),"原Main同页显示完成与真正保存的人物资格"): await finish(main);return
	var profile: Dictionary=Strict.parse_text(FileAccess.get_file_as_string(UNLOCK_PATH)).get("value",{})
	check(TARGET in profile.get("unlocked_actor_ids",[]) and profile.get("sources",{}).get(TARGET)==proof.cards[0].acquired,"原提交frame钩子写入完整实际首次来源")
	if not artifact("earned-profile.json",FileAccess.get_file_as_string(UNLOCK_PATH)) or not await return_title(main) or not await choose_actor_screen(main): await finish(main);return
	var unlocked: Button=tagged(main,"existing_actor_action","select",TARGET)
	var confirm: Button=tagged(main,"existing_actor_action","confirm")
	if not check(unlocked!=null and not unlocked.disabled and confirm!=null,"取得资格后标题原人物名单实际开放目标"): await finish(main);return
	unlocked.pressed.emit();confirm.pressed.emit()
	if not await settled(main): await finish(main);return
	var world: Dictionary=strict_state(main,"produce-selected-world.json")
	check(world.get("player_actor_id")==TARGET and world.get("tick")==0 and main.kernel.source_identity().world_id!=proof.source_identity.world_id,"明确确认创建县仓书佐本人的新人生")
	if not await open_cards(main): await finish(main);return
	view=main._cards_view().get("actor_cards",{})
	check(view.get("cards",[]).size()==1 and view.cards[0].get("completed_count")==0 and view.cards[0].get("acquired")==null and view.cards[0].get("unlocked",false),"前世资格不复制本人任务成果或本局取得")
	if not await click(main,"growth_cards_close") or not await click(main,"system_menu"): await finish(main);return
	var save: Button=button(main,"save")
	if not check(save!=null and not save.disabled,"真实新人生保存入口"): await finish(main);return
	save.pressed.emit()
	if not await choose_bank(main,true): await finish(main);return
	bank_saved=true
	# 保留原件而非删除；后继进程由正常标题读取原槽，证明cold不重读人物资格。
	var absolute: String=ProjectSettings.globalize_path(UNLOCK_PATH)
	if not check(not FileAccess.file_exists(UNLOCK_PATH+".preserved"),"资格旁置目标为新路径"): await finish(main);return
	profile_preserved=DirAccess.rename_absolute(absolute,absolute+".preserved")==OK
	check(profile_preserved and not FileAccess.file_exists(UNLOCK_PATH),"资格文件原字节旁置保留")
	await finish(main)
