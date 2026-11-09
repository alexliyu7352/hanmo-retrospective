## 家庭三命令的受信适配；只复用原来源规则，收养完工继续等待既有postclock桥。
extends RefCounted
const Family=preload("res://core/family_rules.gd")

## 唯一命令集合仍由旧领域公开，不能向适配器另塞家庭玩法。
static func command_kinds() -> Array:
	return Family.KINDS.duplicate()

## 收养只注册明确旧行动命令桥，生命周期与postclock仍由原路径负责，不伪造回调。
static func descriptor() -> Dictionary:
	var commands: Array=[]
	for kind in Family.KINDS:
		var command: Dictionary={"kind":kind,"mode":"action" if kind=="family_adopt" else "instant","preview":preview,"apply":null if kind=="family_adopt" else apply,"gate_id":"legacy_kernel"}
		if kind=="family_adopt": command.compat_action="legacy_begin_action"
		commands.append(command)
	return {"commands":commands}

## 保持公共可用性/占用守卫之后才检查family能力，不重建缺失来源。
static func preview(state: Dictionary, data: Dictionary, actor_id: String, kind: String, params: Dictionary) -> Dictionary:
	if not kind in Family.KINDS: return {"ok":false,"code":"UnsupportedAction"}
	if not "family" in data.required_capabilities: return {"ok":false,"code":"FamilyUnavailable"}
	return Family.preview(state,data,actor_id,kind,params)

## 指定和退隐重新核验原报价；继任真实修改玩家身份，失败由外层候选事务回滚。
static func apply(state: Dictionary, data: Dictionary, actor_id: String, kind: String, _params: Dictionary, offer: Dictionary) -> Dictionary:
	if not kind in Family.KINDS or kind=="family_adopt": return {"ok":false,"code":"UnsupportedAction"}
	return Family.apply(state,data,actor_id,kind,offer)

## 完整世界必须具备三条真实命令及唯一收养行动；禁止借owner、错门位和额外家庭命令。
static func command_contract_status(registry: Dictionary) -> Dictionary:
	var commands: Dictionary=registry.get("commands",{}); var legacy: Dictionary=registry.get("legacy_action_validators",{})
	for kind in Family.KINDS:
		if not commands.has(kind): return {"ok":false,"code":"MissingBuiltinCommand","detail":kind}
		var entry: Dictionary=commands[kind]
		var mode: String="action" if kind=="family_adopt" else "instant"
		if entry.get("owner")!="family" or entry.get("mode")!=mode or entry.get("gate_id")!="legacy_kernel": return {"ok":false,"code":"InvalidBuiltinCommand","detail":kind}
		if kind=="family_adopt":
			if entry.get("compat_action")!="legacy_begin_action" or entry.get("apply")!=null: return {"ok":false,"code":"InvalidBuiltinCommand","detail":kind}
		elif entry.has("compat_action"): return {"ok":false,"code":"InvalidBuiltinCommand","detail":kind}
	if not legacy.has("family_adopt") or legacy.family_adopt.get("owner")!="family": return {"ok":false,"code":"MissingBuiltinAction","detail":"family_adopt"}
	for kind in commands:
		if commands[kind].get("owner")=="family" and not kind in Family.KINDS: return {"ok":false,"code":"InvalidBuiltinCommand","detail":kind}
	return {"ok":true}
