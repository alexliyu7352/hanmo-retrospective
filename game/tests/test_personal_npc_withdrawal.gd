## NPC脱离局部门：真实出口、非死亡状态、合法AI路径、停止交战和关键结果优先级。
extends SceneTree
const Round=preload("res://core/personal_round.gd")
const Withdrawal=preload("res://core/domains/personal/personal_withdrawal.gd")
const Geometry=preload("res://core/personal_geometry.gd")
const Fixture=preload("res://tests/test_personal_directional_guard.gd")
var checks: int=0
var failures: int=0

## 每条失败标明实际规则，不把编译成功当作行为已验证。
func check(value: bool, label: String) -> void:
	checks+=1
	if not value: failures+=1; printerr("FAIL: ",label)

## 两方各有真实出口，场地与武器仍是原连续几何合同。
static func content() -> Dictionary:
	var data: Dictionary=Fixture.data()
	data.personal_objectives.spar.allow_withdrawal=true
	data.personal_battlefields.yard.exits=[{"side":0,"position":[50,200],"radius":28},{"side":1,"position":[550,200],"radius":28}]
	return data

## 低血敌与玩家拉开，第二敌维持战斗以观察离场后的轮次。
static func battle(extra: bool=true) -> Dictionary:
	var state: Dictionary=Fixture.battle([480,200])
	state.units.defender.position=[50,200]
	state.units.attacker.hp=25
	if extra: state.units["remaining"]=Fixture.unit("remaining",1,[300,70],180)
	return state

## 真正生成保存AI；其他敌静止仅隔离其攻击对出口断言的影响。
static func commit(state: Dictionary, data: Dictionary) -> void:
	state.ai_plans=Round.make_ai(state,data)
	if state.ai_plans.has("remaining"): state.ai_plans.remaining=Fixture.plan(state.units.remaining,"wait")

## 默认玩家等待，不改变NPC已经保存的撤离意图。
static func resolve(state: Dictionary, data: Dictionary, action: String="wait") -> Dictionary:
	return Round.resolve(state,data,Fixture.plan(state.units.defender,action))

