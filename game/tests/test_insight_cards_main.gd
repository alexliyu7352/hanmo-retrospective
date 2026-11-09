## 原生四课银行→正常收藏与跨局发现→实际新局→manual-1→新进程冷读。
extends "./test_actor_card_main.gd"
const INSIGHT_SCENARIO="insights::scenario.camp_training"
const INSIGHT_ID="insights::card.vi04"
const DISCOVERY_PATH="user://profiles/card_discovery_v1.json"

## 原地图与收藏沿真实可见控件切换，避免被顶层面板遮住的菜单调用。
func open_insights(main: Node) -> bool:
	if button(main,"map_close")!=null and not await click(main,"map_close"): return false
	if not await open_cards(main): return false
	return check(main.find_child("InsightCardsSection",true,false)!=null,"原收藏页出现见闻区域")

func insight(main: Node) -> Dictionary:
	for row in main._cards_view().get("insight_cards",{}).get("cards",[]):
		if row.get("card_id")==INSIGHT_ID: return row
	return {}

## 每个进程独立保原结果；不把已通过Native四课重新执行成另一份来源。
func finish(main: Node) -> void:
	var result: Dictionary={"mode":practice_mode,"checks":checks,"failures":failures,"source_bank":source_bank,"bank_saved":bank_saved,"bank_restored":bank_restored,"profile":DISCOVERY_PATH,"user_data":ProjectSettings.globalize_path("user://"),"gui":"NOT_TESTED"}
	if is_instance_valid(main):
		if main._started: result.identity=main.kernel.source_identity();result.message=main._message
		main.queue_free()
	await process_frame
	if not evidence_dir.is_empty(): artifact(practice_mode+"-insight-main-result.json",JSON.stringify(result,"\t"))
	print("INSIGHT_CARDS_MAIN ",practice_mode," ",checks," checks ",failures," failures; BANK_SAVED=",bank_saved," BANK_RESTORED=",bank_restored," GUI=NOT_TESTED")
	quit(1 if failures else 0)

func run() -> void:
	for arg in OS.get_cmdline_user_args():
		if arg.begins_with("--insight-mode="): practice_mode=arg.trim_prefix("--insight-mode=")
		if arg.begins_with("--insight-evidence="): evidence_dir=arg.trim_prefix("--insight-evidence=")
		if arg.begins_with("--insight-bank="): source_bank=arg.trim_prefix("--insight-bank=")
	if not check(practice_mode in ["produce","consume"] and not evidence_dir.is_empty() and DirAccess.dir_exists_absolute(evidence_dir),"明确模式与独占证据目录") or not check(not OS.get_environment("XDG_DATA_HOME").is_empty() and ProjectSettings.globalize_path("user://").begins_with(OS.get_environment("XDG_DATA_HOME")),"独立XDG槽与图鉴"): quit(1);return
	var main=Main.new();root.add_child(main);main.size=Vector2(1280,800);await process_frame
	if practice_mode=="consume":
		var title: Button=button(main,"continue_dynamic")
		if not check(title!=null and not title.disabled,"新进程正式标题继续入口"): await finish(main);return
		title.pressed.emit()
		if not await choose_bank(main,false): await finish(main);return
		bank_restored=true
		var restored: Dictionary=strict_state(main,"consume-new-life.json")
		var expected: Dictionary=Strict.parse_text(FileAccess.get_file_as_string(evidence_dir.path_join("produce-new-life.json"))).get("value",{})
		check(not expected.is_empty() and restored==expected,"实际槽冷读保留新人生完整状态")
		if await open_insights(main):
			var row: Dictionary=insight(main)
			check(row.get("discovered",false) and not row.get("experienced_this_world",true) and row.get("experience")==null,"冷读仍只有跨局发现，本人未经历军营训练")
		check(FileAccess.get_file_as_string(DISCOVERY_PATH)==FileAccess.get_file_as_string(evidence_dir.path_join("earned-discovery.json")),"冷读不重写原图鉴字节")
		await finish(main);return
	if not check(not FileAccess.file_exists(DISCOVERY_PATH) and FileAccess.file_exists(source_bank),"新图鉴只从实际四课银行取得"): await finish(main);return
	var prepared: Dictionary=main._resident_prepare_load_candidate(FileAccess.get_file_as_bytes(source_bank))
	if not check(prepared.get("ok",false) and prepared.get("restored",false),"正式目录匹配与完整冷准入："+str(prepared)): await finish(main);return
	var adopted: Dictionary=main._resident_adopt_load_candidate()
	if not check(adopted.get("ok",false),"正式控制器采用实际训练完成世界"): await finish(main);return
	main._resident_frame_pending=true;main._advancing=true
	if not await settled(main) or not await open_insights(main): await finish(main);return
	var earned: Dictionary=insight(main)
	if not check(earned.get("experienced_this_world",false) and earned.get("acquired_this_world",false) and earned.get("experience",{}).get("actor_id")=="qingxi::actor.player","原收藏实际显示本人军营经历"): await finish(main);return
	check(not earned.has("learned") and not earned.has("mastery") and not earned.has("configured"),"见闻没有伪本领或配置字段")
	var profile: Dictionary=Strict.parse_text(FileAccess.get_file_as_string(DISCOVERY_PATH)).get("value",{})
	if not check(INSIGHT_ID in profile.get("discovered_card_ids",[]) and profile.get("free_discovered_card_ids",[]).is_empty(),"原提交钩子记录正常来源的跨局发现"): await finish(main);return
	if not artifact("earned-discovery.json",FileAccess.get_file_as_string(DISCOVERY_PATH)) or not artifact("earned-insight.json",JSON.stringify(earned,"\t")): await finish(main);return
	if not await return_title(main): await finish(main);return
	var start: Button=button(main,"start")
	if not check(start!=null and not start.disabled,"正式标题新局入口"): await finish(main);return
	start.pressed.emit();await process_frame
	var scenario: Button=button(main,"scenario_select",INSIGHT_SCENARIO)
	if not check(scenario!=null and not scenario.disabled,"同作者见闻情景真实控件"): await finish(main);return
	scenario.pressed.emit()
	if not await settled(main): await finish(main);return
	var fresh: Dictionary=strict_state(main,"produce-new-life.json")
	check(fresh.get("tick")==0 and fresh.get("player_actor_id")=="qingxi::actor.player" and main.kernel.source_identity().world_id!=earned.experience.world_id,"标题新局创建本人独立世界")
	if not await open_insights(main): await finish(main);return
	var row: Dictionary=insight(main)
	check(row.get("discovered",false) and not row.get("experienced_this_world",true) and row.get("experience")==null,"前世见闻仅供回看，不复制本局参训")
	if not await click(main,"growth_cards_close") or not await click(main,"system_menu"): await finish(main);return
	var save: Button=button(main,"save")
	if not check(save!=null and not save.disabled,"正式新人生保存入口"): await finish(main);return
	save.pressed.emit()
	if not await choose_bank(main,true): await finish(main);return
	bank_saved=true
	await finish(main)
