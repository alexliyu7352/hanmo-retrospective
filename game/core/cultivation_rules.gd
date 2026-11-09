## 人物装备、学习与传授规则；只改变Kernel候选世界，不建立独立经验、资金或时间账本。
extends RefCounted
const Armor=preload("res://core/domains/personal/personal_armor.gd")
const SkillEligibility=preload("res://core/domains/cultivation/skill_eligibility.gd")
const Interaction=preload("res://core/interaction_rules.gd")
const Inventory=preload("res://core/inventory_rules.gd")
const Location=preload("res://core/world_location.gd")
const KINDS=["equip_weapon","equip_armor","configure_heart","learn_skill","practice_skill","teach_skill"]
const TRAINING_KINDS=["learn_skill","practice_skill","teach_skill"]

## 查询实际装备和成长条件，不改变人物、日期、资金、事实或随机流。
static func preview(state: Dictionary, data: Dictionary, actor_id: String, kind: String, params: Dictionary) -> Dictionary:
	return _preview(state,data,actor_id,kind,params,"")

## 在Kernel已占用参与者时允许核验自身行动，其他占用和参战仍拒绝。
static func _preview(state: Dictionary, data: Dictionary, actor_id: String, kind: String, params: Dictionary, own_action: String) -> Dictionary:
	if not kind in KINDS: return _fail("UnknownCultivationCommand")
	var ready: Dictionary=_ready(state,actor_id,own_action)
	if not ready.ok: return ready
	var actor: Dictionary=state.actors[actor_id]
	if kind=="equip_armor":
		if params.size()!=1 or not params.get("armor_id") is String: return _fail("InvalidArmorChoice")
		return Armor.selectable(state,data,actor_id,params.armor_id)
	if kind=="equip_weapon":
		var weapon: Variant=params.get("weapon_id")
		if not weapon is String: return _fail("InvalidWeaponChoice")
		if not weapon.is_empty():
			if not data.get("items",{}).has(weapon) or not data.items[weapon].get("weapon") is Dictionary: return _fail("NotAWeapon")
			if not state.get("inventories",{}).has(actor.inventory_id): return _fail("MissingPersonalInventory")
			if Inventory.count(state.inventories[actor.inventory_id],weapon,[{"kind":"actor","id":actor_id}])<1: return _fail("WeaponNotOwned")
		return {"ok":true,"duration":0,"weapon_id":weapon}
	var skill_id: Variant=params.get("skill_id")
	if not skill_id is String: return _fail("InvalidSkillChoice")
	if kind=="configure_heart" and skill_id.is_empty(): return {"ok":true,"duration":0,"skill_id":""}
	if not data.get("personal_skills",{}).has(skill_id): return _fail("UnknownPersonalSkill")
	var skill: Dictionary=data.personal_skills[skill_id]
	if kind=="configure_heart":
		if skill.kind!="heart" or not actor.combat.learned.has(skill_id): return _fail("HeartNotLearned")
		return {"ok":true,"duration":0,"skill_id":skill_id}
	if actor.get("injury",0)>=3 or actor.fatigue>=90: return _fail("TrainingNeedsRest")
	var location: Dictionary=Location.resolve(state,actor_id)
	if location.get("kind")!="Settlement" or not data.get("places",{}).has(location.get("place_id")): return _fail("TrainingPlaceRequired")
	var student_id: String=actor_id
	var teacher_id: String=""
	var duration: int=0
	var context: Dictionary={}
	var fee: int=0
	var recipient_wallet: String=""
	if kind=="practice_skill":
		if not _whole(params.get("ticks"),1,8): return _fail("InvalidPracticeDuration")
		duration=params.ticks
	else:
		if params.get("context") is Dictionary and params.context.get("kind")=="encounter": return _fail("TrainingPlaceRequired")
		var service: String="learn_skill" if kind=="learn_skill" else ""
		var contact: Dictionary=Interaction.validate(state,data,actor_id,params.get("context"),service,own_action)
		if not contact.ok: return contact
		context=contact.context
		var provider_id: String=context.provider_actor_id
		ready=_ready(state,provider_id,own_action)
		if not ready.ok: return ready
		if state.actors[provider_id].get("injury",0)>=3 or state.actors[provider_id].fatigue>=90: return _fail("TeacherNeedsRest")
		teacher_id=provider_id if kind=="learn_skill" else actor_id
		student_id=actor_id if kind=="learn_skill" else provider_id
		if state.actors[teacher_id].combat.learned.get(skill_id,-1)<80: return _fail("TeacherNotQualified")
		if kind=="learn_skill":
			var role: Dictionary=data.roles[context.role_id]
			var lesson: Dictionary={}
			for entry in role.get("lessons",[]):
				if entry.skill_id==skill_id: lesson=entry; break
			if lesson.is_empty(): return _fail("LessonNotOffered")
			if _trust(state,teacher_id,student_id)<lesson.required_trust: return _fail("TeacherTrustRequired")
			duration=lesson.study_ticks; fee=lesson.fee; recipient_wallet=lesson.wallet_id
			var owner: Dictionary={"kind":"actor","id":teacher_id} if role.organization_id==null else {"kind":"organization","id":role.organization_id}
			if not state.wallets.has(recipient_wallet) or state.wallets[recipient_wallet].owner!=owner: return _fail("LessonPaymentAuthority")
		else: duration=2
	var student: Dictionary=state.actors[student_id]
	var qualified: Dictionary=SkillEligibility.check(student,skill)
	if not qualified.ok: return qualified
	var before: int=student.combat.learned.get(skill_id,-1)
	var after: int=0
	if kind=="learn_skill":
		if before>=0: return _fail("SkillAlreadyLearned")
	elif kind=="practice_skill":
		if before<0: return _fail("SkillNotLearned")
		if before>=100: return _fail("SkillAlreadyMastered")
		var gain: int=_practice_gain(skill.practice_ticks,duration)
		if student.fatigue>=60: gain=maxi(1,gain/2)
		after=mini(100,before+gain)
	else:
		var teacher_mastery: int=state.actors[teacher_id].combat.learned[skill_id]
		if before>=teacher_mastery: return _fail("StudentAlreadyQualified")
		after=mini(teacher_mastery,maxi(0,before)+10)
	var payer_wallet: String=actor.wallet_id if kind=="learn_skill" else ""
	if not payer_wallet.is_empty():
		if not state.wallets.has(payer_wallet) or state.wallets[payer_wallet].owner!={"kind":"actor","id":actor_id}: return _fail("PersonalPaymentRequired")
		if payer_wallet==recipient_wallet: return _fail("SelfPayment")
		if Inventory.spendable(state.wallets[payer_wallet])<fee: return _fail("InsufficientFunds")
		if state.wallets[recipient_wallet].balance>2147483647-fee: return _fail("MoneyOverflow")
	var training: Dictionary={"skill_id":skill_id,"student_id":student_id,"teacher_id":teacher_id,"place_id":location.place_id,"duration":duration,"mastery_before":before,"mastery_after":after,"student_fatigue":duration*(6 if kind=="practice_skill" else 4),"teacher_fatigue":duration*2 if not teacher_id.is_empty() else 0,"fee":fee,"payer_wallet_id":payer_wallet,"recipient_wallet_id":recipient_wallet}
	var quote: Dictionary={"ok":true,"duration":duration,"cost":fee,"training":training}
	if not context.is_empty(): quote.context=context
	return quote

