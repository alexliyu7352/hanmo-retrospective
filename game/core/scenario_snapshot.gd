## 从已校验目录投影独立剧本初态；只读定义，不分配实例、不消费RNG或改写内容锁。
extends RefCounted
const SNAPSHOT_FIELDS=["playable_actor_ids","start_date","enabled","initial"]
const BASE_SKILLS=["infantry","cavalry","archery","martial","tactics","debate","negotiation","rites","commerce","transport"]
const EXTENDED_SKILLS=["naval","espionage","fortification","agriculture","medicine","forging"]

## 严格集合和整数范围复用作者合同；未启用政策不写出额外零经验。
static func _skills_match(values: Variant, extended: bool) -> bool:
	var keys: Array=BASE_SKILLS+EXTENDED_SKILLS if extended else BASE_SKILLS
	if not values is Dictionary or values.size()!=keys.size(): return false
	for key in keys:
		if not _whole(values.get(key),0,1200): return false
	return true

## 解析新旧剧本模式；新字段必须成组出现，任何失败都不交出部分有效目录。
static func resolve(catalog: Dictionary, scenario_id: String, player_actor_id: String="") -> Dictionary:
	if not catalog.get("ok",false) or not catalog.get("data") is Dictionary: return _fail("NoCatalog","")
	var data: Dictionary=catalog.data
	var path: String="/scenarios/"+scenario_id
	var scenario: Variant=data.get("scenarios",{}).get(scenario_id)
	if not scenario is Dictionary: return _fail("UnknownScenario",path)
	if scenario.has("character_creation") and scenario.get("dynamic_world",{}).get("founder_policy")!="created_character_v1": return _fail("CharacterCreationCapabilityRequired",path+"/character_creation")
	if scenario.has("household_control"):
		if not scenario.household_control is Dictionary or scenario.household_control.get("profile","")!="initial_household_stewardship_v1": return _fail("UnsupportedHouseholdControlProfile",path+"/household_control")
		if not scenario.get("dynamic_world") is Dictionary: return _fail("HouseholdControlNativeProfileRequired",path+"/household_control")
	if scenario.get("dynamic_world",{}).has("household_provision_management"):
		var managed: Variant=scenario.dynamic_world.household_provision_management
		if not managed is Dictionary or not managed.get("version") is int or managed.version!=1: return _fail("UnsupportedHouseholdProvisionManagement",path+"/dynamic_world/household_provision_management")
		if not scenario.has("household_control") or not scenario.dynamic_world.get("local_economy",{}).has("household_provisioning"): return _fail("HouseholdProvisionManagementDependencies",path)
	if scenario.get("dynamic_world",{}).has("production_disruptions"):
		var disruption: Variant=scenario.dynamic_world.production_disruptions
		if not disruption is Dictionary or not disruption.get("version") is int or disruption.version!=1 or disruption.get("profile")!="authored_production_disruptions_v1" or disruption.get("combination_policy")!="minimum_remaining_yield_v1": return _fail("UnsupportedProductionDisruption",path+"/dynamic_world/production_disruptions")
		if not "production_disruptions_v1" in data.get("required_capabilities",[]): return _fail("ProductionDisruptionCapabilityRequired",path)
		if scenario.dynamic_world.get("profile")!="local_society_v1" or scenario.dynamic_world.get("local_economy",{}).get("enabled")!=true: return _fail("ProductionDisruptionEconomyRequired",path)
	if scenario.has("skill_profile"):
		if scenario.skill_profile!="full_sixteen_v1": return _fail("UnsupportedSkillProfile",path+"/skill_profile")
		if not scenario.get("dynamic_world") is Dictionary: return _fail("ExtendedSkillsNativeProfileRequired",path+"/skill_profile")
		if not scenario.has("initial"): return _fail("ExtendedSkillsSnapshotRequired",path+"/initial")
	if scenario.has("insight_cards"):
		if not scenario.insight_cards is Dictionary or scenario.insight_cards.get("profile","")!="completed_camp_training_insights_v1": return _fail("UnsupportedInsightCardsProfile",path+"/insight_cards")
		if not scenario.get("dynamic_world") is Dictionary: return _fail("InsightsNativeProfileRequired",path+"/insight_cards")
		if not data.get("insight_cards") is Dictionary or data.insight_cards.is_empty(): return _fail("MissingInsightCardCatalog",path+"/insight_cards")
	if scenario.has("study_teaching_requests"):
		if not scenario.study_teaching_requests is Dictionary or scenario.study_teaching_requests.get("profile","")!="authored_study_teaching_requests_v1": return _fail("UnsupportedStudyTeachingProfile",path+"/study_teaching_requests")
		if not scenario.get("dynamic_world") is Dictionary: return _fail("StudyTeachingNativeProfileRequired",path+"/study_teaching_requests")
		if not data.get("study_teaching_requests") is Dictionary or data.study_teaching_requests.is_empty(): return _fail("MissingStudyTeachingRequestCatalog",path+"/study_teaching_requests")
	if scenario.has("family_innate_learning"):
		var innate=scenario.family_innate_learning
		if not innate is Dictionary or innate.size()!=2 or not innate.get("version") is int or innate.version!=1 or innate.get("profile")!="lineage_per_lesson_v1": return _fail("UnsupportedInnateLearningProfile",path+"/family_innate_learning")
		if not scenario.has("family_birth") or not scenario.has("family_lineage") or not "family_innate_learning_v1" in data.get("required_capabilities",[]) or not "cultivation" in data.get("required_capabilities",[]): return _fail("InnateLearningProfileRequired",path+"/family_innate_learning")
	if scenario.get("dynamic_world",{}).has("personal_injury_recovery"):
		var recovery=scenario.dynamic_world.personal_injury_recovery
		if not recovery is Dictionary or recovery.size()!=2 or recovery.get("version")!=1 or not recovery.get("services") is Array or recovery.services.is_empty(): return _fail("InvalidPersonalRecoveryCatalog",path+"/dynamic_world/personal_injury_recovery")
		if not scenario.get("family_life") is Dictionary or not "personal_injury_recovery_v1" in data.get("required_capabilities",[]) or not "regional_life_v1" in data.get("required_capabilities",[]): return _fail("PersonalInjuryRecoveryProfileRequired",path)
		var recovery_roles: Dictionary={}
		for facility in recovery.services:
			if not facility is Dictionary or facility.size()!=2 or not facility.get("role_id") is String or not facility.get("method") in ["safe_lodging","qualified_clinic"] or recovery_roles.has(facility.role_id): return _fail("InvalidPersonalRecoveryFacility",path)
			recovery_roles[facility.role_id]=true
	if scenario.has("family_care_relief"):
		var relief=scenario.get("family_care_relief")
		if not relief is Dictionary or relief.size()!=3 or relief.get("version")!=1 or relief.get("profile")!="living_parent_private_relief_v1" or not relief.get("offers") is Dictionary or relief.offers.is_empty(): return _fail("UnsupportedChildCareReliefProfile",path+"/family_care_relief")
		if not scenario.get("family_birth") is Dictionary or not scenario.get("family_lineage") is Dictionary or not scenario.get("family_life") is Dictionary or not "family_care_relief_v1" in data.get("required_capabilities",[]): return _fail("ChildCareReliefProfileRequired",path+"/family_care_relief")
	if scenario.has("family_birth") or scenario.has("family_lineage"):
		var birth=scenario.get("family_birth"); var lineage=scenario.get("family_lineage")
		if not birth is Dictionary or birth.size()!=3 or birth.get("version")!=1 or birth.get("profile")!="adult_birth_personal_care_v1" or not birth.get("arrangements") is Dictionary: return _fail("UnsupportedBirthProfile",path+"/family_birth")
		if not lineage is Dictionary or lineage.size()!=5 or lineage.get("version")!=1 or lineage.get("profile")!="biological_origin_innate_v1" or not lineage.get("origins") is Dictionary or not lineage.get("original_innate_levels") is Dictionary or not lineage.get("endowment") is Dictionary: return _fail("UnsupportedLineageProfile",path+"/family_lineage")
		if not scenario.get("family_life") is Dictionary or scenario.get("skill_profile")!="full_sixteen_v1" or not "family_birth_v1" in data.get("required_capabilities",[]): return _fail("BirthProfileRequired",path+"/family_birth")
	if scenario.has("family_disciple_legacy"):
		var disciple=scenario.family_disciple_legacy
		if not disciple is Dictionary or disciple.size()!=3 or disciple.get("version")!=1 or disciple.get("profile")!="accepted_disciple_posthumous_control_v1" or not disciple.get("requests") is Dictionary: return _fail("UnsupportedDiscipleControlProfile",path+"/family_disciple_legacy")
		if not scenario.has("family_legacy") or not scenario.has("family_apprenticeship") or not "family_disciple_legacy_v1" in data.get("required_capabilities",[]): return _fail("DiscipleControlDependenciesRequired",path+"/family_disciple_legacy")
	if scenario.has("family_apprenticeship"):
		var apprenticeship=scenario.family_apprenticeship
		if not apprenticeship is Dictionary or apprenticeship.size()!=3 or apprenticeship.get("version")!=1 or apprenticeship.get("profile")!="adult_personal_apprenticeship_v1" or not apprenticeship.get("statements") is Dictionary: return _fail("UnsupportedApprenticeshipProfile",path+"/family_apprenticeship")
		if scenario.get("family_life")!={"profile":"verified_military_deaths_v1","player_death":"await_explicit_continuation"} or not "family_apprenticeship_v1" in data.get("required_capabilities",[]): return _fail("ApprenticeshipProfileRequired",path+"/family_apprenticeship")
	if scenario.has("family_marriage"):
		var marriage=scenario.family_marriage
		if not marriage is Dictionary or marriage.size()!=4 or marriage.get("version")!=1 or marriage.get("profile")!="personal_marriage_separate_assets_v1" or not marriage.get("origins") is Dictionary or not marriage.get("statements") is Dictionary: return _fail("UnsupportedMarriageProfile",path+"/family_marriage")
		if not scenario.get("family_life") is Dictionary or not "family_marriage_v1" in data.get("required_capabilities",[]): return _fail("MarriageProfileRequired",path+"/family_marriage")
	if scenario.has("family_inheritance"):
		var inheritance=scenario.family_inheritance
		if not inheritance is Dictionary or inheritance.size()!=1 or inheritance.get("profile")!="disclosed_personal_bequest_v1": return _fail("UnsupportedFamilyInheritanceProfile",path+"/family_inheritance")
		if not scenario.get("family_bequest") is Dictionary or not scenario.get("task_message_notices") is Dictionary or not "family_inheritance_v1" in data.get("required_capabilities",[]): return _fail("FamilyInheritanceProfileRequired",path+"/family_inheritance")
	if scenario.has("family_bequest"):
		var bequest=scenario.family_bequest
		if not bequest is Dictionary or bequest.size()!=1 or bequest.get("profile")!="revocable_personal_bequest_v1": return _fail("UnsupportedFamilyBequestProfile",path+"/family_bequest")
		if not scenario.get("family_life") is Dictionary or not "family_bequest_v1" in data.get("required_capabilities",[]): return _fail("FamilyBequestProfileRequired",path+"/family_bequest")
	if scenario.has("family_legacy"):
		var legacy=scenario.family_legacy
		if not legacy is Dictionary or legacy.get("profile")!="authored_posthumous_control_consent_v1" or not legacy.get("request_ids") is Array or legacy.size()!=2: return _fail("UnsupportedLegacyControlConsentProfile",path+"/family_legacy")
		if not scenario.get("family_life") is Dictionary or not "family_legacy_v1" in data.get("required_capabilities",[]) or not data.get("legacy_control_consent_requests") is Dictionary: return _fail("LegacyControlConsentProfileRequired",path+"/family_legacy")
	if scenario.has("family_life"):
		var life=scenario.family_life
		if not life is Dictionary or life.get("profile")!="verified_military_deaths_v1" or life.get("player_death")!="await_explicit_continuation" or life.size()!=2: return _fail("UnsupportedFamilyDeathProfile",path+"/family_life")
		if not scenario.get("dynamic_world") is Dictionary or not "family_life_v1" in data.get("required_capabilities",[]): return _fail("FamilyLifeCapabilityRequired",path+"/family_life")
	if scenario.has("voluntary_succession"):
		if not scenario.voluntary_succession is Dictionary or scenario.voluntary_succession.get("profile","")!="authored_voluntary_succession_v1": return _fail("UnsupportedVoluntarySuccessionProfile",path+"/voluntary_succession")
		if not scenario.get("dynamic_world") is Dictionary: return _fail("VoluntarySuccessionNativeProfileRequired",path+"/voluntary_succession")
		if not data.get("succession_consent_requests") is Dictionary or data.succession_consent_requests.is_empty(): return _fail("MissingSuccessionConsentCatalog",path+"/voluntary_succession")
	if scenario.has("adult_adoption"):
		if not scenario.adult_adoption is Dictionary or scenario.adult_adoption.get("profile","")!="authored_adult_adoption_requests_v1": return _fail("UnsupportedAdultAdoptionProfile",path+"/adult_adoption")
		if not scenario.get("dynamic_world") is Dictionary: return _fail("AdultAdoptionNativeProfileRequired",path+"/adult_adoption")
		if not data.get("adult_adoption_requests") is Dictionary or data.adult_adoption_requests.is_empty(): return _fail("MissingAdultAdoptionRequestCatalog",path+"/adult_adoption")
	if scenario.has("teaching_requests"):
		if not scenario.teaching_requests is Dictionary or scenario.teaching_requests.get("profile","")!="authored_teaching_requests_v1": return _fail("UnsupportedTeachingRequestsProfile",path+"/teaching_requests")
		if not scenario.get("dynamic_world") is Dictionary: return _fail("TeachingNativeProfileRequired",path+"/teaching_requests")
		if not data.get("teaching_requests") is Dictionary or data.teaching_requests.is_empty(): return _fail("MissingTeachingRequestCatalog",path+"/teaching_requests")
	if scenario.has("growth_cards"):
		if not scenario.growth_cards is Dictionary or scenario.growth_cards.get("profile","")!="learned_ability_cards_v1": return _fail("UnsupportedCardsProfile",path+"/growth_cards")
		if not scenario.get("dynamic_world") is Dictionary: return _fail("CardsNativeProfileRequired",path+"/growth_cards")
		if not data.get("cards") is Dictionary or data.cards.is_empty(): return _fail("MissingCardCatalog",path+"/growth_cards")
	if scenario.has("actor_card_unlocks"):
		if not scenario.actor_card_unlocks is Dictionary or scenario.actor_card_unlocks.get("profile","") not in ["completed_procurement_actor_cards_v1","completed_task_actor_cards_v2"]: return _fail("UnsupportedActorStartProfile",path+"/actor_card_unlocks")
		if not scenario.get("dynamic_world",{}).get("founder_policy","") in ["selected_existing_actor_v1","selected_existing_actor_roster_v1"]: return _fail("ActorCardsExistingRosterRequired",path+"/actor_card_unlocks")
		if not data.get("actor_cards") is Dictionary or data.actor_cards.is_empty(): return _fail("MissingActorCardCatalog",path+"/actor_card_unlocks")
	var present: int=0
	for field in SNAPSHOT_FIELDS:
		if scenario.has(field): present+=1
	if present==0: return _legacy(catalog,scenario_id,player_actor_id)
	if present!=SNAPSHOT_FIELDS.size(): return _fail("PartialScenarioSnapshot",path)
	if not _date(scenario.start_date): return _fail("InvalidScenarioStartDate",path+"/start_date")
	var creating: bool=scenario.get("dynamic_world",{}).get("founder_policy")=="created_character_v1"
	if not _ids(scenario.playable_actor_ids) or (scenario.playable_actor_ids.is_empty() and not creating): return _fail("InvalidPlayableActor",path+"/playable_actor_ids")
	return _resolve_snapshot(catalog,scenario,player_actor_id,path)

