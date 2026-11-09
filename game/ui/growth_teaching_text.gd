## 具名邀学沿原对白确认入口展示双方与实际报价，不代学生回应或触发教学。
extends RefCounted

static func confirmation(host, request: Dictionary, preview: Dictionary) -> String:
	if request.get("kind")!="teach_skill" or not preview.get("ok",false): return ""
	var training: Variant=preview.get("training")
	if not training is Dictionary: return ""
	var agreement: Variant=training.get("teaching_agreement")
	if not agreement is Dictionary: return ""
	var student: String=str(training.get("student_id",""))
	var skill: String=str(training.get("skill_id",""))
	if student.is_empty() or skill.is_empty() or agreement.get("student_actor_id")!=student or agreement.get("skill_id")!=skill: return ""
	if agreement.get("teacher_actor_id")!=training.get("teacher_id") or skill!=request.get("skill_id"): return ""
	var ability: Dictionary=host._data.personal_skills.get(skill,{})
	if ability.is_empty(): return ""
	var before: int=int(training.get("mastery_before",-1))
	var after: int=int(training.get("mastery_after",-1))
	if before < -1 or before > 100 or after < 0 or after > 100: return ""
	var progress: String="尚未学会→%d"%after if before<0 else "%d→%d"%[before,after]
	return "%s已明确求教%s。你将亲自传授；双方都须留出时间。按当前条件，学员掌握预计%s，完成后再按实际结果记录。"%[host._name(host._actor(student)),host._name(ability),progress]
