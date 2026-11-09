## 六角野战规则验收：实际名单守恒、指挥范围、保存意图、兵种战法与独立目标均走同一纯结算入口。
extends SceneTree
const Round=preload("res://core/military_round.gd")
const F=preload("res://tests/military_fixture.gd")
const Hex=preload("res://core/military_hex.gd")
const Strict=preload("res://core/strict_json.gd")
var checks: int=0
var failures: int=0

## 延迟至树可用后运行，不读取正式存档、世界 Kernel 或系统时间。
func _init() -> void:
	call_deferred("run_checks")

## 断言失败保留明确标签，最终进程不能因部分目标完成而假报成功。
func check(value: bool, label: String) -> void:
	checks+=1
	if not value: failures+=1; printerr("FAIL: ",label)

## 每次提交先保存输入，验证成功与失败均不修改调用方状态或作者定义。
func step(battle: Dictionary, data: Dictionary, orders: Dictionary, label: String) -> Dictionary:
	var before: Dictionary=battle.duplicate(true); var original: Dictionary=data.duplicate(true)
	var result: Dictionary=Round.resolve(battle,data,battle.controller_actor_id,orders)
	check(result.get("ok",false),label+"："+str(result.get("code",""))+" "+str(result.get("path","")))
	check(battle==before and data==original,label+" 输入保持不变")
	return result.get("battle",battle)

## 一次运行覆盖边界、实效与两组自主会战，不以只有字段的结构检查代替真实交战结果。
func run_checks() -> void:
	check_authority_and_replay()
	check_movement()
	check_tactics_and_damage()
	check_objectives()
	check_autonomous()
	check_ten_units()
	print("MILITARY_ROUND_CHECKS=",checks," FAILURES=",failures)
	quit(0 if failures==0 else 1)

