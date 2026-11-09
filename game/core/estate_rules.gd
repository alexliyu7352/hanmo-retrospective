## 私人庄园与坞堡经营：真人承担管理、劳动和守备，钱粮仍在同一世界的真实资产中。
extends RefCounted
const Factory=preload("res://core/world_factory.gd")
const Inventory=preload("res://core/inventory_rules.gd")
const Location=preload("res://core/world_location.gd")
const KINDS=["estate_fund","estate_appoint","estate_assign","estate_build","estate_deposit","estate_withdraw","estate_muster","estate_return"]
const FACILITIES=["farm","granary","palisade"]
const DUTIES=["idle","labor","guard"]
const MAX_VALUE=2147483647

## 在原世界资产创建后补入庄园任命与设施；不创建人物、粮食或钱款，重复初始化保留运行归属。
static func initialize(state: Dictionary, data: Dictionary) -> Dictionary:
	if state.has("estates"): return validate_state(state,data)
	var entries: Dictionary={}
	for id in data.get("estates",{}):
		var definition: Dictionary=data.estates[id]
		var duties: Dictionary={}
		for actor_id in definition.resident_actor_ids: duties[actor_id]="idle"
		for assignment in definition.initial_duties: duties[assignment.actor_id]=assignment.duty
		duties[definition.manager_actor_id]="idle"
		entries[id]={"id":id,"owner_actor_id":definition.owner_actor_id,"manager_actor_id":definition.manager_actor_id,"resident_actor_ids":definition.resident_actor_ids.duplicate(),"duties":duties,"facilities":definition.initial_facilities.duplicate(true),"detached":{},"last_tick":state.tick,"last_production_id":""}
		if not state.inventories.has(definition.inventory_id) or not state.wallets.has(definition.wallet_id): return _fail("EstateAssetsMissing")
	state.estates=entries
	for id in entries: state.inventories[data.estates[id].inventory_id].capacity=_capacity(data,id,entries[id])
	return validate_state(state,data)

## 在本半日行动释放占用之前调用一次；重复终点不再吃粮或产出，跳时和倒退明确失败。
static func step(state: Dictionary, data: Dictionary, finish_tick: int, hold: Array=[]) -> Dictionary:
	var valid: Dictionary=validate_state(state,data)
	if not valid.ok: return valid
	if finish_tick==state.tick: return {"ok":true,"events":[],"applied":false}
	if finish_tick!=state.tick+1: return _fail("EstateTickMismatch")
	var events: Array=[]
	var ids: Array=state.estates.keys(); ids.sort()
	for id in ids:
		var estate: Dictionary=state.estates[id]
		var definition: Dictionary=data.estates[id]
		var workers: Array=_members(state,data,id,"labor")
		var guards: Array=_members(state,data,id,"guard")
		workers=workers.filter(func(person): return not person in hold)
		guards=guards.filter(func(person): return not person in hold)
		var manager: String=estate.manager_actor_id
		var managed: bool=not manager.is_empty() and not manager in hold and not estate.detached.has(manager) and _ready(state,manager,definition.place_id)
		var mouths: Array=[manager] if managed else []
		mouths.append_array(workers); mouths.append_array(guards)
		var fed: Array=[]
		var unfed: Array=[]
		var consumed: int=0
		var owner: Dictionary={"kind":"actor","id":estate.owner_actor_id}
		for person in mouths:
			if _consume(state,definition.inventory_id,definition.food_item_id,definition.ration_per_person,owner):
				fed.append(person); consumed+=definition.ration_per_person
				state.actors[person].fatigue=maxi(0,state.actors[person].fatigue-1)
			else:
				unfed.append(person); state.actors[person].fatigue=mini(100,state.actors[person].fatigue+4)
		var working: Array=[]
		if managed and manager in fed:
			for person in workers:
				if person in fed: working.append(person)
		var watching: Array=[]
		for person in guards:
			if person in fed: watching.append(person)
		var expected: int=working.size()*definition.output_per_worker*estate.facilities.farm
		var inventory: Dictionary=state.inventories[definition.inventory_id]
		var weight: int=data.items[definition.output_item_id].weight
		var produced: int=expected
		if weight>0: produced=mini(expected,int((inventory.capacity-Inventory.weight(inventory,data))/weight))
		if produced>0: Inventory.add(state,inventory.id,definition.output_item_id,produced,owner)
		var reasons: Array=[]
		if not managed or not manager in fed: reasons.append("manager_unavailable")
		if not unfed.is_empty(): reasons.append("food_shortage")
		if expected>produced: reasons.append("storage_full")
		if workers.is_empty(): reasons.append("no_workers")
		var fact_id: String=Factory.next_id(state,"estate")
		state.facts[fact_id]={"kind":"estate_production","estate_id":id,"tick":finish_tick,"owner_actor_id":estate.owner_actor_id,"manager_actor_id":manager,"workers":working,"guards":watching,"fed":fed,"unfed":unfed,"farm_level":estate.facilities.farm,"palisade_level":estate.facilities.palisade,"consumed":{"item_id":definition.food_item_id,"quantity":consumed},"produced":{"item_id":definition.output_item_id,"quantity":produced},"lost_output":expected-produced,"defense":_defense(state,definition,estate,watching),"reasons":reasons}
		estate.last_tick=finish_tick; estate.last_production_id=fact_id
		events.append(fact_id)
	return {"ok":true,"events":events,"applied":true}

