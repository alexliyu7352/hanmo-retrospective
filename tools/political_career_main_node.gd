## 正式prepared政治情景的实际Main入口；由冻结stage运行，不改目录、初态或原生回执。
extends SceneTree
const Main=preload("res://ui/main.gd")
var output_path: String=""

## 只接明确的真实catalog、已冻结helper与证据路径；不设可绕过业务准入的测试开关。
func _initialize() -> void: call_deferred("run")

## 原Main/Backend自己轮询；等首个完整页面，异步尚未结束不得被算成成功空页。
func settled(main: Node) -> bool:
	var deadline: int=Time.get_ticks_msec()+30000
	while Time.get_ticks_msec()<deadline:
		if main._resident_mode and main._started and not main._advancing and not main._resident_frame_pending and not main.kernel.is_busy(): return true
		await process_frame
	return false

## 失败保真实入口和页面诊断；调用者另核进程日志中的SCRIPT ERROR，不只看退出码。
func save(value: Dictionary) -> void:
	if output_path.is_empty(): return
	var file=FileAccess.open(output_path,FileAccess.WRITE)
	if file!=null: file.store_string(JSON.stringify(value,"\t",false,true)+"\n")

## 运行一次真实现任公务流程；此门使用实际控件信号，不代表OS鼠标或像素视觉验收。
func run() -> void:
	var args=OS.get_cmdline_user_args()
	if args.size()!=3:
		printerr("Usage: -- CATALOG_PATH HELPER_PATH OUTPUT_JSON");quit(1);return
	output_path=args[2]
	ProjectSettings.set_setting("development/native_resident_world_enabled",true)
	ProjectSettings.set_setting("application/config/storage_profile","political-main-"+str(Time.get_ticks_usec()))
	if not ClassDB.class_exists("HanmoResidentBridge"):
		save({"ok":false,"code":"ActualResidentBridgeRequired"});quit(1);return
	var helper=load(args[1])
	if helper==null:
		save({"ok":false,"code":"PoliticalMainHelperMissing"});quit(1);return
	var main=Main.new();root.add_child(main)
	var installed: Dictionary=main.start_content("civicecon::scenario.qingchuan_political_economy",args[0],{},"society::actor.city_steward")
	if not installed.get("ok",false):
		save({"ok":false,"code":"ActualPoliticalMainInstallFailed","result":installed});main.queue_free();await process_frame;quit(1);return
	if not await settled(main):
		save({"ok":false,"code":"InitialPoliticalMainPageTimeout","page_error":main._resident_page_error.duplicate(true),"message":main._message});main.queue_free();await process_frame;quit(1);return
	var result: Dictionary=await helper.run(main,output_path)
	print("POLITICAL_MAIN_RESULT=",JSON.stringify({"ok":result.get("ok",false),"code":result.get("code",""),"steps":result.get("steps",[]).size(),"source":main.kernel.source_identity(),"GUI_PIXELS_ACCEPTED":false}))
	main.queue_free();await process_frame
	quit(0 if result.get("ok",false) else 1)
