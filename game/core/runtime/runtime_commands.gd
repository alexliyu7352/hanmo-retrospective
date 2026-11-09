## 核心推进与耗时命令的受信声明；等待复用唯一旧开工与完工，不创建第二套时钟。
extends RefCounted
const KINDS=["advance","wait"]
const ACTION_KINDS=["wait"]

## 唯一核心集合供组合根和兼容清单复用，不从内容接收新命令。
static func command_kinds() -> Array:
	return KINDS.duplicate()

## 推进的真实跨域预览由组合根固定注入；等待继续使用唯一旧开工桥。
static func descriptor(advance_preview: Callable) -> Dictionary:
	return {"commands":[
		{"kind":"advance","mode":"instant","preview":advance_preview,"apply":request_advance,"gate_id":"advance_before_actor"},
		{"kind":"wait","mode":"action","preview":preview,"apply":null,"compat_action":"legacy_begin_action","gate_id":"legacy_kernel"},
	]}

## 推进只产生精确控制请求；Kernel必须消费并返回真实世界推进结果，不能直接提交此中间回执。
static func request_advance(_state: Dictionary, _data: Dictionary, _actor_id: String, kind: String, _params: Dictionary, _offer: Dictionary) -> Dictionary:
	if kind!="advance": return {"ok":false,"code":"UnsupportedAction"}
	return {"ok":true,"control":{"advance_world":true}}

## 公共人物和占用守卫通过后报价半日；未知命令明确拒绝。
static func preview(_state: Dictionary, _data: Dictionary, _actor_id: String, kind: String, _params: Dictionary) -> Dictionary:
	return {"ok":true,"duration":1} if kind=="wait" else {"ok":false,"code":"UnsupportedAction"}

## 完整世界必须拥有推进、等待命令与原载荷校验，不允许错所有者、门位或桥类型。
static func command_contract_status(registry: Dictionary) -> Dictionary:
	var commands: Dictionary=registry.get("commands",{})
	if not commands.has("advance"): return {"ok":false,"code":"MissingBuiltinCommand","detail":"advance"}
	var advance: Dictionary=commands.advance
	if advance.get("owner")!="runtime" or advance.get("mode")!="instant" or advance.get("gate_id")!="advance_before_actor" or advance.has("compat_action"): return {"ok":false,"code":"InvalidBuiltinCommand","detail":"advance"}
	for kind in commands:
		if commands[kind].get("owner")=="runtime" and not kind in KINDS: return {"ok":false,"code":"InvalidBuiltinCommand","detail":kind}
	if not commands.has("wait"): return {"ok":false,"code":"MissingBuiltinCommand","detail":"wait"}
	var entry: Dictionary=commands.wait
	if entry.get("owner")!="runtime" or entry.get("mode")!="action" or entry.get("gate_id")!="legacy_kernel" or entry.get("compat_action")!="legacy_begin_action" or entry.get("apply")!=null: return {"ok":false,"code":"InvalidBuiltinCommand","detail":"wait"}
	if registry.get("legacy_action_validators",{}).get("wait",{}).get("owner")!="runtime": return {"ok":false,"code":"MissingBuiltinAction","detail":"wait"}
	return {"ok":true}
