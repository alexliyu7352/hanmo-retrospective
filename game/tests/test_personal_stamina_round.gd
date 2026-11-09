## 显式轮末气势：真实命中/正伤恢复、一次费用、单轮上限，以及旧内容保持原行为。
extends SceneTree
const Round=preload("res://core/personal_round.gd")
const Stamina=preload("res://core/domains/personal/personal_stamina.gd")
const Fixture=preload("res://tests/test_personal_directional_guard.gd")
const HitFixture=preload("res://tests/test_personal_hit_round.gd")
const CounterFixture=preload("res://tests/test_personal_counter.gd")
const ExitFixture=preload("res://tests/test_personal_npc_withdrawal.gd")
const Strict=preload("res://core/strict_json.gd")
var checks: int=0
var failures: int=0

## 失败保留明确标签与退出码；本脚本仅覆盖本片，不宣称已发布作者内容。
func check(value: bool, label: String) -> void:
	checks+=1
	if not value: failures+=1; printerr("FAIL: ",label)

## 仅显式目标启用固定round_v1，定义缺省不变。
static func content() -> Dictionary:
	var data: Dictionary=Fixture.data(); data.personal_objectives.spar.stamina_model="round_v1"
	return data

## 中等气势便于读出实际恢复差額，不让100上限掩盖错误。
static func battle() -> Dictionary:
	var state: Dictionary=Fixture.battle([310,200])
	for unit in state.units.values(): unit.stamina=50
	return state

