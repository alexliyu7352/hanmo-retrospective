## BA03压制门：80%普通远射只降低下一轮移动，不借旧volley加射程或减命中，不叠加永久状态。
extends SceneTree
const Round=preload("res://core/military_round.gd")
const F=preload("res://tests/military_fixture.gd")
const Strict=preload("res://core/strict_json.gd")
var checks: int=0
var failures: int=0

## 在已导入的纯规则工程内执行，不创建正式世界或美术资源。
func _init() -> void:
	call_deferred("_run")

## 断言失败直接影响进程退出码，源码回执另外由有界运行器保存。
func check(value: bool, label: String) -> void:
	checks+=1
	if not value: failures+=1; printerr("FAIL: ",label)

## 对照定义固定80%/指挥20，兵员名册由既有夹具创建；不用假任务或世界事实触发状态。
func fresh() -> Dictionary:
	var data: Dictionary=F.data(); data.battlefields["field.open"]=F.board("field.open",7,5)
	for troop in data.troop_types.values():
		troop.attack=20; troop.accuracy=100; troop.defense=0; troop.tactic_capacity=60; troop.tactics=[]
	data.troop_types["troop.bow"].tactics=[{"id":"tactic.suppress","kind":"suppress","point_cost":20,"power_bp":8000,"accuracy_bonus":0,"morale_effect":0,"move_bonus":0,"status_rounds":1}]
	var battle: Dictionary=F.battle(data)
	battle.units["unit.a"]=F.unit("unit.a",0,[0,2],"troop.bow",100)
	battle.units["unit.b"]=F.unit("unit.b",1,[3,2],"troop.foot",100)
	for unit in battle.units.values(): unit.morale=100; unit.tactic_points=60; unit.heading=3
	F.wait_ai(battle)
	return {"data":data,"battle":battle,"orders":{"unit.a":F.order([0,2],"tactic","unit.b","tactic.suppress")}}

## 正式移动圈中寻找落点，不在测试重算状态减益或六角路径。
func reachable(battle: Dictionary, data: Dictionary, id: String, position: Array) -> bool:
	var space: Dictionary=Round.movement_options(battle,data,id)
	return space.ok and space.cells.any(func(entry): return entry.position==position)

## 实际结算前后核输入整态/RNG和原兵员/AI验证，绝不从损坏候选继续推进。
func resolve(f: Dictionary, label: String) -> Dictionary:
	var before: Dictionary=f.duplicate(true); var result: Dictionary=Round.resolve(f.battle,f.data,f.battle.controller_actor_id,f.orders)
	check(result.ok,label+"："+str(result.get("code","")))
	check(f==before,label+" 原整态/RNG不变")
	if not result.ok: return f.battle
	check(Round.validate(result.battle,f.data,true).ok,label+" 输出/AI计划合法")
	for id in f.battle.units: check(result.battle.units[id].member_ids==f.battle.units[id].member_ids,label+" 兵员守恒 "+id)
	return result.battle

