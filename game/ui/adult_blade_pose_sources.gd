## 独立完整姿态样本的原图读取与摘要锁；不作为正式资源清单或角色解析器。
extends RefCounted

## 仅按调用方显式候选读取同仓库PNG，任何缺图/坏摘要整库失败并返回具体资源键。
static func load_bank(repository_root: String, candidate_path: String) -> Dictionary:
	if not FileAccess.file_exists(candidate_path): return _failure("MissingCandidate")
	var definition: Variant = JSON.parse_string(FileAccess.get_file_as_string(candidate_path))
	if not definition is Dictionary or not definition.get("poses") is Dictionary: return _failure("InvalidCandidate")
	var images: Dictionary = {}; var hashes: Dictionary = {}
	for key in definition.poses:
		var row: Variant = definition.poses[key]
		if not row is Dictionary or not row.get("file") is String or not row.get("sha256") is String: return _failure("InvalidSource:" + str(key))
		var relative: String = row.file
		if relative.is_absolute_path() or relative.contains("..") or relative.contains("\\") or not relative.ends_with(".png"): return _failure("InvalidSourcePath:" + str(key))
		var path: String = repository_root.path_join(relative)
		if not FileAccess.file_exists(path): return _failure("MissingSource:" + str(key))
		var digest: String = FileAccess.get_sha256(path)
		if digest != row.sha256: return _failure("SourceHashMismatch:" + str(key))
		var source: Image = Image.load_from_file(path)
		if source == null or source.is_empty(): return _failure("UnreadableSource:" + str(key))
		images[key] = source; hashes[key] = digest
	return {"ok": true, "definition": definition, "images": images, "source_hashes": hashes, "candidate_sha256": FileAccess.get_sha256(candidate_path)}

## 坏源不返回半套图像，防止宿主误用旧库补齐未知身份。
static func _failure(reason: String) -> Dictionary:
	return {"ok": false, "reason": reason}
