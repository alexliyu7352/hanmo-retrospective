## 公职、驻军与军团人产账；复用同一人物、库存和半日，不决定六角胜负或战略军令是否完成。
extends RefCounted
const Factory=preload("res://core/world_factory.gd")
const Inventory=preload("res://core/inventory_rules.gd")
const Interaction=preload("res://core/interaction_rules.gd")
const Location=preload("res://core/world_location.gd")
const Estate=preload("res://core/estate_rules.gd")
const Ownership=preload("res://core/military_ownership.gd")
const KINDS=["office_accept","office_resign","army_pledge","army_form","army_resupply","army_return"]
const ACTION_KINDS=["office_accept","army_form","army_resupply","army_return"]
const GROUPS=["fit","wounded","dead","captured","deserted"]
const MAX_VALUE=2147483647

## 初次初始化只把作者驻军编成有来源的稳定名册，不发放目录之外的钱粮或私人资产。
static func initialize(state: Dictionary, data: Dictionary) -> Dictionary:
	if state.has("offices") or state.has("garrisons") or state.has("military_units") or state.has("armies"): return validate_state(state,data)
	var offices: Dictionary={}; var camps: Dictionary={}; var units: Dictionary={}
	for id in data.get("offices",{}): offices[id]={"id":id,"holder_actor_id":"","enabled":false,"appointment_id":""}
	for id in data.get("garrisons",{}):
		var definition: Dictionary=data.garrisons[id]
		var unit_ids: Array=[]
		for entry in definition.units:
			if units.has(entry.id): return _fail("DuplicateGarrisonUnit")
			var soldiers: Array=[]
			for index in range(entry.initial_soldiers): soldiers.append("soldier:"+entry.id+":"+str(index+1))
			units[entry.id]={"id":entry.id,"source_garrison_id":id,"organization_id":definition.organization_id,"garrison_id":id,"army_id":"","mobilizing_action_id":"","commander_actor_id":entry.commander_actor_id,"troop_type_id":entry.troop_type_id,"member_ids":soldiers.duplicate(),"fit":soldiers.duplicate(),"wounded":[],"dead":[],"captured":[],"deserted":[],"captors":{},"retainer_sources":{},"morale":70}
			unit_ids.append(entry.id)
		camps[id]={"id":id,"unit_ids":unit_ids,"prisoner_ids":[],"last_supply_tick":state.tick}
	state.offices=offices; state.garrisons=camps; state.military_units=units; state.armies={}
	return validate_state(state,data)

## 军令领域只借此核验当前本人公职；当面上下文、行动占用与具体军令目标由调用方另行核验。
static func ready_office(state: Dictionary, data: Dictionary, actor_id: String, office_id: String) -> Dictionary:
	var office: Variant=state.get("offices",{}).get(office_id)
	var actor: Variant=state.get("actors",{}).get(actor_id)
	if not office is Dictionary or not data.get("offices",{}).has(office_id) or not actor is Dictionary or not actor.get("alive",false) or not actor.get("available",false): return _fail("OfficeAuthorityRequired")
	if not office.get("enabled",false) or office.get("holder_actor_id")!=actor_id: return _fail("OfficeAuthorityRequired")
	var definition: Dictionary=data.offices[office_id]
	var owner: Dictionary=garrison_owner(state,data,definition.garrison_id)
	if not owner.ok: return owner
	if owner.organization_id!=definition.organization_id: return _fail("OfficeAuthorityLost")
	return {"ok":true,"office_id":office_id,"organization_id":definition.organization_id,"garrison_id":definition.garrison_id,"command_scope":definition.command_scope}

## 对外解析驻军当前合法组织；原旗号必须由完整占领链取代，无法解析时明确失败。
static func garrison_owner(state: Dictionary, data: Dictionary, garrison_id: String) -> Dictionary:
	return Ownership.garrison_owner(state,data,garrison_id)

## 供根层存档产权校验查询；仅公共清单中的容器能引用占领后的组织。
static func public_owner(state: Dictionary, data: Dictionary, field: String, container_id: String) -> Dictionary:
	return Ownership.public_owner(state,data,field,container_id)

## 实际驻防按当前位置与本部旗号查询；作者初始清单只作历史来源，绝不冒充后来到营的人员。
static func stationed_units(state: Dictionary, data: Dictionary, garrison_id: String) -> Array:
	var owner: Dictionary=garrison_owner(state,data,garrison_id)
	if not owner.ok: return []
	var result: Array=[]
	for id in _ids(state.get("military_units",{})):
		var unit: Dictionary=state.military_units[id]
		if unit.garrison_id==garrison_id and unit.army_id.is_empty() and unit.organization_id==owner.organization_id: result.append(id)
	return result

## 占领只在根层完成胜利回执后执行；预检全部公共资产，再原子改旗并登记同地实俘交接，真人后果交根层。
static func apply_occupation(state: Dictionary, data: Dictionary, receipt_id: String) -> Dictionary:
	var quote: Dictionary=Ownership.occupation_quote(state,data,receipt_id)
	if not quote.ok: return quote
	if quote.already_completed: return {"ok":true,"already_completed":true,"actor_changes":[],"disabled_office_ids":[]}
	var fact: Dictionary=state.facts[receipt_id]
	var old_owner: Dictionary={"kind":"organization","id":fact.from_organization_id}
	var new_owner: Dictionary={"kind":"organization","id":fact.to_organization_id}
	var custody: Dictionary={}
	for person in state.garrisons[fact.garrison_id].prisoner_ids:
		for unit in state.military_units.values():
			if person in unit.captured: custody[person]=unit
		if not custody.has(person) or not state.armies.has(custody[person].captors.get(person,"")): return _fail("GarrisonPrisonerCustodyMismatch")
	var manifest: Dictionary={"wallets":{},"inventories":{}}
	for id in fact.public_wallet_ids:
		var wallet: Dictionary=state.wallets[id]
		manifest.wallets[id]={"from_owner":old_owner.duplicate(),"to_owner":new_owner.duplicate(),"balance":wallet.balance}
		wallet.owner=new_owner.duplicate()
	for id in fact.public_inventory_ids:
		var inventory: Dictionary=state.inventories[id]; var lots: Array=[]; var protected: Array=[]
		for lot in inventory.lots:
			if lot.owner==old_owner:
				lots.append({"id":lot.id,"item_id":lot.item_id,"quantity":lot.quantity})
				lot.owner=new_owner.duplicate()
			else: protected.append(lot.duplicate(true))
		manifest.inventories[id]={"from_owner":old_owner.duplicate(),"to_owner":new_owner.duplicate(),"public_lots":lots,"protected_lots":protected}
		inventory.owner=new_owner.duplicate(); inventory.revision+=1
	var transfers: Array=[]; var changes: Array=[]
	for person in _ids(custody):
		var unit: Dictionary=custody[person]
		transfers.append({"person_id":person,"from_army_id":unit.captors[person],"to_army_id":fact.army_id})
		unit.captors[person]=fact.army_id
		state.armies[fact.army_id].prisoner_ids.append(person)
		if state.actors.has(person): changes.append({"actor_id":person,"status":"captured","captor_army_id":fact.army_id})
	state.garrisons[fact.garrison_id].prisoner_ids=[]
	var disabled: Array=[]
	for id in _ids(state.offices):
		if data.offices[id].garrison_id==fact.garrison_id and state.offices[id].enabled:
			state.offices[id].enabled=false; disabled.append(id)
	if not state.has("strongholds"): state.strongholds={}
	if not state.strongholds.has(fact.stronghold_id): state.strongholds[fact.stronghold_id]={}
	state.strongholds[fact.stronghold_id].owner_organization_id=fact.to_organization_id
	state.strongholds[fact.stronghold_id].occupation_id=receipt_id
	fact.assets_applied=true; fact.asset_manifest=manifest; fact.custody_transfers=transfers; fact.disabled_office_ids=disabled.duplicate(); fact.actor_changes=changes.duplicate(true)
	return {"ok":true,"already_completed":false,"actor_changes":changes,"disabled_office_ids":disabled}

## 返回作者地理及配额的副本，只以永久归属替换可支配组织；不改写作者数据。
static func _camp(state: Dictionary, data: Dictionary, garrison_id: String) -> Dictionary:
	var owner: Dictionary=garrison_owner(state,data,garrison_id)
	if not owner.ok: return owner
	var definition: Dictionary=data.garrisons[garrison_id].duplicate(true)
	definition.organization_id=owner.organization_id; definition.ownership_id=owner.occupation_id
	return {"ok":true,"definition":definition}

## 当前营地属于指定组织才可办理；不能因旧任命仍留历史姓名而继续调动敌方公库。
static func _camp_matches(state: Dictionary, data: Dictionary, garrison_id: String, organization_id: String) -> bool:
	var owner: Dictionary=garrison_owner(state,data,garrison_id)
	return owner.ok and owner.organization_id==organization_id

## 驻防必须符合最近一次真实归营收据；不能重放旧回执把后来转调的兵员搬回旧营。
static func _stationing_source(state: Dictionary, unit: Dictionary) -> bool:
	var latest: Dictionary={}
	for fact in state.facts.values():
		if not fact is Dictionary or fact.get("kind")!="military_return" or not unit.id in fact.get("unit_ids",[]): continue
		if not _whole(fact.get("tick"),0,state.tick): return false
		if latest.is_empty() or fact.tick>latest.tick: latest=fact
		elif fact.tick==latest.tick: return false
	if latest.is_empty(): return unit.garrison_id==unit.source_garrison_id
	var army: Dictionary=state.armies.get(latest.get("army_id",""),{})
	return latest.get("garrison_id")==unit.garrison_id and army.get("status")=="closed" and army.get("return_garrison_id")==unit.garrison_id and army.get("organization_id")==unit.organization_id

## 预览只列出真实编组和支付计划；私庄、公职与公共钱库保持各自权限。
static func preview(state: Dictionary, data: Dictionary, actor_id: String, kind: String, params: Dictionary) -> Dictionary:
	return _preview(state,data,actor_id,kind,params,"")

