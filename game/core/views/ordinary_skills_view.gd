## 普通技艺总览只读本人唯一经验账和已结课事实；不授奖、结算、推进时间或返回权威容器。
extends RefCounted
const Levels=preload("res://core/domains/cultivation/skill_levels.gd")
const Study=preload("res://core/domains/cultivation/skill_study_rules.gd")
const Pause=preload("res://core/domains/cultivation/skill_study_pause.gd")
const Identity=preload("res://core/actor_identity.gd")

## 十项沿原技艺顺序；缺失经验保留未知，详情只展开选中技艺的本人终态课程。
static func query(state: Dictionary, data: Dictionary, skill_id: String="") -> Dictionary:
	var actor_id: String=state.player_actor_id
	var actor: Dictionary=state.actors[actor_id]
	var selected: String=skill_id if skill_id in Levels.SKILLS else Levels.SKILLS[0]
	var result: Dictionary={"actor_id":actor_id,"display_name":_person(state,data,actor_id),"skills":[],"selected_skill_id":selected,"course_records":[]}
	for id in Levels.SKILLS:
		var xp: Variant=actor.get("skills",{}).get(id)
		var level: int=Levels.level(xp)
		result.skills.append({"skill_id":id,"xp":xp if level>=0 else null,"level":level,"next_threshold":Levels.THRESHOLDS[level+1] if level>=0 and level+1<Levels.THRESHOLDS.size() else null})
	for id in state.get("facts",{}):
		var fact: Variant=state.facts[id]
		if not fact is Dictionary or fact.get("kind")!=Study.FACT_KIND or fact.get("phase")!="completed" or fact.get("actor_id")!=actor_id: continue
		var terms: Dictionary=fact.get("terms",{})
		if terms.get("student_id")!=actor_id or terms.get("skill_id")!=selected: continue
		var receipt: Dictionary=Study._receipt(fact)
		var baseline: int=terms.xp_before if terms.get("settlement_policy","")=="per_lesson_v1" else fact.get("xp_settlement_before",terms.xp_before)
		var end_xp: int=Pause.xp_baseline(fact) if terms.get("settlement_policy","")=="per_lesson_v1" else baseline+receipt.xp_gained
		var record: Dictionary={"action_id":id,"skill_id":selected,"completed_tick":fact.completed_tick,"success":receipt.success,"code":receipt.code,"xp_before":baseline,"xp_gained":receipt.xp_gained,"xp_after":end_xp,"completed_lessons":receipt.completed_lessons,"duration":terms.duration,"fee_paid":receipt.fee_paid,"unprovided_released":receipt.unprovided_released,"teacher_name":_person(state,data,terms.teacher_id),"place_name":_name(data,"places",terms.context.place_id,"场所未记")}
		if terms.get("settlement_policy","")=="per_lesson_v1": record.settlement_policy="per_lesson_v1"
		result.course_records.append(record)
	result.course_records.sort_custom(func(a: Dictionary,b: Dictionary): return a.completed_tick>b.completed_tick if a.completed_tick!=b.completed_tick else a.action_id<b.action_id)
	if _segmented_courses(data): result.settlement_policy="per_lesson_v1"
	return result

## 本页政策来自当前作者课程，不改全局能力身份，也不把旧无标记课程解释成逐课结算。
static func _segmented_courses(data: Dictionary) -> bool:
	for role in data.get("roles",{}).values():
		for course in role.get("skill_courses",[]):
			if course.get("settlement_policy","")=="per_lesson_v1": return true
	return false

## 本人课程事实证明曾当面受教；仅解析该授艺人的显示名，不暴露其经验、资产或档案。
static func _person(state: Dictionary, data: Dictionary, actor_id: String) -> String:
	var definition: Dictionary=Identity.definition(state,data,actor_id)
	return definition.display_name if definition.has("display_name") else _name(data,"actors",actor_id,"姓名未记")

## 只显示作者已提供的名称；缺记录时保留未知，不把内部标识当作玩家知识。
static func _name(data: Dictionary, table: String, id: String, fallback: String) -> String:
	var definition: Dictionary=data.get(table,{}).get(id,{})
	return definition.get("display_name",data.get("texts",{}).get(definition.get("name_key",""),fallback))
