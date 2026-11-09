## 军令页面的有限规则门；部队及编组来源明确取军事边界夹具，节点请求走原Siege，不冒充完整世界试玩。
extends SceneTree
const Page=preload("res://core/views/military_battle_page.gd")
const Combat=preload("res://core/military_combat.gd")
const Round=preload("res://core/military_round.gd")
const Siege=preload("res://core/siege_rules.gd")
const F=preload("res://tests/military_fixture.gd")
const CF=preload("res://tests/campaign_fixture.gd")
var checks: int=0
var failures: int=0

## 延后执行纯读页面门，既不装载GUI也不生成另一套正式内容。
func _init() -> void:
	call_deferred("run")

## 每个失败保留真实非零退出；夹具前置失败不被后续空数组比较掩盖。
func check(value: bool, label: String) -> bool:
	checks+=1
	if not value: failures+=1; printerr("FAIL: ",label)
	return value

## 用已存在的纯规则名册构造明确军事边界；原_contact必须接受编组、军令、位置及双方身份。
func fixture() -> Dictionary:
	var data: Dictionary=CF.data(); data.battlefields["field.open"].objective_id="goal.repel"
	var battle: Dictionary=F.battle(data); battle.source_id="contact.preview"
	var state: Dictionary={"tick":0,"revision":3,"player_actor_id":battle.controller_actor_id,"battles":{battle.id:battle},"military_units":{},"actors":{},"armies":{},"campaigns":{},"facts":{},"sieges":{},"next_instance_id":99,"rng":{"world":{"state":"123"}}}
	var source: Dictionary={"kind":"military_contact","tick":0,"battlefield_id":battle.battlefield_id,"unit_ids":battle.units.keys(),"side_by_army":{},"objective":{"id":battle.objective_id,"unit_ids":[]},"army_order_ids":{},"army_locations":{}}
	for id in battle.units:
		var unit: Dictionary=battle.units[id]; var army_id: String="army.a" if unit.side==0 else "army.b"
		unit.army_id=army_id; state.military_units[id]=unit.duplicate(true); state.actors[unit.commander_actor_id]={"id":unit.commander_actor_id}
		var order_id: String="campaign."+army_id; var formation_id: String="formation."+army_id
		state.armies[army_id]={"id":army_id,"order_id":order_id,"formation_source":formation_id,"organization_id":"org."+str(unit.side)}
		state.campaigns[order_id]={"id":order_id,"army_id":army_id}
		state.facts[formation_id]={"kind":"military_action","command":"army_form","phase":"completed","success":true,"terms":{"unit_ids":[id]},"result":{"army_id":army_id}}
		source.side_by_army[army_id]=unit.side; source.army_order_ids[army_id]=order_id; source.army_locations[army_id]={"kind":"Settlement","settlement_id":"city.b"}
	state.facts[battle.source_id]=source
	return {"data":data,"state":state,"battle":battle}

## 每次只读取真实薄页；完整草案、朝向和目标都由正式Round决定合法格。
func draft(f: Dictionary, selected: String, order: Dictionary) -> Dictionary:
	return Page.draft(f.state,f.data,f.battle.id,selected,{selected:order})

## 用正式preview逐格建立独立期望集，比较路径与费用而非只数格子。
func legal_cells(f: Dictionary, selected: String, order: Dictionary) -> Array:
	var result: Array=[]
	var sources: Dictionary=Combat._damage_sources(f.state,f.data,f.battle)
	if not check(sources.ok,"原军事来源校验成功 "+str(sources)): return result
	for cell in f.data.battlefields[f.battle.battlefield_id].cells:
		var candidate: Dictionary=order.duplicate(true); candidate.destination=[cell.q,cell.r]
		var quote: Dictionary=Round.preview(f.battle,f.data,selected,candidate,sources.factors)
		if quote.ok: result.append({"position":candidate.destination,"path":quote.path,"cost":quote.cost})
	return canonical_cells(result)

