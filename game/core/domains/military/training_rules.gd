## 真实驻军训练服务：军务授权、唯一单位占用与逐半日账由本叶核验；共用runtime负责时钟和人物释放。
extends RefCounted
const Assets=preload("res://core/military_assets.gd")
const Inventory=preload("res://core/inventory_rules.gd")
const Location=preload("res://core/world_location.gd")
const Combat=preload("res://core/combat_session.gd")
const Drill=preload("res://core/domains/military/training_drill.gd")
const Levels=preload("res://core/domains/cultivation/skill_levels.gd")
const Values=preload("res://core/base/value_checks.gd")
const Conditions=preload("res://core/domains/military/condition_history.gd")
const March=preload("res://core/domains/military/march_conditions.gd")
const KIND="military_train"
const FACT_KIND="military_training"
const DURATION=4
const FEE=150
const SKILL_BY_TROOP={"infantry":"infantry","archer":"archery","cavalry":"cavalry"}

## 只给新局初始化器投影作者明确真值；不遍历旧世界补50/0，也不声称迁移已有训练历史。
static func initial_profile(definition: Dictionary) -> Dictionary:
	if not Values.whole(definition.get("initial_training"),0,100) or not Values.whole(definition.get("initial_fatigue"),0,100): return _fail("TrainingProfileRequired")
	return {"ok":true,"profile":{"training":definition.initial_training,"fatigue":definition.initial_fatigue,"training_action_id":""}}

## 行动只接真实公职与一部现驻军；只读核在场、名册、单位占用、同组织公共付款/收款及实际教官等级。
static func preview(state: Dictionary, data: Dictionary, actor_id: String, params: Dictionary, own_action: String="") -> Dictionary:
	if params.size()!=2+int(params.has("challenge_mode")) or not Values.fields(params,{"office_id":TYPE_STRING,"unit_id":TYPE_STRING}) or (params.has("challenge_mode") and (not params.challenge_mode is String or not params.challenge_mode in ["automatic","manual"])): return _fail("InvalidMilitaryTrainingRequest")
	var authority: Dictionary=Assets.ready_office(state,data,actor_id,params.office_id)
	if not authority.ok: return authority
	var unit: Variant=state.get("military_units",{}).get(params.unit_id)
	if not unit is Dictionary or unit.get("garrison_id")!=authority.garrison_id or unit.get("organization_id")!=authority.organization_id or unit.get("army_id")!="" or not unit.get("mobilizing_action_id","").is_empty() or not unit.get("fit") is Array or unit.fit.is_empty(): return _fail("TrainingUnitUnavailable")
	if not _profile(unit): return _fail("TrainingProfileRequired")
	if not unit.training_action_id in ["",own_action]: return _fail("TrainingUnitReserved")
	if unit.training>=100 and own_action.is_empty(): return _fail("MilitaryTrainingAtLimit")
	for battle in state.get("battles",{}).values():
		if battle.get("phase")!="closed" and battle.get("units",{}).has(unit.id): return _fail("TrainingUnitInBattle")
	var camp: Dictionary=data.garrisons[authority.garrison_id]
	if not camp.get("training_wallet_id") is String or not Values.whole(camp.get("training_fatigue_per_tick"),0,20): return _fail("MilitaryTrainingNotOffered")
	var participants: Array=[actor_id]
	if not unit.commander_actor_id in participants: participants.append(unit.commander_actor_id)
	for person in unit.fit:
		if state.actors.has(person) and not person in participants: participants.append(person)
	participants.sort()
	for person in participants:
		var ready: Dictionary=_ready_actor(state,person,camp.place_id,own_action)
		if not ready.ok: return ready
	var skill_id: String=SKILL_BY_TROOP.get(data.get("troop_types",{}).get(unit.troop_type_id,{}).get("category"),"")
	if skill_id.is_empty(): return _fail("TrainingTroopUnsupported")
	var xp: Variant=state.actors[actor_id].get("skills",{}).get(skill_id)
	var level: int=Levels.level(xp)
	if level<0: return _fail("InvalidTrainingSkill")
	var payer_id: String=camp.wallet_id; var recipient_id: String=camp.training_wallet_id
	var owner: Dictionary={"kind":"organization","id":authority.organization_id}
	if payer_id==recipient_id or not state.wallets.has(payer_id) or not state.wallets.has(recipient_id) or not data.wallets.has(recipient_id) or state.wallets[payer_id].owner!=owner or state.wallets[recipient_id].owner!=owner: return _fail("TrainingPublicPaymentRequired")
	var reserved: int=state.wallets[payer_id].reserved.get(own_action,0) if not own_action.is_empty() else 0
	var required_space: int=FEE
	var running: Dictionary=state.get("facts",{}).get(own_action,{})
	if running.get("kind")==FACT_KIND and running.get("lessons") is Array: required_space=_portion(FEE,running.lessons.size()+1)
	if own_action.is_empty() and Inventory.spendable(state.wallets[payer_id])<FEE: return _fail("InsufficientFunds")
	if reserved<0 or state.wallets[recipient_id].balance>2147483647-required_space: return _fail("TrainingPaymentBlocked")
	var ownership: Dictionary=Assets.garrison_owner(state,data,camp.id)
	if not ownership.ok: return ownership
	var fit: Array=unit.fit.duplicate(); fit.sort()
	var terms: Dictionary={"actor_id":actor_id,"office_id":params.office_id,"authority_source":state.offices[params.office_id].appointment_id,"unit_id":unit.id,"garrison_id":camp.id,"organization_id":authority.organization_id,"ownership_id":ownership.occupation_id,"place_id":camp.place_id,"commander_actor_id":unit.commander_actor_id,"troop_type_id":unit.troop_type_id,"fit_ids":fit,"participants":participants,"skill_id":skill_id,"skill_xp":xp,"skill_level":level,"gain_total":3+level,"training_before":unit.training,"fatigue_before":unit.fatigue,"fatigue_per_tick":camp.training_fatigue_per_tick,"payer_wallet_id":payer_id,"recipient_wallet_id":recipient_id,"duration":DURATION,"fee":FEE}
	# 续课只核已冻结题面来源，不拿教官后来的兵法/智谋重抽或改价。
	if running.get("kind")==FACT_KIND:
		var fixed: Dictionary=Drill.validate(data,running,own_action)
		if not fixed.ok: return fixed
		if running.terms.has("drill"): terms.drill=running.terms.drill.duplicate(true)
	else:
		var quoted: Dictionary=Drill.quote(data,terms,state.actors[actor_id],params.get("challenge_mode","automatic"))
		if not quoted.ok: return quoted
		if quoted.has("drill"): terms.drill=quoted.drill
	return {"ok":true,"duration":DURATION,"cost":FEE,"participants":participants.duplicate(),"terms":terms,"expected_training_gain":mini(100,unit.training+3+level)-unit.training,"payment_policy":"provided_half_days_only"}

