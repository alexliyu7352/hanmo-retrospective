## 有限野战输入专测复用原五部夹具；只提供原纯规则来源，不伪造世界验收。
extends RefCounted
const F=preload("res://tests/military_fixture.gd")
## 定义真实兵种差异与两军五部，用既有纯层夹具名册，不修改正式作者包。
static func values() -> Dictionary:
	var data: Dictionary=F.data(); data.texts={"field":"白石谷","foot":"步卒","bow":"弓手","horse":"骑兵"}; data.actors={}
	data.battlefields["field.open"]=F.board("field.open",7,5); data.battlefields["field.open"].visual_tag="battlefield_valley"; data.battlefields["field.open"].name_key="field"
	for cell in data.battlefields["field.open"].cells:
		if [cell.q,cell.r] in [[2,3],[3,3]]: cell.terrain="forest"
		if [cell.q,cell.r]==[4,0]: cell.terrain="hill"
		if [cell.q,cell.r]==[5,3]: cell.terrain="water"
		if [cell.q,cell.r]==[5,4]: cell.terrain="wall"
	for row in [["troop.foot","foot","troop_infantry"],["troop.bow","bow","troop_archers"],["troop.horse","horse","troop_cavalry"]]:
		data.troop_types[row[0]].name_key=row[1]; data.troop_types[row[0]].battle_portrait_kind=row[2]
	var battle: Dictionary=F.battle(data); battle.units={"unit.a":F.unit("unit.a",0,[1,1]),"unit.c":F.unit("unit.c",0,[1,3],"troop.horse"),"unit.g":F.unit("unit.g",0,[0,2],"troop.bow"),"unit.b":F.unit("unit.b",1,[4,1]),"unit.d":F.unit("unit.d",1,[5,2],"troop.bow")}
	var actors: Dictionary={}
	for row in [["unit.a","行旅人"],["unit.c","韩明"],["unit.g","王朴"],["unit.b","赵进"],["unit.d","孙节"]]:
		var id: String="actor."+row[0]; data.actors[id]={"id":id,"name_key":id,"portrait_kind":"guard"}; data.texts[id]=row[1]; actors[id]={"id":id,"hp":100,"wallet_id":"wallet.p"}
	F.wait_ai(battle)
	return {"data":data,"state":{"player_actor_id":"actor.unit.a","actors":actors,"wallets":{"wallet.p":{"id":"wallet.p","owner":{"kind":"actor","id":"actor.unit.a"},"balance":800}},"tick":2,"revision":1,"battles":{battle.id:battle}}}


## 独立作者目录只经真实任官、受令、领装、行旅与接敌建立完整世界，不直接拼会话。
static func open_battle(kernel) -> Dictionary:
	var manifest: Dictionary=JSON.parse_string(FileAccess.get_file_as_string("res://data/military-march-fixture.json"))
	var opened: Dictionary=kernel.new_game("base::scenario.march","res://data/generated/"+manifest.directory+"/catalog.hmcatalog.json")
	if not opened.ok: return opened
	var armies: Dictionary={}
	for label in ["a","b"]:
		var actor: String="base::actor.military."+label+".commander"
		var context: Dictionary=load("res://core/interaction_rules.gd").for_role(kernel.snapshot(),kernel.content().data,"base::role.military."+label)
		var steps: Array=[["office_accept",{"office_id":"base::office.military."+label,"context":context},actor],["advance",{},"base::actor.military.a.commander"]]
		for step in steps:
			var result: Dictionary=submit(kernel,step[0],step[1],step[2])
			if not result.ok: return result
		context=load("res://core/interaction_rules.gd").for_role(kernel.snapshot(),kernel.content().data,"base::role.military."+label)
		var accepted: Dictionary=submit(kernel,"campaign_accept",{"template_id":"base::order.military."+label,"context":context},actor)
		if not accepted.ok: return accepted
		var campaign_id: String=accepted.details.campaign_id
		var unit_ids: Array=["base::unit.military."+label]
		if label=="a": unit_ids.append("base::unit.military.a.cap")
		var formed: Dictionary=submit(kernel,"army_form",{"order_id":campaign_id,"unit_ids":unit_ids,"food_quantity":500,"money":500},actor)
		if not formed.ok: return formed
		var advanced: Dictionary=submit(kernel,"advance")
		if not advanced.ok: return advanced
		armies[label]=kernel.snapshot().campaigns[campaign_id].army_id
	for row in [["base::actor.military.b.commander",[430,300]],["base::actor.military.a.commander",[400,300]]]:
		var travel: Dictionary=submit(kernel,"travel",{"destination":row[1]},row[0])
		if not travel.ok: return travel
		var done: bool=false
		for _step in range(80):
			if kernel.snapshot().actors[row[0]].action_id.is_empty(): done=true; break
			var advanced: Dictionary=submit(kernel,"advance")
			if not advanced.ok: return advanced
		if not done: return {"ok":false,"code":"FixtureTravelNotCompleted"}
	var departed: Dictionary=submit(kernel,"campaign_depart",{"campaign_id":kernel.snapshot().armies[armies.a].order_id})
	if not departed.ok: return departed
	var contact: Dictionary=submit(kernel,"advance")
	if not contact.ok: return contact
	var page: Dictionary=kernel.military_battle_page()
	if not page.get("active",false): return {"ok":false,"code":"FixtureBattleNotCreated"}
	return {"ok":true,"battle_id":page.identity.battle_id,"armies":armies}

## 测试命令仍读取实际版本和序号，原失败原样返回，不替换Kernel验证器。
static func submit(kernel, kind: String, params: Dictionary={}, actor: String="base::actor.military.a.commander") -> Dictionary:
	var state: Dictionary=kernel.snapshot()
	return kernel.submit({"kind":kind,"params":params,"actor_id":actor,"expected_revision":state.revision,"request_seq":state.request_sequences[actor]+1})
