## §5.5生存意图局部门：可见致命威胁下比较真实可用防御，不因低血固定放弃所有进攻。
extends SceneTree
const Round=preload("res://core/personal_round.gd")
const Fixture=preload("res://tests/test_personal_directional_guard.gd")
var checks: int=0
var failures: int=0

## 聚合局部规则断言，失败明示标签并非零退出。
func check(value: bool, label: String) -> void:
	checks+=1
	if not value: failures+=1; printerr("FAIL: ",label)

## 以正常公开人物和已学守势建立濒死敌方，不插入AI专用状态或玩家计划。
static func battle(hp: int=15) -> Dictionary:
	var state: Dictionary=Fixture.battle([310,200])
	state.units.attacker.hp=hp
	state.units.attacker.learned["guard"]=100
	return state

## 验证一击致命、不可防、无现实威胁、掌握/气势及稳定排序等不同候选出口。
func _initialize() -> void:
	var data: Dictionary=Fixture.data()
	var state: Dictionary=battle()
	var before: Dictionary=state.duplicate(true)
	var intents: Dictionary=Round.make_ai(state,data)
	check(intents.attacker.action=="skill" and intents.attacker.skill_id=="guard","learned strong guard chosen when it prevents visible lethal hit")
	check(Round.validate_plan(state,data,"attacker",intents.attacker).ok,"chosen defensive skill is legal")
	check(state==before,"survival planning does not mutate input")
	var resolved_state: Dictionary=state.duplicate(true); resolved_state.ai_plans=intents.duplicate(true)
	var resolved: Dictionary=Round.resolve(resolved_state,data,Fixture.plan(resolved_state.units.defender,"attack","attacker"))
	check(resolved.ok and resolved.battle.units.attacker.hp==11 and resolved.battle.units.attacker.stamina==85,"chosen guard really survives the legal hit and pays original skill cost")
	var hidden: Dictionary=state.duplicate(true); hidden.player_draft={"action":"wait","destination":[100,100]}; hidden.ai_plans={"defender":{"action":"withdraw"}}
	check(Round.make_ai(hidden,data)==intents,"survival AI cannot inspect uncommitted draft or saved intention dictionary")
	state=battle(); state.units.attacker.stamina=10
	intents=Round.make_ai(state,data)
	check(intents.attacker.action=="defend","insufficient guard stamina selects legal basic defense")
	check(Round.validate_plan(state,data,"attacker",intents.attacker).ok,"basic defense remains legal")
	state=battle(); state.units.attacker.learned["guard"]=1
	intents=Round.make_ai(state,data)
	check(intents.attacker.action=="defend","barely learned guard cannot beat actual basic reduction")
	state=battle(1)
	intents=Round.make_ai(state,data)
	check(intents.attacker.action=="attack","when no defense avoids lethal damage preserve original lawful attack candidate")
	state=battle(100)
	intents=Round.make_ai(state,data)
	check(intents.attacker.action=="attack","healthy attacker is not turned into fixed defensive AI")
	state=battle(); state.units.defender.stamina=0
	intents=Round.make_ai(state,data)
	check(intents.attacker.action=="attack","an opponent unable to pay attack cost is not an invented lethal threat")
	state=battle(); state.units.defender.recovery_until=3
	intents=Round.make_ai(state,data)
	check(intents.attacker.action=="attack","weapon recovery removes impossible incoming attack")
	state=battle(); data.personal_skills.guard.weapons=["bow"]
	intents=Round.make_ai(state,data)
	check(intents.attacker.action=="defend","incompatible known guard is never selected")
	data=Fixture.data(); state=battle()
	data.personal_skills["guard.a"]=data.personal_skills.guard.duplicate(true); data.personal_skills["guard.a"].id="guard.a"
	data.personal_skills["guard.z"]=data.personal_skills.guard.duplicate(true); data.personal_skills["guard.z"].id="guard.z"
	state.units.attacker.learned={"guard.z":100,"guard.a":100}
	intents=Round.make_ai(state,data)
	check(intents.attacker.action=="skill" and intents.attacker.skill_id=="guard.a","equal defensive alternatives use stable skill identity")
	var reversed: Dictionary=state.duplicate(true); reversed.units.attacker.learned={"guard.a":100,"guard.z":100}; reversed.units={"attacker":reversed.units.attacker,"defender":reversed.units.defender}
	check(Round.make_ai(reversed,data)==intents,"dictionary insertion order cannot change survival intention")
	data=Fixture.data(); state=battle(25)
	state.units["second_threat"]=Fixture.unit("second_threat",0,[370,200],180)
	intents=Round.make_ai(state,data)
	check(intents.attacker.action=="skill" and intents.attacker.skill_id=="guard","independent guard can prevent simultaneous front and rear lethal threat")
	state.units.attacker.learned={}
	intents=Round.make_ai(state,data)
	check(intents.attacker.action=="attack","partial reduction that remains lethal does not force pointless basic defense")
	print("PERSONAL_AI_SURVIVAL checks=",checks," failures=",failures)
	quit(1 if failures else 0)
