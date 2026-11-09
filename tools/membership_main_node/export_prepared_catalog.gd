## 原Catalog.read_prepared只读导出给同一原生消费者；不创建Main、不安装世界、不执行玩法。
extends SceneTree
const Catalog=preload("res://core/content_catalog.gd")

## 只接受两个明确路径；输出原已校验字典，保留整数及原表顺序。
func _initialize() -> void:
	var args=OS.get_cmdline_user_args()
	if args.size()!=2: quit(2); return
	var value: Dictionary=Catalog.read_prepared(args[0])
	var file=FileAccess.open(args[1],FileAccess.WRITE)
	if file==null: quit(2); return
	file.store_string(JSON.stringify(value,"",false,true)+"\n"); file.close()
	print("PREPARED_CATALOG_OK=",value.get("ok",false)," CODE=",value.get("code",""))
	quit(0 if value.get("ok",false) else 1)
