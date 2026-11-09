## 有限NPC社会模拟：只结算当面私产往来与可追溯消息，沿用世界时钟、人物和关系。
extends RefCounted
const Values=preload("res://core/base/value_checks.gd")
const Factory=preload("res://core/world_factory.gd")
const Inventory=preload("res://core/inventory_rules.gd")
const Location=preload("res://core/world_location.gd")
const Behavior=preload("res://core/domains/identity/actor_behavior.gd")
const HELP_TRUST=20
const EVENT_FIELDS={"id":TYPE_STRING,"kind":TYPE_STRING,"tick":TYPE_INT,"place_id":TYPE_STRING,"actor_id":TYPE_STRING,"target_actor_id":TYPE_STRING,"item_id":TYPE_STRING,"quantity":TYPE_INT,"reason":TYPE_STRING,"fact_id":TYPE_STRING}

## 为新世界或旧档补入空的社会记忆；已有记录保持原样，不能借初始化抹掉错误或补算历史。
static func initialize(state: Dictionary, _data: Dictionary) -> void:
	if not state.has("social"):
		state.social={"last_tick":state.tick,"memories":{},"events":{}}

## 读取同一关系表中的单向信任；尚未相识按现有条件规则视为零。
static func trust(state: Dictionary, from_actor: String, to_actor: String) -> int:
	for edge in state.relations.values():
		if edge.from_actor==from_actor and edge.to_actor==to_actor: return edge.trust
	return 0

## 将实际单向信任提供给援助与同行意愿判断；本函数不替代岗位、距离或占用检查。
static func willing_to_help(state: Dictionary, helper: String, requester: String) -> bool:
	return helper!=requester and state.actors.has(helper) and state.actors.has(requester) and trust(state,helper,requester)>=HELP_TRUST

## 在世界增加tick前结算一次半日；相同终点重放无副作用，跳时或倒退明确失败。
static func step(state: Dictionary, data: Dictionary, finish_tick: int) -> Dictionary:
	initialize(state,data)
	var valid: Dictionary=validate_state(state,data)
	if not valid.ok: return valid
	if finish_tick==state.social.last_tick: return {"ok":true,"applied":false,"events":[]}
	if finish_tick!=state.tick+1 or state.social.last_tick!=state.tick: return {"ok":false,"code":"SocialTickMismatch"}
	var memories: Dictionary=state.social.memories.duplicate(true)
	_observe_facts(state,finish_tick)
	var occupied: Dictionary={}
	var emitted: Array=[]
	var actors: Array=Behavior.scheduled_ids(state,data,"social")
	for actor_id in actors:
		var policy: Dictionary=Behavior.definition(state,data,actor_id).get("social",{})
		if policy.is_empty() or finish_tick%policy.interval_ticks!=0: continue
		var target: String=policy.target_actor_id
		if occupied.has(actor_id) or occupied.has(target) or actor_id==target: continue
		if not _available(state,actor_id) or not _available(state,target): continue
		var place: Dictionary=Location.resolve(state,actor_id)
		if place!=Location.resolve(state,target): continue
		var before: int=emitted.size()
		_exchange(state,data,actor_id,target,policy,finish_tick,place.place_id,emitted)
		if policy.share_news: _share(state,memories,actor_id,target,finish_tick,place.place_id,emitted)
		if Behavior.definition(state,data,target).get("social",{}).get("share_news",false): _share(state,memories,target,actor_id,finish_tick,place.place_id,emitted)
		if emitted.size()>before: occupied[actor_id]=true; occupied[target]=true
	state.social.last_tick=finish_tick
	return {"ok":true,"applied":true,"events":emitted}

## 当面问讯完成后只转述一条较早见闻；调用方负责地点、权限和自身行动占用，本函数不替代交互校验。
static func hear_news(state: Dictionary, data: Dictionary, listener_id: String, speaker_id: String, finish_tick: int) -> Dictionary:
	if listener_id==speaker_id or not state.actors.has(listener_id) or not state.actors.has(speaker_id): return {"ok":false,"code":"InvalidSocialParticipants"}
	var valid: Dictionary=validate_state(state,data)
	if not valid.ok: return valid
	if finish_tick!=state.tick: return {"ok":false,"code":"SocialTickMismatch"}
	var emitted: Array=[]
	_share(state,state.social.memories.duplicate(true),speaker_id,listener_id,finish_tick,Location.resolve(state,speaker_id).get("place_id",""),emitted)
	return {"ok":true,"events":emitted}

