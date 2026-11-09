## 道路会面只适配原两项即时命令；对白推进由Kernel传入窄回调，不反依赖组合根。
extends RefCounted
const Encounters=preload("res://core/encounter_rules.gd")
const GATE_ID="encounter_before_pending"

## 原KINDS逐项追加到interaction，保留既有事件观察和状态验证；不伪造新行动。
static func descriptor() -> Dictionary:
	var commands: Array=[]
	for kind in Encounters.KINDS:
		commands.append({"kind":kind,"mode":"instant","gate_id":GATE_ID,"preview":preview,"apply":apply})
	return {"id":"interaction","commands":commands,"actions":[]}

## 陌生kind在访问世界之前拒绝；真实规则保留原会面参与者、锚点和对白首错。
static func preview(state: Dictionary, data: Dictionary, actor_id: String, kind: String, params: Dictionary) -> Dictionary:
	if not kind in Encounters.KINDS: return {"ok":false,"code":"UnsupportedAction"}
	return Encounters.preview(state,data,actor_id,kind,params)

## 只提交正式preview的私有报价；原规则唯一负责会面、对白初态和行程恢复。
static func apply(state: Dictionary, data: Dictionary, actor_id: String, kind: String, _params: Dictionary, offer: Dictionary) -> Dictionary:
	if not kind in Encounters.KINDS: return {"ok":false,"code":"UnsupportedAction"}
	return Encounters.apply(state,data,actor_id,kind,offer)

## 成功交谈才请求原对白解释器继续；回调失败原样返回，让Kernel丢弃完整候选。
static func after_command(state: Dictionary, actor_id: String, kind: String, outcome: Dictionary, drive_dialogue: Callable) -> Dictionary:
	if not outcome.ok: return outcome
	if not kind in Encounters.KINDS: return {"ok":false,"code":"UnsupportedAction"}
	return drive_dialogue.call(state,actor_id) if kind=="encounter_talk" else outcome

## 独立锚定原集合、owner、即时模式和专用早门，其他领域或陌生kind不得借门。
static func command_contract_status(registry: Dictionary) -> Dictionary:
	var commands: Dictionary=registry.get("commands",{})
	for kind in Encounters.KINDS:
		if not commands.has(kind): return {"ok":false,"code":"MissingBuiltinCommand","detail":kind}
		var command: Dictionary=commands[kind]
		if command.get("owner")!="interaction" or command.get("mode")!="instant" or command.get("gate_id")!=GATE_ID: return {"ok":false,"code":"InvalidBuiltinCommand","detail":kind}
	for kind in commands:
		var command: Dictionary=commands[kind]
		if command.get("gate_id")==GATE_ID and (command.get("owner")!="interaction" or not kind in Encounters.KINDS): return {"ok":false,"code":"InvalidBuiltinCommand","detail":kind}
	return {"ok":true}
