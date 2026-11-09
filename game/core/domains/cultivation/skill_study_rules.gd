## 普通技艺课程只使用人物既有skills经验；报价、课时与永久事实同属cultivation，不复制武学掌握账。
extends RefCounted
const Segments=preload("res://core/domains/cultivation/skill_study_segments.gd")
const Pause=preload("res://core/domains/cultivation/skill_study_pause.gd")
const Values=preload("res://core/base/value_checks.gd")
const Interaction=preload("res://core/interaction_rules.gd")
const Inventory=preload("res://core/inventory_rules.gd")
const Combat=preload("res://core/combat_session.gd")
const Etiquette=preload("res://core/domains/cultivation/study_etiquette.gd")
const Levels=preload("res://core/domains/cultivation/skill_levels.gd")
const SKILLS=Levels.SKILLS
const THRESHOLDS=Levels.THRESHOLDS
const PHYSICAL=["infantry","cavalry","archery","martial"]
const KIND="study_skill"
const CANCEL_KIND="study_cancel"
const PAUSE_KIND="study_pause"
const RESUME_KIND="study_resume"
const CONTROL_KINDS=[PAUSE_KIND,RESUME_KIND]
const FACT_KIND="skill_study"

## 等级由唯一累计经验派生；不保存可与经验矛盾的第二等级字段。
static func level(xp: int) -> int:
	return Levels.level(xp)

## 自动修业采用正常基线，疲劳和伤势取较低效率，不接受调用方自报小游戏倍率。
static func lesson_xp(fatigue: int, injury: int) -> int:
	var fatigue_bp: int=10000 if fatigue<40 else 9000 if fatigue<70 else 7500 if fatigue<90 else 6000
	@warning_ignore("integer_division")
	return 20*mini(fatigue_bp,[10000,9000,7500,6000][injury])/10000

## 明确拒绝不支持的参数、课程、越权钱包与不在场教师；读取不会消费课时或费用。
static func preview(state: Dictionary, data: Dictionary, actor_id: String, params: Dictionary, own_action: String="") -> Dictionary:
	if not Values.fields(params,{"skill_id":TYPE_STRING,"ticks":TYPE_INT,"context":TYPE_DICTIONARY}) or params.size()!=(4 if params.has("challenge_mode") else 3) or (params.has("challenge_mode") and not params.challenge_mode is String) or not params.skill_id in SKILLS or not Values.whole(params.ticks,2,6): return _fail("InvalidStudyRequest")
	if params.context.size()!=3: return _fail("InvalidStudyContext")
	var ready: Dictionary=_ready(state,actor_id,params.skill_id,own_action)
	if not ready.ok: return ready
	var contact: Dictionary=Interaction.validate(state,data,actor_id,params.context,KIND,own_action)
	if not contact.ok: return contact
	for pending in state.get("actions",{}).values():
		if pending.kind==KIND and pending.actor_id==actor_id and pending.id!=own_action: return _fail("StudyAlreadyPending")
	var teacher_id: String=contact.context.provider_actor_id
	ready=_ready(state,teacher_id,params.skill_id,own_action)
	if not ready.ok: return ready
	var course: Dictionary=_course(data,contact.context.role_id,params.skill_id)
	if course.is_empty(): return _fail("StudyCourseNotOffered")
	var actor: Dictionary=state.actors[actor_id]
	var teacher: Dictionary=state.actors[teacher_id]
	var xp: int=actor.skills[params.skill_id]
	var teacher_xp: int=teacher.skills[params.skill_id]
	var target_level: int=level(xp)+1
	if target_level>4: return _fail("StudyAlreadyMastered")
	var limit: int=300
	if course.mode=="teacher":
		if level(teacher_xp)<=level(xp): return _fail("StudyTeacherNotQualified")
		limit=THRESHOLDS[level(teacher_xp)]
	if xp>=limit: return _fail("StudyCourseLevelLimit")
	var payer_id: String=actor.wallet_id
	var recipient_id: String=course.wallet_id
	var role: Dictionary=data.roles[contact.context.role_id]
	var owner: Dictionary={"kind":"actor","id":teacher_id} if role.organization_id==null else {"kind":"organization","id":role.organization_id}
	if not state.wallets.has(payer_id) or state.wallets[payer_id].owner!={"kind":"actor","id":actor_id}: return _fail("PersonalPaymentRequired")
	if not state.wallets.has(recipient_id) or state.wallets[recipient_id].owner!=owner: return _fail("StudyPaymentAuthority")
	if payer_id==recipient_id: return _fail("SelfPayment")
	var fee: int=(20+10*target_level)*params.ticks
	if Inventory.spendable(state.wallets[payer_id])<fee: return _fail("InsufficientFunds")
	if state.wallets[recipient_id].balance>2147483647-fee: return _fail("MoneyOverflow")
	var terms: Dictionary={"skill_id":params.skill_id,"mode":course.mode,"student_id":actor_id,"teacher_id":teacher_id,"context":contact.context.duplicate(true),"duration":params.ticks,"xp_before":xp,"teacher_xp":teacher_xp,"student_fatigue":actor.fatigue,"teacher_fatigue":teacher.fatigue,"xp_limit":limit,"fee":fee,"payer_wallet_id":payer_id,"recipient_wallet_id":recipient_id}
	if course.has("settlement_policy"): terms.settlement_policy=course.settlement_policy
	var challenge: Dictionary=Etiquette.quote(data,course,actor,params.get("challenge_mode","automatic"))
	if not challenge.ok: return challenge
	if challenge.has("etiquette"): terms.etiquette=challenge.etiquette
	if challenge.has("challenge"): terms.challenge=challenge.challenge
	var estimate: int=0
	for index in range(params.ticks): estimate+=lesson_xp(mini(100,actor.fatigue+6*index),actor.get("injury",0))
	return {"ok":true,"duration":params.ticks,"cost":fee,"study":terms,"context":contact.context.duplicate(true),"expected_xp":mini(limit,xp+estimate)-xp,"target_level":target_level,"payment_policy":"reserve_then_pay_per_lesson","cancel_supported":true}

