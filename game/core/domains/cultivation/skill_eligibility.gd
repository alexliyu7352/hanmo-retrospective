## 武学前置只消费真实人物普通技艺XP；开战复制合格已学集合，不改变人物的掌握账。
extends RefCounted
const Levels=preload("res://core/domains/cultivation/skill_levels.gd")

## 没声明前置的旧技能保持原行为；显式前置必须是受支持技能和一至四级。
static func check(actor: Dictionary, definition: Dictionary) -> Dictionary:
	if not definition.has("required_skill_level"): return {"ok":true}
	var required: Variant=definition.required_skill_level
	if not required is Dictionary or required.size()!=2 or not required.get("skill_id") in Levels.SKILLS or typeof(required.get("level"))!=TYPE_INT or required.level<1 or required.level>4: return {"ok":false,"code":"InvalidSkillRequirement"}
	var actual: int=Levels.level(actor.get("skills",{}).get(required.skill_id))
	if actual<0: return {"ok":false,"code":"InvalidSkillExperience"}
	if actual<required.level: return {"ok":false,"code":"SkillLevelRequired","skill_id":required.skill_id,"required_level":required.level,"actual_level":actual}
	return {"ok":true}

## 只过滤显式前置不满足的已掌握技能，保留原掌握数值、顺序与无前置旧技能。
static func learned_snapshot(actor: Dictionary, data: Dictionary) -> Dictionary:
	var learned: Dictionary=actor.get("combat",{}).get("learned",{}).duplicate(true)
	for id in learned.keys():
		if not check(actor,data.get("personal_skills",{}).get(id,{})).ok: learned.erase(id)
	return learned

## 仅用于planning且世界战斗暂停期间XP不变的当前合同；已关闭战斗不以人物后来成长重算快照。
## 新前置技能须仍有真实掌握来源且达到XP门槛，无前置旧技能不在本门收紧。
static func validate_snapshot(actor: Dictionary, data: Dictionary, unit: Dictionary) -> Dictionary:
	for id in unit.get("learned",{}):
		var definition: Dictionary=data.get("personal_skills",{}).get(id,{})
		if not definition.has("required_skill_level"): continue
		if not actor.get("combat",{}).get("learned",{}).has(id) or actor.combat.learned[id]!=unit.learned[id] or not check(actor,definition).ok: return {"ok":false,"code":"InvalidQualifiedBattleSkill","skill_id":id}
	return {"ok":true}
