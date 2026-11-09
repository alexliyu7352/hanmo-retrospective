extends SceneTree
## 实际请求生成器须与原生Affairs版本一致；本门只测四个显式作者组合，不创建世界。
var _checks: int = 0
var _failures: int = 0

## 以独立只读脚本路径核原红与候选，默认读取本工程正式Backend。
func _initialize() -> void:
	var backend_path := "res://ui/native_resident_backend.gd"
	for argument in OS.get_cmdline_user_args():
		if argument.begins_with("--backend-script="):
			backend_path = argument.substr("--backend-script=".length())
	var backend = load(backend_path)
	_check(backend != null, "正式Backend可载入")
	if backend == null:
		quit(1)
		return
	for flags in [[false, false], [true, false], [false, true], [true, true]]:
		var affairs: Dictionary = {"need_policies": {}}
		if flags[0]:
			affairs["obligation_needs_version"] = 1
			affairs["obligation_policies"] = {}
		if flags[1]:
			affairs["fiscal_need_policies"] = {"version": 1, "policies": {}}
		var catalog: Dictionary = {"content_id": "test", "content_version": "1", "content_digest": "test", "schema_sha256": "test", "data": {"roles": {}, "scenarios": {"case": {"seed_text": "fixed", "dynamic_world": {"affairs": affairs}}}}}
		var before := JSON.stringify(catalog, "", false, true)
		var request: Dictionary = backend.native_request(catalog, "case", "actor")
		var expected: Array = [{"name": "dynamic_world", "version": "local_society_v1"}, {"name": "local_economy", "version": "1"}, {"name": "organization", "version": "2"}, {"name": "affairs", "version": "3" if flags[1] else ("2" if flags[0] else "1")}, {"name": "director", "version": "1"}]
		if flags[0]:
			expected.append({"name": "obligation_needs", "version": "1"})
		if flags[1]:
			expected.append({"name": "fiscal_need_policies", "version": "1"})
		_check(request.expected_catalog_identity.module_versions == expected, "模块及顺序符合实际原生组合 %s" % [flags])
		_check(JSON.stringify(catalog, "", false, true) == before, "目录原字节形状未改 %s" % [flags])
		_check(request.founder_selection == "actor" and request.scenario_id == "case", "本人和情景选择保留 %s" % [flags])
	print("FISCAL_NEED_NATIVE_REQUEST %d checks %d failures; WORLD=0" % [_checks, _failures])
	quit(1 if _failures else 0)

## 有限断言保留明确失败文本，原红不能由后续脚本覆盖。
func _check(ok: bool, detail: String) -> void:
	_checks += 1
	if not ok:
		_failures += 1
		print("FAIL: " + detail)
