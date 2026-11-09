## 从空工程挂载实际导出包并记录内容字节；不得从作者工作树补齐缺失资源。
extends SceneTree

## 递归记录生成目录的全部文件；缺失目录返回空列表，由Python严格比对清单。
func collect_files(folder: String, records: Array) -> void:
	for name in DirAccess.get_files_at(folder):
		var path := folder.path_join(name)
		records.append({"path": path, "sha256": FileAccess.get_sha256(path)})
	for name in DirAccess.get_directories_at(folder):
		collect_files(folder.path_join(name), records)

## 挂载失败或证据不可写时退出非零；只读包内资源，不运行游戏规则或作者代码。
func _initialize() -> void:
	var args := OS.get_cmdline_user_args()
	if args.size() != 2 or not ProjectSettings.load_resource_pack(args[0], true):
		push_error("Content package could not be mounted")
		quit(1)
		return
	var records: Array = []
	collect_files("res://data/generated", records)
	for contract in ["res://data/rule_capabilities.json", "res://data/module_contracts.json"]:
		if FileAccess.file_exists(contract):
			records.append({"path": contract, "sha256": FileAccess.get_sha256(contract)})
	var output := FileAccess.open(args[1], FileAccess.WRITE)
	if output == null:
		push_error("Content package evidence could not be written")
		quit(1)
		return
	output.store_string(JSON.stringify({"format": "HM-CONTENT-PACKAGE/1", "files": records}) + "\n")
	output.close()
	quit(0)
