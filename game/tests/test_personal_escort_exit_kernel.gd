## 复用现真实护卫旅程证明新战术到达不清算任务；模型在独立内存目录启用，不冒称作者Schema已发布。
extends SceneTree
const Kernel=preload("res://core/world_kernel.gd")
const F=preload("res://tests/companion_fixture.gd")
const Encounters=preload("res://core/encounter_rules.gd")
const Sessions=preload("res://core/combat_session.gd")
var checks: int=0
var failures: int=0

## 仅覆盖目标模型与任务/道路原结算接缝，任何失败均可定位。
func check(value: bool, label: String) -> void:
	checks+=1
	if not value: failures+=1; printerr("FAIL: ",label)

## 沿原半日行程寻找真实持械旅人，不创建敌方替身或篡改任务状态。
func conflict(kernel) -> Dictionary:
	for tick in range(12):
		var state: Dictionary=kernel.snapshot()
		var meeting: Dictionary=Encounters.pending_for(state,state.player_actor_id)
		if not meeting.is_empty():
			if kernel.preview("encounter_fight",{"encounter_id":meeting.id}).ok: return meeting
			if not F.send(kernel,"encounter_leave",{"encounter_id":meeting.id}).ok: return {}
		if not kernel.advance_one_tick().ok: return {}
	return {}

## 关闭战果后继续同一世界旅程，真实目的地位置仍由原行动结算产生。
func travel_done(kernel) -> bool:
	for tick in range(20):
		var state: Dictionary=kernel.snapshot()
		if state.actors[state.player_actor_id].location.kind=="Settlement": return true
		var meeting: Dictionary=Encounters.pending_for(state,state.player_actor_id)
		if not meeting.is_empty() and not F.send(kernel,"encounter_leave",{"encounter_id":meeting.id}).ok: return false
		if not kernel.advance_one_tick().ok: return false
	return false

## 全程普通命令受托/同行/遇敌/原地保护/继续旅程，战术victory不是任务ready或发钱。
func _initialize() -> void:
	var kernel=Kernel.new(); var loaded: Dictionary=kernel.new_game("scenario.alpha_life","res://data/generated/alpha_life.hmcatalog.json")
	check(loaded.ok,"reuse existing tiny catalog without any prepare")
	if not loaded.ok: printerr(loaded); quit(1); return
	kernel._catalog.data.personal_objectives["objective.escort"].escort_model="exit_v1"
	check(F.send(kernel,"move_local",{"place_id":"west.tavern"}).ok,"actual actor reaches escort client")
	var task_params: Dictionary={"offer_id":"offer.escort","context":F.context(kernel,"role.sender")}
	check(F.send(kernel,"discover_offer",task_params).ok and F.send(kernel,"accept_offer",task_params).ok and kernel.advance_one_tick().ok,"accept real escort contract through original registered commands")
	check(F.send(kernel,"join_party",{"context":F.context(kernel,"role.carter")}).ok,"protected carter joins actual travelling party")
	F.send(kernel,"move_local",{"place_id":"west.street"})
	check(F.send(kernel,"travel",{"destination":"village.east"}).ok,"start real journey toward promised settlement")
	var meeting: Dictionary=conflict(kernel)
	check(not meeting.is_empty(),"true road conflict supplies encounter source")
	if meeting.is_empty(): quit(1); return
	check(F.send(kernel,"encounter_fight",{"encounter_id":meeting.id}).ok,"existing encounter command starts explicit escort objective")
	var before: Dictionary=kernel.snapshot(); var player: String=before.player_actor_id
	var battle: Dictionary=Sessions.for_actor(before,player)
	for turn in range(8):
		if battle.phase!="planning": break
		var submitted: Dictionary=F.send(kernel,"battle_plan",{"battle_id":battle.id,"destination":battle.units[player].position.duplicate(),"heading":0,"action":"defend"})
		if not submitted.ok: printerr(submitted); break
		battle=Sessions.for_actor(kernel.snapshot(),player)
	var after: Dictionary=kernel.snapshot(); var task: Dictionary=after.tasks.values()[0]
	check(battle.phase=="result" and battle.outcome=="victory" and battle.last_events.has({"action":"objective","code":"EscortExitReached"}),"actual protected actor legally reaches own battlefield exit")
	check(task.status=="active" and task.escort.battle_ids==[battle.id] and task.escort.arrival_tick==-1,"tactical arrival records battle but never invents world escort arrival")
	check(after.wallets==before.wallets and after.inventories==before.inventories,"tactical victory gives no client reward or duplicate goods")
	check(after.actors[player].location==before.actors[player].location and after.actors["actor.carter"].location==before.actors["actor.carter"].location and after.tick==before.tick,"battlefield exit does not teleport party or advance world half-day")
	check(F.send(kernel,"battle_close",{"battle_id":battle.id}).ok and travel_done(kernel),"close resumes actual source journey normally")
	check(F.send(kernel,"move_local",{"place_id":"east.home"}).ok,"party really moves to promised recipient home")
	task=kernel.snapshot().tasks.values()[0]
	check(task.status=="ready" and task.escort.arrival_tick>=task.accepted_tick,"only real destination arrival enables original later client settlement")
	print("PERSONAL_ESCORT_EXIT_KERNEL checks=",checks," failures=",failures)
	quit(1 if failures else 0)
