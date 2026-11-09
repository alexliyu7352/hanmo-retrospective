## 易帜领域夹具：真实任官编组与 Siege 破门守点；宏观到场仅在显式测试边界设置，不冒充地图行程验收。
extends RefCounted
const F=preload("res://tests/military_combat_fixture.gd")
const C=preload("res://tests/campaign_fixture.gd")
const Catalog=preload("res://core/content_catalog.gd")
const Assets=preload("res://core/military_assets.gd")
const Siege=preload("res://core/siege_rules.gd")
const Campaign=preload("res://core/campaign_rules.gd")
const Factory=preload("res://core/world_factory.gd")
const Inventory=preload("res://core/inventory_rules.gd")

## 在同一作者包增设实际友营，驻军仍由工厂初始化，不凭返营临时发放新兵。
static func fresh() -> Dictionary:
	var f: Dictionary=F.fresh()
	var camp: Dictionary=f.data.garrisons[f.enemy_camp].duplicate(true)
	camp.id="garrison.c"; camp.place_id="west.street"; camp.inventory_id="inventory.garrison.c"; camp.wallet_id="wallet.garrison.c"
	camp.units=[camp.units[0].duplicate(true)]; camp.units[0].id="unit.garrison.c.a"; camp.units[0].commander_actor_id="actor.he"
	f.friend_camp=camp.id; f.data.garrisons[camp.id]=camp
	for pair in [["inventories","inventory_id"],["wallets","wallet_id"]]:
		f.data[pair[0]][camp[pair[1]]]=f.data[pair[0]][f.data.garrisons[f.enemy_camp][pair[1]]].duplicate(true)
		f.data[pair[0]][camp[pair[1]]].id=camp[pair[1]]
	var stronghold: Dictionary=f.data.strongholds["stronghold.test"]
	stronghold.topology_id="topology.ownership"; stronghold.public_inventory_ids=[f.data.garrisons[f.enemy_camp].inventory_id]; stronghold.public_wallet_ids=[f.data.garrisons[f.enemy_camp].wallet_id]
	f.data.siege_topologies={stronghold.topology_id:C.topology(stronghold.topology_id)}
	for node in f.data.siege_topologies[stronghold.topology_id].nodes:
		if node.id=="G": node.max_hp=2
	for item in ["wood","iron","ram"]: f.data.items[item]={"id":item,"kind":"commodity","weight":1,"base_price":1,"name_key":"name.actor.player"}
	# 保留真实准备目录的成功标记与Schema身份；测试扩展只替换作者数据副本。
	var catalog: Dictionary=Catalog.read_prepared("res://data/generated/lived_city.hmcatalog.json"); catalog.data=f.data
	f.state=Factory.create(catalog,f.state.scenario_id).state; f.state.campaigns={}; f.state.sieges={}
	for actor in f.state.actors.values(): actor.injury=0; actor.combat={"weapon_id":"","learned":{},"heart_id":""}
	f.initialized=Assets.initialize(f.state,f.data)
	return f

## 从真实军团、名册、统帅位置和公库构建围城观察；不会把待到达军团当作已经增援。
static func observation(f: Dictionary, siege: Dictionary={}) -> Dictionary:
	var obs: Dictionary={"tick":f.state.tick,"controller_actor_id":f.actor,"armies":{},"units":{},"public_goods":{},"control_sets":{},"receipts":f.state.facts.duplicate(true),"defender_food":0,"defender_commander_out":f.army_b in siege.get("withdrawn_army_ids",[])}
	for id in [f.army_a,f.army_b]:
		var army: Dictionary=f.state.armies[id]
		obs.armies[id]={"commander_actor_id":army.commander_actor_id,"organization_id":army.organization_id,"location":f.state.actors[army.commander_actor_id].location.duplicate(true)}
		obs.control_sets[army.commander_actor_id]=army.unit_ids.duplicate(); obs.public_goods[id]={}
		for item in f.data.items: obs.public_goods[id][item]=Inventory.count(f.state.inventories[army.inventory_id],item,[{"kind":"organization","id":army.organization_id}])
		for unit_id in army.unit_ids:
			var unit: Dictionary=f.state.military_units[unit_id]
			obs.units[unit_id]={"army_id":id,"fit_count":unit.fit.size(),"morale":unit.morale,"available":army.status=="active","engaged":false}
	var camp: Dictionary=f.data.garrisons[f.enemy_camp]
	obs.defender_food=Inventory.count(f.state.inventories[camp.inventory_id],camp.food_item_id,[{"kind":"organization","id":Assets.garrison_owner(f.state,f.data,camp.id).organization_id}])
	return obs

