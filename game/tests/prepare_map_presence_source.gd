## 仅用既有正式Catalog/原生请求器准备地图读门输入，不造作者内容、人口或世界初态。
extends SceneTree
const Catalog=preload("res://core/content_catalog.gd")
const Backend=preload("res://ui/native_resident_backend.gd")

## 输出新目录中的真实已校验目录与请求；失败不写半份成功输入。
func _init() -> void:
	var input: String=""; var output: String=""
	var scenario: String="society::scenario.qingchuan_spring_reserve"
	for arg in OS.get_cmdline_user_args():
		if arg.begins_with("--input="): input=arg.trim_prefix("--input=")
		if arg.begins_with("--output="): output=arg.trim_prefix("--output=")
		if arg.begins_with("--scenario="): scenario=arg.trim_prefix("--scenario=")
	if input.is_empty() or output.is_empty() or DirAccess.dir_exists_absolute(output): printerr("Fresh input/output paths required"); quit(2); return
	var catalog: Dictionary=Catalog.read_prepared(input)
	if not catalog.get("ok",false): printerr("PREPARED_CATALOG_FAILED=",JSON.stringify(catalog)); quit(1); return
	if not catalog.data.get("scenarios",{}).has(scenario):printerr("Unknown authored scenario");quit(1);return
	var request: Dictionary=Backend.native_request(catalog,scenario,"society::actor.apprentice")
	if request.get("ok",true)==false:printerr("NATIVE_REQUEST_FAILED=",JSON.stringify(request));quit(1);return
	if DirAccess.make_dir_recursive_absolute(output)!=OK: printerr("Cannot create output"); quit(1); return
	for item in [["catalog.json",catalog],["request.json",request]]:
		var file=FileAccess.open(output.path_join(item[0]),FileAccess.WRITE)
		if file==null: printerr("Cannot write validated input"); quit(1); return
		file.store_string(JSON.stringify(item[1],"",false,true)); file.close()
	print("MAP_PRESENCE_SOURCE=PREPARED_BY_CATALOG_AND_NATIVE_REQUEST WORLD_INSTANTIATIONS=0")
	quit(0)