## 开始阶段允许本行动自己的占用，其他人的公职、部队或保留资源不能借报价夺取。
static func _preview(state: Dictionary, data: Dictionary, actor_id: String, kind: String, params: Dictionary, own_action: String) -> Dictionary:
	if not kind in KINDS: return _fail("UnknownMilitaryCommand")
	if not _ready(state,actor_id,own_action): return _fail("MilitaryActorUnavailable")
	var quote: Dictionary={"ok":true,"duration":1 if kind in ACTION_KINDS else 0,"params":params.duplicate(true)}
	if kind in ["office_accept","office_resign"]:
		var office_id: Variant=params.get("office_id")
		if not office_id is String or not data.get("offices",{}).has(office_id): return _fail("UnknownOffice")
		var definition: Dictionary=data.offices[office_id]
		var contact: Dictionary=Interaction.validate(state,data,actor_id,params.get("context"),kind,own_action)
		if not contact.ok: return contact
		if contact.context.role_id!=definition.appointing_role_id or data.roles[contact.context.role_id].get("organization_id")!=definition.organization_id: return _fail("OfficeAppointerRequired")
		if kind=="office_accept":
			var owner: Dictionary=garrison_owner(state,data,definition.garrison_id)
			if not owner.ok: return owner
			if owner.organization_id!=definition.organization_id: return _fail("OfficeAuthorityLost")
			if state.offices[office_id].enabled: return _fail("OfficeOccupied")
			for office in state.offices.values():
				if office.enabled and office.holder_actor_id==actor_id: return _fail("PublicAppointmentPending")
			for action in state.actions.values():
				if action.id!=own_action and action.kind=="office_accept" and action.params.get("terms",{}).get("office_id")==office_id: return _fail("OfficeAppointmentReserved")
		else:
			var authority: Dictionary=ready_office(state,data,actor_id,office_id)
			if not authority.ok: return authority
			for campaign in state.get("campaigns",{}).values():
				if campaign.get("actor_id")==actor_id and campaign.get("status")!="closed": return _fail("MilitaryOrderUnreported")
			if not _army_of(state,actor_id).is_empty(): return _fail("ArmyNotReturned")
		quote.context=contact.context
		quote.participants=[actor_id,contact.context.provider_actor_id]
		quote.terms={"office_id":office_id,"actor_id":actor_id,"organization_id":definition.organization_id,"place_id":contact.context.place_id,"provider_actor_id":contact.context.provider_actor_id}
		return quote
	if kind=="army_pledge": return _pledge_quote(state,data,actor_id,params)
	if kind=="army_form": return _formation_quote(state,data,actor_id,params,own_action)
	var army_id: Variant=params.get("army_id")
	if not army_id is String or not state.get("armies",{}).has(army_id): return _fail("UnknownArmy")
	var army: Dictionary=state.armies[army_id]
	if army.status!="active": return _fail("ArmyCommandRequired")
	var return_authority: Dictionary=_return_authority(state,data,army,actor_id) if kind=="army_return" else {"ok":army.commander_actor_id==actor_id,"authority":{"kind":"commander","id":actor_id}}
	if not return_authority.ok: return _fail("ArmyCommandRequired")
	var destination: Variant=params.get("garrison_id",army.garrison_id)
	if not destination is String: return _fail("UnknownGarrison")
	var resolved: Dictionary=_camp(state,data,destination)
	if not resolved.ok: return resolved
	var camp: Dictionary=resolved.definition
	if camp.organization_id!=army.organization_id: return _fail("FriendlyGarrisonRequired")
	if not _at(state,actor_id,camp.place_id) or not _at(state,army.commander_actor_id,camp.place_id) or _battle_pending(state,army.unit_ids): return _fail("ArmyMustReturnToCamp")
	if kind!="army_return":
		var authority: Dictionary=ready_office(state,data,actor_id,state.campaigns[army.order_id].office_id)
		if not authority.ok: return authority
	var people: Array=_return_people(state,army,actor_id) if kind=="army_return" else army.member_actor_ids.duplicate()
	for person in people:
		if not _ready(state,person,own_action) or (person!=actor_id and state.actors[person].location!={"kind":"InArmy","army_id":army.id}): return _fail("ArmyMemberUnavailable")
	quote.participants=people
	quote.terms={"army_id":army.id,"actor_id":actor_id,"garrison_id":camp.id,"source_garrison_id":army.garrison_id,"ownership_id":camp.ownership_id,"place_id":camp.place_id,"organization_id":army.organization_id}
	if kind=="army_return": quote.terms.authority=return_authority.authority
	if kind=="army_resupply":
		var provision: Dictionary=camp.duplicate(true)
		provision.food_item_id=data.garrisons[army.garrison_id].food_item_id
		var supply: Dictionary=_supply_quote(state,data,provision,params,army.inventory_id)
		if not supply.ok: return supply
		quote.terms.goods=supply.goods; quote.terms.money=supply.money
		quote.terms.source_inventory_id=camp.inventory_id; quote.terms.source_wallet_id=camp.wallet_id
	else:
		var returns: Dictionary=_return_assets(state,data,army,camp.id)
		if not returns.ok: return returns
	return quote

## 受任、编组和装卸需要实际半日；开工只保留来源，未完工不创建军团或发出装备。
static func begin(state: Dictionary, data: Dictionary, action: Dictionary, quote: Dictionary) -> Dictionary:
	if not _action(action) or not action.kind in ACTION_KINDS or state.facts.has(action.id): return _fail("InvalidMilitaryAction")
	var current: Dictionary=_preview(state,data,action.actor_id,action.kind,action.params,action.id)
	if not current.ok: return current
	if current!=quote or action.participants!=quote.participants or action.start_tick!=state.tick or action.remaining_ticks!=1: return _fail("StaleMilitaryQuote")
	for person in action.participants:
		if state.actors[person].action_id!=action.id: return _fail("MilitaryOccupationMissing")
	var terms: Dictionary=current.terms
	if action.kind in ["army_form","army_resupply"]:
		var owner: Dictionary={"kind":"organization","id":terms.organization_id}
		for item_id in _ids(terms.goods):
			var held: Dictionary=Inventory.reserve(state,terms.source_inventory_id,item_id,terms.goods[item_id],[owner],action.id)
			if not held.ok: return held
		state.wallets[terms.source_wallet_id].reserved[action.id]=terms.money
		if action.kind=="army_form":
			for id in terms.unit_ids: state.military_units[id].mobilizing_action_id=action.id
	action.params={"terms":terms.duplicate(true),"request":current.params.duplicate(true)}
	if current.has("context"): action.params.context=current.context.duplicate(true)
	state.facts[action.id]={"kind":"military_action","command":action.kind,"actor_id":action.actor_id,"start_tick":state.tick,"completed_tick":-1,"phase":"started","success":false,"terms":terms.duplicate(true),"context":current.get("context",{}).duplicate(true),"result":{}}
	return {"ok":true}

## 完工在世界 tick 增加前调用；失效行动释放自身资源并留下失败事实，不发幽灵任职或军团。
static func finish(state: Dictionary, data: Dictionary, action: Dictionary) -> Dictionary:
	var valid: Dictionary=_validate_action(state,data,action,true)
	if not valid.ok: return valid
	var fact: Dictionary=state.facts[action.id]
	if fact.phase=="completed": return {"ok":true,"success":fact.success,"already_completed":true,"details":fact.result.duplicate(true)}
	if action.remaining_ticks!=0 or state.tick+1!=action.start_tick+1: return _fail("MilitaryActionNotReady")
	var terms: Dictionary=action.params.terms
	var success: bool=true
	for person in action.participants:
		if not _ready(state,person,action.id) or state.actors[person].action_id!=action.id: success=false
	if not _at(state,action.actor_id,terms.place_id): success=false
	if action.kind=="office_accept":
		if state.offices[terms.office_id].enabled or not _camp_matches(state,data,data.offices[terms.office_id].garrison_id,terms.organization_id) or not Interaction.validate(state,data,action.actor_id,action.params.context,"office_accept",action.id).ok: success=false
	elif action.kind=="army_form":
		var campaign: Dictionary=state.campaigns.get(terms.order_id,{})
		if campaign.get("status")!="accepted" or campaign.get("army_id","")!="" or not ready_office(state,data,action.actor_id,terms.office_id).ok: success=false
		for person in action.participants:
			if not _at(state,person,terms.place_id): success=false
	else:
		if state.armies[terms.army_id].status!="active" or _battle_pending(state,state.armies[terms.army_id].unit_ids): success=false
		if not _camp_matches(state,data,terms.garrison_id,terms.organization_id): success=false
		if action.kind=="army_return" and (not _return_assets(state,data,state.armies[terms.army_id],terms.garrison_id).ok or not _return_authority(state,data,state.armies[terms.army_id],action.actor_id).ok or not _at(state,state.armies[terms.army_id].commander_actor_id,terms.place_id)): success=false
		if action.kind=="army_resupply":
			var army: Dictionary=state.armies[terms.army_id]
			if not ready_office(state,data,action.actor_id,state.campaigns[army.order_id].office_id).ok: success=false
			var weight: int=Inventory.weight(state.inventories[army.inventory_id],data)
			for item_id in terms.goods: weight+=terms.goods[item_id]*data.items[item_id].weight
			if weight>state.inventories[army.inventory_id].capacity or state.wallets[army.wallet_id].balance>MAX_VALUE-terms.money: success=false
	var details: Dictionary={}
	if success:
		match action.kind:
			"office_accept":
				state.offices[terms.office_id]={"id":terms.office_id,"holder_actor_id":action.actor_id,"enabled":true,"appointment_id":action.id}
				details.office_id=terms.office_id
			"army_form":
				details=_form(state,data,action,terms)
				if not details.ok: return details
			"army_resupply":
				var army: Dictionary=state.armies[terms.army_id]
				var moved: Dictionary=_deliver(state,terms,army.inventory_id,army.wallet_id,action.id)
				if not moved.ok: return moved
				details={"army_id":army.id}
			"army_return":
				details=_return(state,data,action,terms)
				if not details.ok: return details
	if action.kind=="army_form":
		for id in terms.unit_ids:
			if state.military_units[id].mobilizing_action_id==action.id: state.military_units[id].mobilizing_action_id=""
	Inventory.release(state,action.id)
	fact.phase="completed"; fact.completed_tick=state.tick+1; fact.success=success; fact.result=details.duplicate(true)
	return {"ok":true,"success":success,"code":"" if success else "MilitaryActionInterrupted","details":details}

## 私人借调和卸任只在已核验候选内提交；公共职权的退出不会收走个人或庄园财产。
static func apply(state: Dictionary, data: Dictionary, actor_id: String, kind: String, quote: Dictionary) -> Dictionary:
	if not kind in ["office_resign","army_pledge"]: return _fail("MilitaryActionRequired")
	var current: Dictionary=preview(state,data,actor_id,kind,quote.get("params",{}))
	if not current.ok: return current
	if current!=quote: return _fail("StaleMilitaryQuote")
	var id: String=Factory.next_id(state,"military")
	if kind=="office_resign":
		state.offices[current.terms.office_id].enabled=false
		state.facts[id]={"kind":"office_resignation","actor_id":actor_id,"office_id":current.terms.office_id,"tick":state.tick}
	else:
		var terms: Dictionary=current.terms
		state.facts[id]={"kind":"military_muster","actor_id":actor_id,"estate_id":terms.estate_id,"order_id":terms.order_id,"actor_ids":terms.actor_ids.duplicate(),"tick":state.tick}
		var params: Dictionary={"estate_id":terms.estate_id,"actor_ids":terms.actor_ids,"source_id":id}
		var pledge: Dictionary=Estate.preview(state,data,actor_id,"estate_muster",params)
		if not pledge.ok: return pledge
		var accepted: Dictionary=Estate.apply(state,data,actor_id,"estate_muster",pledge)
		if not accepted.ok: return accepted
	return {"ok":true,"fact_id":id,"source_id":id if kind=="army_pledge" else ""}

