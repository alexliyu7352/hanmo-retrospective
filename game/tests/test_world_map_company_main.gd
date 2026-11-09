## 原清溪作者经正常Main安装后核本人军团有限页；不把静态军职或部队声明当成现役军团。
extends "test_world_map_presence_main.gd"
const Company=preload("res://ui/world_map_company.gd")
var evidence: Dictionary={}
var output_path: String=""

## 无论冷装或有限页失败均保实际错误与来源，不修造初始化状态使样本通过。
func finish(main: Node) -> void:
	evidence.checks=checks;evidence.failures=failures;evidence.gui="NOT_TESTED";evidence.commands=0
	if not output_path.is_empty():
		var file=FileAccess.open(output_path,FileAccess.WRITE)
		if file!=null:file.store_string(JSON.stringify(evidence,"\t",false,true)+"\n")
	if is_instance_valid(main):main.queue_free()
	await process_frame
	print("MAP_COMPANY_MAIN_CHECKS=",checks," FAILURES=",failures," SOURCE=ORIGINAL_QINGXI_AUTHOR_NATIVE_MAIN COMMANDS=0 GUI=NOT_TESTED")
	quit(0 if failures==0 else 1)

## 正常初始章节尚未受职编组；先明确核心能否接纳此原包及地图的真实空态。
func _run() -> void:
	var catalog: String=""
	for arg in OS.get_cmdline_user_args():
		if arg.begins_with("--catalog="):catalog=arg.trim_prefix("--catalog=")
		if arg.begins_with("--output="):output_path=arg.trim_prefix("--output=")
	if not check(ClassDB.class_exists("HanmoResidentBridge") and not catalog.is_empty(),"原生产扩展与清溪已安装目录就绪"):quit(1);return
	var main=Main.new();root.add_child(main);main.size=Vector2(1280,800)
	var installed: Dictionary=main.start_content("qingxi::scenario.grain_road_integration",catalog,{},"qingxi::actor.player")
	evidence.install=installed.duplicate(true)
	if not check(installed.get("ok",false),"正常Main接纳原清溪情景："+str(installed)) or not await settled(main):await finish(main);return
	var source: Dictionary=main.kernel.source_identity()
	evidence.before_identity=source;evidence.location=main.kernel.journey_sidebar().location.duplicate(true)
	main._open_map()
	if not await settled(main):await finish(main);return
	var company: Dictionary=main.get_meta(Company.META,{})
	evidence.company=company.duplicate(true)
	check(company.get("page",{}).get("ok")==true and company.get("source",{}).get("identity")==source and company.get("page",{}).get("observer_actor_id")=="qingxi::actor.player","原清溪本人有限页绑定正常安装来源")
	check(company.get("page",{}).get("army",{"missing":true}).is_empty(),"未按原军旅业务编组时没有虚构军团")
	check(main.kernel.source_identity()==source,"进入原地图未推进或改写世界")
	await finish(main)
