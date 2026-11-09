## 庄园八命令的受信适配；只转接真实经营规则，建设仍保留既有行动与生产先后。
extends RefCounted
const Estate=preload("res://core/estate_rules.gd")

## 直接公开原规则集合副本，适配器不能自行增加庄园玩法。
static func command_kinds() -> Array:
	return Estate.KINDS.duplicate()

## 建设明确登记旧行动桥；不以瞬时成功伪装耗时，不另建第二份生命周期。
static func descriptor() -> Dictionary:
	var commands: Array=[]
	for kind in Estate.KINDS:
		var entry: Dictionary={"kind":kind,"mode":"action" if kind=="estate_build" else "instant","preview":preview,"apply":null if kind=="estate_build" else apply,"gate_id":"legacy_kernel"}
		if kind=="estate_build": entry.compat_action="legacy_begin_action"
		commands.append(entry)
	return {"commands":commands}

## 能力拒绝仍位于公共人物可用与占用守卫之后，未知命令不能借庄园入口放行。
static func preview(state: Dictionary, data: Dictionary, actor_id: String, kind: String, params: Dictionary) -> Dictionary:
	if not kind in Estate.KINDS: return {"ok":false,"code":"UnsupportedAction"}
	if not "estates" in data.required_capabilities: return {"ok":false,"code":"EstatesUnavailable"}
	return Estate.preview(state,data,actor_id,kind,params)

## 即时命令继续复核原报价并修改同一真实资产；建设不得绕过占用和支付阶段。
static func apply(state: Dictionary, data: Dictionary, actor_id: String, kind: String, _params: Dictionary, offer: Dictionary) -> Dictionary:
	if not kind in Estate.KINDS or kind=="estate_build": return {"ok":false,"code":"UnsupportedAction"}
	return Estate.apply(state,data,actor_id,kind,offer)

## 完整世界独立锚定八命令及建设验证归属，缺项、错门、借owner和新增庄园命令均失败。
static func command_contract_status(registry: Dictionary) -> Dictionary:
	var commands: Dictionary=registry.get("commands",{})
	var legacy: Dictionary=registry.get("legacy_action_validators",{})
	for kind in Estate.KINDS:
		if not commands.has(kind): return {"ok":false,"code":"MissingBuiltinCommand","detail":kind}
		var entry: Dictionary=commands[kind]
		var mode: String="action" if kind=="estate_build" else "instant"
		if entry.get("owner")!="estates" or entry.get("mode")!=mode or entry.get("gate_id")!="legacy_kernel": return {"ok":false,"code":"InvalidBuiltinCommand","detail":kind}
		if kind=="estate_build":
			if entry.get("compat_action")!="legacy_begin_action" or entry.get("apply")!=null: return {"ok":false,"code":"InvalidBuiltinCommand","detail":kind}
		elif entry.has("compat_action"): return {"ok":false,"code":"InvalidBuiltinCommand","detail":kind}
	if not legacy.has("estate_build") or legacy.estate_build.get("owner")!="estates": return {"ok":false,"code":"MissingBuiltinAction","detail":"estate_build"}
	for kind in commands:
		if commands[kind].get("owner")=="estates" and not kind in Estate.KINDS: return {"ok":false,"code":"InvalidBuiltinCommand","detail":kind}
	return {"ok":true}