## 旧剧本保留定义枚举顺序与缺失年龄，仅附稳定的全部静态人物激活集合。
static func _legacy(catalog: Dictionary, scenario_id: String, player_actor_id: String) -> Dictionary:
	var data: Dictionary=catalog.data
	var player: String=data.scenarios[scenario_id].player_actor
	if not player_actor_id.is_empty() and player_actor_id!=player: return _fail("InvalidPlayableActor","/scenarios/"+scenario_id+"/player_actor")
	var effective: Dictionary=catalog.duplicate(true)
	var active_ids: Array=data.actors.keys(); active_ids.sort()
	effective.data.active_static_actor_ids=active_ids
	var initial: Dictionary={"legacy":true,"player_actor_id":player,"actors":[]}
	for actor in data.actors.values():
		if not _skills_match(actor.get("skills"),false): return _fail("InvalidScenarioSkillProfile","/actors/"+actor.id+"/skills")
		var row: Dictionary={"actor_id":actor.id,"place_id":actor.initial_place,"alive":true,"available":true,"attributes":actor.attributes.duplicate(true),"skills":actor.skills.duplicate(true),"combat":actor.get("combat",{}).duplicate(true)}
		if actor.has("initial_age_years"): row.age_years=actor.initial_age_years
		if actor.has("appearance_definition"): row.appearance_definition=actor.appearance_definition.duplicate(true)
		initial.actors.append(row)
	return {"ok":true,"catalog":effective,"initial":initial}

