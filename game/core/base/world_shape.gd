## 共有世界形状和逐人物引用检查；保持旧错误交错顺序，不取得领域状态所有权。
extends RefCounted
const Values=preload("res://core/base/value_checks.gd")
const Identity=preload("res://core/actor_identity.gd")
const TerrainJourney=preload("res://core/terrain_journey.gd")
const Terrain=preload("res://core/world_terrain.gd")
const Rules=preload("res://core/rules_migration.gd")

## 保留版本、容器及每个人物全部引用的原序；不按owner重新分批。
static func validate(state: Dictionary, catalog: Dictionary) -> Dictionary:
	if not catalog.get("ok",false): return {"ok":false,"code":"NoCatalog"}
	var data: Dictionary = catalog.data
	var required: Dictionary = {"ruleset_id":TYPE_STRING,"content_id":TYPE_STRING,"content_version":TYPE_STRING,"content_digest":TYPE_STRING,"engine_build":TYPE_STRING,"rng_backend":TYPE_STRING,"scenario_id":TYPE_STRING,"player_actor_id":TYPE_STRING,"tick":TYPE_INT,"revision":TYPE_INT,"next_instance_id":TYPE_INT,"actors":TYPE_DICTIONARY,"roles":TYPE_DICTIONARY,"wallets":TYPE_DICTIONARY,"inventories":TYPE_DICTIONARY,"tasks":TYPE_DICTIONARY,"actions":TYPE_DICTIONARY,"knowledge":TYPE_DICTIONARY,"relations":TYPE_DICTIONARY,"events":TYPE_DICTIONARY,"interactions":TYPE_DICTIONARY,"flags":TYPE_DICTIONARY,"request_sequences":TYPE_DICTIONARY,"results":TYPE_DICTIONARY,"history":TYPE_ARRAY,"rng":TYPE_DICTIONARY}
	if not Values.fields(state,required): return {"ok":false,"code":"InvalidWorldShape"}
	if not Values.fields(state,{"estates":TYPE_DICTIONARY,"family":TYPE_DICTIONARY}): return {"ok":false,"code":"InvalidWorldShape"}
	if state.ruleset_id!=Rules.CURRENT or state.content_id!=catalog.content_id or state.content_version!=catalog.content_version or state.content_digest!=catalog.content_digest:
		return {"ok":false,"code":"IncompatibleContent"}
	if not data.scenarios.has(state.scenario_id):
		return {"ok":false,"code":"InvalidScenario"}
	for key in ["tick","revision","next_instance_id"]:
		if not Values.whole(state[key],1 if key=="next_instance_id" else 0): return {"ok":false,"code":"InvalidCounter"}
	var active_ids: Array=data.get("active_static_actor_ids",data.actors.keys())
	if state.roles.size()!=data.roles.size(): return {"ok":false,"code":"MissingWorldEntities"}
	for id in active_ids:
		if not state.actors.has(id): return {"ok":false,"code":"MissingWorldEntities"}
	for id in state.actors:
		if data.actors.has(id) and not id in active_ids: return {"ok":false,"code":"InactiveStaticActor"}
	for id in state.actors:
		var actor: Variant = state.actors[id]
		var definition: Dictionary=Identity.definition(state,data,id)
		if definition.is_empty() or not Values.fields(actor,{"id":TYPE_STRING,"location":TYPE_DICTIONARY,"alive":TYPE_BOOL,"available":TYPE_BOOL,"hp":TYPE_INT,"fatigue":TYPE_INT,"merit":TYPE_INT,"attributes":TYPE_DICTIONARY,"skills":TYPE_DICTIONARY,"wallet_id":TYPE_STRING,"inventory_id":TYPE_STRING,"action_id":TYPE_STRING}):
			return {"ok":false,"code":"InvalidActor"}
		if actor.id!=id or not Values.whole(actor.hp,0,100) or not Values.whole(actor.fatigue,0,100) or not Values.whole(actor.merit) or not Values.whole(state.request_sequences.get(id)):
			return {"ok":false,"code":"InvalidActorState"}
		if not state.wallets.has(actor.wallet_id) or not state.inventories.has(actor.inventory_id): return {"ok":false,"code":"ActorAssetsMissing"}
		for stat in definition.attributes:
			if not Values.whole(actor.attributes.get(stat),1,110): return {"ok":false,"code":"InvalidAttribute"}
		for skill in definition.skills:
			if not Values.whole(actor.skills.get(skill),0,1200): return {"ok":false,"code":"InvalidSkill"}
		var location: Dictionary = actor.location
		if location.get("kind")=="Settlement":
			if not Values.fields(location,{"settlement_id":TYPE_STRING,"place_id":TYPE_STRING}) or not data.places.has(location.place_id) or data.places[location.place_id].settlement_id!=location.settlement_id:
				return {"ok":false,"code":"InvalidPlace"}
		elif location.get("kind")=="OnRoute":
			if not Values.fields(location,{"edge_id":TYPE_STRING,"progress":TYPE_INT,"origin":TYPE_STRING}) or not data.routes.has(location.edge_id): return {"ok":false,"code":"InvalidRouteLocation"}
			if not Values.whole(location.progress,0,data.routes[location.edge_id].duration_ticks*100) or location.origin!=data.routes[location.edge_id].origin or location.has("place_id"): return {"ok":false,"code":"InvalidRouteLocation"}
		elif location.get("kind")=="Wilderness":
			if not Values.fields(location,{"map_id":TYPE_STRING,"point":TYPE_ARRAY,"origin":TYPE_STRING}) or location.size()!=4 or not data.get("world_maps",{}).has(location.map_id) or location.map_id!=TerrainJourney.map_id(state,data) or not data.settlements.has(location.origin): return {"ok":false,"code":"InvalidWildernessLocation"}
			if not Terrain.point_valid(data.world_maps[location.map_id],location.point,"foot",{}): return {"ok":false,"code":"InvalidWildernessPoint"}
		elif location.get("kind")=="InArmy":
			if not location.get("army_id") is String or location.size()!=2: return {"ok":false,"code":"InvalidArmyLocation"}
		elif location.get("kind")=="Following":
			if not location.get("party_id") is String or location.size()!=2: return {"ok":false,"code":"InvalidPartyLocation"}
		else: return {"ok":false,"code":"InvalidLocationKind"}
		if not state.knowledge.get(id) is Dictionary: return {"ok":false,"code":"InvalidKnowledge"}
		for offer_id in state.knowledge[id]:
			if not data.offers.has(offer_id) or not Values.fields(state.knowledge[id][offer_id],{"source":TYPE_DICTIONARY,"tick":TYPE_INT}): return {"ok":false,"code":"InvalidKnowledge"}
	return {"ok":true}