## 只允许空闲且在据点内的非玩家人物参与；跟随者使用队伍的真实位置，交谈与战斗也占用人物。
static func _available(state: Dictionary, actor_id: String) -> bool:
	if actor_id==state.player_actor_id or not state.actors.has(actor_id): return false
	var actor: Dictionary=state.actors[actor_id]
	if not actor.alive or not actor.available or not actor.action_id.is_empty() or actor.get("injury",0)>=3: return false
	if Location.resolve(state,actor_id).get("kind","")!="Settlement": return false
	for battle in state.get("battles",{}).values():
		if battle.phase!="closed" and battle.units.has(actor_id): return false
	for owner in state.interactions:
		if owner==actor_id or state.interactions[owner].context.get("provider_actor_id","")==actor_id: return false
	return true

## 饥者只能索取或夺取实际粮食，施助者保留至少一份口粮；不存在粮食时不生成替代资源。
static func _exchange(state: Dictionary, data: Dictionary, actor_id: String, target: String, policy: Dictionary, tick: int, place_id: String, emitted: Array) -> void:
	var item_id: String=policy.food_item_id
	var own: int=_food(state,actor_id,item_id)
	var other: int=_food(state,target,item_id)
	if policy.kind=="selfish":
		if own==0 and other>0 and _transfer(state,data,target,actor_id,item_id):
			_change_relation(state,target,actor_id,-30,-10)
			_change_relation(state,actor_id,target,-5,-10)
			emitted.append(_event(state,"theft",tick,place_id,actor_id,target,item_id,1,"shortage",""))
		return
	var helper: String=""
	var requester: String=""
	if own>=2 and other==0: helper=actor_id; requester=target
	elif own==0 and other>=2: helper=target; requester=actor_id
	if helper.is_empty(): return
	var selfish: bool=Behavior.definition(state,data,helper).get("social",{}).get("kind","")=="selfish"
	if selfish or not willing_to_help(state,helper,requester):
		var reason: String="selfish" if selfish else "distrust"
		if not _known_refusal(state,helper,requester,item_id,reason):
			_change_relation(state,requester,helper,-5,-2)
			emitted.append(_event(state,"refusal",tick,place_id,helper,requester,item_id,0,reason,""))
	elif _transfer(state,data,helper,requester,item_id):
		_change_relation(state,requester,helper,10,5)
		_change_relation(state,helper,requester,2,2)
		emitted.append(_event(state,"aid",tick,place_id,helper,requester,item_id,1,"shortage",""))

## 只计算本人随身且未被行动预留的食物，代管物品不构成可支配口粮。
static func _food(state: Dictionary, actor_id: String, item_id: String) -> int:
	return Inventory.count(state.inventories[state.actors[actor_id].inventory_id],item_id,[{"kind":"actor","id":actor_id}])

## 检查真实容积后沿用库存转交；任何失败都不改库存，成功同时移交私有权。
static func _transfer(state: Dictionary, data: Dictionary, from_actor: String, to_actor: String, item_id: String) -> bool:
	var destination: Dictionary=state.inventories[state.actors[to_actor].inventory_id]
	if Inventory.weight(destination,data)+data.items[item_id].weight>destination.capacity: return false
	return Inventory.transfer(state,state.actors[from_actor].inventory_id,destination.id,item_id,1,[{"kind":"actor","id":from_actor}],"",{"kind":"actor","id":to_actor}).ok

## 既有明确拒绝保留为记忆；相同处境不在每个半日制造重复拒绝和无限负面关系。
static func _known_refusal(state: Dictionary, helper: String, requester: String, item_id: String, reason: String) -> bool:
	for id in state.social.memories.get(requester,{}):
		var event: Dictionary=state.social.events.get(id,{})
		if event.get("kind","")=="refusal" and event.actor_id==helper and event.target_actor_id==requester and event.item_id==item_id and event.reason==reason: return true
	return false

## 修改原关系边并夹紧既有数值范围；首次相识从零开始，实例ID仍由世界工厂分配。
static func _change_relation(state: Dictionary, from_actor: String, to_actor: String, trust_delta: int, affinity_delta: int) -> void:
	var relation: Dictionary={}
	for edge in state.relations.values():
		if edge.from_actor==from_actor and edge.to_actor==to_actor: relation=edge; break
	if relation.is_empty():
		var id: String=Factory.next_id(state,"relation")
		relation={"id":id,"from_actor":from_actor,"to_actor":to_actor,"trust":0,"affinity":0}
		state.relations[id]=relation
	relation.trust=clampi(relation.trust+trust_delta,0,100)
	relation.affinity=clampi(relation.affinity+affinity_delta,0,100)