## 显式ID集合必须为非空字符串数组，重复项不能通过自动去重隐藏作者错误。
static func _ids(value: Variant) -> bool:
	if not value is Array: return false
	var seen: Dictionary={}
	for id in value:
		if not id is String or id.is_empty() or seen.has(id): return false
		seen[id]=true
	return true

## 作者整数不可由布尔值或小数替代，避免日期和年龄进入不一致的跨端分支。
static func _whole(value: Variant, low: int, high: int) -> bool:
	return typeof(value)==TYPE_INT and value>=low and value<=high

## 起始日期使用360日与两个半日的明确投影，不参与世界tick初始化。
static func _date(value: Variant) -> bool:
	return value is Dictionary and value.size()==3 and _whole(value.get("year"),1,2147483647) and _whole(value.get("day_of_year"),1,360) and _whole(value.get("half_day"),0,1)

## 返回稳定诊断位置供作者和运行入口共同处理，不抛出半成品世界。
static func _fail(code: String, path: String) -> Dictionary:
	return {"ok":false,"code":code,"path":path}

static var _reference_rules: Dictionary={}
const INITIAL_FIELDS={
	"wallets":["id","owner","initial_balance"],
	"inventories":["id","owner","capacity","goods"],
	"roles":["role_id","actor_id","enabled"],
	"relations":["id","from_actor","to_actor","affinity","trust"],
	"estates":["id","owner_actor_id","manager_actor_id","resident_actor_ids","initial_facilities","initial_duties"],
	"garrisons":["id","organization_id","units"],
	"offices":["id"],"strongholds":["id"]}
