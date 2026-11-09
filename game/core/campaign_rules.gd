## 战略军令只管理已授权目标与交接；位置、兵员、军粮归军团与世界半日协调器，所有写入仅作用于私有候选。
extends RefCounted
const Interaction=preload("res://core/interaction_rules.gd")
const Inventory=preload("res://core/inventory_rules.gd")
const Ownership=preload("res://core/military_ownership.gd")
const Terrain=preload("res://core/world_terrain.gd")
const Graph=preload("res://core/route_graph.gd")
const Factory=preload("res://core/world_factory.gd")
const KINDS=["campaign_accept","campaign_depart","campaign_deliver","campaign_return","campaign_report","campaign_cancel"]
const STATUSES=["accepted","mustered","marching","operating","returning","awaiting_report","closed"]

## 新世界只初始化军令实例集合，不生成组织、军队或兵粮；旧内容缺定义仍可保留空集合。
static func initialize(state: Dictionary, _data: Dictionary) -> Dictionary:
	if not state.has("campaigns"): state.campaigns={}
	return {"ok":true}

## 核验模板的跨表来源与目标，不把缺失据点、驻军、公职或未知目标默认为已经达成。
static func validate_definitions(data: Dictionary) -> Dictionary:
	for id in data.get("military_orders",{}):
		var definition: Variant=data.military_orders[id]
		if not definition is Dictionary or definition.get("id")!=id: return _fail("InvalidMilitaryOrderDefinition",str(id))
		for pair in [["office_id","offices"],["garrison_id","garrisons"],["issuer_role_id","roles"],["report_role_id","roles"],["target_stronghold_id","strongholds"]]:
			if not definition.get(pair[0]) is String or not data.get(pair[1],{}).has(definition[pair[0]]): return _fail("UnknownCampaignReference",str(id)+"/"+pair[0])
		var office: Dictionary=data.offices[definition.office_id]; var garrison: Dictionary=data.garrisons[definition.garrison_id]
		var target: Dictionary=data.strongholds[definition.target_stronghold_id]
		if not data.get("places",{}).has(garrison.get("place_id")) or not data.get("settlements",{}).has(target.get("settlement_id")) or not data.garrisons.has(target.get("garrison_id")): return _fail("UnknownCampaignDestination",id)
		if office.get("garrison_id")!=definition.garrison_id or office.get("organization_id")!=garrison.get("organization_id"): return _fail("CampaignOrganizationMismatch",id)
		for role_id in [definition.issuer_role_id,definition.report_role_id]:
			if data.roles[role_id].get("organization_id")!=office.organization_id: return _fail("CampaignRoleOrganizationMismatch",id)
		var objective: Variant=definition.get("objective")
		if not objective is Dictionary or not objective.get("kind") in ["occupy","deliver_supply","relieve"]: return _fail("UnknownCampaignObjective",id)
		if not _whole(definition.get("deadline_ticks"),1,1000000): return _fail("InvalidCampaignDeadline",id)
		if objective.kind=="deliver_supply" and (not data.get("items",{}).has(objective.get("item_id")) or not _whole(objective.get("quantity"),1,1000000)): return _fail("InvalidCampaignSupply",id)
		if objective.kind=="deliver_supply":
			if not objective.get("recipient","garrison") in ["garrison","army"]: return _fail("UnknownCampaignRecipient",id)
			if objective.has("recipient_organization_id") and not data.get("organizations",{}).has(objective.recipient_organization_id): return _fail("UnknownCampaignRecipientOrganization",id)
	return {"ok":true}

