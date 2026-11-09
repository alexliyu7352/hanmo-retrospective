## 统一作者原型与运行人物身份；出生或收养只能建立同一世界中的独立人物，不替换初始主角。
extends RefCounted
const Appearance=preload("res://core/domains/identity/actor_appearance.gd")
const Factory=preload("res://core/world_factory.gd")
const TimeRules=preload("res://core/base/time_rules.gd")
const TICKS_PER_YEAR=TimeRules.TICKS_PER_YEAR
const MAX_TICK=2147483647
const CREATION_FIELDS={"kind":TYPE_STRING,"prototype_id":TYPE_STRING,"actor_id":TYPE_STRING,"created_tick":TYPE_INT,"birth_tick":TYPE_INT,"display_name":TYPE_STRING,"source":TYPE_STRING}

## 按运行人物解析作者原型的深副本；缺失或错误身份返回空值，动态名字不会写回不可变目录。
static func definition(state: Dictionary, data: Dictionary, actor_id: String) -> Dictionary:
	var actor: Variant=state.get("actors",{}).get(actor_id)
	if not actor is Dictionary: return {}
	var prototype: Variant=actor.get("prototype_id",actor_id)
	if not prototype is String or not data.get("actors",{}).has(prototype): return {}
	var result: Dictionary=data.actors[prototype].duplicate(true)
	var name: Variant=actor.get("display_name",data.get("texts",{}).get(result.get("name_key",""),actor_id))
	if not name is String or (actor.has("display_name") and not _valid_name(name)): return {}
	result.id=actor_id; result.display_name=name
	return result

## 按唯一世界半日计算周岁；旧人未迁入出生日期或身份无效时返回-1，不猜测成年年龄。
static func age_years(state: Dictionary, actor_id: String) -> int:
	var actor: Variant=state.get("actors",{}).get(actor_id)
	if not actor is Dictionary or not _whole(actor.get("birth_tick"),-MAX_TICK,state.tick): return -1
	return TimeRules.age_years(state.tick,actor.birth_tick)

## 在已授权候选中根据永久出生或收养来源创建真人；失败不修改状态，同一来源重放只返回原人物。
static func instantiate(state: Dictionary, data: Dictionary, prototype_id: Variant, display_name: Variant, birth_tick: Variant, place_id: Variant, source: Variant) -> Dictionary:
	if not prototype_id is String or not data.get("actors",{}).has(prototype_id): return _fail("UnknownActorPrototype")
	if not _valid_name(display_name): return _fail("InvalidActorDisplayName")
	if not _whole(birth_tick,-MAX_TICK,state.tick): return _fail("InvalidActorBirthTick")
	if not place_id is String or not data.get("places",{}).has(place_id): return _fail("InvalidActorCreationPlace")
	if not _valid_source(state,source): return _fail("InvalidActorCreationSource")
	var valid: Dictionary=validate(state,data)
	if not valid.ok: return valid
	for id in state.facts:
		var fact: Variant=state.facts[id]
		if not fact is Dictionary or fact.get("kind")!="actor_creation" or fact.source!=source: continue
		if fact.prototype_id!=prototype_id or fact.display_name!=display_name or fact.birth_tick!=birth_tick: return _fail("ActorCreationSourceConflict")
		return {"ok":true,"actor_id":id,"created":false}
	if state.next_instance_id>=MAX_TICK: return _fail("IdentityCounterExhausted")
	var next: String="actor:"+str(state.next_instance_id)
	var wallet_id: String="personal:"+next
	var inventory_id: String="bag:"+next
	if state.actors.has(next) or state.facts.has(next) or state.wallets.has(wallet_id) or state.inventories.has(inventory_id) or state.knowledge.has(next) or state.request_sequences.has(next): return _fail("ActorIdentityCollision")
	var prototype: Dictionary=data.actors[prototype_id]
	var seed: String=""
	if state.get("rng") is Dictionary:
		var world_stream: Variant=state.rng.get("world")
		if world_stream is Dictionary and world_stream.get("seed") is String: seed=world_stream.seed
	var appearance: Dictionary=Appearance.prepare(next,seed,prototype)
	if not appearance.ok: return appearance
	var actor_id: String=Factory.next_id(state,"actor")
	var owner: Dictionary={"kind":"actor","id":actor_id}
	state.wallets[wallet_id]={"id":wallet_id,"owner":owner.duplicate(),"balance":0,"reserved":{}}
	state.inventories[inventory_id]={"id":inventory_id,"owner":owner.duplicate(),"capacity":20,"revision":0,"lots":[]}
	state.actors[actor_id]={"id":actor_id,"prototype_id":prototype_id,"display_name":display_name,"birth_tick":birth_tick,"created_tick":state.tick,"location":Factory.settlement_location(data,place_id),"alive":true,"available":true,"hp":100,"fatigue":0,"merit":0,"attributes":prototype.attributes.duplicate(true),"skills":prototype.skills.duplicate(true),"wallet_id":wallet_id,"inventory_id":inventory_id,"action_id":"","injury":0,"combat":{"weapon_id":"","learned":{},"heart_id":""}}
	state.actors[actor_id].appearance=appearance.profile
	state.knowledge[actor_id]={}; state.request_sequences[actor_id]=0
	if state.has("social"): state.social.memories[actor_id]={}
	state.facts[actor_id]={"kind":"actor_creation","prototype_id":prototype_id,"actor_id":actor_id,"created_tick":state.tick,"birth_tick":birth_tick,"display_name":display_name,"source":source}
	return {"ok":true,"actor_id":actor_id,"created":true}

