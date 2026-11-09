## BA08/T44局部规则门：同一真实Round入口核首伤分摊、名册守恒、计划复读及失败原子性。
extends SceneTree
const Round=preload("res://core/military_round.gd")
const Support=preload("res://core/domains/military/military_support.gd")
const F=preload("res://tests/military_fixture.gd")
const Strict=preload("res://core/strict_json.gd")
var checks: int=0
var failures: int=0

## 资源和全局类导入完成后只执行这一协防短门。
func _init() -> void:
	call_deferred("_run")

## 局部失败影响退出码，不把编译通过或战斗能结束冒充守恒成立。
func check(value: bool, label: String) -> void:
	checks+=1
	if not value: failures+=1; printerr("FAIL: ",label)

## 复用现有六角夹具及原兵种结构，20点仅该作者样本，不变成生产固定成本。
func fresh() -> Dictionary:
	var data: Dictionary=F.data()
	var troop: Dictionary=data.troop_types["troop.foot"]
	troop.attack=100; troop.accuracy=100; troop.defense=0; troop.tactic_capacity=40
	troop.tactics.append({"id":"tactic.support","kind":"support","point_cost":20,"power_bp":10000,"accuracy_bonus":0,"morale_effect":0,"move_bonus":0,"status_rounds":1})
	var battle: Dictionary=F.battle(data)
	battle.units={"unit.a":F.unit("unit.a",0,[1,1],"troop.foot",100),"unit.b":F.unit("unit.b",1,[3,1],"troop.foot",20),"unit.c":F.unit("unit.c",0,[2,1],"troop.foot",100)}
	for unit in battle.units.values(): unit.morale=100; unit.tactic_points=40
	battle.command_scope="army"; battle.command_army_id="army.0"; battle.command_unit_ids=["unit.a","unit.c"]
	battle.ai_plans={"unit.b":F.order([3,1],"attack","unit.c","",3)}
	return {"data":data,"battle":battle,"orders":{"unit.a":F.order([1,1],"tactic","unit.c","tactic.support"),"unit.c":F.order([2,1],"wait")}}

## 额外部队仍由原测试编制函数建立；正式Round自行验证阵营/名册/授权与保存计划。
func add_unit(f: Dictionary, id: String, side: int, position: Array, count: int=100) -> void:
	f.battle.units[id]=F.unit(id,side,position,"troop.foot",count)
	f.battle.units[id].morale=100; f.battle.units[id].tactic_points=40
	if side==0:
		f.battle.command_unit_ids.append(id); f.battle.command_unit_ids.sort()
		f.orders[id]=F.order(position,"wait")
	else: f.battle.ai_plans[id]=F.order(position,"wait","","",3)

## 所有成功和失败都调用原轮次入口，并逐次证明输入完整状态/RNG与作者定义未被修改。
func resolve(f: Dictionary, label: String) -> Dictionary:
	var before: Dictionary=f.duplicate(true)
	var result: Dictionary=Round.resolve(f.battle,f.data,f.battle.controller_actor_id,f.orders)
	check(result.ok,label+" 正式Round成功："+str(result))
	check(f==before,label+" 原始会话、RNG、输入计划和定义不变")
	if not result.ok: return f.battle
	check(Round.validate(result.battle,f.data,true).ok,label+" 结算后五类名册与保存AI完整合法")
	for id in f.battle.units:
		check(result.battle.units[id].member_ids==f.battle.units[id].member_ids,label+" "+id+" 总编制不增减")
	return result.battle

## 按实际attack/support事件读本轮减员；分摊后的两部总计须等于原始正数伤害。
func support_events(battle: Dictionary) -> Array:
	return battle.last_events.filter(func(event): return event.kind=="support")

