## 共享资格合同在真实个人战begin/规划校验接线；技能来源与XP门不由战斗副本冒充。
extends SceneTree
const Personal=preload("res://core/personal_combat.gd")
const ConditionFixture=preload("res://tests/test_personal_condition_snapshot.gd")
const Counter=preload("res://core/domains/personal/personal_counter.gd")
var checks: int=0
var failures: int=0

## 汇总资格接缝结果，失败保留具体标签和非零退出。
func check(value: bool, label: String) -> void:
	checks+=1
	if not value: failures+=1; printerr("FAIL: ",label)

## 显式前置只放在新技能；无前置旧本领用对照证明不被收紧。
static func content() -> Dictionary:
	var data: Dictionary=ConditionFixture.data()
	data.personal_skills["legacy"]=data.personal_skills.guard.duplicate(true); data.personal_skills.legacy.id="legacy"
	data.personal_skills.guard.required_skill_level={"skill_id":"martial","level":3}
	data.personal_skills.guard.counter={"damage_bp":4000}
	data.personal_skills["qualified.heart"]=data.personal_skills.guard.duplicate(true)
	data.personal_skills["qualified.heart"].id="qualified.heart"; data.personal_skills["qualified.heart"].kind="heart"; data.personal_skills["qualified.heart"].erase("counter")
	return data

## 学习账和普通XP保持世界真值，战斗只有合格的深拷贝。
static func source(xp: int) -> Dictionary:
	var state: Dictionary=ConditionFixture.state()
	state.facts={}; state.tasks={}
	state.actors.a.skills.martial=xp
	state.actors.a.combat.learned={"guard":100,"qualified.heart":100,"legacy":20}
	state.actors.a.combat.heart_id="qualified.heart"
	return state

## 验证门槛两侧、心法过滤、伪快照、实际终局与历史不追改。
func _initialize() -> void:
	var data: Dictionary=content(); var state: Dictionary=source(649); var before: Dictionary=state.actors.a.duplicate(true)
	var quote: Dictionary=Personal.preview(state,data,"a","duel_start",ConditionFixture.params())
	check(quote.ok and Personal.begin(state,data,quote).ok,"insufficient optional new skill does not remove ordinary sparring")
	var battle: Dictionary=state.battles.values()[0]
	check(battle.units.a.learned=={"legacy":20},"begin filters only explicit unqualified skills")
	check(battle.units.a.heart_id=="","unqualified heart cannot remain active after learned filtering")
	check(state.actors.a==before,"qualification does not edit actor equipment or mastery ledger")
	check(Personal.validate_state(state,data).ok,"filtered planning snapshot is accepted")
	var forged: Dictionary=state.duplicate(true); forged.battles[battle.id].units.a.learned["guard"]=100
	check(Personal.validate_state(forged,data).get("code")=="InvalidQualifiedBattleSkill","planning cannot inject ineligible qualified skill")
	state=source(650); quote=Personal.preview(state,data,"a","duel_start",ConditionFixture.params()); check(Personal.begin(state,data,quote).ok,"actual martial3 XP admits snapshot")
	battle=state.battles.values()[0]
	check(battle.units.a.learned==state.actors.a.combat.learned and battle.units.a.heart_id=="qualified.heart","qualified skills and heart preserve exact original mastery")
	forged=state.duplicate(true); forged.battles[battle.id].units.a.learned.guard=99
	check(Personal.validate_state(forged,data).get("code")=="InvalidQualifiedBattleSkill","planning rejects forged mastery that differs from actor source")
	check(Personal.validate_state(state,data).ok,"qualified planning snapshot is accepted")
	data.personal_objectives.spar.max_rounds=1
	var params: Dictionary={"battle_id":battle.id,"destination":battle.units.a.position.duplicate(),"heading":0,"action":"wait"}
	quote=Personal.preview(state,data,"a","battle_plan",params)
	check(quote.ok and Personal.apply(state,data,"a","battle_plan",quote).ok,"normal personal round produces actual terminal fact")
	quote=Personal.preview(state,data,"a","battle_close",{"battle_id":battle.id})
	check(quote.ok and Personal.apply(state,data,"a","battle_close",quote).ok,"normal result closes without recomputing skill source")
	state.actors.a.skills.martial=0; state.actors.a.combat.learned.guard=0
	check(Personal.validate_state(state,data).ok,"closed history is not reinterpreted through later eligibility")
	var invalid: Dictionary=data.personal_skills.guard.duplicate(true); invalid.erase("required_skill_level")
	check(Counter.validate_skill(invalid).get("code")=="PERSONAL_COUNTER_REQUIREMENT","counter definition cannot omit the declared martial3 prerequisite")
	print("PERSONAL_QUALIFIED_SNAPSHOT checks=",checks," failures=",failures)
	quit(1 if failures else 0)
