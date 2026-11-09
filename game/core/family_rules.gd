## 收养、指定与生前退隐的同世界规则；只移交有来源的私产，不复制知识、人脉、功勋或公职。
extends RefCounted
const Behavior=preload("res://core/domains/identity/actor_behavior.gd")
const Identity=preload("res://core/actor_identity.gd")
const Estate=preload("res://core/estate_rules.gd")
const Factory=preload("res://core/world_factory.gd")
const Inventory=preload("res://core/inventory_rules.gd")
const Interaction=preload("res://core/interaction_rules.gd")
const Location=preload("res://core/world_location.gd")
const KINDS=["family_adopt","family_designate","family_retire"]
const MAX_VALUE=2147483647
const TimeRules=preload("res://core/base/time_rules.gd")
const YEAR=TimeRules.TICKS_PER_YEAR

## 首次建立控制起点与空家庭索引；迁移不能凭当前玩家字段伪造一条继任链。
static func initialize(state: Dictionary, data: Dictionary) -> Dictionary:
	if not state.has("family"):
		if not data.get("scenarios",{}).has(state.get("scenario_id")): return _fail("UnknownFamilyScenario")
		if state.get("player_actor_id")!=data.scenarios[state.scenario_id].player_actor: return _fail("MissingSuccessionChain")
		state.family={"founder_actor_id":state.player_actor_id,"children":{},"designations":{},"successions":[]}
	return validate_state(state,data)

## 只读核验真实在场、年龄与资产；报价不会替人物增长年龄或补齐未知出生日期。
static func preview(state: Dictionary, data: Dictionary, actor_id: String, kind: String, params: Dictionary) -> Dictionary:
	return _preview(state,data,actor_id,kind,params,"")

## 通用开始阶段允许自己的占用；指定和退隐依然必须由空闲的两名真人当面办理。
static func _preview(state: Dictionary, data: Dictionary, actor_id: String, kind: String, params: Dictionary, own_action: String) -> Dictionary:
	if not kind in KINDS: return _fail("UnknownFamilyCommand")
	var valid: Dictionary=validate_state(state,data)
	if not valid.ok: return valid
	var partner: String=str(params.get("context",{}).get("provider_actor_id","")) if params.get("context") is Dictionary else ""
	var ready: Dictionary=_ready(state,actor_id,own_action,partner if kind=="family_adopt" else "",kind=="family_retire")
	if not ready.ok: return ready
	if _retired(state,actor_id): return _fail("FamilyAlreadyRetired")
	if kind=="family_adopt":
		var contact: Dictionary=Interaction.validate(state,data,actor_id,params.get("context"),"family_adopt",own_action)
		if not contact.ok: return contact
		ready=_ready(state,contact.context.provider_actor_id,own_action,actor_id)
		if not ready.ok: return ready
		var role: Dictionary=data.roles[contact.context.role_id]
		var config: Variant=role.get("family")
		if not _valid_config(config,data): return _fail("FamilyServiceNotOffered")
		var name: Variant=params.get("display_name",data.texts.get(config.child_name_key,""))
		if not _valid_name(name): return _fail("InvalidActorDisplayName")
		if state.tick>MAX_VALUE-config.adoption_ticks: return _fail("FamilyClockOverflow")
		var policy: Dictionary={}
		for key in ["child_age_years","guardian_min_age","heir_min_age","retirement_min_age"]: policy[key]=config[key]
		if config.get("enable_travel_plan",false): policy.enable_travel_plan=true
		var terms: Dictionary={"guardian_actor_id":actor_id,"provider_actor_id":contact.context.provider_actor_id,"role_id":contact.context.role_id,"place_id":contact.context.place_id,"prototype_id":config.prototype_id,"display_name":name,"birth_tick":state.tick+config.adoption_ticks-config.child_age_years*YEAR,"duration":config.adoption_ticks,"fee":config.fee,"payer_wallet_id":state.actors[actor_id].wallet_id,"recipient_wallet_id":config.wallet_id,"policy":policy}
		valid=_validate_terms(state,data,terms,state.tick)
		if not valid.ok: return valid
		if Inventory.spendable(state.wallets[terms.payer_wallet_id])<terms.fee: return _fail("InsufficientFunds")
		if state.wallets[terms.recipient_wallet_id].balance>MAX_VALUE-terms.fee: return _fail("MoneyOverflow")
		return {"ok":true,"duration":terms.duration,"cost":terms.fee,"context":contact.context,"training":terms}
	var designation_id: String=state.family.designations.get(actor_id,"")
	var heir: Variant=params.get("heir_actor_id") if kind=="family_designate" else state.facts.get(designation_id,{}).get("heir_actor_id")
	if not heir is String: return _fail("FamilyHeirRequired")
	valid=_eligible(state,actor_id,heir,state.tick,kind=="family_retire")
	if not valid.ok: return valid
	ready=_ready(state,heir,"","",kind=="family_retire")
	if not ready.ok: return ready
	var here: Dictionary=Location.resolve(state,actor_id)
	if here.get("kind")!="Settlement" or here!=Location.resolve(state,heir): return _fail("FamilyNotTogether")
	var family: Dictionary={"guardian_actor_id":actor_id,"heir_actor_id":heir,"adoption_source":state.family.children[heir],"place_id":here.place_id}
	if kind=="family_retire":
		if state.player_actor_id!=actor_id: return _fail("FamilyNotCurrentPlayer")
		var assets: Dictionary=_assets(state,data,actor_id,heir)
		if not assets.ok: return assets
		family.designation_source=designation_id; family.assets=assets.assets
	return {"ok":true,"duration":0,"family":family}

