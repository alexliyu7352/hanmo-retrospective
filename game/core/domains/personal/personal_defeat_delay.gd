## 普通遭遇败北的两半日行旅延误；从既有永久战果派生hold，不保存第二份倒计时或扣钱货。
extends RefCounted
const Values = preload("res://core/base/value_checks.gd")
const DELAY_TICKS: int = 2

## 只阻止承载原参败人物的当前travel领队移动；非行旅、原地交互和救援不在此查询范围。
static func held_travel_actors(state: Dictionary, data: Dictionary) -> Array:
	if typeof(state.get("tick")) != TYPE_INT or state.tick < 0: return []
	for key in ["battles", "facts", "actions", "actors"]:
		if not state.get(key) is Dictionary: return []
	if not data.get("personal_objectives") is Dictionary: return []
	var affected: Dictionary = {}
	for battle in state.battles.values():
		if not battle is Dictionary: continue
		for actor_id in _affected_participants(state, data, battle): affected[actor_id] = true
	var held: Dictionary = {}
	for action_id in state.actions:
		var action: Variant = state.actions[action_id]
		if not action_id is String or not Values.fields(action, {"id": TYPE_STRING, "kind": TYPE_STRING, "actor_id": TYPE_STRING, "participants": TYPE_ARRAY}): continue
		if action.kind != "travel" or action.id != action_id: continue
		var leader: Variant = state.actors.get(action.actor_id)
		if not Values.fields(leader, {"action_id": TYPE_STRING}) or leader.action_id != action_id or not action.actor_id in action.participants: continue
		for participant in action.participants:
			if participant is String and affected.has(participant):
				held[action.actor_id] = true
				break
	var ids: Array = held.keys(); ids.sort()
	return ids

## 仅从实际encounter/skirmish/defeat永久来源读取原同侧参战身份；坏形状安全忽略，不能替代Kernel世界校验。
static func _affected_participants(state: Dictionary, data: Dictionary, battle: Dictionary) -> Array:
	if not Values.fields(battle, {"id": TYPE_STRING, "kind": TYPE_STRING, "phase": TYPE_STRING, "outcome": TYPE_STRING, "applied": TYPE_BOOL, "source": TYPE_DICTIONARY, "completed_tick": TYPE_INT, "objective_id": TYPE_STRING, "result_id": TYPE_STRING, "units": TYPE_DICTIONARY, "controller_id": TYPE_STRING}): return []
	if battle.kind != "personal" or not battle.phase in ["result", "closed"] or battle.outcome != "defeat" or not battle.applied: return []
	if not Values.fields(battle.source, {"kind": TYPE_STRING, "encounter_id": TYPE_STRING}) or battle.source.kind != "encounter" or battle.source.encounter_id.is_empty(): return []
	if battle.completed_tick < 0 or battle.completed_tick > state.tick: return []
	# 在旧tick上查询：T和T+1各hold一次，T+2自然失效；不用tick相加或额外累计处罚。
	if state.tick - battle.completed_tick >= DELAY_TICKS: return []
	var objective: Variant = data.personal_objectives.get(battle.objective_id)
	if not Values.fields(objective, {"kind": TYPE_STRING}) or objective.kind != "skirmish" or battle.result_id.is_empty(): return []
	var controller: Variant = battle.units.get(battle.controller_id)
	if not Values.fields(controller, {"side": TYPE_INT}) or not controller.side in [0, 1]: return []
	var fact: Variant = state.facts.get(battle.id)
	if not Values.fields(fact, {"kind": TYPE_STRING, "battle_id": TYPE_STRING, "result_id": TYPE_STRING, "outcome": TYPE_STRING, "tick": TYPE_INT, "source": TYPE_DICTIONARY, "participants": TYPE_ARRAY}): return []
	if fact.kind != "personal_battle" or fact.battle_id != battle.id or fact.result_id != battle.result_id or fact.outcome != "defeat" or fact.tick != battle.completed_tick or fact.source != battle.source: return []
	var actual: Array = fact.participants.duplicate(); var recorded: Array = battle.units.keys()
	for actor_id in actual + recorded:
		if not actor_id is String: return []
	actual.sort(); recorded.sort()
	if actual != recorded: return []
	var affected: Array = []
	for actor_id in recorded:
		var unit: Variant = battle.units[actor_id]
		if not Values.fields(unit, {"actor_id": TYPE_STRING, "side": TYPE_INT}) or unit.actor_id != actor_id or not unit.side in [0, 1]: return []
		if unit.side == controller.side: affected.append(actor_id)
	return affected