## 只查询当前真实可接触岗位的声明课程；本人进度可见，教师精确经验和内部结算条款不外传。
## 每项资格及报价复用preview，返回新副本；查询既不远程开课，也不预留费用或推进时间。
static func courses(state: Dictionary, data: Dictionary, actor_id: String, context: Dictionary, ticks: int=2) -> Dictionary:
	if not Values.whole(ticks,2,6) or context.size()!=3: return _fail("InvalidStudyRequest")
	if not "cultivation" in data.get("required_capabilities",[]): return _fail("CultivationUnavailable")
	var contact: Dictionary=Interaction.validate(state,data,actor_id,context,KIND)
	if not contact.ok: return contact
	var role: Dictionary=data.roles[contact.context.role_id]
	var declared: Array=[]
	for course in role.get("skill_courses",[]):
		if not course is Dictionary or not course.get("skill_id") in SKILLS or course.skill_id in declared or _course(data,contact.context.role_id,course.skill_id).is_empty(): return _fail("InvalidStudyCourse")
		declared.append(course.skill_id)
	var rows: Array=[]
	for skill_id in SKILLS:
		if not skill_id in declared: continue
		var xp: Variant=state.actors[actor_id].get("skills",{}).get(skill_id)
		if not Values.whole(xp,0,1200): return _fail("InvalidStudyActor")
		var current_level: int=level(xp)
		var parameters: Dictionary={"skill_id":skill_id,"ticks":ticks,"context":contact.context.duplicate(true)}
		var quoted: Dictionary=preview(state,data,actor_id,parameters)
		var course: Dictionary=_course(data,contact.context.role_id,skill_id)
		var row: Dictionary={"skill_id":skill_id,"mode":course.mode,"xp":xp,"level":current_level,"next_threshold":THRESHOLDS[current_level+1] if current_level<4 else null,"eligible":quoted.ok,"code":quoted.get("code",""),"params":parameters}
		if quoted.ok:
			row.offer={"duration":quoted.duration,"cost":quoted.cost,"expected_xp":quoted.expected_xp,"target_level":quoted.target_level,"xp_limit":quoted.study.xp_limit,"payment_policy":quoted.payment_policy,"cancel_supported":quoted.cancel_supported}
			if quoted.study.has("settlement_policy"): row.offer.settlement_policy=quoted.study.settlement_policy
			if quoted.study.has("etiquette"): row.offer.etiquette={"challenge_id":quoted.study.etiquette.challenge_id,"modes":["automatic","manual"]}
			if quoted.study.has("challenge"): row.offer.challenge={"kind":quoted.study.challenge.kind,"challenge_id":quoted.study.challenge.challenge_id,"modes":["automatic","manual"],"context":quoted.study.challenge.context.duplicate(true) if quoted.study.challenge.context is Dictionary else null}
		rows.append(row)
	return {"ok":true,"actor_id":actor_id,"context":contact.context.duplicate(true),"ticks":ticks,"courses":rows}

## 主动中断只接受本人当前课程的精确行动ID；公共命令revision仍由Kernel守卫，旧课程不能取消新课。
static func preview_cancel(state: Dictionary, data: Dictionary, actor_id: String, params: Dictionary) -> Dictionary:
	if params.size()!=1 or not Values.fields(params,{"action_id":TYPE_STRING}): return _fail("InvalidStudyCancellation")
	var actor: Variant=state.get("actors",{}).get(actor_id)
	if not actor is Dictionary: return _fail("UnknownActor")
	if not actor.get("alive",false) or not actor.get("available",false): return _fail("ActorUnavailable")
	var action: Dictionary=state.get("actions",{}).get(actor.get("action_id",""),{})
	var requested: Dictionary=state.get("actions",{}).get(params.action_id,{})
	if requested.get("kind")==KIND and requested.get("actor_id")==actor_id and Pause.paused(state.facts[requested.id]): action=requested
	if action.get("kind")!=KIND or action.get("actor_id")!=actor_id: return _fail("NotStudying")
	if params.action_id!=action.id: return _fail("StaleStudyAction")
	var valid: Dictionary=validate_action(state,data,action)
	if not valid.ok: return valid
	var fact: Dictionary=state.facts[action.id]
	var paid: int=_lesson_cost(fact.terms)*fact.lessons.size()
	return {"ok":true,"duration":0,"action_id":action.id,"completed_lessons":fact.lessons.size(),"fee_paid":paid,"unprovided_released":fact.terms.fee-paid}

