## 已知规则版本的显式迁移；原文件不写入，不补造人物、队伍或已发生事件。
extends RefCounted
const Appearance=preload("res://core/domains/identity/actor_appearance.gd")
const CURRENT: String="f10-appearance-1"
const ARCHITECTURE: String="f10-architecture-2"
const ARCHITECTURE_PREVIOUS: String="f10-architecture-1"
const MILITARY: String="f09-military-1"
const FAMILY: String="f08-family-1"
const WORLD: String="f07-world-1"
const SOCIAL: String="f06-social-1"
const PERSONAL: String="f05-personal-1"
const COMPANIONS: String="f04-companions-1"
const TRAVEL: String="f03-travel-1"
const LEGACY: String="f02-1"
## 先完成既有规则链再给无外貌记录的旧人标legacy；混合新字段拒绝，不从当前作者定义或RNG补脸。
static func upgrade(source: Dictionary) -> Dictionary:
	if source.get("ruleset_id")==CURRENT: return {"ok":true,"state":source.duplicate(true),"migrated":false}
	var upgraded: Dictionary=_upgrade_architecture(source)
	if not upgraded.ok: return upgraded
	var candidate: Dictionary=upgraded.state
	if not candidate.get("actors") is Dictionary: return {"ok":false,"code":"InvalidWorldShape"}
	for actor in candidate.actors.values():
		if not actor is Dictionary: return {"ok":false,"code":"InvalidActor"}
		if actor.has("appearance"): return {"ok":false,"code":"AmbiguousLegacyAppearance"}
		actor.appearance=Appearance.legacy()
	candidate.ruleset_id=CURRENT
	return {"ok":true,"state":candidate,"migrated":true,"source_ruleset":source.ruleset_id}

## 只在独立候选中补入旧版本不存在的空集合；混合标识的新事实不能被静默抹掉。
static func _upgrade_architecture(source: Dictionary) -> Dictionary:
	if source.get("ruleset_id")==ARCHITECTURE: return {"ok":true,"state":source.duplicate(true),"migrated":false}
	# 旧标记不得夹带新增动态授权；迁移只换版本，不补授权、行程或历史。
	if _has_dynamic_schedule(source): return {"ok":false,"code":"AmbiguousLegacyActorSchedule"}
	# 前一完整规则仅改版本，绝不借旧军事迁移抹除权威，也不在读档时取消事件。
	if source.get("ruleset_id") in [ARCHITECTURE_PREVIOUS,MILITARY]:
		var candidate: Dictionary=source.duplicate(true)
		candidate.ruleset_id=ARCHITECTURE
		return {"ok":true,"state":candidate,"migrated":true,"source_ruleset":source.ruleset_id}
	for key in ["offices","garrisons","military_units","armies","campaigns","strongholds","sieges"]:
		if source.has(key) and source[key]!={}: return {"ok":false,"code":"AmbiguousLegacyMilitary"}
	if not source.get("facts",{}) is Dictionary: return {"ok":false,"code":"InvalidWorldShape"}
	for fact in source.get("facts",{}).values():
		if fact is Dictionary and (str(fact.get("kind","")).begins_with("military_") or str(fact.get("kind","")).begins_with("campaign_")): return {"ok":false,"code":"AmbiguousLegacyMilitary"}
	var migrated: Dictionary={"ok":true,"state":source.duplicate(true),"migrated":true,"source_ruleset":FAMILY} if source.get("ruleset_id")==FAMILY else _upgrade_family(source)
	if not migrated.ok: return migrated
	migrated.state.ruleset_id=ARCHITECTURE
	for key in ["offices","garrisons","military_units","armies","campaigns","strongholds","sieges"]: migrated.state[key]={}
	return migrated

