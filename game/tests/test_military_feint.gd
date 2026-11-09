## BA07佯退局部门：免ZOC不免真实地形/占用，110%风险只作用当轮原直接伤害，不自动离场。
extends SceneTree
const Round=preload("res://core/military_round.gd")
const F=preload("res://tests/military_fixture.gd")
const Strict=preload("res://core/strict_json.gd")
var checks: int=0
var failures: int=0

## 等待纯规则工程就绪，不加载GUI或正式世界。
func _init() -> void:
	call_deferred("_run")

## 保留有意义断言及错误退出，禁止引擎诊断被成功数量掩盖。
func check(value: bool, label: String) -> void:
	checks+=1
	if not value: failures+=1; printerr("FAIL: ",label)

## 固定作者20点佯退与真实既有名册；后半面约定按轮初朝向，不产生新世界状态。
func fresh() -> Dictionary:
	var data: Dictionary=F.data(); data.battlefields["field.open"]=F.board("field.open",9,5)
	for troop in data.troop_types.values():
		troop.movement=3; troop.attack=20; troop.defense=0; troop.accuracy=100; troop.tactic_capacity=60
		troop.tactics=[{"id":"tactic.feint","kind":"feint","point_cost":20,"power_bp":10000,"accuracy_bonus":0,"morale_effect":0,"move_bonus":0,"status_rounds":1}]
	data.troop_types["troop.bow"].maximum_range=6
	var battle: Dictionary=F.battle(data)
	battle.units["unit.a"]=F.unit("unit.a",0,[5,2],"troop.foot",100)
	battle.units["unit.b"]=F.unit("unit.b",1,[4,1],"troop.bow",100)
	for unit in battle.units.values(): unit.morale=100; unit.tactic_points=60; unit.heading=0
	F.wait_ai(battle)
	return {"data":data,"battle":battle,"orders":{"unit.a":F.order([2,2],"tactic","","tactic.feint")}}

## 所有试验走原Round，成功失败均核原整态/RNG不变，输出依旧完整验证兵员与AI。
func resolve(f: Dictionary, label: String) -> Dictionary:
	var before: Dictionary=f.duplicate(true); var result: Dictionary=Round.resolve(f.battle,f.data,f.battle.controller_actor_id,f.orders)
	check(result.ok,label+"："+str(result.get("code","")))
	check(f==before,label+" 原整态/RNG不变")
	if not result.ok: return f.battle
	check(Round.validate(result.battle,f.data,true).ok,label+" 输出和保存计划合法")
	for id in f.battle.units: check(result.battle.units[id].member_ids==f.battle.units[id].member_ids,label+" 总名册守恒 "+id)
	return result.battle

