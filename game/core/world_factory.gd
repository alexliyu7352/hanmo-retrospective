## 只根据已校验的内容目录与剧本创建新世界，不使用界面或固定人物数据。
extends RefCounted
const Appearance=preload("res://core/domains/identity/actor_appearance.gd")
const Rules = preload("res://core/rules_migration.gd")
const Snapshot=preload("res://core/scenario_snapshot.gd")
const TimeRules=preload("res://core/base/time_rules.gd")

## 建立独立人物、岗位、钱包和库存实例，不在初始化时执行剧情副作用。
static func create(catalog: Dictionary, scenario_id: String, appearance_draft: Dictionary={}, player_actor_id: String="") -> Dictionary:
	var resolved: Dictionary=Snapshot.resolve(catalog,scenario_id,player_actor_id)
	if not resolved.ok: return resolved
	var effective: Dictionary=resolved.catalog
	var initial: Dictionary=resolved.initial
	var data: Dictionary = effective.data
	var scenario: Dictionary = data.scenarios[scenario_id]
	var build: String = str(Engine.get_version_info().get("string","unknown"))
	var state: Dictionary = {"ruleset_id":Rules.CURRENT,"content_id":catalog.content_id,"content_version":catalog.content_version,"content_digest":catalog.content_digest,"engine_build":build,"rng_backend":"godot:"+build,"scenario_id":scenario_id,
		"player_actor_id":initial.player_actor_id,"tick":0,"revision":0,"next_instance_id":1,"actors":{},"parties":{},"encounters":{},"itineraries":{},"battles":{},"facts":{},"roles":{},"wallets":{},"inventories":{},"tasks":{},"actions":{},"knowledge":{},"relations":{},"events":{},"interactions":{},"flags":data.flags.duplicate(true),"request_sequences":{},"results":{},"history":[],"rng":_random_streams(scenario.seed_text)}
	for id in data.wallets:
		state.wallets[id]={"id":id,"owner":data.wallets[id].owner.duplicate(true),"balance":data.wallets[id].initial_balance,"reserved":{}}
	for id in data.inventories:
		var definition: Dictionary = data.inventories[id]
		state.inventories[id]={"id":id,"owner":definition.owner.duplicate(true),"capacity":definition.capacity,"revision":0,"lots":[]}
		for good in definition.goods:
			state.inventories[id].lots.append({"id":next_id(state,"lot"),"item_id":good.item_id,"quantity":good.quantity,"owner":definition.owner.duplicate(true),"reserved_by":""})
	for row in initial.actors:
		var id: String=row.actor_id
		var definition: Dictionary = data.actors[id]
		var appearance: Dictionary=Appearance.prepare(id,state.rng.world.seed,definition,appearance_draft if id==state.player_actor_id else {})
		if not appearance.ok: return appearance
		var owner: Dictionary = {"kind":"actor","id":id}
		var wallet_id: String = ""
		var wallet_ids: Array = state.wallets.keys()
		wallet_ids.sort()
		var estate_wallets: Array=[]
		for estate in data.get("estates",{}).values(): estate_wallets.append(estate.wallet_id)
		for candidate in wallet_ids:
			if state.wallets[candidate].owner==owner and not candidate in estate_wallets:
				wallet_id=candidate
				break
		if wallet_id.is_empty():
			wallet_id="personal:"+id
			state.wallets[wallet_id]={"id":wallet_id,"owner":owner.duplicate(),"balance":0,"reserved":{}}
		var bag_id: String = "bag:"+id
		state.inventories[bag_id]={"id":bag_id,"owner":owner.duplicate(),"capacity":20,"revision":0,"lots":[]}
		state.actors[id]={"id":id,"location":settlement_location(data,definition.initial_place),"alive":row.alive,"available":row.available,"hp":100,"fatigue":0,"merit":0,"attributes":definition.attributes.duplicate(true),"skills":definition.skills.duplicate(true),"wallet_id":wallet_id,"inventory_id":bag_id,"action_id":""}
		state.actors[id].appearance=appearance.profile
		state.actors[id].injury=0
		if definition.has("initial_age_years"):
			state.actors[id].birth_tick=-definition.initial_age_years*TimeRules.TICKS_PER_YEAR
			state.actors[id].created_tick=0
		state.actors[id].combat={"weapon_id":definition.get("combat",{}).get("weapon_id",""),"learned":{},"heart_id":""}
		if definition.get("combat",{}).has("armor_id"): state.actors[id].combat.armor_id=definition.combat.armor_id
		for learned in definition.get("combat",{}).get("learned",[]):
			state.actors[id].combat.learned[learned.skill_id]=learned.mastery
		for good in definition.get("initial_goods",[]) if initial.legacy else row.get("initial_goods",[]):
			state.inventories[bag_id].lots.append({"id":next_id(state,"lot"),"item_id":good.item_id,"quantity":good.quantity,"owner":owner.duplicate(),"reserved_by":""})
		if definition.has("travel_plan"):
			state.itineraries[id]={"index":0,"ready_tick":definition.travel_plan.start_tick,"travelling":false}
		state.request_sequences[id]=0
		state.knowledge[id]={}
	for id in data.organizations:
		var store_id: String = "store:"+id
		state.inventories[store_id]={"id":store_id,"owner":{"kind":"organization","id":id},"capacity":2147483647,"revision":0,"lots":[]}
	var initial_roles: Dictionary={}
	for row in initial.get("roles",[]): initial_roles[row.role_id]=row
	for id in data.roles:
		state.roles[id]={"actor_id":data.roles[id].initial_actor,"enabled":true if initial.legacy else initial_roles[id].enabled}
	for id in data.relations:
		state.relations[id]=data.relations[id].duplicate(true)
	state.social={"last_tick":state.tick,"memories":{},"events":{}}
	state.family={"founder_actor_id":state.player_actor_id,"children":{},"designations":{},"successions":[]}
	return {"ok":true,"state":state,"catalog":effective}

## 构造唯一的据点与场所位置，避免额外维护可独立变化的坐标副本。
static func settlement_location(data: Dictionary, place_id: String) -> Dictionary:
	return {"kind":"Settlement","settlement_id":data.places[place_id].settlement_id,"place_id":place_id}

## 从可保存计数器分配实例ID，不依赖显示名称或现实时间。
static func next_id(state: Dictionary, prefix: String) -> String:
	var id: String = prefix+":"+str(state.next_instance_id)
	state.next_instance_id+=1
	return id

## 沿用相互独立的五条随机流，以十进制字符串无损保存64位种子与状态。
static func _random_streams(seed_text: String) -> Dictionary:
	var streams: Dictionary = {}
	for stream in ["world","event","duel","battle","minigame"]:
		var hashing = HashingContext.new()
		hashing.start(HashingContext.HASH_SHA256)
		hashing.update((seed_text+":"+stream).to_utf8_buffer())
		var bytes: PackedByteArray = hashing.finish()
		var value: int = int(bytes[0]) & 0x7f
		for index in range(1,8):
			value=(value << 8) | int(bytes[index])
		var rng = RandomNumberGenerator.new()
		rng.seed=value
		streams[stream]={"seed":str(rng.seed),"state":str(rng.state)}
	return streams