## 压制使用原远射与普通移动预算，不额外加射程；费用和错误原子边界沿当前战法。
func damage_and_legality() -> void:
	var f: Dictionary=fresh(); var before: Dictionary=f.duplicate(true)
	var suppress: Dictionary=Round.preview(f.battle,f.data,"unit.a",f.orders["unit.a"])
	var ordinary: Dictionary=Round.preview(f.battle,f.data,"unit.a",F.order([0,2],"attack","unit.b"))
	check(suppress.ok and suppress.casualties_min==16 and ordinary.casualties_min==20 and f==before,"同源压制16/普通20，报价不改整态")
	var next: Dictionary=resolve(f,"真实压制远射")
	check(next.units["unit.b"].fit.size()==84 and next.units["unit.a"].tactic_points==40 and next.units["unit.b"].statuses=={"hindered":2},"80%伤害/作者20费用/只下轮移动状态真实落地")
	f.orders["unit.a"].destination=[1,2]
	check(resolve(f,"普通预算内移动压制").units["unit.a"].position==[1,2],"原文未限原地，移动远射仍需普通预算合法")
	f=fresh(); f.battle.units["unit.b"].position=[4,2]; F.wait_ai(f.battle); before=f.duplicate(true)
	check(Round.resolve(f.battle,f.data,f.battle.controller_actor_id,f.orders).code=="MilitaryTargetOutOfRange" and f==before,"压制不借volley额外一格射程，失败不收费")
	f.data.troop_types["troop.bow"].tactics.append(F.tactic("tactic.volley","volley")); f.orders["unit.a"].tactic_id="tactic.volley"
	check(Round.preview(f.battle,f.data,"unit.a",f.orders["unit.a"]).ok,"旧volley自身加射程合同保留")
	f=fresh(); f.battle.units["unit.b"].position=[1,2]; f.data.troop_types["troop.bow"].minimum_range=1
	check(Round.preview(f.battle,f.data,"unit.a",f.orders["unit.a"]).code=="MilitaryTargetOutOfRange","作者最小射程1也不能把远程压制用于贴身")
	f=fresh(); f.battle.units["unit.a"].tactic_points=19; before=f.duplicate(true)
	check(Round.resolve(f.battle,f.data,f.battle.controller_actor_id,f.orders).code=="NotEnoughMilitaryTacticPoints" and f==before,"指挥不足原子拒绝")
	for field in {"power_bp":10000,"move_bonus":1,"accuracy_bonus":1,"morale_effect":1,"status_rounds":2}:
		f=fresh(); f.data.troop_types["troop.bow"].tactics[0][field]={"power_bp":10000,"move_bonus":1,"accuracy_bonus":1,"morale_effect":1,"status_rounds":2}[field]
		check(Round.validate(f.battle,f.data).code=="UnsupportedSuppressParameter","拒未实现压制参数 "+field)
	f=fresh(); f.data.troop_types["troop.foot"].tactics=f.data.troop_types["troop.bow"].tactics.duplicate(true)
	check(Round.validate(f.battle,f.data).code=="MilitaryTacticTroopMismatch","非弓弩定义不得提供远程压制")

## 减益不倒改本轮已移动路径，不减下轮命中，且只持续下轮；多次命中不累加移动惩罚。
func duration_and_effects() -> void:
	var f: Dictionary=fresh(); f.battle.ai_plans["unit.b"]=F.order([3,0],"move","","",3)
	var next: Dictionary=resolve(f,"目标同轮已走两步")
	check(next.units["unit.b"].position==[3,0] and next.units["unit.b"].statuses.hindered==2,"先共同移动后命中，只在下一轮应用减速")
	f=fresh(); f.battle.units["unit.c"]=F.unit("unit.c",0,[4,2],"troop.foot",100); F.wait_ai(f.battle)
	next=resolve(f,"生成仅移动减益")
	check(not reachable(next,f.data,"unit.b",[1,2]) and reachable(next,f.data,"unit.b",[2,2]),"原2点移动下轮准确减为1")
	var hit: Dictionary=Round.preview(next,f.data,"unit.b",F.order([3,2],"attack","unit.c"))
	check(hit.ok and hit.hit_chance==100,"hindered没有旧suppressed的额外命中惩罚")
	var loaded: Dictionary=Strict.parse_text(JSON.stringify(next))
	check(loaded.ok and Round.validate(loaded.value,f.data,true).ok and loaded.value==next,"带hindered保存计划可原格式完整重读")
	f.battle=next; F.wait_ai(f.battle); f.orders["unit.a"]=F.order([0,2],"wait")
	var after: Dictionary=resolve(f,"压制下一轮正常消费")
	check(not after.units["unit.b"].statuses.has("hindered") and reachable(after,f.data,"unit.b",[1,2]),"再下一轮状态消失、移动预算恢复2")
	f=fresh(); f.battle.units["unit.d"]=F.unit("unit.d",0,[0,3],"troop.bow",100); f.battle.units["unit.d"].morale=100; f.battle.units["unit.d"].tactic_points=60
	F.wait_ai(f.battle); f.battle.ai_plans["unit.d"]=F.order([0,3],"tactic","unit.b","tactic.suppress")
	next=resolve(f,"同轮两发压制")
	check(next.units["unit.b"].fit.size()==68 and next.units["unit.b"].statuses.hindered==2 and reachable(next,f.data,"unit.b",[2,2]),"两发各16伤害，但只减1移动且不延长到第三轮")
	f.data.troop_types["troop.bow"].tactics.append(F.tactic("tactic.volley","volley")); f.battle.ai_plans["unit.d"].tactic_id="tactic.volley"
	next=resolve(f,"旧齐射与新压制同轮命中")
	check(next.units["unit.b"].statuses.hindered==2 and next.units["unit.b"].statuses.suppressed==3 and reachable(next,f.data,"unit.b",[2,2]),"旧齐射时长保留，两类移动减益取最大1不叠成2")

