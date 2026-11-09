## 仅核战斗版本的真实注册与旧v2元信息拒绝；不读写槽、不替代A4迁移或完整恢复。
extends SceneTree
const Registry=preload("res://core/runtime/module_registry.gd")
const Builtin=preload("res://core/runtime/builtin_modules.gd")
const Resolver=preload("res://core/content_set_resolver.gd")
const Strict=preload("res://core/strict_json.gd")
var checks: int=0
var failures: int=0

## 延后执行纯元信息门，不构造世界或触发玩法时钟。
func _init() -> void:
	call_deferred("_run")

## 明确输出每条失败并用非零退出码阻断。
func check(value: bool, label: String) -> void:
	checks+=1
	if not value: failures+=1; printerr("FAIL: ",label)

## 从既有明确合成的resolver夹具取内容锁，只替换真实发行模块列表测试版本选择。
func _run() -> void:
	var registry: Dictionary=Registry.build(Builtin.descriptors())
	check(registry.get("ok",false),"真实Builtin与能力版本严格匹配")
	if not registry.get("ok",false): quit(1); return
	for id in ["personal","military"]:
		check(registry.modules[id].version==({"personal":"f10-combat-7","military":"f10-combat-8"}[id]),"战斗域新行为使用新模块身份 "+id)
	var source: String=FileAccess.get_file_as_string("res://../tests/fixtures/saves/a2/resolver-contract.json")
	var parsed: Dictionary=Strict.parse_text(source)
	check(parsed.ok and parsed.value.kind=="synthetic-resolver-boundary-contract","只用明确合成的元信息合同输入")
	if not parsed.ok: quit(1); return
	var catalog: Dictionary=parsed.value.catalog
	var build: String=Engine.get_version_info().string
	var metadata: Dictionary={"format_version":2,"content_id":catalog.content_id,"content_version":catalog.content_version,"content_digest":catalog.content_digest,"engine_build":build,"rng_backend":"godot:"+build,"scenario_id":"scenario.test","player_actor_id":"actor.test","pack_set":catalog.pack_set,"pack_set_digest":catalog.pack_set_digest,"rule_modules":Resolver._rule_modules(registry).modules}
	check(Resolver._metadata(metadata,registry).ok,"写入所用真实模块投影通过同一元信息门")
	for id in ["personal","military"]:
		var old: Dictionary=metadata.duplicate(true)
		for row in old.rule_modules:
			if row.id==id: row.version={"personal":"f10-combat-6","military":"f10-combat-7"}[id]
		var before: Dictionary=old.duplicate(true)
		check(Resolver._metadata(old,registry).get("code")=="IncompatibleRuleModules","旧版本严格拒绝 "+id)
		check(old==before,"拒绝不改原元信息 "+id)
	print("COMBAT_VERSION_CHECKS=",checks," FAILURES=",failures)
	quit(0 if failures==0 else 1)
