## 三个既有内联命令的窄组合桥；真实集合来自领域KINDS，领域之间不反向依赖。
extends RefCounted
const News=preload("res://core/domains/social/news_commands.gd")
const Discovery=preload("res://core/domains/interaction/discovery_commands.gd")
const Knowledge=preload("res://core/domains/tasks/task_knowledge.gd")

## 只向指定既有拥有权追加命令；remember唯一写入口由受信组合根固定注入。
static func commands(owner: String) -> Array:
	if owner=="social": return News.descriptor().commands
	if owner=="interaction": return Discovery.descriptor(Knowledge.remember).commands
	return []

## 固定原三个命令的拥有权与普通门位，不排斥interaction并存的会面命令或观察器。
static func command_contract_status(registry: Dictionary) -> Dictionary:
	for kind in News.KINDS+Discovery.KINDS:
		if not registry.get("commands",{}).has(kind): return {"ok":false,"code":"MissingBuiltinCommand","detail":kind}
		var entry: Dictionary=registry.commands[kind]
		var owner: String="social" if kind in News.KINDS else "interaction"
		if entry.owner!=owner or entry.mode!="instant" or entry.gate_id!="legacy_kernel" or entry.has("compat_action"): return {"ok":false,"code":"InvalidBuiltinCommand","detail":kind}
	return {"ok":true}
