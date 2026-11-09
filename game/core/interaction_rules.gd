## 解析真实人物、所在地点和岗位授权；界面上下文不能自行授予权限。
extends RefCounted
const Values=preload("res://core/base/value_checks.gd")
const Location=preload("res://core/world_location.gd")
const Combat=preload("res://core/combat_session.gd")
const Identity=preload("res://core/actor_identity.gd")

## 核验双方与当值人员确实在场且可办理指定事务。
static func validate(state: Dictionary, data: Dictionary, actor_id: String, context: Variant, service: String = "", own_action: String = "") -> Dictionary:
	if Combat.occupied(state,actor_id) or (context is Dictionary and Combat.occupied(state,str(context.get("provider_actor_id","")))): return {"ok":false,"code":"BattlePending"}
	if context is Dictionary and context.get("kind")=="encounter": return road_context(state,data,actor_id,context,service)
	if not context is Dictionary:
		return {"ok":false,"code":"MissingInteractionContext"}
	for key in ["place_id","provider_actor_id","role_id"]:
		if not context.get(key) is String: return {"ok":false,"code":"InvalidInteractionContext"}
	if not state.actors.has(actor_id) or not state.actors.has(context.provider_actor_id) or not data.roles.has(context.role_id):
		return {"ok":false,"code":"UnknownInteractionTarget"}
	var actor: Dictionary = state.actors[actor_id]
	var provider: Dictionary = state.actors[context.provider_actor_id]
	var role: Dictionary = data.roles[context.role_id]
	var assignment: Dictionary = state.roles[context.role_id]
	if Location.resolve(state,actor_id).get("kind")!="Settlement" or Location.resolve(state,actor_id).get("place_id")!=context.place_id:
		return {"ok":false,"code":"NotAtPlace"}
	if not provider.alive or not provider.available or Location.resolve(state,provider.id).get("kind")!="Settlement" or Location.resolve(state,provider.id).get("place_id")!=context.place_id:
		return {"ok":false,"code":"PersonAbsent"}
	if role.place_id!=context.place_id or assignment.actor_id!=context.provider_actor_id or not assignment.enabled:
		return {"ok":false,"code":"RoleNotAuthorized"}
	if not service.is_empty() and not service in role.services:
		return {"ok":false,"code":"RoleNotAuthorized"}
	for participant in [actor,provider]:
		if not participant.action_id.is_empty() and participant.action_id!=own_action:
			return {"ok":false,"code":"ActorBusy"}
	if not actor.alive or not actor.available or actor_id==context.provider_actor_id:
		return {"ok":false,"code":"InvalidInteraction"}
	return {"ok":true,"context":context.duplicate(true)}

## 从本局当前岗位任职人建立上下文，而不是复用作者初始人选。
static func for_role(state: Dictionary, data: Dictionary, role_id: String) -> Dictionary:
	if not data.roles.has(role_id) or not state.roles.has(role_id): return {}
	return {"place_id":data.roles[role_id].place_id,"role_id":role_id,"provider_actor_id":state.roles[role_id].actor_id}

## 检查新线索的实际来源，包括当前地点上有署名的告示。
static func discovery(state: Dictionary, data: Dictionary, actor_id: String, offer_id: String, params: Dictionary) -> Dictionary:
	if not data.offers.has(offer_id): return {"ok":false,"code":"UnknownOffer"}
	var source: Dictionary = {}
	if params.has("prop_id"):
		var prop: Dictionary = data.props.get(params.prop_id,{})
		if prop.is_empty() or state.actors[actor_id].location.get("place_id")!=prop.place_id:
			return {"ok":false,"code":"NotAtPlace"}
		if not offer_id in prop.offer_ids: return {"ok":false,"code":"SourceUnknown"}
		source={"kind":"prop","id":prop.id}
	else:
		var check: Dictionary = validate(state,data,actor_id,params.get("context"),"information")
		if not check.ok: return check
		source={"kind":"actor","id":check.context.provider_actor_id}
	if not source in data.offers[offer_id].discovery_sources:
		return {"ok":false,"code":"SourceUnknown"}
	return {"ok":true,"source":source,"duration":0}

## 道路上下文只允许真实会面者交谈；旅行保留占用，不继承原岗位的交易或公事权限。
static func road_context(state: Dictionary, data: Dictionary, actor_id: String, context: Dictionary, service: String = "") -> Dictionary:
	if not service.is_empty(): return {"ok":false,"code":"RoadServiceDenied"}
	for key in ["encounter_id","provider_actor_id","role_id","place_id"]:
		if not context.get(key) is String: return {"ok":false,"code":"InvalidInteractionContext"}
	var e: Variant=state.get("encounters",{}).get(context.encounter_id)
	if not e is Dictionary or e.status!="pending": return {"ok":false,"code":"EncounterResolved"}
	var provider: String=context.provider_actor_id
	if context.place_id!="" or actor_id==provider or not actor_id in e.participants or not provider in e.participants: return {"ok":false,"code":"NotEncounterParticipant"}
	for leader in e.leaders:
		var person: Dictionary=state.actors[leader]
		if not person.alive or not person.available or person.location!=e.anchors[leader] or person.action_id!=e.action_ids[leader]: return {"ok":false,"code":"EncounterMoved"}
	var dialogue_id: Variant=Identity.definition(state,data,provider).get("road_dialogue_id")
	if not dialogue_id is String or not data.dialogues.has(dialogue_id) or data.dialogues[dialogue_id].context_role!=context.role_id: return {"ok":false,"code":"NoRoadDialogue"}
	if not state.actors[actor_id].alive or not state.actors[actor_id].available or not state.actors[provider].alive: return {"ok":false,"code":"PersonAbsent"}
	return {"ok":true,"context":context.duplicate(true)}
