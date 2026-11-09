## 独立导出实际Godot兼容清单，供Python源码发现门比对；此片不宣称领域已经正式注册。
extends SceneTree
const Legacy=preload("res://core/runtime/legacy_commands.gd")
const Builtin=preload("res://core/runtime/builtin_modules.gd")
const Registry=preload("res://core/runtime/module_registry.gd")
var failures: int=0
var checks: int=0

## 记录每个清单合同断言，失败必须影响进程返回码。
func check(ok: bool, label: String) -> void:
	checks+=1
	if not ok:
		failures+=1
		push_error(label)

## 不开局、不推进世界，仅校验真实清单与既有注册边界并输出可机器比较的元数据。
func _initialize() -> void:
	var entries: Array=Legacy.descriptors()
	var seen: Dictionary={}
	var registry: Dictionary=Registry.build(Builtin.descriptors())
	check(registry.ok,"原组合根仍然可注册")
	for entry in entries:
		check(not seen.has(entry.kind),"旧命令只能拥有一个描述："+entry.kind)
		seen[entry.kind]=true
		check(entry.stage=="inventory_only" and entry.mode in ["instant","action"],"清单明确标注迁移状态与原模式")
		check(registry.get("modules",{}).has(entry.owner),"owner必须属于实际模块合同："+entry.kind)
		if registry.get("commands",{}).has(entry.kind):
			var migrated: Dictionary=registry.commands[entry.kind]
			check(migrated.owner==entry.owner and migrated.mode==entry.mode and migrated.gate_id==entry.gate_id,"已迁移项保留历史拥有权、模式与门位："+entry.kind)
		check(not entry.has("preview") and not entry.has("apply") and not entry.coordination.is_empty(),"不伪造可执行合同："+entry.kind)
	check(not Registry.build(entries).ok,"清单不能冒充可运行Registry描述")
	check(Legacy.find("unknown_kind").is_empty() and Legacy.find("*").is_empty() and Legacy.find("").is_empty(),"未知命令和通配不得兜底")
	var original: Array=Legacy.descriptors()
	entries[0].coordination.append("污染")
	entries[0].source.path="污染"
	check(Legacy.descriptors()==original,"调用者修改不能污染后续描述")
	var registered: Dictionary={}
	for kind in registry.get("commands",{}):
		var command: Dictionary=registry.commands[kind]
		registered[kind]={"owner":command.owner,"mode":command.mode,"gate_id":command.gate_id}
	print("LEGACY_COMMAND_DESCRIPTORS=",JSON.stringify({"legacy":original,"registered":registered}))
	print("MODULE_COVERAGE_CHECKS=",checks," FAILURES=",failures)
	quit(1 if failures else 0)
