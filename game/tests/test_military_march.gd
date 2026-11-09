## BA06急行有界规则门：真实六角预算、同步占用与公开AI共用Round，不构造世界或修改正式作者数据。
extends SceneTree
const Round=preload("res://core/military_round.gd")
const F=preload("res://tests/military_fixture.gd")
const Strict=preload("res://core/strict_json.gd")
var checks: int=0
var failures: int=0

## 等全局类加载后执行独立规则入口，进程退出码反映全部断言。
func _init() -> void:
	call_deferred("_run")

## 只记录可核验断言，不把日志数量当世界或表现验收。
func check(value: bool, label: String) -> void:
	checks+=1
	if not value: failures+=1; printerr("FAIL: ",label)

## 作者夹具固定BA06指挥15和移动+2；能力由兵种已知战法给出，不伪造运输技能习得。
func fresh() -> Dictionary:
	var data: Dictionary=F.data()
	var board: Dictionary=F.board("field.open",9,3)
	board.cells=board.cells.filter(func(cell): return cell.r==1)
	board.exits=[{"side":0,"position":[8,1]},{"side":1,"position":[0,1]}]
	data.battlefields[board.id]=board
	for troop in data.troop_types.values():
		troop.tactic_capacity=30
		troop.tactics=[{"id":"tactic.march","kind":"march","point_cost":15,"power_bp":10000,"accuracy_bonus":0,"morale_effect":0,"move_bonus":2,"status_rounds":1}]
	var battle: Dictionary=F.battle(data)
	battle.units["unit.b"].position=[8,1]
	for unit in battle.units.values(): unit.tactic_points=30
	F.wait_ai(battle)
	return {"data":data,"battle":battle,"orders":{"unit.a":F.order([4,1],"tactic","","tactic.march")}}

## 查询指定落点是否在原移动圈，不自行重算或扩展可达空间。
func at(space: Dictionary, position: Array) -> Dictionary:
	if space.get("ok",false):
		for cell in space.cells:
			if cell.position==position: return cell
	return {}

## 每次真实结算核输入整态/RNG不变和原五类兵员校验，失败也保持原始候选。
func resolve(f: Dictionary, label: String) -> Dictionary:
	var before: Dictionary=f.duplicate(true)
	var result: Dictionary=Round.resolve(f.battle,f.data,f.battle.controller_actor_id,f.orders)
	check(result.ok,label+"："+str(result.get("code","")))
	check(f==before,label+" 输入含RNG不变")
	if not result.ok: return f.battle
	check(Round.validate(result.battle,f.data,true).ok,label+" 输出/保存AI命令合法")
	for id in f.battle.units: check(result.battle.units[id].member_ids==f.battle.units[id].member_ids,label+" 兵员守恒 "+id)
	return result.battle

