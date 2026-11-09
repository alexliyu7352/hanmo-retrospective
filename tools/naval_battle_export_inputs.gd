## 从实际Installed目录读取并构造同一玩家正式NativeRequest；不预建军团、敌舰或接敌结果。
extends SceneTree
const Catalog=preload("res://core/content_catalog.gd")
const Backend=preload("res://ui/native_resident_backend.gd")
## 原准入失败即停，保原错误，不补module版本或改摘要。
func _initialize() -> void:
	var args=OS.get_cmdline_user_args()
	if args.size()!=2 and args.size()!=3: quit(2);return
	var catalog: Dictionary=Catalog.read_prepared(args[0])
	if not catalog.get("ok",false): printerr(JSON.stringify(catalog));quit(1);return
	var scenario_id: String=str(args[2]) if args.size()==3 else "riverwatch::scenario.inland_battle"
	var request: Dictionary=Backend.native_request(catalog,scenario_id,"qingxi::actor.player")
	if not request.get("ok",true): printerr(JSON.stringify(request));quit(1);return
	if DirAccess.make_dir_recursive_absolute(args[1])!=OK: quit(2);return
	for name in ["catalog","request"]:
		var file=FileAccess.open(args[1].path_join(name+".json"),FileAccess.WRITE)
		if file==null: quit(2);return
		file.store_string(JSON.stringify(catalog if name=="catalog" else request,"",false,true)+"\n");file.close()
	print("NAVAL_INSTALLED_INPUTS=1 WORLDS_CREATED=0")
	quit(0)