## 在当前稳定半日取消，只结算已提供课时的成长，不补课或重付钱；共用协调随后释放预留和双方。
static func cancel(state: Dictionary, data: Dictionary, actor_id: String, action_id: String) -> Dictionary:
	var checked: Dictionary=preview_cancel(state,data,actor_id,{"action_id":action_id})
	if not checked.ok: return checked
	var fact: Dictionary=state.facts[action_id]
	if Pause.paused(fact): Pause.close(state,fact)
	var closed: Dictionary=_settle_growth(state,data,fact)
	if not closed.ok: return closed
	fact.phase="completed"; fact.completed_tick=state.tick; fact.success=false; fact.code="StudyCancelled"
	var receipt: Dictionary=_receipt(fact)
	receipt.action_id=action_id
	return {"ok":true,"details":receipt}

## 显式暂停和恢复只认本人原课程；恢复重新校验在场、资格和双方占用，不重报价或更换教师。
static func preview_control(state: Dictionary, data: Dictionary, actor_id: String, kind: String, params: Dictionary) -> Dictionary:
	if not kind in CONTROL_KINDS or params.size()!=1 or not Values.fields(params,{"action_id":TYPE_STRING}): return _fail("InvalidStudyControl")
	var actor: Variant=state.get("actors",{}).get(actor_id)
	if not actor is Dictionary: return _fail("UnknownActor")
	if not actor.get("alive",false) or not actor.get("available",false): return _fail("ActorUnavailable")
	var action: Dictionary=state.get("actions",{}).get(params.action_id,{})
	if action.get("kind")!=KIND or action.get("actor_id")!=actor_id: return _fail("StaleStudyAction")
	var valid: Dictionary=validate_action(state,data,action)
	if not valid.ok: return valid
	var fact: Dictionary=state.facts[action.id]
	if not Pause.supported(fact.terms): return _fail("StudyPauseUnsupported")
	var paused: bool=Pause.paused(fact)
	if kind==PAUSE_KIND and paused: return _fail("StudyAlreadyPaused")
	if kind==RESUME_KIND and not paused: return _fail("StudyNotPaused")
	if kind==RESUME_KIND:
		for person in [fact.terms.student_id,fact.terms.teacher_id]:
			var ready: Dictionary=_ready(state,person,fact.terms.skill_id,"")
			if not ready.ok: return ready
		var contact: Dictionary=Interaction.validate(state,data,actor_id,fact.terms.context,KIND)
		if not contact.ok: return contact
		var current_xp: int=state.actors[actor_id].skills[fact.terms.skill_id]
		if current_xp>=fact.terms.xp_limit: return _fail("StudyResumeLevelLimit")
		if fact.terms.mode=="teacher" and level(state.actors[fact.terms.teacher_id].skills[fact.terms.skill_id])<=level(current_xp): return _fail("StudyTeacherNotQualified")
		if state.wallets[fact.terms.recipient_wallet_id].balance>2147483647-_lesson_cost(fact.terms): return _fail("StudyPaymentBlocked")
	return {"ok":true,"duration":0,"action_id":action.id,"paused":kind==PAUSE_KIND,"paused_ticks":Pause.elapsed(fact,state.tick),"completed_lessons":fact.lessons.size(),"reserved":state.wallets[fact.terms.payer_wallet_id].reserved[action.id]}

## 只转移同一行动的人物占用并记稳定边界；未提供资金、题面和终态经验结算保持不动。
static func apply_control(state: Dictionary, data: Dictionary, actor_id: String, kind: String, params: Dictionary, offer: Dictionary) -> Dictionary:
	var checked: Dictionary=preview_control(state,data,actor_id,kind,params)
	if not checked.ok: return checked
	if checked!=offer: return _fail("StaleStudyQuote")
	var action: Dictionary=state.actions[params.action_id]
	var fact: Dictionary=state.facts[action.id]
	if kind==PAUSE_KIND:
		if not fact.has("pauses"): fact.pauses=[]
		fact.pauses.append({"start_tick":state.tick,"end_tick":-1,"after_lessons":fact.lessons.size(),"student_fatigue":-1,"teacher_fatigue":-1,"student_xp":-1})
		for person in action.participants: state.actors[person].action_id=""
		action.participants=[]
	else:
		Pause.close(state,fact)
		action.participants=[fact.terms.student_id,fact.terms.teacher_id]
		for person in action.participants: state.actors[person].action_id=action.id
	var details: Dictionary=checked.duplicate(true); details.erase("ok"); details.erase("duration")
	return {"ok":true,"details":details}