## 写入永久社会事实；实际往来的双方直接见闻，转述记录本身不能变成另一条流言。
static func _event(state: Dictionary, kind: String, tick: int, place_id: String, actor_id: String, target: String, item_id: String, quantity: int, reason: String, fact_id: String) -> String:
	var id: String=Factory.next_id(state,"social")
	state.social.events[id]={"id":id,"kind":kind,"tick":tick,"place_id":place_id,"actor_id":actor_id,"target_actor_id":target,"item_id":item_id,"quantity":quantity,"reason":reason,"fact_id":fact_id}
	if kind!="news":
		_remember(state,actor_id,id,tick,actor_id)
		_remember(state,target,id,tick,target)
	return id

## 记忆只保留第一次获知的时间和来源；再次听见不会改写来源或重复改变关系。
static func _remember(state: Dictionary, actor_id: String, fact_id: String, tick: int, source: String) -> void:
	if not state.social.memories.has(actor_id): state.social.memories[actor_id]={}
	if not state.social.memories[actor_id].has(fact_id): state.social.memories[actor_id][fact_id]={"learned_tick":tick,"source_actor_id":source}

## 每次当面交谈最多转告一条步初已知事实；新消息直到下一半日才可能再传播。
static func _share(state: Dictionary, memories: Dictionary, sender: String, receiver: String, tick: int, place_id: String, emitted: Array) -> void:
	var facts: Array=memories.get(sender,{}).keys(); facts.sort()
	for id in facts:
		if memories[sender][id].learned_tick>=tick: continue
		if state.social.memories.get(receiver,{}).has(id): continue
		_remember(state,receiver,id,tick,sender)
		var fact: Dictionary=state.social.events.get(id,{})
		if fact.get("kind","")=="theft" and receiver!=fact.actor_id: _change_relation(state,receiver,fact.actor_id,-10,-3)
		elif fact.get("kind","")=="aid" and receiver!=fact.actor_id: _change_relation(state,receiver,fact.actor_id,3,1)
		emitted.append(_event(state,"news",tick,place_id,sender,receiver,"",0,"",id))
		return

## 已完成训练和个人战只赋予明确参与者亲历记忆；同城或存在全局事实不等于所有人知情。
static func _observe_facts(state: Dictionary, tick: int) -> void:
	var ids: Array=state.get("facts",{}).keys(); ids.sort()
	for id in ids:
		if not state.facts[id] is Dictionary: continue
		var fact: Dictionary=state.facts[id]
		var fact_tick: Variant=fact.get("completed_tick",fact.get("tick"))
		if not fact_tick is int or fact_tick<0 or fact_tick>tick: continue
		for participant in _participants(fact):
			if state.actors.has(participant): _remember(state,participant,id,tick,participant)

## 从已知领域事实取得亲历者，不猜测未知事件类型或未完成学习的旁观者。
static func _participants(fact: Dictionary) -> Array:
	if fact.get("kind","")=="personal_battle" and fact.get("participants") is Array: return fact.participants
	if fact.get("kind","")=="training" and fact.get("phase","")=="completed":
		if not fact.get("training") is Dictionary or not fact.training.get("student_id") is String or not fact.training.get("teacher_id") is String: return []
		var people: Array=[fact.training.student_id]
		if not fact.training.teacher_id.is_empty(): people.append(fact.training.teacher_id)
		return people
	if fact.get("kind","") in ["aid","theft","refusal"] and fact.get("actor_id") is String and fact.get("target_actor_id") is String: return [fact.actor_id,fact.target_actor_id]
	return []

## 读取可传播的原始事实；转述事件不能冒充其消息源，未知领域事实保持不可见。
static func _fact(state: Dictionary, id: String) -> Dictionary:
	var value: Variant=state.social.events.get(id,state.get("facts",{}).get(id,{}))
	if not value is Dictionary: return {}
	var fact: Dictionary=value
	return fact if not _participants(fact).is_empty() else {}

