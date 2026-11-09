## 军政人产夹具：两处真实公仓和不同编制复用原世界工厂，战略军令由测试按已约定边界注入。
extends RefCounted
const Catalog=preload("res://core/content_catalog.gd")
const Factory=preload("res://core/world_factory.gd")
const Military=preload("res://core/military_assets.gd")
const Estate=preload("res://core/estate_rules.gd")

## 只在测试目录构造两种驻军配置，不把固定演员、粮食或军队写进领域实现。
static func fresh() -> Dictionary:
	var catalog: Dictionary=Catalog.read_prepared("res://data/generated/lived_city.hmcatalog.json")
	var data: Dictionary=catalog.data.duplicate(true)
	data.items["item.spear"]={"id":"item.spear","name_key":"name.actor.player","kind":"commodity","weight":2,"base_price":10}
	data.items["item.beans"]={"id":"item.beans","name_key":"name.actor.player","kind":"commodity","weight":1,"base_price":4}
	data.troop_types={"troop.foot":{},"troop.archer":{}}
	data.offices={}; data.garrisons={}
	var definitions: Array=[{"id":"garrison.a","office":"office.a","organization":"org.office","place":"west.office","role":"role.office","provider":"actor.chen","commander":"actor.player","captain":"actor.carter","food":"item.grain"},{"id":"garrison.b","office":"office.b","organization":"org.grain","place":"east.home","role":"role.military_b","provider":"actor.zheng","commander":"actor.shen","captain":"actor.he","food":"item.beans"}]
	for config in definitions:
		var role: Dictionary=data.roles["role.office"].duplicate(true)
		role.id=config.role; role.initial_actor=config.provider; role.organization_id=config.organization; role.place_id=config.place; role.services=["office_accept","office_resign"]
		data.roles[config.role]=role
		for person in [config.provider,config.commander,config.captain]: data.actors[person].initial_place=config.place
		data.offices[config.office]={"id":config.office,"name_key":"name.actor.player","organization_id":config.organization,"appointing_role_id":config.role,"garrison_id":config.id,"command_scope":"army"}
		var inventory_id: String="inventory."+config.id; var wallet_id: String="wallet."+config.id
		var owner: Dictionary={"kind":"organization","id":config.organization}
		data.inventories[inventory_id]={"id":inventory_id,"owner":owner.duplicate(),"capacity":100000,"goods":[{"item_id":config.food,"quantity":10000},{"item_id":"item.spear","quantity":200}]}
		data.wallets[wallet_id]={"id":wallet_id,"owner":owner.duplicate(),"initial_balance":500}
		var units: Array=[{"id":"unit."+config.id+".a","troop_type_id":"troop.foot","commander_actor_id":config.commander,"initial_soldiers":6,"equipment":[{"item_id":"item.spear","quantity_per_person":1}]}]
		if config.id=="garrison.a": units.append({"id":"unit."+config.id+".b","troop_type_id":"troop.archer","commander_actor_id":config.captain,"initial_soldiers":4,"equipment":[{"item_id":"item.spear","quantity_per_person":2}]})
		data.garrisons[config.id]={"id":config.id,"name_key":"name.actor.player","place_id":config.place,"organization_id":config.organization,"inventory_id":inventory_id,"wallet_id":wallet_id,"food_item_id":config.food,"ration_per_person":1,"army_capacity":200,"units":units}
	var runtime: Dictionary=catalog.duplicate(true); runtime.data=data
	var state: Dictionary=Factory.create(runtime,"scenario.lived_city").state
	state.facts={}; state.battles={}; state.campaigns={}
	for actor in state.actors.values(): actor.injury=0; actor.combat={"weapon_id":"","learned":{},"heart_id":""}
	var initialized: Dictionary=Military.initialize(state,data)
	return {"state":state,"data":data,"initialized":initialized,"actor":"actor.player","enemy":"actor.shen","camp":"garrison.a","enemy_camp":"garrison.b","office":"office.a","enemy_office":"office.b","unit":"unit.garrison.a.a","second_unit":"unit.garrison.a.b","enemy_unit":"unit.garrison.b.a","retainer":"actor.apprentice"}

## 创建已有私人庄园，借调将通过既有Estate入口真实改变该人的劳作与守备。
static func add_estate(f: Dictionary) -> Dictionary:
	var id: String="estate.military"
	var owner: Dictionary={"kind":"actor","id":f.actor}
	var inventory_id: String="inventory.private_military"; var wallet_id: String="wallet.private_military"
	f.data.estates={id:{"id":id,"name_key":"name.actor.player","place_id":f.data.garrisons[f.camp].place_id,"owner_actor_id":f.actor,"manager_actor_id":"actor.host","resident_actor_ids":["actor.host",f.retainer],"inventory_id":inventory_id,"wallet_id":wallet_id,"food_item_id":"item.grain","ration_per_person":1,"output_item_id":"item.grain","output_per_worker":3,"granary_capacity_per_level":20,"palisade_defense_per_guard":5,"initial_facilities":{"farm":1,"granary":0,"palisade":1},"initial_duties":[{"actor_id":"actor.host","duty":"idle"},{"actor_id":f.retainer,"duty":"guard"}],"construction":{"max_level":3,"build_ticks":1,"money_cost":10,"material_item_id":"item.grain","material_cost":2,"payee_wallet_id":f.state.actors["actor.chen"].wallet_id}}}
	f.data.inventories[inventory_id]={"id":inventory_id,"owner":owner.duplicate(),"capacity":10000,"goods":[]}
	f.data.wallets[wallet_id]={"id":wallet_id,"owner":owner.duplicate(),"initial_balance":300}
	f.state.inventories[inventory_id]={"id":inventory_id,"owner":owner.duplicate(),"capacity":10000,"revision":0,"lots":[{"id":Factory.next_id(f.state,"lot"),"item_id":"item.grain","quantity":1000,"owner":owner.duplicate(),"reserved_by":""}]}
	f.state.wallets[wallet_id]={"id":wallet_id,"owner":owner.duplicate(),"balance":300,"reserved":{}}
	for actor in ["actor.host",f.retainer]: f.state.actors[actor].location=Factory.settlement_location(f.data,f.data.estates[id].place_id)
	f.estate=id
	return Estate.initialize(f.state,f.data)