## 两种题面的选择都作用于本人当前课程；类型、行动和当前步次始终由同一事实核验。
static func preview_challenge(state: Dictionary, data: Dictionary, actor_id: String, kind: String, params: Dictionary) -> Dictionary:
	if not params.get("action_id") is String: return _fail("InvalidStudyChallengeRequest")
	var active: Dictionary=_challenge_action(state,data,actor_id,params.action_id)
	if not active.ok: return active
	var fact: Dictionary=state.facts[params.action_id]
	var next: Dictionary=Etiquette.transition(data,fact,params.action_id,kind,params)
	if not next.ok: return next
	return {"ok":true,"duration":0,"action_id":params.action_id,"challenge_before":Etiquette.instance(fact).challenge.duplicate(true),"challenge_next":next.state}

## 私有报价复核后写原实例；新政策晚答只补已教课时差额，零课时不会获奖。
static func apply_challenge(state: Dictionary, data: Dictionary, actor_id: String, kind: String, params: Dictionary, offer: Dictionary) -> Dictionary:
	var current: Dictionary=preview_challenge(state,data,actor_id,kind,params)
	if not current.ok: return current
	if current!=offer: return _fail("StaleStudyChallengeQuote")
	var fact: Dictionary=state.facts[current.action_id]
	var instance: Dictionary=Etiquette.instance(fact)
	instance.challenge=current.challenge_next.duplicate(true)
	Segments.adjust(state,fact)
	var details: Dictionary={"action_id":current.action_id,"challenge":Etiquette.view(data,fact,current.action_id)}
	if kind in [Etiquette.GENERIC_CHOOSE_KIND,Etiquette.GENERIC_EXIT_KIND]:
		details.challenge_kind=Etiquette.kind(fact); details.price_applied=false
	return {"ok":true,"details":details}

## 通用查询明确种类且仅返回本人当前公开题面，不把议价课程展示为已经改价的交易。
static func challenge_view(state: Dictionary, data: Dictionary, actor_id: String, action_id: String) -> Dictionary:
	var active: Dictionary=_challenge_action(state,data,actor_id,action_id)
	if not active.ok: return active
	var result: Dictionary=Etiquette.view(data,state.facts[action_id],action_id)
	if result.ok:
		result.challenge_kind=Etiquette.kind(state.facts[action_id])
		result.price_applied=false
	return result

## 精确当前行动及原课程验证供两个即时入口与只读查询共用，不扩大普通ActorBusy豁免。
static func _challenge_action(state: Dictionary, data: Dictionary, actor_id: String, action_id: String) -> Dictionary:
	var actor: Variant=state.get("actors",{}).get(actor_id)
	if not actor is Dictionary or not actor.get("alive",false) or not actor.get("available",false): return _fail("ActorUnavailable")
	var action: Dictionary=state.get("actions",{}).get(actor.get("action_id",""),{})
	if action.get("kind")!=KIND or action.get("actor_id")!=actor_id: return _fail("NotStudying")
	if action.id!=action_id: return _fail("StaleStudyAction")
	return validate_action(state,data,action)

## 旧礼议预览仍只认原命令与参数，内部委托同一课程实例，不静默改名旧存档请求。
static func preview_etiquette(state: Dictionary, data: Dictionary, actor_id: String, kind: String, params: Dictionary) -> Dictionary:
	if not kind in [Etiquette.CHOOSE_KIND,Etiquette.EXIT_KIND]: return _fail("UnsupportedAction")
	return preview_challenge(state,data,actor_id,kind,params)

## 旧礼议提交入口保留原响应形状，不能把旧命令借给议价实例。
static func apply_etiquette(state: Dictionary, data: Dictionary, actor_id: String, kind: String, params: Dictionary, offer: Dictionary) -> Dictionary:
	if not kind in [Etiquette.CHOOSE_KIND,Etiquette.EXIT_KIND]: return _fail("UnsupportedAction")
	return apply_challenge(state,data,actor_id,kind,params,offer)

## 原查询只展示礼议；议价使用明确通用查询，旧调用方不会误认话题结构。
static func etiquette_view(state: Dictionary, data: Dictionary, actor_id: String, action_id: String) -> Dictionary:
	var active: Dictionary=_challenge_action(state,data,actor_id,action_id)
	if not active.ok: return active
	if Etiquette.kind(state.facts[action_id])!="etiquette": return _fail("StudyChallengeNotOffered")
	return Etiquette.view(data,state.facts[action_id],action_id)

## 占用建立后重新核对私有报价，只预留完整课程费用，未提供课时尚不转入教师钱包。
static func begin(state: Dictionary, data: Dictionary, action: Dictionary, offer: Dictionary) -> Dictionary:
	if action.get("kind")!=KIND or not action.get("id") is String or not action.get("actor_id") is String or not action.get("params") is Dictionary: return _fail("InvalidStudyAction")
	if not state.get("facts") is Dictionary: return _fail("MissingStudyFacts")
	if state.facts.has(action.id): return _fail("StudyAlreadyStarted")
	var current: Dictionary=preview(state,data,action.actor_id,action.params,action.id)
	if not current.ok: return current
	if current.get("study")!=offer.get("study"): return _fail("StaleStudyQuote")
	var terms: Dictionary=current.study
	if action.get("participants")!=[terms.student_id,terms.teacher_id] or action.get("start_tick")!=state.tick or action.get("remaining_ticks")!=terms.duration: return _fail("InvalidStudyEnvelope")
	for person in action.participants:
		if state.actors[person].action_id!=action.id: return _fail("StudyOccupationMissing")
	state.wallets[terms.payer_wallet_id].reserved[action.id]=terms.fee
	action.params={"study":terms.duplicate(true)}
	state.facts[action.id]={"kind":FACT_KIND,"phase":"started","actor_id":action.actor_id,"start_tick":state.tick,"completed_tick":-1,"success":false,"terms":terms.duplicate(true),"lessons":[],"code":""}
	return Etiquette.start(state,data,action,state.facts[action.id])