## 实例只引用真实军团与已保存结果；半日游标、唯一军令绑定和终态收据须能完整恢复。
static func validate_state(state: Dictionary, data: Dictionary) -> Dictionary:
	var checked: Dictionary=validate_definitions(data)
	if not checked.ok: return checked
	if not state.get("campaigns",{}) is Dictionary: return _fail("InvalidCampaigns")
	var armies: Array=[]
	for id in state.get("campaigns",{}):
		var campaign: Variant=state.campaigns[id]
		if not campaign is Dictionary or campaign.get("id")!=id or not data.get("military_orders",{}).has(campaign.get("template_id")): return _fail("InvalidCampaign",str(id))
		var definition: Dictionary=data.military_orders[campaign.template_id]
		if not campaign.get("return_garrison_id","") is String or not campaign.get("return_wait_reason","") is String: return _fail("InvalidCampaignReturnDestination",id)
		if not campaign.get("return_garrison_id","").is_empty() and not data.garrisons.has(campaign.return_garrison_id): return _fail("InvalidCampaignReturnDestination",id)
		if not state.actors.has(campaign.get("actor_id")) or not campaign.get("status") in STATUSES or not campaign.get("army_id") is String: return _fail("InvalidCampaignIdentity",id)
		for field in ["office_id","garrison_id","issuer_role_id"]:
			if campaign.get(field)!=definition[field]: return _fail("CampaignDefinitionMismatch",id+"/"+field)
		if campaign.get("issuer_organization_id")!=data.offices[definition.office_id].organization_id: return _fail("CampaignOrganizationMismatch",id)
		if not _whole(campaign.get("accepted_tick"),0,state.tick) or not _whole(campaign.get("deadline_tick"),campaign.accepted_tick+1,2147483647) or not _whole(campaign.get("last_tick"),campaign.accepted_tick,state.tick): return _fail("InvalidCampaignTime",id)
		if campaign.deadline_tick!=campaign.accepted_tick+definition.deadline_ticks: return _fail("CampaignDeadlineMismatch",id)
		if not campaign.get("objective_result") is Dictionary or not campaign.get("settlement_id") is String or not campaign.get("arrival_fact_id") is String or not campaign.get("return_receipt_id") is String or not campaign.get("recipient_army_id") is String: return _fail("InvalidCampaignResult",id)
		if definition.objective.get("recipient","garrison")=="army" and not state.get("armies",{}).has(campaign.recipient_army_id): return _fail("CampaignRecipientArmyMissing",id)
		if not campaign.army_id.is_empty():
			var army: Dictionary=state.get("armies",{}).get(campaign.army_id,{})
			if army.is_empty() or army.get("order_id")!=id or army.get("organization_id")!=campaign.issuer_organization_id or campaign.army_id in armies: return _fail("CampaignArmyMismatch",id)
			armies.append(campaign.army_id)
		elif not campaign.status in ["accepted","closed"]: return _fail("CampaignArmyMissing",id)
		if campaign.status=="closed" and not _fact_matches(state,campaign.settlement_id,"campaign_settlement",campaign,state.tick): return _fail("CampaignSettlementMissing",id)
		if campaign.status in ["awaiting_report","closed"] and not campaign.army_id.is_empty():
			var receipt: Dictionary=state.facts.get(campaign.return_receipt_id,{})
			if receipt.get("kind")!="military_return" or receipt.get("order_id")!=id or receipt.get("army_id")!=campaign.army_id or _army(state,campaign).get("status")!="closed": return _fail("CampaignReturnReceiptMismatch",id)
			var settled: Dictionary=_return_receipt(state,data,campaign,campaign.return_receipt_id)
			if not settled.ok: return settled
			if campaign.get("return_garrison_id",receipt.garrison_id)!=receipt.garrison_id: return _fail("CampaignReturnReceiptMismatch",id)
		if not campaign.arrival_fact_id.is_empty() and (not _fact_matches(state,campaign.arrival_fact_id,"campaign_arrival",campaign,state.tick) or state.facts[campaign.arrival_fact_id].get("stronghold_id")!=definition.target_stronghold_id): return _fail("CampaignArrivalMissing",id)
		if not campaign.objective_result.is_empty():
			var outcome: Dictionary=campaign.objective_result
			if not outcome.get("kind") in ["supply_delivered","occupied","relieved","cancelled","withdrawn","authority_lost","expired"] or not _whole(outcome.get("tick"),campaign.accepted_tick,state.tick): return _fail("InvalidCampaignResult",id)
			if outcome.kind in ["supply_delivered","occupied","relieved"]:
				if not _fact_matches(state,outcome.get("source_id"),{"supply_delivered":"military_delivery","occupied":"military_occupation","relieved":"military_relief"}[outcome.kind],campaign,outcome.tick): return _fail("CampaignResultSourceMissing",id)
				var source: Dictionary=state.facts[outcome.source_id]
				if source.get("stronghold_id")!=definition.target_stronghold_id or (outcome.kind!="supply_delivered" and source.get("phase")!="completed"): return _fail("CampaignResultSourceMismatch",id)
	return {"ok":true}