## 开始收养时冻结来源与条款并保留费用，尚未完工不创建人物也不转移费用。
static func begin(state: Dictionary, data: Dictionary, action: Dictionary, quote: Dictionary) -> Dictionary:
	if not _action_shape(action) or action.kind!="family_adopt" or _number(action.id,"action",state.next_instance_id)<1: return _fail("InvalidFamilyAction")
	if state.facts.has(action.id): return _fail("FamilyAdoptionAlreadyStarted")
	var current: Dictionary=_preview(state,data,action.actor_id,action.kind,action.params,action.id)
	if not current.ok: return current
	if current.get("training")!=quote.get("training") or current.get("context")!=quote.get("context"): return _fail("StaleFamilyQuote")
	if action.start_tick!=state.tick or action.remaining_ticks!=current.duration or action.participants!=[action.actor_id,current.context.provider_actor_id]: return _fail("InvalidFamilyAction")
	for person in action.participants:
		if state.actors[person].action_id!=action.id: return _fail("FamilyOccupationMissing")
	action.params={"training":current.training.duplicate(true),"context":current.context.duplicate(true)}
	state.wallets[current.training.payer_wallet_id].reserved[action.id]=current.cost
	state.facts[action.id]={"kind":"family_adoption_pending","guardian_actor_id":action.actor_id,"start_tick":state.tick,"terms":current.training.duplicate(true),"context":current.context.duplicate(true),"completed_tick":-1,"child_actor_id":"","success":false}
	return {"ok":true}

## 世界先推进到完工半日，再于释放双方占用前调用；中断取消费用，完成以永久来源只创建一次真人。
static func finish(state: Dictionary, data: Dictionary, action: Dictionary) -> Dictionary:
	var valid: Dictionary=_validate_action(state,data,action,true)
	if not valid.ok: return valid
	var fact: Dictionary=state.facts[action.id]
	if fact.kind!="family_adoption_pending": return {"ok":true,"success":fact.success,"actor_id":fact.child_actor_id,"already_completed":true}
	if action.remaining_ticks!=0 or state.tick!=action.start_tick+action.params.training.duration: return _fail("FamilyAdoptionNotReady")
	var terms: Dictionary=action.params.training
	var success: bool=Interaction.validate(state,data,action.actor_id,action.params.context,"family_adopt",action.id).ok
	for person in action.participants:
		if not _ready(state,person,action.id,terms.provider_actor_id if person==action.actor_id else action.actor_id).ok or state.actors[person].action_id!=action.id: success=false
	if state.wallets[terms.recipient_wallet_id].balance>MAX_VALUE-terms.fee: success=false
	fact.completed_tick=state.tick; fact.success=success
	if not success:
		fact.kind="family_adoption_failed"
		Inventory.release(state,action.id)
		return {"ok":true,"success":false,"code":"FamilyAdoptionInterrupted","actor_id":""}
	fact.kind="family_adoption"
	var created: Dictionary=Identity.instantiate(state,data,terms.prototype_id,terms.display_name,terms.birth_tick,terms.place_id,action.id)
	if not created.ok: return created
	fact.child_actor_id=created.actor_id
	state.family.children[created.actor_id]=action.id
	var schedule: Dictionary=Behavior.initialize_schedule(state,data,created.actor_id)
	if not schedule.ok: return schedule
	state.wallets[terms.payer_wallet_id].balance-=terms.fee
	state.wallets[terms.recipient_wallet_id].balance+=terms.fee
	Inventory.release(state,action.id)
	return {"ok":true,"success":true,"actor_id":created.actor_id}

