## 单庄园指定页的有限输入；在场可看不等于有权办理，所有钱货与任职资格仍交原preview。
extends RefCounted
const Estate=preload("res://core/estate_rules.gd")
const Inventory=preload("res://core/inventory_rules.gd")
const Location=preload("res://core/world_location.gd")
const Identity=preload("res://core/actor_identity.gd")
const CharacterInput=preload("res://core/views/character_input.gd")

## 保原存在且同place_id探针，不增加庄主/管事限制，不提前读取画像、仓库或事实。
static func available(state: Dictionary, data: Dictionary, estate_id: String) -> bool:
	return state.get("estates",{}).has(estate_id) and data.get("estates",{}).has(estate_id) and Location.resolve(state,state.player_actor_id).get("place_id")==data.estates[estate_id].place_id

## 先沿原在场短路，再把既有Estate汇总裁成显示标量；分tab读取，绝不返回整庄园或生产事实。
static func query(state: Dictionary, data: Dictionary, estate_id: String, tab: String="people") -> Dictionary:
	if not available(state,data,estate_id): return {"available":false}
	var estate: Dictionary=state.estates[estate_id]; var definition: Dictionary=data.estates[estate_id]
	var observed: Dictionary=Estate.view(state,data,estate_id)
	var view: Dictionary={"available":true,"estate_id":estate_id,"owner":{"actor_id":estate.owner_actor_id,"display_name":_person(state,data,estate.owner_actor_id)},"manager":{},"workforce":{"workers_count":observed.available_workers.size(),"guards_count":observed.available_guards.size(),"defense":observed.defense},"last_production":{},"activity":{},"completion":{}}
	if not estate.manager_actor_id.is_empty(): view.manager={"actor_id":estate.manager_actor_id,"display_name":_person(state,data,estate.manager_actor_id),"portrait_input":CharacterInput.query(state,data,estate.manager_actor_id)}
	var last: Dictionary=observed.last_production
	if not last.is_empty(): view.last_production={"produced_quantity":last.produced.quantity,"consumed_quantity":last.consumed.quantity,"lost_output":last.lost_output,"unfed_count":last.unfed.size()}
	var actor: Dictionary=state.actors[state.player_actor_id]
	if not actor.action_id.is_empty():
		var action: Dictionary=state.actions[actor.action_id]
		view.activity={"kind":action.kind,"remaining_ticks":action.remaining_ticks}
		if action.kind=="estate_build": view.activity.facility=action.params.terms.facility
	else: view.completion=_latest_construction(state,estate_id)
	match tab:
		"build":
			view.facilities=[]
			for facility in Estate.FACILITIES: view.facilities.append({"facility_id":facility,"level":estate.facilities[facility]})
		"storage":
			var store: Dictionary=state.inventories[definition.inventory_id]
			var owner: Dictionary={"kind":"actor","id":estate.owner_actor_id}
			var items: Array=[definition.food_item_id]
			if not definition.construction.material_item_id in items: items.append(definition.construction.material_item_id)
			view.storage={"wallet_balance":state.wallets[definition.wallet_id].balance,"weight":Inventory.weight(store,data),"capacity":store.capacity,"items":[]}
			for item in items: view.storage.items.append({"item_id":item,"available_quantity":Inventory.count(store,item,[owner])})
		_:
			view.residents=[]
			for person in estate.resident_actor_ids:
				view.residents.append({"actor_id":person,"display_name":_person(state,data,person),"absent":Location.resolve(state,person).get("place_id")!=definition.place_id,"duty":estate.duties[person],"detached":estate.detached.has(person),"source_id":estate.detached.get(person,"")})
	return view.duplicate(true)

## 无当前行动时才找本庄最近完成建设；同tick保先遇者，不隐藏失败或限定为当前本人。
static func _latest_construction(state: Dictionary, estate_id: String) -> Dictionary:
	var latest: Dictionary={}
	for fact in state.facts.values():
		if fact.get("kind")!="estate_construction" or fact.get("phase")!="completed" or fact.terms.estate_id!=estate_id: continue
		if latest.is_empty() or fact.completed_tick>latest.completed_tick: latest=fact
	return {} if latest.is_empty() else {"facility":latest.terms.facility,"to_level":latest.terms.to_level,"success":latest.success}

## 复用原Main._actor/姓名表达式，画像失败不阻断庄主、管事和居民名称。
static func _person(state: Dictionary, data: Dictionary, id: String) -> String:
	var record: Dictionary=Identity.definition(state,data,id)
	return record.get("display_name",data.texts.get(record.get("name_key",record.get("title_key","")),record.get("id","")))
