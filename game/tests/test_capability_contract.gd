## 用合法重算摘要覆盖目录的运行语义拒绝；不把目录匹配当作规则通过。
extends SceneTree
const Catalog = preload("res://core/content_catalog.gd")
const Contract = preload("res://core/runtime/capability_contract.gd")
const Strict = preload("res://core/strict_json.gd")
var failures: Array[String] = []
var checks: int = 0

## 引擎初始化后执行真实目录边界测试。
func _init() -> void:
	call_deferred("_run")

## 保存独立断言失败并继续检查其余语义边界。
func check(value: bool, label: String) -> void:
	checks += 1
	if not value:
		failures.append(label)
		printerr("FAIL: ", label)

## 每个变体重新计算合法摘要，保证拒绝原因来自语义合同。
func _run() -> void:
	for sample in [[true,true],[false,false],[null,false],[0,false],[2,true],[0.0,false],[1.5,true],["",false],["known",true],[[],false],[[1],true],[{},false],[{"id":"known"},true]]:
		check(Contract._has_value(sample[0])==sample[1],"选择器按实际JSON类型判断："+str(sample[0]))
	if "--selector-values" in OS.get_cmdline_user_args():
		print("CAPABILITY_SELECTOR_CHECKS=",checks," FAILURES=",failures.size())
		quit(0 if failures.is_empty() else 1); return
	for source in ["procurement", "two_stories", "lived_city", "alpha_life", "journey_life"]:
		var path: String = "res://data/generated/" + source + ".hmcatalog.json"
		check(Catalog.read_prepared(path).ok, "合法作者包保持兼容：" + source)
	var original: Dictionary = Strict.parse_text(FileAccess.get_file_as_string("res://data/generated/two_stories.hmcatalog.json")).value
	for mutation in [
		["events", "event.letter", "trigger", "on_tick", "UNSUPPORTED_EVENT_TRIGGER"],
		["events", "event.letter", "trigger", "after_action", "UNSUPPORTED_EVENT_TRIGGER"],
		["events", "event.letter", "on_unavailable", "defer", "UNSUPPORTED_EVENT_POLICY"],
		["offers", "offer.grain", "on_unavailable", "cancel_without_reward", "UNSUPPORTED_TASK_POLICY"]
	]:
		var changed: Dictionary = original.duplicate(true)
		changed.catalog[mutation[0]][mutation[1]][mutation[2]] = mutation[3]
		changed.catalog_sha256 = (JSON.stringify(changed.catalog, "", true, true) + "\n").sha256_text()
		var rejected: Dictionary = Catalog.load_checked(JSON.stringify(changed))
		check(not rejected.ok and rejected.get("code") == mutation[4], "运行拒绝：" + mutation[3])
		check(rejected.get("record_id") == mutation[1] and rejected.get("field") == mutation[2], "原记录字段：" + mutation[3])
	var changed: Dictionary = original.duplicate(true)
	changed["rule_capabilities_sha256"] = "0".repeat(64)
	check(Catalog.load_checked(JSON.stringify(changed)).get("code") == "ContentCapabilityContractMismatch", "描述摘要须匹配")
	check(FileAccess.file_exists("res://data/rule_capabilities.json"), "单一受信描述存在")
	var trusted: Dictionary = Strict.parse_text(FileAccess.get_file_as_string("res://data/rule_capabilities.json")).value
	check(Contract.supported_ids().size() == 18, "十八项受信能力")
	for mutation in ["duplicate", "module", "tests", "script", "selector", "semantic_field", "semantic_value", "historical_verified"]:
		var candidate: Dictionary = trusted.duplicate(true)
		match mutation:
			"duplicate": candidate.capabilities.append(candidate.capabilities[0].duplicate(true))
			"module": candidate.capabilities[0].module_id = "unknown.module"
			"tests": candidate.capabilities[0].test_ids = []
			"script": candidate.capabilities[0].capability_id = "arbitrary.script"
			"selector": candidate.capabilities[0].schema_selectors = [{"path":"actors.*.not_a_field"}]
			"semantic_field": candidate.capabilities[3].supported_semantics[0].field = "missing_policy"
			"semantic_value": candidate.capabilities[3].supported_semantics[0].values = ["run_external_script"]
			"historical_verified": candidate.capabilities[0].support_status = "verified"
		check(not Contract.validate_contract(candidate).ok, "受信描述拒绝：" + mutation)
	for capability_id in ["events", "multi_actor"]:
		for mutation in ["object", "string", "null", "missing"]:
			var malformed: Dictionary = trusted.duplicate(true)
			for entry in malformed.capabilities:
				if entry.capability_id != capability_id: continue
				match mutation:
					"object": entry.supported_semantics = {}
					"string": entry.supported_semantics = ""
					"null": entry.supported_semantics = null
					"missing": entry.erase("supported_semantics")
			var rejected: Dictionary = Contract.validate_contract(malformed)
			check(rejected.get("ok", true) == false and rejected.get("code") == "InvalidCapabilitySemantics", "必须显式声明策略数组：" + capability_id + "/" + mutation)
	for identifier in Contract.supported_ids():
		var full: Dictionary = Strict.parse_text(FileAccess.get_file_as_string("res://data/generated/alpha_life.hmcatalog.json")).value
		full.catalog.required_capabilities.erase(identifier)
		full.catalog_sha256 = (JSON.stringify(full.catalog, "", true, true) + "\n").sha256_text()
		check(not Catalog.load_checked(JSON.stringify(full)).ok, "不能删去已用能力：" + identifier)
	print("CAPABILITY_CONTRACT_CHECKS=", checks, " FAILURES=", failures.size())
	quit(0 if failures.is_empty() else 1)
