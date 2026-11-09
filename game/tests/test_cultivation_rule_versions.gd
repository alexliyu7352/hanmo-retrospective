## 仅核普通修业新身份及原2版本列表的纯元信息拒绝；不读写存档正文，不执行迁移或课程。
extends SceneTree
const Registry=preload("res://core/runtime/module_registry.gd")
const Builtin=preload("res://core/runtime/builtin_modules.gd")
const Resolver=preload("res://core/content_set_resolver.gd")
const Strict=preload("res://core/strict_json.gd")
const CURRENT="f10-cultivation-3"
const PREVIOUS="f10-cultivation-2"
var checks: int=0
var failures: int=0

## 延后运行身份小门，不新建世界、执行课时或加载Codec正文。
func _init() -> void:
	call_deferred("_run")

## 每项明确计数并保留错误标签，未满足身份前置必须非零退出。
func check(value: bool, label: String) -> void:
	checks+=1
	if not value: failures+=1; printerr("FAIL: ",label)

## 沿真实Builtin依赖与既有Resolver身份比较，旧完整模块列表保持原字节语义而不重贴新号。
func _run() -> void:
	var registry: Dictionary=Registry.build(Builtin.descriptors())
	check(registry.get("ok",false),"真实Builtin与受信能力描述一致")
	if not registry.get("ok",false): quit(1); return
	check(registry.modules.cultivation.version==CURRENT,"普通修业终态成长使用新cultivation3身份")
	check(registry.modules.personal.version=="f10-combat-7" and registry.modules.military.version=="f10-combat-8","个人7和军事8的独立规则身份保持")
	var dependents: Array=[]; var dependencies_current: bool=true
	for id in registry.modules:
		for field in ["requires","optional_requires"]:
			if not registry.modules[id][field].has("cultivation"): continue
			dependents.append(id+"/"+field)
			dependencies_current=dependencies_current and registry.modules[id][field].cultivation==CURRENT
	dependents.sort()
	check(dependencies_current and dependents==["builtin/requires","catalog/requires","personal/requires"],"原三个真实依赖者均要求同一cultivation3，不遗失或绕过依赖")
	var projection: Dictionary=Resolver._rule_modules(registry)
	var parsed: Dictionary=Strict.parse_text(FileAccess.get_file_as_string("res://../tests/fixtures/saves/a2/resolver-contract.json"))
	check(projection.ok and parsed.ok and parsed.value.get("kind")=="synthetic-resolver-boundary-contract","真实身份投影与明确合成的纯元信息夹具可用")
	if not projection.ok or not parsed.ok: quit(1); return
	var current_modules: Array=projection.modules.duplicate(true)
	var catalog: Dictionary=parsed.value.catalog
	var build: String=Engine.get_version_info().string
	var metadata: Dictionary={"format_version":2,"content_id":catalog.content_id,"content_version":catalog.content_version,"content_digest":catalog.content_digest,"engine_build":build,"rng_backend":"godot:"+build,"scenario_id":"scenario.test","player_actor_id":"actor.test","pack_set":catalog.pack_set,"pack_set_digest":catalog.pack_set_digest,"rule_modules":current_modules.duplicate(true)}
	check(Resolver._metadata(metadata,registry).ok,"当前完整模块列表通过原纯身份门")
	var old: Dictionary=metadata.duplicate(true)
	for row in old.rule_modules:
		if row.id=="cultivation": row.version=PREVIOUS
	var before: Dictionary=old.duplicate(true)
	check(Resolver._metadata(old,registry).get("code")=="IncompatibleRuleModules","原cultivation2完整身份列表明确拒绝")
	check(old==before,"拒绝不改原列表、内容锁或元信息，不自动改贴新身份")
	check(Resolver._rule_modules(registry)==projection and metadata.rule_modules==current_modules,"身份检查不改当前注册版本或调用方新列表")
	print("CULTIVATION_VERSION_CHECKS=",checks," FAILURES=",failures)
	quit(0 if failures==0 else 1)