## 部将、统帅和远方自主会战各有明确权限；坏名单与被替换的保存 AI 必须清楚失败。
func check_authority_and_replay() -> void:
	var data: Dictionary=F.data(); var battle: Dictionary=F.battle(data)
	check(Round.validate(battle,data).ok,"实际部队名册与六角会话有效")
	check(Round.validate(battle,data,true).ok,"恢复入口可核验已保存 AI 的完整性和合法性")
	var original: Dictionary=battle.duplicate(true)
	var orders: Dictionary={"unit.a":F.order([1,1],"move")}
	var quote: Dictionary=Round.preview(battle,data,"unit.a",orders["unit.a"])
	check(quote.ok and quote.path[-1]==[1,1] and quote.cost==1,"预览给出实际六角落点与代价")
	check(Round.preview(battle,data,"unit.a",orders["unit.a"])==quote and battle==original,"重复预览不耗随机数或改动局面")
	var ai: Dictionary=Round.make_ai(battle,data)
	var draft: Dictionary=battle.duplicate(true); draft.player_draft={"unit.a":F.order([2,0],"move")}; draft.ai_plans={"unit.b":{"action":"invalid"}}
	check(ai==Round.make_ai(draft,data),"AI 不偷看玩家草案或自己过去保存的计划")
	check(not Round.validate_orders(battle,data,"actor.unit.b",orders).ok,"敌方指挥官不能伪装玩家发令")
	check(Round.validate_orders(battle,data,battle.controller_actor_id,{}).code=="MissingMilitaryUnitOrder","部将必须明确给出本部命令")
	check(Round.validate_orders(battle,data,battle.controller_actor_id,{"unit.b":F.order([3,1])}).code=="MilitaryUnitUnauthorized","部将无敌方部队控制权")
	battle.units["unit.c"]=F.unit("unit.c",0,[0,3])
	F.wait_ai(battle)
	check(Round.validate_orders(battle,data,battle.controller_actor_id,{"unit.a":F.order([0,1]),"unit.c":F.order([0,3])}).code=="MilitaryUnitUnauthorized","部将也不能越权控制同侧其他部队")
	battle.command_scope="army"; battle.command_army_id="army.0"; battle.command_unit_ids=["unit.a","unit.c"]
	F.wait_ai(battle)
	var army_orders: Dictionary={"unit.a":F.order([0,1]),"unit.c":F.order([0,3])}
	check(Round.validate_orders(battle,data,battle.controller_actor_id,army_orders).ok,"统帅可以提交所辖授权集")
	var damaged: Dictionary=battle.duplicate(true); damaged.units["unit.c"].army_id="army.other"
	check(Round.validate_orders(damaged,data,battle.controller_actor_id,army_orders).code=="MilitaryArmyUnauthorized","同侧其他军团仍不属于统帅授权集")
	damaged=battle.duplicate(true); damaged.units["unit.a"].wounded.append(damaged.units["unit.a"].fit[0])
	check(Round.validate(damaged,data).code=="MilitaryRosterNotConserved","同一兵员不能同时可战和受伤")
	damaged=battle.duplicate(true); damaged.units["unit.c"].member_ids[0]=damaged.units["unit.a"].member_ids[0]; damaged.units["unit.c"].fit[0]=damaged.units["unit.a"].member_ids[0]
	check(Round.validate(damaged,data).code=="DuplicateMilitaryMember","同一兵员不能在两个部队登记")
	damaged=battle.duplicate(true); damaged.units["unit.c"].troop_type_id="troop.unknown"
	check(Round.validate(damaged,data).code=="UnknownTroopType","未知兵种不能退回默认步卒")
	check(Round.preview(battle,data,"unit.a",F.order([0,1],"tactic","","skill.cleave")).code=="MilitaryTacticNotKnown","个人武功不能直接充当部队战法")
	damaged=battle.duplicate(true); damaged.ai_plans={}
	var saved: Dictionary=damaged.duplicate(true)
	check(Round.resolve(damaged,data,battle.controller_actor_id,army_orders).code=="MissingMilitaryAIOrder" and damaged==saved,"保存 AI 缺失拒绝，不在提交时重想敌军命令")
	check(Round.validate(damaged,data,true).code=="MissingMilitaryAIOrder","存档恢复也能拒绝缺失 AI，初建与恢复边界明确")
	damaged=battle.duplicate(true); damaged.ai_plans["unit.b"].destination=[99,99]
	check(Round.resolve(damaged,data,battle.controller_actor_id,army_orders).code=="InvalidMilitaryAIOrder","损坏保存 AI 清楚失败")
	damaged=battle.duplicate(true); damaged.units["unit.c"].withdrawn=true; damaged.objective_state.escaped_unit_ids=["unit.c"]
	check(Round.validate(damaged,data).code=="InvalidMilitaryWithdrawal","伪造在地图中央撤离的存档拒绝")
	var result: Dictionary=Round.resolve(battle,data,battle.controller_actor_id,army_orders)
	var decoded: Dictionary=Strict.parse_text(JSON.stringify(battle))
	check(result.ok and decoded.ok and Round.resolve(decoded.value,data,battle.controller_actor_id,army_orders)==result,"轮间经过真实严格 JSON 重启后相同命令产生相同结果")
	check(battle.rng==saved.rng and battle.round==1,"结算只生成副本，没有私自推进原世界或原会话")

