## 正式个人战撤离局部门：本轮真实近战命中阻止脱离，远射与未成立攻击不冒充近战缠斗。
extends SceneTree
const Round=preload("res://core/personal_round.gd")
const Fixture=preload("res://tests/test_personal_directional_guard.gd")
var checks: int=0
var failures: int=0

## 保留局部失败标签并使用非零退出，不生成世界结果或手工结算。
func check(value: bool, label: String) -> void:
	checks+=1
	if not value: failures+=1; printerr("FAIL: ",label)

## 标记己方当前真实出口；所有其他字段延续已有战斗格式。
static func content() -> Dictionary:
	var result: Dictionary=Fixture.data()
	result.personal_objectives.spar.allow_withdrawal=true
	result.personal_battlefields.yard.exits=[{"side":0,"position":[250,200],"radius":45}]
	return result

## 同样在出口等待，只有已保存攻击意图及其真实类别决定是否被缠住。
func _initialize() -> void:
	var data: Dictionary=content()
	var state: Dictionary=Fixture.battle([310,200])
	var before: Dictionary=state.duplicate(true)
	var intent: Dictionary=Fixture.plan(state.units.defender,"withdraw")
	var result: Dictionary=Round.resolve(state,data,intent)
	check(result.ok and result.battle.phase=="planning","actual melee hit blocks exit despite standing inside gate")
	check(result.battle.units.defender.hp==79,"blocked withdrawal still applies actual hit once")
	check(result.battle.last_events.has({"actor_id":"defender","action":"withdraw","code":"WithdrawalMeleeHit"}),"failure explains actual melee contact")
	check(state==before and result==Round.resolve(state,data,intent),"blocked withdrawal is deterministic and input-pure")
	if result.battle.phase=="planning":
		var next: Dictionary=result.battle
		next.ai_plans.attacker=Fixture.plan(next.units.attacker,"wait")
		var withdrawn: Dictionary=Round.resolve(next,data,Fixture.plan(next.units.defender,"withdraw"))
		check(withdrawn.ok and withdrawn.battle.outcome=="withdrawn","a later clean round releases melee restriction")
	state=Fixture.battle([410,200])
	result=Round.resolve(state,data,Fixture.plan(state.units.defender,"withdraw"))
	check(result.ok and result.battle.outcome=="withdrawn","out-of-range melee attempt does not block exit")
	data.items.sword.weapon.category="bow"; data.items.sword.weapon.minimum_range=100; data.items.sword.weapon.maximum_range=300
	result=Round.resolve(state,data,Fixture.plan(state.units.defender,"withdraw"))
	check(result.ok and result.battle.units.defender.hp==79 and result.battle.outcome=="withdrawn","real ranged hit does not become melee restriction")
	data=content(); data.personal_battlefields.yard.obstacles=[{"x":275,"y":175,"width":5,"height":50,"blocks_sight":true,"blocks_movement":false}]
	state=Fixture.battle([310,200]); result=Round.resolve(state,data,Fixture.plan(state.units.defender,"withdraw"))
	check(result.ok and result.battle.outcome=="withdrawn","obstructed melee cannot claim a hit")
	data=content(); state=Fixture.battle([310,200]); state.units.defender.hp=1
	result=Round.resolve(state,data,Fixture.plan(state.units.defender,"withdraw"))
	check(result.ok and result.battle.outcome=="defeat","defeat priority remains before withdrawal")
	print("PERSONAL_WITHDRAWAL_RULES checks=",checks," failures=",failures)
	quit(1 if failures else 0)