## runtime已建立action和人物占用后重新报价；成功才预留150及本unit，失败不留下部分账户修改。
static func begin(state: Dictionary, data: Dictionary, action: Dictionary, offer: Dictionary) -> Dictionary:
	if action.get("kind")!=KIND or not action.get("id") is String or action.id.is_empty() or not action.get("params") is Dictionary or not state.get("facts") is Dictionary: return _fail("InvalidMilitaryTrainingAction")
	if state.facts.has(action.id): return _fail("MilitaryTrainingAlreadyStarted")
	var current: Dictionary=preview(state,data,action.actor_id,action.params,action.id)
	if not current.ok: return current
	if current.terms!=offer.get("terms"): return _fail("StaleMilitaryTrainingQuote")
	var terms: Dictionary=current.terms
	if action.get("participants")!=terms.participants or action.get("start_tick")!=state.tick or action.get("remaining_ticks")!=DURATION: return _fail("InvalidMilitaryTrainingEnvelope")
	for person in terms.participants:
		if state.actors[person].action_id!=action.id: return _fail("TrainingActorOccupationMissing")
	if Inventory.spendable(state.wallets[terms.payer_wallet_id])<FEE: return _fail("InsufficientFunds")
	var fact: Dictionary={"kind":FACT_KIND,"phase":"started","actor_id":action.actor_id,"start_tick":action.start_tick,"completed_tick":-1,"success":false,"terms":terms.duplicate(true),"lessons":[],"code":""}
	var started: Dictionary=Drill.start(state,data,action,fact)
	if not started.ok: return started
	state.wallets[terms.payer_wallet_id].reserved[action.id]=FEE
	state.military_units[terms.unit_id].training_action_id=action.id
	action.params={"training":terms.duplicate(true)}
	state.facts[action.id]=fact
	return {"ok":true}

