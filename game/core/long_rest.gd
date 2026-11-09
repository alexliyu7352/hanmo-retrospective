## 私庄中的长期休整仍逐半日消费并推进同一世界；取消和缺粮保留已经度过的日期与真实账目。
extends RefCounted
const Inventory=preload("res://core/inventory_rules.gd")
const Location=preload("res://core/world_location.gd")
const KINDS=["long_rest","rest_cancel"]
const MAX_VALUE=2147483647

## 只读核验本人的庄园和当前供养人数，不预付全年粮食、不替世界跳时。
static func preview(state: Dictionary, data: Dictionary, actor_id: String, kind: String, params: Dictionary, own_action: String="") -> Dictionary:
	if kind=="rest_cancel":
		var action: Dictionary=state.actions.get(state.actors[actor_id].action_id,{})
		if action.get("kind")!="long_rest" or action.actor_id!=actor_id: return fail("NotResting")
		return {"ok":true,"duration":0,"action_id":action.id}
	if kind!="long_rest" or not _whole(params.get("duration"),1,720): return fail("InvalidRestDuration")
	var id: Variant=params.get("estate_id")
	if not id is String or not state.get("estates",{}).has(id): return fail("UnknownEstate")
	var estate: Dictionary=state.estates[id]; var definition: Dictionary=data.estates[id]
	var actor: Dictionary=state.actors[actor_id]
	if estate.owner_actor_id!=actor_id: return fail("RestNeedsOwnEstate")
	if not actor.alive or not actor.available or (not actor.action_id.is_empty() and actor.action_id!=own_action): return fail("ActorBusy")
	if Location.resolve(state,actor_id).get("place_id")!=definition.place_id: return fail("RestNotAtEstate")
	if state.interactions.has(actor_id): return fail("RestNeedsQuiet")
	if not Location.party_id(state,actor_id).is_empty(): return fail("RestNeedsQuiet")
	for army in state.get("armies",{}).values():
		if army.get("status")=="active" and actor_id in army.get("member_actor_ids",[]): return fail("RestNeedsQuiet")
	if state.tick>MAX_VALUE-params.duration: return fail("RestClockOverflow")
	var people: Array=mouths(state,data,actor_id,id)
	var quantity: int=people.size()*definition.ration_per_person
	if Inventory.count(state.inventories[definition.inventory_id],definition.food_item_id,[{"kind":"actor","id":actor_id}])<quantity: return fail("RestFoodShortage")
	return {"ok":true,"duration":params.duration,"estate_id":id,"mouths":people,"food_item_id":definition.food_item_id,"ration_per_person":definition.ration_per_person}

## 休整本人由此供粮，即使他也是居民；行动占用已让庄务停用本人，其他居民子女仍由庄务承担。
static func mouths(state: Dictionary, data: Dictionary, actor_id: String, estate_id: String) -> Array:
	var result: Array=[]; var estate: Dictionary=state.estates[estate_id]
	var place_id: String=data.estates[estate_id].place_id
	var candidates: Array=[actor_id]
	for child in state.get("family",{}).get("children",{}):
		var fact: Dictionary=state.facts.get(state.family.children[child],{})
		if fact.get("guardian_actor_id")==actor_id: candidates.append(child)
	candidates.sort()
	for person in candidates:
		var actor: Dictionary=state.actors[person]
		if actor.alive and actor.available and (person==actor_id or not person in estate.resident_actor_ids) and Location.resolve(state,person).get("place_id")==place_id: result.append(person)
	return result

## 开始仅冻结期限与粮食定义；占用由Kernel建立，真实消费留给后续每一个世界半日。
static func begin(state: Dictionary, data: Dictionary, action: Dictionary, quote: Dictionary) -> Dictionary:
	var checked: Dictionary=preview(state,data,action.actor_id,"long_rest",action.params,action.id)
	if not checked.ok: return checked
	if checked!=quote or action.participants!=[action.actor_id] or action.remaining_ticks!=quote.duration: return fail("StaleRestQuote")
	action.params={"estate_id":quote.estate_id,"duration":quote.duration}
	state.facts[action.id]={"kind":"long_rest","actor_id":action.actor_id,"estate_id":quote.estate_id,"start_tick":state.tick,"duration":quote.duration,"elapsed":0,"phase":"active","completed_tick":-1,"food_item_id":quote.food_item_id,"ration_per_person":quote.ration_per_person,"consumed":0,"last_mouths":[],"stop_reason":""}
	return {"ok":true}

## 庄务已结算本半日后调用；缺粮明确结束休整，已有消费、NPC行动和全局日期不回滚重算。
static func step(state: Dictionary, data: Dictionary, action: Dictionary) -> Dictionary:
	var fact: Dictionary=state.facts[action.id]
	var definition: Dictionary=data.estates[action.params.estate_id]
	var people: Array=mouths(state,data,action.actor_id,action.params.estate_id)
	var quantity: int=people.size()*fact.ration_per_person
	var owner: Dictionary={"kind":"actor","id":action.actor_id}
	var reason: String=""
	if state.estates[fact.estate_id].owner_actor_id!=action.actor_id or Location.resolve(state,action.actor_id).get("place_id")!=definition.place_id or not state.actors[action.actor_id].alive or not state.actors[action.actor_id].available: reason="RestInterrupted"
	elif Inventory.count(state.inventories[definition.inventory_id],fact.food_item_id,[owner])<quantity: reason="RestFoodShortage"
	fact.elapsed+=1; action.remaining_ticks-=1
	fact.last_mouths=[]
	if reason.is_empty():
		_consume(state.inventories[definition.inventory_id],fact.food_item_id,quantity,owner)
		fact.consumed+=quantity; fact.last_mouths=people
		for person in people:
			state.actors[person].fatigue=maxi(0,state.actors[person].fatigue-4)
			state.actors[person].hp=mini(100,state.actors[person].hp+1)
	else: action.remaining_ticks=0
	var completed: bool=action.remaining_ticks==0
	if completed:
		fact.phase="completed"; fact.completed_tick=state.tick+1
		fact.stop_reason=reason if not reason.is_empty() else "duration"
	return {"ok":true,"completed":completed,"success":reason.is_empty(),"code":reason}