## 目标同步脱距或实际零减员不会凭规划加状态，已发动费用仍由原合同决定。
func failed_hits() -> void:
	var f: Dictionary=fresh(); f.battle.ai_plans["unit.b"]=F.order([4,2],"move","","",3)
	var next: Dictionary=resolve(f,"目标同步脱距")
	check(next.units["unit.b"].fit.size()==100 and next.units["unit.b"].statuses.is_empty() and next.rng==f.battle.rng and next.units["unit.a"].tactic_points==40,"失距无伤害/状态/抽样但仍收费")
	f=fresh(); f.data.troop_types["troop.bow"].accuracy=0; f.battle.units["unit.a"].morale=50
	next=resolve(f,"零命中概率压制")
	check(next.units["unit.b"].fit.size()==100 and next.units["unit.b"].statuses.is_empty(),"实际未命中不附加减益")

## AI只给可实际减少的移动点原单格20分价值，真实选择/结算并重放，不复制玩家草案。
func ai_and_replay() -> void:
	var f: Dictionary=fresh(); f.battle.units["unit.a"]=F.unit("unit.a",0,[0,2],"troop.bow",1); f.battle.units["unit.a"].morale=100; f.battle.units["unit.a"].tactic_points=60
	f.battle.controller_actor_id="actor.unit.b"; f.battle.controller_side=1; f.battle.command_unit_ids=["unit.b"]
	var before: Dictionary=f.duplicate(true); var ai: Dictionary=Round.make_ai(f.battle,f.data)
	check(ai.ok and ai.orders["unit.a"].tactic_id=="tactic.suppress" and f==before,"AI在整数伤害相同时自主选有实际控制价值的压制，查询不改整态")
	var draft: Dictionary=f.battle.duplicate(true); draft.pending_orders={"unit.b":{"action":"fake"}}; draft.ai_plans={}
	check(Round.make_ai(draft,f.data)==ai,"AI不读未提交草案或旧AI命令")
	f.battle.ai_plans=ai.orders; f.orders={"unit.b":F.order([3,2],"wait","","",3)}
	var next: Dictionary=resolve(f,"真实AI压制")
	check(next.units["unit.b"].fit.size()==99 and next.units["unit.b"].statuses.hindered==2,"AI真实产生命中及仅移动减益")
	var loaded: Dictionary=Strict.parse_text(JSON.stringify(f.battle))
	check(loaded.ok and Round.resolve(loaded.value,f.data,f.battle.controller_actor_id,f.orders).battle==next,"保存计划重放整态/RNG/名册完全相等")
	f.data.troop_types["troop.foot"].movement=0
	check(Round.make_ai(f.battle,f.data).orders["unit.a"].action=="attack","目标本来不能移动时不虚加压制价值")

## 只运行压制相关短分支，不重跑旧volley/野战全组。
func _run() -> void:
	damage_and_legality(); duration_and_effects(); failed_hits(); ai_and_replay()
	print("MILITARY_SUPPRESS_CHECKS=",checks," FAILURES=",failures)
	quit(0 if failures==0 else 1)