## 输出次序不是规则资格，比较副本保留源容器零写。
func canonical_cells(cells: Array) -> Array:
	var result: Array=cells.duplicate(true)
	result.sort_custom(func(a,b): return a.position[0]<b.position[0] if a.position[0]!=b.position[0] else a.position[1]<b.position[1])
	return result

## 原地、攻击与协防战法分别通过原点数和目标规则，不沿用普通移动圈。
func tactic_cells() -> void:
	var f: Dictionary=fixture(); var unit: Dictionary=f.battle.units["unit.a"]
	var order: Dictionary=F.order(unit.position,"tactic","","tactic.rally")
	check(canonical_cells(draft(f,"unit.a",order).movement)==legal_cells(f,"unit.a",order),"鼓舞只含正式原地合法格")
	unit.troop_type_id="troop.bow"; order=F.order(unit.position,"tactic","unit.b","tactic.volley")
	check(canonical_cells(draft(f,"unit.a",order).movement)==legal_cells(f,"unit.a",order),"齐射只含正式原地合法格")
	order.heading=3
	check(canonical_cells(draft(f,"unit.a",order).movement)==legal_cells(f,"unit.a",order),"齐射完整草案朝向参与落点筛选")
	unit.troop_type_id="troop.foot"; unit.tactic_points=0; order=F.order(unit.position,"tactic","","tactic.rally")
	check(draft(f,"unit.a",order).movement.is_empty(),"点数不足不可残留普通移动圈")
	unit.tactic_points=8; order.tactic_id="missing"
	check(draft(f,"unit.a",order).movement.is_empty(),"未知战法不可残留普通移动圈")
	order=F.order([1,1],"tactic","unit.b","tactic.charge")
	check(canonical_cells(draft(f,"unit.a",order).movement)==legal_cells(f,"unit.a",order),"突击圈逐格保完整目标与必须移动条件")
	order.target_id="absent"
	check(draft(f,"unit.a",order).movement.is_empty(),"失效目标不保留突击移动圈")
	# 支援目标来自同侧新部队，仍只读正式Support与Round；来源补在明确夹具边界。
	f=fixture(); f.battle.units["unit.c"]=F.unit("unit.c",0,[1,1]); f.battle.units["unit.c"].army_id="army.a"
	f.state.military_units["unit.c"]=f.battle.units["unit.c"].duplicate(true); f.state.actors["actor.unit.c"]={"id":"actor.unit.c"}
	f.state.facts["formation.army.a"].terms.unit_ids.append("unit.c"); f.state.facts[f.battle.source_id].unit_ids.append("unit.c")
	f.data.troop_types["troop.foot"].tactics.append({"id":"tactic.support","kind":"support","point_cost":3,"power_bp":10000,"accuracy_bonus":0,"morale_effect":0,"move_bonus":0,"status_rounds":1})
	order=F.order([0,1],"tactic","unit.c","tactic.support")
	var page: Dictionary=draft(f,"unit.a",order)
	check(canonical_cells(page.movement)==legal_cells(f,"unit.a",order) and page.supportable==["unit.c"],"协防原地与真实友军目标同源")
	order.target_id="unit.b"
	check(draft(f,"unit.a",order).movement.is_empty(),"协防非法当前目标不留下可提交格")
	for row in [["troop.horse","flank",12500],["troop.bow","suppress",8000]]:
		f=fixture(); unit=f.battle.units["unit.a"]; unit.troop_type_id=row[0]
		if row[1]=="flank": f.battle.units["unit.b"].heading=0
		f.data.troop_types[row[0]].tactics=[{"id":"tactic."+row[1],"kind":row[1],"point_cost":3,"power_bp":row[2],"accuracy_bonus":0,"morale_effect":0,"move_bonus":0,"status_rounds":1}]
		order=F.order(unit.position,"tactic","unit.b","tactic."+row[1])
		var expected: Array=legal_cells(f,"unit.a",order)
		check(not expected.is_empty() and canonical_cells(draft(f,"unit.a",order).movement)==expected,"侧击/压制完整目标落点与路径沿原规则："+row[1])