## 真实六角移动、代价、控制区与同时争格都作用于部队位置，不能交换占位绕过敌军。
func check_movement() -> void:
	var data: Dictionary=F.data(); var battle: Dictionary=F.battle(data)
	battle.units["unit.b"].position=[4,3]; battle.units["unit.c"]=F.unit("unit.c",0,[1,0])
	battle.command_scope="army"; battle.command_army_id="army.0"; battle.command_unit_ids=["unit.a","unit.c"]
	F.wait_ai(battle)
	var next: Dictionary=step(battle,data,{"unit.a":F.order([1,1],"move"),"unit.c":F.order([1,1],"move")},"同时争抢同一空格")
	check(next.units["unit.a"].position==[0,1] and next.units["unit.c"].position==[1,0],"争格双方停步，实际部队没有重叠")
	check(not Round.validate_orders(battle,data,battle.controller_actor_id,{"unit.a":F.order([1,0],"move"),"unit.c":F.order([0,1],"move")}).ok,"不可交换或穿过彼此起始占格")
	battle=F.battle(data); battle.units["unit.b"].position=[4,3]; F.wait_ai(battle)
	Hex.cell_at(data.battlefields["field.open"],[1,1]).terrain="forest"
	var quote: Dictionary=Round.preview(battle,data,"unit.a",F.order([1,1],"move"))
	next=step(battle,data,{"unit.a":F.order([1,1],"move")},"按森林代价行军")
	check(quote.ok and quote.cost==2 and next.units["unit.a"].position==quote.path[-1],"移动圈预览与实际地形落点一致")
	data=F.data(); battle=F.battle(data); battle.units["unit.b"].position=[2,1]
	data.troop_types["troop.foot"].movement=4
	check(not Round.preview(battle,data,"unit.a",F.order([3,1],"move")).ok,"敌方控制区阻止穿行而不是只有视觉标记")
	battle=F.battle(data); battle.units["unit.b"].position=[2,1]
	battle.ai_plans={"unit.b":F.order([3,1],"move","","",3)}
	var charge: Dictionary=F.order([1,1],"tactic","unit.b","tactic.charge")
	next=step(battle,data,{"unit.a":charge},"敌人移开后重查突击射程")
	check(next.units["unit.b"].fit==battle.units["unit.b"].fit and next.units["unit.a"].tactic_points==5,"突击扑空仍消耗战法点，不凭旧落点命中敌人")
	check(next.last_events.any(func(event): return event.get("code")=="MilitaryTargetOutOfRange"),"落点变化留下实际射空原因")