## 即时命令重新核验报价和全部私产；实际移交后以永久继任事实改变玩家，前代仍作为真人留在世界。
static func apply(state: Dictionary, data: Dictionary, actor_id: String, kind: String, quote: Dictionary) -> Dictionary:
	if not kind in ["family_designate","family_retire"] or not quote.get("family") is Dictionary: return _fail("InvalidFamilyInstant")
	var params: Dictionary={"heir_actor_id":quote.family.get("heir_actor_id")}
	var current: Dictionary=preview(state,data,actor_id,kind,params)
	if not current.ok: return current
	if current.family!=quote.family: return _fail("StaleFamilyQuote")
	var plan: Dictionary=current.family
	if state.next_instance_id>=MAX_VALUE: return _fail("FamilyCounterExhausted")
	var id: String=Factory.next_id(state,"family")
	var fact: Dictionary=plan.duplicate(true)
	fact.kind="family_designation" if kind=="family_designate" else "family_succession"
	fact.tick=state.tick
	if kind=="family_designate":
		state.facts[id]=fact; state.family.designations[actor_id]=id
	else:
		var owner: Dictionary={"kind":"actor","id":actor_id}
		var receiver: Dictionary={"kind":"actor","id":plan.heir_actor_id}
		for entry in plan.assets.wallets:
			state.wallets[entry.source_id].balance-=entry.amount
			state.wallets[entry.destination_id].balance+=entry.amount
		for entry in plan.assets.goods:
			if entry.source_inventory_id==entry.destination_inventory_id:
				var inventory: Dictionary=state.inventories[entry.source_inventory_id]
				for lot in inventory.lots:
					if lot.id==entry.lot_id: lot.owner=receiver.duplicate(); inventory.revision+=1; break
			else:
				var moved: Dictionary=Inventory.transfer(state,entry.source_inventory_id,entry.destination_inventory_id,entry.item_id,entry.quantity,[owner],"",receiver)
				if not moved.ok: return moved
		for entry in plan.assets.estates:
			state.estates[entry.estate_id].owner_actor_id=plan.heir_actor_id
			state.wallets[entry.wallet_id].owner=receiver.duplicate()
			state.inventories[entry.inventory_id].owner=receiver.duplicate()
		for role_id in plan.assets.disabled_roles: state.roles[role_id].enabled=false
		state.facts[id]=fact; state.family.successions.append(id); state.player_actor_id=plan.heir_actor_id
	var valid: Dictionary=validate_state(state,data)
	if not valid.ok: return valid
	return {"ok":true,"fact_id":id,"heir_actor_id":plan.heir_actor_id,"player_actor_id":state.player_actor_id}

## 存档边界验证真实占用、计时和保留额，不从未完成的收养生成身份。
static func validate_action(state: Dictionary, data: Dictionary, action: Dictionary) -> Dictionary:
	return _validate_action(state,data,action,false)

## 完工允许检查已结算来源以支持永久幂等；尚未完成时仍验证冻结条款与唯一世界日期。
static func _validate_action(state: Dictionary, data: Dictionary, action: Dictionary, finishing: bool) -> Dictionary:
	if not _action_shape(action) or action.kind!="family_adopt" or _number(action.id,"action",state.next_instance_id)<1 or action.params.size()!=2 or not action.params.get("training") is Dictionary or not action.params.get("context") is Dictionary: return _fail("InvalidFamilyAction")
	var terms: Dictionary=action.params.training
	var fact: Variant=state.get("facts",{}).get(action.id)
	var historical: bool=fact is Dictionary and fact.get("kind") in ["family_adoption","family_adoption_failed"]
	var valid: Dictionary=_validate_terms(state,data,terms,action.start_tick,historical)
	if not valid.ok: return valid
	if not _adoption_shape(fact) or fact.terms!=terms or fact.context!=action.params.context or not _context_matches(fact.context,terms) or fact.guardian_actor_id!=action.actor_id or fact.start_tick!=action.start_tick or action.participants!=[action.actor_id,terms.provider_actor_id]: return _fail("FamilyAdoptionFactMismatch")
	if fact.kind in ["family_adoption","family_adoption_failed"]:
		if not finishing or fact.completed_tick!=action.start_tick+terms.duration or fact.completed_tick>state.tick: return _fail("FamilyAdoptionAlreadyCompleted")
		if fact.success!=(fact.kind=="family_adoption") or (fact.success and state.family.children.get(fact.child_actor_id)!=action.id): return _fail("FamilyAdoptionFactMismatch")
		return {"ok":true}
	if fact.kind!="family_adoption_pending" or fact.success or fact.completed_tick!=-1 or not fact.child_actor_id.is_empty(): return _fail("InvalidFamilyAdoptionFact")
	if not _whole(action.remaining_ticks,0 if finishing else 1,terms.duration) or action.start_tick+terms.duration-action.remaining_ticks!=state.tick: return _fail("FamilyClockMismatch")
	if state.wallets[terms.payer_wallet_id].reserved.get(action.id)!=terms.fee or state.wallets[terms.payer_wallet_id].balance<terms.fee: return _fail("FamilyAdoptionEscrowMismatch")
	if not finishing:
		for person in action.participants:
			if state.actors[person].action_id!=action.id: return _fail("FamilyOccupationMissing")
	return {"ok":true}

## 验证真实身份、家庭索引、从初始玩家出发的控制链和私庄归属；历史事实不会强迫后代保留已花出的现金。
static func validate_state(state: Dictionary, data: Dictionary) -> Dictionary:
	var valid: Dictionary=Identity.validate(state,data)
	if not valid.ok: return valid
	valid=validate_header(state,data)
	if not valid.ok: return valid
	if state.has("estates") or not data.get("estates",{}).is_empty():
		valid=Estate.validate_state(state,data)
		if not valid.ok: return valid
	return validate_tail(state,data)

## 家庭容器与初始人物先于庄园校验，保留原来的首错。
static func validate_header(state: Dictionary, data: Dictionary) -> Dictionary:
	var family: Variant=state.get("family")
	if not _fields(family,{"founder_actor_id":TYPE_STRING,"children":TYPE_DICTIONARY,"designations":TYPE_DICTIONARY,"successions":TYPE_ARRAY}) or family.size()!=4: return _fail("InvalidFamilyState")
	if not data.get("scenarios",{}).has(state.get("scenario_id")) or family.founder_actor_id!=data.scenarios[state.scenario_id].player_actor: return _fail("InvalidFamilyFounder")
	return {"ok":true}