## 校验原型、永久创建依据和私产独立性；仅验证身份，不授予当前玩家控制权或继任合法性。
static func validate(state: Dictionary, data: Dictionary) -> Dictionary:
	if not _fields(state,{"actors":TYPE_DICTIONARY,"facts":TYPE_DICTIONARY,"wallets":TYPE_DICTIONARY,"inventories":TYPE_DICTIONARY,"knowledge":TYPE_DICTIONARY,"request_sequences":TYPE_DICTIONARY,"player_actor_id":TYPE_STRING,"tick":TYPE_INT,"next_instance_id":TYPE_INT}): return _fail("InvalidIdentityState")
	if not _whole(state.tick,0,MAX_TICK) or not _whole(state.next_instance_id,1,MAX_TICK): return _fail("InvalidIdentityCounter")
	if state.has("social") and (not state.social is Dictionary or not state.social.get("memories") is Dictionary): return _fail("InvalidIdentityMemories")
	var active_ids: Array=data.get("active_static_actor_ids",data.actors.keys())
	for id in active_ids:
		if not state.actors.has(id): return _fail("MissingBaseActor")
	var wallets: Dictionary={}
	var inventories: Dictionary={}
	var sources: Dictionary={}
	for id in state.actors:
		var actor: Variant=state.actors[id]
		if not id is String or not _fields(actor,{"id":TYPE_STRING,"wallet_id":TYPE_STRING,"inventory_id":TYPE_STRING}) or actor.id!=id: return _fail("InvalidActorIdentity")
		var dynamic: bool=not data.actors.has(id)
		if dynamic:
			if not _instance_id(id,state.next_instance_id) or not _fields(actor,{"prototype_id":TYPE_STRING,"display_name":TYPE_STRING,"created_tick":TYPE_INT,"birth_tick":TYPE_INT}): return _fail("InvalidDynamicActor")
			if not data.actors.has(actor.prototype_id) or not _valid_name(actor.display_name) or not _whole(actor.created_tick,0,state.tick) or not _whole(actor.birth_tick,-MAX_TICK,actor.created_tick): return _fail("InvalidDynamicActor")
			var fact: Variant=state.facts.get(id)
			if not _fields(fact,CREATION_FIELDS) or fact.size()!=CREATION_FIELDS.size() or fact.kind!="actor_creation" or fact.actor_id!=id or fact.prototype_id!=actor.prototype_id or fact.display_name!=actor.display_name or fact.birth_tick!=actor.birth_tick or fact.created_tick!=actor.created_tick: return _fail("ActorCreationMismatch")
			if not _valid_source(state,fact.source) or sources.has(fact.source): return _fail("InvalidActorCreationSource")
			sources[fact.source]=id
			if actor.wallet_id!="personal:"+id or actor.inventory_id!="bag:"+id: return _fail("DynamicActorAssetsMismatch")
		else:
			if not id in active_ids: return _fail("InactiveStaticActor")
			if actor.get("prototype_id",id)!=id: return _fail("BaseActorPrototypeMismatch")
			if data.actors[id].has("initial_age_years") and (actor.get("birth_tick")!=-data.actors[id].initial_age_years*TICKS_PER_YEAR or actor.get("created_tick")!=0): return _fail("BaseActorAgeMismatch")
			if actor.has("display_name") and not _valid_name(actor.display_name): return _fail("InvalidActorDisplayName")
			if actor.has("created_tick") and not _whole(actor.created_tick,0,0): return _fail("InvalidBaseActorDate")
			if actor.has("birth_tick") and not _whole(actor.birth_tick,-MAX_TICK,0): return _fail("InvalidBaseActorDate")
		if definition(state,data,id).is_empty(): return _fail("InvalidActorDefinition")
		if not actor.get("appearance") is Dictionary: return _fail("InvalidActorAppearance")
		var appearance: Dictionary=Appearance.validate(actor.appearance)
		if not appearance.ok: return appearance
		for parent in actor.appearance.parent_sources:
			if parent.source=="parent" and (not state.actors.has(parent.actor_id) or parent.actor_id==id): return _fail("InvalidAppearanceParents")
		var owner: Dictionary={"kind":"actor","id":id}
		var wallet: Variant=state.wallets.get(actor.wallet_id)
		var inventory: Variant=state.inventories.get(actor.inventory_id)
		if not wallet is Dictionary or not inventory is Dictionary or wallet.get("id")!=actor.wallet_id or inventory.get("id")!=actor.inventory_id or wallet.get("owner")!=owner or inventory.get("owner")!=owner: return _fail("IdentityAssetOwnership")
		if wallets.has(actor.wallet_id) or inventories.has(actor.inventory_id): return _fail("SharedIdentityAssets")
		wallets[actor.wallet_id]=id; inventories[actor.inventory_id]=id
		if not state.knowledge.get(id) is Dictionary or not _whole(state.request_sequences.get(id),0,MAX_TICK): return _fail("MissingIdentityKnowledge")
		if state.has("social") and state.social.memories.has(id) and not state.social.memories[id] is Dictionary: return _fail("InvalidIdentityMemories")
	for id in state.facts:
		var fact: Variant=state.facts[id]
		if fact is Dictionary and fact.get("kind")=="actor_creation" and (not state.actors.has(id) or data.actors.has(id)): return _fail("OrphanActorCreation")
	if not state.actors.has(state.player_actor_id) or definition(state,data,state.player_actor_id).is_empty(): return _fail("InvalidPlayerIdentity")
	return {"ok":true}

