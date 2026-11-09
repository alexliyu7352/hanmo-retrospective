## 普通防御保持原地的完整动作合同：合法转向、AI候选和护卫移动一并核验。
extends SceneTree
const Round=preload("res://core/personal_round.gd")
const Fixture=preload("res://tests/test_personal_directional_guard.gd")
const EscortFixture=preload("res://tests/test_personal_escort_withdrawal.gd")
var checks: int=0
var failures: int=0

## 记录真实合法性和候选结果，失败不进入下一阶段冒称成功。
func check(value: bool, label: String) -> void:
	checks+=1
	if not value: failures+=1; printerr("FAIL: ",label)

## 移动防御被拒绝的同时，已有移动等待和内容允许的守势秘技保持原能力。
func _initialize() -> void:
	var data: Dictionary=Fixture.data(); var state: Dictionary=Fixture.battle([310,200])
	var plan: Dictionary=Fixture.plan(state.units.defender,"defend"); plan.destination=[240,200]
	check(Round.validate_plan(state,data,"defender",plan).get("code")=="DefendRequiresStationary","basic defense cannot move")
	var before: Dictionary=state.duplicate(true)
	check(not Round.resolve(state,data,plan).ok and state==before,"invalid moving guard does not partially resolve")
	plan.destination=state.units.defender.position; plan.heading=180
	check(Round.validate_plan(state,data,"defender",plan).ok,"stationary defense may turn")
	check(Round.movement_distance(state,data,"defender","","defend")==0 and Round.movement_distance(state,data,"defender")==100,"optional action-aware query preserves old movement ABI and zeroes basic guard")
	plan.action="wait"; plan.destination=[240,200]
	check(Round.validate_plan(state,data,"defender",plan).ok,"existing movement wait remains legal")
	state.units.defender.learned["guard"]=100; plan.action="skill"; plan.skill_id="guard"
	check(Round.validate_plan(state,data,"defender",plan).ok,"guard skill keeps definition-based legal mobility")
	state=Fixture.battle([550,200])
	var ai: Dictionary=Round.make_ai(state,data)
	check(ai.attacker.destination!=state.units.attacker.position and ai.attacker.action=="wait","repositioning AI uses legal movement instead of moving basic guard")
	check(Round.validate_plan(state,data,"attacker",ai.attacker).ok,"repositioning intention remains legal")
	state=Fixture.battle([310,200]); state.units.attacker.hp=15; state.units.attacker.stamina=10
	ai=Round.make_ai(state,data)
	check(ai.attacker.action=="defend" and ai.attacker.destination==state.units.attacker.position,"survival AI compares actual stationary guard position")
	check(Round.validate_plan(state,data,"attacker",ai.attacker).ok,"survival intention remains legal")
	state=EscortFixture.battle(); data=EscortFixture.content()
	ai=Round.make_ai(state,data)
	check(ai["protected"].destination!=state.units["protected"].position and ai["protected"].action=="wait","protected follower moves with legal unguarded intent")
	check(Round.validate_plan(state,data,"protected",ai["protected"]).ok,"protected AI is legal after stance restriction")
	state.ai_plans=ai; state.ai_plans.attacker=Fixture.plan(state.units.attacker,"wait")
	check(Round.resolve(state,data,Fixture.plan(state.units.defender,"withdraw")).ok,"normal escort round still consumes committed intents")
	print("PERSONAL_STANCE_CONTRACT checks=",checks," failures=",failures)
	quit(1 if failures else 0)
