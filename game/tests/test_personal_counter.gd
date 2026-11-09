## 反击纯个人战合同：事先声明、首个合法近身机会、40%基础与目标减伤、同步倒地且不连锁。
extends SceneTree
const Counter=preload("res://core/domains/personal/personal_counter.gd")
const Content=preload("res://core/personal_content.gd")
const Round=preload("res://core/personal_round.gd")
const Fixture=preload("res://tests/test_personal_directional_guard.gd")
var checks: int=0
var failures: int=0

## 记录规则断言，失败与脚本退出都明确呈现，不将局部门当正式作者接入。
func check(value: bool, label: String) -> void:
	checks+=1
	if not value: failures+=1; printerr("FAIL: ",label)

## 反击只是在已有guard定义上声明固定比例，不依赖招式ID，也不隐含额外防御加成。
static func content() -> Dictionary:
	var data: Dictionary=Fixture.data()
	data.personal_skills.guard.stamina_cost=25
	data.personal_skills.guard.defense_bp=10000
	data.personal_skills.guard.counter={"damage_bp":4000}
	data.personal_skills.guard.required_skill_level={"skill_id":"martial","level":3}
	return data

## 始终使用已有技能意图；缺掌握或气势仍由原技能资格规则拦截。
static func battle() -> Dictionary:
	var state: Dictionary=Fixture.battle([310,200])
	state.units.defender.learned["guard"]=100
	return state

## 声明反击一次并执行正式同步轮次；返回原子结果便于检查输入和事件。
static func resolve(state: Dictionary, data: Dictionary) -> Dictionary:
	return Round.resolve(state,data,Fixture.plan(state.units.defender,"skill","","guard"))