## 身份、家庭头与庄园已通过后核对历史链；不重复调用前置领域。
static func validate_tail(state: Dictionary, data: Dictionary) -> Dictionary:
	var family: Variant=state.get("family")
	var valid: Dictionary
	var latest: Dictionary={}
	var succession_ids: Dictionary={}
	for id in state.facts:
		var fact: Variant=state.facts[id]
		if not fact is Dictionary: continue
		if fact.get("kind") in ["family_adoption_pending","family_adoption_failed"] or (fact.get("kind")=="family_adoption" and fact.has("terms")):
			if not _adoption_shape(fact) or _number(id,"action",state.next_instance_id)<1: return _fail("InvalidFamilyAdoptionFact")
			valid=_validate_terms(state,data,fact.terms,fact.start_tick,fact.kind!="family_adoption_pending")
			if not valid.ok: return valid
			if fact.guardian_actor_id!=fact.terms.guardian_actor_id or not _context_matches(fact.context,fact.terms): return _fail("FamilyAdoptionFactMismatch")
			if fact.kind=="family_adoption_pending":
				if not state.get("actions",{}).has(id): return _fail("OrphanFamilyAdoption")
				valid=validate_action(state,data,state.actions[id])
				if not valid.ok: return valid
			elif fact.completed_tick!=fact.start_tick+fact.terms.duration or fact.completed_tick>state.tick or fact.success!=(fact.kind=="family_adoption"):
				return _fail("InvalidFamilyAdoptionDate")
			elif fact.success:
				if family.children.get(fact.child_actor_id)!=id: return _fail("OrphanFamilyChild")
			elif not fact.child_actor_id.is_empty(): return _fail("FailedAdoptionHasChild")
		elif fact.get("kind")=="family_designation":
			valid=_validate_choice(state,data,fact,false)
			if not valid.ok or _number(id,"family",state.next_instance_id)<1: return _fail("InvalidFamilyDesignation")
			var prior: String=latest.get(fact.guardian_actor_id,"")
			if prior.is_empty() or _number(id,"family",state.next_instance_id)>_number(prior,"family",state.next_instance_id): latest[fact.guardian_actor_id]=id
		elif fact.get("kind")=="family_succession": succession_ids[id]=true
	for child in family.children:
		var source: Variant=family.children[child]
		var fact: Variant=state.facts.get(source) if source is String else null
		var creation: Variant=state.facts.get(child)
		if not _adoption_shape(fact) or fact.kind!="family_adoption" or fact.child_actor_id!=child or not fact.success or not creation is Dictionary or creation.get("kind")!="actor_creation" or creation.get("source")!=source: return _fail("InvalidFamilyChild")
		if creation.prototype_id!=fact.terms.prototype_id or creation.birth_tick!=fact.terms.birth_tick or creation.display_name!=fact.terms.display_name or creation.created_tick!=fact.completed_tick: return _fail("FamilyChildIdentityMismatch")
	if latest!=family.designations: return _fail("FamilyDesignationIndexMismatch")
	var current: String=family.founder_actor_id
	var seen: Dictionary={current:true}
	var owners: Dictionary={}
	for id in data.get("estates",{}): owners[id]=data.estates[id].owner_actor_id
	var last_tick: int=0
	for id in family.successions:
		if not id is String or not succession_ids.has(id) or _number(id,"family",state.next_instance_id)<1: return _fail("InvalidSuccessionChain")
		var fact: Dictionary=state.facts[id]
		valid=_validate_choice(state,data,fact,true)
		if not valid.ok: return valid
		if fact.guardian_actor_id!=current or seen.has(fact.heir_actor_id) or fact.tick<last_tick: return _fail("InvalidSuccessionChain")
		var choice: Variant=state.facts.get(fact.designation_source)
		if not choice is Dictionary or choice.get("kind")!="family_designation" or choice.get("guardian_actor_id")!=current or choice.get("heir_actor_id")!=fact.heir_actor_id or choice.get("adoption_source")!=fact.adoption_source or choice.get("tick",MAX_VALUE)>fact.tick or _number(fact.designation_source,"family",state.next_instance_id)>=_number(id,"family",state.next_instance_id): return _fail("SuccessionDesignationMismatch")
		for later_id in state.facts:
			var later: Variant=state.facts[later_id]
			if later is Dictionary and later.get("kind")=="family_designation" and later.get("guardian_actor_id")==current and _number(later_id,"family",state.next_instance_id)>_number(fact.designation_source,"family",state.next_instance_id) and _number(later_id,"family",state.next_instance_id)<_number(id,"family",state.next_instance_id): return _fail("SuccessionDesignationSuperseded")
		valid=_validate_assets(state,data,fact,owners)
		if not valid.ok: return valid
		current=fact.heir_actor_id; seen[current]=true; last_tick=fact.tick; succession_ids.erase(id)
	if not succession_ids.is_empty() or state.player_actor_id!=current: return _fail("InvalidSuccessionPlayer")
	for id in owners:
		if state.estates[id].owner_actor_id!=owners[id]: return _fail("EstateSuccessionSourceMissing")
	return {"ok":true,"player_actor_id":current,"estate_owners":owners}