## 相邻普通操作保现有DTO与移动圈，清空或切换草案不保存上一战法的报价。
func ordinary_and_identity() -> void:
	var f: Dictionary=fixture(); var before: Dictionary=f.duplicate(true)
	var order: Dictionary=F.order([1,1],"move")
	var page: Dictionary=draft(f,"unit.a",order)
	check(page.movement==Round.movement_options(f.battle,f.data,"unit.a").cells,"普通移动仍保原同源圈和次序")
	check(page.selected_quote==Round.preview(f.battle,f.data,"unit.a",order),"无墙普通报价不变")
	check(page.keys()==["ok","identity","orders_validation","selected_quote","movement","attackable","supportable"] and page.identity=={"actor_id":f.state.player_actor_id,"revision":3,"battle_id":f.battle.id,"round":1,"phase":"planning"},"既有DTO字段和读取身份不扩展")
	page=draft(f,"unit.a",F.order([0,1],"tactic","","tactic.rally"))
	var cleared: Dictionary=Page.draft(f.state,f.data,f.battle.id,"unit.a",{})
	check(page.movement.size()==1 and cleared.selected_quote.is_empty() and cleared.movement==Round.movement_options(f.battle,f.data,"unit.a").cells,"清空草案不沿用鼓舞原地圈或报价")
	check(f==before,"普通/战法/清空连续查询保持全部输入零写")
	f.state.player_actor_id="other"
	check(draft(f,"unit.a",order)=={"ok":false,"code":"MilitaryBattleUnavailable"},"玩家切换仍由原页面边界拒绝")

## 佯退方向按轮初朝向并免ZOC，急行预算和失效点数均取已有规则。
func movement_tactics() -> void:
	var f: Dictionary=fixture(); f.data.battlefields["field.open"]=F.board("field.open",9,5); f.data.battlefields["field.open"].objective_id="goal.repel"
	var troop: Dictionary=f.data.troop_types["troop.foot"]; troop.movement=3; troop.tactic_capacity=60
	troop.tactics=[{"id":"tactic.feint","kind":"feint","point_cost":20,"power_bp":10000,"accuracy_bonus":0,"morale_effect":0,"move_bonus":0,"status_rounds":1},{"id":"tactic.march","kind":"march","point_cost":15,"power_bp":10000,"accuracy_bonus":0,"morale_effect":0,"move_bonus":2,"status_rounds":1}]
	f.battle.units["unit.a"].position=[5,2]; f.battle.units["unit.a"].tactic_points=60; f.battle.units["unit.b"].position=[4,1]
	var order: Dictionary=F.order([2,2],"tactic","","tactic.feint",3)
	var page: Dictionary=draft(f,"unit.a",order)
	check(page.selected_quote.ok and page.selected_quote.incoming_damage_bp==11000,"佯退报价沿用当轮110%风险")
	check(canonical_cells(page.movement)==legal_cells(f,"unit.a",order),"佯退圈只留轮初后方且可以穿过控制区")
	check(page.movement.any(func(cell): return cell.position==[2,2] and cell.cost==3),"佯退合法后撤保实际路径费用")
	f.battle.units["unit.b"].position=[8,4]; f.battle.units["unit.a"].position=[0,1]; order=F.order([5,1],"tactic","","tactic.march")
	check(canonical_cells(draft(f,"unit.a",order).movement)==legal_cells(f,"unit.a",order),"急行独取已有加二预算")
	f.battle.units["unit.a"].statuses.hindered=1
	page=draft(f,"unit.a",order)
	check(canonical_cells(page.movement)==legal_cells(f,"unit.a",order) and not page.movement.any(func(cell): return cell.position==[5,1]),"阻行仍扣真实急行预算")
	f.battle.units["unit.a"].tactic_points=14
	check(draft(f,"unit.a",order).movement.is_empty(),"急行点数不足不残留普通圈")

