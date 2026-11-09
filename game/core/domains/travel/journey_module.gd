## 途中五命令只适配真实Journey规则；半日推进仍回到Kernel唯一协调入口。
extends RefCounted
const Journey=preload("res://core/journey_commands.gd")
const GATE_ID="journey_before_actor"

## 原KINDS是命令集合唯一来源；即时控制不新增行动类型或抢占travel生命周期。
static func descriptor() -> Dictionary:
	var commands: Array=[]
	for kind in Journey.KINDS:
		commands.append({"kind":kind,"mode":"instant","gate_id":GATE_ID,"preview":preview,"apply":apply})
	return {"id":"travel","commands":commands,"actions":[]}

## 保留Journey自身NotTravelling先于ActorUnavailable的错误顺序，未知命令明确拒绝。
static func preview(state: Dictionary, data: Dictionary, actor_id: String, kind: String, params: Dictionary) -> Dictionary:
	if not kind in Journey.KINDS: return {"ok":false,"code":"UnsupportedAction"}
	return Journey.preview(state,data,actor_id,kind,params)

## 只按已校验报价调用旧规则；改道释放原占用、同行与军令权源仍由原算法决定。
static func apply(state: Dictionary, data: Dictionary, actor_id: String, kind: String, _params: Dictionary, offer: Dictionary) -> Dictionary:
	if not kind in Journey.KINDS: return {"ok":false,"code":"UnsupportedAction"}
	return Journey.apply(state,data,actor_id,kind,offer)

## 只有本集合成功后消费原等待标记；hold本人推进一次，不给其他travel命令附加时钟。
static func after_command(state: Dictionary, actor_id: String, kind: String, outcome: Dictionary, advance: Callable) -> Dictionary:
	if not kind in Journey.KINDS: return {"ok":false,"code":"UnsupportedAction"}
	if not outcome.ok: return outcome
	return advance.call(state,[actor_id]) if outcome.wait else {"ok":true}

## 启动独立核对固定owner、早门与instant模式；不能借同域或别域kind绕过人物守卫。
static func command_contract_status(registry: Dictionary) -> Dictionary:
	var commands: Dictionary=registry.get("commands",{})
	for kind in Journey.KINDS:
		if not commands.has(kind): return {"ok":false,"code":"MissingBuiltinCommand","detail":kind}
		var command: Dictionary=commands[kind]
		if command.get("owner")!="travel" or command.get("mode")!="instant" or command.get("gate_id")!=GATE_ID or command.has("compat_action"): return {"ok":false,"code":"InvalidBuiltinCommand","detail":kind}
	for kind in commands:
		var command: Dictionary=commands[kind]
		if command.get("gate_id")==GATE_ID and (command.get("owner")!="travel" or not kind in Journey.KINDS): return {"ok":false,"code":"InvalidBuiltinCommand","detail":kind}
	return {"ok":true}
