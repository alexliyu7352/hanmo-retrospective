## 护送安全撤离的局部闭环：先保存保护者意图，再按正常同步移动到真实出口。
extends SceneTree
const Round=preload("res://core/personal_round.gd")
const Geometry=preload("res://core/personal_geometry.gd")
const Fixture=preload("res://tests/test_personal_directional_guard.gd")
var checks: int=0
var failures: int=0

## 输出规则失败并保留非零退出码，不依赖界面或世界派生系统。
func check(value: bool, label: String) -> void:
	checks+=1
	if not value: failures+=1; printerr("FAIL: ",label)

## 护送目标沿现有kind和出口字段配置，保护对象保持真实独立身体。
static func content(blocked: bool=false) -> Dictionary:
	var data: Dictionary=Fixture.data()
	data.personal_objectives.spar={"kind":"escort","max_rounds":20,"allow_withdrawal":true}
	data.personal_battlefields.yard.exits=[{"side":0,"position":[60,200],"radius":45}]
	if blocked: data.personal_battlefields.yard.obstacles=[{"x":150,"y":120,"width":20,"height":160,"blocks_sight":true,"blocks_movement":true}]
	return data

## 主角先到出口，保护者在后方；远敌固定等待以单独检验护送路径。
static func battle() -> Dictionary:
	var state: Dictionary=Fixture.battle([550,200])
	state.units.defender.position=[70,200]
	state.units["protected"]=Fixture.unit("protected",0,[240,200],180)
	state.units["protected"].weapon_id=""
	state.units["protected"].protected=true
	return state

## 只替远敌固定等待，保护者始终消费正式make_ai生成并保存的意图。
static func commit_ai(state: Dictionary, data: Dictionary) -> void:
	state.ai_plans=Round.make_ai(state,data)
	state.ai_plans.attacker=Fixture.plan(state.units.attacker,"wait")

## 从有路或绕障碍场地逐轮撤离，逐步检查真实几何，不直接改保护者位置。
func escape(blocked: bool) -> void:
	var data: Dictionary=content(blocked)
	var state: Dictionary=battle()
	var turns: int=0
	var movement_ok: bool=true
	while state.phase=="planning" and turns<15:
		commit_ai(state,data)
		var old: Array=state.units["protected"].position.duplicate()
		var result: Dictionary=Round.resolve(state,data,Fixture.plan(state.units.defender,"withdraw"))
		if not result.ok: movement_ok=false; break
		var next: Dictionary=result.battle
		if not Geometry.can_move(data.personal_battlefields.yard,old,next.units["protected"].position,90,18).ok: movement_ok=false
		if turns==0: check(next.phase=="planning","principal cannot leave protected person behind")
		state=next; turns+=1
	check(movement_ok,"escort moves remain legal without teleport or wall crossing "+str(blocked))
	check(state.phase=="result" and state.outcome=="withdrawn","both participants can actually reach safe exit "+str(blocked))
	if state.outcome=="withdrawn": check(Vector2(state.units["protected"].position[0]-60,state.units["protected"].position[1]-200).length()<=45,"protected body center actually inside friendly exit")

## 同行条件、坏路线、安全条件和AI不读草案共同构成最小完整行为出口。
func _initialize() -> void:
	escape(false)
	escape(true)
	var data: Dictionary=content(); var state: Dictionary=battle()
	var ai: Dictionary=Round.make_ai(state,data)
	var hidden: Dictionary=state.duplicate(true); hidden.player_draft={"action":"attack","destination":[500,300]}; hidden.ai_plans={"fake":{}}
	check(Round.make_ai(hidden,data)==ai,"protected AI ignores current player draft and old AI dictionary")
	check(ai.has("protected") and Round.validate_plan(state,data,"protected",ai["protected"]).ok,"protected intention is a legal existing command")
	data.personal_battlefields.yard.obstacles=[{"x":150,"y":0,"width":20,"height":400,"blocks_sight":true,"blocks_movement":true}]
	commit_ai(state,data); var result: Dictionary=Round.resolve(state,data,Fixture.plan(state.units.defender,"withdraw"))
	check(result.ok and result.battle.phase=="planning" and result.battle.units["protected"].position==state.units["protected"].position,"unreachable exit waits safely instead of crossing full wall")
	data=content(); state=battle(); state.units["protected"].position=[60,240]; state.units.attacker.position=[120,240]
	commit_ai(state,data); state.ai_plans.attacker=Fixture.plan(state.units.attacker,"attack","protected")
	result=Round.resolve(state,data,Fixture.plan(state.units.defender,"withdraw"))
	check(result.ok and result.battle.phase=="planning","melee hit on protected person blocks group withdrawal")
	state=battle(); state.units["protected"].position=[60,240]
	commit_ai(state,data); result=Round.resolve(state,data,Fixture.plan(state.units.defender,"withdraw"))
	check(result.ok and result.battle.outcome=="withdrawn","both at exit with no melee contact may leave")
	print("PERSONAL_ESCORT_WITHDRAWAL checks=",checks," failures=",failures)
	quit(1 if failures else 0)