const ACTOR_FIELDS=["actor_id","place_id","alive","available","age_years","attributes","skills","combat","appearance_definition"]
const PASSIVE_TABLES=["legacy_control_consent_requests","succession_consent_requests","adult_adoption_requests","insight_cards","teaching_requests","study_teaching_requests","actor_cards","cards","items","dialogues","personal_skills","personal_battlefields","personal_objectives","battlefields","troop_types","military_objectives","siege_topologies","etiquette_challenges","bargain_challenges","drill_challenges","texts"]
const Strict=preload("res://core/strict_json.gd")

## 登记装载器已经验证受信来源的Schema原字节；缓存只持有引用描述，不进入公开目录。
static func register_schema(schema_text: String) -> Dictionary:
	var digest: String=schema_text.sha256_text()
	if _reference_rules.has(digest): return {"ok":true,"schema_sha256":digest}
	var parsed: Dictionary=Strict.parse_text(schema_text)
	if not parsed.ok or not parsed.value is Dictionary: return _fail("InvalidScenarioSchema","/schema_sha256")
	var schema: Dictionary=parsed.value
	if not schema.get("x-hanmo-tables") is Dictionary or not schema.get("properties") is Dictionary or not schema.get("$defs") is Dictionary: return _fail("InvalidScenarioSchema","/schema_sha256")
	var tables: Array=schema["x-hanmo-tables"].keys()
	var rules: Dictionary={}; var defs: Dictionary={}
	for table in tables:
		if not schema.properties.get(table) is Dictionary or not schema.properties[table].get("items") is Dictionary: return _fail("InvalidScenarioSchema","/schema_sha256")
		rules[table]=_reference_rule(schema.properties[table].items)
	for key in schema["$defs"]: defs[key]=_reference_rule(schema["$defs"][key])
	_reference_rules[digest]={"tables":tables,"rules":rules,"defs":defs}
	return {"ok":true,"schema_sha256":digest}