## 私庄出人先取得在场所有者或管事同意；用于预览的临时来源只存在于候选副本，不污染世界事实。
static func _pledge_quote(state: Dictionary, data: Dictionary, actor_id: String, params: Dictionary) -> Dictionary:
	var order_id: Variant=params.get("order_id")
	var estate_id: Variant=params.get("estate_id")
	if not order_id is String or not estate_id is String or not state.get("campaigns",{}).has(order_id) or not state.get("estates",{}).has(estate_id): return _fail("UnknownMilitaryPledge")
	if state.campaigns[order_id].get("status")!="accepted": return _fail("MilitaryOrderNotMusterable")
	var probe: Dictionary=state.duplicate(true)
	var source: String=Factory.next_id(probe,"military")
	probe.facts[source]={"kind":"military_muster","estate_id":estate_id}
	var pledge: Dictionary=Estate.preview(probe,data,actor_id,"estate_muster",{"estate_id":estate_id,"actor_ids":params.get("actor_ids"),"source_id":source})
	if not pledge.ok: return pledge
	return {"ok":true,"duration":0,"params":params.duplicate(true),"terms":{"estate_id":estate_id,"order_id":order_id,"actor_ids":pledge.actor_ids.duplicate()}}

## 按已受军令选择真实空闲驻军；本部指挥只能领一部，统帅可领多部且所有军官必须实际集合。
static func _formation_quote(state: Dictionary, data: Dictionary, actor_id: String, params: Dictionary, own_action: String) -> Dictionary:
	var order_id: Variant=params.get("order_id")
	if not order_id is String or not state.get("campaigns",{}).has(order_id): return _fail("UnknownMilitaryOrder")
	var campaign: Dictionary=state.campaigns[order_id]
	if campaign.get("status")!="accepted" or campaign.get("army_id","")!="" or campaign.get("actor_id")!=actor_id: return _fail("MilitaryOrderNotMusterable")
	var authority: Dictionary=ready_office(state,data,actor_id,campaign.office_id)
	if not authority.ok: return authority
	if campaign.garrison_id!=authority.garrison_id: return _fail("MilitaryOrderCampMismatch")
	var resolved: Dictionary=_camp(state,data,authority.garrison_id)
	if not resolved.ok: return resolved
	var camp: Dictionary=resolved.definition
	if camp.organization_id!=authority.organization_id or not _at(state,actor_id,camp.place_id) or not _army_of(state,actor_id).is_empty() or not Location.party_id(state,actor_id).is_empty(): return _fail("MilitaryAssemblyRequired")
	var ids: Variant=params.get("unit_ids")
	if not _unique_strings(ids) or ids.is_empty() or (authority.command_scope=="unit" and ids.size()!=1): return _fail("MilitaryCommandScope")
	var people: Array=[actor_id]; var commanders: Dictionary={}; var goods: Dictionary={}
	for index in range(ids.size()):
		var id: String=ids[index]
		var unit: Variant=state.military_units.get(id)
		if not unit is Dictionary or unit.garrison_id!=camp.id or unit.organization_id!=camp.organization_id or not unit.army_id.is_empty() or not unit.mobilizing_action_id in ["",own_action] or not unit.get("training_action_id","").is_empty() or unit.fit.is_empty(): return _fail("GarrisonUnitUnavailable")
		var commander: String=actor_id if index==0 else unit.commander_actor_id
		if commander in commanders.values() or not _ready(state,commander,own_action) or not _at(state,commander,camp.place_id) or not _army_of(state,commander).is_empty() or not Location.party_id(state,commander).is_empty(): return _fail("MilitaryOfficerUnavailable")
		commanders[id]=commander
		if not commander in people: people.append(commander)
	var retainers: Dictionary={}
	var named: Variant=params.get("retainer_ids",[])
	if not _unique_strings(named): return _fail("InvalidRetainerRoster")
	for person in named:
		if person in people or not _ready(state,person,own_action) or not _at(state,person,camp.place_id) or not _army_of(state,person).is_empty() or not Location.party_id(state,person).is_empty(): return _fail("RetainerAssemblyRequired")
		var pledged: Dictionary=_retainer_source(state,person,order_id)
		if not pledged.ok: return pledged
		retainers[person]={"estate_id":pledged.estate_id,"source_id":pledged.source_id,"unit_id":ids[0]}
		people.append(person)
	var mouths: int=people.size()
	for id in ids:
		var unit: Dictionary=state.military_units[id]
		var count: int=unit.fit.size()+unit.wounded.size()
		mouths+=count
		if id==ids[0]: count+=retainers.size()
		for equipment in _unit_definition(data,unit.source_garrison_id,id).equipment: goods[equipment.item_id]=goods.get(equipment.item_id,0)+count*equipment.quantity_per_person
	var requested: Dictionary=params.duplicate(true)
	if not requested.has("food_quantity"): requested.food_quantity=mouths*camp.ration_per_person*2
	var supply: Dictionary=_supply_quote(state,data,camp,requested,"")
	if not supply.ok: return supply
	for item_id in supply.goods: goods[item_id]=goods.get(item_id,0)+supply.goods[item_id]
	var owner: Dictionary={"kind":"organization","id":camp.organization_id}
	var weight: int=0
	for item_id in goods:
		if Inventory.count(state.inventories[camp.inventory_id],item_id,[owner])<goods[item_id]: return _fail("InsufficientMilitaryEquipmentOrFood")
		weight+=goods[item_id]*data.items[item_id].weight
	if weight>camp.army_capacity: return _fail("ArmyCapacityExceeded")
	var terms: Dictionary={"order_id":order_id,"office_id":campaign.office_id,"actor_id":actor_id,"garrison_id":camp.id,"ownership_id":camp.ownership_id,"place_id":camp.place_id,"organization_id":camp.organization_id,"unit_ids":ids.duplicate(),"commanders":commanders,"member_actor_ids":people.duplicate(),"retainers":retainers,"goods":goods,"money":supply.money,"source_inventory_id":camp.inventory_id,"source_wallet_id":camp.wallet_id}
	return {"ok":true,"duration":1,"params":params.duplicate(true),"participants":people,"terms":terms,"provisions":_provisions(camp,mouths,goods.get(camp.food_item_id,0))}

## 装运粮钱只能从本组织驻军真实公库取得，额外负载和资金保留不得凭空绕过。
static func _supply_quote(state: Dictionary, data: Dictionary, camp: Dictionary, params: Dictionary, destination: String) -> Dictionary:
	var quantity: Variant=params.get("food_quantity")
	var money: Variant=params.get("money",0)
	if not _whole(quantity,0,1000000) or not _whole(money,0,1000000): return _fail("InvalidMilitarySupply")
	var owner: Dictionary={"kind":"organization","id":camp.organization_id}
	if state.inventories[camp.inventory_id].owner!=owner or state.wallets[camp.wallet_id].owner!=owner: return _fail("MilitaryPublicAssetAuthority")
	if Inventory.count(state.inventories[camp.inventory_id],camp.food_item_id,[owner])<quantity or Inventory.spendable(state.wallets[camp.wallet_id])<money: return _fail("InsufficientMilitarySupply")
	if not destination.is_empty() and Inventory.weight(state.inventories[destination],data)+quantity*data.items[camp.food_item_id].weight>state.inventories[destination].capacity: return _fail("ArmyCapacityExceeded")
	return {"ok":true,"goods":{camp.food_item_id:quantity} if quantity>0 else {},"money":money}

## 一次性引用实际私庄借调事实；同一来源已编组过的人不能重新借原凭据再次出征。
static func _retainer_source(state: Dictionary, actor_id: String, order_id: String) -> Dictionary:
	for estate in state.get("estates",{}).values():
		var source: String=estate.detached.get(actor_id,"")
		var fact: Variant=state.facts.get(source)
		if not fact is Dictionary or fact.get("kind")!="military_muster" or fact.get("order_id")!=order_id or not actor_id in fact.get("actor_ids",[]) or fact.get("estate_id")!=estate.id: continue
		for army in state.armies.values():
			if army.retainers.get(actor_id,{}).get("source_id")==source: return _fail("RetainerPledgeAlreadyUsed")
		return {"ok":true,"estate_id":estate.id,"source_id":source}
	return _fail("PrivateRetainerConsentRequired")

## 编组完工才新建同组织容器，领用已保留钱货，关联实际军官与私兵；军令绑定由Campaign在同一候选完成。
static func _form(state: Dictionary, data: Dictionary, action: Dictionary, terms: Dictionary) -> Dictionary:
	var id: String=Factory.next_id(state,"army")
	var inventory_id: String="army_inventory:"+id; var wallet_id: String="army_wallet:"+id
	var owner: Dictionary={"kind":"organization","id":terms.organization_id}
	state.inventories[inventory_id]={"id":inventory_id,"owner":owner.duplicate(),"capacity":data.garrisons[terms.garrison_id].army_capacity,"revision":0,"lots":[]}
	state.wallets[wallet_id]={"id":wallet_id,"owner":owner.duplicate(),"balance":0,"reserved":{}}
	var moved: Dictionary=_deliver(state,terms,inventory_id,wallet_id,action.id)
	if not moved.ok: return moved
	state.armies[id]={"id":id,"organization_id":terms.organization_id,"commander_actor_id":action.actor_id,"member_actor_ids":terms.member_actor_ids.duplicate(),"unit_ids":terms.unit_ids.duplicate(),"inventory_id":inventory_id,"wallet_id":wallet_id,"order_id":terms.order_id,"garrison_id":terms.garrison_id,"status":"active","last_supply_tick":state.tick+1,"retainers":terms.retainers.duplicate(true),"prisoner_ids":[],"formation_source":action.id}
	for unit_id in terms.unit_ids:
		var unit: Dictionary=state.military_units[unit_id]
		unit.army_id=id; unit.commander_actor_id=terms.commanders[unit_id]; unit.garrison_id=""
	for person in _ids(terms.retainers):
		var pledge: Dictionary=terms.retainers[person]
		var unit: Dictionary=state.military_units[pledge.unit_id]
		unit.member_ids.append(person); unit.fit.append(person); unit.retainer_sources[person]=pledge.source_id
	for person in terms.member_actor_ids:
		if person!=action.actor_id: state.actors[person].location={"kind":"InArmy","army_id":id}
	return {"ok":true,"army_id":id,"order_id":terms.order_id}

## 沿用库存保留与转移原语，实物和余额只从已冻结公共来源移动一次。
static func _deliver(state: Dictionary, terms: Dictionary, inventory_id: String, wallet_id: String, action_id: String) -> Dictionary:
	if state.wallets[wallet_id].balance>MAX_VALUE-terms.money: return _fail("MoneyOverflow")
	var owner: Dictionary={"kind":"organization","id":terms.organization_id}
	for item_id in _ids(terms.goods):
		var moved: Dictionary=Inventory.transfer(state,terms.source_inventory_id,inventory_id,item_id,terms.goods[item_id],[owner],action_id)
		if not moved.ok: return moved
	state.wallets[terms.source_wallet_id].balance-=terms.money; state.wallets[wallet_id].balance+=terms.money
	return {"ok":true}