## 当面受令与回报使用正式岗位上下文，出征和返程只给行军请求；预览不移动军团或预扣物资。
static func preview(state: Dictionary, data: Dictionary, actor_id: String, kind: String, params: Dictionary) -> Dictionary:
	var checked: Dictionary=validate_definitions(data)
	if not checked.ok: return checked
	if not kind in KINDS or not state.actors.has(actor_id): return _fail("UnknownCampaignCommand")
	if kind=="campaign_accept":
		var definition: Dictionary=data.get("military_orders",{}).get(params.get("template_id"),{})
		if definition.is_empty(): return _fail("UnknownMilitaryOrder")
		var office: Dictionary=state.get("offices",{}).get(definition.office_id,{})
		if office.get("holder_actor_id")!=actor_id or not office.get("enabled",false): return _fail("CampaignOfficeRequired")
		var context: Dictionary=Interaction.validate(state,data,actor_id,params.get("context"),"military_order")
		if not context.ok: return context
		if context.context.role_id!=definition.issuer_role_id: return _fail("CampaignIssuerRequired")
		for current in state.get("campaigns",{}).values():
			if current.actor_id==actor_id and current.status!="closed": return _fail("CampaignAlreadyActive")
		var recipient: String=""
		if definition.objective.kind=="deliver_supply" and definition.objective.get("recipient","garrison")=="army":
			if not params.get("recipient_army_id") is String: return _fail("CampaignRecipientArmyRequired")
			recipient=params.recipient_army_id; var army: Dictionary=state.get("armies",{}).get(recipient,{})
			if army.get("status")!="active" or army.get("organization_id")!=definition.objective.get("recipient_organization_id",data.offices[definition.office_id].organization_id): return _fail("CampaignRecipientArmyUnauthorized")
		return {"ok":true,"template_id":definition.id,"context":context.context,"recipient_army_id":recipient}
	var campaign: Dictionary=state.get("campaigns",{}).get(params.get("campaign_id"),{})
	if campaign.is_empty() or campaign.actor_id!=actor_id: return _fail("CampaignNotAssigned")
	if campaign.status=="closed": return _fail("CampaignClosed")
	if kind in ["campaign_return","campaign_cancel"] and not campaign.army_id.is_empty():
		var siege_id: String=pending_siege(state,campaign.army_id)
		# 围城终局可先登记返程；结果尚未确认时 _return_request 仍留空，不能提前穿越军议占用。
		if not siege_id.is_empty() and state.sieges[siege_id].phase=="active": return _fail("SiegePending")
		if not params.get("garrison_id","") is String: return _fail("CampaignFriendlyGarrisonRequired")
		if not params.get("garrison_id","").is_empty():
			var destination: Dictionary=return_destination(state,data,campaign.id,params.garrison_id)
			if not destination.ok and destination.code=="CampaignFriendlyGarrisonRequired": return destination
	var definition: Dictionary=data.military_orders[campaign.template_id]
	if kind in ["campaign_depart","campaign_deliver"]:
		var office: Dictionary=state.get("offices",{}).get(campaign.office_id,{})
		if not state.actors[actor_id].alive or office.get("holder_actor_id")!=actor_id or not office.get("enabled",false): return _fail("CampaignAuthorityLost")
		var commander: Dictionary=state.actors.get(_army(state,campaign).get("commander_actor_id"),{})
		if commander.is_empty() or not commander.alive or not commander.get("action_id","").is_empty(): return _fail("CampaignCommanderBusy")
	match kind:
		"campaign_depart":
			if campaign.status!="mustered": return _fail("CampaignNotMustered")
			if _army(state,campaign).get("status")!="active": return _fail("CampaignArmyUnavailable")
		"campaign_return":
			if not campaign.status in ["mustered","marching","operating","returning"]: return _fail("CampaignCannotReturn")
		"campaign_deliver":
			if campaign.status!="operating" or definition.objective.kind!="deliver_supply" or not campaign.objective_result.is_empty(): return _fail("CampaignSupplyNotPending")
			var target: Dictionary=data.strongholds[definition.target_stronghold_id]
			if not at_settlement(state,data,campaign.army_id,target.settlement_id): return _fail("CampaignNotAtTarget")
			var army: Dictionary=_army(state,campaign); var recipient: Dictionary=data.garrisons[target.garrison_id]
			if definition.objective.get("recipient","garrison")=="army":
				recipient=state.get("armies",{}).get(campaign.recipient_army_id,{})
				if recipient.get("status")!="active" or recipient.get("organization_id")!=definition.objective.get("recipient_organization_id",campaign.issuer_organization_id): return _fail("CampaignRecipientArmyUnauthorized")
				if not at_settlement(state,data,campaign.recipient_army_id,target.settlement_id): return _fail("CampaignRecipientNotAtTarget")
			var source: Dictionary=state.inventories.get(army.get("inventory_id"),{}); var destination: Dictionary=state.inventories.get(recipient.inventory_id,{})
			var owner: Dictionary={"kind":"organization","id":army.organization_id}; var recipient_owner: Dictionary={"kind":"organization","id":recipient.organization_id}
			if source.is_empty() or destination.is_empty() or source.owner!=owner or destination.owner!=recipient_owner: return _fail("CampaignSupplyOwnership")
			if source.id==destination.id: return _fail("SameContainer")
			if definition.objective.get("recipient","garrison")=="garrison" and state.get("strongholds",{}).get(target.id,{}).get("owner_organization_id",recipient.organization_id)!=recipient.organization_id: return _fail("CampaignRecipientChanged")
			if Inventory.count(source,definition.objective.item_id,[owner])<definition.objective.quantity: return _fail("CampaignSupplyInsufficient")
			if Inventory.weight(destination,data)+definition.objective.quantity*data.items[definition.objective.item_id].weight>destination.capacity: return _fail("CampaignRecipientFull")
			return {"ok":true,"campaign_id":campaign.id,"source_inventory_id":army.inventory_id,"destination_inventory_id":recipient.inventory_id,"source_owner":owner,"recipient_owner":recipient_owner,"item_id":definition.objective.item_id,"quantity":definition.objective.quantity}
		"campaign_report":
			if campaign.status!="awaiting_report": return _fail("CampaignReturnNotSettled")
			var context: Dictionary=Interaction.validate(state,data,actor_id,params.get("context"),"military_report")
			if not context.ok: return context
			if not context.context.role_id in report_roles(state,data,campaign.id): return _fail("CampaignReportRoleRequired")
		"campaign_cancel": pass
	return {"ok":true,"campaign_id":campaign.id}

