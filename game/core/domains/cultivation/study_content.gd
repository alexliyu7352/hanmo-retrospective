## 普通课程的题面链接只在内容装载时校验；不创建挑战、不消耗随机或授予学习经验。
extends RefCounted
const Bargain=preload("res://core/domains/cultivation/bargain_challenge.gd")
const Adapter=preload("res://core/domains/cultivation/study_etiquette.gd")
const Challenge=preload("res://core/domains/cultivation/etiquette_challenge.gd")

## 校验所有显式题面及课程引用，未选课程不会因定义存在而自动开课。
static func validate(data: Dictionary) -> Dictionary:
	for definition in data.get("etiquette_challenges",{}).values():
		var checked: Dictionary=Challenge.validate_definition(definition,data.texts)
		if not checked.ok: return checked
	for definition in data.get("bargain_challenges",{}).values():
		var checked: Dictionary=Bargain.validate_definition(definition,data.texts)
		if not checked.ok: return checked
	for role in data.roles.values():
		for course in role.get("skill_courses",[]):
			if not Adapter.course_valid(course): return {"ok":false,"code":"STUDY_CHALLENGE_REFERENCE","path":"/roles/"+role.id+"/skill_courses"}
			if course.has("challenge"):
				var table: String="etiquette_challenges" if course.challenge.kind=="etiquette" else "bargain_challenges"
				if not data.get(table,{}).has(course.challenge.id): return {"ok":false,"code":"STUDY_CHALLENGE_REFERENCE","path":"/roles/"+role.id+"/skill_courses"}
			if not course.has("etiquette_challenge_id"): continue
			if not course.skill_id in ["rites","debate"] or not data.get("etiquette_challenges",{}).has(course.etiquette_challenge_id): return {"ok":false,"code":"STUDY_CHALLENGE_REFERENCE","path":"/roles/"+role.id+"/skill_courses"}
	return {"ok":true}