## 只在本半日真实供给之后提供一课；先核全部条件/余额再原子扣款、训练和单位疲劳，重复tick不重复提供。
static func step(state: Dictionary, data: Dictionary, action: Dictionary, finish_tick: int) -> Dictionary:
	var record: Dictionary=_record(state,data,action)
	if not record.ok: return record
	var fact: Dictionary=record.fact
	if fact.phase=="completed": return {"ok":true,"completed":true,"suspended":false,"applied":false}
	if not fact.lessons.is_empty() and fact.lessons[-1].tick==finish_tick: return {"ok":true,"completed":action.remaining_ticks==0,"suspended":false,"applied":false}
	var valid: Dictionary=validate_action(state,data,action)
	if not valid.ok: return valid
	if finish_tick!=state.tick+1: return _fail("MilitaryTrainingClockMismatch")
	var terms: Dictionary=fact.terms
	var continuation: Dictionary=preview(state,data,action.actor_id,{"office_id":terms.office_id,"unit_id":terms.unit_id},action.id)
	var code: String=""
	if not continuation.ok: code=continuation.code
	else:
		for key in terms:
			if key in ["training_before","fatigue_before"]: continue
			if continuation.terms.get(key)!=terms[key]: code="MilitaryTrainingConditionsChanged"; break
	var supplied: Dictionary={}
	if code.is_empty():
		supplied=_supply(state,data,terms,finish_tick)
		if not supplied.ok:
			if supplied.code!="MilitaryTrainingUnsupplied": return supplied
			code=supplied.code
	if not code.is_empty():
		fact.code=code; action.remaining_ticks=0
		return {"ok":true,"completed":true,"suspended":false,"applied":false}
	var frozen: Dictionary=Drill.freeze_before_lesson(data,fact,action.id,finish_tick)
	if not frozen.ok: return frozen
	var index: int=fact.lessons.size()+1
	var cost: int=_portion(FEE,index)
	var unit: Dictionary=state.military_units[terms.unit_id]
	var training_after: int=_training_after(fact,index)
	var fatigue_after: int=mini(100,terms.fatigue_before+terms.fatigue_per_tick*index)
	var lesson: Dictionary={"tick":finish_tick,"supply_fact_id":supplied.id,"fee_paid":cost,"training_before":unit.training,"training_after":training_after,"fatigue_before":unit.fatigue,"fatigue_after":fatigue_after}
	state.wallets[terms.payer_wallet_id].balance-=cost
	state.wallets[terms.payer_wallet_id].reserved[action.id]-=cost
	state.wallets[terms.recipient_wallet_id].balance+=cost
	unit.training=training_after; unit.fatigue=fatigue_after
	fact.lessons.append(lesson); action.remaining_ticks-=1
	return {"ok":true,"completed":action.remaining_ticks==0,"suspended":false,"applied":true}

## 完课/失效只封账和释放本单位/未用钱款；训练与付款早已按真实课时发生，不能再次发整课奖励。
static func finish(state: Dictionary, data: Dictionary, action: Dictionary, finish_tick: int) -> Dictionary:
	var record: Dictionary=_record(state,data,action)
	if not record.ok: return record
	var fact: Dictionary=record.fact
	if fact.phase=="completed": return _receipt(fact,true)
	if action.remaining_ticks!=0 or finish_tick!=state.tick+1: return _fail("MilitaryTrainingNotReady")
	if fact.code.is_empty() and fact.lessons.size()!=DURATION: return _fail("MilitaryTrainingNotReady")
	return _end(state,data,action,fact,finish_tick,fact.code)