## 随机流、短结果及全树实例上界只在资源和关系校验后检查。
static func validate_runtime(state: Dictionary, _data: Dictionary) -> Dictionary:
	for name in ["world","event","duel","battle","minigame"]:
		if not Values.fields(state.rng.get(name),{"seed":TYPE_STRING,"state":TYPE_STRING}): return {"ok":false,"code":"InvalidRandomState"}
		for key in ["seed","state"]:
			var number: String = state.rng[name][key]
			if not number.is_valid_int() or str(number.to_int())!=number: return {"ok":false,"code":"InvalidRandomState"}
	for result in state.results.values():
		if not Values.fields(result,{"ok":TYPE_BOOL,"kind":TYPE_STRING,"revision":TYPE_INT,"result_id":TYPE_STRING}): return {"ok":false,"code":"InvalidCachedResult"}
	if _max_instance(state)>=state.next_instance_id: return {"ok":false,"code":"InstanceCounterCollision"}
	return {"ok":true}

## 遍历存档与历史中的已分配身份，防止下一实例计数器重复使用既有ID。
static func _max_instance(value: Variant, depth: int=0) -> int:
	if depth>64: return 2147483647
	if value is String:
		var colon: int=value.find(":")
		if colon<1: return 0
		if value.left(colon) in ["lot","action","task","result","relation","settlement","interaction","party","encounter","battle","social","actor","estate","journey","family","military","army","campaign","contact","battle_round","siege","quote"]:
			var suffix: String=value.substr(colon+1)
			if suffix.is_valid_int(): return suffix.to_int()
	elif value is Array:
		var largest: int=0
		for child in value: largest=maxi(largest,_max_instance(child,depth+1))
		return largest
	elif value is Dictionary:
		var largest: int=0
		for key in value:
			largest=maxi(largest,_max_instance(key,depth+1))
			largest=maxi(largest,_max_instance(value[key],depth+1))
		return largest
	return 0

## 等待是共有的一拍行动；不调用推进或补建缺失载荷。
static func validate_wait(_state: Dictionary, _data: Dictionary, action: Dictionary) -> Dictionary:
	if action.remaining_ticks!=1: return {"ok":false,"code":"InvalidWaitAction"}
	return {"ok":true}