## 起步时重新核验报价、冻结教学条款并真实转账；同一行动永久事实防止重复收费。
static func begin(state: Dictionary, data: Dictionary, action: Dictionary, quote: Dictionary) -> Dictionary:
	if not action.get("kind") in TRAINING_KINDS or not action.get("id") is String or not action.get("actor_id") is String or not action.get("params") is Dictionary: return _fail("InvalidTrainingAction")
	if not state.get("facts") is Dictionary: return _fail("MissingTrainingFacts")
	if state.facts.has(action.id): return _fail("TrainingAlreadyStarted")
	var current: Dictionary=_preview(state,data,action.actor_id,action.kind,action.params,action.id)
	if not current.ok: return current
	if current.get("training")!=quote.get("training") or current.get("context",{})!=quote.get("context",{}): return _fail("StaleTrainingQuote")
	if action.get("remaining_ticks")!=current.duration or action.get("start_tick")!=state.tick: return _fail("InvalidTrainingDuration")
	var expected: Array=[action.actor_id]
	if current.has("context"): expected.append(current.context.provider_actor_id)
	if action.get("participants")!=expected: return _fail("InvalidTrainingParticipants")
	for person in expected:
		if state.actors[person].action_id!=action.id: return _fail("TrainingOccupationMissing")
	var training: Dictionary=current.training.duplicate(true)
	if training.fee>0:
		state.wallets[training.payer_wallet_id].balance-=training.fee
		state.wallets[training.recipient_wallet_id].balance+=training.fee
	action.params={"training":training}
	if current.has("context"): action.params.context=current.context.duplicate(true)
	state.facts[action.id]={"kind":"training","phase":"started","action_kind":action.kind,"actor_id":action.actor_id,"start_tick":action.start_tick,"completed_tick":-1,"success":false,"training":training.duplicate(true),"context":current.get("context",{}).duplicate(true)}
	return {"ok":true}

