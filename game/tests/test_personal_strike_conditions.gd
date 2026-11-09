## 原地发动与显式外射程：作者合同、预览/提交/真实条件和技能独立AI候选共用同一纯叶。
extends SceneTree
const Round=preload("res://core/personal_round.gd")
const Strike=preload("res://core/domains/personal/personal_strike.gd")
const Fixture=preload("res://tests/test_personal_directional_guard.gd")
const Strict=preload("res://core/strict_json.gd")
var checks: int=0
var failures: int=0

## 每项断言有明确失败出口，专注本次接口，不以新增卡片名冒充规则。
func check(value: bool, label: String) -> void:
	checks+=1
	if not value: failures+=1; printerr("FAIL: ",label)

## 使用当前整数坐标的弓与两种显式招式；100射程是测试作者值，不是旧一格的自动换算。
static func content() -> Dictionary:
	var data: Dictionary=Fixture.data()
	data.items.sword.weapon={"category":"bow","minimum_range":110,"maximum_range":200,"arc_degrees":85,"damage":18,"stamina_cost":10,"recovery_rounds":0,"move_distance":40}
	data.personal_skills={"aim":{"id":"aim","kind":"strike","weapons":["bow"],"stamina_cost":15,"power_bp":10000,"defense_bp":10000,"move_bonus":0,"status":"none","status_rounds":0,"requires_stationary":true,"hit_bonus_bp":1500},"long":{"id":"long","kind":"strike","weapons":["bow"],"stamina_cost":25,"power_bp":9000,"defense_bp":10000,"move_bonus":0,"status":"none","status_rounds":0,"range_modifier":{"maximum_bonus":100}}}
	return data

## 使用真实已学集合，双方轮初远离普通弓的射程但处于远射范围。
static func battle() -> Dictionary:
	var state: Dictionary=Fixture.battle([500,200])
	state.units.defender.learned={"aim":100,"long":100}; state.units.attacker.learned={"aim":100,"long":100}
	state.ai_plans.attacker=Fixture.plan(state.units.attacker,"wait")
	return state

