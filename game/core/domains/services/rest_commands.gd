## 休整两命令只注册原规则；逐半日供养、完工和取消释放仍由既有世界入口协调。
extends RefCounted
const Rest=preload("res://core/long_rest.gd")
const GATE_ID="rest_before_actor"

## 命令集合取原领域唯一KINDS，不能借适配新增供养或时间规则。
static func command_kinds() -> Array:
	return Rest.KINDS.duplicate()

## 耗时休整明确使用旧启动桥；取消只登记即时处理，不虚构生命周期回调。
static func descriptor() -> Dictionary:
	var commands: Array=[]
	for kind in Rest.KINDS:
		var entry: Dictionary={"kind":kind,"mode":"action" if kind=="long_rest" else "instant","preview":preview,"apply":null if kind=="long_rest" else apply,"gate_id":GATE_ID}
		if kind=="long_rest": entry.compat_action="legacy_begin_action"
		commands.append(entry)
	return {"commands":commands}

## 休整早门保留family能力先于原领域ActorBusy的优先级，不能转入别域命令。
static func preview(state: Dictionary, data: Dictionary, actor_id: String, kind: String, params: Dictionary) -> Dictionary:
	if not kind in Rest.KINDS: return {"ok":false,"code":"UnsupportedAction"}
	if not "family" in data.required_capabilities: return {"ok":false,"code":"FamilyUnavailable"}
	return Rest.preview(state,data,actor_id,kind,params)

## 取消仍先更新原来源事实，随后由Kernel释放原行动，不退粮也不推进世界。
static func apply(state: Dictionary, _data: Dictionary, _actor_id: String, kind: String, _params: Dictionary, offer: Dictionary) -> Dictionary:
	if kind!="rest_cancel": return {"ok":false,"code":"UnsupportedAction"}
	return Rest.cancel(state,offer.action_id)

## 固定真实集合、拥有权和早门；借门与假耗时均拒绝，不排斥services的其他正常命令。
static func command_contract_status(registry: Dictionary) -> Dictionary:
	var commands: Dictionary=registry.get("commands",{}); var legacy: Dictionary=registry.get("legacy_action_validators",{})
	for kind in Rest.KINDS:
		if not commands.has(kind): return {"ok":false,"code":"MissingBuiltinCommand","detail":kind}
		var entry: Dictionary=commands[kind]
		var mode: String="action" if kind=="long_rest" else "instant"
		if entry.get("owner")!="services" or entry.get("mode")!=mode or entry.get("gate_id")!=GATE_ID: return {"ok":false,"code":"InvalidBuiltinCommand","detail":kind}
		if kind=="long_rest":
			if entry.get("compat_action")!="legacy_begin_action" or entry.get("apply")!=null: return {"ok":false,"code":"InvalidBuiltinCommand","detail":kind}
		elif entry.has("compat_action"): return {"ok":false,"code":"InvalidBuiltinCommand","detail":kind}
	if not legacy.has("long_rest") or legacy.long_rest.get("owner")!="services": return {"ok":false,"code":"MissingBuiltinAction","detail":"long_rest"}
	for kind in commands:
		if commands[kind].get("gate_id")==GATE_ID and (commands[kind].get("owner")!="services" or not kind in Rest.KINDS): return {"ok":false,"code":"InvalidBuiltinCommand","detail":kind}
	return {"ok":true}