## 仅本人当前精确action可中断；旧ID不能释放新课程或别人的公共预留。
static func preview_cancel(state: Dictionary, data: Dictionary, actor_id: String, params: Dictionary) -> Dictionary:
	if params.size()!=1 or not Values.fields(params,{"action_id":TYPE_STRING}): return _fail("InvalidMilitaryTrainingCancellation")
	var actor: Dictionary=state.get("actors",{}).get(actor_id,{})
	var action: Dictionary=state.get("actions",{}).get(actor.get("action_id",""),{})
	if action.get("kind")!=KIND or action.get("actor_id")!=actor_id: return _fail("NotMilitaryTraining")
	if params.action_id!=action.id: return _fail("StaleMilitaryTrainingAction")
	var valid: Dictionary=validate_action(state,data,action)
	if not valid.ok: return valid
	return {"ok":true,"action_id":action.id,"provided_half_days":state.facts[action.id].lessons.size()}

## 取消保留已提供训练/付款/疲劳，释放未来份额；人物与actions表由共用runtime正常释放。
static func cancel(state: Dictionary, data: Dictionary, actor_id: String, action_id: String) -> Dictionary:
	var checked: Dictionary=preview_cancel(state,data,actor_id,{"action_id":action_id})
	if not checked.ok: return checked
	return _end(state,data,state.actions[action_id],state.facts[action_id],state.tick,"MilitaryTrainingCancelled")

## 忙碌instant只允许本人当前训练的精确题面转换；成功只写已验证实例，不推进时间、随机或结算。
static func preview_drill(state: Dictionary, data: Dictionary, actor_id: String, kind: String, params: Dictionary) -> Dictionary:
	var actor: Dictionary=state.get("actors",{}).get(actor_id,{})
	var action: Dictionary=state.get("actions",{}).get(actor.get("action_id",""),{})
	if action.get("kind")!=KIND or action.get("actor_id")!=actor_id: return _fail("NotMilitaryTraining")
	if params.get("action_id")!=action.get("id"): return _fail("StaleMilitaryTrainingAction")
	var valid: Dictionary=validate_action(state,data,action)
	if not valid.ok: return valid
	var fact: Dictionary=state.facts[action.id]
	var changed: Dictionary=Drill.transition(data,fact,action.id,actor_id,kind,params)
	if not changed.ok: return changed
	return {"ok":true,"action_id":action.id,"before":fact.drill.challenge.duplicate(true),"after":changed.state}

## 提交只消费重新推导的私有候选；来源或旧草案变化时不写实例。
static func apply_drill(state: Dictionary, data: Dictionary, actor_id: String, kind: String, params: Dictionary, offer: Dictionary) -> Dictionary:
	var checked: Dictionary=preview_drill(state,data,actor_id,kind,params)
	if not checked.ok: return checked
	if checked!=offer: return _fail("StaleTrainingDrillQuote")
	state.facts[checked.action_id].drill.challenge=checked.after.duplicate(true)
	return {"ok":true,"action_id":checked.action_id}

## 保留领域直接调用入口，同样复用唯一预览与提交守卫。
static func transition_drill(state: Dictionary, data: Dictionary, actor_id: String, kind: String, params: Dictionary) -> Dictionary:
	var offer: Dictionary=preview_drill(state,data,actor_id,kind,params)
	return apply_drill(state,data,actor_id,kind,params,offer) if offer.ok else offer

## 本人公开题面按持久行动来源读取；查询不泄露作者解证据或写入课时。
static func drill_view(state: Dictionary, data: Dictionary, actor_id: String, action_id: String) -> Dictionary:
	var fact: Variant=state.get("facts",{}).get(action_id)
	if not fact is Dictionary or fact.get("actor_id")!=actor_id: return _fail("TrainingDrillActorMismatch")
	return Drill.view(data,fact,action_id,actor_id)

## 活动事实、双方占用、钱款余量和已提供课时必须一致；角色疲劳永远不是部队疲劳来源。
static func validate_action(state: Dictionary, data: Dictionary, action: Dictionary) -> Dictionary:
	var record: Dictionary=_record(state,data,action)
	if not record.ok: return record
	var fact: Dictionary=record.fact; var t: Dictionary=fact.terms; var count: int=fact.lessons.size()
	if fact.phase!="started" or not fact.code.is_empty() or count>=DURATION or action.remaining_ticks!=DURATION-count or state.tick!=fact.start_tick+count: return _fail("MilitaryTrainingProgressMismatch")
	var unit: Dictionary=state.military_units[t.unit_id]
	if unit.training_action_id!=action.id: return _fail("TrainingUnitOccupationMissing")
	if unit.training!=_training_after(fact,count) or unit.fatigue!=mini(100,t.fatigue_before+t.fatigue_per_tick*count): return _fail("MilitaryTrainingConditionMismatch")
	if state.wallets[t.payer_wallet_id].reserved.get(action.id)!=FEE-FEE*count/DURATION or state.wallets[t.payer_wallet_id].balance<FEE-FEE*count/DURATION: return _fail("MilitaryTrainingReservationMismatch")
	for person in t.participants:
		if state.actors[person].action_id!=action.id: return _fail("TrainingActorOccupationMissing")
	return {"ok":true}