## 丢弃长度和展示注解，仅缓存类型判别与引用边，不复制完整Schema到世界热路径。
static func _reference_rule(rule: Dictionary) -> Dictionary:
	var result: Dictionary={}
	for key in ["type","const","$ref","x-hanmo-ref"]:
		if rule.has(key): result[key]=rule[key]
	for key in ["oneOf","anyOf"]:
		if rule.has(key):
			result[key]=[]
			for child in rule[key]: result[key].append(_reference_rule(child))
	if rule.has("properties"):
		result.properties={}
		for key in rule.properties: result.properties[key]=_reference_rule(rule.properties[key])
	for key in ["items","additionalProperties"]:
		if rule.get(key) is Dictionary: result[key]=_reference_rule(rule[key])
	return result

## 使用Schema的类型与判别常量选择联合分支，不把普通文字误识别成引用。
static func _possible(value: Variant, rule: Dictionary, rules: Dictionary) -> bool:
	if rule.has("$ref"): return _possible(value,rules.defs[str(rule["$ref"]).get_file()],rules)
	if rule.has("const"): return typeof(value)==typeof(rule.const) and value==rule.const
	var types: Dictionary={"object":TYPE_DICTIONARY,"array":TYPE_ARRAY,"string":TYPE_STRING,"integer":TYPE_INT,"boolean":TYPE_BOOL,"null":TYPE_NIL}
	if types.has(rule.get("type")) and typeof(value)!=types[rule.type]: return false
	if value is Dictionary:
		for key in rule.get("properties",{}):
			var child: Dictionary=rule.properties[key]
			if value.has(key) and child.has("const") and value[key]!=child.const: return false
	return true

## 联合及同级公共字段均进入引用闭包；同路径引用去重，局部ID和文本不作实体。
static func _references(value: Variant, rule: Dictionary, rules: Dictionary, path: String="", depth: int=0) -> Array:
	if depth>64: return []
	if rule.has("$ref"): return _references(value,rules.defs[str(rule["$ref"]).get_file()],rules,path,depth+1)
	var result: Array=[]
	for union in ["oneOf","anyOf"]:
		if rule.has(union):
			var options: Array=[]; var literals: Array=[]
			for child in rule[union]:
				if _possible(value,child,rules):
					options.append(child)
					if child.has("const"): literals.append(child)
			for child in options if literals.is_empty() else literals: result.append_array(_references(value,child,rules,path,depth+1))
	if rule.has("x-hanmo-ref") and value is String: result.append([rule["x-hanmo-ref"],value,path])
	if value is Dictionary:
		for key in value:
			var sub: Dictionary=rule.get("properties",{}).get(key,rule.get("additionalProperties",{}))
			result.append_array(_references(value[key],sub,rules,path+"/"+key,depth+1))
	elif value is Array:
		for number in range(value.size()): result.append_array(_references(value[number],rule.get("items",{}),rules,path+"/"+str(number),depth+1))
	var unique: Array=[]
	for edge in result:
		if not edge in unique: unique.append(edge)
	return unique