## 只读显示真实在场人手、守备与上一生产事实；界面不能借此改任命、位置或库存。
static func view(state: Dictionary, data: Dictionary, estate_id: String) -> Dictionary:
	if not state.get("estates",{}).has(estate_id) or not data.get("estates",{}).has(estate_id): return _fail("UnknownEstate")
	var estate: Dictionary=state.estates[estate_id]
	var definition: Dictionary=data.estates[estate_id]
	var last: Dictionary=state.facts.get(estate.last_production_id,{})
	var guards: Array=[]
	for actor_id in _members(state,data,estate_id,"guard"):
		if not actor_id in last.get("unfed",[]): guards.append(actor_id)
	return {"ok":true,"estate":estate.duplicate(true),"available_workers":_members(state,data,estate_id,"labor"),"available_guards":guards,"defense":_defense(state,definition,estate,guards),"capacity":state.inventories[definition.inventory_id].capacity,"last_production":last.duplicate(true)}

## 管理与物资操作必须在庄园实际地点提出；公职没有互斥锁，但也不自动取得私产或公库权限。
static func preview(state: Dictionary, data: Dictionary, actor_id: String, kind: String, params: Dictionary) -> Dictionary:
	return _preview(state,data,actor_id,kind,params)

## 对普通预览与行动开始共用检查；own_action只允许本次已被Kernel占用的人物继续办理。
static func _preview(state: Dictionary, data: Dictionary, actor_id: String, kind: String, params: Dictionary, own_action: String="") -> Dictionary:
	if not kind in KINDS: return _fail("UnknownEstateCommand")
	var id: Variant=params.get("estate_id")
	if not id is String or not state.get("estates",{}).has(id) or not data.get("estates",{}).has(id): return _fail("UnknownEstate")
	var estate: Dictionary=state.estates[id]
	var definition: Dictionary=data.estates[id]
	if not actor_id in [estate.owner_actor_id,estate.manager_actor_id]: return _fail("EstateAuthorityRequired")
	if not _ready(state,actor_id,definition.place_id,own_action,actor_id): return _fail("EstateActorUnavailable")
	var quote: Dictionary={"ok":true,"duration":0,"estate_id":id,"params":params.duplicate(true)}
	match kind:
		"estate_fund":
			if actor_id!=estate.owner_actor_id or not _whole(params.get("quantity"),1,100000): return _fail("EstateFundingAuthority")
			var from: String=state.actors[actor_id].wallet_id
			var to: String=definition.wallet_id
			if from==to or Inventory.spendable(state.wallets[from])<params.quantity: return _fail("InsufficientFunds")
			if state.wallets[to].balance>MAX_VALUE-params.quantity: return _fail("MoneyOverflow")
			quote.cash={"from_wallet_id":from,"to_wallet_id":to,"quantity":params.quantity}
		"estate_appoint":
			var manager: Variant=params.get("manager_actor_id")
			if not manager is String: return _fail("InvalidEstateManager")
			if not manager.is_empty() and (not manager in estate.resident_actor_ids or estate.detached.has(manager) or not _ready(state,manager,definition.place_id,"",actor_id)): return _fail("EstateManagerUnavailable")
		"estate_assign":
			var person: Variant=params.get("resident_actor_id")
			if not person is String or not person in estate.resident_actor_ids or not params.get("duty") in DUTIES: return _fail("InvalidEstateDuty")
			if estate.detached.has(person) or not _ready(state,person,definition.place_id,"",actor_id): return _fail("EstateResidentUnavailable")
			if person==estate.manager_actor_id and params.duty!="idle": return _fail("EstateManagerHasDuty")
		"estate_build":
			var facility: Variant=params.get("facility")
			if not facility is String or not facility in FACILITIES: return _fail("UnknownEstateFacility")
			for action in state.actions.values():
				if action.id!=own_action and action.kind=="estate_build" and action.params.get("terms",{}).get("estate_id")==id and action.params.terms.facility==facility: return _fail("EstateFacilityBusy")
			var terms: Dictionary=_building_terms(data,estate,facility)
			if terms.to_level>definition.construction.max_level: return _fail("EstateFacilityMaxLevel")
			if facility=="granary" and _capacity(data,id,estate)+definition.granary_capacity_per_level>MAX_VALUE: return _fail("EstateCapacityOverflow")
			var payer: Dictionary=state.wallets[terms.payer_wallet_id]
			var payee: Dictionary=state.wallets[terms.payee_wallet_id]
			if payer.owner!={"kind":"actor","id":estate.owner_actor_id} or payer.id==payee.id: return _fail("EstateConstructionPayment")
			if Inventory.spendable(payer)<terms.money_cost: return _fail("InsufficientFunds")
			if payee.balance>MAX_VALUE-terms.money_cost: return _fail("MoneyOverflow")
			if Inventory.count(state.inventories[terms.inventory_id],terms.material_item_id,[{"kind":"actor","id":estate.owner_actor_id}])<terms.material_quantity: return _fail("InsufficientEstateMaterials")
			quote.duration=terms.duration; quote.terms=terms; quote.participants=[actor_id]
		"estate_deposit","estate_withdraw":
			var transfer: Dictionary=_transfer_quote(state,data,actor_id,kind,params,estate)
			if not transfer.ok: return transfer
			quote.transfer=transfer.terms
		"estate_muster","estate_return":
			var people: Variant=params.get("actor_ids")
			if not people is Array or people.is_empty() or people.size()>estate.resident_actor_ids.size() or not _muster_source(state,params.get("source_id"),id): return _fail("InvalidEstateMuster")
			var seen: Array=[]
			for person in people:
				if not person is String or person in seen or not person in estate.resident_actor_ids or not _ready(state,person,definition.place_id,"",actor_id,kind=="estate_return"): return _fail("EstateResidentUnavailable")
				if kind=="estate_return" and not Location.army_id(state,person).is_empty(): return _fail("ArmyNotReturned")
				if kind=="estate_muster" and estate.detached.has(person): return _fail("EstateAlreadyDetached")
				if kind=="estate_return" and estate.detached.get(person)!=params.source_id: return _fail("EstateDetachmentMismatch")
				seen.append(person)
			quote.actor_ids=seen
	return quote

