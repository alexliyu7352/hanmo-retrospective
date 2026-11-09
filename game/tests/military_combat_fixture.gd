## 军事桥接领域夹具：工厂、当面任官、实际军令、编组领装均走正式模块；宏观抵达由测试边界明确提供。
extends RefCounted
const Base=preload("res://tests/military_assets_fixture.gd")
const Definitions=preload("res://tests/military_fixture.gd")
const Catalog=preload("res://core/content_catalog.gd")
const Assets=preload("res://core/military_assets.gd")
const Campaign=preload("res://core/campaign_rules.gd")
const Combat=preload("res://core/military_combat.gd")
const Round=preload("res://core/military_round.gd")
const Factory=preload("res://core/world_factory.gd")
const Interaction=preload("res://core/interaction_rules.gd")

## 复用真实内容工厂和两个驻军，仅增加有限六角作者配置，不手填一个已形成军团绕开军政人产。
static func fresh(paired: bool=false) -> Dictionary:
	var f: Dictionary=Base.fresh(); var definitions: Dictionary=Definitions.data()
	f.data.troop_types=definitions.troop_types.duplicate(true); f.data.troop_types["troop.archer"]=f.data.troop_types["troop.bow"].duplicate(true); f.data.troop_types["troop.archer"].id="troop.archer"
	f.data.battlefields=definitions.battlefields; f.data.military_objectives=definitions.military_objectives
	for field in f.data.battlefields.values():
		field.objective_id="goal.repel"; field.spawns.append({"side":0,"position":[0,2]})
	var target: String=f.data.places[f.data.garrisons[f.enemy_camp].place_id].settlement_id
	f.data.strongholds={"stronghold.test":{"id":"stronghold.test","settlement_id":target,"garrison_id":f.enemy_camp}}
	f.data.military_orders={}
	for office in [f.office,f.enemy_office]:
		var definition: Dictionary=f.data.offices[office]
		f.data.roles[definition.appointing_role_id].services.append_array(["military_order","military_report"])
		f.data.military_orders[office]={"id":office,"office_id":office,"garrison_id":definition.garrison_id,"issuer_role_id":definition.appointing_role_id,"report_role_id":definition.appointing_role_id,"target_stronghold_id":"stronghold.test","deadline_ticks":400,"objective":{"kind":"occupy"}}
	if paired:
		var extra: Dictionary=f.data.garrisons[f.enemy_camp].units[0].duplicate(true); extra.id="unit.garrison.b.b"; extra.commander_actor_id="actor.he"; extra.troop_type_id="troop.archer"
		f.data.garrisons[f.enemy_camp].units.append(extra); f.enemy_second_unit=extra.id
		# 保留真实准备目录的成功标记与Schema身份；测试扩展只替换作者数据副本。
		var catalog: Dictionary=Catalog.read_prepared("res://data/generated/lived_city.hmcatalog.json"); catalog.data=f.data
		f.state=Factory.create(catalog,f.state.scenario_id).state; f.state.campaigns={}
		for actor in f.state.actors.values(): actor.injury=0; actor.combat={"weapon_id":"","learned":{},"heart_id":""}
		f.initialized=Assets.initialize(f.state,f.data)
	f.initialized=Assets.validate_state(f.state,f.data)
	return f

## 调用正式半日行动的预览、占用、材料保留及完工；失败原样交给测试，不偷偷补权限或库存。
static func action(f: Dictionary, actor: String, kind: String, params: Dictionary) -> Dictionary:
	var quote: Dictionary=Assets.preview(f.state,f.data,actor,kind,params)
	if not quote.ok: return quote
	var id: String=Factory.next_id(f.state,"action")
	var command: Dictionary={"id":id,"kind":kind,"actor_id":actor,"participants":quote.participants.duplicate(),"start_tick":f.state.tick,"remaining_ticks":1,"params":params}
	f.state.actions[id]=command
	for person in command.participants: f.state.actors[person].action_id=id
	var begun: Dictionary=Assets.begin(f.state,f.data,command,quote)
	if not begun.ok: return begun
	var supply: Dictionary=Assets.step(f.state,f.data,f.state.tick+1)
	if not supply.ok: return supply
	var campaign: Dictionary=Campaign.step(f.state,f.data,f.state.tick+1)
	if not campaign.ok: return campaign
	command.remaining_ticks=0
	var result: Dictionary=Assets.finish(f.state,f.data,command)
	if not result.ok or not result.get("success",false): return result
	for person in command.participants: f.state.actors[person].action_id=""
	f.state.actions.erase(id)
	if kind=="army_form":
		var bound: Dictionary=Campaign.bind_army(f.state,f.data,result.details.order_id,result.details.army_id)
		if not bound.ok: return bound
	f.state.tick+=1
	return result