## 每个真实半日结算一课费用、经验收据与疲劳；环境失效结束课程并由共用释放器解除未用预留。
static func step(state: Dictionary, data: Dictionary, action: Dictionary) -> Dictionary:
	var valid: Dictionary=validate_action(state,data,action)
	if not valid.ok: return valid
	var fact: Dictionary=state.facts[action.id]
	var terms: Dictionary=fact.terms
	if Pause.paused(fact): return {"ok":true,"completed":false,"suspended":false}
	var present: Dictionary=_continuation(state,data,action)
	if not present.ok:
		fact.code=present.code
		action.remaining_ticks=0
		return {"ok":true,"completed":true,"suspended":false}
	var student: Dictionary=state.actors[terms.student_id]
	var teacher: Dictionary=state.actors[terms.teacher_id]
	var cost: int=_lesson_cost(terms)
	state.wallets[terms.payer_wallet_id].balance-=cost
	state.wallets[terms.payer_wallet_id].reserved[action.id]-=cost
	state.wallets[terms.recipient_wallet_id].balance+=cost
	var lesson: Dictionary={"fatigue_before":student.fatigue,"injury":student.get("injury",0),"xp":lesson_xp(student.fatigue,student.get("injury",0))}
	if Segments.enabled(fact): Segments.teach(state,fact,lesson)
	fact.lessons.append(lesson)
	student.fatigue=mini(100,student.fatigue+6)
	teacher.fatigue=mini(100,teacher.fatigue+2)
	action.remaining_ticks-=1
	return {"ok":true,"completed":action.remaining_ticks==0,"suspended":false}

## 完课或来源失效都只结算已提供课时一次；失败不冒充完成课程，由共用生命周期释放余款和双方。
static func finish(state: Dictionary, data: Dictionary, action: Dictionary, finish_tick: int) -> Dictionary:
	var valid: Dictionary=_action_fact(state,data,action)
	if not valid.ok: return valid
	var fact: Dictionary=state.facts[action.id]
	if fact.phase=="completed": return _receipt(fact)
	var terms: Dictionary=fact.terms
	if action.remaining_ticks!=0 or finish_tick!=state.tick+1: return _fail("StudyNotReady")
	var success: bool=fact.code.is_empty()
	if success and (fact.lessons.size()!=terms.duration or finish_tick!=fact.start_tick+terms.duration+Pause.elapsed(fact,state.tick)): return _fail("StudyNotReady")
	if not success and finish_tick!=fact.start_tick+fact.lessons.size()+1+Pause.elapsed(fact,state.tick): return _fail("InvalidStudyInterruption")
	var closed: Dictionary=_settle_growth(state,data,fact)
	if not closed.ok: return closed
	fact.phase="completed"; fact.completed_tick=finish_tick; fact.success=success
	return _receipt(fact)

## 终态只封存新政策课时；旧政策保持末尾成长，暂停外部经验始终保全。
static func _settle_growth(state: Dictionary, data: Dictionary, fact: Dictionary) -> Dictionary:
	var terms: Dictionary=fact.terms
	var current_xp: int=state.actors[terms.student_id].skills[terms.skill_id]
	if current_xp!=Pause.xp_baseline(fact): return _fail("StudyGrowthChanged")
	var closed: Dictionary=Etiquette.close(data,fact)
	if not closed.ok: return closed
	if Segments.enabled(fact): return {"ok":true}
	if fact.has("pauses"): fact.xp_settlement_before=current_xp
	state.actors[terms.student_id].skills[terms.skill_id]=maxi(current_xp,mini(terms.xp_limit,current_xp+_earned(fact)))
	return {"ok":true}

## 活动载荷与永久事实相互对应；读档只核对已接受政策，不重收费用或补发历史课时。
static func validate_action(state: Dictionary, data: Dictionary, action: Dictionary) -> Dictionary:
	var valid: Dictionary=_action_fact(state,data,action)
	if not valid.ok: return valid
	var fact: Dictionary=state.facts[action.id]
	var terms: Dictionary=fact.terms
	if fact.phase!="started" or not fact.code.is_empty() or action.remaining_ticks!=terms.duration-fact.lessons.size() or not Values.whole(action.remaining_ticks,1,terms.duration) or state.tick!=fact.start_tick+fact.lessons.size()+Pause.elapsed(fact,state.tick): return _fail("InvalidStudyProgress")
	var current_xp: int=state.actors[terms.student_id].skills[terms.skill_id]
	var baseline: int=Pause.xp_baseline(fact)
	if current_xp<baseline or (not Pause.paused(fact) and current_xp!=baseline): return _fail("StudyGrowthChanged")
	if state.wallets[terms.payer_wallet_id].reserved.get(action.id)!=terms.fee-_lesson_cost(terms)*fact.lessons.size(): return _fail("StudyReservationMismatch")
	if Pause.paused(fact):
		for person in [terms.student_id,terms.teacher_id]:
			if state.actors[person].action_id==action.id: return _fail("StudyOccupationMissing")
	for person in action.participants:
		if state.actors[person].action_id!=action.id: return _fail("StudyOccupationMissing")
	return {"ok":true}