## 正式预览拒绝自保/敌方/非邻友/移动/无授权/不足点数及未实现参数，且不污染输入。
func targeting() -> void:
	var f: Dictionary=fresh(); var before: Dictionary=f.duplicate(true)
	var quote: Dictionary=Round.preview(f.battle,f.data,"unit.a",f.orders["unit.a"])
	check(quote.ok and quote.support_share_bp==2000 and f==before,"预览只读且公开20%分摊比例")
	for row in [["unit.a","MilitarySupportRequiresOtherUnit"],["unit.b","MilitarySupportRequiresAlly"],["missing","MilitarySupportUnavailable"]]:
		var order: Dictionary=f.orders["unit.a"].duplicate(); order.target_id=row[0]
		check(Round.preview(f.battle,f.data,"unit.a",order).get("code")==row[1] and f==before,"拒绝错误协防目标："+row[1])
	f.battle.units["unit.c"].position=[4,3]
	check(Round.preview(f.battle,f.data,"unit.a",f.orders["unit.a"]).code=="MilitarySupportRequiresAdjacent","远方友部不能变隐形盾牌")
	f=fresh(); f.orders["unit.a"].destination=[0,1]
	check(Round.preview(f.battle,f.data,"unit.a",f.orders["unit.a"]).code=="MilitaryActionRequiresHold","协防是一项原地主动战法")
	f=fresh(); f.battle.units["unit.a"].tactic_points=19; before=f.duplicate(true)
	var failed: Dictionary=Round.resolve(f.battle,f.data,f.battle.controller_actor_id,f.orders)
	check(failed.code=="NotEnoughMilitaryTacticPoints" and f==before,"不足现定义点数失败整态/RNG回滚")
	f=fresh(); f.battle.command_scope="unit"; f.battle.command_army_id=""; f.battle.command_unit_ids=["unit.a"]; f.battle.ai_plans["unit.c"]=f.orders["unit.c"]
	check(Round.validate_orders(f.battle,f.data,f.battle.controller_actor_id,f.orders).code=="MilitaryUnitUnauthorized","协防不授予友军额外指挥权")
	f=fresh(); f.data.troop_types["troop.foot"].tactics[-1].power_bp=20000
	check(Round.validate(f.battle,f.data).code=="UnsupportedSupportParameter","协防不能夹带倍伤或第二套可配置比例")
	f=fresh(); f.data.troop_types["troop.bow"].tactic_capacity=40; f.data.troop_types["troop.bow"].tactics.append(f.data.troop_types["troop.foot"].tactics[-1].duplicate()); f.battle.units["unit.a"].troop_type_id="troop.bow"
	check(Round.validate(f.battle,f.data).code=="MilitaryTacticTroopMismatch","步兵协防不能通过弓兵战法字段偷开")

## 单次首伤转20%，点数只消费一次；普通攻击和协防合计不超原始伤亡。
func first_hit_and_repeat() -> void:
	var f: Dictionary=fresh(); var next: Dictionary=resolve(f,"首个真实命中")
	var events: Array=support_events(next)
	check(events.size()==1 and events[0].unit_id=="unit.a" and events[0].target_id=="unit.c" and events[0].original_casualties==20 and events[0].casualties==4,"首伤20按一次基点取整分为4协防")
	check(next.units["unit.a"].fit.size()==96 and next.units["unit.c"].fit.size()==84,"原目标损失16且协防部损失4，实际分类守恒")
	check(next.units["unit.a"].tactic_points==20,"只扣作者定义20点一次")
	var plain: Dictionary=f.duplicate(true); plain.orders["unit.a"]=F.order([1,1],"wait")
	var normal: Dictionary=resolve(plain,"不声明协防对照")
	check(normal.units["unit.c"].fit.size()==80 and next.rng==normal.rng,"协防不重抽命中，同一原伤害只在两部之间转移")
	f=fresh(); add_unit(f,"unit.d",1,[2,2],20); f.battle.ai_plans["unit.d"]=F.order([2,2],"attack","unit.c","",3)
	next=resolve(f,"同轮两敌攻击")
	check(support_events(next).size()==1 and next.units["unit.a"].fit.size()==96 and next.units["unit.c"].fit.size()==64,"同轮仅首个正伤触发，第二击完整留给目标")
	var small: Dictionary=fresh(); small.battle.units["unit.b"]=F.unit("unit.b",1,[3,1],"troop.foot",3); small.battle.units["unit.b"].morale=100; small.battle.units["unit.b"].tactic_points=40
	add_unit(small,"unit.d",1,[2,2],20); small.battle.ai_plans["unit.d"]=F.order([2,2],"attack","unit.c","",3)
	next=resolve(small,"首伤不足5的取整边界")
	check(support_events(next).is_empty() and next.units["unit.c"].fit.size()==77 and next.units["unit.a"].fit.size()==100,"首伤3取整0仍消费首伤机会，不延迟到更大伤害")