## 查询与实际沿同路径免ZOC；方向约定、费用和地形不接受伪造的新朝向或目标攻击。
func movement_contract() -> void:
	var f: Dictionary=fresh(); var before: Dictionary=f.duplicate(true)
	var quote: Dictionary=Round.preview(f.battle,f.data,"unit.a",f.orders["unit.a"])
	var space: Dictionary=Round.movement_options(f.battle,f.data,"unit.a","tactic.feint")
	check(quote.ok and quote.cost==3 and quote.path==[[5,2],[4,2],[3,2],[2,2]] and quote.incoming_damage_bp==11000,"佯退合法穿过敌控制区，明确110%风险报价")
	check(space.ok and space.cells.any(func(entry): return entry.position==[2,2] and entry.path==quote.path),"移动圈与定点预览共路径")
	check(not Round.preview(f.battle,f.data,"unit.a",F.order([2,2],"move")).ok and f==before,"普通移动仍受原ZOC约束，纯查询不改输入")
	var next: Dictionary=resolve(f,"完整免ZOC后撤")
	check(next.units["unit.a"].position==[2,2] and next.units["unit.a"].tactic_points==40 and next.rng==f.battle.rng,"真实后撤与报价一致，只扣定义20，无攻击随机")
	for order in [F.order([6,2],"tactic","","tactic.feint",3),F.order([5,2],"tactic","","tactic.feint"),F.order([2,2],"tactic","unit.b","tactic.feint")]:
		f.orders["unit.a"]=order; before=f.duplicate(true)
		check(not Round.resolve(f.battle,f.data,f.battle.controller_actor_id,f.orders).ok and f==before,"原地/向前借转向/带攻击目标均原子拒绝")
	f=fresh(); f.battle.units["unit.a"].tactic_points=19; before=f.duplicate(true)
	check(Round.movement_options(f.battle,f.data,"unit.a","tactic.feint").code=="NotEnoughMilitaryTacticPoints" and not Round.resolve(f.battle,f.data,f.battle.controller_actor_id,f.orders).ok and f==before,"查询提交同样拒绝点数不足")
	for terrain in ["water","forest"]:
		f=fresh(); f.battle.units["unit.b"].position=[8,4]; F.wait_ai(f.battle)
		for cell in f.data.battlefields["field.open"].cells:
			if cell.q==4: cell.terrain=terrain
		before=f.duplicate(true)
		check(Round.resolve(f.battle,f.data,f.battle.controller_actor_id,f.orders).code=="HexDestinationUnreachable" and f==before,"免ZOC不免禁行/额外代价 "+terrain)
	for field in {"power_bp":11000,"move_bonus":1,"accuracy_bonus":1,"morale_effect":1,"status_rounds":2}:
		f=fresh(); f.data.troop_types["troop.foot"].tactics[0][field]={"power_bp":11000,"move_bonus":1,"accuracy_bonus":1,"morale_effect":1,"status_rounds":2}[field]
		check(Round.validate(f.battle,f.data).code=="UnsupportedFeintParameter","拒额外佯退参数 "+field)
	f=fresh(); f.data.battlefields["field.open"].exits[0].position=[2,2]
	next=resolve(f,"佯退抵达实际出口")
	check(not next.units["unit.a"].withdrawn and next.objective_state.escaped_unit_ids.is_empty(),"后撤战法不代替离场withdraw")

## 本轮每次真实直接伤害乘110%，下一次统一行动消失；协防在原直接伤害修正后只分一次。
func incoming_damage() -> void:
	var f: Dictionary=fresh(); f.battle.units["unit.b"].position=[5,0]
	f.battle.ai_plans["unit.b"]=F.order([5,0],"attack","unit.a")
	var control: Dictionary=f.duplicate(true); control.orders["unit.a"]=F.order([2,2],"move")
	var normal: Dictionary=resolve(control,"无佯退风险同源对照")
	var next: Dictionary=resolve(f,"后撤受直接远射")
	check(normal.units["unit.a"].fit.size()==80 and next.units["unit.a"].fit.size()==78 and next.rng==normal.rng,"原20变22，无额外随机抽取")
	check(next.units["unit.a"].statuses.is_empty(),"不留下跨轮持久风险字段")
	f.battle=next; f.battle.ai_plans["unit.b"]=F.order([5,0],"attack","unit.a"); f.orders["unit.a"]=F.order([2,2],"wait")
	next=resolve(f,"下一轮正常行动")
	check(next.units["unit.a"].fit.size()==58,"下一轮统一行动后受伤回原20，不残留110%")
	f=fresh(); f.battle.units["unit.b"].position=[5,0]; f.battle.units["unit.d"]=F.unit("unit.d",1,[4,0],"troop.bow",100); f.battle.units["unit.d"].morale=100
	f.battle.ai_plans={"unit.b":F.order([5,0],"attack","unit.a"),"unit.d":F.order([4,0],"attack","unit.a")}
	check(resolve(f,"同轮两次直接攻击").units["unit.a"].fit.size()==56,"当轮所有直接受击各22，不只第一击风险")
	f=fresh(); f.battle.units["unit.b"].position=[5,0]; f.orders["unit.a"].destination=[4,2]
	f.battle.units["unit.c"]=F.unit("unit.c",0,[4,3],"troop.foot",100); f.battle.units["unit.c"].tactic_points=60
	f.data.troop_types["troop.foot"].tactics.append({"id":"tactic.support","kind":"support","point_cost":20,"power_bp":10000,"accuracy_bonus":0,"morale_effect":0,"move_bonus":0,"status_rounds":1})
	f.battle.ai_plans={"unit.b":F.order([5,0],"attack","unit.a"),"unit.c":F.order([4,3],"tactic","unit.a","tactic.support")}
	next=resolve(f,"佯退受伤后由邻友协防")
	check(next.units["unit.a"].fit.size()==82 and next.units["unit.c"].fit.size()==96,"22只分18+4，协防转移不再次放大/扣减")
	f=fresh(); f.battle.units["unit.b"].position=[5,0]; f.battle.units["unit.c"]=F.unit("unit.c",0,[4,3]); f.battle.ai_plans={"unit.b":F.order([5,0],"attack","unit.a"),"unit.c":F.order([4,2],"move")}
	next=resolve(f,"后撤首格同步争抢")
	check(next.units["unit.a"].position==[5,2] and next.units["unit.c"].position==[4,3],"争抢仍阻挡双方，无免占用特例")
	check(next.units["unit.a"].fit.size()==78 and next.units["unit.a"].tactic_points==40,"受阻后撤仍付费并承担当轮110%风险")

