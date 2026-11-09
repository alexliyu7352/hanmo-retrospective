## 纯野战测试的有限作者定义与编制夹具；兵员只在测试建局时生成，正式规则只能转移已有分类。
extends RefCounted
const Round=preload("res://core/military_round.gd")

## 两个不同形状的轴向场地与三类兵种共用同一协议，不依赖正式人物或军事任务名称。
static func data() -> Dictionary:
	var first: Dictionary=board("field.open",5,4)
	var second: Dictionary=board("field.ford",4,3)
	for cell in second.cells:
		if cell.q==2 and cell.r!=1: cell.terrain="water"
		if cell.q==1 and cell.r==2: cell.terrain="forest"
	var infantry: Dictionary=troop("troop.foot","infantry",2,1,1)
	var archer: Dictionary=troop("troop.bow","archer",2,2,3)
	var cavalry: Dictionary=troop("troop.horse","cavalry",4,1,1)
	cavalry.movement_costs.forest=3; cavalry.movement_costs.hill=3
	infantry.tactics=[tactic("tactic.rally","rally"),tactic("tactic.charge","charge")]
	archer.tactics=[tactic("tactic.volley","volley"),tactic("tactic.rally","rally")]
	cavalry.tactics=[tactic("tactic.charge","charge")]
	return {"battlefields":{first.id:first,second.id:second},"troop_types":{infantry.id:infantry,archer.id:archer,cavalry.id:cavalry},"military_objectives":{
		"goal.repel":{"id":"goal.repel","kind":"repel","side":0,"max_rounds":18,"allow_withdrawal":true},
		"goal.hold":{"id":"goal.hold","kind":"hold","side":0,"max_rounds":8,"allow_withdrawal":true,"point":[1,1],"hold_rounds":2},
		"goal.break":{"id":"goal.break","kind":"breakthrough","side":0,"max_rounds":10,"allow_withdrawal":true,"required_units":1},
		"goal.escort":{"id":"goal.escort","kind":"escort","side":0,"max_rounds":10,"allow_withdrawal":true},
		"goal.cover":{"id":"goal.cover","kind":"cover_retreat","side":0,"max_rounds":10,"allow_withdrawal":true,"required_units":2}}}

## 构造非方格逻辑的有限轴向平行四边形，行列仅用于夹具枚举六角格。
static func board(id: String, columns: int, rows: int) -> Dictionary:
	var cells: Array=[]
	for q in range(columns):
		for r in range(rows): cells.append({"q":q,"r":r,"terrain":"plain"})
	return {"id":id,"cells":cells,"spawns":[{"side":0,"position":[0,1]},{"side":1,"position":[columns-1,1]}],"exits":[{"side":0,"position":[columns-1,0]},{"side":0,"position":[columns-1,rows-1]},{"side":1,"position":[0,0]}]}

## 兵种初值保持明显空间差异；分类、射程和地形成本由正式纯层读取。
static func troop(id: String, category: String, movement: int, inner: int, outer: int) -> Dictionary:
	return {"id":id,"category":category,"movement":movement,"minimum_range":inner,"maximum_range":outer,"attack":35,"defense":10,"accuracy":75,"tactic_capacity":8,"movement_costs":{"plain":1,"forest":2,"hill":2,"water":0,"wall":0},"tactics":[]}

## 三类有限战法只配置已实现效果；普通人物功法 ID 在此没有隐式授权。
static func tactic(id: String, kind: String) -> Dictionary:
	return {"id":id,"kind":kind,"point_cost":3,"power_bp":15000 if kind=="charge" else (12500 if kind=="volley" else 10000),"accuracy_bonus":15 if kind=="volley" else 0,"morale_effect":20 if kind=="rally" else 8,"move_bonus":2 if kind=="charge" else 0,"status_rounds":1 if kind=="charge" else 2}

## 为测试提供明确已有的真实 ID 编制，五类人员的并集从一开始便等于完整名册。
static func unit(id: String, side: int, position: Array, troop_id: String="troop.foot", count: int=24) -> Dictionary:
	var members: Array=[]
	for index in range(count): members.append(id+".soldier."+str(index).pad_zeros(3))
	return {"unit_id":id,"army_id":"army."+str(side),"commander_actor_id":"actor."+id,"troop_type_id":troop_id,"side":side,"position":position.duplicate(),"heading":0 if side==0 else 3,"morale":80,"tactic_points":8,"statuses":{},"member_ids":members.duplicate(),"fit":members,"wounded":[],"dead":[],"captured":[],"deserted":[],"withdrawn":false}

## 初建会话引用既有单位，不在 resolve 中补人；AI 在玩家提供任何命令前生成并保存。
static func battle(definitions: Dictionary, objective: String="goal.repel", field: String="field.open") -> Dictionary:
	var rng=RandomNumberGenerator.new(); rng.seed=17
	var result: Dictionary={"id":"battle.fixture","kind":"military","battlefield_id":field,"objective_id":objective,"phase":"planning","round":1,"controller_actor_id":"actor.unit.a","controller_side":0,"command_scope":"unit","command_army_id":"","command_unit_ids":["unit.a"],"objective_unit_ids":[],"units":{"unit.a":unit("unit.a",0,[0,1]),"unit.b":unit("unit.b",1,[3,1])},"ai_plans":{},"objective_state":{"hold_rounds":0,"escaped_unit_ids":[]},"rng":{"seed":str(rng.seed),"state":str(rng.state)},"last_events":[],"result":{}}
	if objective in ["goal.escort","goal.cover"]: result.objective_unit_ids=["unit.a"]
	var ai: Dictionary=Round.make_ai(result,definitions)
	if ai.ok: result.ai_plans=ai.orders
	return result

## 简短计划构造只服务测试，不绕过正式预览、权限或提交校验。
static func order(position: Array, action: String="defend", target: String="", tactic_id: String="", heading: int=0) -> Dictionary:
	return {"action":action,"destination":position.duplicate(),"heading":heading,"target_id":target,"tactic_id":tactic_id}

## 设置明确已保存的对手等待计划，便于单独检验一次命中或移动而非重新规划 AI。
static func wait_ai(battle: Dictionary) -> void:
	battle.ai_plans={}
	for id in battle.units:
		var current: Dictionary=battle.units[id]
		if not id in battle.command_unit_ids and not current.fit.is_empty() and not current.withdrawn:
			battle.ai_plans[id]=order(current.position,"wait","","",current.heading)