## 在Kernel候选中重验命令并保存永久事实；交货只转移已存在公物，不支付额外奖励或改私人所有权。
static func apply(state: Dictionary, data: Dictionary, actor_id: String, kind: String, params: Dictionary) -> Dictionary:
	var quote: Dictionary=preview(state,data,actor_id,kind,params)
	if not quote.ok: return quote
	if kind=="campaign_accept":
		var definition: Dictionary=data.military_orders[quote.template_id]; var id: String=Factory.next_id(state,"campaign")
		state.campaigns[id]={"id":id,"template_id":definition.id,"actor_id":actor_id,"office_id":definition.office_id,"garrison_id":definition.garrison_id,"issuer_role_id":definition.issuer_role_id,"issuer_organization_id":data.offices[definition.office_id].organization_id,"status":"accepted","army_id":"","accepted_tick":state.tick,"deadline_tick":state.tick+definition.deadline_ticks,"last_tick":state.tick,"objective_result":{},"settlement_id":"","arrival_fact_id":"","return_receipt_id":"","recipient_army_id":quote.recipient_army_id,"return_garrison_id":"","return_wait_reason":""}
		return {"ok":true,"details":{"campaign_id":id}}
	var campaign: Dictionary=state.campaigns[quote.campaign_id]; var details: Dictionary={"campaign_id":campaign.id}
	match kind:
		"campaign_depart":
			campaign.status="marching"; details.journey_request={"army_id":campaign.army_id,"destination_settlement_id":data.strongholds[data.military_orders[campaign.template_id].target_stronghold_id].settlement_id}
		"campaign_deliver":
			var moved: Dictionary=Inventory.transfer(state,quote.source_inventory_id,quote.destination_inventory_id,quote.item_id,quote.quantity,[quote.source_owner],"",quote.recipient_owner)
			if not moved.ok: return moved
			var fact_id: String=_fact(state,"military_delivery",campaign,{"stronghold_id":data.military_orders[campaign.template_id].target_stronghold_id,"recipient_army_id":campaign.recipient_army_id,"item_id":quote.item_id,"quantity":quote.quantity,"from_inventory_id":quote.source_inventory_id,"to_inventory_id":quote.destination_inventory_id,"source_owner":quote.source_owner,"recipient_owner":quote.recipient_owner})
			campaign.objective_result={"kind":"supply_delivered","tick":state.tick,"source_id":fact_id}; details.receipt_id=fact_id
		"campaign_return","campaign_cancel":
			if campaign.objective_result.is_empty(): campaign.objective_result={"kind":"cancelled" if kind=="campaign_cancel" else "withdrawn","tick":state.tick}
			if campaign.army_id.is_empty(): _close(state,campaign,"cancelled")
			else:
				campaign.status="returning"; details.journey_request=_return_request(state,data,campaign,params.get("garrison_id","")); details.return_wait_reason=campaign.return_wait_reason
		"campaign_report":
			_close(state,campaign,"reported")
			state.facts[campaign.settlement_id].report_context=params.context.duplicate(true)
	return {"ok":true,"details":details}

## 编组成功后绑定唯一真实军团；军事资产模块先建军团，调用者须把两步放在同一原子候选。
static func bind_army(state: Dictionary, data: Dictionary, campaign_id: String, army_id: String) -> Dictionary:
	var campaign: Dictionary=state.get("campaigns",{}).get(campaign_id,{}); var army: Dictionary=state.get("armies",{}).get(army_id,{})
	if campaign.is_empty() or campaign.status!="accepted" or not campaign.army_id.is_empty(): return _fail("CampaignAlreadyMustered")
	if army.get("order_id")!=campaign_id or army.get("organization_id")!=campaign.issuer_organization_id or army.get("status")!="active": return _fail("CampaignArmyMismatch")
	if not data.garrisons.has(campaign.garrison_id): return _fail("UnknownCampaignGarrison")
	campaign.army_id=army_id; campaign.status="mustered"
	return {"ok":true}