## 敌军实际从己方入口战术撤退，攻军依次破门入城并守足时段；世界口粮和时钟同半日结算。
static func win(f: Dictionary) -> Dictionary:
	var place: String=f.data.garrisons[f.enemy_camp].place_id
	for actor in [f.actor,f.enemy]: f.state.actors[actor].location=Factory.settlement_location(f.data,place)
	var params: Dictionary={"id":Factory.next_id(f.state,"siege"),"campaign_id":f.state.armies[f.army_a].order_id,"stronghold_id":"stronghold.test","attacker_army_ids":[f.army_a],"defender_army_ids":[f.army_b]}
	var created: Dictionary=Siege.create(f.data,params,observation(f))
	if not created.ok: return created
	var siege: Dictionary=created.siege
	var withdrawal: Dictionary=Siege.strategic(siege,f.data,observation(f,siege),f.enemy,{"kind":"withdraw","army_id":f.army_b})
	if not withdrawal.ok: return withdrawal
	siege=withdrawal.siege
	for _round in range(12):
		if siege.phase!="active": break
		var obs: Dictionary=observation(f,siege); var plans: Dictionary={}
		for unit_id in f.state.armies[f.army_a].unit_ids:
			var node: String=siege.unit_nodes[unit_id]
			plans[unit_id]={"action":"assault" if siege.node_hp.G>0 else "move","node_id":"G" if node=="A" else "I"}
			if node=="I": plans[unit_id]={"action":"wait","node_id":"I"}
		var supply: Dictionary=Assets.step(f.state,f.data,f.state.tick+1)
		if not supply.ok: return supply
		var result: Dictionary=Siege.resolve(siege,f.data,obs,f.actor,plans,f.state.tick+1)
		if not result.ok: return result
		siege=result.siege
		var campaign: Dictionary=Campaign.step(f.state,f.data,f.state.tick+1)
		if not campaign.ok: return campaign
		f.state.tick+=1
	if siege.phase!="result" or siege.result.kind!="occupied": return {"ok":false,"code":"FixtureSiegeNotWon"}
	f.state.sieges[siege.id]=siege; f.siege_id=siege.id
	return {"ok":true}

## 写入根层应提供的最小占领回执，保留旧归属等待正式人产入口一次性转移。
static func receipt(f: Dictionary) -> String:
	var id: String=Factory.next_id(f.state,"military")
	var current: Dictionary=Assets.garrison_owner(f.state,f.data,f.enemy_camp)
	var stronghold: Dictionary=f.data.strongholds["stronghold.test"]
	f.state.facts[id]={"kind":"military_occupation","phase":"completed","tick":f.state.tick,"siege_id":f.siege_id,"campaign_id":f.state.armies[f.army_a].order_id,"army_id":f.army_a,"stronghold_id":stronghold.id,"garrison_id":f.enemy_camp,"from_organization_id":current.organization_id,"to_organization_id":f.state.armies[f.army_a].organization_id,"previous_occupation_id":current.occupation_id,"public_inventory_ids":stronghold.public_inventory_ids.duplicate(),"public_wallet_ids":stronghold.public_wallet_ids.duplicate()}
	return id
