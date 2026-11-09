## 护卫合同只观察真实同行、旅程和个人战事实；报酬仍由任务受托与当面交接规则结算。
extends RefCounted
const Location=preload("res://core/world_location.gd")

## 委托前核验保护对象真实在场且未承诺另一份护卫，不用临时人物替代居民。
static func can_accept(state: Dictionary, data: Dictionary, actor_id: String, definition: Dictionary, context: Dictionary) -> Dictionary:
	var person: String=definition.goal.actor_id
	if person==actor_id or not state.actors.has(person): return fail("InvalidEscortPerson")
	var target: Dictionary=state.actors[person]
	if not target.alive or not target.available or target.hp<=1: return fail("EscortPersonUnavailable")
	if not target.action_id.is_empty() or Location.resolve(state,person).get("place_id")!=context.place_id: return fail("EscortPersonAbsent")
	for task in state.tasks.values():
		if task.status in ["accepting","active","ready"] and data.offers[task.offer_id].template=="escort" and data.offers[task.offer_id].goal.actor_id==person: return fail("EscortAlreadyPromised")
	return {"ok":true}

## 受领完成后建立旅程记录；尚未出发不能因目的地相同直接获得履约。
static func begin(task: Dictionary) -> void:
	task.escort={"departed":false,"arrival_tick":-1,"battle_ids":[],"failure":"","failed_tick":-1}

## 每次候选提交后观察当前位置；实际同队出发并抵达指定场所才可回报，不额外推进时间。
static func observe(state: Dictionary, data: Dictionary) -> void:
	for task in state.tasks.values():
		var definition: Dictionary=data.offers[task.offer_id]
		if definition.template!="escort" or task.status!="active": continue
		var person: String=definition.goal.actor_id
		var same_party: bool=person in Location.members(state,task.assignee)
		var location: Dictionary=Location.resolve(state,person)
		if not state.actors[person].alive:
			mark_failed(task,state.tick,"EscortPersonLost")
			continue
		if same_party and location.get("kind") in ["OnRoute","Wilderness"]: task.escort.departed=true
		if task.escort.departed and same_party and location.get("place_id")==definition.goal.destination_place_id:
			task.escort.arrival_tick=state.tick
			task.objective_completed_tick=state.tick
			task.status="ready"
			state.facts["escort:"+task.id]={"kind":"escort_arrival","task_id":task.id,"actor_id":person,"escort_actor_id":task.assignee,"place_id":definition.goal.destination_place_id,"tick":state.tick}

## 根据同一场战斗的真实保护对象结局更新合同；退避可继续护送，倒下须回报失败。
static func record_battle(state: Dictionary, data: Dictionary, battle: Dictionary) -> void:
	for task in state.tasks.values():
		var definition: Dictionary=data.offers[task.offer_id]
		if definition.template!="escort" or task.status!="active" or task.assignee!=battle.controller_id: continue
		var person: String=definition.goal.actor_id
		if not battle.units.has(person) or not battle.units[person].protected: continue
		if not battle.id in task.escort.battle_ids: task.escort.battle_ids.append(battle.id)
		if battle.outcome in ["protected_lost","defeat"]: mark_failed(task,state.tick,"ProtectedPersonDown" if battle.outcome=="protected_lost" else "EscortDefeated")

## 失败先形成待交接记录，不直接发钱或删除合同；实际委托人随后处理清算。
static func mark_failed(task: Dictionary, tick: int, reason: String) -> void:
	task.escort.failure=reason; task.escort.failed_tick=tick; task.status="ready"

## 校验护卫状态及其持久来源，防止只改ready标记绕过同行旅程或保护失败。
static func validate_task(state: Dictionary, data: Dictionary, task: Dictionary) -> Dictionary:
	if task.status=="accepting": return {"ok":true} if not task.has("escort") else fail("PrematureEscortState")
	# 受领中断的取消实例尚未建立实际旅程，不要求补造护卫记录。
	if task.status=="cancelled" and not task.has("escort"): return {"ok":true}
	var progress: Variant=task.get("escort")
	if not progress is Dictionary or progress.size()!=5 or not progress.get("departed") is bool or not progress.get("battle_ids") is Array or not progress.get("failure") is String: return fail("InvalidEscortState")
	for key in ["arrival_tick","failed_tick"]:
		if typeof(progress.get(key))!=TYPE_INT or progress[key]<-1 or progress[key]>state.tick: return fail("InvalidEscortTime")
	var seen: Array=[]
	for battle_id in progress.battle_ids:
		if not battle_id is String or battle_id in seen or not state.battles.has(battle_id) or not state.facts.has(battle_id): return fail("MissingEscortBattle")
		seen.append(battle_id)
		var battle: Dictionary=state.battles[battle_id]
		var person: String=data.offers[task.offer_id].goal.actor_id
		if battle.controller_id!=task.assignee or not battle.units.has(person) or not battle.units[person].protected or not battle.applied: return fail("WrongEscortBattle")
	if not progress.failure in ["","EscortPersonLost","ProtectedPersonDown","EscortDefeated"]: return fail("InvalidEscortFailure")
	if progress.failure.is_empty()!=bool(progress.failed_tick==-1): return fail("InvalidEscortFailureTime")
	if task.status in ["ready","success"] and progress.failure.is_empty():
		var fact: Variant=state.facts.get("escort:"+task.id)
		var goal: Dictionary=data.offers[task.offer_id].goal
		if not progress.departed or progress.arrival_tick<task.accepted_tick or task.get("objective_completed_tick")!=progress.arrival_tick: return fail("EscortNotArrived")
		if not fact is Dictionary or fact.get("kind")!="escort_arrival" or fact.get("task_id")!=task.id or fact.get("tick")!=progress.arrival_tick or fact.get("actor_id")!=goal.actor_id or fact.get("escort_actor_id")!=task.assignee or fact.get("place_id")!=goal.destination_place_id: return fail("MissingEscortArrival")
	if task.status=="success" and not progress.failure.is_empty(): return fail("FailedEscortRewarded")
	return {"ok":true}

## 返回可显示的领域错误，由统一事务保留原世界状态。
static func fail(code: String) -> Dictionary:
	return {"ok":false,"code":code}
