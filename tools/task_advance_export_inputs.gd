## 从真实Prepared读取并生成正式NativeRequest；不改目录身份、不预建或伪造Task状态。
extends SceneTree
const Catalog=preload("res://core/content_catalog.gd")
const Backend=preload("res://ui/native_resident_backend.gd")

func _initialize() -> void:
	var args=OS.get_cmdline_user_args()
	if args.size()!=2:
		quit(2)
		return
	var catalog: Dictionary=Catalog.read_prepared(args[0])
	if not catalog.get("ok",false):
		printerr(JSON.stringify(catalog))
		quit(1)
		return
	var selections={"advance":"contacts::scenario.qingchuan_task_advances","old_response":"contacts::scenario.qingchuan_dynamic_responses"}
	for key in selections:
		var output: String=args[1].path_join(key)
		DirAccess.make_dir_recursive_absolute(output)
		var request: Dictionary=Backend.native_request(catalog,selections[key],"society::actor.apprentice")
		if not request.get("ok",true):
			printerr(JSON.stringify(request))
			quit(1)
			return
		for name in ["catalog","request"]:
			var file=FileAccess.open(output.path_join(name+".json"),FileAccess.WRITE)
			if file==null:
				quit(1)
				return
			file.store_string(JSON.stringify(catalog if name=="catalog" else request,"",false,true))
			file.close()
	print("2 Installed catalogs/requests exported; 0 worlds")
	quit(0)