## 每个共享半日给活动军团结算一次口粮，伤员、军官和实俘取并集计数；缺粮降低士气但不增发资源。
static func step(state: Dictionary, data: Dictionary, finish_tick: int) -> Dictionary:
	var valid: Dictionary=validate_state(state,data)
	if not valid.ok: return valid
	if finish_tick==state.tick: return {"ok":true,"events":[],"applied":false}
	if finish_tick!=state.tick+1: return _fail("MilitarySupplyClockMismatch")
	var events: Array=[]
	for id in _ids(state.garrisons):
		var camp: Dictionary=state.garrisons[id]
		var definition: Dictionary=_camp(state,data,id).definition
		var stationed: Array=[]; var mouths: Dictionary={}
		for unit_id in stationed_units(state,data,id):
			var unit: Dictionary=state.military_units[unit_id]
			if unit.garrison_id!=id: continue
			stationed.append(unit_id)
			for person in unit.fit+unit.wounded: mouths[person]=true
			if state.actors[unit.commander_actor_id].alive and state.actors[unit.commander_actor_id].available and _at(state,unit.commander_actor_id,definition.place_id) and _army_of(state,unit.commander_actor_id).is_empty(): mouths[unit.commander_actor_id]=true
		for unit_id in stationed:
			var training: Dictionary=_training_mouth(state,data,state.military_units[unit_id],id,definition.place_id)
			if not training.ok: return training
			if not training.actor_id.is_empty(): mouths[training.actor_id]=true
		for person in camp.prisoner_ids: mouths[person]=true
		events.append(_feed(state,definition,definition.inventory_id,stationed,mouths,finish_tick,"",id))
		camp.last_supply_tick=finish_tick
	for id in _ids(state.armies):
		var army: Dictionary=state.armies[id]
		if army.status!="active": continue
		var camp: Dictionary=data.garrisons[army.garrison_id].duplicate(true)
		camp.organization_id=army.organization_id; camp.ownership_id=state.facts[army.formation_source].terms.get("ownership_id","")
		var mouths: Dictionary=_army_mouths(state,army)
		events.append(_feed(state,camp,army.inventory_id,army.unit_ids,mouths,finish_tick,id,""))
		army.last_supply_tick=finish_tick
	return {"ok":true,"events":events,"applied":true}

## 供粮与只读预览共用真实名册并集；伤员、活军官与实俘均照常计口，借调真人不会重复计数。
static func _army_mouths(state: Dictionary, army: Dictionary) -> Dictionary:
	var mouths: Dictionary={}
	for person in army.member_actor_ids:
		if state.actors[person].alive: mouths[person]=true
	for unit_id in army.unit_ids:
		var unit: Dictionary=state.military_units[unit_id]
		for person in unit.fit+unit.wounded: mouths[person]=true
	for person in army.prisoner_ids: mouths[person]=true
	return mouths

## 查询本军尚未预留的实际口粮与下半日需求；未知或已清算军团不提供虚构续航，也不改状态和随机流。
static func supply_preview(state: Dictionary, data: Dictionary, army_id: String) -> Dictionary:
	var army: Dictionary=state.get("armies",{}).get(army_id,{})
	if army.is_empty(): return _fail("UnknownArmy")
	if army.status!="active": return _fail("ArmyNotActive")
	var camp: Dictionary=data.garrisons[army.garrison_id]
	var owner: Dictionary={"kind":"organization","id":army.organization_id}
	var quantity: int=Inventory.count(state.inventories[army.inventory_id],camp.food_item_id,[owner])
	return _provisions(camp,_army_mouths(state,army).size(),quantity)

## 估算只使用当前名册及作者定额；完整半日向下取整，不承诺未来战损、俘虏或补给后的需求不变。
static func _provisions(camp: Dictionary, mouths: int, available: int) -> Dictionary:
	var required: int=mouths*camp.ration_per_person
	@warning_ignore("integer_division")
	var ticks: int=available/required if required>0 else 0
	return {"ok":true,"food_item_id":camp.food_item_id,"mouth_count":mouths,"required":required,"available":available,"full_ticks":ticks,"shortage":maxi(0,required-available)}

## 驻军与行军共用一次供给规则，按实际看管处记账；任一真人在口粮名单中只出现一次。
static func _feed(state: Dictionary, definition: Dictionary, inventory_id: String, unit_ids: Array, mouths: Dictionary, tick: int, army_id: String, garrison_id: String) -> String:
	var required: int=mouths.size()*definition.ration_per_person
	var inventory: Dictionary=state.inventories[inventory_id]
	var owner: Dictionary={"kind":"organization","id":definition.organization_id}
	var quantity: int=mini(required,Inventory.count(inventory,definition.food_item_id,[owner]))
	_consume(inventory,definition.food_item_id,quantity,owner)
	var shortage: int=required-quantity
	for id in unit_ids:
		var unit: Dictionary=state.military_units[id]
		if shortage>0 and not unit.fit.is_empty(): unit.morale=maxi(1,unit.morale-8)
	for person in mouths:
		if shortage>0 and state.actors.has(person): state.actors[person].fatigue=mini(100,state.actors[person].fatigue+4)
	var source: String=Factory.next_id(state,"military")
	state.facts[source]={"kind":"military_supply","organization_id":definition.organization_id,"ownership_id":definition.get("ownership_id",""),"army_id":army_id,"garrison_id":garrison_id,"tick":tick,"item_id":definition.food_item_id,"mouth_ids":_ids(mouths),"required":required,"consumed":quantity,"shortage":shortage}
	return source

## 接收根层已完成战斗的最终名册；只做五类转移，永久来源幂等，真人后果交根层统一写回位置和生命。
static func settle_battle(state: Dictionary, data: Dictionary, battle_id: String, unit_results: Dictionary, captors: Dictionary) -> Dictionary:
	var source: Variant=state.facts.get(battle_id)
	if not _fields(source,{"kind":TYPE_STRING,"phase":TYPE_STRING,"tick":TYPE_INT,"army_ids":TYPE_ARRAY,"unit_ids":TYPE_ARRAY,"result":TYPE_DICTIONARY}) or source.kind!="military_battle" or source.phase!="completed" or not _whole(source.tick,0,state.tick+1): return _fail("MilitaryBattleSourceRequired")
	for id in state.facts:
		var previous: Variant=state.facts[id]
		if previous is Dictionary and previous.get("kind")=="military_casualties" and previous.get("battle_id")==battle_id: return {"ok":true,"already_completed":true,"receipt_id":id,"actor_changes":[]}
	if not _unique_strings(source.unit_ids) or not _unique_strings(source.army_ids) or _ids(unit_results)!=_sorted(source.unit_ids): return _fail("MilitaryBattleRosterMismatch")
	var new_captors: Dictionary={}; var changes: Array=[]; var results: Dictionary={}
	for id in source.unit_ids:
		var unit: Variant=state.military_units.get(id)
		var final: Variant=unit_results[id]
		if not unit is Dictionary or not final is Dictionary or not unit.army_id in source.army_ids or not state.armies.has(unit.army_id) or state.armies[unit.army_id].status!="active": return _fail("MilitaryBattleUnitUnavailable")
		if final.get("army_id")!=unit.army_id or final.get("commander_actor_id")!=unit.commander_actor_id or final.get("troop_type_id")!=unit.troop_type_id or final.get("member_ids")!=unit.member_ids or not _classified(final): return _fail("MilitaryBattleRosterMismatch")
		if not _whole(final.get("morale"),0,100) or (not final.fit.is_empty() and final.morale==0): return _fail("InvalidMilitaryMorale")
		var updated: Dictionary={}
		for group in GROUPS: updated[group]=final[group].duplicate()
		updated.member_ids=unit.member_ids.duplicate(); updated.morale=final.morale; updated.captors=unit.captors.duplicate()
		for group in GROUPS:
			for person in unit[group]:
				var after: String=_group(final,person)
				if group in ["dead","captured","deserted"] and after!=group or group=="wounded" and after=="fit": return _fail("MilitaryCasualtyReversal")
				if after=="captured" and group!="captured":
					var keeper: Variant=captors.get(person)
					if not keeper is String or keeper==unit.army_id or not keeper in source.army_ids or not state.armies.has(keeper) or state.armies[keeper].status!="active" or state.armies[keeper].organization_id==state.armies[unit.army_id].organization_id: return _fail("MilitaryCaptorRequired")
					updated.captors[person]=keeper; new_captors[person]=keeper
				if state.actors.has(person) and after!=group:
					changes.append({"actor_id":person,"status":after,"army_id":unit.army_id,"captor_army_id":updated.captors.get(person,"")})
		results[id]=updated
	if _ids(captors)!=_ids(new_captors): return _fail("UnexpectedMilitaryCaptor")
	for id in results:
		var unit: Dictionary=state.military_units[id]
		for key in results[id]: unit[key]=results[id][key].duplicate(true) if results[id][key] is Dictionary or results[id][key] is Array else results[id][key]
	for person in new_captors: state.armies[new_captors[person]].prisoner_ids.append(person)
	for change in changes:
		if change.status in ["dead","captured","deserted"]: state.armies[change.army_id].member_actor_ids.erase(change.actor_id)
	var receipt: String=Factory.next_id(state,"military")
	state.facts[receipt]={"kind":"military_casualties","battle_id":battle_id,"tick":source.tick,"units":results.duplicate(true),"actor_changes":changes.duplicate(true)}
	return {"ok":true,"receipt_id":receipt,"actor_changes":changes}

## 回营期间仍会消耗本半日口粮，交接以完工时剩余实物为准；不把别人的寄存或预留财物收进公库。
static func _return_assets(state: Dictionary, data: Dictionary, army: Dictionary, garrison_id: String) -> Dictionary:
	var resolved: Dictionary=_camp(state,data,garrison_id)
	if not resolved.ok: return resolved
	var camp: Dictionary=resolved.definition
	if camp.organization_id!=army.organization_id: return _fail("FriendlyGarrisonRequired")
	var owner: Dictionary={"kind":"organization","id":army.organization_id}
	var inventory: Dictionary=state.inventories[army.inventory_id]
	var wallet: Dictionary=state.wallets[army.wallet_id]
	if not wallet.reserved.is_empty(): return _fail("MilitaryAssetsReserved")
	var goods: Dictionary={}
	for lot in inventory.lots:
		if lot.owner!=owner or not lot.reserved_by.is_empty(): return _fail("MilitaryForeignOrReservedCargo")
		goods[lot.item_id]=goods.get(lot.item_id,0)+lot.quantity
	if Inventory.weight(state.inventories[camp.inventory_id],data)+Inventory.weight(inventory,data)>state.inventories[camp.inventory_id].capacity: return _fail("GarrisonCapacityExceeded")
	if state.wallets[camp.wallet_id].balance>MAX_VALUE-wallet.balance: return _fail("MoneyOverflow")
	return {"ok":true,"goods":goods,"money":wallet.balance}