## 逐条覆盖真实轮次，不用预览概率冒充命中事实。
func _initialize() -> void:
	var data: Dictionary=content(); var state: Dictionary=battle()
	var result: Dictionary=Round.resolve(state,data,Fixture.plan(state.units.defender,"defend"))
	check(result.ok and result.battle.units.defender.stamina==63,"defend10 plus actual hurt3 resolves at end")
	check(result.ok and result.battle.units.attacker.stamina==45,"ordinary attack pays10 then real positive hit recovers5")
	state=battle(); state.ai_plans.attacker=Fixture.plan(state.units.attacker,"wait")
	result=Round.resolve(state,data,Fixture.plan(state.units.defender,"wait"))
	check(result.ok and result.battle.units.defender.stamina==65,"true stationary wait recovers15")
	var moving: Dictionary=Fixture.plan(state.units.defender,"wait"); moving.destination=[200,200]
	result=Round.resolve(state,data,moving)
	check(result.ok and result.battle.units.defender.stamina==50,"movement using wait action is not stationary rest")
	state.units.defender.protected=true
	result=Round.resolve(state,data,moving)
	check(result.ok and result.battle.units.defender.stamina==50,"moving protected participant cannot acquire fake idle restoration")
	state=battle(); state.ai_plans.attacker=Fixture.plan(state.units.attacker,"wait"); state.ai_plans.attacker.destination=[410,200]
	result=Round.resolve(state,data,Fixture.plan(state.units.defender,"attack","attacker"))
	check(result.ok and result.battle.units.defender.stamina==50 and result.battle.units.attacker.stamina==50,"geometric failure restores nothing and charges no attack; moved target did not rest")
	state=battle(); state.units.defender.stamina=5; state.units.attacker.stamina=0
	var before: Dictionary=state.duplicate(true)
	result=Round.resolve(state,data,Fixture.plan(state.units.defender,"attack","attacker"))
	check(not result.ok and result.code=="NotEnoughStamina" and state==before,"future hit restoration cannot finance current attack")
	state=battle(); state.units.defender.hp=10
	result=Round.resolve(state,data,Fixture.plan(state.units.defender,"wait"))
	check(result.ok and result.battle.units.defender.hp==0 and result.battle.units.defender.stamina==50,"dead participant gets neither wait nor damage restoration")
	state=battle(); state.units.defender.stamina=95
	result=Round.resolve(state,data,Fixture.plan(state.units.defender,"wait"))
	check(result.ok and result.battle.units.defender.stamina==100,"restoration respects absolute100 cap")
	state=battle(); state.units.defender.stamina=20
	for row in [["left",[190,200]],["up",[250,140]],["down",[250,260]]]:
		state.units[row[0]]=Fixture.unit(row[0],1,row[1],0)
		state.ai_plans[row[0]]=Fixture.plan(state.units[row[0]],"attack","defender")
	result=Round.resolve(state,data,Fixture.plan(state.units.defender,"wait"))
	check(result.ok and result.battle.units.defender.hp==16 and result.battle.units.defender.stamina==40,"four simultaneous positive hits plus wait recover at most20 total")
	data=CounterFixture.content(); data.personal_objectives.spar.stamina_model="round_v1"; state=CounterFixture.battle()
	for unit in state.units.values(): unit.stamina=50
	result=CounterFixture.resolve(state,data)
	check(result.ok and result.battle.units.defender.stamina==28,"guard prepays25 and only its incoming positive damage recovers3, counter gives no attack bonus")
	check(result.ok and result.battle.units.attacker.stamina==48,"ordinary hit5 and received counter damage3 both follow actual events")
	data=content(); data.personal_skills["strike"]=data.personal_skills.guard.duplicate(true); data.personal_skills.strike.id="strike"; data.personal_skills.strike.kind="strike"; data.personal_skills.strike.power_bp=10000
	state=battle(); state.units.defender.learned["strike"]=100; state.ai_plans.attacker=Fixture.plan(state.units.attacker,"wait")
	result=Round.resolve(state,data,Fixture.plan(state.units.defender,"skill","attacker","strike"))
	check(result.ok and result.battle.units.defender.stamina==35 and result.battle.units.attacker.stamina==68,"strike hit gives no ordinary recovery, target still gains actual injury3")
	data=content(); data.personal_objectives.spar.hit_model="skill_v1"
	for hit in [true,false]:
		state=HitFixture.snapshot(battle()); state.ai_plans.attacker=Fixture.plan(state.units.attacker,"wait")
		var stream: Dictionary=HitFixture.chosen_stream(0,7700) if hit else HitFixture.chosen_stream(7700,10000)
		var plan: Dictionary=Fixture.plan(state.units.defender,"attack","attacker")
		result=Round.resolve(state,data,plan,stream)
		check(result.ok and result.battle.units.defender.stamina==(45 if hit else 40),"random result decides hit recovery but both valid shots pay "+str(hit))
		check(result.ok and result.battle.units.attacker.stamina==(68 if hit else 65),"random miss cannot invent injury restoration "+str(hit))
		var saved: Dictionary=Strict.parse_text(JSON.stringify({"battle":state,"stream":stream})).value
		check(result==Round.resolve(saved.battle,data,plan,saved.stream),"saved integer replay preserves restoration and RNG "+str(hit))
	data=ExitFixture.content(); data.personal_objectives.spar.stamina_model="round_v1"; state=ExitFixture.battle()
	for unit in state.units.values(): unit.stamina=50
	ExitFixture.commit(state,data)
	result=ExitFixture.resolve(state,data,"withdraw")
	check(result.ok and result.battle.units.attacker.has("withdrawn_round") and result.battle.units.attacker.stamina==50 and result.battle.units.defender.stamina==50,"newly departed players and NPCs receive no restoration")
	state=ExitFixture.battle(); state.units.attacker.hp=100; state.units.attacker.position=[550,200]; state.units.defender.position=[490,200]
	for unit in state.units.values(): unit.stamina=50
	data.items.sword.weapon.category="bow"; ExitFixture.commit(state,data); state.ai_plans.attacker=Fixture.plan(state.units.attacker,"withdraw")
	result=Round.resolve(state,data,Fixture.plan(state.units.defender,"attack","attacker"))
	check(result.ok and result.battle.units.attacker.has("withdrawn_round") and result.battle.units.attacker.hp==79 and result.battle.units.attacker.stamina==50,"surviving ranged damage before successful departure still cannot restore an already departed unit")
	data=Fixture.data(); state=battle(); result=Round.resolve(state,data,Fixture.plan(state.units.defender,"defend"))
	check(result.ok and result.battle.units.defender.stamina==68 and result.battle.units.attacker.stamina==40,"absent model preserves old18 defense and no hit recovery")
	state=battle(); state.ai_plans.attacker=Fixture.plan(state.units.attacker,"wait")
	result=Round.resolve(state,data,Fixture.plan(state.units.defender,"wait"))
	check(result.ok and result.battle.units.defender.stamina==75,"absent model preserves old25 wait")
	check(Stamina.validate_objective({}).ok and Stamina.validate_objective({"stamina_model":"round_v1"}).ok,"only explicit known model changes rules")
	for unknown in ["round_v2","",null,1,{},[]]:
		data=content(); data.personal_objectives.spar.stamina_model=unknown; state=battle()
		check(Round.resolve(state,data,Fixture.plan(state.units.defender,"wait")).get("code")=="PERSONAL_STAMINA_MODEL","unknown stamina model fails closed "+str(unknown))
	print("PERSONAL_STAMINA_ROUND checks=",checks," failures=",failures)
	quit(1 if failures else 0)