## 在候选中复核瞬时命令并保存实际移交或任命；建设必须进入原通用行动系统，不允许零时升级。
static func apply(state: Dictionary, data: Dictionary, actor_id: String, kind: String, quote: Dictionary) -> Dictionary:
	if kind=="estate_build": return _fail("EstateBuildNeedsAction")
	var current: Dictionary=preview(state,data,actor_id,kind,quote.get("params",{}))
	if not current.ok: return current
	if current!=quote: return _fail("StaleEstateQuote")
	var estate: Dictionary=state.estates[current.estate_id]
	var params: Dictionary=current.params
	var details: Dictionary={"estate_id":estate.id}
	var fact: Dictionary={"kind":"estate_management","estate_id":estate.id,"actor_id":actor_id,"tick":state.tick,"command":kind,"params":params.duplicate(true)}
	match kind:
		"estate_fund":
			state.wallets[current.cash.from_wallet_id].balance-=current.cash.quantity
			state.wallets[current.cash.to_wallet_id].balance+=current.cash.quantity
			fact.kind="estate_funding"; fact.cash=current.cash.duplicate(true)
		"estate_appoint":
			estate.manager_actor_id=params.manager_actor_id
			if not params.manager_actor_id.is_empty(): estate.duties[params.manager_actor_id]="idle"
		"estate_assign": estate.duties[params.resident_actor_id]=params.duty
		"estate_deposit","estate_withdraw":
			var transfer: Dictionary=current.transfer
			var moved: Dictionary=Inventory.transfer(state,transfer.from_inventory_id,transfer.to_inventory_id,transfer.item_id,transfer.quantity,transfer.owners,"",transfer.new_owner)
			if not moved.ok: return moved
			fact.kind="estate_transfer"; fact.transfer=transfer.duplicate(true)
		"estate_muster","estate_return":
			for person in current.actor_ids:
				if kind=="estate_muster": estate.detached[person]=params.source_id
				else: estate.detached.erase(person)
			fact.kind="estate_detachment"; details.actor_ids=current.actor_ids.duplicate()
	var fact_id: String=Factory.next_id(state,"estate")
	state.facts[fact_id]=fact; details.fact_id=fact_id
	return {"ok":true,"details":details}

## 已占用的建造行动先支付真实承建方并消耗私有材料；永久开工事实阻止重复付费和重复扣货。
static func begin(state: Dictionary, data: Dictionary, action: Dictionary, quote: Dictionary) -> Dictionary:
	if action.get("kind")!="estate_build" or state.facts.has(action.get("id")): return _fail("InvalidEstateBuildAction")
	var current: Dictionary=_preview(state,data,action.actor_id,"estate_build",action.params,action.id)
	if not current.ok: return current
	if current!=quote: return _fail("StaleEstateQuote")
	if action.get("participants")!=[action.actor_id] or state.actors[action.actor_id].action_id!=action.id or action.get("start_tick")!=state.tick or action.get("remaining_ticks")!=current.duration: return _fail("InvalidEstateBuildOccupation")
	var terms: Dictionary=current.terms
	if not _consume(state,terms.inventory_id,terms.material_item_id,terms.material_quantity,{"kind":"actor","id":terms.owner_actor_id}): return _fail("InsufficientEstateMaterials")
	state.wallets[terms.payer_wallet_id].balance-=terms.money_cost
	state.wallets[terms.payee_wallet_id].balance+=terms.money_cost
	action.params={"terms":terms.duplicate(true)}
	state.facts[action.id]={"kind":"estate_construction","phase":"started","actor_id":action.actor_id,"start_tick":state.tick,"completed_tick":-1,"success":false,"terms":terms.duplicate(true)}
	return {"ok":true}

