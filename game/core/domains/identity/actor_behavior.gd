## 运行人物的只读行为配置与一次性行旅初始化；原型提供能力，永久本人来源才授予动态行程。
extends RefCounted
const Identity=preload("res://core/actor_identity.gd")
const MAX_TICK=2147483647
const KEYS=["travel_plan","military_plan","social"]

## 复用身份层的深副本；缺人、缺原型或错误身份返回空，不把运行姓名写回作者目录。
static func definition(state: Dictionary, data: Dictionary, actor_id: String) -> Dictionary:
	return Identity.definition(state,data,actor_id)

## 稳定列出有配置与来源的运行身份；不把忙碌、死亡或玩家控制混作删除持久进度的许可。
static func scheduled_ids(state: Dictionary, data: Dictionary, behavior_key: String) -> Array:
	var result: Array=[]
	if not behavior_key in KEYS: return result
	var ids: Array=state.get("actors",{}).keys(); ids.sort()
	for id in ids:
		if not id is String: continue
		var config: Variant=definition(state,data,id).get(behavior_key)
		if not config is Dictionary or config.is_empty(): continue
		if not data.get("actors",{}).has(id):
			if behavior_key=="travel_plan" and not _travel_enabled(state,id): continue
			if behavior_key=="military_plan" and not _military_authorized(state,id,config): continue
		result.append(id)
	return result

## 仅在人物创建的外层候选内调用；重放保留合法游标，坏进度失败，未授权不会补钱或补行程。
static func initialize_schedule(state: Dictionary, data: Dictionary, actor_id: String) -> Dictionary:
	if not state.get("itineraries") is Dictionary: return {"ok":false,"code":"MissingRoadState"}
	var allowed: bool=actor_id in scheduled_ids(state,data,"travel_plan")
	if not allowed:
		return {"ok":false,"code":"UnexpectedItinerary"} if state.itineraries.has(actor_id) else {"ok":true,"initialized":false}
	var plan: Dictionary=definition(state,data,actor_id).travel_plan
	if not plan.get("destinations") is Array or typeof(plan.get("start_tick"))!=TYPE_INT or plan.start_tick<0 or plan.start_tick>MAX_TICK: return {"ok":false,"code":"InvalidItinerary"}
	if state.itineraries.has(actor_id):
		if not valid_itinerary(state.itineraries[actor_id],plan): return {"ok":false,"code":"InvalidItinerary"}
		return {"ok":true,"initialized":false}
	var created: Variant=state.actors[actor_id].get("created_tick",0)
	if typeof(created)!=TYPE_INT or created<0 or created>state.tick: return {"ok":false,"code":"InvalidActorCreationTick"}
	var row: Dictionary={"index":0,"ready_tick":maxi(created,plan.start_tick),"travelling":false}
	if not valid_itinerary(row,plan): return {"ok":false,"code":"InvalidItinerary"}
	state.itineraries[actor_id]=row
	return {"ok":true,"initialized":true}

## 校验既有三项游标边界，不修复、不重置；允许保存死亡、在途或继任后的合法等待进度。
static func valid_itinerary(row: Variant, plan: Dictionary) -> bool:
	return row is Dictionary and typeof(row.get("index"))==TYPE_INT and row.index>=0 and row.index<plan.get("destinations",[]).size() and typeof(row.get("ready_tick"))==TYPE_INT and row.ready_tick>=0 and row.ready_tick<=MAX_TICK and row.get("travelling") is bool

## 授权只来自该真人已完成的收养与创建事实；未完工、失败、孤立出生或同原型身份不能借用。
static func _travel_enabled(state: Dictionary, actor_id: String) -> bool:
	var actor: Variant=state.get("actors",{}).get(actor_id)
	var creation: Variant=state.get("facts",{}).get(actor_id)
	if not actor is Dictionary or not creation is Dictionary or creation.get("kind")!="actor_creation" or creation.get("actor_id")!=actor_id: return false
	var source: Variant=creation.get("source")
	if not source is String or state.get("family",{}).get("children",{}).get(actor_id)!=source: return false
	var fact: Variant=state.get("facts",{}).get(source)
	if not fact is Dictionary or fact.get("kind")!="family_adoption" or fact.get("success")!=true or fact.get("child_actor_id")!=actor_id: return false
	var terms: Variant=fact.get("terms")
	if not terms is Dictionary or not terms.get("policy") is Dictionary or typeof(terms.policy.get("enable_travel_plan"))!=TYPE_BOOL or not terms.policy.enable_travel_plan: return false
	return creation.get("prototype_id")==actor.get("prototype_id") and terms.get("prototype_id")==actor.get("prototype_id") and creation.get("created_tick")==actor.get("created_tick") and fact.get("completed_tick")==actor.get("created_tick")

## 本人现任计划岗位或本人已有对应军令才可排程；失权军令仍进入正式取消、返程和回报预览。
static func _military_authorized(state: Dictionary, actor_id: String, plan: Dictionary) -> bool:
	var office: Variant=state.get("offices",{}).get(plan.get("office_id"))
	if office is Dictionary and office.get("enabled",false) and office.get("holder_actor_id")==actor_id: return true
	for campaign in state.get("campaigns",{}).values():
		if campaign is Dictionary and campaign.get("actor_id")==actor_id and campaign.get("template_id")==plan.get("order_id"): return true
	return false