## 检查本域所有开课和完成事实；拒绝孤立活动事实、重复经验或伪造整数倍率。
static func validate_state(state: Dictionary, data: Dictionary) -> Dictionary:
	var pending_students: Array=[]
	for id in state.get("facts",{}):
		var fact: Variant=state.facts[id]
		if not fact is Dictionary or fact.get("kind")!=FACT_KIND: continue
		var valid: Dictionary=_fact(state,data,fact,id)
		if not valid.ok: return valid
		if fact.phase=="started":
			if fact.actor_id in pending_students: return _fail("InvalidStudyProgress")
			pending_students.append(fact.actor_id)
			if not state.actions.has(id): return _fail("MissingStudyAction")
			valid=validate_action(state,data,state.actions[id])
			if not valid.ok: return valid
		elif state.actions.has(id) or fact.completed_tick>state.tick or state.wallets[fact.terms.payer_wallet_id].reserved.has(id): return _fail("InvalidStudyCompletion")
	return {"ok":true}

## 从真实作者岗位取唯一课程；不将任意在场人物或武学课程解释为普通技艺授课权。
static func _course(data: Dictionary, role_id: String, skill_id: String) -> Dictionary:
	var found: Dictionary={}
	for course in data.get("roles",{}).get(role_id,{}).get("skill_courses",[]):
		if not course is Dictionary or course.get("skill_id")!=skill_id: continue
		if not found.is_empty() or course.size()!=(4 if course.has("etiquette_challenge_id") or course.has("challenge") else 3)+(1 if course.has("settlement_policy") else 0) or (course.has("settlement_policy") and course.settlement_policy!=Segments.POLICY) or not Etiquette.course_valid(course) or not Values.fields(course,{"skill_id":TYPE_STRING,"mode":TYPE_STRING,"wallet_id":TYPE_STRING}) or not course.mode in ["facility","teacher"]: return {}
		found=course
	return found

## 检查普通学习的真人状态；重伤禁体能训练，但保留文事课程，不借战斗快照代替人物。
static func _ready(state: Dictionary, actor_id: String, skill_id: String, own_action: String) -> Dictionary:
	var actor: Variant=state.get("actors",{}).get(actor_id)
	if not actor is Dictionary or not actor.get("alive",false) or not actor.get("available",false) or actor.get("hp",0)<=0: return _fail("ActorUnavailable")
	if not actor.get("skills") is Dictionary or not Values.whole(actor.skills.get(skill_id),0,1200) or not Values.whole(actor.get("fatigue"),0,100) or not Values.whole(actor.get("injury",0),0,3): return _fail("InvalidStudyActor")
	if actor.get("injury",0)==3 and skill_id in PHYSICAL: return _fail("StudyNeedsRecovery")
	if not actor.get("action_id","").is_empty() and actor.action_id!=own_action: return _fail("ActorBusy")
	if Combat.occupied(state,actor_id): return _fail("BattlePending")
	return {"ok":true}

## 各半日重新确认岗位、地点、资格及独占进度；失效结果供正常失败结课而非事务报错。
static func _continuation(state: Dictionary, data: Dictionary, action: Dictionary) -> Dictionary:
	var fact: Dictionary=state.facts[action.id]
	var terms: Dictionary=fact.terms
	for person in action.participants:
		var ready: Dictionary=_ready(state,person,terms.skill_id,action.id)
		if not ready.ok: return ready
	var contact: Dictionary=Interaction.validate(state,data,terms.student_id,terms.context,KIND,action.id)
	if not contact.ok: return contact
	if not fact.has("pauses") and state.actors[terms.teacher_id].skills[terms.skill_id]!=terms.teacher_xp: return _fail("StudyTeacherChanged")
	if fact.has("pauses") and terms.mode=="teacher" and level(state.actors[terms.teacher_id].skills[terms.skill_id])<=level(Pause.resume_xp(fact)): return _fail("StudyTeacherNotQualified")
	if state.wallets[terms.recipient_wallet_id].balance>2147483647-_lesson_cost(terms): return _fail("StudyPaymentBlocked")
	if state.actors[terms.student_id].fatigue!=Pause.fatigue(fact,fact.lessons.size()) or state.actors[terms.teacher_id].fatigue!=Pause.fatigue(fact,fact.lessons.size(),true): return _fail("StudyConditionChanged")
	return {"ok":true}