## 非攻击战法只增加本轮预算，点数消费一次，无随机抽取或隐藏伤害，下一轮恢复普通攻击资格。
func movement_and_exclusion() -> void:
	var f: Dictionary=fresh(); var before: Dictionary=f.duplicate(true)
	var ordinary: Dictionary=Round.movement_options(f.battle,f.data,"unit.a")
	var march: Dictionary=Round.movement_options(f.battle,f.data,"unit.a","tactic.march")
	var quote: Dictionary=Round.preview(f.battle,f.data,"unit.a",f.orders["unit.a"])
	check(at(ordinary,[4,1]).is_empty() and at(march,[4,1]).get("cost")==4,"普通2格与急行4格使用同一地形预算查询")
	check(quote.ok and quote.path==at(march,[4,1]).path and quote.cost==4 and not quote.has("hit_chance"),"定点预览与移动圈路径/代价一致，急行不产生攻击报价")
	check(f==before,"移动圈/定点预览不改整态/RNG")
	var next: Dictionary=resolve(f,"完整4格急行")
	check(next.units["unit.a"].position==[4,1] and next.units["unit.a"].tactic_points==15,"真实移动4格且仅扣作者指挥15")
	check(next.rng==f.battle.rng and next.units["unit.b"].fit==f.battle.units["unit.b"].fit,"无攻击不消耗随机/敌兵")
	check(next.units["unit.a"].statuses=={} and at(Round.movement_options(next,f.data,"unit.a"),[7,1]).is_empty(),"急行不留下永久加程或跨轮状态")
	for order in [F.order([4,1],"tactic","unit.b","tactic.march"),F.order([4,1],"attack","unit.b","tactic.march")]:
		f.orders["unit.a"]=order; before=f.duplicate(true)
		check(not Round.resolve(f.battle,f.data,f.battle.controller_actor_id,f.orders).ok and f==before,"带目标/攻击动作不能借急行加程，失败整态不变")
	f=fresh(); f.battle.units["unit.b"].position=[5,1]; F.wait_ai(f.battle)
	next=resolve(f,"急行接近敌军")
	check(next.units["unit.b"].fit==f.battle.units["unit.b"].fit and next.rng==f.battle.rng,"停在邻敌格也不能自动攻击")
	check(Round.preview(next,f.data,"unit.a",F.order([4,1],"attack","unit.b")).ok,"下一轮可正常近战，无额外禁攻持续期")
	f=fresh(); f.battle.units["unit.a"].tactic_points=14; before=f.duplicate(true)
	check(Round.movement_options(f.battle,f.data,"unit.a","tactic.march").code=="NotEnoughMilitaryTacticPoints","移动圈同样拒绝点数不足")
	check(Round.resolve(f.battle,f.data,f.battle.controller_actor_id,f.orders).code=="NotEnoughMilitaryTacticPoints" and f==before,"提交拒绝点数不足且无部分移动")
	check(Round.movement_options(f.battle,f.data,"unit.a","missing").code=="MilitaryTacticNotKnown","未知战法不获得加程")
	for field in {"move_bonus":3,"power_bp":11000,"accuracy_bonus":1,"morale_effect":1,"status_rounds":2}:
		var bad: Dictionary=fresh(); bad.data.troop_types["troop.foot"].tactics[0][field]={"move_bonus":3,"power_bp":11000,"accuracy_bonus":1,"morale_effect":1,"status_rounds":2}[field]
		check(Round.validate(bad.battle,bad.data).code=="UnsupportedMarchParameter","拒绝未实现急行参数 "+field)
	f=fresh(); f.data.troop_types["troop.foot"].tactics[0].point_cost=7
	check(resolve(f,"作者不同指挥消耗").units["unit.a"].tactic_points==23,"正式消耗读取定义，不硬编码夹具15")
	for category in ["troop.bow","troop.horse"]:
		f=fresh(); f.battle.units["unit.a"].troop_type_id=category
		check(Round.preview(f.battle,f.data,"unit.a",f.orders["unit.a"]).ok,"已授予急行的兵种可用 "+category)

## 增程仍付真实地形成本，压制/ZOC/零通行和固定占用均沿原查询；同步抢格绝不穿透。
func terrain_and_conflicts() -> void:
	var f: Dictionary=fresh()
	for cell in f.data.battlefields["field.open"].cells:
		if cell.q in [1,2]: cell.terrain="forest"
	f.orders["unit.a"].destination=[2,1]
	var quote: Dictionary=Round.preview(f.battle,f.data,"unit.a",f.orders["unit.a"])
	check(quote.ok and quote.cost==4 and not Round.preview(f.battle,f.data,"unit.a",F.order([3,1],"tactic","","tactic.march")).ok,"森林按2点逐格收费，+2不是免费穿越")
	check(resolve(f,"森林急行").units["unit.a"].position==[2,1],"实际森林移动与报价一致")
	f=fresh(); f.battle.units["unit.a"].statuses.suppressed=1
	check(at(Round.movement_options(f.battle,f.data,"unit.a","tactic.march"),[4,1]).is_empty() and not at(Round.movement_options(f.battle,f.data,"unit.a","tactic.march"),[3,1]).is_empty(),"压制真实从急行预算扣1")
	for terrain in ["water","wall"]:
		f=fresh()
		for cell in f.data.battlefields["field.open"].cells:
			if cell.q==1: cell.terrain=terrain
		var before: Dictionary=f.duplicate(true)
		check(Round.resolve(f.battle,f.data,f.battle.controller_actor_id,f.orders).code=="HexDestinationUnreachable" and f==before,"急行不能穿越"+terrain+"且失败不扣点")
	f=fresh(); f.battle.units["unit.b"].position=[3,1]; F.wait_ai(f.battle)
	check(not at(Round.movement_options(f.battle,f.data,"unit.a","tactic.march"),[2,1]).is_empty() and at(Round.movement_options(f.battle,f.data,"unit.a","tactic.march"),[4,1]).is_empty(),"进入敌控制区即停，不穿越敌人或控制区")
	f=fresh(); f.battle.units["unit.c"]=F.unit("unit.c",0,[2,1]); F.wait_ai(f.battle)
	check(Round.preview(f.battle,f.data,"unit.a",f.orders["unit.a"]).code=="HexDestinationUnreachable","急行不能穿越固定友军占用")
	f=fresh(); f.battle.units["unit.c"]=F.unit("unit.c",0,[6,1]); f.battle.units["unit.c"].tactic_points=30
	F.wait_ai(f.battle); f.battle.ai_plans["unit.c"]=F.order([3,1],"tactic","","tactic.march",3); f.orders["unit.a"].destination=[3,1]
	var next: Dictionary=resolve(f,"同步急行抢格")
	check(next.units["unit.a"].position==[2,1] and next.units["unit.c"].position==[4,1],"同微步争抢一格均停，不重叠/交换")
	check(next.units["unit.a"].tactic_points==15 and next.units["unit.c"].tactic_points==15 and next.rng==f.battle.rng,"已执行受阻急行仍各扣一次，沿原战法消费时点")

