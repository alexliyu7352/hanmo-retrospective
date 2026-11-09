## T43弓弩贴身减效门：普通近战与远射共用原预览/同步结算/AI，不引入世界或表现特例。
extends SceneTree
const Round=preload("res://core/military_round.gd")
const F=preload("res://tests/military_fixture.gd")
const Strict=preload("res://core/strict_json.gd")
var checks: int=0
var failures: int=0

## 复用已导入的有限纯规则工程，不读取正式世界或用户存档。
func _init() -> void:
	call_deferred("_run")

## 失败计入进程退出码，伤害取整与候选50%明确记录在断言中。
func check(value: bool, label: String) -> void:
	checks+=1
	if not value: failures+=1; printerr("FAIL: ",label)

## 同一编制与确定命中参数只隔离距离变化，不改正式作者源或旧夹具。
func fresh(distance: int=1) -> Dictionary:
	var data: Dictionary=F.data()
	for troop in data.troop_types.values(): troop.attack=20; troop.accuracy=100; troop.defense=0
	var battle: Dictionary=F.battle(data)
	battle.units["unit.a"]=F.unit("unit.a",0,[0,1],"troop.bow",100)
	battle.units["unit.b"]=F.unit("unit.b",1,[distance,1],"troop.foot",100)
	for unit in battle.units.values(): unit.morale=100
	F.wait_ai(battle)
	return {"data":data,"battle":battle,"orders":{"unit.a":F.order([0,1],"attack","unit.b")}}

## 所有试验走原Round，成功失败均核完整输入含RNG不变，成功后再核五类名册合法。
func resolve(f: Dictionary, label: String) -> Dictionary:
	var before: Dictionary=f.duplicate(true)
	var result: Dictionary=Round.resolve(f.battle,f.data,f.battle.controller_actor_id,f.orders)
	check(result.ok,label+"："+str(result.get("code","")))
	check(f==before,label+" 不改变原会话/RNG/作者/命令")
	if not result.ok: return f.battle
	check(Round.validate(result.battle,f.data,true).ok,label+" 输出与保存计划通过原完整Round校验")
	for id in f.battle.units: check(result.battle.units[id].member_ids==f.battle.units[id].member_ids,label+" 兵员总名册守恒 "+id)
	return result.battle

## 距1普通攻击采用候选半强度，距2以后继续原射程，近身远程战法始终拒绝。
func distance_and_damage() -> void:
	var close: Dictionary=fresh(1); var ranged: Dictionary=fresh(2)
	var before: Dictionary=close.duplicate(true)
	var short_quote: Dictionary=Round.preview(close.battle,close.data,"unit.a",close.orders["unit.a"])
	var long_quote: Dictionary=Round.preview(ranged.battle,ranged.data,"unit.a",ranged.orders["unit.a"])
	check(short_quote.ok and long_quote.ok and short_quote.casualties_min==10 and short_quote.casualties_max==10 and long_quote.casualties_min==20,"同编制贴身10/远射20，预览准确应用当前50%候选")
	check(close==before,"纯预览不消耗会话或世界随机状态")
	var near_next: Dictionary=resolve(close,"普通贴身近战")
	var far_next: Dictionary=resolve(ranged,"原远射对照")
	check(near_next.units["unit.b"].fit.size()==90 and far_next.units["unit.b"].fit.size()==80,"真实减员等于各自预览，不仅改变描述")
	check(near_next.rng==far_next.rng and near_next.units["unit.a"].tactic_points==8,"近战不多抽随机、不额外收战法点")
	close.orders["unit.a"]=F.order([0,1],"tactic","unit.b","tactic.volley")
	before=close.duplicate(true)
	check(Round.resolve(close.battle,close.data,close.battle.controller_actor_id,close.orders).code=="MilitaryTargetOutOfRange" and close==before,"距1禁止volley且失败整态/RNG不变")
	close.data.troop_types["troop.bow"].minimum_range=1
	check(Round.preview(close.battle,close.data,"unit.a",close.orders["unit.a"]).code=="MilitaryTargetOutOfRange","作者minimum_range为1也不能把远程战法偷改贴身技")
	ranged.data.troop_types["troop.bow"].minimum_range=3
	check(Round.preview(ranged.battle,ranged.data,"unit.a",ranged.orders["unit.a"]).code=="MilitaryTargetOutOfRange","贴身特例不扩大距2远射的原minimum_range")
	ranged.battle.units["unit.b"].position=[3,1]
	check(Round.preview(ranged.battle,ranged.data,"unit.a",ranged.orders["unit.a"]).ok,"作者原minimum_range为3时距3照常远射")
	var infantry: Dictionary=fresh(1); infantry.battle.units["unit.a"].troop_type_id="troop.foot"
	check(Round.preview(infantry.battle,infantry.data,"unit.a",infantry.orders["unit.a"]).casualties_min==20,"非弓兵原普通近战强度不变")
	var tiny: Dictionary=fresh(1); tiny.battle.units["unit.a"]=F.unit("unit.a",0,[0,1],"troop.bow",1); tiny.battle.units["unit.a"].morale=100
	check(Round.preview(tiny.battle,tiny.data,"unit.a",tiny.orders["unit.a"]).casualties_min==1,"候选减效沿既有整数最小潜在减员1，不冒称低人数精确小数平衡")

