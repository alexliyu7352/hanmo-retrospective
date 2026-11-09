## 侧背伤害为目标显式启用的候选规则；未启用保旧数值，预览/实际/反击/AI共用一条倍率路径。
extends SceneTree
const Content=preload("res://core/personal_content.gd")
const Round=preload("res://core/personal_round.gd")
const Fixture=preload("res://tests/test_personal_directional_guard.gd")
const CounterFixture=preload("res://tests/test_personal_counter.gd")
var checks: int=0
var failures: int=0

## 汇总规则和旧默认边界，不把候选倍率局部通过称为平衡验收。
func check(value: bool, label: String) -> void:
	checks+=1
	if not value: failures+=1; printerr("FAIL: ",label)

## 在已有目标添加可选规则，其他字段及武器原公式不变。
static func content(enabled: bool=true) -> Dictionary:
	var data: Dictionary=Fixture.data()
	if enabled: data.personal_objectives.spar.facing_damage={"side_bp":11000,"back_bp":12500}
	return data

## 目标转向来自本轮已提交计划，攻击者不变位置，隔离方向倍率本身。
func _initialize() -> void:
	for enabled in [false,true]:
		var data: Dictionary=content(enabled)
		for row in [[180,21],[90,23],[0,26]]:
			var state: Dictionary=Fixture.battle([310,200]); state.units.attacker.heading=row[0]; state.ai_plans.attacker=Fixture.plan(state.units.attacker,"wait")
			var before: Dictionary=state.duplicate(true)
			var preview: Dictionary=Round.preview_attack(state,data,"defender",[250,200],0,"attacker")
			var result: Dictionary=Round.resolve(state,data,Fixture.plan(state.units.defender,"attack","attacker"))
			var expected: int=row[1] if enabled else 21
			check(preview.ok and preview.damage==expected,"optional facing preview "+str(enabled)+" heading "+str(row[0]))
			check(result.ok and result.battle.units.attacker.hp==100-expected,"actual damage uses same facing path")
			check(state==before,"directional queries and resolve preserve source")
	var data: Dictionary=content(); var state: Dictionary=Fixture.battle([310,200])
	state.ai_plans.attacker=Fixture.plan(state.units.attacker,"defend")
	var result: Dictionary=Round.resolve(state,data,Fixture.plan(state.units.defender,"attack","attacker"))
	check(result.ok and result.battle.units.attacker.hp==90,"front basic guard retains original half reduction")
	state.units.attacker.heading=0; state.ai_plans.attacker=Fixture.plan(state.units.attacker,"defend")
	result=Round.resolve(state,data,Fixture.plan(state.units.defender,"attack","attacker"))
	check(result.ok and result.battle.units.attacker.hp==74,"rear cannot add basic guard after rear bonus")
	data=CounterFixture.content(); data.personal_objectives.spar.facing_damage={"side_bp":11000,"back_bp":12500}
	state=CounterFixture.battle(); state.units.attacker.heading=0; state.ai_plans.attacker=Fixture.plan(state.units.attacker,"attack","defender")
	result=CounterFixture.resolve(state,data)
	check(result.ok and result.battle.units.attacker.hp==90,"counter uses same directional ordinary power before40 percent")
	var target: Dictionary=state.units.attacker
	var incoming: Array=[{"unit":state.units.defender,"weapon":data.items.sword.weapon,"skill":{}}]
	check(Round._incoming_damage(target,Fixture.plan(target,"wait"),incoming,data,1,data.personal_objectives.spar.facing_damage)==26,"AI danger comparison uses identical directional power")
	data=content(); state=Fixture.battle([310,200]); state.ai_plans.attacker.heading=0
	var preview: Dictionary=Round.preview_attack(state,data,"defender",[250,200],0,"attacker")
	result=Round.resolve(state,data,Fixture.plan(state.units.defender,"attack","attacker"))
	check(preview.damage==21 and result.battle.units.attacker.hp==74,"preview reads public current heading while actual uses submitted new heading")
	check(Content.validate({"personal_objectives":{"spar":{"id":"spar","facing_damage":{"side_bp":11000,"back_bp":12500}}}}).ok,"static content accepts the explicit frozen candidate profile")
	for bad in [{"side_bp":11000},{"side_bp":11000,"back_bp":12000},{"side_bp":11000.0,"back_bp":12500},{"side_bp":11000,"back_bp":12500,"front_bp":10000},"script"]:
		check(Content.validate({"personal_objectives":{"spar":{"id":"spar","facing_damage":bad}}}).get("code")=="PERSONAL_FACING_DAMAGE","malformed or unsupported profile is rejected statically")
	data=content(); data.personal_objectives.spar.facing_damage={"side_bp":11000}
	check(Round.preview_attack(Fixture.battle([310,200]),data,"defender",[250,200],0,"attacker").get("code")=="PERSONAL_FACING_DAMAGE","raw runtime callers also fail closed on malformed profile")
	print("PERSONAL_FACING_DAMAGE checks=",checks," failures=",failures)
	quit(1 if failures else 0)