## 核验冻结的作者服务和实际付费所有者；年龄使用开始时的真实出生日期，至少相差十六年。
static func _validate_terms(state: Dictionary, data: Dictionary, terms: Variant, start_tick: Variant, historical: bool=false) -> Dictionary:
	if not _fields(terms,{"guardian_actor_id":TYPE_STRING,"provider_actor_id":TYPE_STRING,"role_id":TYPE_STRING,"place_id":TYPE_STRING,"prototype_id":TYPE_STRING,"display_name":TYPE_STRING,"birth_tick":TYPE_INT,"duration":TYPE_INT,"fee":TYPE_INT,"payer_wallet_id":TYPE_STRING,"recipient_wallet_id":TYPE_STRING,"policy":TYPE_DICTIONARY}) or terms.size()!=12 or not _whole(start_tick,0,state.tick): return _fail("InvalidFamilyTerms")
	if not state.actors.has(terms.guardian_actor_id) or not state.actors.has(terms.provider_actor_id) or terms.guardian_actor_id==terms.provider_actor_id or not data.roles.has(terms.role_id) or not data.places.has(terms.place_id): return _fail("InvalidFamilyReference")
	var role: Dictionary=data.roles[terms.role_id]
	var config: Variant=role.get("family")
	if not _valid_config(config,data) or not "family_adopt" in role.services or role.place_id!=terms.place_id: return _fail("FamilyServiceNotOffered")
	if terms.policy.size()!=(5 if terms.policy.has("enable_travel_plan") else 4): return _fail("InvalidFamilyPolicy")
	if terms.policy.has("enable_travel_plan") and (typeof(terms.policy.enable_travel_plan)!=TYPE_BOOL or not terms.policy.enable_travel_plan or not config.get("enable_travel_plan",false)): return _fail("InvalidFamilyPolicy")
	for key in ["child_age_years","guardian_min_age","heir_min_age","retirement_min_age"]:
		if typeof(terms.policy.get(key))!=TYPE_INT or terms.policy[key]!=config[key]: return _fail("FamilyPolicyMismatch")
	if terms.prototype_id!=config.prototype_id or terms.duration!=config.adoption_ticks or terms.fee!=config.fee or terms.recipient_wallet_id!=config.wallet_id or terms.birth_tick!=start_tick+terms.duration-config.child_age_years*YEAR or not _valid_name(terms.display_name): return _fail("FamilyTermsMismatch")
	if start_tick>MAX_VALUE-terms.duration: return _fail("FamilyClockOverflow")
	var guardian: Dictionary=state.actors[terms.guardian_actor_id]
	if _age_at(state,terms.guardian_actor_id,start_tick)<config.guardian_min_age or terms.birth_tick-guardian.get("birth_tick",terms.birth_tick)<16*YEAR: return _fail("FamilyGuardianAgeRequired")
	var recipient: Dictionary={"kind":"actor","id":terms.provider_actor_id} if role.get("organization_id")==null else {"kind":"organization","id":role.organization_id}
	var paid_owner: Dictionary=_wallet_owner_at(state,data,terms.recipient_wallet_id,start_tick) if historical else state.wallets.get(terms.recipient_wallet_id,{}).get("owner",{})
	if terms.payer_wallet_id!=guardian.wallet_id or terms.payer_wallet_id==terms.recipient_wallet_id or state.wallets.get(terms.payer_wallet_id,{}).get("owner")!={"kind":"actor","id":guardian.id} or paid_owner!=recipient: return _fail("FamilyPaymentAuthority")
	return {"ok":true}

## 历史收款按当时合法钱柜归属核验；以后庄园继任不会倒过来使已完成收养变成无授权付款。
static func _wallet_owner_at(state: Dictionary, data: Dictionary, wallet_id: String, tick: int) -> Dictionary:
	for estate_id in data.get("estates",{}):
		var estate: Dictionary=data.estates[estate_id]
		if estate.wallet_id!=wallet_id: continue
		var owner: String=estate.owner_actor_id
		for source in state.get("family",{}).get("successions",[]):
			var fact: Variant=state.facts.get(source)
			if not fact is Dictionary or not fact.get("tick") is int or fact.tick>tick or not fact.get("assets") is Dictionary or not fact.assets.get("estates") is Array: continue
			for entry in fact.assets.estates:
				if entry is Dictionary and entry.get("estate_id")==estate_id and fact.get("heir_actor_id") is String: owner=fact.heir_actor_id
		return {"kind":"actor","id":owner}
	return state.wallets.get(wallet_id,{}).get("owner",{})

