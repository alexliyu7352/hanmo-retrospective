## 显式新政策逐课入账；不可变教学收据与迟答差额分开，旧课程不经过此账本。
extends RefCounted
const Values=preload("res://core/base/value_checks.gd")
const Etiquette=preload("res://core/domains/cultivation/study_etiquette.gd")
const POLICY="per_lesson_v1"

## 条款缺省是原末尾结算，不能因加载或新界面自动升级历史行动。
static func enabled(fact: Dictionary) -> bool:
	return fact.terms.get("settlement_policy")==POLICY

## 只汇总真正写入人物的本课贡献；外部成长不会被归入课程回执。
static func credited_xp(fact: Dictionary) -> int:
	var total: int=0
	for lesson in fact.lessons:
		if lesson.has("segment"):
			total+=lesson.segment.xp_gained
			total+=lesson.segment.get("adjustment",{}).get("xp_gained",0)
	return total

## 原正常基线及已决表现倍率只从原题面读取，不能由调用方自报。
static func performance(fact: Dictionary) -> int:
	var instance: Dictionary=Etiquette.instance(fact)
	return instance.challenge.result.performance_bp if not instance.is_empty() and instance.challenge.phase=="completed" else 10000

## 真正授课半日写基础收据及人物经验；支付与疲劳由同一候选的原课程规则负责。
static func teach(state: Dictionary, fact: Dictionary, lesson: Dictionary) -> void:
	var student: Dictionary=state.actors[fact.terms.student_id]
	var before: int=student.skills[fact.terms.skill_id]
	var bp: int=performance(fact)
	@warning_ignore("integer_division")
	var gain: int=maxi(0,mini(lesson.xp*bp/10000,fact.terms.xp_limit-before))
	var instance: Dictionary=Etiquette.instance(fact)
	lesson.segment={"ordinal":fact.lessons.size()+1,"settled_tick":state.tick+1,"xp_before":before,"performance_bp":bp,"xp_gained":gain,"performance_pending":not instance.is_empty() and instance.challenge.phase=="active"}
	student.skills[fact.terms.skill_id]=before+gain

## 迟答只为原先未决课时补一次原取整差额，已授基础经验和费用绝不重算。
static func adjust(state: Dictionary, fact: Dictionary) -> void:
	if not enabled(fact): return
	var bp: int=performance(fact)
	if bp==10000: return
	var student: Dictionary=state.actors[fact.terms.student_id]
	for lesson in fact.lessons:
		var part: Dictionary=lesson.segment
		if not part.performance_pending or part.has("adjustment"): continue
		var before: int=student.skills[fact.terms.skill_id]
		@warning_ignore("integer_division")
		var delta: int=lesson.xp*bp/10000-lesson.xp
		var gain: int=maxi(0,mini(delta,fact.terms.xp_limit-before))
		part.adjustment={"settled_tick":state.tick,"xp_before":before,"performance_bp":bp,"xp_gained":gain}
		student.skills[fact.terms.skill_id]=before+gain

## 任意冷输入先收紧整数范围，避免在暂停累计或精确公式验证之前发生int64溢出。
static func prevalidate(fact: Dictionary, now: int) -> Dictionary:
	for lesson in fact.lessons:
		if not lesson is Dictionary or not Values.whole(lesson.get("xp"),0,20): return _fail("InvalidStudySegment")
		var part: Variant=lesson.get("segment")
		if not part is Dictionary or not Values.whole(part.get("ordinal"),1,6) or not Values.whole(part.get("settled_tick"),0,now) or not Values.whole(part.get("xp_before"),0,1200) or not Values.whole(part.get("performance_bp"),10000,12000) or not Values.whole(part.get("xp_gained"),0,24): return _fail("InvalidStudySegment")
		if part.has("adjustment"):
			var extra: Variant=part.adjustment
			if not extra is Dictionary or not Values.whole(extra.get("settled_tick"),0,now) or not Values.whole(extra.get("xp_before"),0,1200) or not Values.whole(extra.get("performance_bp"),10000,12000) or not Values.whole(extra.get("xp_gained"),0,4): return _fail("InvalidStudyAdjustment")
	if fact.has("pauses") and fact.pauses is Array:
		for pause in fact.pauses:
			if pause is Dictionary and pause.has("credited_xp") and not Values.whole(pause.credited_xp,0,144): return _fail("InvalidStudyPause")
	return {"ok":true}