## 初态逐项使用固定字段，重复ID和任意字段覆盖必须失败，不能自动纠正作者输入。
static func _rows(value: Variant, key: String, fields: Array, path: String, optional_fields: Array=[]) -> Dictionary:
	if not value is Array: return _fail("InvalidScenarioInitial",path)
	var records: Dictionary={}
	for number in range(value.size()):
		var row: Variant=value[number]
		if not row is Dictionary or row.size()<fields.size() or row.size()>fields.size()+optional_fields.size() or not row.get(key) is String or row[key].is_empty(): return _fail("InvalidScenarioInitial",path+"/"+str(number))
		for field in fields:
			if not row.has(field): return _fail("InvalidScenarioInitial",path+"/"+str(number))
		for field in row:
			if not field in fields and not field in optional_fields: return _fail("InvalidScenarioInitial",path+"/"+str(number))
		if records.has(row[key]): return _fail("DuplicateScenarioInitial",path+"/"+str(number))
		records[row[key]]=row.duplicate(true)
	var ids: Array=records.keys(); ids.sort()
	var ordered: Dictionary={}
	for id in ids: ordered[id]=records[id]
	return {"ok":true,"rows":ordered}

## 活动集合与定义闭包分开构造，剧本字段仅投影到独立候选，任何错误不改源。
static func _resolve_snapshot(catalog: Dictionary, scenario: Dictionary, player_actor_id: String, path: String) -> Dictionary:
	if not _reference_rules.has(catalog.get("schema_sha256")): return _fail("ScenarioReferenceRulesMissing","/schema_sha256")
	var rules: Dictionary=_reference_rules[catalog.schema_sha256]
	var source: Dictionary=catalog.data
	var enabled: Variant=scenario.enabled
	var authored_initial: Variant=scenario.initial
	var tables: Array=rules.tables+["texts","flags"]
	if not enabled is Dictionary: return _fail("InvalidScenarioEnabled",path+"/enabled")
	for table in enabled:
		if not table in tables: return _fail("InvalidScenarioEnabled",path+"/enabled")
	var active: Dictionary={}
	for table in tables:
		var ids: Variant=enabled.get(table,[])
		if not _ids(ids): return _fail("InvalidScenarioEnabled",path+"/enabled/"+table)
		for id in ids:
			if not source.get(table,{}).has(id): return _fail("UnknownScenarioEnabled",path+"/enabled/"+table+"/"+id)
		active[table]=ids.duplicate(); active[table].sort()
	active.scenarios=[scenario.id]
	if not authored_initial is Dictionary: return _fail("InvalidScenarioInitial",path+"/initial")
	for key in authored_initial:
		if not key in ["actors","offer_ids"] and not INITIAL_FIELDS.has(key): return _fail("InvalidScenarioInitial",path+"/initial")
	var projected: Dictionary=source.duplicate(true)
	var normalized: Dictionary={}
	var checked: Dictionary=_rows(authored_initial.get("actors"),"actor_id",ACTOR_FIELDS,path+"/initial/actors",["initial_goods"])
	if not checked.ok: return checked
	var people: Dictionary=checked.rows
	for id in people:
		var row: Dictionary=people[id]
		var location: String=path+"/initial/actors/"+id
		if not id in active.actors: return _fail("ScenarioInitialNotEnabled",location)
		if not row.place_id in active.places or not row.alive is bool or not row.available is bool or not _whole(row.age_years,0,120): return _fail("InvalidScenarioActorInitial",location)
		for field in ["attributes","skills","combat","appearance_definition"]:
			if not row[field] is Dictionary: return _fail("InvalidScenarioActorInitial",location)
		if not _skills_match(row.skills,scenario.get("skill_profile","")=="full_sixteen_v1"): return _fail("InvalidScenarioSkillProfile",location+"/skills")
		var goods: Variant=row.get("initial_goods",[])
		if not goods is Array or goods.size()>20: return _fail("InvalidScenarioActorInitial",location+"/initial_goods")
		var checked_goods: Dictionary=_rows(goods,"item_id",["item_id","quantity"],location+"/initial_goods")
		if not checked_goods.ok: return checked_goods
		for good in checked_goods.rows.values():
			if not _whole(good.quantity,1,100): return _fail("InvalidScenarioActorInitial",location+"/initial_goods/"+good.item_id+"/quantity")
		# 新剧本缺省空袋；不能从人物基础定义继承另一年代的初始物资。
		row.initial_goods=checked_goods.rows.values()
		var actor: Dictionary=projected.actors[id]
		actor.initial_goods=row.initial_goods.duplicate(true)
		for pair in [["place_id","initial_place"],["age_years","initial_age_years"],["attributes","attributes"],["skills","skills"],["combat","combat"],["appearance_definition","appearance_definition"]]:
			actor[pair[1]]=row[pair[0]].duplicate(true) if row[pair[0]] is Dictionary else row[pair[0]]
	normalized.actors=people.values()
	var player: String=scenario.player_actor if player_actor_id.is_empty() else player_actor_id
	var creating: bool=scenario.get("dynamic_world",{}).get("founder_policy")=="created_character_v1"
	if creating:
		var creation: Variant=scenario.get("character_creation")
		if not creation is Dictionary or creation.get("profile")!="point_buy_character_v1" or creation.get("prototype_actor_id")!=scenario.player_actor or not source.actors.has(scenario.player_actor) or source.actors[scenario.player_actor].get("origin")!="original" or people.has(scenario.player_actor) or not creation.get("starting_place_id") in active.places or not player_actor_id.is_empty() or not scenario.playable_actor_ids.is_empty(): return _fail("InvalidCharacterCreationCatalog",path+"/character_creation")
	elif not scenario.player_actor in scenario.playable_actor_ids or not player in scenario.playable_actor_ids: return _fail("InvalidPlayableActor",path+"/playable_actor_ids")
	for id in scenario.playable_actor_ids:
		if not people.has(id): return _fail("MissingScenarioInitial",path+"/initial/actors/"+id)
		if not people[id].alive or not people[id].available or (projected.actors[id].has("travel_plan") and not scenario.get("dynamic_world",{}).get("founder_policy","") in ["selected_existing_actor_v1","selected_existing_actor_roster_v1"]): return _fail("InvalidPlayableActor",path+"/playable_actor_ids/"+id)
	if scenario.get("dynamic_world",{}).get("founder_policy","") in ["selected_existing_actor_v1","selected_existing_actor_roster_v1"]:
		var policy_check: Dictionary=existing_actor_policies(scenario,people)
		if not policy_check.ok: return policy_check
	for table in INITIAL_FIELDS:
		var key: String="role_id" if table=="roles" else "id"
		checked=_rows(authored_initial.get(table,[]),key,INITIAL_FIELDS[table],path+"/initial/"+table)
		if not checked.ok: return checked
		var rows: Dictionary=checked.rows
		for id in active[table]:
			if not rows.has(id): return _fail("MissingScenarioInitial",path+"/initial/"+table+"/"+id)
		for id in rows:
			var row: Dictionary=rows[id]
			_order_initial(row,table)
			if not id in active[table]: return _fail("ScenarioInitialNotEnabled",path+"/initial/"+table+"/"+id)
			if table=="roles":
				if not row.enabled is bool or not people.has(row.actor_id): return _fail("MissingScenarioInitial",path+"/initial/roles/"+id)
				projected[table][id].initial_actor=row.actor_id
			else:
				projected[table][id].merge(row.duplicate(true),true)
		normalized[table]=rows.values()
	var offers: Variant=authored_initial.get("offer_ids")
	if not _ids(offers): return _fail("InvalidScenarioOffers",path+"/initial/offer_ids")
	for id in offers:
		if not id in active.offers: return _fail("InvalidScenarioOffers",path+"/initial/offer_ids")
	for id in active.offers:
		if not id in offers: return _fail("MissingScenarioInitial",path+"/initial/offer_ids/"+id)
	normalized.offer_ids=offers.duplicate(); normalized.offer_ids.sort()
	if scenario.has("world_map_id") and not scenario.world_map_id in active.world_maps: return _fail("ScenarioMapNotEnabled",path+"/world_map_id")
	var current: Dictionary=projected.scenarios[scenario.id]
	current.player_actor=player
	current.playable_actor_ids=scenario.playable_actor_ids.duplicate(); current.playable_actor_ids.sort()
	current.enabled=active.duplicate(true)
	current.initial=normalized.duplicate(true)
	var closure: Dictionary=_closure(projected,active,people,rules)
	if not closure.ok: return closure
	var definitions: Dictionary=closure.definitions
	var effective: Dictionary=catalog.duplicate(true)
	var data: Dictionary={"required_capabilities":source.get("required_capabilities",[]).duplicate()}
	for table in tables:
		var ids: Array=definitions[table].keys() if table in PASSIVE_TABLES or table=="actors" else active[table].duplicate()
		ids.sort(); data[table]={}
		for id in ids: data[table][id]=projected[table][id].duplicate(true) if projected[table][id] is Dictionary else projected[table][id]
	data.definitions=definitions
	data.active_entity_ids=active
	data.active_static_actor_ids=people.keys(); data.active_static_actor_ids.sort()
	effective.data=data
	normalized.legacy=false; normalized.player_actor_id=player; normalized.start_date=scenario.start_date.duplicate(true)
	return {"ok":true,"catalog":effective,"initial":normalized}