## 保留既有个人、地形与家庭迁移边界；外层只补军事空集合，不抹掉旧事实。
static func _upgrade_family(source: Dictionary) -> Dictionary:
	var version: Variant=source.get("ruleset_id")
	if version==ARCHITECTURE: return {"ok":true,"state":source.duplicate(true),"migrated":false}
	if not version in [LEGACY,TRAVEL,COMPANIONS,PERSONAL,SOCIAL,WORLD]: return {"ok":false,"code":"UnsupportedRuleset"}
	if not source.get("actions") is Dictionary: return {"ok":false,"code":"InvalidWorldShape"}
	var empty_family: Dictionary={"founder_actor_id":source.get("player_actor_id",""),"children":{},"designations":{},"successions":[]}
	if source.has("family") and source.family!=empty_family: return {"ok":false,"code":"AmbiguousLegacyFamily"}
	if not source.get("facts",{}) is Dictionary: return {"ok":false,"code":"InvalidWorldShape"}
	for fact in source.get("facts",{}).values():
		if fact is Dictionary and (fact.get("kind")=="long_rest" or (str(fact.get("kind","")).begins_with("family_") and fact.has("terms"))): return {"ok":false,"code":"AmbiguousLegacyFamily"}
	if version==WORLD:
		var upgraded: Dictionary=source.duplicate(true)
		upgraded.ruleset_id=ARCHITECTURE; upgraded.family=empty_family
		return {"ok":true,"state":upgraded,"migrated":true,"source_ruleset":version}
	if source.has("estates") and source.estates!={}: return {"ok":false,"code":"AmbiguousLegacyEstates"}
	for action in source.actions.values():
		if action is Dictionary and action.get("params",{}).get("terrain",false): return {"ok":false,"code":"AmbiguousLegacyTerrain"}
	for actor in source.get("actors",{}).values():
		if actor is Dictionary and actor.get("location",{}).get("kind")=="Wilderness": return {"ok":false,"code":"AmbiguousLegacyTerrain"}
	if version==SOCIAL:
		var upgraded: Dictionary=source.duplicate(true)
		upgraded.ruleset_id=ARCHITECTURE; upgraded.estates={}; upgraded.family=empty_family
		return {"ok":true,"state":upgraded,"migrated":true,"source_ruleset":version}
	var empty_social: Dictionary={"last_tick":source.get("tick",0),"memories":{},"events":{}}
	if source.has("social") and source.social!=empty_social: return {"ok":false,"code":"AmbiguousLegacySocial"}
	if version==PERSONAL:
		var upgraded: Dictionary=source.duplicate(true)
		upgraded.ruleset_id=ARCHITECTURE; upgraded.social=empty_social; upgraded.estates={}; upgraded.family=empty_family
		return {"ok":true,"state":upgraded,"migrated":true,"source_ruleset":version}
	for key in ["battles","facts"]:
		if source.has(key) and (not source[key] is Dictionary or not source[key].is_empty()): return {"ok":false,"code":"AmbiguousLegacyCombat"}
	for key in ["parties","encounters","itineraries"]:
		if version!=COMPANIONS and source.has(key) and (not source[key] is Dictionary or not source[key].is_empty()): return {"ok":false,"code":"AmbiguousLegacyMobility"}
	for action in source.actions.values():
		if not action is Dictionary or not action.get("params") is Dictionary: return {"ok":false,"code":"InvalidAction"}
		if version==LEGACY and (action.params.has("paused") or action.params.has("reverse_first")): return {"ok":false,"code":"AmbiguousLegacyJourney"}
		if version!=COMPANIONS and (action.params.get("encounter_id","")!="" or action.params.get("met_actions",[])!=[]): return {"ok":false,"code":"AmbiguousLegacyMobility"}
	var candidate: Dictionary=source.duplicate(true)
	candidate.ruleset_id=ARCHITECTURE
	if version!=COMPANIONS:
		for key in ["parties","encounters","itineraries"]: candidate[key]={}
	candidate.battles={}; candidate.facts={}
	candidate.social=empty_social
	candidate.estates={}
	candidate.family=empty_family
	if not candidate.get("actors") is Dictionary: return {"ok":false,"code":"InvalidWorldShape"}
	for actor in candidate.actors.values():
		if not actor is Dictionary: return {"ok":false,"code":"InvalidActor"}
		if actor.has("combat") and actor.combat!={"weapon_id":"","learned":{},"heart_id":""}: return {"ok":false,"code":"AmbiguousLegacyCombat"}
		if actor.get("injury",0)!=0: return {"ok":false,"code":"AmbiguousLegacyCombat"}
		actor.combat={"weapon_id":"","learned":{},"heart_id":""}; actor.injury=0
	return {"ok":true,"state":candidate,"migrated":true,"source_ruleset":version}

## 拒绝新授权伪装旧规则；只识别新增字段和有创建事实的行程，不替旧档补修。
static func _has_dynamic_schedule(source: Dictionary) -> bool:
	if not source.get("facts",{}) is Dictionary: return false
	for fact in source.get("facts",{}).values():
		if not fact is Dictionary: continue
		var terms: Variant=fact.get("terms")
		if terms is Dictionary and terms.get("policy") is Dictionary and terms.policy.has("enable_travel_plan"): return true
		if fact.get("kind")=="actor_creation" and source.get("itineraries",{}) is Dictionary and source.itineraries.has(fact.get("actor_id")): return true
	if source.get("actions") is Dictionary:
		for action in source.actions.values():
			if not action is Dictionary or not action.get("params") is Dictionary: continue
			var terms: Variant=action.params.get("training")
			if terms is Dictionary and terms.get("policy") is Dictionary and terms.policy.has("enable_travel_plan"): return true
	return false