## 只有实际时间到期且同一庄园归属和前置等级仍成立才完工；中断保留已支付成本与失败事实。
static func finish(state: Dictionary, data: Dictionary, action: Dictionary) -> Dictionary:
	var valid: Dictionary=_validate_build(state,data,action,true)
	if not valid.ok: return valid
	var fact: Dictionary=state.facts[action.id]
	if fact.phase=="completed": return {"ok":true,"success":fact.success,"already_completed":true}
	var terms: Dictionary=action.params.terms
	if action.remaining_ticks!=0 or state.tick+1!=action.start_tick+terms.duration: return _fail("EstateBuildNotReady")
	var estate: Dictionary=state.estates[terms.estate_id]
	var success: bool=estate.owner_actor_id==terms.owner_actor_id and estate.facilities[terms.facility]==terms.from_level and _ready(state,action.actor_id,terms.place_id,action.id,action.actor_id)
	if success:
		estate.facilities[terms.facility]=terms.to_level
		var capacity: int=_capacity(data,estate.id,estate)
		if state.inventories[terms.inventory_id].capacity!=capacity:
			state.inventories[terms.inventory_id].capacity=capacity
			state.inventories[terms.inventory_id].revision+=1
	fact.phase="completed"; fact.completed_tick=state.tick+1; fact.success=success
	return {"ok":true,"success":success,"code":"" if success else "EstateBuildInterrupted"}

## 保存边界核验建设占用、已支付条款和时间，不要求当前继任所有者仍是开工时的所有者。
static func validate_action(state: Dictionary, data: Dictionary, action: Dictionary) -> Dictionary:
	return _validate_build(state,data,action,false)

## 对活动和到期建设共用只读校验；完成事实作为永久依据，不借短命令缓存重放升级。
static func _validate_build(state: Dictionary, data: Dictionary, action: Dictionary, finishing: bool) -> Dictionary:
	if not _fields(action,{"id":TYPE_STRING,"kind":TYPE_STRING,"actor_id":TYPE_STRING,"participants":TYPE_ARRAY,"remaining_ticks":TYPE_INT,"start_tick":TYPE_INT,"params":TYPE_DICTIONARY}) or action.kind!="estate_build": return _fail("InvalidEstateBuildAction")
	var terms: Variant=action.params.get("terms")
	if action.params.size()!=1 or not _valid_terms(state,data,terms): return _fail("InvalidEstateBuildTerms")
	var fact: Variant=state.facts.get(action.id)
	if not _fields(fact,{"kind":TYPE_STRING,"phase":TYPE_STRING,"actor_id":TYPE_STRING,"start_tick":TYPE_INT,"completed_tick":TYPE_INT,"success":TYPE_BOOL,"terms":TYPE_DICTIONARY}) or fact.kind!="estate_construction" or fact.actor_id!=action.actor_id or fact.start_tick!=action.start_tick or fact.terms!=terms: return _fail("EstateBuildFactMismatch")
	if action.participants!=[action.actor_id] or not state.actors.has(action.actor_id): return _fail("InvalidEstateBuildOccupation")
	if fact.phase=="completed":
		if not finishing or fact.completed_tick!=action.start_tick+terms.duration or action.remaining_ticks!=0: return _fail("InvalidEstateBuildTime")
		return {"ok":true}
	if fact.phase!="started" or fact.completed_tick!=-1 or fact.success or state.actors[action.actor_id].action_id!=action.id: return _fail("InvalidEstateBuildOccupation")
	if not _whole(action.start_tick,0,state.tick) or not _whole(action.remaining_ticks,0 if finishing else 1,terms.duration): return _fail("InvalidEstateBuildTime")
	var elapsed: int=action.start_tick+terms.duration-action.remaining_ticks
	if elapsed!=state.tick+(1 if finishing else 0): return _fail("InvalidEstateBuildTime")
	return {"ok":true}

