## 显式护送到出口目标；关键失败先判、真到达与安全撤离区分、超时不能用歼敌冒完成。
extends SceneTree
const Round=preload("res://core/personal_round.gd")
const Objectives=preload("res://core/domains/personal/personal_objectives.gd")
const Geometry=preload("res://core/personal_geometry.gd")
const Fixture=preload("res://tests/test_personal_escort_withdrawal.gd")
const UnitFixture=preload("res://tests/test_personal_directional_guard.gd")
var checks: int=0
var failures: int=0

## 单独记录目标规则断言，不启动世界准备或界面。
func check(value: bool, label: String) -> void:
	checks+=1
	if not value: failures+=1; printerr("FAIL: ",label)

## 新模型只在显式escort目标启用，旧场地与真实圆身坐标不变。
static func content() -> Dictionary:
	var data: Dictionary=Fixture.content(); data.personal_objectives.spar.escort_model="exit_v1"
	return data

## 主角远离出口但活着，保护者可通过原合法导航自行抵达。
static func battle() -> Dictionary:
	var state: Dictionary=Fixture.battle(); state.units.defender.position=[350,350]
	return state

## 只固定对方等待以隔离保护者导航；不是把最终位置或结果直接写入。
static func step(state: Dictionary, data: Dictionary) -> Dictionary:
	state.ai_plans=Round.make_ai(state,data)
	if state.ai_plans.has("attacker"): state.ai_plans.attacker=UnitFixture.plan(state.units.attacker,"wait")
	return Round.resolve(state,data,UnitFixture.plan(state.units.defender,"wait"))

## 核实际每轮行走、目标顺序、历史理由与旧模型差异。
func _initialize() -> void:
	var data: Dictionary=content(); var state: Dictionary=battle(); var turns: int=0; var legal: bool=true
	while state.phase=="planning" and turns<10:
		var old: Array=state.units["protected"].position.duplicate()
		var result: Dictionary=step(state,data)
		if not result.ok: legal=false; break
		state=result.battle; turns+=1
		if not Geometry.can_move(data.personal_battlefields.yard,old,state.units["protected"].position,90,18).ok: legal=false
	check(legal and state.phase=="result" and state.outcome=="victory","protected actor actually walks to friendly exit without leader first reaching it")
	check(state.units.defender.position==[350,350] and not state.units["protected"].has("withdrawn_round"),"arrival victory neither requires leader at exit nor invents safe-withdrawal state")
	check(state.last_events.has({"action":"objective","code":"EscortExitReached"}) and Objectives.validate_result(state,data.personal_objectives.spar,data.personal_battlefields.yard).ok,"arrival result persists explicit valid reason")
	var forged: Dictionary=state.duplicate(true); forged.units["protected"].position=[300,200]
	check(not Objectives.validate_result(forged,data.personal_objectives.spar,data.personal_battlefields.yard).ok,"forged victory away from actual exit is rejected")
	forged=state.duplicate(true); forged.last_events.erase({"action":"objective","code":"EscortExitReached"})
	check(Objectives.validate_result(forged,data.personal_objectives.spar,data.personal_battlefields.yard).get("code")=="MissingEscortOutcomeReason","goal reason cannot silently disappear from persisted result")
	state=battle(); state.units.attacker.hp=0
	var result: Dictionary=step(state,data)
	check(result.ok and result.battle.phase=="planning","eliminating all enemies cannot replace protected arrival in explicit model")
	state=battle(); state.units["protected"].hp=0; state.units.attacker.hp=0; result=step(state,data)
	check(result.ok and result.battle.outcome=="protected_lost","critical protected loss beats simultaneous last enemy defeat")
	state=battle(); state.units["protected"].position=[60,200]; state.units.defender.hp=0; result=step(state,data)
	check(result.ok and result.battle.outcome=="defeat","controller death takes precedence over otherwise successful protected arrival")
	state=battle(); state.units["protected"].position=[60,200]; state.units["protected"].hp=0; result=step(state,data)
	check(result.ok and result.battle.outcome=="protected_lost","dead protected body in exit is not a completed escort")
	state=battle(); state.round=20; state.units["protected"].position=[450,300]; result=step(state,data)
	check(result.ok and result.battle.outcome=="draw" and result.battle.last_events.has({"action":"objective","code":"EscortExitTimeout"}),"unreached exit at existing round limit yields explicit timeout truce")
	state=battle(); state.round=20; state.units["protected"].position=[120,240]; result=step(state,data)
	check(result.ok and result.battle.outcome=="victory","actual last-round arrival wins instead of unconditional timeout draw")
	state=battle(); state.units["protected"].position=[60,200]; state.units["second"]=UnitFixture.unit("second",0,[300,270],180); state.units.second.protected=true
	Fixture.commit_ai(state,data); result=Round.resolve(state,data,UnitFixture.plan(state.units.defender,"wait"))
	check(result.ok and result.battle.phase=="planning","all real protected participants must arrive, not only first dictionary entry")
	state=battle(); state.units["protected"].position=[120,240]; state.units.attacker.position=[120,160]
	Fixture.commit_ai(state,data); state.ai_plans.attacker=UnitFixture.plan(state.units.attacker,"attack","protected")
	result=Round.resolve(state,data,UnitFixture.plan(state.units.defender,"wait"))
	check(result.ok and result.battle.outcome=="victory" and result.battle.units["protected"].hp==79 and not result.battle.units["protected"].has("withdrawn_round"),"surviving goal arrival under hit is not falsely labelled safe withdrawal")
	state=battle(); state.units.erase("protected"); result=step(state,data)
	check(result.get("code")=="ProtectedPersonMissing","explicit escort objective cannot win without actual protected identity")
	state=battle(); state.units["protected"].side=1; result=step(state,data)
	check(result.get("code")=="InvalidEscortProtectedSide","opponent cannot stand in for friendly protected person")
	data=Fixture.content(); state=battle(); state.units["protected"].position=[60,200]; result=step(state,data)
	check(result.ok and result.battle.phase=="planning","old escort with omitted model does not gain new automatic arrival victory")
	state=battle(); state.units.attacker.hp=0; result=step(state,data)
	check(result.ok and result.battle.outcome=="victory","old objective keeps its original enemy-elimination result")
	for malformed in [{"kind":"sparring","escort_model":"exit_v1"},{"kind":"escort","escort_model":"unknown"},{"kind":"escort","escort_model":true}]:
		check(not Objectives.validate_objective(malformed).ok,"unknown or non-escort model rejected")
	print("PERSONAL_ESCORT_EXIT checks=",checks," failures=",failures)
	quit(1 if failures else 0)
