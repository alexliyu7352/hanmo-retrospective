## 当面问讯的领域适配；只转述原社会规则已允许传播的见闻。
extends RefCounted
const KINDS=["ask_news"]
const Social=preload("res://core/social_rules.gd")
const Interaction=preload("res://core/interaction_rules.gd")

## 集合仅来自本适配器真实实现；原问讯不建行动或增加生命周期。
static func descriptor() -> Dictionary:
	var commands: Array=[]
	for kind in KINDS:
		commands.append({"kind":kind,"mode":"instant","gate_id":"legacy_kernel","preview":preview,"apply":apply})
	return {"commands":commands}

## 公共守卫之后才检查能力和真实会话，保留原错误优先级。
static func preview(state: Dictionary, data: Dictionary, actor_id: String, kind: String, _params: Dictionary) -> Dictionary:
	if kind=="ask_news":
		if not "npc_society" in data.required_capabilities: return {"ok":false,"code":"SocialUnavailable"}
		var conversation: Dictionary=state.interactions.get(actor_id,{})
		if not conversation.get("phase","") in ["say","choice"]: return {"ok":false,"code":"NoConversation"}
		var valid: Dictionary=Interaction.validate(state,data,actor_id,conversation.context)
		if not valid.ok: return valid
		return {"ok":true,"duration":0,"context":valid.context}
	return {"ok":false,"code":"UnsupportedAction"}

## 复用有限消息传播及来源记录，不把社会事实全量复制给听者。
static func apply(state: Dictionary, data: Dictionary, actor_id: String, kind: String, _params: Dictionary, offer: Dictionary) -> Dictionary:
	if kind=="ask_news":
		var heard: Dictionary=Social.hear_news(state,data,actor_id,offer.context.provider_actor_id,state.tick)
		return {"ok":true,"details":{"news_ids":heard.events}} if heard.ok else heard
	return {"ok":false,"code":"UnsupportedAction"}
