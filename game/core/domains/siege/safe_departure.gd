## 安全离城条款的只读安置预检；复用原军令路线和军事归还资格，不创建兵员、库存或另一行军引擎。
extends RefCounted
const Allocation=preload("res://core/domains/siege/departure_allocation.gd")
const Campaign=preload("res://core/campaign_rules.gd")
const Assets=preload("res://core/military_assets.gd")
const Inventory=preload("res://core/inventory_rules.gd")
const Travel=preload("res://core/travel_rules.gd")
const MAX_VALUE: int=2147483647

## 失败保留具体军团和原领域原因，外层不得先完成占领或扣除任何财物。
static func _fail(army_id: String, reason: String) -> Dictionary:
	return {"ok":false,"code":"SiegeSafeDepartureUnavailable","army_id":army_id,"reason":reason}

## 只列仍在本次围城中的真实守军；顺序不受存档字典重排影响。
static func defenders(siege: Dictionary) -> Array:
	var result: Array=[]
	for army_id in siege.armies:
		if siege.armies[army_id]==1 and not army_id in siege.withdrawn_army_ids: result.append(army_id)
	result.sort()
	return result

## 原接口预核每军合法选项，再有界求全队安置；到达和卸装时仍由原军令/资产规则重验。
static func quote(state: Dictionary, data: Dictionary, siege: Dictionary) -> Dictionary:
	var army_ids: Array=defenders(siege)
	if army_ids.is_empty(): return _fail("","SiegeDefenderRequired")
	if army_ids.size()>Allocation.MAX_ARMIES: return Allocation.limit("armies",Allocation.MAX_ARMIES)
	var target: Dictionary=data.strongholds[siege.stronghold_id]
	var plans: Dictionary={}; var room: Dictionary={}
	var camps: Array=data.garrisons.keys(); camps.sort()
	for army_id in army_ids:
		var army: Dictionary=state.armies[army_id]
		var campaign: Dictionary=state.campaigns[army.order_id]
		if army.status!="active" or campaign.army_id!=army_id or campaign.actor_id!=army.commander_actor_id or not campaign.status in ["mustered","marching","operating","returning"]: return _fail(army_id,"CampaignCannotReturn")
		for member in army.member_actor_ids:
			var person: Dictionary=state.actors[member]
			if not person.alive or not person.available or not person.action_id.is_empty(): return _fail(army_id,"ArmyMemberUnavailable")
		for battle in state.battles.values():
			if battle.phase=="closed": continue
			for unit_id in army.unit_ids:
				if battle.get("units",{}).has(unit_id): return _fail(army_id,"ArmyMustReturnToCamp")
		var weight: int=Inventory.weight(state.inventories[army.inventory_id],data)
		var options: Array=[]; var reason: String="CampaignNoFriendlyGarrison"
		for camp_id in camps:
			var camp: Dictionary=data.garrisons[camp_id]
			# 留在即将交城的任何同城营地都不算履行离城条款。
			if data.places[camp.place_id].settlement_id==target.settlement_id: continue
			var route: Dictionary=Campaign.return_destination(state,data,campaign.id,camp_id)
			if not route.ok: reason=route.code; continue
			# 该只读内部helper与正式army_return共用所有权、预留、公物和金额上限检查。
			var cargo: Dictionary=Assets._return_assets(state,data,army,camp_id)
			if not cargo.ok: reason=cargo.code; continue
			var travel: Dictionary=Travel.plan(state,data,army.commander_actor_id,route.destination_settlement_id)
			if not travel.ok: reason=travel.code; continue
			var inventory: Dictionary=state.inventories[camp.inventory_id]
			room[camp_id]={"weight":inventory.capacity-Inventory.weight(inventory,data),"money":MAX_VALUE-state.wallets[camp.wallet_id].balance}
			if room.size()>Allocation.MAX_CAMPS: return Allocation.limit("camps",Allocation.MAX_CAMPS)
			options.append({"army_id":army_id,"campaign_id":campaign.id,"garrison_id":camp_id,"destination_settlement_id":route.destination_settlement_id,"cost":route.cost,"weight":weight,"money":cargo.money,"preferred":camp_id==campaign.garrison_id})
		if options.is_empty(): return _fail(army_id,reason)
		plans[army_id]=options
	var allocated: Dictionary=Allocation.solve(plans,room)
	if not allocated.ok: return allocated
	return {"ok":true,"army_ids":army_ids,"destinations":allocated.destinations}
