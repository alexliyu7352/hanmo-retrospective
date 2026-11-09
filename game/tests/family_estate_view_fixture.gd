## 家事庄务显示夹具：小领域输入沿真实身份、收养、指定与庄务owner准备，不冒充完整世界Schema或存档。
extends RefCounted
const Appearance=preload("res://core/domains/identity/actor_appearance.gd")
const Family=preload("res://core/family_rules.gd")
const Estate=preload("res://core/estate_rules.gd")
const Rest=preload("res://core/long_rest.gd")
const Factory=preload("res://core/world_factory.gd")
const Inventory=preload("res://core/inventory_rules.gd")
const Interaction=preload("res://core/interaction_rules.gd")

## 每人资产容器和身份独立，两个庄园仅提供已有页实际读取的同形来源。
static func create() -> Dictionary:
	var state: Dictionary={"tick":0,"revision":7,"scenario_id":"fixture","player_actor_id":"player","next_instance_id":1,"actors":{},"roles":{},"wallets":{},"inventories":{},"facts":{},"actions":{},"knowledge":{},"request_sequences":{},"battles":{},"sieges":{},"interactions":{},"encounters":{},"tasks":{},"itineraries":{},"rng":{"world":{"seed":"family-page-fixture","state":"unchanged"}}}
	var data: Dictionary={"required_capabilities":["family","estates"],"scenarios":{"fixture":{"player_actor":"player"}},"actors":{},"roles":{},"places":{},"settlements":{"town":{"id":"town","entry_place":"room"}},"items":{},"inventories":{},"wallets":{},"estates":{},"organizations":{},"offers":{},"texts":{}}
	for place in ["room","away"]:
		data.places[place]={"id":place,"name_key":place,"settlement_id":"town","parent_place":null,"passages":[]}; data.texts[place]="庄院" if place=="room" else "远庄"
	for id in ["player","manager","worker","guard","absent","other","provider","prototype"]:
		var owner: Dictionary={"kind":"actor","id":id}
		data.actors[id]={"id":id,"name_key":"name."+id,"attributes":{"martial":60,"administration":60},"skills":{},"portrait_kind":"adult.dedicated.a","presentation_profile_id":"fixture.person"}; data.texts["name."+id]="姓名"+id
		state.actors[id]={"id":id,"birth_tick":-42*720,"created_tick":0,"location":{"kind":"Settlement","settlement_id":"town","place_id":"away" if id in ["absent","other"] else "room"},"alive":true,"available":true,"hp":100,"fatigue":12,"injury":0,"action_id":"","wallet_id":"wallet."+id,"inventory_id":"bag."+id,"attributes":{"martial":60,"administration":60},"skills":{},"appearance":Appearance.legacy()}
		state.wallets["wallet."+id]={"id":"wallet."+id,"owner":owner.duplicate(),"balance":200,"reserved":{}}
		state.inventories["bag."+id]={"id":"bag."+id,"owner":owner.duplicate(),"capacity":1000,"revision":0,"lots":[]}
		state.knowledge[id]={}; state.request_sequences[id]=0
	state.actors.player.display_name="本局庄主"; state.actors.manager.display_name="实际管事"
	data.texts.child="阿宁"
	for role in ["role.z.family","role.a.family"]:
		data.roles[role]={"id":role,"place_id":"room","initial_actor":"other","services":["family_adopt"],"organization_id":null,"family":{"prototype_id":"prototype","child_name_key":"child","child_age_years":16,"fee":12,"wallet_id":"wallet.provider","adoption_ticks":2,"guardian_min_age":30,"heir_min_age":18,"retirement_min_age":40}}
		state.roles[role]={"actor_id":"provider","enabled":true}
	for id in ["grain","wood"]:
		data.items[id]={"id":id,"name_key":id,"kind":"commodity","weight":1 if id=="grain" else 2,"base_price":5}; data.texts[id]="粮食" if id=="grain" else "木料"
	for id in ["estate.z","estate.a"]:
		var local: bool=id=="estate.z"; var owner: Dictionary={"kind":"actor","id":"player"}
		var residents: Array=["worker","manager","absent","guard"] if local else ["other"]
		var duties: Array=[]
		for person in residents: duties.append({"actor_id":person,"duty":"labor" if person=="worker" else "guard" if person in ["absent","guard"] else "idle"})
		data.estates[id]={"id":id,"name_key":id,"place_id":"room" if local else "away","owner_actor_id":"player","manager_actor_id":"manager" if local else "other","resident_actor_ids":residents,"inventory_id":"store."+id,"wallet_id":"cash."+id,"food_item_id":"grain","ration_per_person":1,"output_item_id":"grain","output_per_worker":3,"granary_capacity_per_level":20,"palisade_defense_per_guard":5,"initial_facilities":{"farm":1,"granary":0,"palisade":1},"initial_duties":duties,"construction":{"max_level":3,"build_ticks":1,"money_cost":10,"material_item_id":"wood","material_cost":2,"payee_wallet_id":"wallet.provider"}}
		data.texts[id]="本庄" if local else "异地私庄"
		data.inventories["store."+id]={"capacity":1000}
		state.inventories["store."+id]={"id":"store."+id,"owner":owner.duplicate(),"capacity":1000,"revision":0,"lots":[{"id":Factory.next_id(state,"lot"),"item_id":"grain","quantity":100,"owner":owner.duplicate(),"reserved_by":""},{"id":Factory.next_id(state,"lot"),"item_id":"wood","quantity":20,"owner":owner.duplicate(),"reserved_by":""}]}
		state.wallets["cash."+id]={"id":"cash."+id,"owner":owner.duplicate(),"balance":170,"reserved":{}}
	var checked: Dictionary=Estate.initialize(state,data)
	if not checked.ok: return checked
	checked=Family.initialize(state,data)
	if not checked.ok: return checked
	return {"ok":true,"state":state,"data":data,"context":Interaction.for_role(state,data,"role.z.family"),"child":""}