## 稳定时钟下合并原课时与已核真实行军回执；旧剧本活动仍为空，训练和疲劳不增加第二份当前真值。
static func validate_state(state: Dictionary, data: Dictionary) -> Dictionary:
	var marches: Dictionary=March.validated_projections(state,data,state.tick)
	if not marches.ok: return marches
	var timelines: Dictionary={}
	for id in state.get("facts",{}):
		var fact: Variant=state.facts[id]
		if not fact is Dictionary or fact.get("kind")!=FACT_KIND: continue
		var checked: Dictionary=_fact(state,data,fact,id)
		if not checked.ok: return checked
		var unit_id: String=fact.terms.unit_id
		if not timelines.has(unit_id): timelines[unit_id]=[]
		timelines[unit_id].append({"action_id":id,"fact":fact})
		if fact.phase=="started":
			if not state.actions.has(id): return _fail("MissingMilitaryTrainingAction")
			checked=validate_action(state,data,state.actions[id])
			if not checked.ok: return checked
		elif state.actions.has(id) or fact.completed_tick>state.tick or state.wallets[fact.terms.payer_wallet_id].reserved.has(id): return _fail("MilitaryTrainingCompletionMismatch")
	for id in state.get("military_units",{}):
		var unit: Dictionary=state.military_units[id]; var definition: Dictionary=_definition(data,unit)
		if not definition.has("initial_training") and not unit.has("training") and not timelines.has(id): continue
		var initial: Dictionary=initial_profile(definition)
		if not initial.ok or not _profile(unit): return _fail("TrainingProfileRequired",id)
		var folded: Dictionary=Conditions.fold(id,{"training":initial.profile.training,"fatigue":initial.profile.fatigue},timelines.get(id,[]),marches.activities_by_unit.get(id,[]),state.tick)
		if not folded.ok:
			return _fail("DuplicateMilitaryTrainingTick" if folded.code=="MilitaryConditionHistoryTimeMismatch" else "MilitaryTrainingHistoryMismatch",id)
		if unit.training!=folded.profile.training or unit.fatigue!=folded.profile.fatigue: return _fail("MilitaryTrainingHistoryMismatch",id)
		if not unit.training_action_id.is_empty():
			var action: Dictionary=state.get("actions",{}).get(unit.training_action_id,{})
			if action.get("kind")!=KIND or action.get("params",{}).get("training",{}).get("unit_id")!=id: return _fail("TrainingUnitOccupationMissing",id)
	return {"ok":true}

## 只消费当前已实际落账的唯一驻军口粮，不预造食物收据或重扣第二份。
static func _supply(state: Dictionary, data: Dictionary, terms: Dictionary, tick: int) -> Dictionary:
	var found: String=""
	for id in state.facts:
		var fact: Variant=state.facts[id]
		if not fact is Dictionary or fact.get("kind")!="military_supply" or fact.get("garrison_id")!=terms.garrison_id or fact.get("army_id")!="" or fact.get("tick")!=tick: continue
		if not found.is_empty(): return _fail("MilitaryTrainingSupplyAmbiguous")
		found=id
	if found.is_empty(): return _fail("MilitaryTrainingSupplyRequired")
	var checked: Dictionary=_supply_fact(state.facts[found],data,terms,tick)
	if not checked.ok: return checked
	return {"ok":true,"id":found}

