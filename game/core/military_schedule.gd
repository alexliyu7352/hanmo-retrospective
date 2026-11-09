## 有明确作者军政计划的NPC通过普通命令办事；不补造军团、不越过岗位、不为玩家自动接主线。
extends RefCounted
const Behavior=preload("res://core/domains/identity/actor_behavior.gd")
const Location=preload("res://core/world_location.gd")
const Interaction=preload("res://core/interaction_rules.gd")
const Campaign=preload("res://core/campaign_rules.gd")
const Combat=preload("res://core/combat_session.gd")
const SiegeWorld=preload("res://core/siege_world.gd")

## 每位空闲NPC每半日最多提出一步，开始动作仍由Kernel正式预览和事务验证。
static func proposals(state: Dictionary, data: Dictionary) -> Array:
	var result: Array=[]; var ids: Array=Behavior.scheduled_ids(state,data,"military_plan")
	for id in ids:
		if id==state.player_actor_id: continue
		var actor: Dictionary=state.actors[id]
		if not actor.alive or not actor.available or not actor.action_id.is_empty() or Combat.occupied(state,id): continue
		var plan: Dictionary=Behavior.definition(state,data,id).military_plan
		var campaign: Dictionary={}
		for current in state.campaigns.values():
			if current.actor_id==id and current.template_id==plan.order_id: campaign=current
		if not campaign.is_empty() and campaign.status=="closed": continue
		var next: Dictionary=_next(state,data,id,plan,campaign)
		if not next.is_empty(): next.actor_id=id; result.append(next)
	return result

## 职位、当面军令与实际营地逐步衔接；没有就位或没有供给时只等待，不篡改地点或库存。
static func _next(state: Dictionary, data: Dictionary, actor_id: String, plan: Dictionary, campaign: Dictionary) -> Dictionary:
	if not campaign.is_empty():
		var siege_id: String=Campaign.pending_siege(state,campaign.army_id)
		if not siege_id.is_empty():
			# 本人仍在结果页时由本人确认，旁观敌将不能替玩家关闭整场围城。
			var player_siege: String=SiegeWorld.for_actor(state,state.player_actor_id)
			return {"kind":"siege_close","params":{"siege_id":siege_id}} if state.sieges[siege_id].phase=="result" and player_siege!=siege_id else {}
		if campaign.status=="returning": return _returning(state,data,actor_id,campaign)
		if campaign.status=="awaiting_report": return _reporting(state,data,actor_id,campaign)
		var authority: Dictionary=state.offices.get(campaign.office_id,{})
		if not authority.get("enabled",false) or authority.get("holder_actor_id")!=actor_id:
			return {"kind":"campaign_return","params":{"campaign_id":campaign.id}} if not campaign.army_id.is_empty() else {"kind":"campaign_cancel","params":{"campaign_id":campaign.id}}
	var office: Dictionary=state.offices[plan.office_id]; var definition: Dictionary=data.offices[plan.office_id]
	var camp: Dictionary=data.garrisons[definition.garrison_id]
	if not office.enabled:
		# 动态人物只能使用本人已取得的权源，绝不按原型计划自动受任。
		if not data.actors.has(actor_id) or not office.appointment_id.is_empty(): return {}
		var context: Dictionary=Interaction.for_role(state,data,definition.appointing_role_id)
		return at_or_walk(state,data,actor_id,context.place_id,"office_accept",{"office_id":plan.office_id,"context":context})
	if office.holder_actor_id!=actor_id: return {}
	var order: Dictionary=data.military_orders[plan.order_id]
	if campaign.is_empty():
		var context: Dictionary=Interaction.for_role(state,data,order.issuer_role_id)
		return at_or_walk(state,data,actor_id,context.place_id,"campaign_accept",{"template_id":plan.order_id,"context":context})
	match campaign.status:
		"accepted": return at_or_walk(state,data,actor_id,camp.place_id,"army_form",{"order_id":campaign.id,"unit_ids":plan.unit_ids.duplicate(),"food_quantity":plan.food_quantity,"money":plan.money})
		"mustered":
			var entry: String=data.settlements[data.places[camp.place_id].settlement_id].entry_place
			return at_or_walk(state,data,actor_id,entry,"campaign_depart",{"campaign_id":campaign.id})
		"operating":
			if not campaign.objective_result.is_empty(): return {"kind":"campaign_return","params":{"campaign_id":campaign.id}}
			if order.objective.kind=="deliver_supply": return {"kind":"campaign_deliver","params":{"campaign_id":campaign.id}}
			if order.objective.kind=="occupy": return {"kind":"siege_start","params":{"campaign_id":campaign.id}}
	return {}

## 失职返程仍逐步出城、地形行军、入营卸装；无可用友营只等待，不提出必然失败的重复假行程。
static func _returning(state: Dictionary, data: Dictionary, actor_id: String, campaign: Dictionary) -> Dictionary:
	var choice: Dictionary=Campaign.return_destination(state,data,campaign.id)
	if not choice.ok: return {}
	var camp: Dictionary=data.garrisons[choice.return_garrison_id]
	return at_or_walk(state,data,actor_id,camp.place_id,"army_return",{"army_id":campaign.army_id,"garrison_id":camp.id})

## 报告只能交给当前真实合法岗位，优先实际交接城的办理人；没有人可办时不冒用原敌营名义。
static func _reporting(state: Dictionary, data: Dictionary, actor_id: String, campaign: Dictionary) -> Dictionary:
	var roles: Array=Campaign.report_roles(state,data,campaign.id)
	if roles.is_empty(): return {}
	var role_id: String=roles[0]; var city: String=Location.resolve(state,actor_id).get("settlement_id","")
	for id in roles:
		if data.places[data.roles[id].place_id].settlement_id==city: role_id=id; break
	var context: Dictionary=Interaction.for_role(state,data,role_id)
	return at_or_walk(state,data,actor_id,context.place_id,"campaign_report",{"campaign_id":campaign.id,"context":context})

## 当面事务先要求沿同城通道真正到场；本次只给一步提案，不调用行为回调。
static func at_or_walk(state: Dictionary, data: Dictionary, actor_id: String, place_id: String, kind: String, params: Dictionary) -> Dictionary:
	if not data.places.has(place_id): return {}
	var location: Dictionary=Location.resolve(state,actor_id)
	if location.get("place_id")==place_id: return {"kind":kind,"params":params}
	var city: String=data.places[place_id].settlement_id
	if location.get("kind")=="Settlement" and location.get("settlement_id")==city: return walk(state,data,actor_id,place_id)
	if location.get("kind")=="Settlement" and location.get("place_id")!=data.settlements[location.settlement_id].entry_place: return walk(state,data,actor_id,data.settlements[location.settlement_id].entry_place)
	if location.get("kind") in ["Settlement","Wilderness"]: return {"kind":"travel","params":{"destination":city}}
	return {}

## 小型城内通道广度搜索复用作者passages，返回首段；无通道或跨城就不提出瞬移。
static func walk(state: Dictionary, data: Dictionary, actor_id: String, target: String) -> Dictionary:
	var start: String=Location.resolve(state,actor_id).get("place_id","")
	if start.is_empty() or start==target: return {}
	var pending: Array=[start]; var first: Dictionary={start:""}
	while not pending.is_empty():
		var here: String=pending.pop_front()
		for next in data.places[here].passages:
			if first.has(next): continue
			first[next]=next if here==start else first[here]
			if next==target: return {"kind":"move_local","params":{"place_id":first[next]}}
			pending.append(next)
	return {}