## 回营交回真实钱货、兵员和看管名单，解除现场军官占用；私兵仍须实际回原庄园办理归还。
static func _return(state: Dictionary, data: Dictionary, action: Dictionary, terms: Dictionary) -> Dictionary:
	var army: Dictionary=state.armies[terms.army_id]
	var cargo: Dictionary=_return_assets(state,data,army,terms.garrison_id)
	if not cargo.ok: return cargo
	var camp: Dictionary=data.garrisons[terms.garrison_id]
	var owner: Dictionary={"kind":"organization","id":army.organization_id}
	for item_id in _ids(cargo.goods):
		var moved: Dictionary=Inventory.transfer(state,army.inventory_id,camp.inventory_id,item_id,cargo.goods[item_id],[owner])
		if not moved.ok: return moved
	state.wallets[army.wallet_id].balance-=cargo.money; state.wallets[camp.wallet_id].balance+=cargo.money
	var returned: Array=[]; var lost: Array=[]; var changes: Array=[]
	for person in _ids(army.retainers):
		var pledge: Dictionary=army.retainers[person]
		var unit: Dictionary=state.military_units[pledge.unit_id]
		var status: String=_group(unit,person)
		if status in ["fit","wounded"]:
			unit[status].erase(person); unit.member_ids.erase(person); unit.retainer_sources.erase(person)
			returned.append(person)
		elif status in ["dead","deserted"]:
			lost.append(person)
			if state.estates[pledge.estate_id].detached.get(person)==pledge.source_id: state.estates[pledge.estate_id].detached.erase(person)
	for id in army.unit_ids:
		state.military_units[id].army_id=""; state.military_units[id].garrison_id=terms.garrison_id
	for person in army.member_actor_ids:
		if person!=army.commander_actor_id: state.actors[person].location=Factory.settlement_location(data,camp.place_id)
	for person in army.prisoner_ids:
		state.garrisons[terms.garrison_id].prisoner_ids.append(person)
		if state.actors.has(person): changes.append({"actor_id":person,"status":"captured","captor_army_id":army.id,"garrison_id":terms.garrison_id})
	var prisoners: Array=army.prisoner_ids.duplicate()
	army.prisoner_ids=[]; army.status="closed"; army.return_garrison_id=terms.garrison_id
	var receipt: String=Factory.next_id(state,"military")
	state.facts[receipt]={"kind":"military_return","army_id":army.id,"order_id":army.order_id,"garrison_id":terms.garrison_id,"source_garrison_id":army.garrison_id,"ownership_id":terms.ownership_id,"actor_id":action.actor_id,"authority":terms.authority.duplicate(true),"tick":state.tick+1,"goods":cargo.goods.duplicate(true),"money":cargo.money,"unit_ids":army.unit_ids.duplicate(),"returned_retainers":returned,"lost_retainers":lost,"prisoner_ids":prisoners,"actor_changes":changes.duplicate(true)}
	return {"ok":true,"receipt_id":receipt,"army_id":army.id,"order_id":army.order_id,"actor_changes":changes}

## 原统帅死亡后，仅实际任职的军令公职或报告人可在回营地点代办清算；不会远程接管野外军队。
static func _return_authority(state: Dictionary, data: Dictionary, army: Dictionary, actor_id: String) -> Dictionary:
	if army.commander_actor_id==actor_id: return {"ok":true,"authority":{"kind":"commander","id":actor_id}}
	if state.actors[army.commander_actor_id].alive: return _fail("ArmyCommandRequired")
	var campaign: Dictionary=state.campaigns.get(army.order_id,{})
	var office_id: String=campaign.get("office_id","")
	if ready_office(state,data,actor_id,office_id).ok: return {"ok":true,"authority":{"kind":"office","id":office_id}}
	var report: Variant=data.get("military_orders",{}).get(campaign.get("template_id",""),{}).get("report_role_id")
	if report is String and data.roles.has(report) and data.roles[report].get("organization_id")==army.organization_id and state.roles.get(report,{}).get("enabled",false) and state.roles[report].get("actor_id")==actor_id: return {"ok":true,"authority":{"kind":"report_role","id":report}}
	return _fail("ArmyCommandRequired")

## 归营实际参与者保留在世军官和部曲，合法代办者加入办理；死亡人物不被伪装为还可接受行动占用。
static func _return_people(state: Dictionary, army: Dictionary, actor_id: String) -> Array:
	var people: Array=[]
	for person in army.member_actor_ids:
		if state.actors[person].alive: people.append(person)
	if not actor_id in people: people.append(actor_id)
	return people

## 存档边界核验军事行动，不根据缺失的编组事实补发人物或粮饷。
static func validate_action(state: Dictionary, data: Dictionary, action: Dictionary) -> Dictionary:
	return _validate_action(state,data,action,false)

## 在稳定边界核验保留名册与钱货；终结重放只读永久结果，不重复编组或卸下同一批资产。
static func _validate_action(state: Dictionary, data: Dictionary, action: Dictionary, finishing: bool) -> Dictionary:
	if not _action(action) or not action.kind in ACTION_KINDS or not action.params.get("terms") is Dictionary or not action.params.get("request") is Dictionary: return _fail("InvalidMilitaryAction")
	var fact: Variant=state.facts.get(action.id)
	if not _fields(fact,{"kind":TYPE_STRING,"command":TYPE_STRING,"actor_id":TYPE_STRING,"start_tick":TYPE_INT,"completed_tick":TYPE_INT,"phase":TYPE_STRING,"success":TYPE_BOOL,"terms":TYPE_DICTIONARY,"context":TYPE_DICTIONARY,"result":TYPE_DICTIONARY}) or fact.kind!="military_action" or fact.command!=action.kind or fact.actor_id!=action.actor_id or fact.start_tick!=action.start_tick or fact.terms!=action.params.terms or fact.context!=action.params.get("context",{}): return _fail("MilitaryActionFactMismatch")
	if fact.phase=="completed":
		return {"ok":true} if finishing and fact.completed_tick==action.start_tick+1 and fact.completed_tick<=state.tick+1 else _fail("MilitaryActionAlreadyCompleted")
	if fact.phase!="started" or fact.completed_tick!=-1 or fact.success or not fact.result.is_empty() or not _whole(action.remaining_ticks,0 if finishing else 1,1): return _fail("InvalidMilitaryActionPhase")
	if not finishing and action.start_tick+1-action.remaining_ticks!=state.tick: return _fail("MilitaryActionClockMismatch")
	var terms: Dictionary=action.params.terms
	if terms.get("actor_id")!=action.actor_id or not data.places.has(terms.get("place_id")) or not data.organizations.has(terms.get("organization_id")): return _fail("InvalidMilitaryTerms")
	var expected: Array=[]
	if action.kind=="office_accept":
		if not data.get("offices",{}).has(terms.get("office_id")) or not terms.get("provider_actor_id") is String or not state.actors.has(terms.provider_actor_id) or terms.provider_actor_id==action.actor_id or terms.get("provider_actor_id")!=fact.context.get("provider_actor_id"): return _fail("InvalidOfficeAppointment")
		var definition: Dictionary=data.offices[terms.office_id]
		if terms.organization_id!=definition.organization_id or fact.context.get("role_id")!=definition.appointing_role_id or fact.context.get("place_id")!=terms.place_id or data.roles[definition.appointing_role_id].place_id!=terms.place_id: return _fail("OfficeAppointmentAuthorityMismatch")
		expected=[action.actor_id,terms.provider_actor_id]
	elif action.kind=="army_form":
		if not _fields(terms,{"order_id":TYPE_STRING,"office_id":TYPE_STRING,"garrison_id":TYPE_STRING,"unit_ids":TYPE_ARRAY,"commanders":TYPE_DICTIONARY,"member_actor_ids":TYPE_ARRAY,"retainers":TYPE_DICTIONARY}): return _fail("InvalidArmyFormation")
		if not state.get("campaigns",{}).has(terms.order_id) or not _unique_strings(terms.unit_ids) or terms.unit_ids.is_empty() or not _unique_strings(terms.member_actor_ids): return _fail("InvalidArmyFormation")
		for id in terms.unit_ids:
			if not state.military_units.has(id) or state.military_units[id].mobilizing_action_id!=action.id or not state.military_units[id].army_id.is_empty(): return _fail("MilitaryUnitReservationMismatch")
		var formed: Dictionary=_validate_formation_terms(state,data,terms,action.params.request)
		if not formed.ok: return formed
		expected=terms.member_actor_ids
	else:
		if not state.armies.has(terms.get("army_id")): return _fail("UnknownArmy")
		var army: Dictionary=state.armies[terms.army_id]
		if not data.garrisons.has(terms.get("garrison_id")) or terms.get("source_garrison_id")!=army.garrison_id or (action.kind!="army_return" and terms.actor_id!=army.commander_actor_id) or terms.organization_id!=army.organization_id or terms.place_id!=data.garrisons[terms.garrison_id].place_id: return _fail("ArmyActionAuthorityMismatch")
		if not terms.get("ownership_id") is String: return _fail("MilitaryOwnershipSourceMissing")
		var historical: Dictionary=Ownership.historical_owner(state,data,terms.garrison_id,terms.ownership_id)
		if not historical.ok: return historical
		if historical.organization_id!=army.organization_id: return _fail("ArmyActionAuthorityMismatch")
		if action.kind=="army_return" and (not _fields(terms.get("authority"),{"kind":TYPE_STRING,"id":TYPE_STRING}) or not terms.authority.kind in ["commander","office","report_role"]): return _fail("ArmyReturnAuthorityRequired")
		expected=_return_people(state,army,action.actor_id) if action.kind=="army_return" else army.member_actor_ids
	if action.participants!=expected or not _unique_strings(expected): return _fail("MilitaryParticipantsMismatch")
	if not finishing:
		for person in expected:
			if not state.actors.has(person) or state.actors[person].action_id!=action.id: return _fail("MilitaryOccupationMissing")
	if action.kind in ["army_form","army_resupply"]:
		if not _fields(terms,{"goods":TYPE_DICTIONARY,"money":TYPE_INT,"source_inventory_id":TYPE_STRING,"source_wallet_id":TYPE_STRING}) or not state.inventories.has(terms.source_inventory_id) or not state.wallets.has(terms.source_wallet_id) or not _whole(terms.money,0,1000000): return _fail("InvalidMilitarySupplyTerms")
		var owner: Dictionary={"kind":"organization","id":terms.organization_id}
		if not data.garrisons.has(terms.get("garrison_id")): return _fail("UnknownGarrison")
		var camp: Dictionary=_camp(state,data,terms.garrison_id).definition
		if camp.inventory_id!=terms.source_inventory_id or camp.wallet_id!=terms.source_wallet_id or camp.organization_id!=terms.organization_id or camp.ownership_id!=terms.get("ownership_id"): return _fail("MilitarySupplySourceMismatch")
		if state.wallets[terms.source_wallet_id].owner!=owner or state.inventories[terms.source_inventory_id].owner!=owner or state.wallets[terms.source_wallet_id].reserved.get(action.id)!=terms.money: return _fail("MilitaryEscrowMismatch")
		for item_id in terms.goods:
			if not data.items.has(item_id) or not _whole(terms.goods[item_id],1,1000000) or Inventory.count(state.inventories[terms.source_inventory_id],item_id,[owner],action.id)!=terms.goods[item_id]: return _fail("MilitaryEquipmentReservationMismatch")
		if action.kind=="army_resupply":
			var quantity: Variant=action.params.request.get("food_quantity")
			if not _whole(quantity,0,1000000) or terms.goods!=({data.garrisons[state.armies[terms.army_id].garrison_id].food_item_id:quantity} if quantity>0 else {}) or terms.money!=action.params.request.get("money",0): return _fail("MilitarySupplyTermsMismatch")
	return {"ok":true}

