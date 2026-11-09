## 道路会面生命周期：绑定真实旅人和未完成行程，不发奖励或伪造战斗结果。
extends RefCounted
const Identity=preload("res://core/actor_identity.gd")
const Location=preload("res://core/world_location.gd")
const Factory=preload("res://core/world_factory.gd")
const Dialogue=preload("res://core/dialogue_runner.gd")
const KINDS=["encounter_talk","encounter_leave"]
## 查询人物参与的待处理会面，返回副本以免界面绕过命令改变占用。
static func pending_for(state: Dictionary, actor_id: String) -> Dictionary:
	for e in state.get("encounters",{}).values():
		if e.status=="pending" and actor_id in e.participants: return e.duplicate(true)
	return {}
## 行旅结算后在实际接触点挂起原行动；NPC会面自动礼让，使用同一记录和去重规则。
static func install(state: Dictionary, hit: Dictionary) -> void:
	var id: String=Factory.next_id(state,"encounter")
	var people: Array=[]; var actions: Dictionary={}; var resume: Dictionary={}
	for leader in [hit.a,hit.b]:
		people.append_array(Location.members(state,leader))
		actions[leader]=hit.traces[leader].action_id
		resume[leader]=hit.traces[leader].params.get("paused",false)
	var interactive: bool=state.player_actor_id in people
	var e: Dictionary={"id":id,"leaders":[hit.a,hit.b],"participants":people,"anchors":hit.anchors.duplicate(true),"action_ids":actions,"resume_paused":resume,"created_tick":state.tick+1,"resolved_tick":-1 if interactive else state.tick+1,"status":"pending" if interactive else "resolved","result_id":"" if interactive else Factory.next_id(state,"result"),"talked":false}
	for leader in e.leaders:
		var trace: Dictionary=hit.traces[leader]; var action: Dictionary=state.actions[trace.action_id]
		action.params=trace.params.duplicate(true)
		action.params.paused=true if interactive else resume[leader]
		action.params.encounter_id=id if interactive else ""
		var peer: String=hit.b if leader==hit.a else hit.a
		action.params.met_actions=action.params.get("met_actions",[]).duplicate()
		action.params.met_actions.append(actions[peer])
		state.actors[leader].location=hit.anchors[leader].duplicate(true)
	state.encounters[id]=e
	var resolved: Array=[]
	for key in state.encounters:
		if state.encounters[key].status=="resolved": resolved.append(key)
	resolved.sort_custom(func(a,b): return state.encounters[a].created_tick<state.encounters[b].created_tick if state.encounters[a].created_tick!=state.encounters[b].created_tick else a<b)
	while resolved.size()>128: state.encounters.erase(resolved.pop_front())
## 只接纳尚在同一锚点的会面参与者；不把任意岗位变成路上的远程服务。
static func preview(state: Dictionary, data: Dictionary, actor_id: String, kind: String, params: Dictionary) -> Dictionary:
	var id: Variant=params.get("encounter_id")
	if not id is String or not state.encounters.has(id): return {"ok":false,"code":"UnknownEncounter"}
	var e: Dictionary=state.encounters[id]
	if e.status!="pending": return {"ok":false,"code":"EncounterResolved"}
	if not actor_id in e.participants or not state.actors[actor_id].alive or not state.actors[actor_id].available: return {"ok":false,"code":"NotEncounterParticipant"}
	for leader in e.leaders:
		if state.actors[leader].location!=e.anchors[leader] or state.actors[leader].action_id!=e.action_ids[leader]: return {"ok":false,"code":"EncounterMoved"}
	if kind=="encounter_leave": return {"ok":true,"duration":0,"encounter_id":id}
	if state.interactions.has(actor_id): return {"ok":false,"code":"AlreadyTalking"}
	var provider: String=e.leaders[1] if actor_id==e.leaders[0] else e.leaders[0]
	var dialogue_id: Variant=Identity.definition(state,data,provider).get("road_dialogue_id")
	if not dialogue_id is String or not data.dialogues.has(dialogue_id): return {"ok":false,"code":"NoRoadDialogue"}
	var role_id: String=data.dialogues[dialogue_id].context_role
	return {"ok":true,"duration":0,"encounter_id":id,"dialogue_id":dialogue_id,"context":{"kind":"encounter","encounter_id":id,"place_id":"","provider_actor_id":provider,"role_id":role_id}}
## 交谈使用原对白会话；道别只恢复原行程及先前暂停状态，不重复任何资源结算。
static func apply(state: Dictionary, data: Dictionary, actor_id: String, kind: String, quote: Dictionary) -> Dictionary:
	var e: Dictionary=state.encounters[quote.encounter_id]
	if kind=="encounter_talk":
		e.talked=true
		Dialogue.begin(state,data,actor_id,quote)
		return {"ok":true}
	for leader in e.leaders:
		var action: Dictionary=state.actions[e.action_ids[leader]]
		action.params.encounter_id=""
		action.params.paused=e.resume_paused[leader]
	for person in state.interactions.keys():
		if state.interactions[person].context.get("encounter_id")==e.id: state.interactions.erase(person)
	e.status="resolved"; e.resolved_tick=state.tick; e.result_id=Factory.next_id(state,"result")
	return {"ok":true,"details":{"encounter_id":e.id,"settlement_result":e.result_id}}