## 原Siege产生当前pending节点；编组、永久contact字典为声明边界，墙伤害只与原Combat/Round比较。
func wall_quote() -> void:
	var f: Dictionary=fixture(); var obs: Dictionary=CF.observation(); obs.controller_actor_id=f.state.player_actor_id
	obs.units={"unit.a":obs.units.a1,"unit.b":obs.units.b1}; obs.control_sets={f.state.player_actor_id:["unit.a"]}
	f.data.siege_topologies["topology.one"].defender_entry_id="G"
	var made: Dictionary=Siege.create(f.data,CF.siege_params(),obs)
	if not check(made.ok,"墙例原Siege.create成功 "+str(made.get("code",""))): return
	made.siege.ai_plans={"unit.b":{"action":"wait","node_id":"G"}}
	var resolved: Dictionary=Siege.resolve(made.siege,f.data,obs,f.state.player_actor_id,{"unit.a":{"action":"move","node_id":"G"}},1)
	if not check(resolved.ok and resolved.battle_requests.size()==1,"墙例原Siege.resolve产生接敌 "+str(resolved.get("code",""))): return
	var request: Dictionary=resolved.battle_requests[0]; var source: Dictionary=f.state.facts[f.battle.source_id]
	f.state.facts.erase(f.battle.source_id); f.battle.source_id=request.source_id; f.state.facts[request.source_id]=source
	source.siege_id=resolved.siege.id; source.node_id=request.node_id; f.state.sieges[resolved.siege.id]=resolved.siege
	f.battle.controller_actor_id="actor.unit.b"; f.battle.controller_side=1; f.battle.command_unit_ids=["unit.b"]; f.state.player_actor_id="actor.unit.b"
	f.battle.units["unit.b"].troop_type_id="troop.bow"; f.battle.units["unit.b"].heading=3
	f.data.troop_types["troop.bow"].attack=100
	f.data.battlefields["field.open"].damage_model={"kind":"legacy_base_factors_v1","side_bp":11000,"rear_bp":12000,"forest_ranged_bp":8000,"hill_melee_bp":9000,"min_total_bp":4000,"max_total_bp":18000}
	var sources: Dictionary=Combat._damage_sources(f.state,f.data,f.battle)
	if not check(sources.ok and sources.factors.has("unit.b"),"墙例完整军事来源接受且只守方取得墙因子 "+str(sources)): return
	var order: Dictionary=F.order([3,1],"attack","unit.a","",3)
	var expected: Dictionary=Round.preview(f.battle,f.data,"unit.b",order,sources.factors)
	var neutral: Dictionary=Round.preview(f.battle,f.data,"unit.b",order)
	if not check(expected.ok and neutral.ok and expected.casualties_max>neutral.casualties_max,"原规则墙伤害确实不同于无源报价"): return
	var before: Dictionary=f.duplicate(true); seed(617); var expected_random: int=randi(); seed(617)
	var page: Dictionary=draft(f,"unit.b",order)
	check(page.selected_quote==expected,"守方页面报价与实际Round墙因子逐字段一致")
	check(f==before and randi()==expected_random,"页面查询整态、源、ID和两类RNG零写")
	page.selected_quote.path[0][0]=999; page.movement.clear()
	check(f==before,"返回报价及圈独立副本")
	f.state.sieges[resolved.siege.id].node_hp.W=0
	check(draft(f,"unit.b",order).selected_quote==neutral,"墙破后当前报价立即回中性，不缓存旧墙")
	f.state.facts[request.source_id].army_order_ids["army.a"]="missing"
	check(draft(f,"unit.b",order)==Combat._damage_sources(f.state,f.data,f.battle),"坏来源整份draft透传原错误，不伪装中性墙")

## 一个有界脚本保留原红与修后绿，不重跑本人档案、GUI或全量旧矩阵。
func run() -> void:
	tactic_cells(); ordinary_and_identity(); movement_tactics(); wall_quote()
	print("MILITARY_BATTLE_DRAFT_RULES_CHECKS=",checks," FAILURES=",failures)
	quit(0 if failures==0 else 1)