## 检查原地条件、距离边界、障碍、AI不依赖普通攻击以及移动侧背预览一致性。
func _initialize() -> void:
	var data: Dictionary=content(); var state: Dictionary=battle(); var before: Dictionary=data.duplicate(true)
	check(Strike.validate_skill(data.personal_skills.aim).ok and Strike.validate_skill(data.personal_skills.long).ok,"explicit strike contracts accepted")
	check(Strike.weapon(data.items.sword.weapon,data.personal_skills.long).maximum_range==300 and data==before,"range is author-defined addition on a copy")
	check(not Round.preview_attack(state,data,"defender",[250,200],0,"attacker").ok,"ordinary shot remains out of range")
	var preview: Dictionary=Round.preview_attack(state,data,"defender",[250,200],0,"attacker","long")
	check(preview.ok,"range-only skill independently reaches target beyond ordinary weapon")
	var intent: Dictionary=Fixture.plan(state.units.defender,"skill","attacker","long")
	var result: Dictionary=Round.resolve(state,data,intent)
	check(result.ok and result.battle.units.attacker.hp==100-preview.damage and result.battle.units.defender.stamina==75,"actual extended shot agrees with preview and pays single skill cost")
	state.units.attacker.position=[550,200]; state.ai_plans.attacker=Fixture.plan(state.units.attacker,"wait")
	check(Round.preview_attack(state,data,"defender",[250,200],0,"attacker","long").ok,"explicit outer boundary is included")
	state.units.attacker.position=[551,200]
	check(Round.preview_attack(state,data,"defender",[250,200],0,"attacker","long").get("code")=="OutOfRange","range one unit beyond extension fails")
	state.units.attacker.position=[350,200]
	check(Round.preview_attack(state,data,"defender",[250,200],0,"attacker","long").get("code")=="InsideMinimumRange","extension never removes bow near dead zone")
	state=battle()
	check(Round.preview_attack(state,data,"defender",[250,200],180,"attacker","long").get("code")=="WrongFacing","extension keeps weapon arc")
	data.personal_battlefields.yard.obstacles=[{"x":370,"y":150,"width":20,"height":100,"blocks_movement":false,"blocks_sight":true}]
	check(Round.preview_attack(state,data,"defender",[250,200],0,"attacker","long").get("code")=="LineBlocked","extension cannot shoot through sight obstacle")
	data=content(); state=battle(); state.units.defender.learned["long"]=1
	check(Round.preview_attack(state,data,"defender",[250,200],0,"attacker","long").ok,"range modifier is not silently rescaled by mastery")
	state.units.attacker.position=[410,200]; state.ai_plans.attacker=Fixture.plan(state.units.attacker,"wait")
	var stationary: Dictionary=Fixture.plan(state.units.defender,"skill","attacker","aim")
	check(Round.validate_plan(state,data,"defender",stationary).ok and Round.preview_attack(state,data,"defender",[250,200],0,"attacker","aim").ok,"true stationary aimed strike is legal")
	stationary.destination=[260,200]
	check(Round.validate_plan(state,data,"defender",stationary).get("code")=="StrikeRequiresStationary","moving submission cannot request stationary skill")
	check(Round.preview_attack(state,data,"defender",[260,200],0,"attacker","aim").get("code")=="StrikeRequiresStationary","preview uses identical stationary restriction")
	check(Round.movement_distance(state,data,"defender","aim")==0,"UI movement query exposes zero for stationary strike")
	check(not Strike.condition(data.personal_skills.aim,[250,200],[250,200],[251,200]).ok,"post-move condition rejects displacement despite stationary submission")
	check(Strike.condition(data.personal_skills.aim,[250,200],[250,200],[250,200]).ok,"unchanged actual endpoint satisfies second check")
	var frozen: Dictionary=state.duplicate(true)
	result=Round.resolve(state,data,stationary)
	check(not result.ok and result.code=="StrikeRequiresStationary" and state==frozen,"illegal stationary request does not spend or mutate world")
	state=battle(); var ai: Dictionary=Round.make_ai(state,data)
	check(ai.attacker.action=="skill" and ai.attacker.skill_id=="long","AI enumerates skill firing domain even when no ordinary shot can reach")
	check(Round.validate_plan(state,data,"attacker",ai.attacker).ok,"extended AI intention remains an ordinary validated plan")
	state.units.attacker.position=[410,200]; state.units.attacker.learned={"aim":100}; data.personal_skills.aim.power_bp=20000
	ai=Round.make_ai(state,data)
	check(ai.attacker.action=="skill" and ai.attacker.skill_id=="aim" and ai.attacker.destination==state.units.attacker.position,"AI can independently select actual stationary firing point")
	var hidden: Dictionary=state.duplicate(true); hidden.player_draft={"destination":[500,390]}; hidden.ai_plans={}
	check(Round.make_ai(hidden,data)==ai,"new skill candidates never inspect player draft")
	for kind in ["guard","heart"]:
		var malformed: Dictionary=data.personal_skills.aim.duplicate(true); malformed.kind=kind
		check(not Strike.validate_skill(malformed).ok,"stationary modifier is strike-only "+kind)
		malformed=data.personal_skills.long.duplicate(true); malformed.kind=kind
		check(not Strike.validate_skill(malformed).ok,"range modifier is strike-only "+kind)
	for value in [false,1,"true",null]:
		var malformed: Dictionary=data.personal_skills.aim.duplicate(true); malformed.requires_stationary=value
		check(not Strike.validate_skill(malformed).ok,"stationary requires explicit boolean true "+str(value))
	for value in [0,-1,2001,1.0,true]:
		var malformed: Dictionary=data.personal_skills.long.duplicate(true); malformed.range_modifier.maximum_bonus=value
		check(not Strike.validate_skill(malformed).ok,"range budget and strict type enforced "+str(value))
	var malformed: Dictionary=data.personal_skills.long.duplicate(true); malformed.range_modifier.minimum_bonus=1
	check(not Strike.validate_skill(malformed).ok,"unexpected range fields cannot alter inner dead zone")
	data=Fixture.data(); data.personal_objectives.spar.facing_damage={"side_bp":11000,"back_bp":12500}
	state=Fixture.battle([310,200]); state.units.attacker.heading=0; state.ai_plans.attacker=Fixture.plan(state.units.attacker,"wait")
	preview=Round.preview_attack(state,data,"defender",[310,260],270,"attacker")
	intent=Fixture.plan(state.units.defender,"attack","attacker"); intent.destination=[310,260]; intent.heading=270
	result=Round.resolve(state,data,intent)
	check(preview.ok and preview.damage==23 and result.ok and result.battle.units.attacker.hp==77,"moving-side preview uses actual proposed attacker position rather than old rear position")
	var restored: Dictionary=Strict.parse_text(JSON.stringify(state)).value
	check(Round.resolve(restored,data,intent)==result,"integer save replay preserves projected-position fix")
	data=Fixture.data(); state=Fixture.battle([310,200])
	var old_point: Array=Round.make_ai(state,data).attacker.destination.duplicate()
	data.personal_skills["old_strike"]=data.personal_skills.guard.duplicate(true); data.personal_skills.old_strike.id="old_strike"; data.personal_skills.old_strike.kind="strike"; data.personal_skills.old_strike.power_bp=20000
	state.units.attacker.learned={"old_strike":100}
	ai=Round.make_ai(state,data)
	check(ai.attacker.skill_id=="old_strike" and ai.attacker.destination==old_point,"legacy skill without new modifiers keeps original AI movement candidate")

	print("PERSONAL_STRIKE_CONDITIONS checks=",checks," failures=",failures)
	quit(1 if failures else 0)