## 三类兵种、掩护、侧后、防御、士气和三种战法都改变真实命中或人员结果，并保留状态期限。
func check_tactics_and_damage() -> void:
	var data: Dictionary=F.data(); var battle: Dictionary=F.battle(data)
	battle.units["unit.a"].troop_type_id="troop.bow"; battle.units["unit.b"].position=[1,1]
	check(Round.preview(battle,data,"unit.a",F.order([0,1],"attack","unit.b")).ok,"T43弓兵贴身普通攻击沿减效近战，远程战法仍由射程核验")
	battle.units["unit.b"].position=[4,1]; F.wait_ai(battle)
	check(not Round.preview(battle,data,"unit.a",F.order([0,1],"attack","unit.b")).ok and Round.preview(battle,data,"unit.a",F.order([0,1],"tactic","unit.b","tactic.volley")).ok,"齐射实际增加一格射程")
	var next: Dictionary=step(battle,data,{"unit.a":F.order([0,1],"tactic","unit.b","tactic.volley")},"齐射命中")
	check(next.units["unit.a"].tactic_points==5 and next.units["unit.b"].fit.size()<battle.units["unit.b"].fit.size() and next.units["unit.b"].statuses.has("suppressed"),"齐射消耗点数、造成伤亡并留下压制")
	check(not Round.preview(next,data,"unit.b",F.order([2,1],"move")).ok,"受压制者移动预算真实降低")
	for _index in range(2):
		F.wait_ai(next)
		next=step(next,data,{"unit.a":F.order(next.units["unit.a"].position)},"等待齐射压制到期")
	check(not next.units["unit.b"].statuses.has("suppressed") and Round.preview(next,data,"unit.b",F.order([2,1],"move")).ok,"状态按会战轮次到期，恢复原移动能力")
	data=F.data(); battle=F.battle(data); battle.units["unit.b"].position=[4,1]; F.wait_ai(battle)
	check(not Round.preview(battle,data,"unit.a",F.order([3,1],"attack","unit.b")).ok and Round.preview(battle,data,"unit.a",F.order([3,1],"tactic","unit.b","tactic.charge")).ok,"突击增加实际移动距离")
	next=step(battle,data,{"unit.a":F.order([3,1],"tactic","unit.b","tactic.charge")},"突击接战")
	check(next.units["unit.a"].position==[3,1] and next.units["unit.a"].statuses.get("exposed",0)==2,"突击落点与暴露状态进入同一会话")
	battle=F.battle(data); battle.units["unit.b"].position=[4,3]; battle.units["unit.a"].morale=10
	battle.units["unit.c"]=F.unit("unit.c",0,[1,1]); battle.units["unit.c"].morale=12; F.wait_ai(battle)
	next=step(battle,data,{"unit.a":F.order([0,1],"tactic","","tactic.rally")},"鼓舞同侧相邻部队")
	check(next.units["unit.a"].morale==30 and next.units["unit.c"].morale>=32 and next.units["unit.a"].tactic_points==5,"鼓舞真实恢复自己和邻军士气且扣战法点")
	check(next.units["unit.a"].statuses.has("inspired"),"鼓舞留下有期限的命中增益")
	F.wait_ai(next); next=step(next,data,{"unit.a":F.order([0,1])},"鼓舞期限结算")
	check(not next.units["unit.a"].statuses.has("inspired"),"鼓舞状态到期不会永久叠加")
	battle=F.battle(data); battle.units["unit.a"].position=[1,1]; battle.units["unit.b"].position=[2,1]; F.wait_ai(battle)
	var ordinary: Dictionary=F.order([1,1],"attack","unit.b")
	check(Round.preview(battle,data,"unit.a",F.order([1,1],"tactic","unit.b","tactic.charge")).code=="MilitaryChargeNeedsMovement","突击必须有实际助跑落点，不能原地白拿倍率")
	var front: Dictionary=Round.preview(battle,data,"unit.a",ordinary)
	var flank: Dictionary=battle.duplicate(true); flank.units["unit.b"].heading=0
	check(Round.preview(flank,data,"unit.a",ordinary).hit_chance>front.hit_chance,"侧后方向真实改变命中")
	var woods: Dictionary=data.duplicate(true); Hex.cell_at(woods.battlefields["field.open"],[2,1]).terrain="forest"
	check(Round.preview(battle,woods,"unit.a",ordinary).hit_chance<front.hit_chance,"目标森林掩护真实降低命中")
	var low: Dictionary=battle.duplicate(true); low.units["unit.a"].morale=20
	check(Round.preview(low,data,"unit.a",ordinary).casualties_max<front.casualties_max,"低士气真实削弱攻击效果")
	var normal: Dictionary=step(battle,data,{"unit.a":ordinary},"普通攻击造成实际兵员伤亡")
	var defending: Dictionary=battle.duplicate(true); defending.ai_plans["unit.b"]=F.order([2,1],"defend","","",3)
	var defended: Dictionary=step(defending,data,{"unit.a":ordinary},"步卒防御减伤")
	check(defended.units["unit.b"].fit.size()>normal.units["unit.b"].fit.size(),"防御保全更多实际兵员")
	check(not normal.units["unit.b"].dead.is_empty() and not normal.units["unit.b"].wounded.is_empty() and Round.validate(normal,data).ok,"同一攻击区分战死与伤员且保持名册守恒")
	var cavalry: Dictionary=battle.duplicate(true); cavalry.units["unit.a"].troop_type_id="troop.horse"
	var forest: Dictionary=data.duplicate(true); Hex.cell_at(forest.battlefields["field.open"],[1,1]).terrain="forest"
	check(Round.preview(cavalry,forest,"unit.a",ordinary).hit_chance<Round.preview(battle,forest,"unit.a",ordinary).hit_chance,"骑兵在森林接战有实际命中劣势")
	battle=F.battle(data); battle.units["unit.a"]=F.unit("unit.a",0,[1,1],"troop.foot",48); battle.units["unit.b"]=F.unit("unit.b",1,[2,1],"troop.foot",48); battle.units["unit.b"].morale=1; F.wait_ai(battle)
	next=step(battle,data,{"unit.a":F.order([1,1],"attack","unit.b")},"低士气部队受击溃退")
	check(next.units["unit.b"].fit.is_empty() and not next.units["unit.b"].captured.is_empty() and not next.units["unit.b"].deserted.is_empty(),"溃退转入实际俘虏和逃散名单")
	check(next.units["unit.b"].dead.size()<next.units["unit.b"].member_ids.size() and Round.validate(next,data).ok,"士气溃退不是全体死亡，五类兵员总量守恒")
	battle.units["unit.c"]=F.unit("unit.c",0,[2,0]); F.wait_ai(battle)
	next=step(battle,data,{"unit.a":F.order([1,1],"attack","unit.b")},"包围中的部队溃退")
	check(next.units["unit.b"].deserted.is_empty() and next.units["unit.b"].wounded.is_empty() and next.units["unit.b"].captured.size()>0,"被两侧包围时余部及伤员被俘，不能仍留两份伤员账")
	var route_events: Array=next.last_events.filter(func(event): return event.get("kind")=="rout")
	check(route_events.size()==1 and route_events[0].captured==next.units["unit.b"].captured.size(),"包围战果的俘虏数字包含实际被转入的伤员")
	check(route_events[0].captured_ids.size()==route_events[0].captured and route_events[0].captor_unit_id=="unit.a" and route_events[0].captor_army_id=="army.0","俘获事件引用实际兵员、看管部队和军团，供世界接收看管账")
	data=F.data(); data.troop_types["troop.foot"].accuracy=100; data.troop_types["troop.foot"].attack=100
	battle=F.battle(data); battle.units["unit.a"].position=[1,1]; battle.units["unit.b"]=F.unit("unit.b",1,[2,1],"troop.foot",1)
	battle.units["unit.c"]=F.unit("unit.c",0,[2,0]); battle.command_scope="army"; battle.command_army_id="army.0"; battle.command_unit_ids=["unit.a","unit.c"]; F.wait_ai(battle)
	next=step(battle,data,{"unit.a":F.order([1,1],"attack","unit.b"),"unit.c":F.order([2,0],"attack","unit.b")},"两部同时攻击同一名剩余敌兵")
	var accounted: int=0
	for event in next.last_events:
		if event.get("kind")=="attack": accounted+=event.casualties
	check(accounted==1 and next.units["unit.b"].fit.is_empty(),"同时攻击的事件总数不能重复申领同一兵员伤亡")