## 核验社会存档结构、实例来源与消息因果；坏引用或同半日多跳不得靠读档变成全知。
static func validate_state(state: Dictionary, data: Dictionary) -> Dictionary:
	var social: Variant=state.get("social")
	if not _fields(social,{"last_tick":TYPE_INT,"memories":TYPE_DICTIONARY,"events":TYPE_DICTIONARY}): return {"ok":false,"code":"InvalidSocialState"}
	if social.last_tick<0 or social.last_tick!=state.tick: return {"ok":false,"code":"InvalidSocialTick"}
	for id in social.events:
		var event: Variant=social.events[id]
		if not id is String or not _fields(event,EVENT_FIELDS) or event.id!=id or not _instance_id(id,state.next_instance_id): return {"ok":false,"code":"InvalidSocialEvent"}
		if not event.kind in ["aid","theft","refusal","news"] or event.tick<1 or event.tick>social.last_tick: return {"ok":false,"code":"InvalidSocialEvent"}
		if not data.places.has(event.place_id) or event.actor_id==event.target_actor_id or not state.actors.has(event.actor_id) or not state.actors.has(event.target_actor_id): return {"ok":false,"code":"InvalidSocialParticipants"}
		if event.kind=="news":
			if event.item_id!="" or event.quantity!=0 or event.reason!="" or _fact(state,event.fact_id).is_empty(): return {"ok":false,"code":"InvalidSocialNews"}
		elif not data.items.has(event.item_id) or event.fact_id!="" or event.quantity!=(0 if event.kind=="refusal" else 1) or not event.reason in (["distrust","selfish"] if event.kind=="refusal" else ["shortage"]): return {"ok":false,"code":"InvalidSocialTransfer"}
	for actor_id in social.memories:
		if not state.actors.has(actor_id) or not social.memories[actor_id] is Dictionary: return {"ok":false,"code":"InvalidSocialMemory"}
		for id in social.memories[actor_id]:
			var memory: Variant=social.memories[actor_id][id]
			if not id is String or not _fields(memory,{"learned_tick":TYPE_INT,"source_actor_id":TYPE_STRING}): return {"ok":false,"code":"InvalidSocialMemory"}
			var fact: Dictionary=_fact(state,id)
			if fact.is_empty() or not state.actors.has(memory.source_actor_id) or memory.learned_tick<0 or memory.learned_tick>social.last_tick: return {"ok":false,"code":"InvalidSocialMemory"}
			var fact_tick: Variant=fact.get("completed_tick",fact.get("tick"))
			if not fact_tick is int or fact_tick<0 or fact_tick>memory.learned_tick: return {"ok":false,"code":"InvalidSocialMemoryTime"}
			if memory.source_actor_id==actor_id:
				if not actor_id in _participants(fact): return {"ok":false,"code":"InvalidSocialWitness"}
			else:
				var source: Variant=social.memories.get(memory.source_actor_id,{}).get(id)
				if not _fields(source,{"learned_tick":TYPE_INT,"source_actor_id":TYPE_STRING}) or source.learned_tick>=memory.learned_tick or not _transmission(social.events,id,memory.source_actor_id,actor_id,memory.learned_tick): return {"ok":false,"code":"InvalidSocialProvenance"}
	for event in social.events.values():
		if event.kind=="news":
			var memory: Dictionary=social.memories.get(event.target_actor_id,{}).get(event.fact_id,{})
			if memory.get("source_actor_id")!=event.actor_id or memory.get("learned_tick")!=event.tick: return {"ok":false,"code":"InvalidSocialNews"}
		else:
			for participant in [event.actor_id,event.target_actor_id]:
				if social.memories.get(participant,{}).get(event.id,{})!={"learned_tick":event.tick,"source_actor_id":participant}: return {"ok":false,"code":"InvalidSocialWitness"}
	return {"ok":true}

## 转述必须有对应的真实交流记录，不能只伪造一个来源人物ID。
static func _transmission(events: Dictionary, fact_id: String, source: String, receiver: String, tick: int) -> bool:
	for event in events.values():
		if event.kind=="news" and event.fact_id==fact_id and event.actor_id==source and event.target_actor_id==receiver and event.tick==tick: return true
	return false

## 检查精确字段与原始类型，拒绝小数、布尔冒充整数及额外隐藏状态。
static func _fields(value: Variant, fields: Dictionary) -> bool:
	if not value is Dictionary or value.size()!=fields.size(): return false
	for key in fields:
		if typeof(value.get(key))!=fields[key]: return false
	return true

## 社会事件沿用全局实例计数器，拒绝伪造的未来编号或非规范数字。
static func _instance_id(id: String, next_id: int) -> bool:
	if not id.begins_with("social:"): return false
	var suffix: String=id.trim_prefix("social:")
	return suffix.is_valid_int() and str(int(suffix))==suffix and int(suffix)>0 and int(suffix)<next_id

## 关系共有引用保持旧的必需字段子集语义，不替换社会记录的精确形状检查。
static func validate_relations(state: Dictionary, _data: Dictionary) -> Dictionary:
	for edge in state.relations.values():
		if not Values.fields(edge,{"from_actor":TYPE_STRING,"to_actor":TYPE_STRING,"affinity":TYPE_INT,"trust":TYPE_INT}) or not state.actors.has(edge.from_actor) or not state.actors.has(edge.to_actor) or not Values.whole(edge.affinity,0,100) or not Values.whole(edge.trust,0,100): return {"ok":false,"code":"InvalidRelation"}
	return {"ok":true}