## 覆盖所有已冻结方向、射程、费用、机会顺序和基础/减伤阶段边界。
func _initialize() -> void:
	var data: Dictionary=content(); var state: Dictionary=battle(); var before: Dictionary=state.duplicate(true)
	var result: Dictionary=resolve(state,data)
	check(result.ok and result.battle.units.attacker.hp==92,"ordinary base21 produces fixed eight counter damage")
	check(result.battle.units.defender.hp==79 and result.battle.units.defender.stamina==75,"declared counter pays25 once and has no invented reduction")
	check(state==before and result==resolve(state,data),"counter is input-pure and replay stable")
	state=battle(); state.ai_plans.attacker=Fixture.plan(state.units.attacker,"wait")
	result=resolve(state,data)
	check(result.ok and result.battle.units.defender.stamina==75 and result.battle.units.attacker.hp==100,"preparation cost is paid even with no trigger")
	state=battle(); state.units.defender.hp=10; state.units.attacker.hp=8
	result=resolve(state,data)
	check(result.ok and result.battle.units.defender.hp==0 and result.battle.units.attacker.hp==0,"counter survives simultaneous fatal incoming hit")
	check(result.battle.outcome=="defeat","counter does not replace existing simultaneous defeat priority")
	state=battle(); state.units["z.attacker"]=Fixture.unit("z.attacker",1,[250,260],270); state.ai_plans["z.attacker"]=Fixture.plan(state.units["z.attacker"],"attack","defender")
	result=resolve(state,data)
	check(result.ok and result.battle.units.attacker.hp==92 and result.battle.units["z.attacker"].hp==100,"multiple incoming hits trigger only first stable legal opportunity")
	data.items.sword.weapon.arc_degrees=90; state=Fixture.battle([190,200]); state.units.defender.learned["guard"]=100
	state.units.attacker.heading=0; state.ai_plans.attacker=Fixture.plan(state.units.attacker,"attack","defender")
	state.units["z.attacker"]=Fixture.unit("z.attacker",1,[310,200],180); state.ai_plans["z.attacker"]=Fixture.plan(state.units["z.attacker"],"attack","defender")
	result=resolve(state,data)
	check(result.ok and result.battle.units.attacker.hp==100 and result.battle.units["z.attacker"].hp==92,"illegal rear opportunity does not consume first legal front counter")
	data=content(); data.items["long_weapon"]=data.items.sword.duplicate(true); data.items.long_weapon.weapon.category="spear"; data.items.long_weapon.weapon.maximum_range=150
	state=battle(); state.units.attacker.position=[370,200]; state.units.attacker.weapon_id="long_weapon"; state.ai_plans.attacker=Fixture.plan(state.units.attacker,"attack","defender")
	result=resolve(state,data)
	check(result.ok and result.battle.units.defender.hp==79 and result.battle.units.attacker.hp==100,"long-weapon hit cannot grant short-weapon counter extra range")
	data.items.long_weapon.weapon.category="bow"; state=battle(); state.units.attacker.weapon_id="long_weapon"; state.ai_plans.attacker=Fixture.plan(state.units.attacker,"attack","defender")
	result=resolve(state,data)
	check(result.ok and result.battle.units.attacker.hp==100,"ranged weapon hit does not trigger melee counter")
	data=content(); data.personal_battlefields.yard.obstacles=[{"x":275,"y":175,"width":5,"height":50,"blocks_sight":true,"blocks_movement":false}]; state=battle()
	result=resolve(state,data)
	check(result.ok and result.battle.units.defender.hp==100 and result.battle.units.attacker.hp==100,"blocked original attack cannot trigger a counter through wall")
	data=content(); data.items.sword.weapon.damage=7
	data.personal_skills["heart.target"]={"id":"heart.target","kind":"heart","weapons":["sword"],"stamina_cost":0,"power_bp":10000,"defense_bp":7500,"move_bonus":0,"status":"none","status_rounds":0}
	state=battle(); state.units.attacker.learned["heart.target"]=100; state.units.attacker.heart_id="heart.target"
	result=resolve(state,data)
	check(result.ok and result.battle.units.attacker.hp==97,"counter multiplies base10 by40 percent before target75 percent reduction")
	data.personal_skills["heart.source"]=data.personal_skills["heart.target"].duplicate(true); data.personal_skills["heart.source"].id="heart.source"; data.personal_skills["heart.source"].power_bp=15000; data.personal_skills["heart.source"].defense_bp=10000
	state.units.defender.learned["heart.source"]=100; state.units.defender.heart_id="heart.source"
	result=resolve(state,data)
	check(result.ok and result.battle.units.attacker.hp==96,"counter includes existing attacker heart before ratio without second mastery scale")
	data=content(); state=battle(); state.units.defender.learned["guard"]=1
	result=resolve(state,data)
	check(result.ok and result.battle.units.attacker.hp==92,"fixed counter percentage does not invert mastery growth")
	data=content(); state=battle()
	var hits: Array=[{"actor_id":"attacker","target_id":"defender","action":"attack","code":"Hit"},{"actor_id":"defender","target_id":"attacker","action":"counter","code":"Hit"}]
	var selected: Dictionary=Counter.select(state,data,{"defender":4000,"attacker":4000},hits)
	check(selected.ok and selected.reactions.size()==1 and selected.reactions[0].actor_id=="defender","reaction events cannot trigger counter-counter recursion")
	hits.reverse()
	check(Counter.select(state,data,{"attacker":4000,"defender":4000},hits)==selected,"same real hit set has stable order independent of input arrays")
	check(Content.validate({"personal_skills":{"guard":data.personal_skills.guard}}).ok,"domain content validates frozen optional counter contract")
	for malformed in [{"damage_bp":3000},{"damage_bp":4000.0},{"damage_bp":4000,"repeat":true},{}]:
		var invalid: Dictionary=data.personal_skills.guard.duplicate(true); invalid.counter=malformed
		check(Content.validate({"personal_skills":{"guard":invalid}}).get("code")=="PERSONAL_COUNTER_DEFINITION","unsupported counter shape fails before execution")
	var wrong_kind: Dictionary=data.personal_skills.guard.duplicate(true); wrong_kind.kind="strike"
	check(not Counter.validate_skill(wrong_kind).ok,"counter is not a second strike effect kind")
	wrong_kind=data.personal_skills.guard.duplicate(true); wrong_kind.weapons=["bow"]
	check(Counter.validate_skill(wrong_kind).get("code")=="PERSONAL_COUNTER_WEAPON","counter cannot silently activate with a ranged weapon")
	state=battle(); state.units.defender.learned={}
	check(resolve(state,data).get("code")=="SkillNotLearned","counter cannot bypass existing learned-skill qualification")
	state=battle(); state.units.defender.stamina=24
	check(resolve(state,data).get("code")=="NotEnoughStamina","counter cannot begin without configured preparation cost")
	print("PERSONAL_COUNTER checks=",checks," failures=",failures)
	quit(1 if failures else 0)
