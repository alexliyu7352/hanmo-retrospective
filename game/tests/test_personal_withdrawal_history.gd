## 实际个人入口的NPC撤离事实与唯一世界回写；脱离不成为倒地、击杀、重复报酬或重新解释历史。
extends SceneTree
const Personal=preload("res://core/personal_combat.gd")
const Round=preload("res://core/personal_round.gd")
const Sessions=preload("res://core/combat_session.gd")
const Source=preload("res://tests/test_personal_condition_snapshot.gd")
var checks: int=0
var failures: int=0

## 对真实入口输出与原世界副作用分别断言，失败保持非零退出。
func check(value: bool, label: String) -> void:
	checks+=1
	if not value: failures+=1; printerr("FAIL: ",label)

## 由已有同地切磋入口建立会话，当前战斗损伤用于模拟已经打到撤离阈值的现场。
func _initialize() -> void:
	var data: Dictionary=Source.data()
	data.personal_objectives.spar.allow_withdrawal=true
	data.personal_objectives.spar.injury_on_defeat=1
	data.personal_battlefields.yard.spawns=[{"side":0,"position":[50,200],"heading":0},{"side":1,"position":[480,200],"heading":180}]
	data.personal_battlefields.yard.exits=[{"side":0,"position":[50,200],"radius":28},{"side":1,"position":[550,200],"radius":28}]
	var state: Dictionary=Source.state(); state.facts={}; state.tasks={}
	var quote: Dictionary=Personal.preview(state,data,"a","duel_start",Source.params())
	check(quote.ok and Personal.begin(state,data,quote).ok,"real preview and begin establish source identity")
	var battle: Dictionary=state.battles.values()[0]
	battle.units.b.hp=25
	battle.ai_plans=Round.make_ai(battle,data)
	check(Personal.validate_state(state,data).ok and battle.ai_plans.b.action=="withdraw","battle-only damage preserves world entry HP and generates committed exit intention")
	var balances: Dictionary=state.wallets.duplicate(true); var goods: Dictionary=state.inventories.duplicate(true)
	var plan: Dictionary={"battle_id":battle.id,"destination":battle.units.a.position.duplicate(),"heading":0,"action":"wait"}
	quote=Personal.preview(state,data,"a","battle_plan",plan)
	check(quote.ok and Personal.apply(state,data,"a","battle_plan",quote).ok,"actual personal apply finishes NPC tactical exit")
	battle=state.battles[battle.id]
	var fact: Dictionary=state.facts[battle.id]
	check(battle.phase=="result" and battle.outcome=="victory" and fact.withdrawn==["b"],"victory keeps explicit exact living-withdrawal fact")
	check(fact.defeated==[] and fact.participants==["a","b"],"withdrawn participant is neither removed from history nor added to defeated")
	check(state.actors.b.hp==25 and state.actors.b.injury==0 and state.actors.b.alive,"world HP preserved and no false injury or death")
	check(state.actors.a.fatigue==6 and state.actors.b.fatigue==6,"original once-per-session fatigue writeback applies exactly once")
	check(state.wallets==balances and state.inventories==goods,"tactical withdrawal creates no coins, loot or asset loss")
	check(Personal.validate_state(state,data).ok,"persisted result and exact fact agree")
	check(Sessions.occupied(state,"b") and Sessions.pauses_world(state),"departed NPC remains occupied until whole result closes")
	var before: Dictionary=state.duplicate(true)
	check(Personal._finish(state,data,battle).get("code")=="BattleResultAlreadyApplied" and state==before,"second settlement rejected before any HP, fatigue or fact write")
	check(Personal.preview(state,data,"a","battle_plan",plan).get("code")=="BattleAlreadyResolved","resolved round cannot be submitted again")
	var forged: Dictionary=state.duplicate(true); forged.facts[battle.id].withdrawn=[]
	check(Personal.validate_state(forged,data).get("code")=="BattleWithdrawalMismatch","empty list cannot hide actual withdrawn actor")
	forged=state.duplicate(true); forged.facts[battle.id].withdrawn=["a","b"]
	check(Personal.validate_state(forged,data).get("code")=="BattleWithdrawalMismatch","superset list cannot invent another withdrawal")
	forged=state.duplicate(true); forged.facts[battle.id].withdrawn=["b","b"]
	check(Personal.validate_state(forged,data).get("code")=="BattleWithdrawalMismatch","duplicate list cannot multiply withdrawal facts")
	forged=state.duplicate(true); forged.facts[battle.id].erase("withdrawn")
	check(Personal.validate_state(forged,data).get("code")=="BattleWithdrawalMismatch","missing list cannot discard new state semantics")
	forged=state.duplicate(true); forged.battles[battle.id].units.b.hp=0; forged.facts[battle.id].defeated=["b"]
	check(Personal.validate_state(forged,data).get("code")=="InvalidWithdrawalState","coordinated defeated edit still cannot turn escaped actor into dead combatant")
	quote=Personal.preview(state,data,"a","battle_close",{"battle_id":battle.id})
	check(quote.ok and Personal.apply(state,data,"a","battle_close",quote).ok and not Sessions.occupied(state,"b"),"ordinary close releases shared session without second settlement")
	state.actors.b.hp=100; state.actors.b.location={"kind":"Settlement","place_id":"elsewhere"}
	check(Personal.validate_state(state,data).ok and state.battles[battle.id].units.b.hp==25 and state.facts[battle.id].withdrawn==["b"],"later world healing and travel do not rewrite local exit history")
	print("PERSONAL_WITHDRAWAL_HISTORY checks=",checks," failures=",failures)
	quit(1 if failures else 0)
