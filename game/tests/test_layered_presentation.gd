## 使用Python同一份字面量夹具与变异，验证候选原子加载和诊断。
extends SceneTree
const Registry = preload("res://ui/presentation_registry.gd")
var checks: int = 0
var failures: int = 0
## 延迟到资源系统就绪后运行真实图像加载。
func _init() -> void:
	call_deferred("_run")
## 记录断言并以非零退出保留失败证据。
func check(ok: bool, label: String) -> void:
	checks += 1
	if not ok:
		failures += 1
		printerr("FAIL: ", label)
## 显式定位项目外测试夹具，不复制成第二份Schema或语义样本。
func fixture(name: String) -> Variant:
	var path: String = ProjectSettings.globalize_path("res://").path_join("../tests/fixtures/presentation/" + name)
	return load("res://core/strict_json.gd").parse_text(FileAccess.get_file_as_string(path)).value
## 在独立候选应用受限JSON指针变异，保留原始样本。
func mutate(data: Dictionary, patches: Array) -> Dictionary:
	data = data.duplicate(true)
	for patch in patches:
		var parts: PackedStringArray = patch.path.trim_prefix("/").split("/")
		var node: Variant = data
		for i in range(parts.size() - 1):
			var part: String = parts[i].replace("~1", "/").replace("~0", "~")
			node = node[int(part)] if node is Array else node[part]
		var key: Variant = int(parts[-1]) if node is Array else parts[-1].replace("~1", "/").replace("~0", "~")
		if patch.op == "remove":
			if node is Array: node.remove_at(key)
			else: node.erase(key)
		else: node[key] = patch.value
	return data
## 旧快照、同候选引用、坏候选回滚与公开组合验证共同成立。
func _run() -> void:
	var registry = Registry.new()
	check(registry.configure(fixture("legacy-v1.json")).ok, "旧v1仍有效")
	var valid: Dictionary = fixture("layered-v2.json")
	var accepted: Dictionary = registry.configure(valid)
	check(accepted.ok, "同候选分层方案可用 " + str(accepted))
	if accepted.ok:
		var texture: Texture2D = registry.texture("portraits", "fixture.child")
		check(texture == registry.texture("portraits", "fixture.child-body"), "同路径别名共享纹理")
		check(registry.validate_composition(valid.character_profiles["fixture.person"]).ok, "公开接口校验当前方案")
		for entry in fixture("mutations.json"):
			var result: Dictionary = registry.configure(mutate(valid, entry.patches))
			check(not result.ok, "拒绝 " + entry.name)
			if not entry.code.is_empty():
				check(result.get("code") == entry.code and result.get("path") == entry.path, "诊断 " + entry.name + " " + str(result))
			check(registry.describe() == valid, "坏候选保留快照 " + entry.name)
			check(registry.texture("portraits", "fixture.child") == texture, "坏候选保留纹理 " + entry.name)
		check(registry.ids("character_profiles").has("fixture.second"), "第二方案可按数据枚举")
		var detached: Dictionary = registry.describe(); detached.components.clear()
		check(not registry.describe().components.is_empty(), "外部副本不可篡改")
	for entry in fixture("raw-cases.json"):
		var raw_path: String = OS.get_user_data_dir().path_join("presentation-raw-" + entry.name + ".json")
		var file = FileAccess.open(raw_path, FileAccess.WRITE)
		file.store_string(entry.get("raw", " ".repeat(entry.get("repeat", 0)))); file.close()
		check(not registry.load_from_path(raw_path).ok, "同源原始输入拒绝 " + entry.name)
		check(registry.describe() == valid, "原始输入不破坏旧清单 " + entry.name)
		DirAccess.remove_absolute(raw_path)
	for value in [NAN, INF, Vector2.ZERO, "x".repeat(1048577)]:
		var bad: Dictionary = valid.duplicate(true); bad["unsupported"] = value
		check(not registry.configure(bad).ok, "直接候选类型和大小有界")
	var oversized: Dictionary = valid.duplicate(true)
	for n in range(100): oversized.portraits["long" + str(n)] = {"path": "x".repeat(12000)}
	check(registry.configure(oversized).get("code") == "ManifestTooLarge", "直接候选总字节有界")
	for entry in fixture("positive-mutations.json"):
		check(registry.configure(mutate(valid, entry.patches)).ok, "同源合法扩展 " + entry.name)
	check(registry.configure(valid).ok, "恢复基础分层夹具")
	for table in ["places", "world_maps"]:
		var content: Dictionary = {table: [{"id": "location", "scene_template_id": "fixture.world" if table == "world_maps" else "fixture.room"}]}
		check(registry.validate_content(content).ok, "正确场景尺度 " + table)
		content[table][0].scene_template_id = "fixture.room" if table == "world_maps" else "fixture.world"
		check(registry.validate_content(content).get("code") == "SceneScale", "错误场景尺度 " + table)
	var public_bad: Dictionary = valid.character_profiles["fixture.person"].duplicate(true)
	public_bad["bad/key"] = true
	var rejected_profile: Dictionary = registry.validate_composition(public_bad)
	check(rejected_profile.get("code") == "InvalidKey" and rejected_profile.get("path") == "/bad~1key", "公开组合接口同样转义拒绝路径")
	check(registry.describe() == valid, "公开拒绝不改变接受状态")
	var empty = Registry.new()
	check(not empty.configure(mutate(valid, fixture("mutations.json")[0].patches)).ok and empty.describe().is_empty(), "首次失败保持空")
	print("LAYERED_PRESENTATION_CHECKS=", checks, " FAILURES=", failures)
	quit(0 if failures == 0 else 1)
