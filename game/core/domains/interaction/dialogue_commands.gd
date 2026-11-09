## 旧交互协议的九项受信命令适配；只复用原对白解释器和人物/店铺规则，不定义内容。
extends RefCounted
const Dialogue=preload("res://core/dialogue_runner.gd")
const Interaction=preload("res://core/interaction_rules.gd")
const Events=preload("res://core/event_rules.gd")
const KINDS=["talk","open_event","dialogue_next","dialogue_choose","dialogue_confirm","dialogue_cancel","close_trade","end_interaction","open_trade"]

## 九项均为即时协议；confirm可能递归创建旧行动，但自身不伪造生命周期。
static func descriptor() -> Dictionary:
	var commands: Array=[]
	for kind in KINDS:
		commands.append({"kind":kind,"mode":"instant","gate_id":"legacy_kernel","preview":preview,"apply":apply})
	return {"commands":commands}

## 普通守卫仍由Kernel先执行；仅确认中的真实pending请求使用受信递归预览。
static func preview(state: Dictionary, data: Dictionary, actor_id: String, kind: String, params: Dictionary, preview_command: Callable=Callable()) -> Dictionary:
	if not kind in KINDS: return {"ok":false,"code":"UnsupportedAction"}
	match kind:
		"talk": return Dialogue.can_begin(state,data,actor_id,params.get("role_id"))
		"open_event": return Events.preview(state,data,actor_id,params.get("event_id"))
		"dialogue_next","dialogue_choose": return Dialogue.validate_input(state,data,actor_id,kind,params)
		"dialogue_confirm":
			if not state.interactions.has(actor_id) or state.interactions[actor_id].phase!="action": return {"ok":false,"code":"NoPendingDialogueAction"}
			if not preview_command.is_valid(): return {"ok":false,"code":"DialogueCoordinationUnavailable"}
			var session: Dictionary=state.interactions[actor_id]
			var request: Dictionary=session.pending_command.duplicate(true)
			request.context=session.context.duplicate(true)
			return preview_command.call(state,request.kind,request,actor_id)
		"dialogue_cancel","close_trade":
			if not state.interactions.has(actor_id) or not state.interactions[actor_id].phase in ["action","trade"]: return {"ok":false,"code":"NoPendingDialogueAction"}
			return {"ok":true,"duration":0}
		"end_interaction": return {"ok":true,"duration":0}
		"open_trade":
			if not params.get("shop_id") is String or not data.shops.has(params.shop_id): return {"ok":false,"code":"UnknownShop"}
			var valid: Dictionary=Interaction.validate(state,data,actor_id,params.get("context"),"trade")
			if not valid.ok: return valid
			if not valid.context.role_id in data.shops[params.shop_id].representative_roles or valid.context.place_id!=data.shops[params.shop_id].place_id: return {"ok":false,"code":"WrongShopRepresentative"}
			return {"ok":true,"duration":0,"context":valid.context}
	return {"ok":false,"code":"UnsupportedAction"}

## 原顺序建立/推进/取消会话；回调只由Kernel按固定owner/kind注入，失败交外层整候选回滚。
static func apply(state: Dictionary, data: Dictionary, actor_id: String, kind: String, params: Dictionary, offer: Dictionary, drive_dialogue: Callable=Callable(), apply_command: Callable=Callable()) -> Dictionary:
	if not kind in KINDS: return {"ok":false,"code":"UnsupportedAction"}
	if kind not in ["end_interaction","open_trade"] and not drive_dialogue.is_valid(): return {"ok":false,"code":"DialogueCoordinationUnavailable"}
	if kind=="dialogue_confirm" and not apply_command.is_valid(): return {"ok":false,"code":"DialogueCoordinationUnavailable"}
	match kind:
		"talk","open_event":
			Dialogue.begin(state,data,actor_id,offer)
			if kind=="open_event": Events.presented(state,actor_id,params.event_id)
			return drive_dialogue.call(state,actor_id)
		"dialogue_next","dialogue_choose":
			Dialogue.advance(state,actor_id,offer.next)
			return drive_dialogue.call(state,actor_id)
		"dialogue_confirm":
			var session: Dictionary=state.interactions[actor_id]
			var request: Dictionary=session.pending_command.duplicate(true)
			request.context=session.context.duplicate(true)
			var applied: Dictionary=apply_command.call(state,request.kind,request,actor_id,offer)
			if not applied.ok: return applied
			if not state.interactions.has(actor_id): return applied
			if request.kind=="open_trade":
				session.phase="trade"; session.trade_completed=false
			elif applied.get("details",{}).has("action_id"):
				session.phase="waiting"; session.pending_action_id=applied.details.action_id
			else:
				Dialogue.resolve(state,data,actor_id,{"ok":true})
				return drive_dialogue.call(state,actor_id)
			return applied
		"dialogue_cancel","close_trade":
			Dialogue.resolve(state,data,actor_id,{"ok":kind=="close_trade" and state.interactions[actor_id].trade_completed,"code":"Cancelled"})
			return drive_dialogue.call(state,actor_id)
		"end_interaction":
			state.interactions.erase(actor_id)
			return {"ok":true}
		"open_trade": return {"ok":true}
	return {"ok":false,"code":"UnsupportedAction"}

## 必须保留九项即时命令与原公共门；同owner的Encounter/Discovery命令不被此片覆盖或排除。
static func command_contract_status(registry: Dictionary) -> Dictionary:
	var commands: Dictionary=registry.get("commands",{})
	for kind in KINDS:
		if not commands.has(kind): return {"ok":false,"code":"MissingBuiltinCommand","detail":kind}
		var entry: Dictionary=commands[kind]
		if entry.get("owner")!="interaction" or entry.get("mode")!="instant" or entry.get("gate_id")!="legacy_kernel" or entry.has("compat_action"): return {"ok":false,"code":"InvalidBuiltinCommand","detail":kind}
	return {"ok":true}
