## 显式正面守势与单槽防御回气；新效果按数据生效，旧全向guard及缺省恢复不变。
extends SceneTree
const Round=preload("res://core/personal_round.gd")
const Personal=preload("res://core/personal_combat.gd")
const Guard=preload("res://core/domains/personal/personal_guard.gd")
const Stamina=preload("res://core/domains/personal/personal_stamina.gd")
const Fixture=preload("res://tests/test_personal_directional_guard.gd")
const Source=preload("res://tests/test_personal_condition_snapshot.gd")
var checks: int=0
var failures: int=0

## 对方向、资格和回气来源分别断言，不用文本卡名充当效果标志。
func check(value: bool, label: String) -> void:
	checks+=1
	if not value: failures+=1; printerr("FAIL: ",label)

## 最终4000为显式作者完整倍率；配置心法使用既有单槽与武艺1资格，不增加被动列表。
static func content() -> Dictionary:
	var data: Dictionary=Fixture.data()
	data.personal_objectives.spar.stamina_model="round_v1"
	data.personal_skills.guard.guard_facing="front"; data.personal_skills.guard.defense_bp=4000
	data.personal_skills["calm"]={"id":"calm","kind":"heart","weapons":["sword"],"stamina_cost":0,"power_bp":10000,"defense_bp":10000,"move_bonus":0,"status":"none","status_rounds":0,"defend_recovery_bonus":5,"required_skill_level":{"skill_id":"martial","level":1}}
	return data

## 气势中点便于观察费用和实际恢复，不让满额上限掩盖重复收益。
static func battle(position: Array=[310,200]) -> Dictionary:
	var state: Dictionary=Fixture.battle(position)
	state.units.defender.stamina=50; state.units.defender.learned={"guard":100,"calm":100}; state.units.defender.heart_id="calm"
	return state

## 防御技能仍是单次主动行动，只支付自己的原费用，不能同时领取普通防御的恢复。
static func guarded(state: Dictionary, data: Dictionary) -> Dictionary:
	return Round.resolve(state,data,Fixture.plan(state.units.defender,"skill","","guard"))