## 历史与当前口粮都核本营/组织/半日、真实人数与全部参训者；不足粮是正常中断，坏来源则失败。
static func _supply_fact(fact: Dictionary, data: Dictionary, terms: Dictionary, tick: int) -> Dictionary:
	if fact.get("kind")!="military_supply" or fact.get("garrison_id")!=terms.garrison_id or fact.get("army_id")!="" or fact.get("organization_id")!=terms.organization_id or fact.get("ownership_id","")!=terms.ownership_id or fact.get("tick")!=tick or not _strings(fact.get("mouth_ids")) or not Values.whole(fact.get("required"),0,2147483647) or not Values.whole(fact.get("consumed"),0,2147483647) or not Values.whole(fact.get("shortage"),0,2147483647): return _fail("InvalidMilitaryTrainingSupply")
	if fact.required!=fact.mouth_ids.size()*data.garrisons[terms.garrison_id].ration_per_person or fact.consumed+fact.shortage!=fact.required: return _fail("InvalidMilitaryTrainingSupply")
	for person in terms.fit_ids+terms.participants:
		if not person in fact.mouth_ids: return _fail("MilitaryTrainingSupplyRosterMismatch")
	if fact.shortage>0: return _fail("MilitaryTrainingUnsupplied")
	return {"ok":true}