## 冻结编组仍须符合原军令、真实成员与作者装具定额；同时篡改条款和短回执不能减免领装来源。
static func _validate_formation_terms(state: Dictionary, data: Dictionary, terms: Dictionary, request: Dictionary) -> Dictionary:
	var campaign: Dictionary=state.campaigns[terms.order_id]
	if terms.office_id!=campaign.get("office_id") or terms.garrison_id!=campaign.get("garrison_id") or terms.actor_id!=campaign.get("actor_id") or request.get("order_id")!=terms.order_id or request.get("unit_ids")!=terms.unit_ids or not data.offices.has(terms.office_id): return _fail("MilitaryFormationOrderMismatch")
	var office: Dictionary=data.offices[terms.office_id]
	if not data.garrisons.has(terms.garrison_id) or terms.organization_id!=office.organization_id or terms.garrison_id!=office.garrison_id or (office.command_scope=="unit" and terms.unit_ids.size()!=1): return _fail("MilitaryCommandScope")
	var camp: Dictionary=_camp(state,data,terms.garrison_id).definition
	if terms.place_id!=camp.place_id or terms.organization_id!=camp.organization_id or terms.get("ownership_id")!=camp.ownership_id or terms.commanders.size()!=terms.unit_ids.size() or not _fields(terms,{"goods":TYPE_DICTIONARY,"money":TYPE_INT}): return _fail("InvalidMilitaryFormationTerms")
	var people: Array=[terms.actor_id]; var goods: Dictionary={}; var mouths: int=0
	for index in range(terms.unit_ids.size()):
		var id: String=terms.unit_ids[index]
		var unit: Dictionary=state.military_units[id]
		var commander: Variant=terms.commanders.get(id)
		if not commander is String or not state.actors.has(commander) or commander!=(terms.actor_id if index==0 else unit.commander_actor_id) or (index>0 and commander in people) or unit.garrison_id!=camp.id or unit.organization_id!=camp.organization_id: return _fail("MilitaryCommanderMismatch")
		if index>0: people.append(commander)
		var count: int=unit.fit.size()+unit.wounded.size()
		mouths+=count
		if index==0: count+=terms.retainers.size()
		for equipment in _unit_definition(data,unit.source_garrison_id,id).equipment: goods[equipment.item_id]=goods.get(equipment.item_id,0)+count*equipment.quantity_per_person
	var named: Variant=request.get("retainer_ids",[])
	if not _unique_strings(named) or _sorted(named)!=_ids(terms.retainers): return _fail("MilitaryRetainerTermsMismatch")
	for person in named:
		var pledge: Variant=terms.retainers[person]
		if not _fields(pledge,{"estate_id":TYPE_STRING,"source_id":TYPE_STRING,"unit_id":TYPE_STRING}) or pledge.unit_id!=terms.unit_ids[0] or person in people: return _fail("MilitaryRetainerTermsMismatch")
		var source: Dictionary=_retainer_source(state,person,terms.order_id)
		if not source.ok or source.estate_id!=pledge.estate_id or source.source_id!=pledge.source_id: return _fail("PrivateRetainerConsentRequired")
		people.append(person)
	if terms.member_actor_ids!=people: return _fail("MilitaryParticipantsMismatch")
	mouths+=people.size()
	var quantity: Variant=request.get("food_quantity",mouths*camp.ration_per_person*2)
	if not _whole(quantity,0,1000000) or not _whole(terms.money,0,1000000) or terms.money!=request.get("money",0): return _fail("InvalidMilitarySupplyTerms")
	if quantity>0: goods[camp.food_item_id]=goods.get(camp.food_item_id,0)+quantity
	if terms.goods!=goods: return _fail("MilitaryEquipmentTermsMismatch")
	return {"ok":true}