## 只读检查庄园真人、私产、设施容量与永久事实；允许真实继任后的所有人，不锁定作者初始归属。
static func validate_state(state: Dictionary, data: Dictionary) -> Dictionary:
	if not state.get("estates") is Dictionary or not state.get("facts") is Dictionary: return _fail("InvalidEstateState")
	if state.estates.size()!=data.get("estates",{}).size(): return _fail("MissingEstates")
	var residents: Dictionary={}
	var inventories: Dictionary={}
	var wallets: Dictionary={}
	var personal_wallets: Dictionary={}
	var personal_inventories: Dictionary={}
	var production_dates: Dictionary={}
	var transfer_sources: Dictionary={}
	for actor in state.actors.values():
		personal_inventories[actor.inventory_id]=true; personal_wallets[actor.wallet_id]=true
	for id in state.estates:
		var estate: Variant=state.estates[id]
		if not data.get("estates",{}).has(id) or not _fields(estate,{"id":TYPE_STRING,"owner_actor_id":TYPE_STRING,"manager_actor_id":TYPE_STRING,"resident_actor_ids":TYPE_ARRAY,"duties":TYPE_DICTIONARY,"facilities":TYPE_DICTIONARY,"detached":TYPE_DICTIONARY,"last_tick":TYPE_INT,"last_production_id":TYPE_STRING}) or estate.size()!=9 or estate.id!=id: return _fail("InvalidEstateState")
		var definition: Dictionary=data.estates[id]
		if not state.actors.has(estate.owner_actor_id) or estate.last_tick!=state.tick: return _fail("InvalidEstateOwnerOrTime")
		if not estate.manager_actor_id.is_empty() and not estate.manager_actor_id in estate.resident_actor_ids: return _fail("InvalidEstateManager")
		if estate.resident_actor_ids.is_empty() or estate.duties.size()!=estate.resident_actor_ids.size(): return _fail("InvalidEstateResidents")
		for person in estate.resident_actor_ids:
			if not person is String or not state.actors.has(person) or residents.has(person) or not estate.duties.get(person) in DUTIES: return _fail("InvalidEstateResidents")
			residents[person]=id
		if not estate.manager_actor_id.is_empty() and estate.duties[estate.manager_actor_id]!="idle": return _fail("EstateManagerHasDuty")
		for person in estate.detached:
			if not person in estate.resident_actor_ids or not _muster_source(state,estate.detached[person],id): return _fail("InvalidEstateDetachment")
		if estate.facilities.size()!=3: return _fail("InvalidEstateFacilities")
		for facility in FACILITIES:
			if not _whole(estate.facilities.get(facility),0,definition.construction.max_level): return _fail("InvalidEstateFacilities")
		var owner: Dictionary={"kind":"actor","id":estate.owner_actor_id}
		var inventory: Variant=state.inventories.get(definition.inventory_id)
		var wallet: Variant=state.wallets.get(definition.wallet_id)
		if not inventory is Dictionary or not wallet is Dictionary or inventory.get("owner")!=owner or wallet.get("owner")!=owner or inventories.has(definition.inventory_id) or wallets.has(definition.wallet_id): return _fail("EstateAssetOwnership")
		if personal_inventories.has(definition.inventory_id) or personal_wallets.has(definition.wallet_id): return _fail("EstateAssetsNeedSeparateContainers")
		inventories[definition.inventory_id]=true; wallets[definition.wallet_id]=true
		if not _whole(inventory.capacity,1,MAX_VALUE) or inventory.capacity!=_capacity(data,id,estate) or Inventory.weight(inventory,data)>inventory.capacity: return _fail("EstateCapacityMismatch")
		if not estate.last_production_id.is_empty():
			var last: Variant=state.facts.get(estate.last_production_id)
			if not last is Dictionary or last.get("kind")!="estate_production" or last.get("estate_id")!=id or last.get("tick")!=estate.last_tick: return _fail("EstateProductionReference")
	for fact_id in state.facts:
		var fact: Variant=state.facts[fact_id]
		if not fact is Dictionary or not str(fact.get("kind","")).begins_with("estate_"): continue
		var status: Dictionary=_validate_fact(state,data,fact_id,fact)
		if not status.ok: return status
		if fact.kind=="estate_production":
			var date: String=fact.estate_id+":"+str(fact.tick)
			if production_dates.has(date): return _fail("DuplicateEstateProduction")
			production_dates[date]=fact_id
			if fact.tick==state.tick and state.estates[fact.estate_id].last_production_id!=fact_id: return _fail("EstateProductionReference")
		if fact.kind=="estate_transfer" and not fact.transfer.source_id.is_empty():
			if transfer_sources.has(fact.transfer.source_id): return _fail("EstateTransferSourceUsed")
			transfer_sources[fact.transfer.source_id]=true
	return {"ok":true}

## 私库容量由原容器和仓廪级数共同决定，不在界面保存第二个仓储数值。
static func _capacity(data: Dictionary, estate_id: String, estate: Dictionary) -> int:
	var definition: Dictionary=data.estates[estate_id]
	return data.inventories[definition.inventory_id].capacity+estate.facilities.granary*definition.granary_capacity_per_level

## 读取实际空闲且留在庄园的分工人手；外出、负伤、军役和借调者不能同时参加劳动或守备。
static func _members(state: Dictionary, data: Dictionary, estate_id: String, duty: String) -> Array:
	var estate: Dictionary=state.estates[estate_id]
	var members: Array=[]
	for person in estate.resident_actor_ids:
		if estate.duties[person]==duty and not estate.detached.has(person) and _ready(state,person,data.estates[estate_id].place_id): members.append(person)
	members.sort()
	return members

## 只认同一地点的真人；交谈也占用劳动时间，但允许本人正在办理的管理会话或建设行动。
static func _ready(state: Dictionary, actor_id: String, place_id: String, own_action: String="", own_speaker: String="", allow_injury: bool=false) -> bool:
	if not state.actors.has(actor_id): return false
	var actor: Dictionary=state.actors[actor_id]
	if not actor.alive or not actor.available or (not allow_injury and actor.get("injury",0)>0) or not actor.action_id in ["",own_action]: return false
	if Location.resolve(state,actor_id).get("place_id")!=place_id: return false
	for battle in state.get("battles",{}).values():
		if battle.phase!="closed" and battle.units.has(actor_id): return false
	for owner in state.interactions:
		if owner!=own_speaker and (owner==actor_id or state.interactions[owner].context.get("provider_actor_id")==actor_id): return false
	return true

## 守备只由实际在场且有口粮的真人贡献，木栅提高其防御但不会凭空产生兵员。
static func _defense(state: Dictionary, definition: Dictionary, estate: Dictionary, guards: Array) -> int:
	var result: int=0
	for person in guards: result+=maxi(1,int(state.actors[person].attributes.martial/10))+estate.facilities.palisade*definition.palisade_defense_per_guard
	return result