## 多声明稳定选唯一协防者；转移伤害不引出链式或循环协防。
func arbitration_and_nonrecursive() -> void:
	var f: Dictionary=fresh(); add_unit(f,"unit.e",0,[1,2])
	f.orders["unit.e"]=F.order([1,2],"tactic","unit.c","tactic.support")
	var next: Dictionary=resolve(f,"两部保护同一友部")
	check(support_events(next).size()==1 and support_events(next)[0].unit_id=="unit.a" and next.units["unit.e"].fit.size()==100,"稳定ID选择一部，不叠成40%")
	var reversed: Dictionary=f.duplicate(true); reversed.battle.units={}; reversed.orders={}
	var keys: Array=f.battle.units.keys(); keys.reverse()
	for id in keys: reversed.battle.units[id]=f.battle.units[id].duplicate(true)
	keys=f.orders.keys(); keys.reverse()
	for id in keys: reversed.orders[id]=f.orders[id].duplicate(true)
	check(Round.resolve(reversed.battle,reversed.data,reversed.battle.controller_actor_id,reversed.orders).battle==next,"字典插入顺序反转不改变完整结果/RNG")
	f.orders["unit.e"]=F.order([1,2],"tactic","unit.a","tactic.support")
	next=resolve(f,"协防链")
	check(support_events(next).size()==1 and next.units["unit.a"].fit.size()==96 and next.units["unit.e"].fit.size()==100,"转移给a的4伤害不递归转给e")
	f=fresh(); f.orders["unit.c"]=F.order([2,1],"tactic","unit.a","tactic.support")
	next=resolve(f,"互相协防")
	check(support_events(next).size()==1 and next.units["unit.a"].fit.size()==96 and next.units["unit.c"].fit.size()==84,"互相保护不循环递归、不制造第二次减伤")

## 移动后重新核邻接，首伤与已累计伤亡容量采用同一同步局面。
func movement_capacity_and_replay() -> void:
	var f: Dictionary=fresh(); f.orders["unit.c"]=F.order([3,0],"move")
	var next: Dictionary=resolve(f,"友军移动离开协防邻格")
	check(next.units["unit.c"].position==[3,0] and support_events(next).is_empty() and next.units["unit.c"].fit.size()==80,"实际离开邻格后攻击仍成立但不能远程分摊")
	check(next.units["unit.a"].tactic_points==20,"已执行原地协防不凭结果退回本轮行动成本")
	f=fresh(); f.battle.units["unit.a"]=F.unit("unit.a",0,[1,1],"troop.foot",1); f.battle.units["unit.a"].morale=100; f.battle.units["unit.a"].tactic_points=40
	next=resolve(f,"协防容量只有一人")
	check(next.units["unit.a"].fit.is_empty() and next.units["unit.c"].fit.size()==81 and support_events(next)[0].casualties==1,"本部只承担现有1人，其余3仍留原目标，无伤害凭空消失")
	f=fresh()
	var assigned: Dictionary=Support.assignments(f.battle,f.data,f.orders)
	var allocation: Dictionary=Support.allocate(f.battle,assigned,{}, {"unit.a":100},"unit.c",20)
	check(allocation.target_casualties==20 and allocation.supporter_casualties==0,"已经累计致命伤的协防部没有第二份容量")
	var first: Dictionary=Round.resolve(f.battle,f.data,f.battle.controller_actor_id,f.orders)
	var loaded: Dictionary=Strict.parse_text(JSON.stringify(f.battle))
	check(loaded.ok and Round.validate(loaded.value,f.data,true).ok,"已保存协防命令经Strict读取保持合法")
	check(Round.resolve(loaded.value,f.data,f.battle.controller_actor_id,f.orders)==first,"稳定轮次保存/重读不改分摊、名册或随机状态")
	check(Round.resolve(f.battle,f.data,f.battle.controller_actor_id,f.orders)==first,"重复读取同一候选产生相同结果，不累加伤亡")