## 明确结束本人现有休整，不增加日期或返还已经吃掉的粮食；人物占用由Kernel随后释放。
static func cancel(state: Dictionary, action_id: String) -> Dictionary:
	var fact: Dictionary=state.facts[action_id]
	fact.phase="completed"; fact.completed_tick=state.tick; fact.stop_reason="cancelled"
	return {"ok":true,"details":{"action_id":action_id,"elapsed":fact.elapsed}}

## 保存时核对活动期限与永久账目，不接受跳过半日、复用结束行动或更换私庄。
static func validate_action(state: Dictionary, data: Dictionary, action: Dictionary) -> Dictionary:
	var fact: Variant=state.facts.get(action.id)
	if not fact is Dictionary or fact.get("kind")!="long_rest" or fact.get("phase")!="active": return fail("InvalidRestAction")
	if action.params!={"estate_id":fact.estate_id,"duration":fact.duration} or action.participants!=[action.actor_id] or action.actor_id!=fact.actor_id or action.start_tick!=fact.start_tick or action.remaining_ticks!=fact.duration-fact.elapsed or state.tick!=fact.start_tick+fact.elapsed: return fail("RestClockMismatch")
	if state.estates.get(fact.estate_id,{}).get("owner_actor_id")!=action.actor_id or Location.resolve(state,action.actor_id).get("place_id")!=data.estates[fact.estate_id].place_id: return fail("RestNotAtEstate")
	return {"ok":true}

## 保留完整休整来源与累计真实消费，完成记录独立于会被裁剪的短命令历史。
static func validate_state(state: Dictionary, data: Dictionary) -> Dictionary:
	for id in state.facts:
		var fact: Variant=state.facts[id]
		if not fact is Dictionary or fact.get("kind")!="long_rest": continue
		var required: Dictionary={"actor_id":TYPE_STRING,"estate_id":TYPE_STRING,"phase":TYPE_STRING,"completed_tick":TYPE_INT,"stop_reason":TYPE_STRING}
		for key in required:
			if typeof(fact.get(key))!=required[key]: return fail("InvalidRestFact")
		if not id is String or not id.begins_with("action:") or not state.actors.has(fact.get("actor_id")) or not data.get("estates",{}).has(fact.get("estate_id")): return fail("InvalidRestFact")
		if not _whole(fact.get("duration"),1,720) or not _whole(fact.get("elapsed"),0,fact.duration) or not _whole(fact.get("start_tick"),0,state.tick) or not _whole(fact.get("consumed"),0,MAX_VALUE): return fail("InvalidRestFact")
		var definition: Dictionary=data.estates[fact.estate_id]
		if fact.get("food_item_id")!=definition.food_item_id or fact.get("ration_per_person")!=definition.ration_per_person or not fact.get("last_mouths") is Array: return fail("InvalidRestFood")
		var seen: Array=[]
		for person in fact.last_mouths:
			if not person is String or not state.actors.has(person) or person in seen: return fail("InvalidRestMouths")
			seen.append(person)
		if fact.phase=="active":
			if not state.actions.has(id) or fact.get("completed_tick")!=-1 or fact.get("stop_reason")!="": return fail("OrphanRestAction")
			var checked: Dictionary=validate_action(state,data,state.actions[id])
			if not checked.ok: return checked
		elif fact.phase=="completed":
			if state.actions.has(id) or fact.get("completed_tick")!=fact.start_tick+fact.elapsed or fact.completed_tick>state.tick or not fact.get("stop_reason") in ["duration","cancelled","RestFoodShortage","RestInterrupted"]: return fail("InvalidRestCompletion")
			if fact.stop_reason=="duration" and fact.elapsed!=fact.duration: return fail("RestClockMismatch")
		else: return fail("InvalidRestPhase")
	return {"ok":true}

## 消耗已由数量查询确认的真实未预留私粮，不建立另一份仓储或产生负批次。
static func _consume(inventory: Dictionary, item_id: String, quantity: int, owner: Dictionary) -> void:
	var left: int=quantity
	for lot in inventory.lots:
		if lot.item_id!=item_id or lot.owner!=owner or not lot.reserved_by.is_empty(): continue
		var taken: int=mini(left,lot.quantity); lot.quantity-=taken; left-=taken
		if left==0: break
	inventory.lots=inventory.lots.filter(func(lot): return lot.quantity>0)
	if quantity>0: inventory.revision+=1

## 限制持久日期与数量为整数，避免小数或溢出破坏逐半日账目。
static func _whole(value: Variant, low: int, high: int) -> bool:
	return typeof(value)==TYPE_INT and value>=low and value<=high

## 失败交还唯一事务入口；领域不自行写入玩家当前存档。
static func fail(code: String) -> Dictionary:
	return {"ok":false,"code":code}
