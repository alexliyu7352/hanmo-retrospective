## 普通场所与行旅右栏的有限用途输入；复用原选择规则，不授予交谈、移动或军事权限。
extends RefCounted
const Topics=preload("res://core/interaction_topics.gd")
const Dialogue=preload("res://core/dialogue_runner.gd")
const Identity=preload("res://core/actor_identity.gd")
const Location=preload("res://core/world_location.gd")
const Encounters=preload("res://core/encounter_rules.gd")
const Assets=preload("res://core/military_assets.gd")
const EstatePage=preload("res://core/views/estate_page.gd")

## 保留岗位与人物原排序、禁用主题、庄务插入序和本人待开事件；按钮仍由真实preview授权。
static func place(state: Dictionary, data: Dictionary) -> Dictionary:
	if state.is_empty(): return {"ok":false,"code":"NoWorld"}
	var actor: Dictionary=state.actors[state.player_actor_id]
	var result: Dictionary=_common(state)
	result.merge({"action_busy":false,"people":[],"estate_ids":[],"military_here":false,"operating_campaigns":[],"queued_event_ids":[]})
	var view: Dictionary=Dialogue.view(state,data,actor.id)
	if view.phase!="scene":
		result.dialogue_speaker=_speaker(state,data,view)
		return result
	result.action_busy=not actor.action_id.is_empty()
	var place_id: String=actor.location.get("place_id","")
	for group in Topics.query(state,data,actor.id,place_id):
		var person: Dictionary=state.actors[group.provider_actor_id]
		if person.id==actor.id or Location.resolve(state,person.id).get("place_id")!=place_id or not person.alive: continue
		var topics: Array=[]
		for topic in group.topics:
			topics.append({"role_id":topic.role_id,"available":topic.available,"code":topic.code})
		result.people.append({"actor_id":person.id,"display_name":_name(state,data,person.id),"topics":topics})
	for id in state.get("estates",{}):
		if EstatePage.available(state,data,id): result.estate_ids.append(id)
	for camp in data.get("garrisons",{}).values():
		if camp.place_id==place_id: result.military_here=true
	for estate in data.get("estates",{}).values():
		if estate.place_id==place_id and state.estates[estate.id].owner_actor_id==state.player_actor_id: result.military_here=true
	for campaign in state.get("campaigns",{}).values():
		if campaign.actor_id==state.player_actor_id and campaign.status!="closed": result.military_here=true
	for army in state.get("armies",{}).values():
		if army.commander_actor_id==state.player_actor_id and army.status=="active": result.military_here=true
	for campaign in state.get("campaigns",{}).values():
		if campaign.actor_id!=state.player_actor_id or campaign.status!="operating": continue
		var order: Dictionary=data.get("military_orders",{}).get(campaign.template_id,{})
		var target: Dictionary=state.get("strongholds",{}).get(order.get("target_stronghold_id",""),{})
		result.operating_campaigns.append({"campaign_id":campaign.id,"target_siege_id":target.get("siege_id","")})
	for id in state.events:
		var event: Dictionary=state.events[id]
		if event.actor_id==actor.id and event.status=="queued": result.queued_event_ids.append(event.event_id)
	return result

## 会面保持原早退，不读同行和军粮；否则只给原行程标量、同伴顺序和本人成军的同源报价。
static func journey(state: Dictionary, data: Dictionary) -> Dictionary:
	if state.is_empty(): return {"ok":false,"code":"NoWorld"}
	var actor: Dictionary=state.actors[state.player_actor_id]
	var result: Dictionary=_common(state)
	result.merge({"travel":{"active":false},"encounter":{},"party_present":false,"companions":[],"logistics_quote":{}})
	result.travel.active=not actor.action_id.is_empty() and state.actions[actor.action_id].kind=="travel"
	var encounter: Dictionary=Encounters.pending_for(state,actor.id)
	if not encounter.is_empty():
		var provider: String=encounter.leaders[1] if encounter.leaders[0]==actor.id else encounter.leaders[0]
		result.encounter={"encounter_id":encounter.id,"provider_actor_id":provider,"display_name":_name(state,data,provider)}
		var view: Dictionary=Dialogue.view(state,data,actor.id)
		if view.phase!="scene": result.dialogue_speaker=_speaker(state,data,view)
		return result
	var party_id: String=Location.party_id(state,actor.id)
	if not party_id.is_empty():
		result.party_present=true
		var party: Dictionary=state.parties[party_id]
		for member in party.members:
			if member==party.leader_id: continue
			result.companions.append({"actor_id":member,"display_name":_name(state,data,member),"cost_per_tick":party.terms[member].cost_per_tick})
	var army_id: String=Location.army_id(state,actor.id)
	var army: Dictionary=state.get("armies",{}).get(army_id,{})
	if army.get("commander_actor_id")==actor.id:
		var quote: Dictionary=Assets.supply_preview(state,data,army_id)
		if quote.ok:
			result.logistics_quote={"ok":true,"available":quote.available,"required":quote.required,"mouth_count":quote.mouth_count,"full_ticks":quote.full_ticks,"shortage":quote.shortage}
	if result.travel.active:
		var action: Dictionary=state.actions[actor.action_id]
		if actor.location.kind=="Wilderness":
			var destination: Variant=action.params.destination
			result.travel.paused=action.params.paused
			result.travel.remaining_ticks=action.remaining_ticks
			result.travel.destination=destination.duplicate(true) if destination is Array else destination
		else:
			result.travel.edge_id=actor.location.edge_id
			result.travel.progress=actor.location.progress
			result.travel.paused=action.params.get("paused",false)
	return result

## 普通外壳仅取原raw位置，不解析Following/InArmy，不提前读取对白、活动或其它页面。
static func raw_location(state: Dictionary) -> Dictionary:
	var actor: Dictionary=state.actors[state.player_actor_id]
	var location: Dictionary={"kind":actor.location.kind}
	if actor.location.has("place_id"): location.place_id=actor.location.place_id
	return location.duplicate(true)

## 只保路由需要的地点身份；这里不预读对白或未选中分支的来源。
static func _common(state: Dictionary) -> Dictionary:
	var actor: Dictionary=state.actors[state.player_actor_id]
	var location: Dictionary=raw_location(state)
	if actor.location.has("settlement_id"): location.settlement_id=actor.location.settlement_id
	return {"ok":true,"actor_id":actor.id,"revision":state.revision,"location":location,"dialogue_speaker":{}}

## 只在真实对白分支确定后取原说话者；普通地图不因公共字段提前查询互动。
static func _speaker(state: Dictionary, data: Dictionary, view: Dictionary) -> Dictionary:
	var speaker: String=view.get("speaker",view.context.provider_actor_id)
	return {"actor_id":speaker,"display_name":_name(state,data,speaker)}

## 精确保留原Main身份名称回退，包括合法空名；不借美术验证改变文本。
static func _name(state: Dictionary, data: Dictionary, actor_id: String) -> String:
	var record: Dictionary=Identity.definition(state,data,actor_id)
	return record.get("display_name",data.texts.get(record.get("name_key",record.get("title_key","")),record.get("id","")))