## 每一信用事件按真实tick、教学先于答题及课时次序验证，恢复基线隔离外部经验。
static func validate(fact: Dictionary, now: int) -> Dictionary:
	var credits: Array=[]
	var final_bp: int=performance(fact)
	var instance: Dictionary=Etiquette.instance(fact)
	var previous_tick: int=fact.start_tick
	for index in range(fact.lessons.size()):
		var lesson: Dictionary=fact.lessons[index]
		var part: Variant=lesson.get("segment")
		if not part is Dictionary or part.size()!=6+(1 if part.has("adjustment") else 0) or not Values.fields(part,{"ordinal":TYPE_INT,"settled_tick":TYPE_INT,"xp_before":TYPE_INT,"performance_bp":TYPE_INT,"xp_gained":TYPE_INT,"performance_pending":TYPE_BOOL}): return _fail("InvalidStudySegment")
		if now<fact.start_tick or index+1>now-fact.start_tick: return _fail("InvalidStudySegment")
		var tick: int=fact.start_tick+index+1
		for pause in fact.get("pauses",[]):
			if pause.end_tick>=0 and pause.after_lessons<=index:
				var duration: int=pause.end_tick-pause.start_tick
				if duration>now-tick: return _fail("InvalidStudySegment")
				tick+=duration
		@warning_ignore("integer_division")
		var expected_gain: int=maxi(0,mini(lesson.xp*part.performance_bp/10000,fact.terms.xp_limit-part.xp_before))
		if part.ordinal!=index+1 or part.settled_tick!=tick or part.settled_tick<=previous_tick or part.settled_tick>now or not Values.whole(part.xp_before,0,1200) or not Values.whole(part.performance_bp,10000,12000) or part.xp_gained!=expected_gain: return _fail("InvalidStudySegment")
		if part.performance_pending:
			if instance.is_empty() or instance.challenge.mode!="manual" or part.performance_bp!=10000: return _fail("InvalidStudySegment")
		elif part.performance_bp!=final_bp or (not instance.is_empty() and instance.challenge.phase!="completed"): return _fail("InvalidStudySegment")
		if part.has("adjustment")!=(part.performance_pending and final_bp>10000): return _fail("InvalidStudyAdjustment")
		credits.append({"tick":part.settled_tick,"ordinal":part.ordinal,"before":part.xp_before,"gained":part.xp_gained,"adjustment":false})
		if part.has("adjustment"):
			var extra: Variant=part.adjustment
			if not extra is Dictionary or extra.size()!=4 or not Values.fields(extra,{"settled_tick":TYPE_INT,"xp_before":TYPE_INT,"performance_bp":TYPE_INT,"xp_gained":TYPE_INT}): return _fail("InvalidStudyAdjustment")
			@warning_ignore("integer_division")
			var expected_delta: int=maxi(0,mini(lesson.xp*final_bp/10000-lesson.xp,fact.terms.xp_limit-extra.xp_before))
			if not Values.whole(extra.settled_tick,part.settled_tick,now) or not Values.whole(extra.xp_before,0,1200) or extra.performance_bp!=final_bp or extra.xp_gained!=expected_delta: return _fail("InvalidStudyAdjustment")
			credits.append({"tick":extra.settled_tick,"ordinal":part.ordinal,"before":extra.xp_before,"gained":extra.xp_gained,"adjustment":true})
		previous_tick=part.settled_tick
	credits.sort_custom(_credit_before)
	var total: int=0
	for credit in credits:
		var expected: int=fact.terms.xp_before+total
		for pause in fact.get("pauses",[]):
			if pause.end_tick>=0 and pause.end_tick<=credit.tick and pause.get("credited_xp",-1)>=0 and pause.credited_xp<=total and (credit.adjustment or pause.after_lessons<credit.ordinal): expected=pause.student_xp+total-pause.credited_xp
		if credit.before!=expected: return _fail("InvalidStudySegmentBaseline")
		total+=credit.gained
	for pause in fact.get("pauses",[]):
		if pause.end_tick<0: continue
		var before: int=0
		var at: int=0
		for credit in credits:
			if not credit.adjustment:
				if credit.ordinal<=pause.after_lessons: before+=credit.gained
			elif credit.tick<pause.end_tick: before+=credit.gained
			elif credit.tick==pause.end_tick: at+=credit.gained
		if not pause.has("credited_xp") or not pause.credited_xp in [before,before+at]: return _fail("InvalidStudyPause")
	return {"ok":true}

## 同一真实边界先记教学，再按原课时补差，保持C++与GD的唯一验证顺序。
static func _credit_before(a: Dictionary, b: Dictionary) -> bool:
	if a.tick!=b.tick: return a.tick<b.tick
	if a.adjustment!=b.adjustment: return not a.adjustment
	return a.ordinal<b.ordinal

## 证据拒绝保留原原因，不在读档验证中修复或增发课程贡献。
static func _fail(code: String) -> Dictionary:
	return {"ok":false,"code":code}