## 永久事实严格核固定服务条款、真实任命来源和每课的差额；动作外壳另由_record核对。
static func _fact(state: Dictionary, data: Dictionary, fact: Dictionary, action_id: String) -> Dictionary:
	if fact.size()!=9+int(fact.has("drill")) or not Values.fields(fact,{"kind":TYPE_STRING,"phase":TYPE_STRING,"actor_id":TYPE_STRING,"start_tick":TYPE_INT,"completed_tick":TYPE_INT,"success":TYPE_BOOL,"terms":TYPE_DICTIONARY,"lessons":TYPE_ARRAY,"code":TYPE_STRING}) or fact.kind!=FACT_KIND or not fact.phase in ["started","completed"]: return _fail("InvalidMilitaryTrainingFact")
	var t: Dictionary=fact.terms
	if t.size()!=23+int(t.has("drill")) or not Values.fields(t,{"duration":TYPE_INT,"fee":TYPE_INT,"skill_xp":TYPE_INT,"skill_level":TYPE_INT,"gain_total":TYPE_INT,"training_before":TYPE_INT,"fatigue_before":TYPE_INT,"fatigue_per_tick":TYPE_INT}): return _fail("InvalidMilitaryTrainingTerms")
	for key in ["actor_id","office_id","authority_source","unit_id","garrison_id","organization_id","ownership_id","place_id","commander_actor_id","troop_type_id","skill_id","payer_wallet_id","recipient_wallet_id"]:
		if not t.get(key) is String: return _fail("InvalidMilitaryTrainingTerms",key)
	if t.get("duration")!=DURATION or t.get("fee")!=FEE or not _strings(t.get("fit_ids")) or t.fit_ids.is_empty() or not _strings(t.get("participants")) or not t.actor_id in t.participants or not t.commander_actor_id in t.participants: return _fail("InvalidMilitaryTrainingTerms")
	if not Values.whole(t.get("training_before"),0,100) or not Values.whole(t.get("fatigue_before"),0,100) or not Values.whole(t.get("fatigue_per_tick"),0,20) or Levels.level(t.get("skill_xp"))<0 or t.get("skill_level")!=Levels.level(t.skill_xp) or t.get("gain_total")!=3+t.skill_level: return _fail("InvalidMilitaryTrainingTerms")
	if not state.get("military_units",{}).has(t.unit_id) or not data.get("garrisons",{}).has(t.garrison_id) or not data.get("offices",{}).has(t.office_id) or not state.wallets.has(t.payer_wallet_id) or not state.wallets.has(t.recipient_wallet_id) or t.payer_wallet_id==t.recipient_wallet_id: return _fail("UnknownMilitaryTrainingReference")
	var office: Dictionary=data.offices[t.office_id]
	var unit: Dictionary=state.military_units[t.unit_id]
	if office.garrison_id!=t.garrison_id or office.organization_id!=t.organization_id or unit.get("troop_type_id")!=t.troop_type_id or not unit.get("member_ids") is Array: return _fail("MilitaryTrainingAuthoritySourceMissing")
	var expected_people: Array=[t.actor_id]
	if not t.commander_actor_id in expected_people: expected_people.append(t.commander_actor_id)
	for person in t.fit_ids:
		if not person in unit.member_ids: return _fail("MilitaryTrainingRosterSourceMissing")
		if state.actors.has(person) and not person in expected_people: expected_people.append(person)
	expected_people.sort()
	if expected_people!=t.participants: return _fail("MilitaryTrainingRosterSourceMissing")
	for person in expected_people:
		if not state.actors.has(person): return _fail("UnknownMilitaryTrainingReference")
	var camp: Dictionary=data.garrisons[t.garrison_id]
	if t.place_id!=camp.place_id or t.payer_wallet_id!=camp.wallet_id or t.recipient_wallet_id!=camp.get("training_wallet_id") or t.fatigue_per_tick!=camp.get("training_fatigue_per_tick") or t.skill_id!=SKILL_BY_TROOP.get(data.get("troop_types",{}).get(t.troop_type_id,{}).get("category")): return _fail("MilitaryTrainingDefinitionMismatch")
	var appointment: Dictionary=state.facts.get(t.authority_source,{})
	if appointment.get("kind")!="military_action" or appointment.get("command")!="office_accept" or appointment.get("phase")!="completed" or appointment.get("success")!=true or appointment.get("actor_id")!=t.actor_id or appointment.get("terms",{}).get("office_id")!=t.office_id or not Values.whole(appointment.get("completed_tick"),0,fact.start_tick): return _fail("MilitaryTrainingAuthoritySourceMissing")
	if fact.actor_id!=t.actor_id or not Values.whole(fact.start_tick,0,state.tick) or fact.lessons.size()>DURATION: return _fail("InvalidMilitaryTrainingTime")
	var drill_valid: Dictionary=Drill.validate(data,fact,action_id)
	if not drill_valid.ok: return drill_valid
	var training: int=t.training_before; var fatigue: int=t.fatigue_before
	for index in range(fact.lessons.size()):
		var row: Variant=fact.lessons[index]; var number: int=index+1
		if not row is Dictionary or row.size()!=7 or not Values.fields(row,{"tick":TYPE_INT,"supply_fact_id":TYPE_STRING,"fee_paid":TYPE_INT,"training_before":TYPE_INT,"training_after":TYPE_INT,"fatigue_before":TYPE_INT,"fatigue_after":TYPE_INT}): return _fail("InvalidMilitaryTrainingLesson")
		if row.tick!=fact.start_tick+number or row.fee_paid!=_portion(FEE,number) or row.training_before!=training or row.fatigue_before!=fatigue or row.training_after!=_training_after(fact,number) or row.fatigue_after!=mini(100,t.fatigue_before+t.fatigue_per_tick*number): return _fail("MilitaryTrainingLessonMismatch")
		var supplied: Dictionary=_supply_fact(state.facts.get(row.supply_fact_id,{}),data,t,row.tick)
		if not supplied.ok: return supplied
		training=row.training_after; fatigue=row.fatigue_after
	if fact.phase=="started":
		if fact.completed_tick!=-1 or fact.success: return _fail("PrematureMilitaryTrainingResult")
	else:
		if fact.code.is_empty() and fact.lessons.size()!=DURATION: return _fail("InvalidMilitaryTrainingCompletion")
		if not Values.whole(fact.completed_tick,fact.start_tick+fact.lessons.size(),fact.start_tick+fact.lessons.size()+1) or fact.success!=(fact.code.is_empty() and fact.lessons.size()==DURATION): return _fail("InvalidMilitaryTrainingCompletion")
	return {"ok":true}

## action与其唯一永久事实完全一致，不能拿另课条款或名册来支付/重复授益。
static func _record(state: Dictionary, data: Dictionary, action: Dictionary) -> Dictionary:
	if action.get("kind")!=KIND or not action.get("id") is String or not action.get("params") is Dictionary or action.params.size()!=1 or not action.params.get("training") is Dictionary: return _fail("InvalidMilitaryTrainingAction")
	var fact: Variant=state.get("facts",{}).get(action.id)
	if not fact is Dictionary: return _fail("MilitaryTrainingFactMissing")
	var valid: Dictionary=_fact(state,data,fact,action.id)
	if not valid.ok: return valid
	if action.get("actor_id")!=fact.actor_id or action.get("start_tick")!=fact.start_tick or action.get("participants")!=fact.terms.participants or action.params.training!=fact.terms or not Values.whole(action.get("remaining_ticks"),0,DURATION): return _fail("MilitaryTrainingFactMismatch")
	return {"ok":true,"fact":fact}

