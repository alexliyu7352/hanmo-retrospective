## 显式命中轮次的费用、唯一抽样、预览纯性与严格JSON重放；旧默认不受新模型影响。
extends SceneTree
const Round=preload("res://core/personal_round.gd")
const Hit=preload("res://core/domains/personal/personal_hit.gd")
const Strict=preload("res://core/strict_json.gd")
const Fixture=preload("res://tests/test_personal_directional_guard.gd")
const CounterFixture=preload("res://tests/test_personal_counter.gd")
const HitFixture=preload("res://tests/test_personal_hit_leaf.gd")
var checks: int=0
var failures: int=0

## 断言真实返回和源状态，失败不以零伤害或缺事件伪装成功。
func check(value: bool, label: String) -> void:
	checks+=1
	if not value: failures+=1; printerr("FAIL: ",label)

## 只给显式模型战斗添加已派生的两项等级快照。
static func snapshot(state: Dictionary) -> Dictionary:
	state.units.defender.skill_levels={"martial":3,"archery":1}
	state.units.attacker.skill_levels={"martial":2,"archery":0}
	return state

## 固定有限种子夹具覆盖命中/未命中两边；不对玩家世界重抽，所有候选流只存在本测试。
static func chosen_stream(minimum: int, maximum: int) -> Dictionary:
	for seed_value in range(1,1000):
		var stream: Dictionary=HitFixture.stream(seed_value)
		var sample: Dictionary=Hit.draw(stream,7500)
		if sample.roll>=minimum and sample.roll<maximum: return stream
	return {}

## 仅取真实原攻击事件，不把移动和反击误算为随机攻击。
static func attacks(battle: Dictionary) -> Array:
	var result: Array=[]
	for event in battle.last_events:
		if event.action in ["attack","skill"] and event.has("damage"): result.append(event)
	return result