## 沿用库存所有权与预留规则消耗实物；数量不足时完全不改库存，消费后移除空批次。
static func _consume(state: Dictionary, inventory_id: String, item_id: String, quantity: int, owner: Dictionary) -> bool:
	var inventory: Dictionary=state.inventories[inventory_id]
	if Inventory.count(inventory,item_id,[owner])<quantity: return false
	if quantity==0: return true
	var remaining: int=quantity
	for lot in inventory.lots:
		if lot.item_id!=item_id or lot.owner!=owner or not lot.reserved_by.is_empty(): continue
		var used: int=mini(remaining,lot.quantity); lot.quantity-=used; remaining-=used
		if remaining==0: break
	inventory.lots=inventory.lots.filter(func(lot): return lot.quantity>0)
	inventory.revision+=1
	return true

## 建造报价冻结真实来源与目标等级；每级成本由同一作者配置计算，不凭空发放建材或现金。
static func _building_terms(data: Dictionary, estate: Dictionary, facility: String) -> Dictionary:
	var definition: Dictionary=data.estates[estate.id]
	var build: Dictionary=definition.construction
	var target: int=estate.facilities[facility]+1
	return {"estate_id":estate.id,"facility":facility,"from_level":target-1,"to_level":target,"duration":build.build_ticks*target,"money_cost":build.money_cost*target,"material_item_id":build.material_item_id,"material_quantity":build.material_cost*target,"payer_wallet_id":definition.wallet_id,"payee_wallet_id":build.payee_wallet_id,"inventory_id":definition.inventory_id,"owner_actor_id":estate.owner_actor_id,"place_id":definition.place_id}

## 将本人或受托私产实际装卸；公物必须有一次性的永久调拨依据，管事领货不会取得个人所有权。
static func _transfer_quote(state: Dictionary, data: Dictionary, actor_id: String, kind: String, params: Dictionary, estate: Dictionary) -> Dictionary:
	if not params.get("item_id") is String or not data.items.has(params.item_id) or not _whole(params.get("quantity"),1,10000): return _fail("InvalidEstateTransfer")
	var definition: Dictionary=data.estates[estate.id]
	var bag: String=state.actors[actor_id].inventory_id
	var owner: Dictionary={"kind":"actor","id":estate.owner_actor_id}
	var owners: Array=[owner]
	var source_id: Variant=params.get("source_id","")
	if not source_id is String: return _fail("InvalidEstateTransferSource")
	if kind=="estate_deposit":
		var personal: Dictionary={"kind":"actor","id":actor_id}
		if not personal in owners: owners.append(personal)
		if not source_id.is_empty():
			var source: Variant=state.facts.get(source_id)
			if not source is Dictionary or source.get("kind")!="public_estate_transfer" or source.get("estate_id")!=estate.id or source.get("actor_id")!=actor_id or source.get("item_id")!=params.item_id or source.get("quantity")!=params.quantity: return _fail("InvalidEstateTransferSource")
			var public_owner: Variant=source.get("source_owner")
			if not public_owner is Dictionary or public_owner.get("kind")!="organization" or not data.organizations.has(public_owner.get("id")): return _fail("InvalidEstateTransferSource")
			for fact in state.facts.values():
				if fact is Dictionary and fact.get("kind")=="estate_transfer" and fact.get("transfer",{}).get("source_id")==source_id: return _fail("EstateTransferSourceUsed")
			owners=[public_owner]
	elif not source_id.is_empty(): return _fail("InvalidEstateTransferSource")
	var from_id: String=bag if kind=="estate_deposit" else definition.inventory_id
	var to_id: String=definition.inventory_id if kind=="estate_deposit" else bag
	if from_id==to_id: return _fail("SameContainer")
	if Inventory.count(state.inventories[from_id],params.item_id,owners)<params.quantity: return _fail("InsufficientOwnedEstateGoods")
	var destination: Dictionary=state.inventories[to_id]
	if Inventory.weight(destination,data)+params.quantity*data.items[params.item_id].weight>destination.capacity: return _fail("EstateStorageFull")
	return {"ok":true,"terms":{"from_inventory_id":from_id,"to_inventory_id":to_id,"item_id":params.item_id,"quantity":params.quantity,"owners":owners,"new_owner":owner,"source_id":source_id}}

## 部曲借调只接受调用方已有的同庄园永久来源；同一真实人物的驻防身份不会被克隆。
static func _muster_source(state: Dictionary, source_id: Variant, estate_id: String) -> bool:
	if not source_id is String or source_id.is_empty(): return false
	var fact: Variant=state.facts.get(source_id)
	return fact is Dictionary and fact.get("kind") in ["military_muster","estate_muster"] and fact.get("estate_id")==estate_id

## 验证冻结建设条款与作者造价一致；历史所有者按真人引用校验，不能因继任抹掉开工依据。
static func _valid_terms(state: Dictionary, data: Dictionary, terms: Variant) -> bool:
	if not _fields(terms,{"estate_id":TYPE_STRING,"facility":TYPE_STRING,"from_level":TYPE_INT,"to_level":TYPE_INT,"duration":TYPE_INT,"money_cost":TYPE_INT,"material_item_id":TYPE_STRING,"material_quantity":TYPE_INT,"payer_wallet_id":TYPE_STRING,"payee_wallet_id":TYPE_STRING,"inventory_id":TYPE_STRING,"owner_actor_id":TYPE_STRING,"place_id":TYPE_STRING}) or terms.size()!=13: return false
	if not data.get("estates",{}).has(terms.estate_id) or not terms.facility in FACILITIES or not state.actors.has(terms.owner_actor_id): return false
	var estate: Dictionary={"id":terms.estate_id,"owner_actor_id":terms.owner_actor_id,"facilities":{terms.facility:terms.from_level}}
	return _whole(terms.from_level,0,data.estates[terms.estate_id].construction.max_level-1) and terms==_building_terms(data,estate,terms.facility)

