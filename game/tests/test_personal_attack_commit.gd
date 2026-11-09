## T38局部回归：共同移动后失去目标条件不扣进攻气势，也不追击或施加状态。
extends SceneTree
const Round=preload("res://core/personal_round.gd")
const Fixture=preload("res://tests/test_personal_directional_guard.gd")
var checks: int=0
var failures: int=0

## 汇总真实规则断言，失败退出非零，不将轮次错误当作免伤成功。
func check(value: bool, label: String) -> void:
	checks+=1
	if not value: failures+=1; printerr("FAIL: ",label)

## 在已知最小合法夹具上补一种原格式进攻秘技，不新增玩法字段。
static func content() -> Dictionary:
	var result: Dictionary=Fixture.data()
	result.personal_skills["strike"]={"id":"strike","kind":"strike","weapons":["sword"],"stamina_cost":15,"power_bp":20000,"defense_bp":10000,"move_bonus":0,"status":"slow","status_rounds":1}
	return result

## 玩家进攻已承诺的目标；敌方意图先保存为等待后退，不能因玩家选择重新规划。
static func battle() -> Dictionary:
	var state: Dictionary=Fixture.battle([310,200])
	state.units.defender.stamina=50
	state.units.defender.learned["strike"]=100
	state.ai_plans.attacker=Fixture.plan(state.units.attacker,"wait")
	return state

## 验证普攻、秘技、几何失败、守势预付及旧装填语义各自保持明确边界。
func _initialize() -> void:
	var data: Dictionary=content()
	for action in ["attack","skill"]:
		var state: Dictionary=battle()
		state.ai_plans.attacker.destination=[410,200]
		var intent: Dictionary=Fixture.plan(state.units.defender,action,"attacker","strike" if action=="skill" else "")
		var before: Dictionary=state.duplicate(true)
		var result: Dictionary=Round.resolve(state,data,intent)
		check(result.ok,"out of range resolves "+action)
		check(result.battle.units.defender.stamina==50,"unlaunched attack does not spend stamina "+action)
		check(result.battle.units.attacker.hp==100 and result.battle.units.attacker.statuses.is_empty(),"miss cannot damage or apply status "+action)
		check(result.battle.units.defender.position==[250,200],"no automatic pursuit "+action)
		check(state==before and result==Round.resolve(state,data,intent),"replay and input purity "+action)
		state=battle(); intent=Fixture.plan(state.units.defender,action,"attacker","strike" if action=="skill" else "")
		result=Round.resolve(state,data,intent)
		check(result.ok and result.battle.units.defender.stamina==(35 if action=="skill" else 40),"valid hit pays exactly one original cost "+action)
		check(result.battle.units.attacker.hp==(58 if action=="skill" else 79),"damage and mastery formula unchanged "+action)
	var state: Dictionary=battle()
	data.items.sword.weapon.arc_degrees=90
	var attack: Dictionary=Fixture.plan(state.units.defender,"skill","attacker","strike"); attack.heading=180
	var result: Dictionary=Round.resolve(state,data,attack)
	check(result.ok and result.battle.units.defender.stamina==50,"wrong-facing skill not charged")
	data=content(); data.items.sword.weapon.minimum_range=70
	result=Round.resolve(state,data,Fixture.plan(state.units.defender,"attack","attacker"))
	check(result.ok and result.battle.units.defender.stamina==50,"inside minimum range not charged")
	data=content(); data.personal_battlefields.yard.obstacles=[{"x":275,"y":175,"width":5,"height":50,"blocks_sight":true,"blocks_movement":false}]
	result=Round.resolve(state,data,Fixture.plan(state.units.defender,"skill","attacker","strike"))
	check(result.ok and result.battle.units.defender.stamina==50,"blocked sight does not spend attack skill")
	data=content(); state.units.defender.learned["guard"]=100
	result=Round.resolve(state,data,Fixture.plan(state.units.defender,"skill","","guard"))
	check(result.ok and result.battle.units.defender.stamina==35,"guard skill still prepays even without incoming attack")
	data.items.sword.weapon.recovery_rounds=1; state=battle(); state.ai_plans.attacker.destination=[410,200]
	result=Round.resolve(state,data,Fixture.plan(state.units.defender,"attack","attacker"))
	check(result.ok and result.battle.units.defender.recovery_until==3,"existing attempted-weapon recovery timing unchanged")
	print("PERSONAL_ATTACK_COMMIT checks=",checks," failures=",failures)
	quit(1 if failures else 0)