## 家庭作者配置复用已有角色、原型和文本表；拒绝缺失引用与超界年龄、费用和时间。
static func _valid_config(config: Variant, data: Dictionary) -> bool:
	if not _fields(config,{"prototype_id":TYPE_STRING,"child_name_key":TYPE_STRING,"child_age_years":TYPE_INT,"fee":TYPE_INT,"wallet_id":TYPE_STRING,"adoption_ticks":TYPE_INT,"guardian_min_age":TYPE_INT,"heir_min_age":TYPE_INT,"retirement_min_age":TYPE_INT}): return false
	if config.has("enable_travel_plan") and typeof(config.enable_travel_plan)!=TYPE_BOOL: return false
	if config.get("enable_travel_plan",false) and not data.actors.get(config.prototype_id,{}).get("travel_plan") is Dictionary: return false
	return data.actors.has(config.prototype_id) and data.texts.has(config.child_name_key) and _whole(config.child_age_years,0,17) and _whole(config.fee,0,1000000) and _whole(config.adoption_ticks,2,30) and _whole(config.guardian_min_age,18,100) and _whole(config.heir_min_age,18,100) and _whole(config.retirement_min_age,40,120)

## 资格只来自该监护人的完整收养事实，旧演员或别人的子女不能通过人物名字冒认继任。
static func _eligible(state: Dictionary, guardian: String, heir: String, at_tick: int, retirement: bool) -> Dictionary:
	var source: String=state.family.children.get(heir,"")
	var fact: Variant=state.facts.get(source)
	if guardian==heir or not state.actors.has(guardian) or not state.actors.has(heir) or not _adoption_shape(fact) or fact.kind!="family_adoption" or fact.guardian_actor_id!=guardian or fact.completed_tick>at_tick: return _fail("FamilyNotOwnChild")
	if _age_at(state,heir,at_tick)<fact.terms.policy.heir_min_age: return _fail("FamilyHeirNotAdult")
	if retirement and _age_at(state,guardian,at_tick)<fact.terms.policy.retirement_min_age: return _fail("FamilyRetirementAgeRequired")
	return {"ok":true}

## 验证历史指定或接续的日期与资格，不因后来的旅行、交友或花费而改写已发生的事实。
static func _validate_choice(state: Dictionary, data: Dictionary, fact: Dictionary, succession: bool) -> Dictionary:
	if not _fields(fact,{"kind":TYPE_STRING,"guardian_actor_id":TYPE_STRING,"heir_actor_id":TYPE_STRING,"adoption_source":TYPE_STRING,"place_id":TYPE_STRING,"tick":TYPE_INT}) or fact.size()!=(8 if succession else 6) or not _whole(fact.tick,0,state.tick) or not data.places.has(fact.place_id): return _fail("InvalidFamilyChoice")
	if succession and (not fact.get("designation_source") is String or not fact.get("assets") is Dictionary): return _fail("InvalidFamilySuccession")
	if state.family.children.get(fact.heir_actor_id)!=fact.adoption_source: return _fail("FamilyAdoptionSourceMismatch")
	return _eligible(state,fact.guardian_actor_id,fact.heir_actor_id,fact.tick,succession)

## 读取可交接的真实私产；公物、外部寄存、未结合同、保留资产和容量不足要求先实际处理。
static func _assets(state: Dictionary, data: Dictionary, guardian: String, heir: String) -> Dictionary:
	var owner: Dictionary={"kind":"actor","id":guardian}
	var receiver: Dictionary={"kind":"actor","id":heir}
	var old: Dictionary=state.actors[guardian]
	var next: Dictionary=state.actors[heir]
	if not state.wallets[next.wallet_id].reserved.is_empty(): return _fail("FamilyAssetsReserved")
	var assets: Dictionary={"wallets":[],"goods":[],"estates":[],"disabled_roles":[]}
	var stores: Dictionary={}
	var cashboxes: Dictionary={}
	for id in _sorted(state.get("estates",{})):
		var estate: Dictionary=state.estates[id]
		if estate.owner_actor_id!=guardian: continue
		if not estate.detached.is_empty(): return _fail("FamilyEstateRetainersAway")
		for action in state.actions.values():
			if action.kind=="estate_build" and action.params.get("terms",{}).get("estate_id")==id: return _fail("FamilyEstateConstructionPending")
		var definition: Dictionary=data.estates[id]
		stores[definition.inventory_id]=true; cashboxes[definition.wallet_id]=true
		assets.estates.append({"estate_id":id,"inventory_id":definition.inventory_id,"wallet_id":definition.wallet_id})
	var total: int=0
	for id in _sorted(state.wallets):
		var wallet: Dictionary=state.wallets[id]
		if wallet.owner!=owner: continue
		if not wallet.reserved.is_empty(): return _fail("FamilyAssetsReserved")
		if cashboxes.has(id): continue
		total+=wallet.balance
		assets.wallets.append({"source_id":id,"destination_id":next.wallet_id,"amount":wallet.balance})
	if state.wallets[next.wallet_id].balance>MAX_VALUE-total: return _fail("MoneyOverflow")
	var load_weight: int=Inventory.weight(state.inventories[next.inventory_id],data)
	for id in _sorted(state.inventories):
		var inventory: Dictionary=state.inventories[id]
		for lot in inventory.lots:
			if id==old.inventory_id and lot.owner!=owner or id==next.inventory_id and lot.owner!=receiver: return _fail("FamilyForeignCargo")
			if id==next.inventory_id and not lot.reserved_by.is_empty(): return _fail("FamilyAssetsReserved")
			if lot.owner!=owner: continue
			if not lot.reserved_by.is_empty(): return _fail("FamilyAssetsReserved")
			if id!=old.inventory_id and not stores.has(id): return _fail("FamilyPrivateAssetsElsewhere")
			var destination: String=next.inventory_id if id==old.inventory_id else id
			if id==old.inventory_id: load_weight+=lot.quantity*data.items[lot.item_id].weight
			assets.goods.append({"source_inventory_id":id,"destination_inventory_id":destination,"lot_id":lot.id,"item_id":lot.item_id,"quantity":lot.quantity})
	if load_weight>state.inventories[next.inventory_id].capacity: return _fail("FamilyHeirCapacityExceeded")
	for id in _sorted(state.roles):
		if state.roles[id].actor_id==guardian and state.roles[id].enabled: assets.disabled_roles.append(id)
	return {"ok":true,"assets":assets}