## 只有人产模块已经实际回营并关闭军团的永久交接收据才能进入待回报；读档重复调用不重领物资。
static func army_returned(state: Dictionary, data: Dictionary, campaign_id: String, receipt_id: String) -> Dictionary:
	var campaign: Dictionary=state.get("campaigns",{}).get(campaign_id,{}); var receipt: Dictionary=state.facts.get(receipt_id,{})
	if campaign.is_empty(): return _fail("UnknownCampaign")
	var checked: Dictionary=_return_receipt(state,data,campaign,receipt_id)
	if not checked.ok: return checked
	if campaign.status in ["awaiting_report","closed"]:
		return {"ok":true} if campaign.return_receipt_id==receipt_id else _fail("CampaignReturnReceiptMismatch")
	if not campaign.status in ["mustered","marching","operating","returning"]: return _fail("CampaignReturnNotSettled")
	if campaign.objective_result.is_empty(): campaign.objective_result={"kind":"withdrawn","tick":state.tick}
	campaign.status="awaiting_report"; campaign.return_receipt_id=receipt_id
	campaign.return_garrison_id=receipt.garrison_id; campaign.return_wait_reason=""
	_close_orphaned_return(state,data,campaign,state.tick)
	return {"ok":true}

## 世界只调用一次半日观察：实际到场才改阶段，超期/失效只请求返程，不瞬移回营也不重复扣粮。
static func step(state: Dictionary, data: Dictionary, finish_tick: int, observations: Dictionary={}) -> Dictionary:
	if finish_tick!=state.tick+1: return _fail("CampaignInvalidStep")
	var requests: Array=[]
	for id in state.get("campaigns",{}):
		var campaign: Dictionary=state.campaigns[id]
		if campaign.last_tick==finish_tick: continue
		if campaign.last_tick!=state.tick: return _fail("CampaignSkippedStep",id)
		campaign.last_tick=finish_tick
		if campaign.status=="closed": continue
		var definition: Dictionary=data.military_orders[campaign.template_id]
		if campaign.status=="marching" and at_settlement(state,data,campaign.army_id,data.strongholds[definition.target_stronghold_id].settlement_id):
			campaign.status="operating"; campaign.arrival_fact_id=_fact(state,"campaign_arrival",campaign,{"stronghold_id":definition.target_stronghold_id,"tick":finish_tick})
	var consumed: Dictionary=consume_receipts(state,data,observations.get("completed_receipt_ids",[]),finish_tick)
	if not consumed.ok: return consumed
	for id in state.get("campaigns",{}):
		var campaign: Dictionary=state.campaigns[id]
		if campaign.status=="closed": continue
		var lost: bool=_authority_lost(state,campaign)
		if lost or (campaign.objective_result.is_empty() and finish_tick>campaign.deadline_tick):
			if campaign.objective_result.is_empty(): campaign.objective_result={"kind":"authority_lost" if lost else "expired","tick":finish_tick}
			if campaign.army_id.is_empty(): _close(state,campaign,"authority_lost" if lost else "expired",finish_tick)
			elif not campaign.status in ["returning","awaiting_report"]:
				campaign.status="returning"; campaign.return_wait_reason="CampaignReturnPending"
		if campaign.status=="returning":
			var previous: String=campaign.get("return_garrison_id",""); var waiting: String=campaign.get("return_wait_reason","")
			var request: Dictionary=_return_request(state,data,campaign)
			if not request.is_empty() and (previous!=campaign.return_garrison_id or not waiting.is_empty()): requests.append(request)
		_close_orphaned_return(state,data,campaign,finish_tick)
	return {"ok":true,"journey_requests":requests}

## 已落地的占领或解围可在当前时刻即时入账，也可由统一半日结算调用；重放不改结果或半日游标。
static func consume_receipts(state: Dictionary, data: Dictionary, receipt_ids: Array, tick: int) -> Dictionary:
	if tick<state.tick or tick>state.tick+1: return _fail("CampaignReceiptTime")
	var consumed: Array=[]
	for receipt_id in receipt_ids:
		if not receipt_id is String or not state.facts.get(receipt_id) is Dictionary: return _fail("CampaignReceiptMissing",str(receipt_id))
		var fact: Dictionary=state.facts[receipt_id]
		var campaign: Dictionary=state.get("campaigns",{}).get(fact.get("campaign_id"),{})
		if campaign.is_empty() or campaign.status!="operating" or not campaign.objective_result.is_empty(): continue
		var definition: Dictionary=data.military_orders[campaign.template_id]
		var expected: String="military_occupation" if definition.objective.kind=="occupy" else "military_relief"
		if definition.objective.kind=="deliver_supply" or fact.get("kind")!=expected or fact.get("army_id")!=campaign.army_id or fact.get("stronghold_id")!=definition.target_stronghold_id or fact.get("phase")!="completed": continue
		if not _whole(fact.get("tick"),campaign.accepted_tick,tick): return _fail("CampaignReceiptTime",receipt_id)
		campaign.objective_result={"kind":"occupied" if definition.objective.kind=="occupy" else "relieved","tick":tick,"source_id":receipt_id}
		consumed.append(receipt_id)
	return {"ok":true,"consumed_receipt_ids":consumed}

