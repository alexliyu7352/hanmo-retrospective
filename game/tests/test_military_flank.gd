## BA04精确侧击门：骑兵真实移动两格且侧后命中，保留旧charge合同，不把规划路线当实际移动。
extends SceneTree
const Round=preload("res://core/military_round.gd")
const F=preload("res://tests/military_fixture.gd")
const Strict=preload("res://core/strict_json.gd")
var checks: int=0
var failures: int=0

## 原工程全局类就绪后执行有界纯战场测试。
func _init() -> void:
	call_deferred("_run")

## 断言失败让进程失败，保留真实规则错误码供定位。
func check(value: bool, label: String) -> void:
	checks+=1
	if not value: failures+=1; printerr("FAIL: ",label)

## 作者夹具使用明确BA04参数，真实名册与RNG由既有有限夹具创建。
func fresh() -> Dictionary:
	var data: Dictionary=F.data(); data.battlefields["field.open"]=F.board("field.open",7,5)
	for troop in data.troop_types.values():
		troop.attack=20; troop.defense=0; troop.accuracy=100; troop.tactic_capacity=40; troop.tactics=[]
	data.troop_types["troop.horse"].tactics=[{"id":"tactic.flank","kind":"flank","point_cost":20,"power_bp":12500,"accuracy_bonus":0,"morale_effect":0,"move_bonus":0,"status_rounds":1}]
	var battle: Dictionary=F.battle(data)
	battle.units["unit.a"]=F.unit("unit.a",0,[0,2],"troop.horse",100)
	battle.units["unit.b"]=F.unit("unit.b",1,[3,2],"troop.foot",100)
	for unit in battle.units.values(): unit.morale=100; unit.tactic_points=40; unit.heading=0
	F.wait_ai(battle)
	return {"data":data,"battle":battle,"orders":{"unit.a":F.order([2,2],"tactic","unit.b","tactic.flank")}}

## 正式预览是唯一资格入口，不在测试另算侧后或移动步数。
func quote(f: Dictionary) -> Dictionary:
	return Round.preview(f.battle,f.data,"unit.a",f.orders["unit.a"])

## 成功失败均比较完整输入含随机状态；成功用原Round保存校验和五类兵员总名册核守恒。
func resolve(f: Dictionary, label: String) -> Dictionary:
	var before: Dictionary=f.duplicate(true)
	var result: Dictionary=Round.resolve(f.battle,f.data,f.battle.controller_actor_id,f.orders)
	check(result.ok,label+"："+str(result.get("code","")))
	check(f==before,label+" 输入整态/RNG不变")
	if not result.ok: return f.battle
	check(Round.validate(result.battle,f.data,true).ok,label+" 输出/保存AI计划合法")
	for id in f.battle.units: check(result.battle.units[id].member_ids==f.battle.units[id].member_ids,label+" 总兵员守恒 "+id)
	return result.battle