## 校验作者驻军来源、五类守恒、实际成员与公共资产；本次复用原始兵员索引，不保存跨提交缓存。
static func validate_state(state: Dictionary, data: Dictionary) -> Dictionary:
	for key in ["offices","garrisons","military_units","armies"]:
		if not state.get(key) is Dictionary: return _fail("InvalidMilitaryState")
	if _ids(state.offices)!=_ids(data.get("offices",{})) or _ids(state.garrisons)!=_ids(data.get("garrisons",{})): return _fail("MissingMilitaryDefinitions")
	var ownership: Dictionary=Ownership.validate_state(state,data)
	if not ownership.ok: return ownership
	# 历史供给允许已死亡的作者兵员和仍有身份的真人，不能仅收集当前可战或在营人数。
	var known_members: Dictionary={}
	for actor_id in state.actors: known_members[actor_id]=true
	var units: Dictionary={}; var members: Dictionary={}; var actors: Dictionary={}; var inventories: Dictionary={}; var wallets: Dictionary={}; var office_holders: Dictionary={}; var captured: Dictionary={}
	for id in state.offices:
		var office: Variant=state.offices[id]
		if not _fields(office,{"id":TYPE_STRING,"holder_actor_id":TYPE_STRING,"enabled":TYPE_BOOL,"appointment_id":TYPE_STRING}) or office.id!=id: return _fail("InvalidOfficeState")
		if not office.holder_actor_id.is_empty() and not state.actors.has(office.holder_actor_id): return _fail("UnknownOfficeHolder")
		if office.enabled:
			if not _camp_matches(state,data,data.offices[id].garrison_id,data.offices[id].organization_id): return _fail("OfficeAuthorityLost")
			var source: Variant=state.facts.get(office.appointment_id)
			if not source is Dictionary or source.get("kind")!="military_action" or source.get("command")!="office_accept" or source.get("phase")!="completed" or source.get("success")!=true or source.get("actor_id")!=office.holder_actor_id or source.get("terms",{}).get("office_id")!=id: return _fail("OfficeAppointmentSourceMissing")
			if office_holders.has(office.holder_actor_id): return _fail("DuplicatePublicAppointment")
			office_holders[office.holder_actor_id]=id
		elif office.holder_actor_id.is_empty()!=office.appointment_id.is_empty(): return _fail("InvalidOfficeHistory")
	for id in state.garrisons:
		var camp: Variant=state.garrisons[id]
		var definition: Dictionary=data.garrisons[id]
		if not _fields(camp,{"id":TYPE_STRING,"unit_ids":TYPE_ARRAY,"prisoner_ids":TYPE_ARRAY,"last_supply_tick":TYPE_INT}) or camp.id!=id or not _unique_strings(camp.prisoner_ids) or camp.last_supply_tick!=state.tick: return _fail("InvalidGarrisonState")
		var owner: Dictionary={"kind":"organization","id":garrison_owner(state,data,id).organization_id}
		if not data.organizations.has(definition.organization_id) or not data.places.has(definition.place_id) or state.inventories.get(definition.inventory_id,{}).get("owner")!=owner or state.wallets.get(definition.wallet_id,{}).get("owner")!=owner: return _fail("GarrisonPublicAssetMismatch")
		if inventories.has(definition.inventory_id) or wallets.has(definition.wallet_id): return _fail("SharedMilitaryContainer")
		inventories[definition.inventory_id]=true; wallets[definition.wallet_id]=true
		var expected: Array=[]
		for entry in definition.units:
			if units.has(entry.id): return _fail("DuplicateGarrisonUnit")
			units[entry.id]={"camp_id":id,"definition":entry}; expected.append(entry.id)
		if camp.unit_ids!=expected: return _fail("GarrisonRosterMismatch")
	if _ids(units)!=_ids(state.military_units): return _fail("MilitaryUnitSourceMissing")
	for id in state.military_units:
		var unit: Variant=state.military_units[id]
		if not _fields(unit,{"id":TYPE_STRING,"source_garrison_id":TYPE_STRING,"organization_id":TYPE_STRING,"garrison_id":TYPE_STRING,"army_id":TYPE_STRING,"mobilizing_action_id":TYPE_STRING,"commander_actor_id":TYPE_STRING,"troop_type_id":TYPE_STRING,"captors":TYPE_DICTIONARY,"retainer_sources":TYPE_DICTIONARY,"morale":TYPE_INT}) or unit.id!=id or not _classified(unit) or not _whole(unit.morale,0,100): return _fail("InvalidMilitaryUnit")
		var definition: Dictionary=units[id].definition
		if unit.source_garrison_id!=units[id].camp_id or unit.organization_id!=data.garrisons[units[id].camp_id].organization_id or unit.troop_type_id!=definition.troop_type_id or not data.get("troop_types",{}).has(unit.troop_type_id) or not state.actors.has(unit.commander_actor_id): return _fail("MilitaryUnitSourceMismatch")
		var expected: Array=[]
		for index in range(definition.initial_soldiers):
			var person: String="soldier:"+id+":"+str(index+1)
			expected.append(person); known_members[person]=true
		for person in unit.retainer_sources:
			var source: Variant=state.facts.get(unit.retainer_sources[person])
			if not state.actors.has(person) or not source is Dictionary or source.get("kind")!="military_muster" or not person in source.get("actor_ids",[]): return _fail("MilitaryRetainerSourceMissing")
			expected.append(person)
		if _sorted(unit.member_ids)!=_sorted(expected): return _fail("MilitaryRosterSourceMismatch")
		for person in unit.member_ids:
			if members.has(person): return _fail("DuplicateMilitaryMember")
			members[person]=id
		if _ids(unit.captors)!=_sorted(unit.captured): return _fail("MilitaryCaptorMismatch")
		for person in unit.captors:
			if not unit.captors[person] is String or not state.armies.has(unit.captors[person]): return _fail("MilitaryCaptorMissing")
			captured[person]=unit.captors[person]
		for person in unit.retainer_sources:
			if person in unit.dead and state.actors[person].alive or person in unit.deserted and state.actors[person].available: return _fail("MilitaryActorOutcomeNotApplied")
		if unit.army_id.is_empty():
			if not data.garrisons.has(unit.garrison_id): return _fail("GarrisonUnitLocationMismatch")
			if (not unit.fit.is_empty() or not unit.wounded.is_empty()) and not _camp_matches(state,data,unit.garrison_id,unit.organization_id): return _fail("GarrisonUnitAllegianceMismatch")
			if not _stationing_source(state,unit): return _fail("GarrisonTransferReceiptMissing")
		elif not state.armies.has(unit.army_id) or state.armies[unit.army_id].status!="active" or not id in state.armies[unit.army_id].unit_ids or unit.organization_id!=state.armies[unit.army_id].organization_id or not unit.garrison_id.is_empty(): return _fail("ArmyUnitLocationMismatch")
		if not unit.mobilizing_action_id.is_empty() and not state.actions.has(unit.mobilizing_action_id): return _fail("DanglingMilitaryReservation")
	for id in state.armies:
		var army: Variant=state.armies[id]
		if not _fields(army,{"id":TYPE_STRING,"organization_id":TYPE_STRING,"commander_actor_id":TYPE_STRING,"member_actor_ids":TYPE_ARRAY,"unit_ids":TYPE_ARRAY,"inventory_id":TYPE_STRING,"wallet_id":TYPE_STRING,"order_id":TYPE_STRING,"garrison_id":TYPE_STRING,"status":TYPE_STRING,"last_supply_tick":TYPE_INT,"retainers":TYPE_DICTIONARY,"prisoner_ids":TYPE_ARRAY,"formation_source":TYPE_STRING}) or army.id!=id or not _instance(id,"army",state.next_instance_id) or not army.status in ["active","closed"] or not _unique_strings(army.member_actor_ids) or not _unique_strings(army.unit_ids) or not _unique_strings(army.prisoner_ids): return _fail("InvalidArmyState")
		if not data.garrisons.has(army.garrison_id) or not data.organizations.has(army.organization_id) or not state.actors.has(army.commander_actor_id) or not state.get("campaigns",{}).has(army.order_id): return _fail("ArmySourceMissing")
		var source: Variant=state.facts.get(army.formation_source)
		if not source is Dictionary or source.get("kind")!="military_action" or source.get("command")!="army_form" or source.get("phase")!="completed" or source.get("success")!=true or source.get("result",{}).get("army_id")!=id: return _fail("ArmyFormationSourceMissing")
		var historical: Dictionary=Ownership.historical_owner(state,data,army.garrison_id,source.get("terms",{}).get("ownership_id",""))
		if not historical.ok: return historical
		if historical.organization_id!=army.organization_id or source.get("terms",{}).get("organization_id")!=army.organization_id or source.terms.get("garrison_id")!=army.garrison_id: return _fail("ArmyFormationOrganizationMismatch")
		if source.get("terms",{}).get("unit_ids")!=army.unit_ids or source.terms.get("retainers")!=army.retainers or source.terms.get("order_id")!=army.order_id or source.terms.get("actor_id")!=army.commander_actor_id: return _fail("ArmyFormationTermsMismatch")
		var owner: Dictionary={"kind":"organization","id":army.organization_id}
		if state.inventories.get(army.inventory_id,{}).get("owner")!=owner or state.wallets.get(army.wallet_id,{}).get("owner")!=owner or inventories.has(army.inventory_id) or wallets.has(army.wallet_id): return _fail("ArmyPublicAssetMismatch")
		if army.inventory_id!="army_inventory:"+id or army.wallet_id!="army_wallet:"+id or state.inventories[army.inventory_id].capacity!=data.garrisons[army.garrison_id].army_capacity or Inventory.weight(state.inventories[army.inventory_id],data)>state.inventories[army.inventory_id].capacity: return _fail("ArmyContainerMismatch")
		inventories[army.inventory_id]=true; wallets[army.wallet_id]=true
		if not _whole(army.last_supply_tick,0,state.tick) or (army.status=="active" and army.last_supply_tick!=state.tick): return _fail("MilitarySupplyClockMismatch")
		if army.status=="active":
			if not army.commander_actor_id in army.member_actor_ids: return _fail("ArmyCommanderMissing")
			for unit_id in army.unit_ids:
				if not state.military_units.has(unit_id) or state.military_units[unit_id].army_id!=id: return _fail("ArmyRosterMismatch")
			for person in army.member_actor_ids:
				if not person is String or not state.actors.has(person) or actors.has(person): return _fail("DuplicateArmyActor")
				actors[person]=id
				if person!=army.commander_actor_id and state.actors[person].location!={"kind":"InArmy","army_id":id}: return _fail("ArmyActorLocationMismatch")
		elif not state.inventories[army.inventory_id].lots.is_empty() or state.wallets[army.wallet_id].balance!=0 or not army.prisoner_ids.is_empty(): return _fail("ClosedArmyAssetsNotReturned")
	var custody: Dictionary={}
	for id in state.armies:
		var army: Dictionary=state.armies[id]
		for person in army.prisoner_ids:
			if custody.has(person) or captured.get(person)!=id or army.status!="active": return _fail("ArmyPrisonerCustodyMismatch")
			custody[person]=true
			if state.actors.has(person) and (state.actors[person].available or state.actors[person].location!={"kind":"InArmy","army_id":id}): return _fail("MilitaryActorOutcomeNotApplied")
	for id in state.garrisons:
		for person in state.garrisons[id].prisoner_ids:
			var keeper: Variant=state.armies.get(captured.get(person,""))
			if custody.has(person) or not keeper is Dictionary or keeper.status!="closed" or keeper.get("return_garrison_id",keeper.garrison_id)!=id: return _fail("GarrisonPrisonerCustodyMismatch")
			custody[person]=true
			if state.actors.has(person) and (state.actors[person].available or not _at(state,person,data.garrisons[id].place_id)): return _fail("MilitaryActorOutcomeNotApplied")
	if _ids(custody)!=_ids(captured): return _fail("MissingMilitaryPrisonerCustody")
	return _validate_receipts(state,data,known_members)

## 军事永久回执的日期、来源和消耗可核算；传入本次校验建立的完整兵员索引，逐条保留类型与重复来源检查。
static func _validate_receipts(state: Dictionary, data: Dictionary, known_members: Dictionary) -> Dictionary:
	var supplies: Dictionary={}; var battles: Dictionary={}; var returns: Dictionary={}
	# 仅本次只读校验复用同营同来源的归属结果；退出即丢弃，仍逐条核对账目与名单。
	var ownership_cache: Dictionary={}
	for id in state.facts:
		var fact: Variant=state.facts[id]
		if not fact is Dictionary: continue
		var kind: String=fact.get("kind","")
		if not kind in ["military_action","military_supply","military_casualties","military_return","military_muster","office_resignation"]: continue
		if not _instance(id,"action" if kind=="military_action" else "military",state.next_instance_id): return _fail("InvalidMilitaryFactId")
		if kind=="military_action":
			if not _fields(fact,{"command":TYPE_STRING,"actor_id":TYPE_STRING,"start_tick":TYPE_INT,"completed_tick":TYPE_INT,"phase":TYPE_STRING,"success":TYPE_BOOL,"terms":TYPE_DICTIONARY,"context":TYPE_DICTIONARY,"result":TYPE_DICTIONARY}) or not fact.command in ACTION_KINDS or not state.actors.has(fact.actor_id) or not _whole(fact.start_tick,0,state.tick): return _fail("InvalidMilitaryActionFact")
			if fact.phase=="started":
				if not state.actions.has(id): return _fail("OrphanMilitaryAction")
				var valid: Dictionary=validate_action(state,data,state.actions[id])
				if not valid.ok: return valid
			elif fact.phase!="completed" or fact.completed_tick!=fact.start_tick+1 or fact.completed_tick>state.tick: return _fail("MilitaryCompletionDateMismatch")
			continue
		if not _whole(fact.get("tick"),0,state.tick): return _fail("InvalidMilitaryFactDate")
		if kind=="military_supply":
			if not _fields(fact,{"army_id":TYPE_STRING,"garrison_id":TYPE_STRING,"item_id":TYPE_STRING,"mouth_ids":TYPE_ARRAY,"required":TYPE_INT,"consumed":TYPE_INT,"shortage":TYPE_INT}) or not _unique_strings(fact.mouth_ids) or fact.army_id.is_empty()==fact.garrison_id.is_empty(): return _fail("InvalidMilitarySupplyFact")
			var camp_id: String=fact.garrison_id
			if not fact.army_id.is_empty():
				if not state.armies.has(fact.army_id): return _fail("MilitarySupplyArmyMissing")
				camp_id=state.armies[fact.army_id].garrison_id
			if not data.garrisons.has(camp_id) or fact.item_id!=data.garrisons[camp_id].food_item_id or fact.required!=fact.mouth_ids.size()*data.garrisons[camp_id].ration_per_person or not _whole(fact.consumed,0,fact.required) or fact.shortage!=fact.required-fact.consumed: return _fail("MilitarySupplyAccountingMismatch")
			if not fact.get("organization_id") is String or not fact.get("ownership_id") is String: return _fail("MilitarySupplyOwnershipMissing")
			if not ownership_cache.has(camp_id): ownership_cache[camp_id]={}
			if not ownership_cache[camp_id].has(fact.ownership_id):
				ownership_cache[camp_id][fact.ownership_id]=Ownership.historical_owner(state,data,camp_id,fact.ownership_id)
			var historical: Dictionary=ownership_cache[camp_id][fact.ownership_id]
			if not historical.ok: return historical
			if historical.organization_id!=fact.organization_id or (not fact.army_id.is_empty() and fact.organization_id!=state.armies[fact.army_id].organization_id): return _fail("MilitarySupplyOrganizationMismatch")
			for person in fact.mouth_ids:
				if not known_members.has(person): return _fail("UnknownMilitarySupplyMouth")
			var key: String=(fact.army_id if not fact.army_id.is_empty() else fact.garrison_id)+":"+str(fact.tick)
			if supplies.has(key): return _fail("DuplicateMilitarySupply")
			supplies[key]=true
		elif kind=="military_casualties":
			if not _fields(fact,{"battle_id":TYPE_STRING,"units":TYPE_DICTIONARY,"actor_changes":TYPE_ARRAY}) or battles.has(fact.battle_id): return _fail("DuplicateMilitarySettlement")
			var source: Variant=state.facts.get(fact.battle_id)
			if not source is Dictionary or source.get("kind")!="military_battle" or source.get("phase")!="completed" or source.get("tick")!=fact.tick or not source.get("unit_ids") is Array or _ids(fact.units)!=_sorted(source.unit_ids): return _fail("MilitaryBattleSourceRequired")
			for unit_id in fact.units:
				if not state.military_units.has(unit_id) or not fact.units[unit_id] is Dictionary or not _classified(fact.units[unit_id]): return _fail("MilitaryCasualtyRosterMismatch")
			battles[fact.battle_id]=true
		elif kind=="military_return":
			if not _fields(fact,{"army_id":TYPE_STRING,"order_id":TYPE_STRING,"garrison_id":TYPE_STRING,"actor_id":TYPE_STRING,"goods":TYPE_DICTIONARY,"money":TYPE_INT,"unit_ids":TYPE_ARRAY,"returned_retainers":TYPE_ARRAY,"lost_retainers":TYPE_ARRAY,"prisoner_ids":TYPE_ARRAY,"actor_changes":TYPE_ARRAY}) or not state.armies.has(fact.army_id) or returns.has(fact.army_id): return _fail("InvalidMilitaryReturn")
			var army: Dictionary=state.armies[fact.army_id]
			if army.status!="closed" or fact.order_id!=army.order_id or fact.garrison_id!=army.get("return_garrison_id",army.garrison_id) or fact.get("source_garrison_id")!=army.garrison_id or not state.actors.has(fact.actor_id) or fact.unit_ids!=army.unit_ids or not _whole(fact.money,0,MAX_VALUE) or not _fields(fact.get("authority"),{"kind":TYPE_STRING,"id":TYPE_STRING}): return _fail("MilitaryReturnSourceMismatch")
			var historical: Dictionary=Ownership.historical_owner(state,data,fact.garrison_id,fact.get("ownership_id",""))
			if not historical.ok: return historical
			if historical.organization_id!=army.organization_id: return _fail("MilitaryReturnOrganizationMismatch")
			var signed: bool=false
			for action_fact in state.facts.values():
				if action_fact is Dictionary and action_fact.get("kind")=="military_action" and action_fact.get("command")=="army_return" and action_fact.get("phase")=="completed" and action_fact.get("success")==true and action_fact.get("actor_id")==fact.actor_id and action_fact.get("result",{}).get("receipt_id")==id and action_fact.get("terms",{}).get("authority")==fact.authority and action_fact.terms.get("garrison_id")==fact.garrison_id and action_fact.terms.get("ownership_id")==fact.ownership_id: signed=true; break
			if not signed: return _fail("MilitaryReturnSignerMissing")
			for item_id in fact.goods:
				if not data.items.has(item_id) or not _whole(fact.goods[item_id],1,MAX_VALUE): return _fail("InvalidMilitaryReturnedGoods")
			returns[fact.army_id]=true
		elif kind=="military_muster":
			if not _fields(fact,{"actor_id":TYPE_STRING,"estate_id":TYPE_STRING,"order_id":TYPE_STRING,"actor_ids":TYPE_ARRAY}) or not state.actors.has(fact.actor_id) or not state.get("estates",{}).has(fact.estate_id) or not state.get("campaigns",{}).has(fact.order_id) or not _unique_strings(fact.actor_ids) or fact.actor_ids.is_empty(): return _fail("InvalidMilitaryPledgeFact")
			for person in fact.actor_ids:
				if not person in state.estates[fact.estate_id].resident_actor_ids: return _fail("MilitaryPledgeSourceMismatch")
		elif not _fields(fact,{"actor_id":TYPE_STRING,"office_id":TYPE_STRING}) or not state.actors.has(fact.actor_id) or not data.offices.has(fact.office_id): return _fail("InvalidOfficeResignation")
	for id in state.armies:
		if state.armies[id].status=="closed" and not returns.has(id): return _fail("MilitaryReturnReceiptMissing")
	return {"ok":true}