## 固定条款和半日证据足以独立重算上限、费用和奖励，未知字段与非整数拒绝。
static func _fact(state: Dictionary, data: Dictionary, fact: Dictionary, action_id: String) -> Dictionary:
	if fact.size()!=(10 if fact.has("etiquette") or fact.has("challenge") else 9)+(1 if fact.has("pauses") else 0)+(1 if fact.has("xp_settlement_before") else 0) or not Values.fields(fact,{"kind":TYPE_STRING,"phase":TYPE_STRING,"actor_id":TYPE_STRING,"start_tick":TYPE_INT,"completed_tick":TYPE_INT,"success":TYPE_BOOL,"terms":TYPE_DICTIONARY,"lessons":TYPE_ARRAY,"code":TYPE_STRING}) or fact.kind!=FACT_KIND: return _fail("InvalidStudyFact")
	var t: Dictionary=fact.terms
	if t.size()!=(15 if t.has("etiquette") or t.has("challenge") else 14)+(1 if t.has("settlement_policy") else 0) or not Values.fields(t,{"skill_id":TYPE_STRING,"mode":TYPE_STRING,"student_id":TYPE_STRING,"teacher_id":TYPE_STRING,"context":TYPE_DICTIONARY,"duration":TYPE_INT,"xp_before":TYPE_INT,"teacher_xp":TYPE_INT,"student_fatigue":TYPE_INT,"teacher_fatigue":TYPE_INT,"xp_limit":TYPE_INT,"fee":TYPE_INT,"payer_wallet_id":TYPE_STRING,"recipient_wallet_id":TYPE_STRING}): return _fail("InvalidStudyTerms")
	var segmented: bool=Segments.enabled(fact)
	if t.has("settlement_policy") and not segmented: return _fail("InvalidStudySettlementPolicy")
	if not t.skill_id in SKILLS or not t.mode in ["facility","teacher"] or not state.actors.has(t.student_id) or not state.actors.has(t.teacher_id) or t.student_id==t.teacher_id or fact.actor_id!=t.student_id or not Values.whole(t.duration,2,6) or not Values.whole(t.xp_before,0,1199) or not Values.whole(t.teacher_xp,0,1200) or not Values.whole(t.student_fatigue,0,100) or not Values.whole(t.teacher_fatigue,0,100): return _fail("InvalidStudyTerms")
	if t.context.size()!=3 or not Values.fields(t.context,{"place_id":TYPE_STRING,"provider_actor_id":TYPE_STRING,"role_id":TYPE_STRING}) or t.context.provider_actor_id!=t.teacher_id or not data.get("roles",{}).has(t.context.role_id): return _fail("InvalidStudyContext")
	var role: Dictionary=data.roles[t.context.role_id]
	var course: Dictionary=_course(data,t.context.role_id,t.skill_id)
	if course.is_empty() or course.mode!=t.mode or course.wallet_id!=t.recipient_wallet_id or role.place_id!=t.context.place_id or not KIND in role.services: return _fail("StudyTermsMismatch")
	if t.get("settlement_policy")!=course.get("settlement_policy"): return _fail("StudySettlementPolicyMismatch")
	if not Etiquette.matches_course(course,fact): return _fail("StudyChallengeTermsMismatch")
	var challenge_valid: Dictionary=Etiquette.validate(data,fact,action_id)
	if not challenge_valid.ok: return challenge_valid
	var limit: int=300 if t.mode=="facility" else THRESHOLDS[level(t.teacher_xp)]
	if t.xp_limit!=limit or t.xp_before>=limit or (t.mode=="teacher" and level(t.teacher_xp)<=level(t.xp_before)) or t.fee!=(20+10*(level(t.xp_before)+1))*t.duration: return _fail("InvalidStudyGrowth")
	if t.payer_wallet_id==t.recipient_wallet_id or not state.wallets.has(t.payer_wallet_id) or not state.wallets.has(t.recipient_wallet_id) or state.actors[t.student_id].wallet_id!=t.payer_wallet_id: return _fail("InvalidStudyPayment")
	var recipient_owner: Dictionary={"kind":"actor","id":t.teacher_id} if role.organization_id==null else {"kind":"organization","id":role.organization_id}
	if state.wallets[t.payer_wallet_id].owner!={"kind":"actor","id":t.student_id} or state.wallets[t.recipient_wallet_id].owner!=recipient_owner: return _fail("InvalidStudyPayment")
	if fact.lessons.size()>t.duration or not Values.whole(fact.start_tick,0,state.tick): return _fail("InvalidStudyProgress")
	var segment_tick: int=state.tick
	if segmented:
		if fact.phase=="completed": segment_tick=fact.completed_tick
		elif state.get("actions",{}).get(action_id,{}).get("remaining_ticks",-1)==0:
			if state.tick==9223372036854775807: return _fail("InvalidStudySegment")
			segment_tick+=1
		var bounded: Dictionary=Segments.prevalidate(fact,segment_tick)
		if not bounded.ok: return bounded
	var pause_valid: Dictionary=Pause.validate(fact,state.tick)
	if not pause_valid.ok: return pause_valid
	if fact.has("xp_settlement_before")!=(not segmented and fact.has("pauses") and fact.phase=="completed"): return _fail("InvalidStudyGrowth")
	if fact.has("xp_settlement_before") and (not Values.whole(fact.xp_settlement_before,0,1200) or fact.xp_settlement_before!=Pause.xp_baseline(fact)): return _fail("InvalidStudyGrowth")
	for index in range(fact.lessons.size()):
		var lesson: Variant=fact.lessons[index]
		if not lesson is Dictionary or lesson.size()!=(4 if segmented else 3) or lesson.has("segment")!=segmented or not Values.fields(lesson,{"fatigue_before":TYPE_INT,"injury":TYPE_INT,"xp":TYPE_INT}) or lesson.fatigue_before!=Pause.fatigue(fact,index) or not Values.whole(lesson.injury,0,3): return _fail("InvalidStudyLesson")
		if (lesson.injury==3 and t.skill_id in PHYSICAL) or lesson.xp!=lesson_xp(lesson.fatigue_before,lesson.injury): return _fail("InvalidStudyLesson")
	if segmented:
		var segments_valid: Dictionary=Segments.validate(fact,segment_tick)
		if not segments_valid.ok: return segments_valid
	if fact.phase=="started":
		if fact.completed_tick!=-1 or fact.success: return _fail("InvalidStudyFact")
	elif fact.phase=="completed":
		if fact.success:
			if not fact.code.is_empty() or fact.lessons.size()!=t.duration or fact.completed_tick!=fact.start_tick+t.duration+Pause.elapsed(fact,state.tick): return _fail("InvalidStudyCompletion")
		elif fact.code=="StudyCancelled":
			if fact.lessons.size()>=t.duration or fact.completed_tick!=fact.start_tick+fact.lessons.size()+Pause.elapsed(fact,state.tick): return _fail("InvalidStudyCompletion")
		elif fact.code.is_empty() or fact.lessons.size()>=t.duration or fact.completed_tick!=fact.start_tick+fact.lessons.size()+1+Pause.elapsed(fact,state.tick): return _fail("InvalidStudyCompletion")
	else: return _fail("InvalidStudyFact")
	return {"ok":true}

