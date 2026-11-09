## 个人战四种即时命令的真实注册适配；战斗规则、报价和两阶段战果仍归原Personal。
extends RefCounted
const Personal=preload("res://core/personal_combat.gd")
const GATE_ID="personal_before_world"

## 从原KINDS生成唯一personal命令片段，开战半日协调不伪装成耗时行动。
static func descriptor() -> Dictionary:
	var commands: Array=[]
	for kind in Personal.KINDS:
		commands.append({"kind":kind,"mode":"instant","gate_id":GATE_ID,"preview":preview,"apply":apply})
	return {"id":"personal","commands":commands,"actions":[]}

## 已知kind保留原能力和控制者首错；未知kind不得落入原规则的道路接战分支。
static func preview(state: Dictionary, data: Dictionary, actor_id: String, kind: String, params: Dictionary) -> Dictionary:
	if not kind in Personal.KINDS: return {"ok":false,"code":"UnsupportedAction"}
	return Personal.preview(state,data,actor_id,kind,params)

## 开战前置协调由Kernel明确调用；本绑定真正建立会话或执行原轮次、结果确认。
static func apply(state: Dictionary, data: Dictionary, actor_id: String, kind: String, _params: Dictionary, offer: Dictionary) -> Dictionary:
	if not kind in Personal.KINDS: return {"ok":false,"code":"UnsupportedAction"}
	if kind in ["duel_start","encounter_fight"]: return Personal.begin(state,data,offer)
	return Personal.apply(state,data,actor_id,kind,offer)

## 固定owner、原kind和专用早门共同限制资格；别域或新命令不可借此绕过公共守卫。
static func command_contract_status(registry: Dictionary) -> Dictionary:
	var commands: Dictionary=registry.get("commands",{})
	for kind in Personal.KINDS:
		if not commands.has(kind): return {"ok":false,"code":"MissingBuiltinCommand","detail":kind}
		var command: Dictionary=commands[kind]
		if command.get("owner")!="personal" or command.get("mode")!="instant" or command.get("gate_id")!=GATE_ID: return {"ok":false,"code":"InvalidBuiltinCommand","detail":kind}
	for kind in commands:
		var command: Dictionary=commands[kind]
		if command.get("gate_id")==GATE_ID and (command.get("owner")!="personal" or not kind in Personal.KINDS): return {"ok":false,"code":"InvalidBuiltinCommand","detail":kind}
	return {"ok":true}