## 只在实际行动到期后写入掌握与疲劳；中断记为失败，重试已结算事实不再产生效果。
static func finish(state: Dictionary, data: Dictionary, action: Dictionary) -> Dictionary:
	var valid: Dictionary=_validate(state,data,action,true)
	if not valid.ok: return valid
	var fact: Dictionary=state.facts[action.id]
	if fact.phase=="completed": return {"ok":true,"success":fact.success,"already_completed":true,"code":"" if fact.success else "TrainingInterrupted"}
	if action.remaining_ticks!=0 or state.tick+1<action.start_tick+action.params.training.duration: return _fail("TrainingNotReady")
	var training: Dictionary=action.params.training
	var success: bool=true
	for person in action.participants:
		var actor: Dictionary=state.actors[person]
		if not _ready(state,person,action.id).ok or actor.get("injury",0)>=3 or Location.resolve(state,person).get("place_id")!=training.place_id: success=false
	if action.kind!="practice_skill":
		var contact: Dictionary=Interaction.validate(state,data,action.actor_id,action.params.context,"learn_skill" if action.kind=="learn_skill" else "",action.id)
		if not contact.ok or state.actors[training.teacher_id].combat.learned.get(training.skill_id,-1)<80: success=false
	if success:
		var student: Dictionary=state.actors[training.student_id]
		student.combat.learned[training.skill_id]=training.mastery_after
		student.fatigue=mini(100,student.fatigue+training.student_fatigue)
		if not training.teacher_id.is_empty():
			var teacher: Dictionary=state.actors[training.teacher_id]
			teacher.fatigue=mini(100,teacher.fatigue+training.teacher_fatigue)
	fact.phase="completed"; fact.completed_tick=state.tick+1; fact.success=success
	return {"ok":true,"success":success,"code":"" if success else "TrainingInterrupted","skill_id":training.skill_id,"mastery":training.mastery_after if success else training.mastery_before}

## 装备和心法配置在提交时复核随身所有权或已学资格，不消费时间、复制物品或授予本领。
static func apply_instant(state: Dictionary, data: Dictionary, actor_id: String, kind: String, quote: Dictionary) -> Dictionary:
	if kind=="equip_armor":
		var armor: Dictionary=preview(state,data,actor_id,kind,{"armor_id":quote.get("armor_id")})
		if not armor.ok: return armor
		if armor!=quote: return _fail("StaleArmorChoice")
		state.actors[actor_id].combat.armor_id=armor.armor_id
		return {"ok":true}
	if not kind in ["equip_weapon","configure_heart"]: return _fail("NotInstantCultivation")
	var current: Dictionary=preview(state,data,actor_id,kind,quote)
	if not current.ok: return current
	var actor: Dictionary=state.actors[actor_id]
	if kind=="equip_weapon": actor.combat.weapon_id=current.weapon_id
	else: actor.combat.heart_id=current.skill_id
	return {"ok":true}