## 行动引用必须匹配永久事实和原参与者，不能拿别人的完成事实重放学习。
static func _action_fact(state: Dictionary, data: Dictionary, action: Dictionary) -> Dictionary:
	if not Values.fields(action,{"id":TYPE_STRING,"kind":TYPE_STRING,"actor_id":TYPE_STRING,"participants":TYPE_ARRAY,"remaining_ticks":TYPE_INT,"start_tick":TYPE_INT,"params":TYPE_DICTIONARY}) or action.kind!=KIND or action.params.size()!=1 or not action.params.get("study") is Dictionary: return _fail("InvalidStudyAction")
	var fact: Variant=state.get("facts",{}).get(action.id)
	if not fact is Dictionary: return _fail("MissingStudyFact")
	var valid: Dictionary=_fact(state,data,fact,action.id)
	if not valid.ok: return valid
	if action.actor_id!=fact.actor_id or action.start_tick!=fact.start_tick or action.params.study!=fact.terms or action.participants!=([] if Pause.paused(fact) else [fact.terms.student_id,fact.terms.teacher_id]): return _fail("StudyFactMismatch")
	return {"ok":true}

## 旧政策按末尾公式计算；新政策只读取已入账收据，不重复乘手动倍率。
static func _earned(fact: Dictionary) -> int:
	return Etiquette.earned(fact)

## 有限查询和回执只汇总本课已入账贡献；旧政策仍按原终态可得值解释。
static func credited_xp(fact: Dictionary) -> int:
	if Segments.enabled(fact): return Segments.credited_xp(fact)
	var baseline: int=fact.get("xp_settlement_before",fact.terms.xp_before)
	return maxi(0,mini(fact.terms.xp_limit,baseline+_earned(fact))-baseline)

## 固定整课时费来自下一目标等级；总额整除课时，不因分期次序产生舍入余款。
static func _lesson_cost(terms: Dictionary) -> int:
	return 20+10*(level(terms.xp_before)+1)

## 回执分开已提供部分费用与未提供预留释放；不是从已付教师余额伪造退款。
## 重复完成返回原业务结果，资产实际释放仅由统一生命周期进行一次。
static func _receipt(fact: Dictionary) -> Dictionary:
	var t: Dictionary=fact.terms
	var receipt: Dictionary={"ok":true,"success":fact.success,"code":fact.code,"skill_id":t.skill_id,"xp_gained":credited_xp(fact),"fee_paid":_lesson_cost(t)*fact.lessons.size(),"unprovided_released":t.fee-_lesson_cost(t)*fact.lessons.size(),"completed_lessons":fact.lessons.size(),"payment_policy":"reserve_then_pay_per_lesson"}
	if fact.has("etiquette"): receipt.etiquette=fact.etiquette.challenge.result.duplicate(true)
	if fact.has("challenge"): receipt.challenge={"kind":Etiquette.kind(fact),"result":fact.challenge.challenge.result.duplicate(true),"price_applied":false}
	return receipt

## 领域拒绝保留稳定原因，调用方丢弃候选即可，不伪装成功学习。
static func _fail(code: String) -> Dictionary:
	return {"ok":false,"code":code}
