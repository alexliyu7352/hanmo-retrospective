## 营务显示专用小领域来源：任命、军令和编组走原owner；不冒称完整世界Schema或远征验收。
extends RefCounted
const Base=preload("res://tests/family_estate_view_fixture.gd")
const Assets=preload("res://core/military_assets.gd")
const Campaign=preload("res://core/campaign_rules.gd")
const Training=preload("res://core/domains/military/training_rules.gd")
const Factory=preload("res://core/world_factory.gd")
const Step=preload("res://core/world_step.gd")
const Interaction=preload("res://core/interaction_rules.gd")
const Strict=preload("res://core/strict_json.gd")

## 三个作者营地保插入序，第三处默认营地与首项不同；库房及明确训练初值只存在于夹具。
static func create() -> Dictionary:
	var f: Dictionary=Base.create()
	if not f.ok: return f
	var state: Dictionary=f.state; var data: Dictionary=f.data
	data.required_capabilities.append("military"); data.required_capabilities.append("campaigns")
	state.parties={}; state.campaigns={}; state.strongholds={}
	data.offices={}; data.garrisons={}; data.military_orders={}; data.strongholds={}
	data.troop_types={"foot":{"id":"foot","name_key":"foot","category":"infantry"},"bow":{"id":"bow","name_key":"bow","category":"archer"}}
	data.texts.merge({"foot":"步卒","bow":"弓手","spear":"长矛","org":"本军","foreign":"异军","gate":"城门","target":"目标城"})
	data.items.spear={"id":"spear","name_key":"spear","weight":2,"kind":"commodity","base_price":10}
	data.organizations={"org":{"id":"org","name_key":"org"},"foreign":{"id":"foreign","name_key":"foreign"}}
	data.settlements.town.entry_place="gate"; data.settlements.target={"id":"target","entry_place":"target.room","name_key":"target"}
	data.places.gate={"id":"gate","name_key":"gate","settlement_id":"town","parent_place":null,"passages":[]}
	data.places["target.room"]={"id":"target.room","name_key":"target","settlement_id":"target","parent_place":null,"passages":[]}
	data.routes={"route":{"id":"route","origin":"town","destination":"target","duration_ticks":3,"cost_per_tick":2}}
	for person in state.actors.values():
		person.skills={"infantry":100,"archery":100,"tactics":100}; person.attributes.intellect=60
	for id in ["garrison.a","garrison.foreign","garrison.z"]:
		var home: bool=id=="garrison.z"; var foreign: bool=id=="garrison.foreign"
		var organization: String="foreign" if foreign else "org"; var owner: Dictionary={"kind":"organization","id":organization}
		var place: String="room" if home else "target.room" if foreign else "away"
		var units: Array=[]
		if home:
			for row in [["unit.z","player","foot",6],["unit.a","manager","bow",4]]:
				units.append({"id":row[0],"troop_type_id":row[2],"commander_actor_id":row[1],"initial_soldiers":row[3],"initial_training":20,"initial_fatigue":7,"equipment":[{"item_id":"spear","quantity_per_person":1}]})
		data.texts[id]=id; data.garrisons[id]={"id":id,"name_key":id,"place_id":place,"organization_id":organization,"inventory_id":"store."+id,"wallet_id":"cash."+id,"food_item_id":"grain","ration_per_person":1,"army_capacity":1000,"units":units}
		data.inventories["store."+id]={"id":"store."+id,"owner":owner.duplicate(),"capacity":10000}; data.wallets["cash."+id]={"id":"cash."+id,"owner":owner.duplicate(),"initial_balance":500}
		state.inventories["store."+id]={"id":"store."+id,"owner":owner.duplicate(),"capacity":10000,"revision":0,"lots":[]}
		for item in ["grain","spear"]: state.inventories["store."+id].lots.append({"id":Factory.next_id(state,"lot"),"item_id":item,"quantity":1000,"owner":owner.duplicate(),"reserved_by":""})
		state.wallets["cash."+id]={"id":"cash."+id,"owner":owner.duplicate(),"balance":500,"reserved":{}}
	for id in ["office.z","office.a"]:
		data.offices[id]={"id":id,"name_key":id,"organization_id":"org","appointing_role_id":"role."+id,"garrison_id":"garrison.z","command_scope":"army" if id=="office.z" else "unit"}; data.texts[id]=id
		data.roles["role."+id]={"id":"role."+id,"place_id":"room","organization_id":"org","services":["office_accept","office_resign","military_order","military_report"]}
		state.roles["role."+id]={"actor_id":"provider","enabled":true}
	data.strongholds.target={"id":"target","garrison_id":"garrison.foreign","settlement_id":"target","name_key":"target"}
	for id in ["order.z","order.a","order.supply"]:
		data.texts[id]=id
		var office: String="office.a" if id=="order.a" else "office.z"
		data.military_orders[id]={"id":id,"name_key":id,"office_id":office,"garrison_id":"garrison.z","issuer_role_id":"role."+office,"report_role_id":"role."+office,"target_stronghold_id":"target","deadline_ticks":40,"objective":{"kind":"occupy"}}
		if id=="order.supply": data.military_orders[id].objective={"kind":"deliver_supply","item_id":"grain","quantity":6,"recipient":"army"}
	var initialized: Dictionary=Assets.initialize(state,data)
	if not initialized.ok: return initialized
	for unit in state.military_units.values(): unit.merge(Training.initial_profile({"initial_training":20,"initial_fatigue":7}).profile)
	var camp: Dictionary=data.garrisons["garrison.z"]; camp.training_wallet_id="training.wallet"; camp.training_fatigue_per_tick=4
	data.wallets[camp.training_wallet_id]={"id":camp.training_wallet_id,"owner":{"kind":"organization","id":"org"},"initial_balance":0}
	state.wallets[camp.training_wallet_id]={"id":camp.training_wallet_id,"owner":{"kind":"organization","id":"org"},"balance":0,"reserved":{}}
	f.context=Interaction.for_role(state,data,"role.office.z"); f.campaign_id=""; f.army_id=""
	return f

