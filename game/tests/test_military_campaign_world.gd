## 同一Alpha中真实军营、NPC自主集结与相向行军接敌；战场计划经Kernel结算，不改空间或战果。
extends SceneTree
const Kernel=preload("res://core/world_kernel.gd")
const F=preload("res://tests/companion_fixture.gd")
const Plans=preload("res://tests/military_combat_fixture.gd")
const Codec=preload("res://core/save_codec.gd")
var checks: int=0
var failures: int=0
var world=Kernel.new()

## 延后启动以使用正常资源目录和独立存档profile。
func _init() -> void:
	call_deferred("run_checks")

## 实际失败输出附上下文并传递非零退出，不隐藏后续动作未执行。
func check(value: bool, label: String) -> bool:
	checks+=1
	if not value: failures+=1; printerr("FAIL: ",label)
	return value

## 命令始终以最新修订提交，同步失败不修改原世界。
func send(k, kind: String, params: Dictionary={}) -> bool:
	var result: Dictionary=F.send(k,kind,params)
	return check(result.ok,kind+": "+str(result))

## 当面受任后领三部真实官兵出营；敌方按作者计划办同样手续，相向路线自然相遇。
func run_checks() -> void:
	var k=world;var opened: Dictionary=k.new_game("scenario.alpha_life","res://data/generated/alpha_life.hmcatalog.json")
	if not check(opened.ok,"实际Alpha军事开局："+str(opened)): finish();return
	if not send(k,"move_local",{"place_id":"west.camp"}): finish();return
	if not send(k,"office_accept",{"office_id":"office.qingchuan.army","context":F.context(k,"role.qingchuan.0")}): finish();return
	if not check(k.advance_one_tick().ok,"玩家受任与敌军自主军政共享半日"): finish();return
	if not send(k,"campaign_accept",{"template_id":"order.qingchuan.army","context":F.context(k,"role.qingchuan.0")}): finish();return
	var state: Dictionary=k.snapshot();var campaign: String=""
	for entry in state.campaigns.values():
		if entry.actor_id==state.player_actor_id: campaign=entry.id
	if not send(k,"army_form",{"order_id":campaign,"unit_ids":["unit.qingchuan.infantry","unit.qingchuan.archer","unit.qingchuan.cavalry"],"food_quantity":500,"money":300}): finish();return
	if not check(k.advance_one_tick().ok,"真实三部编组与NPC受令同日完工"): finish();return
	state=k.snapshot();var army_id: String=state.campaigns[campaign].army_id
	check(state.actors["actor.qingchuan.bowman"].location=={"kind":"InArmy","army_id":army_id},"副将只跟随统帅位置，不复制可独立移动的场所")
	if not send(k,"move_local",{"place_id":"west.street"}) or not send(k,"campaign_depart",{"campaign_id":campaign}): finish();return
	var battle_id: String=""
	for _half in range(24):
		var advanced: Dictionary=k.advance_one_tick()
		if not check(advanced.ok,"沿实际地图行军："+str(advanced)): finish();return
		state=k.snapshot()
		for id in state.battles:
			if state.battles[id].get("kind")=="military": battle_id=id
		if not battle_id.is_empty(): break
	if not check(not battle_id.is_empty(),"相向军团以连续轨迹接敌进入正式六角战场"): finish();return
	var battle: Dictionary=state.battles[battle_id];var source: Dictionary=state.facts[battle.source_id]
	check(source.army_locations[army_id].kind=="Wilderness" and battle.units.size()==5,"河谷接触来源有实际原野位置与双方五部兵员")
	var held: Dictionary=state.actors[state.player_actor_id].location.duplicate(true)
	check(k.preview("duel_start",{}).get("code")=="BattlePending","军事接战期间不能绕过占用另开个人切磋")
	var failed: Dictionary=k.advance_one_tick()
	check(failed.get("code")=="MilitaryPlanRequired" and k.snapshot()==state,"没有完整玩家计划时不先扣粮或偷走半日")
	var saved: Dictionary=Codec.decode(Codec.encode(state,0,"military-contact",k.content()),k.content())
	check(saved.ok and k.restore(saved.state).ok and k.snapshot()==state,"接敌轮间读档完整保留原地形余程、AI和军团账")
	for _round in range(30):
		state=k.snapshot();battle=state.battles[battle_id]
		if battle.phase!="planning": break
		if not send(k,"military_plan",{"battle_id":battle_id,"orders":Plans.plans(battle,k.content().data)}): finish();return
		var advanced: Dictionary=k.advance_one_tick()
		if not check(advanced.ok,"统一世界半日结算野战："+str(advanced)): finish();return
		check(k.snapshot().actors[state.player_actor_id].location==held,"每轮会战只改变战场局部位置，原宏观行程停驻")
	state=k.snapshot();battle=state.battles[battle_id]
	check(battle.phase=="result" and battle.applied and state.facts.has(battle.id),"真实战术目标产生可确认且已一次落账的结果")
	if battle.phase=="result": send(k,"military_close",{"battle_id":battle_id})
	check(k.snapshot().campaigns[campaign].objective_result.is_empty(),"野战终局不会冒充占领营寨")
	finish()

## 只报告当前已实际执行的集成断言，不代替图形或围城验收。
func finish() -> void:
	print("MILITARY_CAMPAIGN_WORLD_CHECKS=",checks," FAILURES=",failures)
	quit(1 if failures else 0)