## 封账只释放自己的单位标记和钱包预留，保留其他并行动作，不修改角色疲劳或世界RNG。
static func _end(state: Dictionary, data: Dictionary, action: Dictionary, fact: Dictionary, tick: int, code: String) -> Dictionary:
	var closed: Dictionary=Drill.close_unprovided(data,fact,action.id)
	if not closed.ok: return closed
	fact.phase="completed"; fact.completed_tick=tick; fact.success=code.is_empty() and fact.lessons.size()==DURATION; fact.code=code
	var unit: Dictionary=state.military_units[fact.terms.unit_id]
	if unit.training_action_id==action.id: unit.training_action_id=""
	state.wallets[fact.terms.payer_wallet_id].reserved.erase(action.id)
	action.remaining_ticks=0
	return _receipt(fact)

## 回执从不可变已提供课时计算，结束后重复读取不依赖未来单位变化。
static func _receipt(fact: Dictionary, repeated: bool=false) -> Dictionary:
	var count: int=fact.lessons.size()
	var training_after: int=fact.terms.training_before if count==0 else fact.lessons[-1].training_after
	return {"ok":true,"success":fact.success,"code":fact.code,"already_completed":repeated,"details":{"unit_id":fact.terms.unit_id,"provided_half_days":count,"fee_paid":FEE*count/DURATION,"training_gain":training_after-fact.terms.training_before,"released_unprovided_fee":FEE-FEE*count/DURATION}}

## 正常累计先取整，再乘首课冻结表现并取整，最后封顶100；零课/无题面不新增收益或默认实例。
static func _training_after(fact: Dictionary, count: int) -> int:
	var normal: int=fact.terms.gain_total*count/DURATION
	var bp: int=10000
	if count>0 and fact.has("drill"): bp=fact.drill.performance_bp
	return mini(100,fact.terms.training_before+normal*bp/10000)

## 累计差额保证150钱恰为37/38/37/38；不用四次向下取整丢掉尾数。
static func _portion(total: int, number: int) -> int:
	return total*number/DURATION-total*(number-1)/DURATION

## 精确初值只从该部源驻军作者条目读取，不能把当前驻地的别部定义套入。
static func _definition(data: Dictionary, unit: Dictionary) -> Dictionary:
	for entry in data.get("garrisons",{}).get(unit.get("source_garrison_id"),{}).get("units",[]):
		if entry.get("id")==unit.get("id"): return entry
	return {}

## 永久名册采用已排序唯一字符串，拒重复来源而非静默去重。
static func _strings(value: Variant) -> bool:
	if not value is Array: return false
	var previous: String=""
	for entry in value:
		if not entry is String or entry.is_empty() or (not previous.is_empty() and entry<=previous): return false
		previous=entry
	return true

## 部队训练/疲劳独立于角色疲劳；缺任一字段拒绝，不从旁人状态代填。
static func _profile(unit: Dictionary) -> bool:
	return Values.whole(unit.get("training"),0,100) and Values.whole(unit.get("fatigue"),0,100) and unit.get("training_action_id") is String

## 实际教官和命名参训者必须在营、可用且只被本action占用，重伤和战斗占用不能借训练绕过。
static func _ready_actor(state: Dictionary, actor_id: String, place_id: String, own_action: String) -> Dictionary:
	var actor: Variant=state.get("actors",{}).get(actor_id)
	if not actor is Dictionary or not actor.get("alive",false) or not actor.get("available",false) or actor.get("hp",0)<=0 or actor.get("injury",0)>=3: return _fail("TrainingActorUnavailable")
	if not actor.get("action_id","") in ["",own_action]: return _fail("ActorBusy")
	if Combat.occupied(state,actor_id): return _fail("BattlePending")
	if Location.resolve(state,actor_id).get("place_id")!=place_id: return _fail("TrainingAssemblyRequired")
	return {"ok":true}

## 统一领域错误供外层原子候选处理，不把失败解释为免费训练完成。
static func _fail(code: String, path: String="") -> Dictionary:
	return {"ok":false,"code":code,"path":path}