## 敌人同步靠近或远离后按实际距离重验；近身普通攻击不把原齐射改成命中。
func simultaneous_distance() -> void:
	var f: Dictionary=fresh(2)
	f.battle.ai_plans["unit.b"]=F.order([1,1],"move","","",3)
	var next: Dictionary=resolve(f,"射击目标同步靠近")
	check(next.units["unit.b"].position==[1,1] and next.units["unit.b"].fit.size()==90,"实际进入距1后由原20预览改为10近战，攻击不承诺敌人原地不动")
	f.orders["unit.a"]=F.order([0,1],"tactic","unit.b","tactic.volley")
	next=resolve(f,"齐射目标同步贴身")
	check(next.units["unit.b"].fit.size()==100 and next.last_events.any(func(event): return event.get("code")=="MilitaryTargetOutOfRange"),"同步贴身使齐射失效，不隐式换成近战战法")
	check(next.rng==f.battle.rng,"实际射程失效不重抽伤害")
	f=fresh(1); f.data.troop_types["troop.bow"].minimum_range=3
	f.battle.ai_plans["unit.b"]=F.order([2,1],"move","","",3)
	next=resolve(f,"近战目标脱离到内射程空档")
	check(next.units["unit.b"].position==[2,1] and next.units["unit.b"].fit.size()==100,"目标离开距1却未到远射最小距离，实际攻击安全落空")

## AI使用同一查询自主选择合法贴身攻击，保存/复读不抽第二套结果，也不看玩家草案。
func ai_and_replay() -> void:
	var f: Dictionary=fresh(1); f.data.troop_types["troop.bow"].movement=0
	f.battle.controller_actor_id="actor.unit.b"; f.battle.controller_side=1; f.battle.command_unit_ids=["unit.b"]
	var before: Dictionary=f.duplicate(true)
	var planned: Dictionary=Round.make_ai(f.battle,f.data)
	check(planned.ok and planned.orders["unit.a"].action=="attack" and planned.orders["unit.a"].target_id=="unit.b","无移动空间的弓兵AI自主选择普通贴身攻击")
	check(f==before,"AI查询完整输入与随机状态不变")
	var draft: Dictionary=f.battle.duplicate(true)
	draft.pending_orders={"unit.b":{"action":"arbitrary draft"}}; draft.ai_plans={}
	check(Round.make_ai(draft,f.data)==planned,"AI不读取玩家草案或旧保存意图")
	f.battle.ai_plans=planned.orders; f.orders={"unit.b":F.order([1,1],"wait","","",3)}
	var next: Dictionary=resolve(f,"真实AI贴身攻击结算")
	check(next.units["unit.b"].fit.size()==90,"AI实际攻击使用同一减效强度")
	var loaded: Dictionary=Strict.parse_text(JSON.stringify(f.battle))
	check(loaded.ok and Round.validate(loaded.value,f.data,true).ok,"轮初贴身攻击计划可按既有格式保存读取")
	check(Round.resolve(loaded.value,f.data,f.battle.controller_actor_id,f.orders).battle==next,"保存复读后整结果/RNG/名册完全一致")

## 只跑本新增战场内机制；旧Round相邻门由外部有界调用复用，不复制GUI或生成内容包。
func _run() -> void:
	distance_and_damage(); simultaneous_distance(); ai_and_replay()
	print("MILITARY_ARCHER_CONTACT_CHECKS=",checks," FAILURES=",failures)
	quit(0 if failures==0 else 1)