## AI只在公开退路目标下比较同源合法行动；保存后复读同计划，不偷合并撤离或读玩家草案。
func ai_and_replay() -> void:
	var f: Dictionary=fresh(); f.battle.controller_actor_id="actor.unit.b"; f.battle.controller_side=1; f.battle.command_unit_ids=["unit.b"]; f.battle.objective_id="goal.break"
	f.data.battlefields["field.open"].exits=[{"side":0,"position":[0,2]},{"side":1,"position":[8,4]}]
	var before: Dictionary=f.duplicate(true); var ai: Dictionary=Round.make_ai(f.battle,f.data)
	check(ai.ok and ai.orders["unit.a"].tactic_id=="tactic.feint" and ai.orders["unit.a"].destination==[2,2],"AI自主以免ZOC佯退靠近公开退出目标")
	check(f==before,"AI查询整态/RNG不变")
	var draft: Dictionary=f.battle.duplicate(true); draft.pending_orders={"unit.b":{"action":"fake"}}; draft.ai_plans={}
	check(Round.make_ai(draft,f.data)==ai,"AI不读玩家草案或旧计划")
	f.battle.ai_plans=ai.orders; f.orders={"unit.b":F.order([4,1],"wait")}
	var next: Dictionary=resolve(f,"真实AI佯退")
	check(next.units["unit.a"].position==[2,2] and not next.units["unit.a"].withdrawn and next.ai_plans["unit.a"].action=="withdraw","AI下一轮另选合法withdraw，无提前离场")
	var loaded: Dictionary=Strict.parse_text(JSON.stringify(f.battle))
	check(loaded.ok and Round.validate(loaded.value,f.data,true).ok and Round.resolve(loaded.value,f.data,f.battle.controller_actor_id,f.orders).battle==next,"保存/复读整结果含兵员/RNG相等")
	f.battle.units["unit.a"].tactic_points=19
	check(Round.make_ai(f.battle,f.data).orders["unit.a"].tactic_id!="tactic.feint","AI点数不足不佯退")

## 本独立门只跑新增规则及必要短对照，旧withdraw/其他战法整组证据不重跑不改写。
func _run() -> void:
	movement_contract(); incoming_damage(); ai_and_replay()
	print("MILITARY_FEINT_CHECKS=",checks," FAILURES=",failures)
	quit(0 if failures==0 else 1)
