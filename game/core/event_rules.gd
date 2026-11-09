## 场景事件只在真实入场时排队；提交尾部取消语义失效记录，读取不创建或修改事实。
extends RefCounted
const Conditions=preload("res://core/condition_rules.gd")
const Dialogue=preload("res://core/dialogue_runner.gd")
const Location=preload("res://core/world_location.gd")
const Identity=preload("res://core/actor_identity.gd")

## 仅处理已支持的入场触发；不把其他触发当成入场，也不执行未知不可用策略。
static func on_enter(state: Dictionary, data: Dictionary, actor_id: String) -> Dictionary:
	var place_id: String=state.actors[actor_id].location.get("place_id","")
	var ids: Array=data.events.keys(); ids.sort()
	for id in ids:
		var definition: Dictionary=data.events[id]
		if definition.trigger!="on_enter": continue
		if definition.on_unavailable!="cancel": return {"ok":false,"code":"UnsupportedEventPolicy"}
		if definition.place_id!=place_id or not Conditions.all_true(state,actor_id,definition.conditions): continue
		var key: String=actor_id+"|"+id
		var prior: Dictionary=state.events.get(key,{})
		if not prior.is_empty() and (prior.status=="queued" or definition.once_per_actor or state.tick-prior.last_tick<definition.cooldown_ticks): continue
		var role_id: String=data.dialogues[definition.dialogue_id].context_role
		var present: Dictionary=Dialogue.can_begin(state,data,actor_id,role_id)
		if not present.ok or present.dialogue_id!=definition.dialogue_id: continue
		state.events[key]={"event_id":id,"actor_id":actor_id,"place_id":place_id,"status":"queued","last_tick":state.tick}
	return {"ok":true}

## 独立判断语义有效性，避免BattlePending或ActorBusy遮蔽死亡、离场与岗位失效。
static func _semantically_available(state: Dictionary, data: Dictionary, actor_id: String, definition: Dictionary) -> bool:
	var actor: Dictionary=state.actors[actor_id]
	var location: Dictionary=Location.resolve(state,actor_id)
	if not actor.alive or not actor.available or location.get("kind")!="Settlement" or location.get("place_id")!=definition.place_id: return false
	if not Conditions.all_true(state,actor_id,definition.conditions): return false
	var role_id: String=data.dialogues[definition.dialogue_id].context_role
	var assignment: Dictionary=state.roles[role_id]
	var provider: Dictionary=state.actors[assignment.actor_id]
	var provider_location: Dictionary=Location.resolve(state,provider.id)
	if not assignment.enabled or data.roles[role_id].place_id!=definition.place_id or provider.id==actor_id: return false
	if not provider.alive or not provider.available or provider_location.get("kind")!="Settlement" or provider_location.get("place_id")!=definition.place_id: return false
	# 旧事件不猜历史provider；与入场门一致，优先当前岗位显式对白，再沿人物旧定义。
	return data.roles[role_id].get("dialogue_id",Identity.definition(state,data,provider.id).get("dialogue_id"))==definition.dialogue_id

## 按稳定键仅取消queued记录；忙碌保留队列，已取消记录不刷新冷却起点。
static func reconcile(state: Dictionary, data: Dictionary) -> Dictionary:
	var keys: Array=state.events.keys(); keys.sort()
	for key in keys:
		var record: Dictionary=state.events[key]
		if record.status!="queued": continue
		var definition: Dictionary=data.events[record.event_id]
		if definition.on_unavailable!="cancel": return {"ok":false,"code":"UnsupportedEventPolicy"}
		if not _semantically_available(state,data,record.actor_id,definition):
			record.status="cancelled"
			record.last_tick=state.tick
	return {"ok":true}

## 预览纯读，已取消只报历史状态，不从当前世界反推没有持久化的历史原因。
static func preview(state: Dictionary, data: Dictionary, actor_id: String, event_id: Variant) -> Dictionary:
	if not event_id is String or not data.events.has(event_id): return {"ok":false,"code":"UnknownEvent"}
	var definition: Dictionary=data.events[event_id]
	if definition.on_unavailable!="cancel": return {"ok":false,"code":"UnsupportedEventPolicy"}
	var instance: Dictionary=state.events.get(actor_id+"|"+event_id,{})
	if instance.get("status")=="cancelled": return {"ok":false,"code":"EventCancelled"}
	if instance.is_empty() or instance.status!="queued" or state.actors[actor_id].location.get("place_id")!=definition.place_id: return {"ok":false,"code":"EventNotHere"}
	if not Conditions.all_true(state,actor_id,definition.conditions): return {"ok":false,"code":"EventUnavailable"}
	var result: Dictionary=Dialogue.can_begin(state,data,actor_id,data.dialogues[definition.dialogue_id].context_role)
	if result.ok and result.dialogue_id!=definition.dialogue_id: return {"ok":false,"code":"EventParticipantChanged"}
	return result

## 实际展示才标记shown；忽略或取消事件不会创建任务或奖励。
static func presented(state: Dictionary, actor_id: String, event_id: String) -> void:
	var record: Dictionary=state.events[actor_id+"|"+event_id]
	record.status="shown"
	record.last_tick=state.tick