## 原军事开工/完工生成任命与人产凭据，显示门不替它造成功结果。
static func action(f: Dictionary, kind: String, params: Dictionary) -> Dictionary:
	var quote: Dictionary=Assets.preview(f.state,f.data,"player",kind,params)
	if not quote.ok: return quote
	var id: String=Factory.next_id(f.state,"action")
	var current: Dictionary={"id":id,"kind":kind,"actor_id":"player","participants":quote.participants.duplicate(),"start_tick":f.state.tick,"remaining_ticks":1,"params":params.duplicate(true)}
	f.state.actions[id]=current
	for person in current.participants: f.state.actors[person].action_id=id
	var result: Dictionary=Assets.begin(f.state,f.data,current,quote)
	if not result.ok: return result
	current.remaining_ticks=0; result=Assets.finish(f.state,f.data,current)
	if not result.ok or not result.success: return result
	Step.release(f.state,current); f.state.tick+=1
	return result

## 真实任命后由Campaign.apply接受军令，避免只给职位改enabled制造成功报价。
static func accept(f: Dictionary, single: bool=false) -> Dictionary:
	var office: String="office.a" if single else "office.z"
	var result: Dictionary=action(f,"office_accept",{"office_id":office,"context":Interaction.for_role(f.state,f.data,"role."+office)})
	if not result.ok: return result
	result=Campaign.apply(f.state,f.data,"player","campaign_accept",{"template_id":"order.a" if single else "order.z","context":Interaction.for_role(f.state,f.data,"role."+office)})
	if result.ok: f.campaign_id=result.details.campaign_id
	return result

## 原编组生成军团，绑定阶段沿既有Campaign世界协调边界；不执行远征或供应长轨迹。
static func form(f: Dictionary) -> Dictionary:
	var result: Dictionary=accept(f)
	if not result.ok: return result
	result=action(f,"army_form",{"order_id":f.campaign_id,"unit_ids":["unit.z","unit.a"],"food_quantity":80,"money":40})
	if result.ok:
		f.army_id=result.details.army_id; result=Campaign.bind_army(f.state,f.data,f.campaign_id,f.army_id)
	return result

## 训练题面只复用既有小作者见证，不改真实成长或练习时序。
static func add_drill(f: Dictionary) -> void:
	var definition: Dictionary=Strict.parse_text(FileAccess.get_file_as_string("res://../tests/fixtures/drill_challenge/definition.json")).value
	f.data.drill_challenges={definition.id:definition}; f.data.texts.merge(Strict.parse_text(FileAccess.get_file_as_string("res://../tests/fixtures/drill_challenge/texts.json")).value)
	f.data.garrisons["garrison.z"].training_drill_id=definition.id

## 同一真实借调、编组和归营链供旧回执失败后重试同人；前置坏回执仅是显式显示边界。
static func returned_retainer(f: Dictionary) -> Dictionary:
	var result: Dictionary=accept(f)
	if not result.ok: return result
	var quote: Dictionary=Assets.preview(f.state,f.data,"player","army_pledge",{"order_id":f.campaign_id,"estate_id":"estate.z","actor_ids":["guard"]})
	if not quote.ok: return quote
	result=Assets.apply(f.state,f.data,"player","army_pledge",quote)
	if not result.ok: return result
	result=action(f,"army_form",{"order_id":f.campaign_id,"unit_ids":["unit.z"],"retainer_ids":["guard"],"food_quantity":40,"money":0})
	if not result.ok: return result
	f.army_id=result.details.army_id
	result=Campaign.bind_army(f.state,f.data,f.campaign_id,f.army_id)
	if not result.ok: return result
	f.state.facts["earlier-unusable-return"]={"kind":"military_return","army_id":"absent-army","returned_retainers":["guard"]}
	result=action(f,"army_return",{"army_id":f.army_id,"garrison_id":"garrison.z"})
	if not result.ok: return result
	f.state.campaigns[f.campaign_id].status="awaiting_report"; f.state.relations={}
	f.data.required_capabilities.append("companions")
	f.data.roles["role.guard"]={"id":"role.guard","place_id":"room","services":["join_party"],"organization_id":null,"companion_cost_per_tick":6}
	f.state.roles["role.guard"]={"actor_id":"guard","enabled":true}
	return {"ok":true}