## 冻结真实收养报价与占用，并由原finish创建新身份；日期只用于此领域夹具，不称世界长期推进。
static func adopt(f: Dictionary, pending: bool=false) -> Dictionary:
	var quote: Dictionary=Family.preview(f.state,f.data,"player","family_adopt",{"context":f.context})
	if not quote.ok: return quote
	var id: String=Factory.next_id(f.state,"action")
	var action: Dictionary={"id":id,"kind":"family_adopt","actor_id":"player","participants":["player","provider"],"start_tick":f.state.tick,"remaining_ticks":quote.duration,"params":{"context":f.context.duplicate(true)}}
	f.state.actions[id]=action
	for person in action.participants: f.state.actors[person].action_id=id
	var result: Dictionary=Family.begin(f.state,f.data,action,quote)
	if not result.ok or pending: return result
	f.state.tick+=quote.duration; action.remaining_ticks=0
	for estate in f.state.estates.values(): estate.last_tick=f.state.tick
	result=Family.finish(f.state,f.data,action)
	if not result.ok or not result.success: return result
	for person in action.participants: f.state.actors[person].action_id=""
	f.state.actions.erase(id); f.child=result.actor_id
	return Family.validate_state(f.state,f.data)

## 只准备自洽的成年时点并用真实指定命令写来源，不把改日期算作实际成长验收。
static func designate(f: Dictionary) -> Dictionary:
	f.state.tick=f.state.actors[f.child].birth_tick+18*720
	for estate in f.state.estates.values(): estate.last_tick=f.state.tick
	var quote: Dictionary=Family.preview(f.state,f.data,"player","family_designate",{"heir_actor_id":f.child})
	if not quote.ok: return quote
	return Family.apply(f.state,f.data,"player","family_designate",quote)

## 当前/完成休整显示来源由原begin、一步口粮与cancel生成；不测试自动推进控制。
static func rest(f: Dictionary, completed: bool=false) -> Dictionary:
	var params: Dictionary={"estate_id":"estate.z","duration":180}
	var quote: Dictionary=Rest.preview(f.state,f.data,"player","long_rest",params)
	if not quote.ok: return quote
	var id: String=Factory.next_id(f.state,"action")
	var action: Dictionary={"id":id,"kind":"long_rest","actor_id":"player","participants":["player"],"start_tick":f.state.tick,"remaining_ticks":180,"params":params}
	f.state.actions[id]=action; f.state.actors.player.action_id=id
	var result: Dictionary=Rest.begin(f.state,f.data,action,quote)
	if not result.ok: return result
	result=Estate.step(f.state,f.data,f.state.tick+1)
	if not result.ok: return result
	result=Rest.step(f.state,f.data,action); f.state.tick+=1
	if not result.ok or not completed: return result
	result=Rest.cancel(f.state,id); f.state.actors.player.action_id=""; f.state.actions.erase(id)
	return result

## 营造显示使用原扣款/材料开工与完工，所有修改仅在专用测试输入，不扩实际交易回归。
static func build(f: Dictionary, completed: bool=false, interrupted: bool=false) -> Dictionary:
	var params: Dictionary={"estate_id":"estate.z","facility":"farm"}
	var quote: Dictionary=Estate.preview(f.state,f.data,"player","estate_build",params)
	if not quote.ok: return quote
	var id: String=Factory.next_id(f.state,"action")
	var action: Dictionary={"id":id,"kind":"estate_build","actor_id":"player","participants":["player"],"start_tick":f.state.tick,"remaining_ticks":quote.duration,"params":params}
	f.state.actions[id]=action; f.state.actors.player.action_id=id
	var result: Dictionary=Estate.begin(f.state,f.data,action,quote)
	if not result.ok or not completed: return result
	while action.remaining_ticks>0:
		result=Estate.step(f.state,f.data,f.state.tick+1)
		if not result.ok: return result
		action.remaining_ticks-=1
		if action.remaining_ticks==0:
			if interrupted: f.state.actors.player.available=false
			result=Estate.finish(f.state,f.data,action)
			f.state.actors.player.available=true
			if not result.ok: return result
		f.state.tick+=1
	Inventory.release(f.state,id); f.state.actors.player.action_id=""; f.state.actions.erase(id)
	return result