## 原型旧地点只保留为定义依赖，不能激活另一地图；当前运行资产引用必须显式启用。
static func _closure(data: Dictionary, active: Dictionary, people: Dictionary, rules: Dictionary) -> Dictionary:
	var definitions: Dictionary={}; var queue: Array=[]; var visited: Dictionary={}
	for table in active:
		definitions[table]={}
		for id in active[table]: queue.append([table,id,table!="actors" or people.has(id)])
	var cursor: int=0
	while cursor<queue.size():
		var entry: Array=queue[cursor]; cursor+=1
		var table: String=entry[0]; var id: String=entry[1]; var live: bool=entry[2]
		var key: String=JSON.stringify(entry)
		if visited.has(key): continue
		visited[key]=true
		if not data.get(table,{}).has(id): return _fail("UnknownScenarioReference","/"+table+"/"+id)
		var record: Variant=data[table][id]
		definitions[table][id]=record.duplicate(true) if record is Dictionary else record
		if table in ["texts","flags"]: continue
		for edge in _references(record,rules.rules[table],rules):
			var target_table: String=edge[0]; var target_id: String=edge[1]
			if not definitions.has(target_table) or not data.get(target_table,{}).has(target_id): return _fail("UnknownScenarioReference","/"+table+"/"+id+edge[2])
			var next_live: bool=live and (target_table!="actors" or people.has(target_id))
			if live and not target_table in PASSIVE_TABLES and target_table!="actors" and not target_id in active[target_table]: return _fail("ScenarioDependencyNotEnabled","/"+table+"/"+id+edge[2])
			queue.append([target_table,target_id,next_live])
	for table in definitions:
		var ids: Array=definitions[table].keys(); ids.sort()
		var ordered: Dictionary={}
		for id in ids: ordered[id]=definitions[table][id]
		definitions[table]=ordered
	return {"ok":true,"definitions":definitions}


