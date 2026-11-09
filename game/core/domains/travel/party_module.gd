## 同行两个即时命令的真实注册适配；原Parties保留位置、占用、回营回执与私产规则。
extends RefCounted
const Parties=preload("res://core/party_rules.gd")
const GATE_ID="party_before_actor"

## 只取原KINDS，合入既有travel拥有权；不把即时同行手续包装成耗时行动。
static func descriptor() -> Dictionary:
	var commands: Array=[]
	for kind in Parties.KINDS:
		commands.append({"kind":kind,"mode":"instant","gate_id":GATE_ID,"preview":preview,"apply":apply})
	return {"id":"travel","commands":commands,"actions":[]}

## 未知kind先明确拒绝；已知命令保留领域ActorBusy先于中央ActorUnavailable的旧门位。
static func preview(state: Dictionary, data: Dictionary, actor_id: String, kind: String, params: Dictionary) -> Dictionary:
	if not kind in Parties.KINDS: return {"ok":false,"code":"UnsupportedAction"}
	return Parties.preview(state,data,actor_id,kind,params)

## 仅使用私有preview报价写候选；调用者不能换params转移别人的同行或财产。
static func apply(state: Dictionary, _data: Dictionary, actor_id: String, kind: String, _params: Dictionary, offer: Dictionary) -> Dictionary:
	if not kind in Parties.KINDS: return {"ok":false,"code":"UnsupportedAction"}
	return Parties.apply(state,actor_id,kind,offer)

## 原集合锚定两个必需命令，早门同时限定owner和kind；其他域不能借gate避开人物守卫。
static func command_contract_status(registry: Dictionary) -> Dictionary:
	var commands: Dictionary=registry.get("commands",{})
	for kind in Parties.KINDS:
		if not commands.has(kind): return {"ok":false,"code":"MissingBuiltinCommand","detail":kind}
		var command: Dictionary=commands[kind]
		if command.get("owner")!="travel" or command.get("mode")!="instant" or command.get("gate_id")!=GATE_ID: return {"ok":false,"code":"InvalidBuiltinCommand","detail":kind}
	for kind in commands:
		var command: Dictionary=commands[kind]
		if command.get("gate_id")==GATE_ID and (command.get("owner")!="travel" or not kind in Parties.KINDS): return {"ok":false,"code":"InvalidBuiltinCommand","detail":kind}
	return {"ok":true}