## 击退、守点、突破、护送与掩护撤离分别有完成条件，保护失败优先于主力仍可作战。
func check_objectives() -> void:
	var data: Dictionary=F.data(); var battle: Dictionary=F.battle(data,"goal.hold")
	battle.units["unit.a"].position=[1,1]; battle.units["unit.b"].position=[3,1]; F.wait_ai(battle)
	var next: Dictionary=step(battle,data,{"unit.a":F.order([1,1])},"守住点位第一轮")
	check(next.phase=="planning" and next.objective_state.hold_rounds==1,"守点需要真实连续轮数")
	var contested: Dictionary=next.duplicate(true); contested.ai_plans["unit.b"]=F.order([2,1],"move","","",3)
	contested=step(contested,data,{"unit.a":F.order([1,1])},"敌部进入据点控制区")
	check(contested.phase=="planning" and contested.objective_state.hold_rounds==0,"敌人进入控制区会打断连续守点")
	F.wait_ai(next); next=step(next,data,{"unit.a":F.order([1,1])},"守点第二轮完成")
	check(next.phase=="result" and next.result.reason=="MilitaryPointHeld" and next.result.winner_side==0,"守点无需歼灭敌人也能胜利")
	check(not Round.resolve(next,data,next.controller_actor_id,{}).ok,"终局不能再结算一轮复制战果")
	battle=F.battle(data,"goal.break"); battle.units["unit.a"].position=[3,0]; battle.units["unit.b"].position=[0,3]; F.wait_ai(battle)
	next=step(battle,data,{"unit.a":F.order([4,0],"withdraw")},"突围到本阵营出口")
	check(next.result.get("reason")=="MilitaryBreakthrough" and next.units["unit.a"].withdrawn and next.units["unit.a"].fit==battle.units["unit.a"].fit,"突破以实际退至出口完成，保留可战人员")
	check(not Round.preview(battle,data,"unit.a",F.order([0,0],"withdraw")).ok,"不能借对方出口越权撤离")
	var forbidden: Dictionary=data.duplicate(true); forbidden.military_objectives["goal.repel"].allow_withdrawal=false
	battle.objective_id="goal.repel"
	check(Round.preview(battle,forbidden,"unit.a",F.order([4,0],"withdraw")).code=="MilitaryWithdrawalForbidden","禁止撤离的军令具有实际命令约束")
	battle=F.battle(data,"goal.escort"); battle.units["unit.a"].position=[3,0]; battle.units["unit.b"].position=[0,3]; F.wait_ai(battle)
	next=step(battle,data,{"unit.a":F.order([4,0],"withdraw")},"实际绑定护送单位抵达")
	check(next.result.get("reason")=="EscortDelivered","护送依实际保护单位退出判定")
	battle=F.battle(data,"goal.escort"); battle.units["unit.a"].position=[0,0]; battle.units["unit.b"]=F.unit("unit.b",1,[2,1],"troop.foot",48)
	battle.units["unit.c"]=F.unit("unit.c",0,[1,1],"troop.foot",1); battle.objective_unit_ids=["unit.c"]
	data.troop_types["troop.foot"].accuracy=100; data.troop_types["troop.foot"].attack=100
	F.wait_ai(battle); battle.ai_plans["unit.b"]=F.order([2,1],"attack","unit.c","",3)
	next=step(battle,data,{"unit.a":F.order([0,0])},"保护对象失去作战人员")
	check(next.result.get("reason")=="ProtectedMilitaryUnitLost" and next.result.winner_side==1 and not next.units["unit.a"].fit.is_empty(),"主力仍在也不能覆盖护送失败")
	battle=F.battle(data,"goal.escort"); battle.units["unit.a"].position=[1,1]; battle.units["unit.b"]=F.unit("unit.b",1,[2,1],"troop.foot",1); F.wait_ai(battle)
	next=step(battle,data,{"unit.a":F.order([1,1],"attack","unit.b")},"护送中先击退敌军")
	check(next.phase=="planning" and next.units["unit.b"].fit.is_empty() and not next.units["unit.a"].withdrawn,"敌军消失不等于货队已经抵达出口")
	data=F.data(); battle=F.battle(data,"goal.cover"); battle.units["unit.a"].position=[3,0]; battle.units["unit.b"].position=[0,3]
	battle.units["unit.c"]=F.unit("unit.c",0,[3,3]); battle.command_scope="army"; battle.command_army_id="army.0"; battle.command_unit_ids=["unit.a","unit.c"]; F.wait_ai(battle)
	next=step(battle,data,{"unit.a":F.order([4,0],"withdraw"),"unit.c":F.order([3,3])},"保护对象先撤离")
	check(next.phase=="planning" and next.objective_state.escaped_unit_ids==["unit.a"],"掩护撤离还要求掩护部队有序退下")
	F.wait_ai(next); next=step(next,data,{"unit.c":F.order([4,3],"withdraw")},"掩护部队后撤")
	check(next.result.get("reason")=="RetreatCovered" and next.objective_state.escaped_unit_ids.size()==2,"不同出口可完成同一掩护撤离目标")
	battle=F.battle(data); battle.units["unit.a"].position=[4,3]; battle.units["unit.b"].position=[0,1]
	battle.ai_plans={"unit.b":F.order([0,0],"withdraw","","",3)}
	next=step(battle,data,{"unit.a":F.order([4,3])},"敌军按合法出口退兵")
	check(next.result.get("reason")=="EnemyMilitaryRepelled" and next.units["unit.b"].fit==battle.units["unit.b"].fit,"击退无需伪造敌军战死")
	data.troop_types["troop.foot"].accuracy=100; data.troop_types["troop.foot"].attack=100; data.troop_types["troop.foot"].defense=0
	battle=F.battle(data); battle.units["unit.a"].position=[1,1]; battle.units["unit.b"].position=[2,1]
	battle.units["unit.a"].morale=100; battle.units["unit.b"].morale=100
	battle.ai_plans={"unit.b":F.order([2,1],"attack","unit.a","",3)}
	next=step(battle,data,{"unit.a":F.order([1,1],"attack","unit.b")},"双方同时攻击失去全部可战人员")
	check(next.phase=="result" and next.result.winner_side==-1 and next.units["unit.a"].fit.is_empty() and next.units["unit.b"].fit.is_empty(),"同步伤害双方都生效，互溃不偏袒玩家")

