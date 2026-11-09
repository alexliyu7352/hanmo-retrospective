## 必须显式提供C++来源投影原件；原件是组件DTO，不冒完整World或Main已解锁。
extends SceneTree
const Controller=preload("res://ui/actor_unlock_controller.gd")
var checks: int=0
var failures: int=0
var native_page: Dictionary={}
var source: Dictionary={}
func _init() -> void: call_deferred("run")
func check(value: bool,label: String) -> bool:
	checks+=1
	if not value: failures+=1; printerr("FAIL "+label)
	return value
func query(_args: Dictionary) -> Dictionary:
	return native_page.duplicate(true)
func run() -> void:
	var args=OS.get_cmdline_user_args()
	if args.size()!=1: printerr("需要同源C++原件native-observation.json绝对路径"); quit(2); return
	var parsed=JSON.new()
	if parsed.parse(FileAccess.get_file_as_string(args[0]))!=OK or not parsed.data is Dictionary: printerr("DTO不可读"); quit(2); return
	native_page=parsed.data; source=native_page.source_identity.duplicate(true); source.actor_id=native_page.actor_id; source.ok=true
	var path: String="user://tests/actor-native-"+str(OS.get_process_id())+"-"+str(Time.get_ticks_usec())+".json"
	var controller=Controller.new(path)
	var result: Dictionary=controller.sync_after_commit(query,func(): return source.duplicate(true),"standard")
	if not check(result.get("ok",false) and FileAccess.file_exists(path),"同源C++实际两采购DTO经控制器真实保存人物资格"):
		quit(1); return
	var target: String=native_page.cards[0].target_actor_id
	check(controller.profile.unlocked_actor_ids()==[target] and controller.profile.source_for(target).tasks[1].task_id==native_page.cards[0].acquired.tasks[1].task_id,"两条任务来源保持原生ID")
	var page: Dictionary=controller.page_view(query)
	check(page.get("cards",[]).size()==1 and page.cards[0].get("unlocked",false) and not page.cards[0].get("unlocked_free",true),"本人有限页呈现普通取得")
	var original: String=FileAccess.get_file_as_string(path)
	source.revision+=1
	check(not controller.sync_after_commit(query,func(): return source.duplicate(true),"standard").get("ok",true) and FileAccess.get_file_as_string(path)==original,"陈旧原生投影不覆盖跨局资格")
	var cold=Controller.new(path);var cold_page: Dictionary=cold.page_view(query)
	check(cold_page.cards[0].get("unlocked",false) and cold.profile.source_for(target).tasks[0].task_id==native_page.cards[0].acquired.tasks[0].task_id,"新对象从真实资格文件恢复同一原生来源")
	print("ACTOR_UNLOCK_NATIVE_OBSERVATION ",checks," checks ",failures," failures; WORLD_MAIN_NOT_RUN; profile=",path)
	quit(1 if failures else 0)