## 统帅真人是军团唯一宏观位置，荒野坐标必须真的到达城点；时间到期或请求援军本身不算抵达。
static func at_settlement(state: Dictionary, data: Dictionary, army_id: String, settlement_id: String) -> bool:
	var army: Dictionary=state.get("armies",{}).get(army_id,{})
	var commander: Dictionary=state.actors.get(army.get("commander_actor_id"),{})
	if commander.is_empty() or not data.settlements.has(settlement_id): return false
	var location: Dictionary=commander.get("location",{})
	if location.get("kind")=="Settlement": return location.get("settlement_id")==settlement_id
	if location.get("kind")!="Wilderness": return false
	var settlement: Dictionary=data.settlements[settlement_id]
	var map_id: String=data.get("scenarios",{}).get(state.get("scenario_id"),{}).get("world_map_id",settlement.get("map_id",""))
	return not map_id.is_empty() and settlement.has("map_xy") and location.get("point")==settlement.map_xy and location.get("map_id")==map_id

## 读取军团引用，不通过军令字段补造不存在的军团。
static func _army(state: Dictionary, campaign: Dictionary) -> Dictionary:
	return state.get("armies",{}).get(campaign.army_id,{})

## 尚未撤离的围城或结果页阻止宏观返程；正常撤离由 Siege 正式命令留下 withdrawn_army_ids。
static func pending_siege(state: Dictionary, army_id: String) -> String:
	if army_id.is_empty(): return ""
	for id in _ids(state.get("sieges",{})):
		var siege: Dictionary=state.sieges[id]
		if siege.get("phase")!="closed" and siege.get("armies",{}).has(army_id) and not army_id in siege.get("withdrawn_army_ids",[]): return id
	return ""

## 返程只选择现有同旗营；优先已明确选择或仍合法的原营，原营失陷才比较真实通行成本。
static func return_destination(state: Dictionary, data: Dictionary, campaign_id: String, garrison_id: String="") -> Dictionary:
	var campaign: Dictionary=state.get("campaigns",{}).get(campaign_id,{})
	if campaign.is_empty(): return _fail("UnknownCampaign")
	var army: Dictionary=_army(state,campaign)
	if army.get("status")!="active": return _fail("CampaignArmyUnavailable")
	var preferred: String=garrison_id if not garrison_id.is_empty() else campaign.get("return_garrison_id","")
	if not garrison_id.is_empty() and (not data.get("garrisons",{}).has(garrison_id) or not _friendly(state,data,campaign,garrison_id)): return _fail("CampaignFriendlyGarrisonRequired")
	if preferred.is_empty() or not _friendly(state,data,campaign,preferred): preferred=campaign.garrison_id if _friendly(state,data,campaign,campaign.garrison_id) else ""
	var candidates: Array=[preferred] if not preferred.is_empty() else _ids(data.get("garrisons",{}))
	var best: Dictionary={}; var any_friendly: bool=false
	for id in candidates:
		if not _friendly(state,data,campaign,id): continue
		any_friendly=true
		var place: Dictionary=data.places[data.garrisons[id].place_id]
		var route: Dictionary=_return_cost(state,data,army,place.settlement_id)
		if not route.ok: continue
		if best.is_empty() or route.cost<best.cost:
			best={"ok":true,"army_id":army.id,"destination_settlement_id":place.settlement_id,"return_garrison_id":id,"cost":route.cost}
	if best.is_empty():
		return {"ok":false,"code":"CampaignNoFriendlyRoute" if any_friendly else "CampaignNoFriendlyGarrison","return_garrison_id":preferred if any_friendly else ""}
	return best

## 产权只从永久占领链读取，作者原旗号不能给已经失去的营地继续发军令。
static func _friendly(state: Dictionary, data: Dictionary, campaign: Dictionary, garrison_id: String) -> bool:
	var owner: Dictionary=Ownership.garrison_owner(state,data,garrison_id)
	return owner.ok and owner.organization_id==campaign.issuer_organization_id

