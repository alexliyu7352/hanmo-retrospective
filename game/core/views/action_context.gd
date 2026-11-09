## 当前玩家行动控制的有限读取；基础字段不触碰战斗/事实窗口，按需判定沿原算法。
extends RefCounted
const Values=preload("res://core/base/value_checks.gd")
const CombatSession=preload("res://core/combat_session.gd")
const SiegeWorld=preload("res://core/siege_world.gd")
const Encounters=preload("res://core/encounter_rules.gd")

## 基础六键只读当前行动；未请求的战斗、围城、会面和课程事实容器完全不访问。
static func query(state: Dictionary) -> Dictionary:
	if state.is_empty(): return {"ok":false,"code":"NoWorld"}
	if not Values.fields(state,{"player_actor_id":TYPE_STRING,"revision":TYPE_INT,"actors":TYPE_DICTIONARY}) or not Values.whole(state.revision): return _fail()
	var actor_id: String=state.player_actor_id
	var actor: Variant=state.actors.get(actor_id)
	if not Values.fields(actor,{"action_id":TYPE_STRING}): return _fail()
	var result: Dictionary={"ok":true,"actor_id":actor_id,"revision":state.revision,"action_id":actor.action_id,"action_kind":"","travel_paused":false}
	if actor.action_id.is_empty(): return result
	if not state.get("actions") is Dictionary: return _fail()
	var action: Variant=state.actions.get(actor.action_id)
	# 共享行动的被占用参与者可以不是发起人；这里只绑定当前行动，成员资格仍由世界校验负责。
	if not Values.fields(action,{"id":TYPE_STRING,"kind":TYPE_STRING}) or action.id!=actor.action_id: return _fail()
	result.action_kind=action.kind
	if action.kind=="travel":
		if not action.get("params") is Dictionary or not action.params.get("paused",false) is bool: return _fail()
		result.travel_paused=action.params.get("paused",false)
	return result

## 原state型调用者的兼容判定；只按当前行动种类判断，不把原野停留等同于旅行。
static func travelling(state: Dictionary) -> bool:
	var actor: Dictionary=state.actors[state.player_actor_id]
	return not actor.action_id.is_empty() and state.actions[actor.action_id].kind=="travel"

## 按原Combat、Siege、Encounter顺序短路；仅在Main原调用点执行，不预计算成自动授权。
static func needs_input(state: Dictionary) -> bool:
	var actor: String=state.player_actor_id
	return CombatSession.occupied(state,actor) or not SiegeWorld.for_actor(state,actor).is_empty() or not Encounters.pending_for(state,actor).is_empty()

## 手动题面答完或退出后课程仍继续，未完成真实行动都不由界面偷偷推进首半日。
static func manual_window(state: Dictionary, actor_id: String) -> bool:
	var actor: Dictionary=state.get("actors",{}).get(actor_id,{})
	var action: Dictionary=state.get("actions",{}).get(actor.get("action_id",""),{})
	if not action.get("kind") in ["study_skill","military_train"] or action.get("actor_id")!=actor_id: return false
	var fact: Dictionary=state.get("facts",{}).get(action.get("id",""),{})
	if fact.get("phase")!="started" or fact.get("actor_id")!=actor_id: return false
	var terms: Dictionary=fact.get("terms",{})
	var offered: Variant=terms.get("drill",{}) if action.kind=="military_train" else terms.get("challenge",terms.get("etiquette",{}))
	return offered is Dictionary and offered.get("mode")=="manual"

## 新基础投影的坏必要来源错误，不修改已有两个bool判定的旧输入边界。
static func _fail() -> Dictionary: return {"ok":false,"code":"InvalidActionContext"}