## 两种兵种组合在不同地图上自主完成会战；双方合法 AI 与玩家同规则，每轮仍仅增加局部 round。
func check_autonomous() -> void:
	for field in ["field.open","field.ford"]:
		var data: Dictionary=F.data(); var battle: Dictionary=F.battle(data,"goal.repel",field)
		battle.controller_actor_id=""; battle.command_scope="army"; battle.command_unit_ids=[]; battle.command_army_id=""
		battle.units["unit.a"].troop_type_id="troop.horse" if field=="field.open" else "troop.foot"
		battle.units["unit.b"].troop_type_id="troop.bow"
		var first_rng: Dictionary=battle.rng.duplicate(true)
		var ai: Dictionary=Round.make_ai(battle,data)
		check(ai.ok and ai.orders.size()==2,"无人旁观时两侧仍生成真实合法 AI："+field)
		if not ai.ok: continue
		battle.ai_plans=ai.orders
		var rounds: int=0
		while battle.phase=="planning" and rounds<19:
			var current: int=battle.round
			var next: Dictionary=step(battle,data,{},"远方会战合法推进 "+field+" "+str(current))
			if next==battle: break
			check(next.phase=="result" or next.round==current+1,"一轮只增加一个会话 round")
			battle=next; rounds+=1
		check(battle.phase=="result" and rounds<=18,"远方会战按目标或期限结束："+field)
		check(battle.rng!=first_rng and battle.units["unit.a"].fit.size()+battle.units["unit.b"].fit.size()<48,"远方不是按总战力直接判胜，有真实命中与人员变化："+field)
		check(Round.validate(battle,data).ok and not battle.has("tick") and not battle.has("food"),"远方结果继续守恒且不私设世界时间和军粮账")