## 继任账目只能引用前代本人私袋、私人钱包和其当时所有庄园，不允许以伪造记录接管组织仓库。
static func _validate_assets(state: Dictionary, data: Dictionary, fact: Dictionary, owners: Dictionary) -> Dictionary:
	var assets: Dictionary=fact.assets
	if not _fields(assets,{"wallets":TYPE_ARRAY,"goods":TYPE_ARRAY,"estates":TYPE_ARRAY,"disabled_roles":TYPE_ARRAY}) or assets.size()!=4: return _fail("InvalidSuccessionAssets")
	var source_actor: Dictionary=state.actors[fact.guardian_actor_id]
	var target_actor: Dictionary=state.actors[fact.heir_actor_id]
	var expected: Array=[]
	for id in _sorted(owners):
		if owners[id]==fact.guardian_actor_id: expected.append(id)
	var received: Array=[]
	var stores: Dictionary={}
	var cashboxes: Dictionary={}
	for entry in assets.estates:
		if not _fields(entry,{"estate_id":TYPE_STRING,"inventory_id":TYPE_STRING,"wallet_id":TYPE_STRING}) or entry.size()!=3 or not data.get("estates",{}).has(entry.estate_id): return _fail("InvalidSuccessionEstate")
		var definition: Dictionary=data.estates[entry.estate_id]
		if entry.inventory_id!=definition.inventory_id or entry.wallet_id!=definition.wallet_id: return _fail("InvalidSuccessionEstate")
		received.append(entry.estate_id); stores[entry.inventory_id]=true; cashboxes[entry.wallet_id]=true
	if received!=expected: return _fail("SuccessionEstateSourceMismatch")
	var wallets: Dictionary={}
	for entry in assets.wallets:
		if not _fields(entry,{"source_id":TYPE_STRING,"destination_id":TYPE_STRING,"amount":TYPE_INT}) or entry.size()!=3 or not _whole(entry.amount,0,MAX_VALUE): return _fail("InvalidSuccessionMoney")
		if wallets.has(entry.source_id) or cashboxes.has(entry.source_id) or entry.destination_id!=target_actor.wallet_id or state.wallets.get(entry.source_id,{}).get("owner")!={"kind":"actor","id":fact.guardian_actor_id}: return _fail("SuccessionMoneySourceMismatch")
		wallets[entry.source_id]=true
	if not wallets.has(source_actor.wallet_id): return _fail("MissingSuccessionPersonalWallet")
	var lots: Dictionary={}
	for entry in assets.goods:
		if not _fields(entry,{"source_inventory_id":TYPE_STRING,"destination_inventory_id":TYPE_STRING,"lot_id":TYPE_STRING,"item_id":TYPE_STRING,"quantity":TYPE_INT}) or entry.size()!=5 or not _whole(entry.quantity,1,MAX_VALUE) or not data.items.has(entry.item_id) or lots.has(entry.lot_id) or _number(entry.lot_id,"lot",state.next_instance_id)<1: return _fail("InvalidSuccessionGoods")
		if entry.source_inventory_id==source_actor.inventory_id:
			if entry.destination_inventory_id!=target_actor.inventory_id: return _fail("SuccessionGoodsSourceMismatch")
		elif not stores.has(entry.source_inventory_id) or entry.source_inventory_id!=entry.destination_inventory_id: return _fail("SuccessionGoodsSourceMismatch")
		lots[entry.lot_id]=true
	var roles: Dictionary={}
	for id in assets.disabled_roles:
		if not id is String or not data.roles.has(id) or roles.has(id): return _fail("InvalidSuccessionRoles")
		roles[id]=true
	for id in expected: owners[id]=fact.heir_actor_id
	return {"ok":true}

