## 在空工程只读核实际resident导出包；不加载Main、不创建世界或调用存档。
extends SceneTree

## 每个原始JSON摘要与导入资源均须在PCK内存在，缺件不由源工作区补齐。
func _initialize() -> void:
	var args := OS.get_cmdline_user_args()
	if args.size() != 3 or not ProjectSettings.load_resource_pack(args[0], true):
		push_error("Resident export package could not be mounted")
		quit(1)
		return
	var expected: Variant = JSON.parse_string(FileAccess.get_file_as_string(args[1]))
	if not expected is Dictionary or not expected.get("files") is Array or not expected.get("resources") is Array:
		push_error("Resident package inspection input is invalid")
		quit(1)
		return
	var rows: Array = []
	var resources: Array = []
	var valid: bool = true
	var installed_script: Variant = load("res://core/installed_content.gd")
	var installed: Dictionary = {"ok": false, "code": "MissingInstalledContentReader"}
	if installed_script != null:
		installed = installed_script.read(expected.catalog_path)
	valid = installed.get("ok", false) and installed.get("pack_set_digest", "") == expected.pack_set_digest
	for row in expected.files:
		var digest: String = FileAccess.get_sha256(row.path) if FileAccess.file_exists(row.path) else ""
		rows.append({"path": row.path, "sha256": digest})
		valid = valid and digest == row.sha256
	for path in expected.resources:
		if ResourceLoader.exists(path):
			resources.append(path)
		else:
			valid = false
	var output := FileAccess.open(args[2], FileAccess.WRITE)
	if output == null:
		push_error("Resident package inspection output is unavailable")
		quit(1)
		return
	var installed_summary: Dictionary = {"ok": installed.get("ok", false), "code": installed.get("code", ""),
		"path": installed.get("path", ""), "pack_set_digest": installed.get("pack_set_digest", "")}
	output.store_string(JSON.stringify({"ok": valid, "files": rows, "resources": resources, "installed_content": installed_summary}) + "\n")
	output.close()
	quit(0 if valid else 1)