## 初期每侧五部的真实定义规模可直接复用，记录纯规划耗时但不以机器相关阈值掩盖规则失败。
func check_ten_units() -> void:
	var data: Dictionary=F.data(); data.battlefields["field.open"]=F.board("field.open",7,5)
	var battle: Dictionary=F.battle(data); battle.units={}
	for index in range(5):
		var left: String="unit.a"+str(index); var right: String="unit.b"+str(index)
		battle.units[left]=F.unit(left,0,[0,index],"troop.bow" if index%2 else "troop.foot")
		battle.units[right]=F.unit(right,1,[6,index],"troop.horse" if index%2 else "troop.foot")
	battle.controller_actor_id="actor.unit.a0"; battle.command_unit_ids=["unit.a0"]
	var started: int=Time.get_ticks_usec()
	var ai: Dictionary=Round.make_ai(battle,data)
	print("MILITARY_TEN_UNITS_AI_USEC=",Time.get_ticks_usec()-started)
	check(ai.ok and ai.orders.size()==9,"每侧五部时其余九部 AI 使用相同权限和命令规则")
	if not ai.ok: return
	battle.ai_plans=ai.orders
	var next: Dictionary=step(battle,data,{"unit.a0":F.order([1,0],"move")},"十部同场推进同一轮")
	check(next.round==2 and next.units.size()==10 and Round.validate(next,data,true).ok,"十部没有各自增加世界时间或丢失下一轮保存 AI")