## 只读比较真实地形或旧道路时耗，实际出城、付款与每半日位移仍由 Travel/Kernel 决定。
static func _return_cost(state: Dictionary, data: Dictionary, army: Dictionary, settlement_id: String) -> Dictionary:
	if at_settlement(state,data,army.id,settlement_id): return {"ok":true,"cost":0}
	var location: Dictionary=state.actors.get(army.get("commander_actor_id",""),{}).get("location",{})
	var map_id: String=data.get("scenarios",{}).get(state.get("scenario_id",""),{}).get("world_map_id","")
	if data.get("world_maps",{}).has(map_id):
		var point: Array=[]
		if location.get("kind")=="Wilderness" and location.get("map_id")==map_id: point=location.point
		elif location.get("kind")=="Settlement" and data.settlements.has(location.get("settlement_id")): point=data.settlements[location.settlement_id].map_xy
		if point.is_empty(): return _fail("CampaignCommanderLocationRequired")
		return Terrain.plan(data.world_maps[map_id],point,data.settlements[settlement_id].map_xy,"army",state.get("flags",{}))
	if not data.get("routes",{}).is_empty() and location.get("kind")=="Settlement":
		var route: Dictionary=Graph.plan(data,location.settlement_id,settlement_id,state.tick)
		return {"ok":true,"cost":route.duration} if route.ok else route
	# 旧领域包可以仅给请求边界；没有路线定义时不伪造路径或改变人物当前位置。
	return {"ok":true,"cost":0}

## 记录选营与等待原因；无安置处时保留军团和物资，只返回空路线供根层跳过。
static func _return_request(state: Dictionary, data: Dictionary, campaign: Dictionary, garrison_id: String="") -> Dictionary:
	var siege: String=pending_siege(state,campaign.army_id)
	if not siege.is_empty(): campaign.return_wait_reason="SiegePending"; return {}
	var commander: Dictionary=state.actors.get(_army(state,campaign).get("commander_actor_id",""),{})
	if not commander.get("alive",false) or not commander.get("available",false): campaign.return_wait_reason="CampaignCommanderUnavailable"; return {}
	var choice: Dictionary=return_destination(state,data,campaign.id,garrison_id)
	if not choice.ok:
		campaign.return_garrison_id=choice.get("return_garrison_id",""); campaign.return_wait_reason=choice.code; return {}
	campaign.return_garrison_id=choice.return_garrison_id
	var location: Dictionary=commander.get("location",{})
	if location.get("kind")=="Settlement" and location.get("settlement_id")!=choice.destination_settlement_id and data.settlements[location.settlement_id].has("entry_place") and location.get("place_id")!=data.settlements[location.settlement_id].entry_place:
		campaign.return_wait_reason="CampaignNotAtCityExit"; return {}
	var action: Dictionary=state.actions.get(commander.get("action_id",""),{})
	if not action.is_empty() and action.get("kind")!="travel": campaign.return_wait_reason="CampaignCommanderBusy"; return {}
	campaign.return_wait_reason=""
	return {"army_id":campaign.army_id,"destination_settlement_id":choice.destination_settlement_id,"return_garrison_id":choice.return_garrison_id}

## 报告只选当前在场的同组织岗位；行政归档用非在场查询保留暂时离岗的合法办理人，不把等待误作无人负责。
static func report_roles(state: Dictionary, data: Dictionary, campaign_id: String, present_only: bool=true) -> Array:
	var campaign: Dictionary=state.get("campaigns",{}).get(campaign_id,{})
	if campaign.is_empty(): return []
	var definition: Dictionary=data.military_orders[campaign.template_id]
	var camp_id: String=campaign.get("return_garrison_id",campaign.garrison_id)
	if camp_id.is_empty(): camp_id=campaign.garrison_id
	var candidates: Array=[]
	if _friendly(state,data,campaign,campaign.garrison_id): candidates.append(definition.report_role_id)
	if _friendly(state,data,campaign,camp_id):
		for id in _ids(data.roles):
			var role: Dictionary=data.roles[id]; var linked: bool=role.get("place_id")==data.garrisons[camp_id].place_id
			for office in data.get("offices",{}).values():
				if office.garrison_id==camp_id and office.get("appointing_role_id")==id: linked=true
			for order in data.get("military_orders",{}).values():
				if order.garrison_id==camp_id and order.report_role_id==id: linked=true
			if linked and not id in candidates: candidates.append(id)
	var result: Array=[]
	for id in candidates:
		var role: Dictionary=data.roles.get(id,{}); var runtime: Dictionary=state.roles.get(id,{})
		var person: Dictionary=state.actors.get(runtime.get("actor_id",""),{})
		if role.get("organization_id")!=campaign.issuer_organization_id or not "military_report" in role.get("services",[]) or not runtime.get("enabled",false) or not person.get("alive",false) or not person.get("available",false): continue
		if present_only and (person.get("location",{}).get("kind")!="Settlement" or person.location.get("place_id")!=role.place_id or not person.get("action_id","").is_empty()): continue
		result.append(id)
	return result

