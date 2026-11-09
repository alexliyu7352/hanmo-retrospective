## 从既有分层夹具及Python运行时候选验证语义绑定；不维护第二份清单或图片。
extends SceneTree
const Registry = preload("res://ui/presentation_registry.gd")
var checks: int = 0
var failures: int = 0

## 延迟到资源系统就绪；共享候选路径由本模块的Python测试生成并传入。
func _init() -> void:
	call_deferred("_run")

## 记录断言并以非零退出保留所有失败证据。
func check(ok: bool, label: String) -> void:
	checks += 1
	if not ok:
		failures += 1
		printerr("FAIL: ", label)

## 独立运行也验证最小合法绑定；完整边界候选由同名Python测试复用。
func _run() -> void:
	var path: String = ProjectSettings.globalize_path("res://").path_join("../tests/fixtures/presentation/layered-v2.json")
	var base: Dictionary = load("res://core/strict_json.gd").parse_text(FileAccess.get_file_as_string(path)).value
	var registry = Registry.new()
	check(registry.configure(base).ok, "既有V0.1夹具")
	var data: Dictionary = base.duplicate(true)
	data.character_profiles["fixture.person"].age_variants[0]["feature_bindings"] = [{"id": "body", "features": {"build": "stocky"}, "component_id": "child.body"}]
	check(registry.configure(data).ok, "已有组件可作同槽语义绑定")
	check(registry.validate_composition(data.character_profiles["fixture.person"]).ok, "公开组合入口接受绑定")
	var args: PackedStringArray = OS.get_cmdline_user_args()
	if not args.is_empty():
		var cases: Variant = JSON.parse_string(FileAccess.get_file_as_string(args[0]))
		check(cases is Array and not cases.is_empty(), "完整共享候选已加载")
		if cases is Array:
			for entry in cases:
				entry.data = load("res://core/strict_json.gd").parse_text(JSON.stringify(entry.data)).value
				check(registry.configure(base).ok, "重置 " + entry.name)
				var before: Dictionary = entry.data.duplicate(true)
				var texture: Texture2D = registry.texture("portraits", "fixture.child")
				var result: Dictionary = registry.configure(entry.data)
				check(result.ok == entry.ok, "候选 " + entry.name + " " + str(result))
				check(entry.data == before, "输入未修改 " + entry.name)
				if not entry.code.is_empty():
					check(result.get("code") == entry.code and result.get("path") == entry.path, "诊断 " + entry.name + " " + str(result))
				if entry.ok:
					check(registry.describe() == entry.data, "接受源形状 " + entry.name)
					check(registry.validate_composition(entry.data.character_profiles["fixture.person"]).ok, "公开组合 " + entry.name)
				else:
					check(registry.describe() == base, "失败保留快照 " + entry.name)
					check(registry.texture("portraits", "fixture.child") == texture, "失败保留纹理 " + entry.name)
					var changed: Dictionary = entry.data.character_profiles["fixture.person"]
					if changed != base.character_profiles["fixture.person"]:
						check(not registry.validate_composition(changed).ok, "公开入口同样拒绝 " + entry.name)
	print("FEATURE_BINDING_CHECKS=", checks, " FAILURES=", failures)
	quit(0 if failures == 0 else 1)
