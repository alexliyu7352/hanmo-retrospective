## 同源绑定/选择器/尺度样本覆盖Godot真实校验与失败时快照保留。
extends SceneTree
const Registry = preload("res://ui/presentation_registry.gd")
const Contract = preload("res://ui/presentation_contract.gd")
const Strict = preload("res://core/strict_json.gd")
var checks: int = 0
var failures: int = 0

## 资源系统就绪后执行，失败使用非零退出供Python探针记录。
func _init() -> void:
	call_deferred("_run")

## 收集所有语义断言，保留用例名方便区分Schema和运行期边界。
func check(ok: bool, label: String) -> void:
	checks += 1
	if not ok:
		failures += 1
		printerr("FAIL: ", label)

## 读取项目外共享字面量，避免两种语言悄悄维护不同合同。
func fixture(name: String) -> Dictionary:
	var path: String = ProjectSettings.globalize_path("res://").path_join("../tests/fixtures/presentation/" + name)
	return Strict.parse_text(FileAccess.get_file_as_string(path)).value

## 有限JSON指针仅修改候选副本，不写清单或当前生效资源。
func mutate(data: Dictionary, patches: Array) -> Dictionary:
	data = data.duplicate(true)
	for patch in patches:
		var parts: PackedStringArray = patch.path.trim_prefix("/").split("/")
		var node: Variant = data
		for i in range(parts.size() - 1):
			node = node[int(parts[i])] if node is Array else node[parts[i]]
		var key: Variant = int(parts[-1]) if node is Array else parts[-1]
		if patch.op == "remove": node.erase(key)
		else: node[key] = patch.value
	return data

## 检查共享样本、公开能力门、内容引用和旧清单兼容，完全不运行游戏事务。
func _run() -> void:
	var cases: Dictionary = fixture("scene-binding-cases.json")
	var base: Dictionary = fixture("layered-v2.json")
	var registry = Registry.new()
	check(registry.configure(base).ok, "原始静态分层夹具有效")
	for entry in cases.registry:
		var before: Dictionary = registry.describe()
		var result: Dictionary = registry.configure(mutate(base, entry.patches))
		check(result.ok == entry.ok, entry.name + " " + str(result))
		if not entry.code.is_empty():
			check(result.get("code") == entry.code and result.get("path") == entry.path, entry.name + " 诊断 " + str(result))
		if not entry.ok: check(registry.describe() == before, entry.name + " 保留旧快照")
	for entry in cases.selectors:
		var result: Dictionary = Contract.scene_selector_valid(entry.selector, entry.target_ref)
		check(result.ok == entry.ok, entry.name + " 公开选择器 " + str(result))
		if not entry.ok: check(result.get("code") == entry.code, entry.name + " 公开诊断 " + str(result))
	var move: Dictionary = Contract.scene_selector_params({"command_kind": "move_local", "params": {}}, {"kind": "place", "id": "west.office"})
	var inspect: Dictionary = Contract.scene_selector_params({"command_kind": "inspect", "params": {}}, {"kind": "prop", "id": "notice.grain"})
	check(move == {"ok": true, "params": {"place_id": "west.office"}} and inspect == {"ok": true, "params": {"prop_id": "notice.grain"}}, "地点与物件完整参数只取绑定真实引用")
	var talk: Dictionary = Contract.scene_selector_params({"command_kind": "talk", "params": {"role_id": "role.office"}}, {"kind": "actor", "id": "actor.chen"})
	var build: Dictionary = Contract.scene_selector_params({"command_kind": "estate_build", "params": {"facility": "farm"}}, {"kind": "estate", "id": "estate.home"})
	check(talk == {"ok": true, "params": {"role_id": "role.office"}} and build == {"ok": true, "params": {"estate_id": "estate.home", "facility": "farm"}}, "人物与建设参数精确保留唯一受支持字段")
	check(Contract.scene_selector_params({"command_kind": "open", "params": {}}, {"kind": "place", "id": "west.office"}).get("code") == "UnsupportedSceneCommand", "未来命令不能生成完整参数")
	check(Contract.scene_selector_params({"command_kind": "move_local", "params": {"place_id": "elsewhere"}}, {"kind": "place", "id": "west.office"}).get("code") == "InvalidSceneSelector", "注入其他目标参数在生成前拒绝")
	check(registry.configure(base).ok, "恢复原始静态场景")
	for entry in cases.references:
		for dictionary_rows in [false, true]:
			var rows: Variant = {entry.record.id: entry.record} if dictionary_rows else [entry.record]
			var result: Dictionary = registry.validate_content({"places": rows})
			check(result.ok == entry.ok, entry.name + " 场所尺度 " + str(result))
			if not entry.ok: check(result.get("code") == entry.code, entry.name + " 尺度诊断 " + str(result))
	var legacy: Dictionary = base.duplicate(true)
	legacy.version = 1
	for group in ["components", "character_profiles", "scene_templates"]: legacy.erase(group)
	check(registry.configure(legacy).ok, "既有本地资源的旧v1形状仍有效")
	print("SCENE_BINDING_CONTRACT_CHECKS=", checks, " FAILURES=", failures)
	quit(0 if failures == 0 else 1)