## 家事仍受真实占用约束；只有退隐交接才要求结清合同和公职，任官期间可以正常收养。
static func _ready(state: Dictionary, actor_id: String, own_action: String, contact_actor_id: String="", retirement: bool=false) -> Dictionary:
	var actor: Variant=state.get("actors",{}).get(actor_id)
	if not actor is Dictionary or not actor.get("alive",false) or not actor.get("available",false) or actor.get("hp",0)<=0 or actor.get("injury",0)>=3: return _fail("ActorUnavailable")
	if not actor.get("action_id","").is_empty() and actor.action_id!=own_action: return _fail("ActorBusy")
	if not Location.party_id(state,actor_id).is_empty(): return _fail("FamilyPartyPending")
	for battle in state.get("battles",{}).values():
		if battle.get("phase")!="closed" and battle.get("units",{}).has(actor_id): return _fail("BattlePending")
	for task in state.get("tasks",{}).values():
		if retirement and task.get("assignee")==actor_id and not task.get("status") in ["success","failed","cancelled"]: return _fail("FamilyContractPending")
	for speaker in state.get("interactions",{}):
		var listener: String=state.interactions[speaker].get("context",{}).get("provider_actor_id","")
		if (speaker==actor_id and listener!=contact_actor_id) or (listener==actor_id and speaker!=contact_actor_id): return _fail("FamilyConversationPending")
	for estate in state.get("estates",{}).values():
		if estate.detached.has(actor_id): return _fail("FamilyMilitaryPending")
	for table in ["offices","armies"]:
		if table=="offices" and not retirement: continue
		for record in state.get(table,{}).values():
			if not record is Dictionary or record.get("enabled",true)==false or record.get("status","") in ["released","closed","disbanded","resigned"]: continue
			for key in ["actor_id","holder_actor_id","commander_actor_id","leader_id"]:
				if record.get(key)==actor_id: return _fail("FamilyMilitaryOrOfficePending")
			for key in ["member_actor_ids","members"]:
				if actor_id in record.get(key,[]): return _fail("FamilyMilitaryOrOfficePending")
	return {"ok":true}

## 已退隐人物保留生命与资产容器，不能再次消费同一控制权制造第二条接续。
static func _retired(state: Dictionary, actor_id: String) -> bool:
	for id in state.family.successions:
		if state.facts[id].guardian_actor_id==actor_id: return true
	return false

## 在事实发生的半日计算年龄；未声明生日的旧人物保持未知，不以原型年龄覆盖。
static func _age_at(state: Dictionary, actor_id: String, tick: int) -> int:
	var birth: Variant=state.actors.get(actor_id,{}).get("birth_tick")
	if not _whole(birth,-MAX_VALUE,tick): return -1
	return TimeRules.age_years(tick,birth)

## 收养上下文只能描述冻结的真实场所与提供人，遭遇上下文不能借用室内事务权限。
static func _context_matches(context: Dictionary, terms: Dictionary) -> bool:
	return context.get("kind","")!="encounter" and context.get("provider_actor_id")==terms.provider_actor_id and context.get("place_id")==terms.place_id and context.get("role_id")==terms.role_id

## 在读取通用行动字段前验证基础形状，错误协议不进入候选写入路径。
static func _action_shape(action: Dictionary) -> bool:
	return _fields(action,{"id":TYPE_STRING,"kind":TYPE_STRING,"actor_id":TYPE_STRING,"participants":TYPE_ARRAY,"remaining_ticks":TYPE_INT,"start_tick":TYPE_INT,"params":TYPE_DICTIONARY})

## 三种收养阶段共用完整记录结构，终结状态仍保留失败或创建来源供读档核查。
static func _adoption_shape(fact: Variant) -> bool:
	return _fields(fact,{"kind":TYPE_STRING,"guardian_actor_id":TYPE_STRING,"start_tick":TYPE_INT,"terms":TYPE_DICTIONARY,"context":TYPE_DICTIONARY,"completed_tick":TYPE_INT,"child_actor_id":TYPE_STRING,"success":TYPE_BOOL}) and fact.size()==8

## 显示名与统一身份层保持相同边界，不允许空名字、控制字符或以空白伪装两个名字。
static func _valid_name(value: Variant) -> bool:
	if not value is String or value.is_empty() or value.length()>80 or value!=value.strip_edges(): return false
	for index in range(value.length()):
		if value.unicode_at(index)<32 or value.unicode_at(index)==127: return false
	return true

## 按稳定ID排序资产来源，保证相同存档与报价得到相同移交计划。
static func _sorted(entries: Dictionary) -> Array:
	var ids: Array=entries.keys(); ids.sort(); return ids

## 家庭永久编号沿用世界计数器，拒绝未来编号或重复文字形式。
static func _number(id: String, prefix: String, next: int) -> int:
	if not id.begins_with(prefix+":"): return -1
	var suffix: String=id.trim_prefix(prefix+":")
	if not suffix.is_valid_int() or str(int(suffix))!=suffix or int(suffix)<1 or int(suffix)>=next: return -1
	return int(suffix)

## 深读前验证类型，未知附加领域由其自身的保存校验负责。
static func _fields(value: Variant, fields: Dictionary) -> bool:
	if not value is Dictionary: return false
	for key in fields:
		if typeof(value.get(key))!=fields[key]: return false
	return true

## 家庭日期、数量与年龄必须是有界整数，不把存档小数静默取整。
static func _whole(value: Variant, low: int, high: int) -> bool:
	return typeof(value)==TYPE_INT and value>=low and value<=high

## 将失败交给Kernel候选事务，领域本身不发布部分写入或另起一局游戏。
static func _fail(code: String) -> Dictionary:
	return {"ok":false,"code":code}