## 校验说话者的实际在场；路上只使用已绑定参与者，不能借空place_id引入远方人物。
static func speaker_present(state: Dictionary, context: Dictionary, actor_id: String) -> bool:
	if not state.actors.has(actor_id) or not state.actors[actor_id].alive: return false
	if context.get("kind")=="encounter":
		var e: Dictionary=state.get("encounters",{}).get(context.get("encounter_id"),{})
		return e.get("status")=="pending" and actor_id in e.get("participants",[])
	return Location.resolve(state,actor_id).get("place_id")==context.get("place_id")

## 岗位身份形状在资源校验前检查，不重算当前服务权限。
static func validate_roles(state: Dictionary, data: Dictionary) -> Dictionary:
	for role_id in state.roles:
		var role: Variant = state.roles[role_id]
		if not data.roles.has(role_id) or not Values.fields(role,{"actor_id":TYPE_STRING,"enabled":TYPE_BOOL}) or not state.actors.has(role.actor_id): return {"ok":false,"code":"InvalidRoleAssignment"}
	return {"ok":true}

## 作者标记的类型按旧位置检查，未知标记仍拒绝。
static func validate_flags(state: Dictionary, data: Dictionary) -> Dictionary:
	for key in state.flags:
		if not data.flags.has(key) or not state.flags[key] is bool: return {"ok":false,"code":"InvalidFlag"}
	return {"ok":true}

## 校验对白继续点、待完成行动与场景事件引用，不重放这些行为。
static func validate_sessions(state: Dictionary, data: Dictionary) -> Dictionary:
	for actor_id in state.interactions:
		var session: Variant=state.interactions[actor_id]
		if not state.actors.has(actor_id) or not Values.fields(session,{"id":TYPE_STRING,"dialogue_id":TYPE_STRING,"context":TYPE_DICTIONARY,"node_id":TYPE_STRING,"phase":TYPE_STRING,"pending_command":TYPE_DICTIONARY,"pending_action_id":TYPE_STRING,"last_outcome":TYPE_DICTIONARY,"trade_completed":TYPE_BOOL}) or not data.dialogues.has(session.dialogue_id): return {"ok":false,"code":"InvalidInteractionSession"}
		if not Values.fields(session.context,{"place_id":TYPE_STRING,"role_id":TYPE_STRING,"provider_actor_id":TYPE_STRING}) or (session.context.get("kind")!="encounter" and not data.places.has(session.context.place_id)) or not data.roles.has(session.context.role_id) or not state.actors.has(session.context.provider_actor_id): return {"ok":false,"code":"InvalidSessionContext"}
		if session.context.get("kind")=="encounter":
			if not road_context(state,data,actor_id,session.context).ok or Identity.definition(state,data,session.context.provider_actor_id).get("road_dialogue_id")!=session.dialogue_id: return {"ok":false,"code":"InvalidRoadSession"}
		var selected: Dictionary={}
		for node in data.dialogues[session.dialogue_id].nodes:
			if node.id==session.node_id: selected=node
		if selected.is_empty() or not session.phase in ["say","choice","action","waiting","trade"]: return {"ok":false,"code":"InvalidDialogueContinuePoint"}
		if session.phase in ["say","choice"] and selected.kind!=session.phase: return {"ok":false,"code":"DialoguePhaseMismatch"}
		if session.phase in ["action","waiting","trade"] and (selected.kind!="action" or session.pending_command!=selected.command): return {"ok":false,"code":"DialogueCommandMismatch"}
		if session.phase=="waiting" and (not state.actions.has(session.pending_action_id) or state.actions[session.pending_action_id].actor_id!=actor_id): return {"ok":false,"code":"MissingDialogueAction"}
		if session.phase!="waiting" and not session.pending_action_id.is_empty(): return {"ok":false,"code":"UnexpectedDialogueAction"}
		if session.phase=="trade" and selected.command.kind!="open_trade": return {"ok":false,"code":"InvalidTradeContinuePoint"}
	for event in state.events.values():
		if not Values.fields(event,{"event_id":TYPE_STRING,"actor_id":TYPE_STRING,"place_id":TYPE_STRING,"status":TYPE_STRING,"last_tick":TYPE_INT}) or not data.events.has(event.event_id) or not state.actors.has(event.actor_id) or data.events[event.event_id].place_id!=event.place_id or not event.status in ["queued","shown","cancelled"] or not Values.whole(event.last_tick,0,state.tick): return {"ok":false,"code":"InvalidSceneEvent"}
	return {"ok":true}