## 纯侧后资格、125%强度和作者消耗；地形花费不能伪造两格移动，不附加旧charge暴露状态。
func qualification_and_damage() -> void:
	var f: Dictionary=fresh(); var before: Dictionary=f.duplicate(true)
	var preview: Dictionary=quote(f)
	var normal: Dictionary=Round.preview(f.battle,f.data,"unit.a",F.order([2,2],"attack","unit.b"))
	check(preview.ok and preview.path.size()==3 and preview.casualties_min==25 and normal.casualties_min==20,"两格骑兵背击25对普通20，125%真实参与报价")
	check(f==before,"纯报价不改原会话/RNG")
	var next: Dictionary=resolve(f,"合法骑兵背击")
	check(next.units["unit.a"].position==[2,2] and next.units["unit.b"].fit.size()==75 and next.units["unit.a"].tactic_points==20,"同步实际两格背击25，收作者指挥20")
	check(next.units["unit.a"].statuses.is_empty(),"flank不继承charge的额外exposed")
	var control: Dictionary=f.duplicate(true); control.orders["unit.a"]=F.order([2,2],"attack","unit.b")
	var ordinary: Dictionary=resolve(control,"同源普通移动攻击对照")
	check(ordinary.units["unit.b"].fit.size()==80 and ordinary.units["unit.a"].tactic_points==40 and ordinary.rng==next.rng,"本轮步数记录不改变普通伤害/点数/随机抽取次数")
	f.battle.units["unit.b"].heading=1; F.wait_ai(f.battle)
	check(quote(f).ok,"相对朝向差2的侧面同样合法")
	f.battle.units["unit.b"].heading=3; F.wait_ai(f.battle); before=f.duplicate(true)
	check(Round.resolve(f.battle,f.data,f.battle.controller_actor_id,f.orders).code=="MilitaryFlankNeedsSideOrRear" and f==before,"正面侧击提交拒绝且不扣点/随机")
	f=fresh(); f.battle.units["unit.a"].position=[1,2]
	for cell in f.data.battlefields["field.open"].cells:
		if cell.q==2 and cell.r==2: cell.terrain="forest"
	check(quote(f).code=="MilitaryFlankNeedsMovement","仅一格森林虽花3点仍不满足实际两格")
	f=fresh(); f.data.troop_types["troop.horse"].movement=6
	for cell in f.data.battlefields["field.open"].cells:
		if cell.q in [1,2]: cell.terrain="forest"
	check(quote(f).ok and quote(f).cost==6 and quote(f).path.size()==3,"两格森林花6点仍按真实两步合格")
	check(resolve(f,"高地形成本两格侧击").units["unit.b"].fit.size()==75,"真实移动成本和减员符合预览")
	f=fresh(); f.battle.units["unit.a"].tactic_points=19; before=f.duplicate(true)
	check(Round.resolve(f.battle,f.data,f.battle.controller_actor_id,f.orders).code=="NotEnoughMilitaryTacticPoints" and f==before,"不足指挥资源原子拒绝")
	for category in ["troop.foot","troop.bow"]:
		f=fresh(); f.data.troop_types[category].tactics=f.data.troop_types["troop.horse"].tactics.duplicate(true); f.battle.units["unit.a"].troop_type_id=category
		check(Round.validate(f.battle,f.data).code=="MilitaryTacticTroopMismatch","定义不能给非骑兵侧击 "+category)
	for field in {"power_bp":12000,"move_bonus":1,"accuracy_bonus":1,"morale_effect":1,"status_rounds":2}:
		f=fresh(); f.data.troop_types["troop.horse"].tactics[0][field]={"power_bp":12000,"move_bonus":1,"accuracy_bonus":1,"morale_effect":1,"status_rounds":2}[field]
		check(Round.validate(f.battle,f.data).code=="UnsupportedFlankParameter","拒未实现侧击参数 "+field)
	f=fresh(); f.data.troop_types["troop.horse"].tactics[0].point_cost=7
	check(resolve(f,"作者自定侧击费用").units["unit.a"].tactic_points==33,"消费读取作者定义，不硬编码20")
	f=fresh(); f.battle.units["unit.a"].troop_type_id="troop.foot"; f.battle.units["unit.a"].position=[1,2]; f.battle.units["unit.b"].heading=3; F.wait_ai(f.battle)
	f.data.troop_types["troop.foot"].tactics=[F.tactic("tactic.charge","charge")]; f.orders["unit.a"].tactic_id="tactic.charge"
	check(quote(f).ok,"旧charge仍接受原步兵/一格/正面合同，不冒充BA04")
	next=resolve(f,"旧charge同源实际对照")
	check(next.units["unit.b"].fit.size()==70 and next.units["unit.a"].tactic_points==37 and next.units["unit.a"].statuses.has("exposed"),"旧charge保留150%/费用3/暴露，未被悄悄改成BA04")

