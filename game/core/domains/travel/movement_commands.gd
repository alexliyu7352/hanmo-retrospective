## 原内联移动三命令的真实注册；路线、参与者、权限与事件仍交既有领域规则。
extends RefCounted
const Travel=preload("res://core/travel_rules.gd")
const Parties=preload("res://core/party_rules.gd")
const Location=preload("res://core/world_location.gd")
const Conditions=preload("res://core/condition_rules.gd")
const Inventory=preload("res://core/inventory_rules.gd")
const KINDS=["travel","rescue","move_local"]
const RESCUE_GATE="rescue_before_actor"

## travel沿用已登记旧行动桥；rescue只改既有行动，不能伪装为新开工。
static func descriptor() -> Dictionary:
	var commands: Array=[]
	for kind in KINDS:
		var entry: Dictionary={"kind":kind,"mode":"action" if kind=="travel" else "instant","gate_id":RESCUE_GATE if kind=="rescue" else "legacy_kernel","preview":preview,"apply":null if kind=="travel" else apply}
		if kind=="travel": entry.compat_action="legacy_begin_action"
		commands.append(entry)
	return {"commands":commands}

## 保留原救援与本地准入首错；旅行规划直接使用原规则，不复制路线或同行费用。
static func preview(state: Dictionary, data: Dictionary, actor_id: String, kind: String, params: Dictionary) -> Dictionary:
	if not kind in KINDS: return {"ok":false,"code":"UnsupportedAction"}
	var actor: Dictionary=state.actors[actor_id]
	if kind=="travel": return Travel.plan(state,data,actor_id,params.get("destination"),params.get("via",[]))
	if kind=="rescue":
		if not Location.army_id(state,actor_id).is_empty(): return {"ok":false,"code":"ArmyNotReturned"}
		if actor.action_id.is_empty() or state.actions[actor.action_id].kind!="travel": return {"ok":false,"code":"NotTravelling"}
		var action: Dictionary=state.actions[actor.action_id]
		if Inventory.spendable(state.wallets[action.params.payer_wallet_id])>=Travel.next_cost(state,data,action): return {"ok":false,"code":"NotStranded"}
		return {"ok":true,"duration":2}
	var target: Variant=params.get("place_id")
	if not target is String or not data.places.has(target): return {"ok":false,"code":"UnknownPlace"}
	if actor.location.kind!="Settlement" or not target in data.places[actor.location.place_id].passages: return {"ok":false,"code":"NoLocalPassage"}
	if not Parties.available(state,actor_id): return {"ok":false,"code":"ParticipantBusy"}
	for member in Location.members(state,actor_id):
		if not Conditions.all_true(state,member,data.places[target].admission): return {"ok":false,"code":"AudienceDenied"}
	return {"ok":true,"duration":0}

## 只在候选内执行原写入；救援保留ID及参与者，本地移动失败由外层整候选回滚。
static func apply(state: Dictionary, data: Dictionary, actor_id: String, kind: String, params: Dictionary, _offer: Dictionary) -> Dictionary:
	if kind=="rescue":
		var actor: Dictionary=state.actors[actor_id]
		var action: Dictionary=state.actions[actor.action_id]
		action.kind="rescue"; action.remaining_ticks=2; action.params={"origin":actor.location.origin}
		return {"ok":true}
	if kind=="move_local": return Travel.move_local(state,data,actor_id,params.place_id)
	return {"ok":false,"code":"UnsupportedAction"}

## 固定三项所属、桥与门位；专用救援门禁止其他owner或kind借道绕开人物守卫。
static func command_contract_status(registry: Dictionary) -> Dictionary:
	var commands: Dictionary=registry.get("commands",{})
	for kind in KINDS:
		if not commands.has(kind): return {"ok":false,"code":"MissingBuiltinCommand","detail":kind}
		var entry: Dictionary=commands[kind]
		if entry.get("owner")!="travel" or entry.get("mode")!=("action" if kind=="travel" else "instant") or entry.get("gate_id")!=(RESCUE_GATE if kind=="rescue" else "legacy_kernel"): return {"ok":false,"code":"InvalidBuiltinCommand","detail":kind}
		if kind=="travel":
			if entry.get("compat_action")!="legacy_begin_action" or entry.get("apply")!=null: return {"ok":false,"code":"InvalidBuiltinCommand","detail":kind}
		elif entry.has("compat_action"): return {"ok":false,"code":"InvalidBuiltinCommand","detail":kind}
	var legacy: Dictionary=registry.get("legacy_action_validators",{})
	for kind in ["travel","rescue"]:
		if not legacy.has(kind) or legacy[kind].get("owner")!="travel": return {"ok":false,"code":"MissingBuiltinAction","detail":kind}
	for kind in commands:
		var entry: Dictionary=commands[kind]
		if entry.get("gate_id")==RESCUE_GATE and (entry.get("owner")!="travel" or kind!="rescue"): return {"ok":false,"code":"InvalidBuiltinCommand","detail":kind}
	return {"ok":true}