## 真实AI只依据公开轮初目标选择加程；有更优普通攻击/同效普通移动时不浪费，保存后不重规划。
func ai_and_replay() -> void:
	var f: Dictionary=fresh(); f.battle.controller_actor_id="actor.unit.b"; f.battle.controller_side=1; f.battle.command_unit_ids=["unit.b"]
	var before: Dictionary=f.duplicate(true); var ai: Dictionary=Round.make_ai(f.battle,f.data)
	check(ai.ok and ai.orders["unit.a"].tactic_id=="tactic.march" and ai.orders["unit.a"].destination==[4,1] and ai.orders["unit.a"].target_id=="","AI自主选4格急行接近公开敌人，无攻击目标")
	check(f==before,"AI查询不改输入/RNG")
	var choices: Dictionary=f.data.duplicate(true)
	var cheap: Dictionary=choices.troop_types["troop.foot"].tactics[0].duplicate(true); cheap.id="tactic.a"; cheap.point_cost=7
	var peer: Dictionary=cheap.duplicate(true); peer.id="tactic.z"
	choices.troop_types["troop.foot"].tactics.append_array([peer,cheap])
	var cheapest: Dictionary=Round.make_ai(f.battle,choices)
	check(cheapest.orders["unit.a"].tactic_id=="tactic.a","等效急行先最低消耗，再稳定ID")
	choices.troop_types["troop.foot"].tactics.reverse()
	check(Round.make_ai(f.battle,choices)==cheapest,"作者战法枚举逆序不改变选招")
	var draft: Dictionary=f.battle.duplicate(true); draft.pending_orders={"unit.b":{"action":"fake"}}; draft.ai_plans={}
	check(Round.make_ai(draft,f.data)==ai,"AI不窥玩家草案或旧AI意图")
	f.battle.ai_plans=ai.orders; f.orders={"unit.b":F.order([8,1],"wait")}
	var next: Dictionary=resolve(f,"真实AI急行")
	check(next.units["unit.a"].position==[4,1] and next.units["unit.a"].tactic_points==15,"AI实际移动消费与同源报价一致")
	var loaded: Dictionary=Strict.parse_text(JSON.stringify(f.battle))
	check(loaded.ok and Round.validate(loaded.value,f.data,true).ok and Round.resolve(loaded.value,f.data,f.battle.controller_actor_id,f.orders).battle==next,"保存计划复读整结果/兵员/RNG完全一致")
	f.battle.units["unit.a"].tactic_points=14
	check(Round.make_ai(f.battle,f.data).orders["unit.a"].action!="tactic","AI点数不足走原普通移动")
	f.battle.units["unit.a"].tactic_points=30; f.battle.units["unit.b"].position=[3,1]
	check(Round.make_ai(f.battle,f.data).orders["unit.a"].action=="attack","原合法移动攻击优于无伤害急行")
	f=fresh(); f.battle.controller_actor_id="actor.unit.b"; f.battle.controller_side=1; f.battle.command_unit_ids=["unit.b"]; f.battle.objective_id="goal.hold"
	f.data.military_objectives["goal.hold"].point=[1,1]
	check(Round.make_ai(f.battle,f.data).orders["unit.a"].action=="move","普通已能到目标时不花指挥点")
	f.data.military_objectives["goal.hold"].point=[4,1]
	check(Round.make_ai(f.battle,f.data).orders["unit.a"].tactic_id=="tactic.march","夺点目标超普通预算时可选急行")
	f.battle.objective_id="goal.break"; f.battle.units["unit.a"].position=[2,1]; f.data.battlefields["field.open"].exits[0].position=[6,1]
	ai=Round.make_ai(f.battle,f.data)
	check(ai.orders["unit.a"].action=="tactic" and ai.orders["unit.a"].destination==[6,1],"撤离目标急行只移动到出口，不偷合并withdraw")
	f.battle.ai_plans=ai.orders; f.orders={"unit.b":F.order([8,1],"wait")}; next=resolve(f,"急行抵出口")
	check(not next.units["unit.a"].withdrawn and next.ai_plans["unit.a"].action=="withdraw","下一轮原规则单独提交撤离")

## 只运行新增急行规则分支，相邻轮次门由同源外部调用一次。
func _run() -> void:
	movement_and_exclusion(); terrain_and_conflicts(); ai_and_replay()
	print("MILITARY_MARCH_CHECKS=",checks," FAILURES=",failures)
	quit(0 if failures==0 else 1)