## 覆盖真实生命周期与不可逃条件，补稳定性和不窥探玩家草案。
func _initialize() -> void:
	var data: Dictionary=content(); var state: Dictionary=battle(); var before: Dictionary=state.duplicate(true)
	var ai: Dictionary=Round.make_ai(state,data)
	check(ai.attacker.action=="withdraw","25 percent NPC selects reachable safe withdrawal")
	check(Round.validate_plan(state,data,"attacker",ai.attacker).ok,"withdrawal intention uses existing legal command")
	check(state==before,"planning is pure")
	var hidden: Dictionary=state.duplicate(true); hidden.player_draft={"action":"attack","destination":[550,200]}; hidden.ai_plans={"invented":{}}
	check(Round.make_ai(hidden,data)==ai,"retreat never reads player draft or stale AI intentions")
	state.units.attacker.hp=26
	check(Round.make_ai(state,data).attacker.action!="withdraw","26 percent does not silently expand approved threshold")
	state=battle(); data.personal_objectives.spar.allow_withdrawal=false
	check(Round.make_ai(state,data).attacker.action!="withdraw","objective withdrawal prohibition remains authoritative")
	data=content(); state=battle(); data.personal_battlefields.yard.exits=[data.personal_battlefields.yard.exits[0]]
	check(Round.make_ai(state,data).attacker.action!="withdraw","opponent exit cannot substitute for missing own exit")
	data=content(); state=battle(); data.personal_battlefields.yard.obstacles=[{"x":510,"y":0,"width":20,"height":400,"blocks_sight":true,"blocks_movement":true}]
	check(Round.make_ai(state,data).attacker.action!="withdraw","sealed exit falls back to existing guard or attack candidate")
	data=content(); state=battle(); state.units.attacker.recovery_until=3
	check(Round.make_ai(state,data).attacker.action!="withdraw","recovering weapon does not permit illegal travel")
	state=battle(); commit(state,data); before=state.duplicate(true)
	var result: Dictionary=resolve(state,data)
	check(result.ok and result.battle.phase=="planning","one NPC may leave while another keeps battle active")
	check(result.ok and result.battle.units.attacker.get("withdrawn_round")==1 and result.battle.units.attacker.hp==25,"departed NPC retains positive HP and actual exit round")
	check(result.ok and not result.battle.ai_plans.has("attacker"),"departed NPC receives no future intentions")
	check(result==resolve(state,data) and state==before,"same saved intentions replay exactly without input mutation")
	if not result.ok: quit(1); return
	state=result.battle
	check(Round.validate_plan(state,data,"attacker",Fixture.plan(state.units.attacker,"wait")).get("code")=="ActorWithdrawn","departed identity cannot submit another action")
	check(Round.preview_attack(state,data,"defender",state.units.defender.position,0,"attacker").get("code")=="InvalidTarget","departed enemy cannot be selected for attacks")
	check(Round.movement_distance(state,data,"attacker")==0,"departed combatant has no movement budget")
	check(Withdrawal.participants(state.units)==["attacker"],"withdrawn identity remains in unit map and exact list")
	check(not Withdrawal.bodies(state.units).has("attacker"),"departed history position is removed from collision inputs")
	state.units.defender.position=[450,200]; commit(state,data)
	var player: Dictionary=Fixture.plan(state.units.defender,"wait"); player.destination=state.units.attacker.position.duplicate()
	result=Round.resolve(state,data,player)
	check(result.ok and result.battle.units.defender.position==state.units.attacker.position,"another actor can actually move through vacated body location")
	state=battle(false); commit(state,data); result=resolve(state,data)
	check(result.ok and result.battle.outcome=="victory" and result.battle.units.attacker.hp==25,"all enemies withdrawn yields tactical victory without fake defeat")
	state=battle(false); commit(state,data); result=resolve(state,data,"withdraw")
	check(result.ok and result.battle.outcome=="withdrawn" and Withdrawal.participants(result.battle.units)==["attacker","defender"],"both sides leave safely: explicit player retreat stays withdrawn")
	state=battle(); state.units.attacker.position=[550,200]; state.units.attacker.hp=100; state.units.defender.position=[490,200]
	commit(state,data); state.ai_plans.attacker=Fixture.plan(state.units.attacker,"withdraw")
	result=Round.resolve(state,data,Fixture.plan(state.units.defender,"attack","attacker"))
	check(result.ok and not result.battle.units.attacker.has("withdrawn_round") and result.battle.units.attacker.hp==79,"actual melee hit blocks NPC exit even after reaching gate")
	data.items.sword.weapon.category="bow"; state.ai_plans.attacker=Fixture.plan(state.units.attacker,"withdraw")
	result=Round.resolve(state,data,Fixture.plan(state.units.defender,"attack","attacker"))
	check(result.ok and result.battle.units.attacker.get("withdrawn_round")==1 and result.battle.units.attacker.hp==79,"surviving ranged hit does not invent melee engagement")
	data=content(); state=battle(); state.units.attacker.position=[550,200]; state.units.attacker.hp=10; state.units.defender.position=[490,200]
	commit(state,data); state.ai_plans.attacker=Fixture.plan(state.units.attacker,"withdraw")
	result=Round.resolve(state,data,Fixture.plan(state.units.defender,"attack","attacker"))
	check(result.ok and result.battle.units.attacker.hp==0 and not result.battle.units.attacker.has("withdrawn_round"),"lethal damage precedes withdrawal and cannot become living escape")
	state=battle(); state.units.attacker.protected=true
	check(Round.make_ai(state,data).attacker.action!="withdraw" and Round.validate_plan(state,data,"attacker",Fixture.plan(state.units.attacker,"withdraw")).get("code")=="ProtectedWithdrawalRequiresController","protected identity may not independently flee")
	state=battle(); state.units.attacker.position=[550,200]; state.units.attacker.withdrawn_round=1; state.round=2
	check(Withdrawal.validate_unit(state,data,"attacker").ok,"persistent withdrawal shape is valid at following round")
	for value in [0,3,1.0,true]:
		state.units.attacker.withdrawn_round=value
		check(not Withdrawal.validate_unit(state,data,"attacker").ok,"invalid withdrawal tick type or range is rejected "+str(value))
	state.units.attacker.withdrawn_round=1; state.units.attacker.hp=0
	check(not Withdrawal.validate_unit(state,data,"attacker").ok,"dead and withdrawn states are mutually exclusive")
	state.units.attacker.hp=25; state.units.attacker.position=[50,200]
	check(not Withdrawal.validate_unit(state,data,"attacker").ok,"wrong side historical exit is rejected")
	data=content(); state=battle()
	data.personal_battlefields.yard.obstacles=[{"x":495,"y":120,"width":20,"height":160,"blocks_sight":true,"blocks_movement":true}]
	state.units.attacker.position=[450,200]
	var steps: int=0; var legal: bool=true
	while state.phase=="planning" and not state.units.attacker.has("withdrawn_round") and steps<15:
		commit(state,data)
		var old: Array=state.units.attacker.position.duplicate()
		result=resolve(state,data)
		if not result.ok: legal=false; break
		state=result.battle; steps+=1
		if not Geometry.can_move(data.personal_battlefields.yard,old,state.units.attacker.position,100,18).ok: legal=false
	check(legal and state.units.attacker.has("withdrawn_round") and steps>1,"multi-round legal detour actually reaches own exit around obstacle")
	data=content(); state=battle(false); state.units.defender.hp=0; commit(state,data)
	result=resolve(state,data)
	check(result.ok and result.battle.outcome=="defeat" and not result.battle.units.attacker.has("withdrawn_round"),"controller casualty takes priority over intended NPC flight")
	data=content(); state=battle(); state.units.attacker.hp=10; state.units.attacker.position=[550,200]; state.units.defender.position=[490,200]
	check(Round.make_ai(state,data).attacker.action!="withdraw","AI does not prefer stationary exit attempt under visible lethal melee threat")

	print("PERSONAL_NPC_WITHDRAWAL checks=",checks," failures=",failures)
	quit(1 if failures else 0)
