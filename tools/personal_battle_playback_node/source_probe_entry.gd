## 已产出真实公开页的离线来源反例入口；不加载银行、不建世界、不重复结算任何一轮。
extends SceneTree
const Probe=preload("source_boundaries.gd")
const Catalog=preload("res://core/content_catalog.gd")
const Strict=preload("res://core/strict_json.gd")

## 延后到SceneTree就绪，仅从显式文件读取公开页并保留整数原类型。
func _initialize() -> void:
	call_deferred("run")

## 使用原产品Strict解析与整型边界；公开页仅作测试输入，不给产品恢复或写世界。
func run() -> void:
	var args: PackedStringArray=OS.get_cmdline_user_args()
	if args.size()!=4: quit(2); return
	var before: Dictionary=Strict.parse_text(FileAccess.get_file_as_string(args[0]))
	var after: Dictionary=Strict.parse_text(FileAccess.get_file_as_string(args[1]))
	if not before.get("ok",false) or not after.get("ok",false): quit(2); return
	var previous: Variant=before.value
	var current: Variant=after.value
	var catalog: Dictionary=Catalog.read_prepared(str(ProjectSettings.get_setting("application/config/default_catalog","")))
	if not previous is Dictionary or not current is Dictionary or not catalog.get("ok",false): quit(2); return
	var result: Dictionary=Probe.verify(self,previous,current,catalog.data)
	result["refcounted_instance_id_probe"]=RefCounted.new().get_instance_id()
	result["input_kind"]=args[3]
	var file=FileAccess.open(args[2],FileAccess.WRITE)
	if file==null: quit(2); return
	file.store_string(JSON.stringify(result,"",false,true)+"\n"); file.close()
	quit(0 if result.get("ok",false) else 1)