## 自主协防只看公开威胁，旧撤退/攻击优先级保留；越权草案与字典顺序不能左右AI。
func autonomous_support() -> void:
	var f: Dictionary=fresh()
	var troop: Dictionary=f.data.troop_types["troop.foot"]
	troop.movement=1; troop.tactics=[troop.tactics[-1]]
	var bow: Dictionary=f.data.troop_types["troop.bow"]
	bow.attack=100; bow.accuracy=100; bow.defense=0; bow.tactic_capacity=40
	# 敌弓仅在距2射界内可攻击邻友；不预先伪写AI目标来强行制造协防命中。
	bow.movement=0; bow.maximum_range=2; bow.tactics=[]
	f.battle.units["unit.b"]=F.unit("unit.b",1,[4,1],"troop.bow",100)
	f.battle.units["unit.b"].morale=100; f.battle.units["unit.b"].tactic_points=40
	f.battle.command_scope="unit"; f.battle.command_army_id=""; f.battle.command_unit_ids=["unit.c"]; f.battle.controller_actor_id="actor.unit.c"
	var before: Dictionary=f.duplicate(true)
	var planned: Dictionary=Round.make_ai(f.battle,f.data)
	check(planned.ok and planned.orders["unit.a"].action=="tactic" and planned.orders["unit.a"].tactic_id=="tactic.support" and planned.orders["unit.a"].target_id=="unit.c","无法直接打到弓手的AI按公开射界自主选择邻友协防")
	check(f==before and Round.preview(f.battle,f.data,"unit.a",planned.orders["unit.a"]).ok,"AI合法选招不改变完整局面/随机流")
	var changed: Dictionary=f.battle.duplicate(true)
	changed.ai_plans={"garbage":{"action":"forged"}}; changed.pending_orders={"unit.c":{"action":"arbitrary player draft"}}
	check(Round.make_ai(changed,f.data)==planned,"AI不读取已有敌我计划或尚未提交玩家草案")
	var reversed: Dictionary=f.battle.duplicate(true); reversed.units={}
	var ids: Array=f.battle.units.keys(); ids.reverse()
	for id in ids: reversed.units[id]=f.battle.units[id].duplicate(true)
	check(Round.make_ai(reversed,f.data)==planned,"公开单位遍历顺序不改变协防目标或计划")
	f.battle.ai_plans=planned.orders
	f.orders={"unit.c":F.order([2,1],"wait")}
	var next: Dictionary=resolve(f,"自主协防进入同一真实Round")
	check(not support_events(next).is_empty() and support_events(next)[0].unit_id=="unit.a","实际冻结AI协防计划被统一结算消费："+str(planned.orders))
	f=fresh(); f.data.troop_types["troop.foot"].tactics=[f.data.troop_types["troop.foot"].tactics[-1]]
	f.battle.command_scope="unit"; f.battle.command_army_id=""; f.battle.command_unit_ids=["unit.c"]; f.battle.controller_actor_id="actor.unit.c"
	f.battle.units["unit.a"].tactic_points=19
	planned=Round.make_ai(f.battle,f.data)
	check(planned.ok and planned.orders["unit.a"].get("tactic_id","")!="tactic.support","不足点数的AI仍选择其他原合法行为")
	f.battle.units["unit.a"].tactic_points=40; f.battle.units["unit.b"].position=[4,3]
	planned=Round.make_ai(f.battle,f.data)
	check(planned.ok and planned.orders["unit.a"].get("tactic_id","")!="tactic.support","没有轮初合法直接威胁不无故消费协防点数")

## 只执行本新增战法危险边界；旧战法相邻门由有界runner按需另跑一次。
func _run() -> void:
	targeting(); first_hit_and_repeat(); arbitration_and_nonrecursive(); movement_capacity_and_replay(); autonomous_support()
	print("MILITARY_SUPPORT_CHECKS=",checks," FAILURES=",failures)
	quit(0 if failures==0 else 1)