## 保存永久经营账目引用与数量；历史生产允许前任所有者和已经离庄的居民，不能冒称另一庄园的资产。
static func _validate_fact(state: Dictionary, data: Dictionary, fact_id: String, fact: Dictionary) -> Dictionary:
	if fact.kind=="estate_muster": return {"ok":true}
	if not _instance_id(fact_id,"action" if fact.kind=="estate_construction" else "estate",state.next_instance_id): return _fail("InvalidEstateFactId")
	if fact.kind=="estate_construction":
		if not _fields(fact,{"phase":TYPE_STRING,"actor_id":TYPE_STRING,"start_tick":TYPE_INT,"completed_tick":TYPE_INT,"success":TYPE_BOOL,"terms":TYPE_DICTIONARY}) or not _valid_terms(state,data,fact.terms) or not state.actors.has(fact.actor_id): return _fail("InvalidEstateConstructionFact")
		if not _whole(fact.start_tick,0,state.tick): return _fail("InvalidEstateConstructionFact")
		if fact.phase=="started":
			if fact.completed_tick!=-1 or fact.success or not state.actions.has(fact_id): return _fail("InvalidEstateConstructionFact")
			return validate_action(state,data,state.actions[fact_id])
		elif fact.phase!="completed" or fact.completed_tick!=fact.start_tick+fact.terms.duration or fact.completed_tick>state.tick: return _fail("InvalidEstateConstructionFact")
		return {"ok":true}
	if not _fields(fact,{"estate_id":TYPE_STRING,"tick":TYPE_INT}) or not data.get("estates",{}).has(fact.estate_id) or not _whole(fact.tick,0,state.tick): return _fail("InvalidEstateFact")
	if fact.kind=="estate_production":
		if not _fields(fact,{"owner_actor_id":TYPE_STRING,"manager_actor_id":TYPE_STRING,"workers":TYPE_ARRAY,"guards":TYPE_ARRAY,"fed":TYPE_ARRAY,"unfed":TYPE_ARRAY,"farm_level":TYPE_INT,"palisade_level":TYPE_INT,"consumed":TYPE_DICTIONARY,"produced":TYPE_DICTIONARY,"lost_output":TYPE_INT,"defense":TYPE_INT,"reasons":TYPE_ARRAY}) or not state.actors.has(fact.owner_actor_id) or (fact.manager_actor_id!="" and not state.actors.has(fact.manager_actor_id)): return _fail("InvalidEstateProductionFact")
		var people: Array=[]
		for group in [fact.fed,fact.unfed]:
			for person in group:
				if not person is String or not state.actors.has(person) or person in people: return _fail("InvalidEstateProductionPeople")
				people.append(person)
		var definition: Dictionary=data.estates[fact.estate_id]
		if fact.consumed.get("item_id")!=definition.food_item_id or fact.produced.get("item_id")!=definition.output_item_id or not _whole(fact.consumed.get("quantity"),0,MAX_VALUE) or not _whole(fact.produced.get("quantity"),0,MAX_VALUE) or not _whole(fact.lost_output,0,MAX_VALUE) or not _whole(fact.defense,0,MAX_VALUE): return _fail("InvalidEstateProductionQuantity")
		people=[]
		for group in [fact.workers,fact.guards]:
			for person in group:
				if not person in fact.fed or person in people or person==fact.manager_actor_id: return _fail("InvalidEstateProductionPeople")
				people.append(person)
		if not _whole(fact.farm_level,0,definition.construction.max_level) or not _whole(fact.palisade_level,0,definition.construction.max_level): return _fail("InvalidEstateProductionQuantity")
		if fact.consumed.quantity!=fact.fed.size()*definition.ration_per_person or fact.produced.quantity+fact.lost_output!=fact.workers.size()*definition.output_per_worker*fact.farm_level: return _fail("InvalidEstateProductionQuantity")
		if not fact.workers.is_empty() and not fact.manager_actor_id in fact.fed: return _fail("InvalidEstateProductionPeople")
		if not _whole(fact.defense,fact.guards.size()*(1+fact.palisade_level*definition.palisade_defense_per_guard),fact.guards.size()*(11+fact.palisade_level*definition.palisade_defense_per_guard)): return _fail("InvalidEstateProductionQuantity")
		for reason in fact.reasons:
			if not reason in ["manager_unavailable","food_shortage","storage_full","no_workers"]: return _fail("InvalidEstateProductionReason")
	elif fact.kind in ["estate_management","estate_transfer","estate_detachment","estate_funding"]:
		if not _fields(fact,{"actor_id":TYPE_STRING,"command":TYPE_STRING,"params":TYPE_DICTIONARY}) or not state.actors.has(fact.actor_id) or not fact.command in KINDS or fact.params.get("estate_id")!=fact.estate_id: return _fail("InvalidEstateManagementFact")
		if fact.kind=="estate_funding":
			var cash: Variant=fact.get("cash")
			if fact.command!="estate_fund" or not _fields(cash,{"from_wallet_id":TYPE_STRING,"to_wallet_id":TYPE_STRING,"quantity":TYPE_INT}) or not _whole(cash.quantity,1,100000) or cash.quantity!=fact.params.get("quantity"): return _fail("InvalidEstateFunding")
			if cash.from_wallet_id!=state.actors[fact.actor_id].wallet_id or cash.to_wallet_id!=data.estates[fact.estate_id].wallet_id or cash.from_wallet_id==cash.to_wallet_id: return _fail("InvalidEstateFunding")
		if fact.kind=="estate_management":
			if fact.command=="estate_appoint":
				var manager: Variant=fact.params.get("manager_actor_id")
				if not manager is String or (manager!="" and not state.actors.has(manager)): return _fail("InvalidEstateManagementFact")
			elif fact.command!="estate_assign" or not state.actors.has(fact.params.get("resident_actor_id")) or not fact.params.get("duty") in DUTIES: return _fail("InvalidEstateManagementFact")
		if fact.kind=="estate_detachment":
			if not fact.command in ["estate_muster","estate_return"] or not fact.params.get("actor_ids") is Array or fact.params.actor_ids.is_empty() or not _muster_source(state,fact.params.get("source_id"),fact.estate_id): return _fail("InvalidEstateDetachment")
			var members: Array=[]
			for person in fact.params.actor_ids:
				if not person is String or not state.actors.has(person) or person in members: return _fail("InvalidEstateDetachment")
				members.append(person)
		if fact.kind=="estate_transfer":
			var transfer: Variant=fact.get("transfer")
			if not _fields(transfer,{"from_inventory_id":TYPE_STRING,"to_inventory_id":TYPE_STRING,"item_id":TYPE_STRING,"quantity":TYPE_INT,"owners":TYPE_ARRAY,"new_owner":TYPE_DICTIONARY,"source_id":TYPE_STRING}) or not state.inventories.has(transfer.from_inventory_id) or not state.inventories.has(transfer.to_inventory_id) or not data.items.has(transfer.item_id) or not _whole(transfer.quantity,1,10000): return _fail("InvalidEstateTransferFact")
			var private_owner: Dictionary=transfer.new_owner
			if private_owner.get("kind")!="actor" or not state.actors.has(private_owner.get("id")) or transfer.quantity!=fact.params.get("quantity") or transfer.item_id!=fact.params.get("item_id"): return _fail("InvalidEstateTransferFact")
			if not fact.command in ["estate_deposit","estate_withdraw"] or transfer.owners.is_empty() or transfer.source_id!=fact.params.get("source_id",""): return _fail("InvalidEstateTransferFact")
			var store: String=data.estates[fact.estate_id].inventory_id
			var bag: String=state.actors[fact.actor_id].inventory_id
			if transfer.from_inventory_id!=(bag if fact.command=="estate_deposit" else store) or transfer.to_inventory_id!=(store if fact.command=="estate_deposit" else bag): return _fail("InvalidEstateTransferFact")
			if not transfer.source_id.is_empty():
				var source: Variant=state.facts.get(transfer.source_id)
				if not source is Dictionary or source.get("kind")!="public_estate_transfer" or source.get("estate_id")!=fact.estate_id or source.get("actor_id")!=fact.actor_id or source.get("item_id")!=transfer.item_id or source.get("quantity")!=transfer.quantity or transfer.owners!=[source.get("source_owner")]: return _fail("InvalidEstateTransferSource")
				if fact.command!="estate_deposit" or not source.get("source_owner") is Dictionary or source.source_owner.get("kind")!="organization" or not data.organizations.has(source.source_owner.get("id")): return _fail("InvalidEstateTransferSource")
			else:
				for owner in transfer.owners:
					if not owner in [private_owner,{"kind":"actor","id":fact.actor_id}]: return _fail("InvalidEstateTransferSource")
	else: return _fail("UnknownEstateFact")
	return {"ok":true}

## 解引用前严格检查字段类型；其他世界字段仍由原StateSchema负责。
static func _fields(value: Variant, fields: Dictionary) -> bool:
	if not value is Dictionary: return false
	for key in fields:
		if typeof(value.get(key))!=fields[key]: return false
	return true

## 整数账目拒绝布尔、小数与越界，不通过静默取整改变钱粮或设施等级。
static func _whole(value: Variant, low: int, high: int) -> bool:
	return typeof(value)==TYPE_INT and value>=low and value<=high

## 永久经营事实沿用全局编号，拒绝伪造的未来序号或另一种实例前缀。
static func _instance_id(id: String, prefix: String, next_id: int) -> bool:
	if not id.begins_with(prefix+":"): return false
	var suffix: String=id.trim_prefix(prefix+":")
	return suffix.is_valid_int() and str(int(suffix))==suffix and int(suffix)>0 and int(suffix)<next_id

## 将失败交给原子事务边界；调用者应丢弃失败候选，不保存一半的世界修改。
static func _fail(code: String) -> Dictionary:
	return {"ok":false,"code":code}
