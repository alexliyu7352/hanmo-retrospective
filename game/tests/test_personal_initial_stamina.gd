## 入战作者气势初值只应用一次；恢复读取已有轮次气势，不造人物持续气势账。
extends SceneTree
const Personal=preload("res://core/personal_combat.gd")
const Stamina=preload("res://core/domains/personal/personal_stamina.gd")
const Strict=preload("res://core/strict_json.gd")
const Source=preload("res://tests/test_personal_condition_snapshot.gd")
var checks: int=0
var failures: int=0

## 固定源状态和边界数值，逐项记录生效时点。
func check(value: bool, label: String) -> void:
	checks+=1
	if not value: failures+=1; printerr("FAIL: ",label)

## 真实预览/入战和一次等待后保存恢复，不能从当前初始定义重算战中气势。
func _initialize() -> void:
	for initial in [-1,0,50,100]:
		var data: Dictionary=Source.data(); var state: Dictionary=Source.state(); state.facts={}; state.tasks={}
		if initial>=0: data.personal_objectives.spar.initial_stamina=initial
		var actors: Dictionary=state.actors.duplicate(true)
		var quote: Dictionary=Personal.preview(state,data,"a","duel_start",Source.params())
		check(quote.ok and Personal.begin(state,data,quote).ok,"explicit or absent author initial enters through original source "+str(initial))
		var battle: Dictionary=state.battles.values()[0]; var expected: int=50 if initial<0 else initial
		check(battle.units.a.stamina==expected and battle.units.b.stamina==expected,"all participants receive the same author start "+str(initial))
		check(state.actors==actors,"initial stamina never adds or modifies a persistent actor resource")
	var data: Dictionary=Source.data(); data.personal_objectives.spar.initial_stamina=0
	var state: Dictionary=Source.state(); state.facts={}; state.tasks={}
	var quote: Dictionary=Personal.preview(state,data,"a","duel_start",Source.params()); Personal.begin(state,data,quote)
	var battle: Dictionary=state.battles.values()[0]
	var params: Dictionary={"battle_id":battle.id,"action":"wait","destination":battle.units.a.position.duplicate(),"heading":0}
	quote=Personal.preview(state,data,"a","battle_plan",params)
	check(quote.ok and Personal.apply(state,data,"a","battle_plan",quote).ok and state.battles[battle.id].units.a.stamina==25,"ordinary real round changes stored zero initial using legacy recovery")
	var restored: Dictionary=Strict.parse_text(JSON.stringify(state)).value
	check(Personal.validate_state(restored,data).ok and restored.battles[battle.id].units.a.stamina==25,"restore validates current stored stamina rather than resetting author zero")
	for invalid in [-1,101,50.0,true,null]:
		data=Source.data(); data.personal_objectives.spar.initial_stamina=invalid; state=Source.state()
		var untouched: Dictionary=state.duplicate(true)
		check(Personal.preview(state,data,"a","duel_start",Source.params()).get("code")=="PERSONAL_INITIAL_STAMINA" and state==untouched,"invalid initial type/range refused before side effects "+str(invalid))
	data=Source.data(); state=Source.state(); quote=Personal.preview(state,data,"a","duel_start",Source.params()); var untouched: Dictionary=state.duplicate(true)
	data.personal_objectives.spar.initial_stamina=101
	check(Personal.begin(state,data,quote).get("code")=="PERSONAL_INITIAL_STAMINA" and state==untouched,"begin rechecks stale quote before charging or allocating battle identity")
	print("PERSONAL_INITIAL_STAMINA checks=",checks," failures=",failures)
	quit(1 if failures else 0)
