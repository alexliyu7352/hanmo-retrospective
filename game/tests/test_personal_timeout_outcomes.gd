## 切磋超时使用精确体力比例；10个百分点是本片采用的既有候选值，不代表平衡验收。
extends SceneTree
const Round=preload("res://core/personal_round.gd")
const Fixture=preload("res://tests/test_personal_directional_guard.gd")
var checks: int=0
var failures: int=0

## 逐条保留裁决边界的真实结果，失败退出非零。
func check(value: bool, label: String) -> void:
	checks+=1
	if not value: failures+=1; printerr("FAIL: ",label)

## 双方原地等待越过既有目标轮上限，排除攻击随机或UI干扰。
func outcome(hp: int, initial_hp: int, enemy_hp: int, enemy_initial: int, kind: String="sparring") -> String:
	var data: Dictionary=Fixture.data(); data.personal_objectives.spar.kind=kind
	var state: Dictionary=Fixture.battle([310,200]); state.round=10
	state.units.defender.hp=hp; state.units.defender.initial_hp=initial_hp
	state.units.attacker.hp=enemy_hp; state.units.attacker.initial_hp=enemy_initial
	state.ai_plans.attacker=Fixture.plan(state.units.attacker,"wait")
	var before: Dictionary=state.duplicate(true)
	var result: Dictionary=Round.resolve(state,data,Fixture.plan(state.units.defender,"wait"))
	check(result.ok and state==before,"limit resolution is valid and input-pure")
	return result.battle.outcome if result.ok else result.get("code","")

## 包括同值不同初始体力、整分数边界、非切磋不变和终局优先级。
func _initialize() -> void:
	check(outcome(60,100,50,100)=="victory","exactly ten percentage points is judged victory")
	check(outcome(50,100,60,100)=="defeat","exactly ten points behind is judged defeat")
	check(outcome(59,100,50,100)=="draw","nine points remains draw")
	check(outcome(50,100,50,50)=="defeat","equal absolute hp does not erase unequal remaining percentage")
	check(outcome(29,60,23,60)=="victory","exact rational ten-percent boundary")
	check(outcome(30,61,24,61)=="draw","fraction below threshold must not round to ten percent")
	check(outcome(31,61,24,61)=="victory","fraction above threshold is judged without float rounding")
	check(outcome(99,100,1,100,"escort")=="draw","escort timeout is not overridden by sparring percentage")
	check(outcome(99,100,1,100,"skirmish")=="draw","other objective keeps established limit rule")
	check(outcome(0,100,0,100)=="defeat","simultaneous knockdown still follows existing controller defeat priority")
	check(outcome(1,100,0,100)=="victory","enemy down remains real victory before percentage comparison")
	var data: Dictionary=Fixture.data(); data.personal_objectives.spar.kind="sparring"
	var state: Dictionary=Fixture.battle([310,200]); state.round=10
	state.units.defender.hp=50; state.units.attacker.hp=70; state.ai_plans.attacker=Fixture.plan(state.units.attacker,"wait")
	var result: Dictionary=Round.resolve(state,data,Fixture.plan(state.units.defender,"attack","attacker"))
	check(result.ok and result.battle.outcome=="draw" and result.battle.units.attacker.hp==49,"limit comparison uses hp after the current simultaneous round")
	state=Fixture.battle([310,200]); state.round=10
	state.units["protected"]=Fixture.unit("protected",0,[100,100],0); state.units["protected"].hp=0; state.units["protected"].protected=true
	result=Round.resolve(state,data,Fixture.plan(state.units.defender,"wait"))
	check(result.ok and result.battle.outcome=="protected_lost","critical protected failure stays above timeout judgement")
	print("PERSONAL_TIMEOUT_OUTCOMES checks=",checks," failures=",failures)
	quit(1 if failures else 0)