## 口粮历史只引用原驻军稳定兵员或一直存在的真人，不能凭任意文字人数形成可结算名单。
static func _known_member(state: Dictionary, data: Dictionary, person: String) -> bool:
	if state.actors.has(person): return true
	for camp in data.garrisons.values():
		for unit in camp.units:
			var prefix: String="soldier:"+unit.id+":"
			if person.begins_with(prefix):
				var suffix: String=person.trim_prefix(prefix)
				return suffix.is_valid_int() and str(int(suffix))==suffix and int(suffix)>=1 and int(suffix)<=unit.initial_soldiers
	return false

## 五类名册恰好覆盖本部成员，重复分类、漏失或额外兵员均失败。
static func _classified(unit: Dictionary) -> bool:
	if not _unique_strings(unit.get("member_ids")): return false
	var classified: Dictionary={}
	for group in GROUPS:
		if not unit.get(group) is Array: return false
		for person in unit[group]:
			if not person is String or not person in unit.member_ids or classified.has(person): return false
			classified[person]=true
	return classified.size()==unit.member_ids.size()

## 查询已经过守恒校验的某名兵员分类，不制造默认可战资格。
static func _group(unit: Dictionary, person: String) -> String:
	for group in GROUPS:
		if person in unit[group]: return group
	return ""

## 只扣实际可用公共粮食，零量不改变批次或容量；供应不足由调用方记录。
static func _consume(inventory: Dictionary, item_id: String, quantity: int, owner: Dictionary) -> void:
	if quantity<=0: return
	var remaining: int=quantity
	for lot in inventory.lots:
		if lot.item_id!=item_id or lot.owner!=owner or not lot.reserved_by.is_empty(): continue
		var used: int=mini(remaining,lot.quantity); lot.quantity-=used; remaining-=used
		if remaining==0: break
	inventory.lots=inventory.lots.filter(func(lot): return lot.quantity>0); inventory.revision+=1

## 按真正的驻军来源查作者部队，单位编号不能借另一营的兵员与装备套用。
static func _unit_definition(data: Dictionary, camp_id: String, unit_id: String) -> Dictionary:
	for entry in data.garrisons[camp_id].units:
		if entry.id==unit_id: return entry
	return {}

## 实际人物只属于一个活动军团；关闭记录保留历史但不继续占用人身。
static func _army_of(state: Dictionary, actor_id: String) -> String:
	for id in state.armies:
		if state.armies[id].status=="active" and actor_id in state.armies[id].member_actor_ids: return id
	return ""

## 地点以真人现有位置为准；被随军引用的人不能独自在军营重复办理另一份编组。
static func _at(state: Dictionary, actor_id: String, place_id: String) -> bool:
	var location: Dictionary=state.actors.get(actor_id,{}).get("location",{})
	return location.get("kind")=="Settlement" and location.get("place_id")==place_id

## 公职与军队不取消个人生命、伤势或行动占用；此处不另建角色时钟。
static func _ready(state: Dictionary, actor_id: String, own_action: String) -> bool:
	var actor: Variant=state.actors.get(actor_id)
	if not actor is Dictionary or not actor.get("alive",false) or not actor.get("available",false) or actor.get("hp",0)<=0 or actor.get("injury",0)>=3 or not actor.get("action_id","") in ["",own_action]: return false
	return not _battle_pending(state,[actor_id])

## 野战部队仍在未关闭会战中时，不能回营、重编或重复上交其装备。
static func _battle_pending(state: Dictionary, unit_ids: Array) -> bool:
	for battle in state.get("battles",{}).values():
		if battle.get("phase") in ["closed","completed"]: continue
		for id in unit_ids:
			if battle.get("units",{}).has(id): return true
	return false

## 通用动作字段须有明确类型，冻结条款的进一步检查由各命令分支负责。
static func _action(action: Dictionary) -> bool:
	return _fields(action,{"id":TYPE_STRING,"kind":TYPE_STRING,"actor_id":TYPE_STRING,"participants":TYPE_ARRAY,"remaining_ticks":TYPE_INT,"start_tick":TYPE_INT,"params":TYPE_DICTIONARY})

## 输入名单必须是不重复的稳定字符串ID，不能用数量代替既有成员。
static func _unique_strings(value: Variant) -> bool:
	if not value is Array: return false
	var seen: Dictionary={}
	for id in value:
		if not id is String or id.is_empty() or seen.has(id): return false
		seen[id]=true
	return true

## 对引用表按稳定ID排序，避免装载顺序改变结算或资产来源。
static func _ids(value: Dictionary) -> Array:
	return _sorted(value.keys())

## 排序副本，不让验证改变原有军令、名册或调用者数组。
static func _sorted(value: Array) -> Array:
	var result: Array=value.duplicate(); result.sort(); return result

## 世界实例编号不得冒用未来序号或非标准文字形式，驻军作者编号不通过此函数。
static func _instance(id: String, prefix: String, next: int) -> bool:
	if not id.begins_with(prefix+":"): return false
	var suffix: String=id.trim_prefix(prefix+":")
	return suffix.is_valid_int() and str(int(suffix))==suffix and int(suffix)>0 and int(suffix)<next

## 在深入读取前检查协议结构，失败由候选事务统一拒绝。
static func _fields(value: Variant, fields: Dictionary) -> bool:
	if not value is Dictionary: return false
	for key in fields:
		if typeof(value.get(key))!=fields[key]: return false
	return true

## 人数、日期与资产均为有界整数，不对损坏存档静默取整。
static func _whole(value: Variant, low: int, high: int) -> bool:
	return typeof(value)==TYPE_INT and value>=low and value<=high

## 返回明确领域错误，调用者不得提交失败候选或把失败解释为免费军队。
static func _fail(code: String) -> Dictionary:
	return {"ok":false,"code":code}

## 训练教官只凭本部真实活动/事实和当前公职进入原口粮并集；失权或离场由训练下一步骤正常中断。
static func _training_mouth(state: Dictionary, data: Dictionary, unit: Dictionary, camp_id: String, place_id: String) -> Dictionary:
	var action_id: Variant=unit.get("training_action_id","")
	if not action_id is String: return _fail("InvalidTrainingReservation")
	if action_id.is_empty(): return {"ok":true,"actor_id":""}
	var action: Variant=state.actions.get(action_id)
	var fact: Variant=state.facts.get(action_id)
	if not action is Dictionary or action.get("kind")!="military_train" or not action.get("params",{}).get("training") is Dictionary or not fact is Dictionary or fact.get("kind")!="military_training" or fact.get("phase")!="started": return _fail("InvalidTrainingSupplySource")
	var terms: Dictionary=action.params.training
	if fact.get("terms")!=terms or terms.get("unit_id")!=unit.id or terms.get("garrison_id")!=camp_id or terms.get("actor_id")!=action.get("actor_id") or terms.get("participants")!=action.get("participants") or not terms.get("office_id") is String: return _fail("InvalidTrainingSupplySource")
	var actor_id: String=action.actor_id
	var actor: Variant=state.actors.get(actor_id)
	if not actor is Dictionary or not actor.get("alive",false) or not actor.get("available",false) or actor.get("action_id")!=action_id or not _at(state,actor_id,place_id): return {"ok":true,"actor_id":""}
	var authority: Dictionary=ready_office(state,data,actor_id,terms.office_id)
	if not authority.ok or authority.garrison_id!=camp_id or state.offices[terms.office_id].appointment_id!=terms.get("authority_source"): return {"ok":true,"actor_id":""}
	return {"ok":true,"actor_id":actor_id}
