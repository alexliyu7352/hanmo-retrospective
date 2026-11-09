## 真实namespaced安装目录的新局返回标题；只验证节点纹理与中文确认，不冒充OS或视觉验收。
extends SceneTree
const Host=preload("res://ui/main.gd")
const Catalog=preload("res://core/content_catalog.gd")
const Art=preload("res://ui/art_catalog.gd")
const EXPECTED_LOCK="5b2a2862bedf9cbf6dc24ecba13d1608468fc4aaad002090d35bca1d437261a2"
var checks: int=0
var failures: int=0
var host

## 等完整UI宿主可入树后执行真实返回标题回调。
func _init() -> void:
	call_deferred("run")

## 所有断言失败记录并造成非零退出，包括标题纹理缺失。
func check(ok: bool, label: String) -> void:
	checks+=1
	if not ok: failures+=1; printerr("FAIL: ",label)

## 递归摘要原安装文件；Godot导入侧文件不属于composer/stage原输入。
func installed_hashes(path: String="res://data/generated") -> Dictionary:
	var hashes: Dictionary={}
	for name in DirAccess.get_files_at(path):
		if not name.ends_with(".import"): hashes[path.path_join(name)]=FileAccess.get_sha256(path.path_join(name))
	for name in DirAccess.get_directories_at(path): hashes.merge(installed_hashes(path.path_join(name)))
	return hashes

## 使用真实Kernel新局后经实际确认按钮返回，标题必须使用当前清单的default_background。
func run() -> void:
	if not ProjectSettings.get_setting("application/config/restore_menu_isolated",false):
		printerr("FAIL: requires isolated namespaced fixture"); quit(1); return
	var hashes: Dictionary=installed_hashes()
	var installed: Dictionary=Catalog.installed_catalogs()
	check(installed.get("ok",false) and installed.get("catalogs",[]).size()==1,"真实双playable compose/stage目录完整验证")
	if not installed.get("ok",false) or installed.get("catalogs",[]).size()!=1: finish(); return
	var catalog: Dictionary=installed.catalogs[0]
	check(catalog.pack_set_digest==EXPECTED_LOCK,"复用原双playable固定锁，安装原输入未变化")
	var art: Dictionary=Art.prepare_content(catalog)
	check(art.get("ok",false),"真实受信namespaced表现注册表通过")
	if not art.get("ok",false): printerr(art); finish(); return
	Art.commit_content(art)
	var default_background: String=Art.catalog().describe().get("default_background","")
	check(default_background=="menu::city" and not "city" in Art.catalog().ids("backgrounds") and not "title_landscape" in Art.catalog().ids("backgrounds"),"fixture只提供namespace背景且无旧city/title别名")
	ProjectSettings.set_setting("application/config/storage_profile","a2-title-catalog-"+str(OS.get_process_id())+"-"+str(Time.get_ticks_usec()))
	host=Host.new(); root.add_child(host)
	await process_frame
	var started: Dictionary=host.start_content("menu::scenario.west",catalog.catalog_path,{},"menu::actor.partner")
	check(started.get("ok",false) and host._started,"真实Main调用真实Kernel创建指定人物世界")
	if not started.get("ok",false): printerr(started); finish(); return
	var state: Dictionary=host.kernel.snapshot()
	var content: Dictionary=host.kernel.content()
	var kernel=host.kernel
	host._title_prompt()
	var dialog: ConfirmationDialog
	for child in host.get_children():
		if child is ConfirmationDialog: dialog=child; break
	check(dialog!=null,"真实Main创建返回标题确认框")
	if dialog==null: finish(); return
	check(dialog.get_ok_button().text=="返回标题","返回标题确认按钮为中文")
	check(dialog.get_cancel_button().text=="取消","返回标题取消按钮为中文")
	dialog.get_ok_button().pressed.emit()
	await process_frame
	var title=host.find_child("TitleStage",true,false)
	check(not host._started and title!=null,"实际确认按钮回调重建标题节点")
	check(title!=null and title.visual_tag==default_background,"返回标题背景ID等于当前清单default_background")
	check(title!=null and title._art!=null and title._art==Art.background(default_background),"返回标题真实纹理非空且对应当前清单默认图")
	check(host.kernel==kernel and host.kernel.snapshot()==state and host.kernel.content()==content,"返回标题完整world/catalog/RNG不变")
	check(installed_hashes()==hashes,"新局与返回标题未改安装原字节")
	finish()

## 释放实际Host，输出此窄门范围；日志由调用者仅保留本地。
func finish() -> void:
	if is_instance_valid(host): host.queue_free()
	print("TITLE_CATALOG_CHECKS=",checks," FAILURES=",failures,"; OS_INPUT=NOT_RUN; VISUAL_ACCEPTANCE=NOT_RUN")
	quit(0 if failures==0 else 1)
