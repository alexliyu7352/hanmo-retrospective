## 空闲旅人的有限行程提案；只沿作者指定据点往返，不生成钱粮或绕过世界命令。
extends RefCounted
const Behavior=preload("res://core/domains/identity/actor_behavior.gd")
const Location=preload("res://core/world_location.gd")
## 在半日结束时安排下一次出发；提案最早下一半日取得进度，不瞬移补走。
static func proposals(state: Dictionary, data: Dictionary) -> Array:
	var proposals: Array=[]
	var ids: Array=Behavior.scheduled_ids(state,data,"travel_plan")
	for id in ids:
		if id==state.player_actor_id or not state.itineraries.has(id): continue
		var actor: Dictionary=state.actors[id]; var plan: Dictionary=Behavior.definition(state,data,id).travel_plan
		var itinerary: Dictionary=state.itineraries[id]
		if not actor.action_id.is_empty() or itinerary.ready_tick>state.tick or not Location.party_id(state,id).is_empty(): continue
		if not actor.alive or not actor.available or actor.location.kind!="Settlement": continue
		if actor.location.place_id!=data.settlements[actor.location.settlement_id].entry_place: continue
		var talking: bool=state.interactions.has(id)
		for session in state.interactions.values():
			if session.context.provider_actor_id==id: talking=true
		if talking: continue
		var index: int=itinerary.index
		if plan.destinations[index]==actor.location.settlement_id: index=(index+1)%plan.destinations.size()
		if plan.destinations[index]==actor.location.settlement_id: continue
		proposals.append({"actor_id":id,"kind":"travel","params":{"destination":plan.destinations[index]},"next_index":(index+1)%plan.destinations.size()})
	return proposals

## 在世界提交候选中记录已抵达旅人的休整时间，与只读提案查询分开。
static func finish_arrivals(state: Dictionary, data: Dictionary) -> void:
	for id in Behavior.scheduled_ids(state,data,"travel_plan"):
		if not state.itineraries.has(id): continue
		var itinerary: Dictionary=state.itineraries[id]
		if itinerary.travelling and state.actors[id].action_id.is_empty():
			itinerary.travelling=false
			itinerary.ready_tick=state.tick+Behavior.definition(state,data,id).travel_plan.dwell_ticks
