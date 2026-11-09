## 普通课程暂停账只记录实际停课区间与恢复时疲劳；不另存钱、经验或行动身份。
extends RefCounted
const Segments=preload("res://core/domains/cultivation/skill_study_segments.gd")
const Values=preload("res://core/base/value_checks.gd")

## 新政策允许手动暂停；历史条款仍按原题面模式保持既有边界。
static func supported(terms: Dictionary) -> bool:
	return terms.get("settlement_policy")==Segments.POLICY or terms.get("etiquette",terms.get("challenge",{})).get("mode","automatic")=="automatic"

## 只有最后一段未结束才是真正暂停；已完成区间不阻止后续正常授课。
static func paused(fact: Dictionary) -> bool:
	var pauses: Array=fact.get("pauses",[])
	return not pauses.is_empty() and pauses[-1].end_tick==-1

## 暂停耗时单列；当前未结束区间按当前稳定tick计算，不把它当作已提供课程。
static func elapsed(fact: Dictionary, tick: int) -> int:
	var total: int=0
	for pause in fact.get("pauses",[]): total+=(tick if pause.end_tick==-1 else pause.end_tick)-pause.start_tick
	return total

## 每段课程从最近恢复时真实疲劳继续；原冻结条款和过去课时不被重写。
static func fatigue(fact: Dictionary, lesson_index: int, teacher: bool=false) -> int:
	var baseline: int=fact.terms.teacher_fatigue if teacher else fact.terms.student_fatigue
	var first: int=0
	for pause in fact.get("pauses",[]):
		if pause.end_tick!=-1 and pause.after_lessons<=lesson_index:
			baseline=pause.teacher_fatigue if teacher else pause.student_fatigue
			first=pause.after_lessons
	return mini(100,baseline+(2 if teacher else 6)*(lesson_index-first))

## 严格验证暂停顺序、真实时钟与恢复基线；旧无暂停事实保持原形状和因果。
static func validate(fact: Dictionary, tick: int) -> Dictionary:
	if not fact.has("pauses"): return {"ok":true}
	if not fact.pauses is Array or fact.pauses.is_empty() or not supported(fact.terms): return {"ok":false,"code":"InvalidStudyPause"}
	var previous_lessons: int=0
	var previous_xp: int=fact.terms.xp_before
	var previous_credited: int=0
	var segmented: bool=Segments.enabled(fact)
	var total: int=0
	for index in range(fact.pauses.size()):
		var pause: Variant=fact.pauses[index]
		if not pause is Dictionary or pause.size()!=6+(1 if pause.has("credited_xp") else 0) or not Values.fields(pause,{"start_tick":TYPE_INT,"end_tick":TYPE_INT,"after_lessons":TYPE_INT,"student_fatigue":TYPE_INT,"teacher_fatigue":TYPE_INT,"student_xp":TYPE_INT}): return {"ok":false,"code":"InvalidStudyPause"}
		if not Values.whole(pause.after_lessons,previous_lessons,fact.lessons.size()) or pause.after_lessons>=fact.terms.duration or pause.start_tick!=fact.start_tick+pause.after_lessons+total or pause.start_tick>tick: return {"ok":false,"code":"InvalidStudyPause"}
		if pause.has("credited_xp")!=(segmented and pause.end_tick>=0): return {"ok":false,"code":"InvalidStudyPause"}
		if pause.end_tick==-1:
			if index!=fact.pauses.size()-1 or fact.phase!="started" or pause.after_lessons!=fact.lessons.size() or pause.student_fatigue!=-1 or pause.teacher_fatigue!=-1 or pause.student_xp!=-1: return {"ok":false,"code":"InvalidStudyPause"}
		elif not Values.whole(pause.end_tick,pause.start_tick,tick) or not Values.whole(pause.student_fatigue,0,100) or not Values.whole(pause.teacher_fatigue,0,100) or not Values.whole(pause.student_xp,previous_xp,1200): return {"ok":false,"code":"InvalidStudyPause"}
		else:
			if segmented:
				if not Values.whole(pause.credited_xp,previous_credited,Segments.credited_xp(fact)) or pause.student_xp<previous_xp+pause.credited_xp-previous_credited: return {"ok":false,"code":"InvalidStudyPause"}
				previous_credited=pause.credited_xp
			total+=pause.end_tick-pause.start_tick
			previous_xp=pause.student_xp
		previous_lessons=pause.after_lessons
	return {"ok":true}

## 恢复与暂停中取消都关闭同一段停课记录；只记录当前状态，不恢复旧疲劳。
static func close(state: Dictionary, fact: Dictionary) -> void:
	var pause: Dictionary=fact.pauses[-1]
	pause.end_tick=state.tick
	pause.student_fatigue=state.actors[fact.terms.student_id].fatigue
	pause.teacher_fatigue=state.actors[fact.terms.teacher_id].fatigue
	pause.student_xp=state.actors[fact.terms.student_id].skills[fact.terms.skill_id]
	if Segments.enabled(fact): pause.credited_xp=Segments.credited_xp(fact)

## 恢复基线保留外部成长；新政策再叠加恢复后真实贡献，旧政策仍保留待结经验。
static func xp_baseline(fact: Dictionary) -> int:
	var result: int=resume_xp(fact)
	var credited: int=0
	for pause in fact.get("pauses",[]):
		if pause.end_tick!=-1: credited=pause.get("credited_xp",0)
	return result+Segments.credited_xp(fact)-credited if Segments.enabled(fact) else result

## 教师资格只重验本次恢复时的学员水平，不因原课程内抵达上限擅自缩短已接受课时。
static func resume_xp(fact: Dictionary) -> int:
	var result: int=fact.terms.xp_before
	for pause in fact.get("pauses",[]):
		if pause.end_tick!=-1: result=pause.student_xp
	return result