## 按真实出手时点验证新旧模型、顺序、反击、失败、费用与AI查询。
func _initialize() -> void:
	var data: Dictionary=Fixture.data(); data.personal_objectives.spar.hit_model="skill_v1"
	for successful in [true,false]:
		var state: Dictionary=snapshot(Fixture.battle([310,200])); state.ai_plans.attacker=Fixture.plan(state.units.attacker,"wait")
		var stream: Dictionary=chosen_stream(0,7700) if successful else chosen_stream(7700,10000)
		var before: Dictionary=state.duplicate(true); var stream_before: Dictionary=stream.duplicate(true)
		var preview: Dictionary=Round.preview_attack(state,data,"defender",[250,200],0,"attacker")
		Round.make_ai(state,data)
		check(preview.ok and preview.hit_bp==7700 and state==before and stream==stream_before,"preview and AI expose chance without drawing")
		var expected: Dictionary=Hit.draw(stream,7700)
		var result: Dictionary=Round.resolve(state,data,Fixture.plan(state.units.defender,"attack","attacker"),stream)
		check(result.ok and result.duel_stream==expected.duel_stream,"one valid attack advances exactly one draw")
		check(result.battle.units.attacker.hp==(79 if successful else 100),"random hit result alone determines damage")
		check(result.battle.units.defender.stamina==90,"valid ordinary attack pays even on random miss")
		check(attacks(result.battle)[0].code==("Hit" if successful else "Miss") and attacks(result.battle)[0].roll==expected.roll,"attack event preserves exact committed random result")
		check(state==before and stream==stream_before,"resolution preserves battle and random input")
	var state: Dictionary=snapshot(Fixture.battle([310,200])); var stream: Dictionary=HitFixture.stream()
	state.ai_plans.attacker=Fixture.plan(state.units.attacker,"wait"); state.ai_plans.attacker.destination=[410,200]
	var result: Dictionary=Round.resolve(state,data,Fixture.plan(state.units.defender,"attack","attacker"),stream)
	check(result.ok and result.duel_stream==stream and result.battle.units.defender.stamina==100,"geometric failure neither draws nor charges")
	check(not attacks(result.battle)[0].has("roll"),"unlaunched geometry failure does not invent a roll")
	check(Round.resolve(state,data,Fixture.plan(state.units.defender,"attack","attacker")).get("code")=="InvalidDuelRandomState","opted model never invents missing random state")
	state=snapshot(Fixture.battle([310,200])); state.ai_plans.attacker.heading=360
	var failed: Dictionary=Round.resolve(state,data,Fixture.plan(state.units.defender,"attack","attacker"),stream)
	check(not failed.ok and not failed.has("duel_stream") and stream==HitFixture.stream(),"failed plan returns no partially advanced stream")
	state=snapshot(Fixture.battle([310,200]))
	var first: Dictionary=Hit.draw(stream,7300); var second: Dictionary=Hit.draw(first.duel_stream,7700)
	var plan: Dictionary=Fixture.plan(state.units.defender,"attack","attacker")
	result=Round.resolve(state,data,plan,stream)
	var events: Array=attacks(result.battle)
	check(events.size()==2 and events[0].actor_id=="attacker" and events[0].roll==first.roll and events[1].actor_id=="defender" and events[1].roll==second.roll,"simultaneous attacks draw in stable actor order")
	check(result.duel_stream==second.duel_stream,"two valid attacks consume exactly two draws")
	var saved: Dictionary=Strict.parse_text(JSON.stringify({"battle":state,"duel_stream":stream})).value
	check(Round.resolve(saved.battle,data,plan,saved.duel_stream)==result,"strict JSON replay preserves complete battle and random result")
	state.units={"attacker":state.units.attacker,"defender":state.units.defender}
	check(Round.resolve(state,data,plan,stream)==result,"unit dictionary order cannot alter RNG assignment")
	data=CounterFixture.content(); data.personal_objectives.spar.hit_model="skill_v1"
	state=snapshot(CounterFixture.battle()); stream=chosen_stream(0,7300)
	result=Round.resolve(state,data,Fixture.plan(state.units.defender,"skill","","guard"),stream)
	check(result.ok and result.battle.units.attacker.hp==92 and result.duel_stream==Hit.draw(stream,7300).duel_stream,"successful counter adds no second random draw")
	stream=chosen_stream(7300,10000)
	result=Round.resolve(state,data,Fixture.plan(state.units.defender,"skill","","guard"),stream)
	check(result.ok and result.battle.units.attacker.hp==100 and result.battle.units.defender.stamina==75,"miss cannot trigger counter and guard preparation still pays")
	data=Fixture.data(); state=Fixture.battle([310,200]); state.ai_plans.attacker=Fixture.plan(state.units.attacker,"wait")
	result=Round.resolve(state,data,Fixture.plan(state.units.defender,"attack","attacker"),stream)
	check(result.ok and result.battle.units.attacker.hp==79 and not result.has("duel_stream") and not attacks(result.battle)[0].has("roll"),"legacy target remains deterministic and receipt-compatible")
	data.personal_objectives.spar.hit_model="skill_v1"
	check(Round.preview_attack(state,data,"defender",[250,200],0,"attacker").get("code")=="InvalidPersonalHitSnapshot","new model rejects missing level snapshot instead of assuming zero")
	data.personal_skills["accurate"]={"id":"accurate","kind":"strike","weapons":["sword"],"stamina_cost":15,"power_bp":10000,"defense_bp":10000,"move_bonus":0,"status":"none","status_rounds":0,"hit_bonus_bp":1500}
	state=snapshot(Fixture.battle([310,200])); state.units.defender.learned["accurate"]=100; state.ai_plans.attacker=Fixture.plan(state.units.attacker,"wait"); stream=chosen_stream(7700,9200)
	result=Round.resolve(state,data,Fixture.plan(state.units.defender,"skill","attacker","accurate"),stream)
	check(result.ok and result.battle.units.attacker.hp==79 and result.battle.units.defender.stamina==85,"accuracy skill changes actual threshold and pays its original skill cost")
	stream=chosen_stream(9200,10000)
	result=Round.resolve(state,data,Fixture.plan(state.units.defender,"skill","attacker","accurate"),stream)
	check(result.ok and result.battle.units.attacker.hp==100 and result.battle.units.defender.stamina==85,"valid skill random miss still pays cost")
	state.units.attacker.learned["accurate"]=100
	check(Round.make_ai(state,data).attacker.skill_id=="accurate","AI compares public expected damage without sampling future rolls")
	print("PERSONAL_HIT_ROUND checks=",checks," failures=",failures)
	quit(1 if failures else 0)
