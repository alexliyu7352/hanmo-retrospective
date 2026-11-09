## 新局确认重读跨局资格；不创建人物，不写世界或已有资格文件。
extends SceneTree
const Start=preload("res://ui/actor_unlock_start.gd")
var checks: int=0
var failures: int=0
func check(value: bool,label: String) -> void:
	checks+=1
	if not value: failures+=1;printerr("FAIL "+label)
func _initialize() -> void:
	var scenario: Dictionary={"player_actor":"a","dynamic_world":{"founder_policy":"selected_existing_actor_roster_v1"},"actor_card_unlocks":{"profile":"completed_procurement_actor_cards_v1","initially_available_actor_ids":["a"]}}
	var path: String="user://tests/actor-start-gate-"+str(OS.get_process_id())+"-"+str(Time.get_ticks_usec())+".json"
	DirAccess.make_dir_recursive_absolute(ProjectSettings.globalize_path("user://tests"))
	check(Start.creation_mode(scenario)=="standard","现存人物是正式普通人生来源")
	var empty: Dictionary=Start.request_qualification(scenario,"a",path)
	check(empty.get("ok",false) and empty.unlocked_actor_ids.is_empty(),"未有资格文件仍可选原初始人物")
	var file=FileAccess.open(path,FileAccess.WRITE);file.store_string("broken qualification");file.close()
	var original: String=FileAccess.get_file_as_string(path)
	var blocked: Dictionary=Start.request_qualification(scenario,"b",path)
	check(not blocked.get("ok",true) and blocked.get("code")=="InvalidActorUnlockProfile","确认期间文件损坏保留原错误")
	var baseline: Dictionary=Start.request_qualification(scenario,"a",path)
	check(baseline.get("ok",false) and not baseline.profile_available and baseline.unlocked_actor_ids.is_empty(),"坏收藏文件不封锁原初始人物")
	check(FileAccess.get_file_as_string(path)==original,"失败不重写资格原件")
	scenario.dynamic_world.founder_policy="free_created_actor_v1"
	check(Start.creation_mode(scenario).is_empty() and not Start.request_qualification(scenario,"a",path).get("ok",true),"未接自由创建政策不记作普通来源")
	scenario.dynamic_world.erase("founder_policy")
	check(Start.creation_mode(scenario).is_empty(),"未知人生来源不默认普通")
	print("ACTOR_UNLOCK_START_GATE ",checks," checks ",failures," failures; WORLD_NOT_CREATED")
	quit(1 if failures else 0)