## 敌军真实转身/移动及途中占用在同轮攻击前重验；失效仍收费，但不造成伤害或追加随机。
func simultaneous_recheck() -> void:
	var f: Dictionary=fresh()
	f.battle.ai_plans["unit.b"]=F.order([3,2],"wait","","",3)
	check(quote(f).ok,"轮初背击报价不窥对手已保存转身")
	var next: Dictionary=resolve(f,"目标同步转正面")
	check(next.units["unit.b"].fit.size()==100 and next.last_events.any(func(event): return event.get("code")=="MilitaryFlankNeedsSideOrRear"),"最新确认朝向变正面使侧击实际落空")
	check(next.units["unit.a"].tactic_points==20 and next.rng==f.battle.rng,"发动后失效沿旧战法收费，不重抽伤害")
	f=fresh(); f.battle.ai_plans["unit.b"]=F.order([4,2],"move","","",0)
	next=resolve(f,"目标同步离开射程")
	check(next.units["unit.b"].fit.size()==100 and next.units["unit.a"].tactic_points==20 and next.rng==f.battle.rng,"移动后失距同样收费落空，不自动追踪换目标")
	f=fresh(); f.battle.units["unit.b"].position=[2,2]; f.orders["unit.a"].destination=[2,1]
	f.battle.ai_plans["unit.b"]=F.order([2,1],"move","","",0)
	check(quote(f).ok and quote(f).path==[[0,2],[1,1],[2,1]],"规划确实两步且初始侧面有效")
	next=resolve(f,"第二步被真实敌人抢占")
	check(next.units["unit.a"].position==[1,1] and next.units["unit.b"].position==[2,1],"实际仅走一步，单位不穿透或重叠")
	check(next.units["unit.b"].fit.size()==100 and next.last_events.any(func(event): return event.get("code")=="MilitaryFlankNeedsMovement"),"虽仍邻接背面也按实际一步拒绝攻击")
	check(next.units["unit.a"].tactic_points==20 and next.rng==f.battle.rng,"受阻侧击同样只消费定义点数，无额外随机")

## AI自主生成侧击并实际结算；仅消费公开轮初信息，存读重放保存同一计划。
func ai_and_replay() -> void:
	var f: Dictionary=fresh(); f.battle.controller_actor_id="actor.unit.b"; f.battle.controller_side=1; f.battle.command_unit_ids=["unit.b"]
	var before: Dictionary=f.duplicate(true); var ai: Dictionary=Round.make_ai(f.battle,f.data)
	check(ai.ok and ai.orders["unit.a"].tactic_id=="tactic.flank" and Round.preview(f.battle,f.data,"unit.a",ai.orders["unit.a"]).ok,"AI自主选择两格侧后合法flank")
	check(f==before,"AI不改变原输入/RNG")
	var draft: Dictionary=f.battle.duplicate(true); draft.pending_orders={"unit.b":{"action":"untrusted"}}; draft.ai_plans={}
	check(Round.make_ai(draft,f.data)==ai,"AI不读取玩家草案或旧AI计划")
	f.battle.ai_plans=ai.orders; f.orders={"unit.b":F.order([3,2],"wait","","",0)}
	var next: Dictionary=resolve(f,"实际AI侧击")
	check(next.units["unit.b"].fit.size()==75 and next.units["unit.a"].tactic_points==20,"AI实际侧击遵循同一125%与费用")
	var loaded: Dictionary=Strict.parse_text(JSON.stringify(f.battle))
	check(loaded.ok and Round.validate(loaded.value,f.data,true).ok and Round.resolve(loaded.value,f.data,f.battle.controller_actor_id,f.orders).battle==next,"保存计划复读整结果/名册/RNG完全一致")
	f.battle.units["unit.a"].tactic_points=19
	check(Round.make_ai(f.battle,f.data).orders["unit.a"].tactic_id!="tactic.flank","AI不足点数不选侧击")
	f.battle.units["unit.a"].tactic_points=40; f.data.troop_types["troop.horse"].movement=1
	check(Round.make_ai(f.battle,f.data).orders["unit.a"].tactic_id!="tactic.flank","AI实际预算不足两步时不伪造侧击")

## 专属短门只覆盖本战法和其同步移动影响，不启动世界或UI。
func _run() -> void:
	qualification_and_damage(); simultaneous_recheck(); ai_and_replay()
	print("MILITARY_FLANK_CHECKS=",checks," FAILURES=",failures)
	quit(0 if failures==0 else 1)