## 先当面任官受令，再实际编组领粮；无论友敌都不能靠测试直接创建运行军团。
static func mobilize(f: Dictionary, enemy: bool=false, second_unit: bool=true, retainers: Array=[]) -> Dictionary:
	var actor: String=f.enemy if enemy else f.actor; var office: String=f.enemy_office if enemy else f.office
	var role: String=f.data.offices[office].appointing_role_id
	var context: Dictionary=Interaction.for_role(f.state,f.data,role)
	var appointed: Dictionary=action(f,actor,"office_accept",{"office_id":office,"context":context})
	if not appointed.ok: return appointed
	var accepted: Dictionary=Campaign.apply(f.state,f.data,actor,"campaign_accept",{"template_id":office,"context":Interaction.for_role(f.state,f.data,role)})
	if not accepted.ok: return accepted
	if not retainers.is_empty():
		var estate: Dictionary=Base.add_estate(f)
		if not estate.ok: return estate
		var pledge: Dictionary=Assets.preview(f.state,f.data,actor,"army_pledge",{"order_id":accepted.details.campaign_id,"estate_id":f.estate,"actor_ids":retainers})
		if not pledge.ok: return pledge
		var pledged: Dictionary=Assets.apply(f.state,f.data,actor,"army_pledge",pledge)
		if not pledged.ok: return pledged
	var ids: Array=([f.enemy_unit,f.enemy_second_unit] if f.has("enemy_second_unit") and second_unit else [f.enemy_unit]) if enemy else ([f.unit,f.second_unit] if second_unit else [f.unit])
	return action(f,actor,"army_form",{"order_id":accepted.details.campaign_id,"unit_ids":ids,"retainer_ids":retainers,"food_quantity":100,"money":40})

## 显式模拟已完成的宏观行军边界，只搬统帅权威位置；此夹具不声称验证 TerrainJourney 路径。
static func contact(f: Dictionary, army_ids: Array, unit_ids: Array, controller: String="actor.player", field: String="field.open") -> Dictionary:
	var location: Dictionary=f.state.actors[f.actor].location.duplicate(true)
	var sides: Dictionary={}; var orders: Dictionary={}; var locations: Dictionary={}
	for index in range(army_ids.size()):
		var army: Dictionary=f.state.armies[army_ids[index]]
		f.state.actors[army.commander_actor_id].location=location.duplicate(true)
		sides[army.id]=index; orders[army.id]=army.order_id; locations[army.id]=location.duplicate(true)
	var id: String=Factory.next_id(f.state,"contact")
	var objective: Dictionary={"id":f.data.battlefields[field].objective_id,"unit_ids":[]}
	f.state.facts[id]={"kind":"military_contact","tick":f.state.tick,"battlefield_id":field,"unit_ids":unit_ids.duplicate(),"side_by_army":sides.duplicate(),"objective":objective.duplicate(true),"army_order_ids":orders,"army_locations":locations}
	return {"source_id":id,"battlefield_id":field,"unit_ids":unit_ids.duplicate(),"side_by_army":sides,"objective":objective,"controller_actor_id":controller}

## 仅以当前公开战场生成测试玩家的合法计划；正式玩家仍由界面提交，不把胜利写进结果。
static func plans(battle: Dictionary, data: Dictionary) -> Dictionary:
	var autonomous: Dictionary=battle.duplicate(true); autonomous.controller_actor_id=""; autonomous.command_army_id=""; autonomous.command_unit_ids=[]
	var ai: Dictionary=Round.make_ai(autonomous,data); var result: Dictionary={}
	if not ai.ok: return result
	for id in battle.command_unit_ids:
		if ai.orders.has(id): result[id]=ai.orders[id]
	return result

## 根层约定先预检再计半日开始口粮，随后逐场真实战损与真人回写，最后只加一次世界时钟。
static func advance(f: Dictionary) -> Dictionary:
	var finish_tick: int=f.state.tick+1
	var check: Dictionary=Combat.preview_step(f.state,f.data,finish_tick)
	if not check.ok: return check
	var supply: Dictionary=Assets.step(f.state,f.data,finish_tick)
	if not supply.ok: return supply
	var result: Dictionary=Combat.step(f.state,f.data,finish_tick)
	if not result.ok: return result
	apply_people(f,result.actor_changes)
	var campaigns: Dictionary=Campaign.step(f.state,f.data,finish_tick)
	if not campaigns.ok: return campaigns
	f.state.tick=finish_tick; result.supply_events=supply.events
	return result

## 真人死伤与看管变化由根层边界办理；测试沿用正式人产返回的字段，不从战术动画猜伤亡。
static func apply_people(f: Dictionary, changes: Array) -> void:
	for change in changes:
		var actor: Dictionary=f.state.actors[change.actor_id]
		match change.status:
			"wounded": actor.injury=maxi(1,actor.injury); actor.hp=mini(50,actor.hp)
			"dead": actor.alive=false; actor.available=false; actor.hp=0
			"captured": actor.available=false; actor.location={"kind":"InArmy","army_id":change.captor_army_id}
			"deserted": actor.available=false; actor.location=f.state.actors[f.state.armies[change.army_id].commander_actor_id].location.duplicate(true)