## 创建来源必须是调用者已写入的永久出生或收养事实；不接受请求回执、临时标志或另一人物的创建记录。
static func _valid_source(state: Dictionary, source: Variant) -> bool:
	if not source is String or source.is_empty(): return false
	var facts: Variant=state.get("facts")
	if not facts is Dictionary: return false
	var fact: Variant=facts.get(source)
	return fact is Dictionary and fact.get("kind") in ["family_adoption","family_birth"]

## 显示名须为简洁可显示文本；拒绝空白、控制字符和超长名字，名字从不充当引用键。
static func _valid_name(value: Variant) -> bool:
	if not value is String or value.is_empty() or value.length()>80 or value!=value.strip_edges(): return false
	for index in range(value.length()):
		if value.unicode_at(index)<32 or value.unicode_at(index)==127: return false
	return true

## 在解引用前检查字段和类型，允许其他领域在人物上增加自己的运行状态。
static func _fields(value: Variant, fields: Dictionary) -> bool:
	if not value is Dictionary: return false
	for key in fields:
		if typeof(value.get(key))!=fields[key]: return false
	return true

## 严格保留有界整数日期，负出生日期表示开局以前出生，不接受小数静默取整。
static func _whole(value: Variant, low: int, high: int) -> bool:
	return typeof(value)==TYPE_INT and value>=low and value<=high

## 动态人物编号来自全局实例计数器，不能借存档添加不规范或未来编号。
static func _instance_id(id: String, next_id: int) -> bool:
	if not id.begins_with("actor:"): return false
	var suffix: String=id.trim_prefix("actor:")
	return suffix.is_valid_int() and str(int(suffix))==suffix and int(suffix)>0 and int(suffix)<next_id

## 把身份错误交回事务调用方，核心不会通过失败结果继续提交候选。
static func _fail(code: String) -> Dictionary:
	return {"ok":false,"code":code}