## 覆盖真实方向、未叠减、原mastery、AI生存选择、单槽配置和真实入战资格。
func _initialize() -> void:
	var data: Dictionary=content(); var state: Dictionary=battle()
	var result: Dictionary=guarded(state,data)
	check(result.ok and result.battle.units.defender.hp==92,"front guard applies final4000 once, not basic5000 times4000")
	check(result.ok and result.battle.units.defender.stamina==38,"guard spends15 and receives hurt3 but no defend10 or focus5")
	for position in [[190,200],[250,260],[250,140]]:
		state=battle(position); state.ai_plans.attacker.heading=0
		result=guarded(state,data)
		check(result.ok and result.battle.units.defender.hp==79,"side/back do not receive front guard multiplier "+str(position))
	state=battle(); state.units.defender.learned["guard"]=50; result=guarded(state,data)
	check(result.ok and result.battle.units.defender.hp==86,"guard keeps original mastery interpolation7000 rather than double multiplying")
	state=battle([190,200]); data.personal_skills.guard.erase("guard_facing"); result=guarded(state,data)
	check(result.ok and result.battle.units.defender.hp==92,"legacy missing field retains independent all-direction guard")
	data=content(); state=battle(); result=Round.resolve(state,data,Fixture.plan(state.units.defender,"defend"))
	check(result.ok and result.battle.units.defender.stamina==68,"configured qualified heart adds5 to defend10 and real injury3 at same round end")
	state.ai_plans.attacker=Fixture.plan(state.units.attacker,"wait")
	result=Round.resolve(state,data,Fixture.plan(state.units.defender,"defend"))
	check(result.ok and result.battle.units.defender.stamina==65,"configured focus gives exact5 on ordinary stationary defense")
	state.units.defender.heart_id=""; result=Round.resolve(state,data,Fixture.plan(state.units.defender,"defend"))
	check(result.ok and result.battle.units.defender.stamina==60,"merely learned focus is not equipped passive benefit")
	state.units.defender.heart_id="calm"; state.units.defender.learned["calm"]=0
	result=Round.resolve(state,data,Fixture.plan(state.units.defender,"defend"))
	check(result.ok and result.battle.units.defender.stamina==60,"unmastered configured reference does not grant passive benefit")
	state=battle(); state.ai_plans.attacker=Fixture.plan(state.units.attacker,"wait"); data.personal_skills.calm.weapons=["bow"]
	result=Round.resolve(state,data,Fixture.plan(state.units.defender,"defend"))
	check(result.ok and result.battle.units.defender.stamina==60,"incompatible configured heart cannot grant recovery")
	data=content(); data.personal_skills["calm.second"]=data.personal_skills.calm.duplicate(true); data.personal_skills["calm.second"].id="calm.second"
	state=battle(); state.units.defender.learned["calm.second"]=100; state.ai_plans.attacker=Fixture.plan(state.units.attacker,"wait")
	result=Round.resolve(state,data,Fixture.plan(state.units.defender,"defend"))
	check(result.ok and result.battle.units.defender.stamina==65,"two learned same-category hearts never stack across single configured slot")
	data.personal_objectives.spar.erase("stamina_model")
	result=Round.resolve(state,data,Fixture.plan(state.units.defender,"defend"))
	check(result.ok and result.battle.units.defender.stamina==68,"old model still restores18, not18 plus new focus5")
	data=content(); state=battle(); state.units.defender.hp=9; state.units.defender.heart_id=""; result=guarded(state,data)
	check(result.ok and result.battle.units.defender.hp==1,"front stance really saves a unit that basic guard10 damage could not save")
	state=Fixture.battle([310,200]); state.units.attacker.hp=9; state.units.attacker.learned={"guard":100}
	var ai: Dictionary=Round.make_ai(state,data)
	check(ai.attacker.action=="skill" and ai.attacker.skill_id=="guard" and ai.attacker.heading==180,"AI uses same true front guard when it prevents lethal damage")
	state.units.attacker.hp=20; state.units["other_threat"]=Fixture.unit("other_threat",0,[370,200],180)
	ai=Round.make_ai(state,data)
	check(ai.attacker.action!="skill","AI does not misread front guard as all-direction protection under lethal pincer")
	state=battle(); state.units.defender.stamina=95; result=Round.resolve(state,data,Fixture.plan(state.units.defender,"defend"))
	check(result.ok and result.battle.units.defender.stamina==100,"focus respects total100 cap")
	state=battle(); state.units.defender.stamina=20; state.units["other_threat"]=Fixture.unit("other_threat",1,[300,240],180); state.ai_plans["other_threat"]=Fixture.plan(state.units["other_threat"] ,"attack","defender")
	result=Round.resolve(state,data,Fixture.plan(state.units.defender,"defend"))
	check(result.ok and result.battle.units.defender.stamina==40,"focus15 and two positive injuries6 stay under round20 cap")
	for value in ["back","all",1,true,null]:
		var malformed: Dictionary=data.personal_skills.guard.duplicate(true); malformed.guard_facing=value
		check(not Guard.validate_skill(malformed).ok,"unknown directional guard fails closed "+str(value))
	var malformed: Dictionary=data.personal_skills.guard.duplicate(true); malformed.kind="strike"
	check(not Guard.validate_skill(malformed).ok,"front guard field is not a strike modifier")
	for value in [0,6,5.0,true]:
		malformed=data.personal_skills.calm.duplicate(true); malformed.defend_recovery_bonus=value
		check(not Stamina.validate_skill(malformed).ok,"focus only accepts approved strict integer5 "+str(value))
	malformed=data.personal_skills.calm.duplicate(true); malformed.kind="guard"
	check(not Stamina.validate_skill(malformed).ok,"active guard cannot masquerade as configured heart focus")
	data=content(); data.personal_skills.guard.counter={"damage_bp":4000}; data.personal_skills.guard.required_skill_level={"skill_id":"martial","level":3}
	state=battle(); result=guarded(state,data)
	check(result.ok and result.battle.units.defender.hp==92 and result.battle.units.attacker.hp==92,"front guard and declared reaction share actual geometry without multiplying basic defense")

	data=Source.data(); data.personal_skills["calm"]=content().personal_skills.calm
	for xp in [0,100]:
		var world: Dictionary=Source.state(); world.actors.a.skills.martial=xp; world.actors.a.combat.learned={"calm":100}; world.actors.a.combat.heart_id="calm"
		var quote: Dictionary=Personal.preview(world,data,"a","duel_start",Source.params())
		var entered: Dictionary=Personal.begin(world,data,quote)
		var unit: Dictionary=world.battles.values()[0].units.a
		check(entered.ok and unit.heart_id==("calm" if xp>=100 else "") and unit.learned.has("calm")==bool(xp>=100),"real entry snapshots only earned martial1 heart qualification "+str(xp))
		check(world.actors.a.combat.heart_id=="calm","qualification filtering never deletes original actor configuration")
	print("PERSONAL_GUARD_FOCUS checks=",checks," failures=",failures)
	quit(1 if failures else 0)