## 只排序固定的无序初态记录，外貌候选、路线和对白等语义数组仍保持原顺序。
static func _order_initial(row: Dictionary, table: String) -> void:
	if table=="inventories":
		row.goods.sort_custom(func(left,right): return left.item_id<right.item_id)
	elif table=="garrisons":
		row.units.sort_custom(func(left,right): return left.id<right.id)
		for unit in row.units: unit.equipment.sort_custom(func(left,right): return left.item_id<right.item_id)
	elif table=="estates":
		row.resident_actor_ids.sort()
		row.initial_duties.sort_custom(func(left,right): return left.actor_id<right.actor_id)

## 双人旧版与有界名单分别校验；逐人许可不由默认人物推断，未知政策不借此入口放行。
static func existing_actor_policies(scenario: Dictionary, people: Dictionary) -> Dictionary:
	var playable: Variant=scenario.get("playable_actor_ids")
	var policies: Variant=scenario.get("playable_actor_appearance_policies")
	var policy: String=scenario.get("dynamic_world",{}).get("founder_policy","")
	if not policy in ["selected_existing_actor_v1","selected_existing_actor_roster_v1"]: return _fail("UnsupportedFounderPolicy","/dynamic_world/founder_policy")
	if not playable is Array or not _ids(playable) or not scenario.get("player_actor") in playable: return _fail("InvalidPlayableActors","/playable_actor_ids")
	if (policy=="selected_existing_actor_v1" and playable.size()!=2) or (policy=="selected_existing_actor_roster_v1" and (playable.is_empty() or playable.size()>4096)): return _fail("InvalidPlayableActors","/playable_actor_ids")
	if not policies is Array or policies.size()!=playable.size(): return _fail("InvalidPlayableActorAppearancePolicies","/playable_actor_appearance_policies")
	var seen: Dictionary={}
	for row in policies:
		if not row is Dictionary or row.size()!=2 or not row.get("actor_id") in playable or seen.has(row.get("actor_id")) or not row.get("policy") in ["fixed_actor_appearance_composition_v1","preserve_existing_v1"]: return _fail("InvalidPlayableActorAppearancePolicies","/playable_actor_appearance_policies")
		seen[row.actor_id]=row.policy
	for id in playable:
		if not people.get(id) is Dictionary or not people[id].get("alive",false) or not people[id].get("available",false) or not _whole(people[id].get("age_years"),18,120): return _fail("InvalidPlayableActors","/initial/actors/"+id)
	return {"ok":true,"policies":seen}
