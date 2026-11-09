## 新首店输入导出器：只消费正式Installed信任链和原NativeBackend请求，不创建World或填运行事实。
extends SceneTree
const Installed=preload("res://core/installed_content.gd")
const Catalog=preload("res://core/content_catalog.gd")
const Backend=preload("res://ui/native_resident_backend.gd")
const SCENARIO="private_shop::scenario.first_shop"
const PLAYER="society::actor.apprentice"

## 输出必须全新；任一安装、作者或写入错误保持原诊断，既有目录不覆盖。
func _initialize() -> void:
	var args=OS.get_cmdline_user_args()
	if args.size()!=2: printerr("usage: private_shop_export_inputs.gd INSTALLED_CATALOG FRESH_OUTPUT");quit(2);return
	if DirAccess.dir_exists_absolute(args[1]) or FileAccess.file_exists(args[1]): printerr("PrivateShopExportOutputExists");quit(2);return
	var installed: Dictionary=Installed.read(args[0])
	if not installed.get("ok",false): printerr(JSON.stringify(installed));quit(1);return
	var catalog: Dictionary=Catalog.read_prepared(args[0])
	if not catalog.get("ok",false): printerr(JSON.stringify(catalog));quit(1);return
	if catalog.get("pack_set_digest","").is_empty() or catalog.get("pack_set_digest")!=installed.get("pack_set_digest"): printerr("PrivateShopInstalledSourceMismatch");quit(1);return
	if not catalog.get("data",{}).get("scenarios",{}).has(SCENARIO): printerr("PrivateShopScenarioMissing");quit(1);return
	var request: Dictionary=Backend.native_request(catalog,SCENARIO,PLAYER)
	if not request.get("ok",true): printerr(JSON.stringify(request));quit(1);return
	if request.get("scenario_id")!=SCENARIO or request.get("founder_selection")!=PLAYER or request.get("expected_catalog_identity",{}).get("pack_set_digest")!=installed.get("pack_set_digest"): printerr("PrivateShopNativeRequestSourceMismatch");quit(1);return
	if DirAccess.make_dir_recursive_absolute(args[1])!=OK: printerr("PrivateShopExportDirectoryFailed");quit(2);return
	for name in ["catalog","request"]:
		var output=FileAccess.open(args[1].path_join(name+".json"),FileAccess.WRITE)
		if output==null: printerr("PrivateShopExportWriteFailed: "+name);quit(2);return
		output.store_string(JSON.stringify(catalog if name=="catalog" else request,"",false,true)+"\n")
		output.flush()
		if output.get_error()!=OK: printerr("PrivateShopExportWriteFailed: "+name);output.close();quit(2);return
		output.close()
	print("PRIVATE_SHOP_INSTALLED_INPUTS=1 WORLDS_CREATED=0")
	quit(0)
