## 给标准Godot导出补入完整installed原始成员；原PNG字节不能由导入纹理替代。
@tool
extends EditorExportPlugin
const InstalledContent = preload("res://core/installed_content.gd")

## 固定插件名称只用于标准导出器区分当前规则。
func _get_name() -> String:
	return "HanmoResidentInstalledContent"

## 每个原始成员先核摘要，再用标准add_file写入PCK；隐藏marker同样必须保留。
func _export_begin(_features: PackedStringArray, is_debug: bool, _path: String, _flags: int) -> void:
	if not is_debug:
		push_error("Resident Bridge requires the reviewed template_debug ABI")
		return
	var listed: Dictionary = InstalledContent.index()
	if not listed.ok:
		push_error("Resident installed index is invalid: " + str(listed))
		return
	for entry in listed.sets:
		var directory: String = "res://data/generated/" + entry.directory + "/"
		var verified: Dictionary = InstalledContent.read(directory + "catalog.hmcatalog.json")
		if not verified.ok:
			push_error("Resident installed generation is invalid: " + str(verified))
			return
		for row in entry.files:
			var member: String = directory + row.path
			if not FileAccess.file_exists(member) or FileAccess.get_sha256(member) != row.sha256:
				push_error("Resident installed member changed: " + member)
				return
			add_file(member, FileAccess.get_file_as_bytes(member), false)
