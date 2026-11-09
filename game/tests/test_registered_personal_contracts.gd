## 真实个人战KINDS必须在唯一personal描述登记，不能依靠Kernel旧直通分支冒充覆盖。
extends SceneTree
const Builtin=preload("res://core/runtime/builtin_modules.gd")
const Registry=preload("res://core/runtime/module_registry.gd")
const Personal=preload("res://core/personal_combat.gd")

## 独立读取原领域集合；每条即时命令必须具备真实Callable及固定专用早门。
func _init() -> void:
	var registry: Dictionary=Registry.build(Builtin.descriptors())
	var failures: int=0
	for kind in Personal.KINDS:
		var entry: Dictionary=registry.get("commands",{}).get(kind,{})
		if entry.get("owner")!="personal" or entry.get("mode")!="instant" or entry.get("gate_id")!="personal_before_world" or not entry.get("preview") is Callable or not entry.get("apply") is Callable:
			failures+=1; printerr("FAIL: 个人战真注册 ",kind)
	print("REGISTERED_PERSONAL_CONTRACT_CHECKS=",Personal.KINDS.size()," FAILURES=",failures)
	quit(1 if failures else 0)