## 校验稳定半日边界的教学行动、人物占用与永久条款；只读存档检查不重收费或补发技能。
static func validate_action(state: Dictionary, data: Dictionary, action: Dictionary) -> Dictionary:
	return _validate(state,data,action,false)

## 对活动与已结算重试共用身份检查；完成阶段允许剩余时间为零，但不接受伪造的成长幅度。
static func _validate(state: Dictionary, data: Dictionary, action: Dictionary, finishing: bool) -> Dictionary:
	if not action.get("kind") in TRAINING_KINDS or not action.get("id") is String or not action.get("actor_id") is String or not action.get("params") is Dictionary: return _fail("InvalidTrainingAction")
	var training: Variant=action.params.get("training")
	if not training is Dictionary: return _fail("MissingTrainingTerms")
	var string_fields: Array=["skill_id","student_id","teacher_id","place_id","payer_wallet_id","recipient_wallet_id"]
	for key in string_fields:
		if not training.get(key) is String: return _fail("InvalidTrainingTerms")
	if training.size()!=12 or not _whole(training.get("duration"),1,30) or not _whole(training.get("mastery_before"),-1,100) or not _whole(training.get("mastery_after"),0,100) or not _whole(training.get("student_fatigue"),0,180) or not _whole(training.get("teacher_fatigue"),0,60) or not _whole(training.get("fee"),0,10000): return _fail("InvalidTrainingTerms")
	if not data.get("personal_skills",{}).has(training.skill_id) or not data.places.has(training.place_id) or not state.actors.has(training.student_id): return _fail("UnknownTrainingReference")
	if not _whole(action.get("start_tick"),0,state.tick) or not _whole(action.get("remaining_ticks"),0 if finishing else 1,training.duration): return _fail("InvalidTrainingTime")
	var fact: Variant=state.get("facts",{}).get(action.id)
	if not fact is Dictionary or fact.get("kind")!="training" or fact.get("action_kind")!=action.kind or fact.get("actor_id")!=action.actor_id or fact.get("start_tick")!=action.start_tick or fact.get("training")!=training or fact.get("context",{})!=action.params.get("context",{}): return _fail("TrainingFactMismatch")
	if fact.get("phase")=="completed":
		if not finishing or not fact.get("success") is bool or not _whole(fact.get("completed_tick"),action.start_tick+training.duration,state.tick+1): return _fail("TrainingAlreadyCompleted")
		return {"ok":true}
	if fact.get("phase")!="started" or fact.get("completed_tick")!=-1 or fact.get("success")!=false: return _fail("InvalidTrainingFact")
	if not finishing and action.start_tick+training.duration-action.remaining_ticks!=state.tick: return _fail("TrainingClockMismatch")
	var expected: Array=[action.actor_id]
	if action.kind=="practice_skill":
		if action.actor_id!=training.student_id or not training.teacher_id.is_empty() or action.params.size()!=1 or training.duration>8 or training.mastery_before<0: return _fail("InvalidPracticeTerms")
		var gain: int=_practice_gain(data.personal_skills[training.skill_id].practice_ticks,training.duration)
		if training.mastery_after not in [mini(100,training.mastery_before+gain),mini(100,training.mastery_before+maxi(1,gain/2))] or training.student_fatigue!=training.duration*6 or training.teacher_fatigue!=0: return _fail("InvalidPracticeGrowth")
	else:
		var context: Variant=action.params.get("context")
		if action.params.size()!=2 or not context is Dictionary or not context.get("role_id") is String or not data.roles.has(context.role_id) or context.get("place_id")!=training.place_id or context.get("kind","")=="encounter": return _fail("InvalidTrainingContext")
		var provider: String=training.teacher_id if action.kind=="learn_skill" else training.student_id
		if context.get("provider_actor_id")!=provider or not state.actors.has(training.teacher_id) or training.teacher_id==training.student_id: return _fail("InvalidTrainingTeacher")
		expected.append(provider)
		if training.student_fatigue!=training.duration*4 or training.teacher_fatigue!=training.duration*2: return _fail("InvalidTrainingFatigue")
		if action.kind=="learn_skill":
			if action.actor_id!=training.student_id or training.mastery_before!=-1 or training.mastery_after!=0: return _fail("InvalidLessonGrowth")
			if not state.wallets.has(training.payer_wallet_id) or not state.wallets.has(training.recipient_wallet_id) or training.payer_wallet_id!=state.actors[action.actor_id].wallet_id or training.payer_wallet_id==training.recipient_wallet_id or state.wallets[training.payer_wallet_id].owner!={"kind":"actor","id":action.actor_id}: return _fail("InvalidLessonPayment")
			var role: Dictionary=data.roles[context.role_id]
			var owner: Dictionary={"kind":"actor","id":training.teacher_id} if role.organization_id==null else {"kind":"organization","id":role.organization_id}
			if state.wallets[training.recipient_wallet_id].owner!=owner: return _fail("InvalidLessonPayment")
			var offered: bool=false
			for lesson in role.get("lessons",[]):
				if lesson.skill_id==training.skill_id and lesson.fee==training.fee and lesson.study_ticks==training.duration and lesson.wallet_id==training.recipient_wallet_id: offered=true
			if not offered: return _fail("LessonTermsMismatch")
		else:
			if action.actor_id!=training.teacher_id or training.duration!=2 or training.mastery_after!=mini(state.actors[training.teacher_id].combat.learned.get(training.skill_id,-1),maxi(0,training.mastery_before)+10): return _fail("InvalidTeachingGrowth")
	if action.kind!="learn_skill" and (training.fee!=0 or not training.payer_wallet_id.is_empty() or not training.recipient_wallet_id.is_empty()): return _fail("UnexpectedTrainingPayment")
	if action.get("participants")!=expected: return _fail("InvalidTrainingParticipants")
	for person in expected:
		if not state.actors.has(person) or state.actors[person].action_id!=action.id: return _fail("TrainingOccupationMissing")
	if state.actors[training.student_id].combat.learned.get(training.skill_id,-1)!=training.mastery_before: return _fail("TrainingGrowthChanged")
	return {"ok":true}