## 归营事实必须匹配军团实际关闭地点及当时合法营旗；战略层不重复搬运 Assets 已交接的人产。
static func _return_receipt(state: Dictionary, data: Dictionary, campaign: Dictionary, receipt_id: String) -> Dictionary:
	var receipt: Dictionary=state.facts.get(receipt_id,{})
	var army: Dictionary=_army(state,campaign)
	if receipt.get("kind")!="military_return" or receipt.get("order_id")!=campaign.id or receipt.get("army_id")!=campaign.army_id or receipt.get("garrison_id")!=army.get("return_garrison_id",campaign.garrison_id) or receipt.get("source_garrison_id",campaign.garrison_id)!=campaign.garrison_id or not state.actors.has(receipt.get("actor_id")) or not _whole(receipt.get("tick"),campaign.accepted_tick,state.tick): return _fail("CampaignReturnReceiptMismatch")
	if army.get("status")!="closed": return _fail("CampaignReturnNotSettled")
	var owner: Dictionary=Ownership.historical_owner(state,data,receipt.garrison_id,receipt.get("ownership_id",""))
	if not owner.ok or owner.organization_id!=campaign.issuer_organization_id: return _fail("CampaignReturnReceiptMismatch")
	return {"ok":true,"receipt":receipt}

## 任职失效只影响之后的军权，不抹去已经实际交回的公物或原来的战事结果。
static func _authority_lost(state: Dictionary, campaign: Dictionary) -> bool:
	var actor: Dictionary=state.actors[campaign.actor_id]; var office: Dictionary=state.get("offices",{}).get(campaign.office_id,{})
	return not actor.alive or not office.get("enabled",false) or office.get("holder_actor_id")!=campaign.actor_id

## 只有实际归营收据存在时才处理失去负责人的档案，不伪造当面回报或发放原任务奖励。
static func _close_orphaned_return(state: Dictionary, data: Dictionary, campaign: Dictionary, tick: int) -> void:
	if campaign.status!="awaiting_report": return
	if not state.actors[campaign.actor_id].alive: _close(state,campaign,"responsible_actor_lost",tick)
	elif _authority_lost(state,campaign) and report_roles(state,data,campaign.id,false).is_empty(): _close(state,campaign,"authority_lost_returned",tick)

## 稳定遍历来源ID，不因内容字典顺序而改变备选友营或报告岗位。
static func _ids(table: Dictionary) -> Array:
	var result: Array=table.keys(); result.sort(); return result


## 关闭只写交接结果，已经归还的钱货不在战略层重复返还。
static func _close(state: Dictionary, campaign: Dictionary, reason: String, tick: int=-1) -> void:
	campaign.status="closed"; campaign.settlement_id=_fact(state,"campaign_settlement",campaign,{"reason":reason,"objective_result":campaign.objective_result.duplicate(true),"return_receipt_id":campaign.return_receipt_id,"tick":state.tick if tick<0 else tick})

## 使用世界实例序号保存长期事实，不依赖会裁剪的命令缓存来防止重复交付。
static func _fact(state: Dictionary, kind: String, campaign: Dictionary, extra: Dictionary) -> String:
	var id: String=Factory.next_id(state,"fact"); var fact: Dictionary={"kind":kind,"campaign_id":campaign.id,"army_id":campaign.army_id,"actor_id":campaign.actor_id,"tick":state.tick}
	fact.merge(extra,true); state.facts[id]=fact; return id

## 验证事实属于同一军令，名称相同或另一军团的收据不能替代。
static func _fact_matches(state: Dictionary, id: Variant, kind: String, campaign: Dictionary, last_tick: int) -> bool:
	if not id is String or id.is_empty() or not state.facts.get(id) is Dictionary: return false
	var fact: Dictionary=state.facts[id]
	return fact.get("kind")==kind and fact.get("campaign_id")==campaign.id and fact.get("army_id")==campaign.army_id and _whole(fact.get("tick"),campaign.accepted_tick,last_tick)

## 允许JSON整数表示，拒绝布尔、分数和越界值，防止时间或资源溢出。
static func _whole(value: Variant, low: int, high: int) -> bool:
	return (value is int or value is float) and float(value)==floor(float(value)) and value>=low and value<=high

## 结构化失败交给Kernel丢弃候选，不吞掉资产或权限问题继续结算。
static func _fail(code: String, path: String="") -> Dictionary:
	return {"ok":false,"code":code,"path":path}
