## 自建界面只编辑草案；最终预算仍由原生新局入口校验。
extends RefCounted

const ATTRIBUTES={"leadership":"统率","martial":"武力","intellect":"智谋","administration":"政务","charisma":"魅力"}
const BASE_SKILLS={"infantry":"步战","cavalry":"骑战","archery":"弓弩","martial":"武艺","tactics":"兵法","debate":"辩说","negotiation":"交涉","rites":"礼学","commerce":"商术","transport":"运输"}
const EXTENDED_SKILLS={"naval":"水战","espionage":"谍报","fortification":"筑城","agriculture":"农政","medicine":"医术","forging":"锻造"}

static func skill_names(extended: bool) -> Dictionary:
	var result: Dictionary=BASE_SKILLS.duplicate()
	if extended: result.merge(EXTENDED_SKILLS)
	return result

static func create(extended: bool) -> Dictionary:
	var attributes: Dictionary={}; var skills: Dictionary={}
	for id in ATTRIBUTES: attributes[id]=40
	for id in skill_names(extended): skills[id]=0
	return {"creation_mode":"standard","display_name":"","age_years":18,"attributes":attributes,"skills":skills}

static func level(xp: int) -> int:
	if xp>=1200: return 4
	if xp>=650: return 3
	if xp>=300: return 2
	if xp>=100: return 1
	return 0

static func _keys(value: Variant, expected: Array) -> bool:
	if not value is Dictionary or value.size()!=expected.size(): return false
	for key in expected:
		if not value.has(key): return false
	return true

static func inspect(draft: Dictionary, extended: bool) -> Dictionary:
	if not _keys(draft,["creation_mode","display_name","age_years","attributes","skills"]) or not draft.creation_mode in ["standard","free"]: return {"ok":false,"code":"InvalidCharacterCreationMode"}
	var standard: bool=draft.creation_mode=="standard"
	if not _keys(draft.attributes,ATTRIBUTES.keys()) or not _keys(draft.skills,skill_names(extended).keys()): return {"ok":false,"code":"CharacterCreationSkillProfileMismatch"}
	var total: int=0; var points: int=0
	for id in ATTRIBUTES:
		var value: Variant=draft.attributes[id]
		if not value is int or value<(20 if standard else 1) or value>(85 if standard else 100): return {"ok":false,"code":"CharacterCreationAttributeRange"}
		total+=value
	for id in skill_names(extended):
		var xp: Variant=draft.skills[id]
		if not xp is int or xp<0 or xp>1200: return {"ok":false,"code":"CharacterCreationSkillRange"}
		if standard and not xp in [0,100,300]: return {"ok":false,"code":"CharacterCreationSkillPointsRequired"}
		points+=level(xp)
	var result: Dictionary={"ok":true,"attribute_total":total,"skill_level_total":points,"attribute_remaining":300-total,"skill_remaining":4-points}
	if not draft.display_name is String or draft.display_name.is_empty() or draft.display_name.length()>80:
		result.ok=false; result.code="InvalidActorDisplayName"
	else:
		var name_value: String=draft.display_name
		if name_value.unicode_at(0)<=32 or name_value.unicode_at(name_value.length()-1)<=32: result.ok=false; result.code="InvalidActorDisplayName"
		for index in name_value.length():
			if name_value.unicode_at(index)<32 or name_value.unicode_at(index)==127: result.ok=false; result.code="InvalidActorDisplayName"
	if not draft.age_years is int or draft.age_years<18 or draft.age_years>(60 if standard else 120): result.ok=false; result.code="CharacterCreationAgeRange"
	if standard and total!=300: result.ok=false; result.code="CharacterCreationAttributeBudget"
	if standard and points!=4: result.ok=false; result.code="CharacterCreationSkillBudget"
	return result

## 切换模式是明确编辑，不送经验或选择“专长”再叠一份预算。
static func change_mode(draft: Dictionary, mode: String, extended: bool) -> Dictionary:
	if not mode in ["standard","free"]: return {}
	var result: Dictionary=draft.duplicate(true); result.creation_mode=mode
	if mode=="standard":
		result.age_years=clampi(int(result.age_years),18,60)
		for id in ATTRIBUTES: result.attributes[id]=clampi(int(result.attributes[id]),20,85)
		for id in skill_names(extended): result.skills[id]=[0,100,300][mini(level(int(result.skills[id])),2)]
	return result