## 读取人物基本可行动条件，并排除尚未关闭的个人战占用。
static func _ready(state: Dictionary, actor_id: String, own_action: String) -> Dictionary:
	var actor: Variant=state.get("actors",{}).get(actor_id)
	if not actor is Dictionary or not actor.get("alive",false) or not actor.get("available",false) or actor.get("hp",0)<=0: return _fail("ActorUnavailable")
	if not actor.get("combat") is Dictionary or not actor.combat.get("learned") is Dictionary: return _fail("MissingPersonalGrowth")
	if not actor.get("action_id","").is_empty() and actor.action_id!=own_action: return _fail("ActorBusy")
	for battle in state.get("battles",{}).values():
		if battle.get("phase")!="closed" and battle.get("units",{}).has(actor_id): return _fail("BattlePending")
	return {"ok":true}

## 教師依据自己对学习者的已知关系作决定，不读取反向好感冒充信任。
static func _trust(state: Dictionary, teacher_id: String, student_id: String) -> int:
	for edge in state.get("relations",{}).values():
		if edge.from_actor==teacher_id and edge.to_actor==student_id: return edge.trust
	return 0

## 按作者指定修炼周期计算每半日增长；整数向上取整，多个半日仍需真正经过。
static func _practice_gain(practice_ticks: int, ticks: int) -> int:
	return ((100+practice_ticks-1)/practice_ticks)*ticks

## 严格检查规则整数，拒绝浮点、布尔或越界存档条款。
static func _whole(value: Variant, minimum: int, maximum: int) -> bool:
	return value is int and value>=minimum and value<=maximum

## 返回稳定领域错误，不把未满足的学习或资产条件处理为成功。
static func _fail(code: String) -> Dictionary:
	return {"ok":false,"code":code}
