## 命中模型在个人preview/begin/apply/规划验证的局部接口；真实XP快照与duel提交不混为第二技能或随机真值。
extends SceneTree
const Personal=preload("res://core/personal_combat.gd")
const Hit=preload("res://core/domains/personal/personal_hit.gd")
const Strict=preload("res://core/strict_json.gd")
const Fixture=preload("res://tests/test_personal_condition_snapshot.gd")
const HitFixture=preload("res://tests/test_personal_hit_leaf.gd")
var checks: int=0
var failures: int=0

## 保留每个入口的真实结果，错误退出非零。
func check(value: bool, label: String) -> void:
	checks+=1
	if not value: failures+=1; printerr("FAIL: ",label)

## 只在新目标启用模型，已有费用与人物装备保持原合同。
static func content() -> Dictionary:
	var data: Dictionary=Fixture.data(); data.personal_objectives.spar.hit_model="skill_v1"
	data.roles["role.spar"].sparring.fee=12
	return data

## 现duel/world两流用于验证只有指定随机真值会改变；等级来自两名真实人物XP。
static func source() -> Dictionary:
	var state: Dictionary=Fixture.state(); state.facts={}; state.tasks={}
	state.rng={"duel":HitFixture.stream(123),"world":HitFixture.stream(456)}
	state.actors.a.skills={"martial":650,"archery":100}; state.actors.b.skills={"martial":300,"archery":0}
	return state

## 核验入战前零抽样、一次快照、坏源原子拒绝、实际命中提交与历史不追改。
func _initialize() -> void:
	var data: Dictionary=content(); var state: Dictionary=source(); var before: Dictionary=state.duplicate(true)
	var quote: Dictionary=Personal.preview(state,data,"a","duel_start",Fixture.params())
	check(quote.ok and state==before,"entry preview validates without sampling or charging")
	check(Personal.begin(state,data,quote).ok and state.rng==before.rng,"begin captures XP levels but never samples duel")
	var battle: Dictionary=state.battles.values()[0]
	check(battle.units.a.skill_levels=={"martial":3,"archery":1} and battle.units.b.skill_levels=={"martial":2,"archery":0},"both combatants snapshot actual ordinary skill levels")
	check(state.wallets["wallet.a"].balance==88 and state.actors.a.skills==before.actors.a.skills,"existing fee paid once without editing XP")
	check(Personal.validate_state(state,data).ok,"new planning snapshot validates against real XP")
	var forged: Dictionary=state.duplicate(true); forged.battles[battle.id].units.a.skill_levels.martial=4
	check(Personal.validate_state(forged,data).get("code")=="PersonalHitSourceMismatch","planning rejects invented level snapshot")
	forged=state.duplicate(true); forged.battles[battle.id].units.a.erase("skill_levels")
	check(Personal.validate_state(forged,data).get("code")=="InvalidPersonalHitSnapshot","new model cannot restore by filling missing levels with zeros")
	var rng_before: Dictionary=state.rng.duplicate(true)
	var expected: Dictionary=Hit.draw(state.rng.duel,7300)
	var params: Dictionary={"battle_id":battle.id,"destination":battle.units.a.position.duplicate(),"heading":0,"action":"wait"}
	quote=Personal.preview(state,data,"a","battle_plan",params)
	check(quote.ok and Personal.apply(state,data,"a","battle_plan",quote).ok,"real personal apply consumes a legal committed attack")
	check(state.rng.duel==expected.duel_stream and state.rng.world==rng_before.world,"apply updates only unique duel stream to exact one-draw successor")
	check(Personal.validate_state(state,data).ok,"post-round planning snapshot and saved AI are coherent")
	var restored: Dictionary=Strict.parse_text(JSON.stringify(state)).value
	check(Personal.validate_state(restored,data).ok and restored==state,"strict JSON roundtrip preserves entry and RNG interface")
	state=source(); quote=Personal.preview(state,data,"a","duel_start",Fixture.params()); state.actors.b.skills.erase("archery"); before=state.duplicate(true)
	check(Personal.begin(state,data,quote).get("code")=="InvalidHitSkillExperience" and state==before,"stale quote cannot charge or allocate after XP source becomes invalid")
	state=source(); state.rng.duel.state="01"; before=state.duplicate(true)
	check(Personal.preview(state,data,"a","duel_start",Fixture.params()).get("code")=="InvalidDuelRandomState" and state==before,"bad stream rejected instead of reseeded")
	state=source(); data.personal_objectives.spar.max_rounds=1
	quote=Personal.preview(state,data,"a","duel_start",Fixture.params()); check(Personal.begin(state,data,quote).ok,"terminal fixture enters normally")
	battle=state.battles.values()[0]; params={"battle_id":battle.id,"destination":battle.units.a.position.duplicate(),"heading":0,"action":"wait"}
	quote=Personal.preview(state,data,"a","battle_plan",params)
	check(quote.ok and Personal.apply(state,data,"a","battle_plan",quote).ok and state.battles[battle.id].phase=="result","normal final round writes actual fact and stream once")
	state.actors.a.skills.martial=0; state.actors.b.skills.archery=1200
	check(Personal.validate_state(state,data).ok,"result history does not recapture later XP")
	rng_before=state.rng.duplicate(true); quote=Personal.preview(state,data,"a","battle_close",{"battle_id":battle.id})
	check(quote.ok and Personal.apply(state,data,"a","battle_close",quote).ok and state.rng==rng_before,"closing result cannot draw again")
	check(Personal.validate_state(state,data).ok,"closed history keeps original skill snapshot")
	print("PERSONAL_HIT_ENTRY checks=",checks," failures=",failures)
	quit(1 if failures else 0)
