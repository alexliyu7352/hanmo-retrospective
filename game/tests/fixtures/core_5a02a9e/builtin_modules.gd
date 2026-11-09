## 唯一可信组合根：固定发行Callable和元数据合成描述，不加载作者脚本或改变世界。
extends RefCounted
const LegacyPhases=preload("res://core/runtime/legacy_phases.gd")
const InteractionObservers=preload("res://core/domains/interaction/interaction_observers.gd")
const MilitaryModule=preload("res://core/domains/military/military_module.gd")
const PartyModule=preload("res://core/domains/travel/party_module.gd")
const TaskModule=preload("res://core/domains/tasks/task_module.gd")
const TaskState=preload("res://core/domains/tasks/task_state.gd")
const EconomyModule=preload("res://core/domains/economy/economy_module.gd")
const Strict=preload("res://core/strict_json.gd")
const Values=preload("res://core/base/value_checks.gd")
const WorldShape=preload("res://core/base/world_shape.gd")
const Validation=preload("res://core/runtime/world_validation.gd")
const InteractionRules=preload("res://core/interaction_rules.gd")
const TravelRules=preload("res://core/travel_rules.gd")
const ServiceRules=preload("res://core/service_rules.gd")
const ActorIdentity=preload("res://core/actor_identity.gd")
const PersonalCombat=preload("res://core/personal_combat.gd")
const SocialRules=preload("res://core/social_rules.gd")
const EstateRules=preload("res://core/estate_rules.gd")
const FamilyRules=preload("res://core/family_rules.gd")
const FamilyCommands=preload("res://core/domains/family/family_commands.gd")
const EstateCommands=preload("res://core/domains/estates/estate_commands.gd")
const MilitaryWorld=preload("res://core/military_world.gd")
const PartyState=preload("res://core/party_state.gd")
const EncounterState=preload("res://core/encounter_state.gd")
const CultivationRules=preload("res://core/cultivation_rules.gd")
const CultivationModule=preload("res://core/domains/cultivation/cultivation_module.gd")
const LongRest=preload("res://core/long_rest.gd")
const CONTRACT_PATH="res://data/module_contracts.json"
const CAPABILITY_PATH="res://data/rule_capabilities.json"

## 固定发行绑定表；合同中的path仅作比对，绝不传入load/preload。
static func _bindings() -> Dictionary:
	return {
		"interaction.roles":{"owner":"interaction","path":"game/core/interaction_rules.gd","method":"validate_roles","stage":"before_actions","order":10,"signature":"state,data","callback":InteractionRules.validate_roles},
		"economy.state":{"owner":"economy","path":"game/core/domains/economy/economy_module.gd","method":"validate_state","stage":"before_actions","order":20,"signature":"state,data","callback":EconomyModule.validate_state},
		"interaction.flags":{"owner":"interaction","path":"game/core/interaction_rules.gd","method":"validate_flags","stage":"before_actions","order":30,"signature":"state,data","callback":InteractionRules.validate_flags},
		"social.relations":{"owner":"social","path":"game/core/social_rules.gd","method":"validate_relations","stage":"before_actions","order":40,"signature":"state,data","callback":SocialRules.validate_relations},
		"runtime.state":{"owner":"runtime","path":"game/core/base/world_shape.gd","method":"validate_runtime","stage":"before_actions","order":50,"signature":"state,data","callback":WorldShape.validate_runtime},
		"personal.state":{"owner":"personal","path":"game/core/personal_combat.gd","method":"validate_state","stage":"before_actions","order":60,"signature":"state,data","callback":PersonalCombat.validate_state},
		"military.state":{"owner":"military","path":"game/core/military_world.gd","method":"validate_state","stage":"before_actions","order":70,"signature":"state,data","callback":MilitaryWorld.validate_state},
		"social.state":{"owner":"social","path":"game/core/social_rules.gd","method":"validate_state","stage":"before_actions","order":80,"signature":"state,data","callback":SocialRules.validate_state},
		"identity.state":{"owner":"identity","path":"game/core/actor_identity.gd","method":"validate","stage":"before_actions","order":90,"signature":"state,data","callback":ActorIdentity.validate},
		"family.state":{"owner":"family","path":"game/core/family_rules.gd","method":"validate_header","stage":"before_actions","order":100,"signature":"state,data","callback":FamilyRules.validate_header},
		"estates.state":{"owner":"estates","path":"game/core/estate_rules.gd","method":"validate_state","stage":"before_actions","order":110,"signature":"state,data","callback":EstateRules.validate_state},
		"family.tail":{"owner":"family","path":"game/core/family_rules.gd","method":"validate_tail","stage":"before_actions","order":120,"signature":"state,data","callback":FamilyRules.validate_tail},
		"long_rest.state":{"owner":"services","path":"game/core/long_rest.gd","method":"validate_state","stage":"before_actions","order":130,"signature":"state,data","callback":LongRest.validate_state},
		"tasks.state":{"owner":"tasks","path":"game/core/domains/tasks/task_state.gd","method":"validate","stage":"before_actions","order":140,"signature":"state,data","callback":TaskState.validate},
		"parties.state":{"owner":"travel","path":"game/core/party_state.gd","method":"validate","stage":"after_actions","order":10,"signature":"state,data","callback":PartyState.validate},
		"encounters.state":{"owner":"interaction","path":"game/core/encounter_state.gd","method":"validate_encounters","stage":"after_actions","order":20,"signature":"state,data","callback":EncounterState.validate_encounters},
		"itineraries.state":{"owner":"travel","path":"game/core/encounter_state.gd","method":"validate_itineraries","stage":"after_actions","order":30,"signature":"state,data","callback":EncounterState.validate_itineraries},
		"interaction.sessions":{"owner":"interaction","path":"game/core/interaction_rules.gd","method":"validate_sessions","stage":"after_actions","order":40,"signature":"state,data","callback":InteractionRules.validate_sessions},
	}

## 过渡载荷绑定只有三参validate；不注册命令或伪造生命周期。
static func _legacy_bindings() -> Dictionary:
	return {
		"wait":{"owner":"runtime","path":"game/core/base/world_shape.gd","method":"validate_wait","signature":"state,data,action","validate":WorldShape.validate_wait},
		"travel":{"owner":"travel","path":"game/core/travel_rules.gd","method":"validate_action","signature":"state,data,action","validate":TravelRules.validate_action},
		"rescue":{"owner":"travel","path":"game/core/travel_rules.gd","method":"validate_rescue","signature":"state,data,action","validate":TravelRules.validate_rescue},
		"service":{"owner":"services","path":"game/core/service_rules.gd","method":"validate_action","signature":"state,data,action","validate":ServiceRules.validate_action},
		"long_rest":{"owner":"services","path":"game/core/long_rest.gd","method":"validate_action","signature":"state,data,action","validate":LongRest.validate_action},
		"estate_build":{"owner":"estates","path":"game/core/estate_rules.gd","method":"validate_action","signature":"state,data,action","validate":EstateRules.validate_action},
		"family_adopt":{"owner":"family","path":"game/core/family_rules.gd","method":"validate_action","signature":"state,data,action","validate":FamilyRules.validate_action},
		"learn_skill":{"owner":"cultivation","path":"game/core/cultivation_rules.gd","method":"validate_action","signature":"state,data,action","validate":CultivationRules.validate_action},
		"practice_skill":{"owner":"cultivation","path":"game/core/cultivation_rules.gd","method":"validate_action","signature":"state,data,action","validate":CultivationRules.validate_action},
		"teach_skill":{"owner":"cultivation","path":"game/core/cultivation_rules.gd","method":"validate_action","signature":"state,data,action","validate":CultivationRules.validate_action},
	}

## 固定拥有权由独立纯合同提供，不从传入metadata反推必需覆盖。
static func _required_ownership() -> Array:
	return Validation.required_ownership()

## 描述附带要求仅供注册诊断，真实完整门仍独立核对固定合同。
static func _required_validators() -> Array:
	return Validation.required_validators()

## 验证旧能力文件ID到语义ID的一一映射及固定验证器；失配返回坏描述以关闭启动。
static func validate_metadata(contract: Dictionary, capability: Dictionary) -> Dictionary:
	if contract.get("format")!="HM-MODULE-CONTRACTS/1" or contract.get("version_rule")!="exact" or not contract.get("modules") is Array or contract.modules.is_empty() or not contract.get("capability_modules") is Dictionary or not capability.get("modules") is Dictionary or not capability.get("capabilities") is Array: return {"ok":false,"code":"InvalidBuiltinMetadata"}
	var modules: Dictionary={}
	var bindings: Dictionary=_bindings()
	var seen_validators: Array=[]
	var legacy: Dictionary=_legacy_bindings()
	var seen_legacy: Array=[]
	var claims: Dictionary={}
	for module in contract.modules:
		if not Values.fields(module,{"id":TYPE_STRING,"version":TYPE_STRING,"requires":TYPE_DICTIONARY,"optional_requires":TYPE_DICTIONARY,"capabilities":TYPE_ARRAY,"owns":TYPE_ARRAY,"validators":TYPE_ARRAY,"legacy_action_validators":TYPE_ARRAY}) or modules.has(module.id): return {"ok":false,"code":"InvalidBuiltinMetadata"}
		modules[module.id]=module.duplicate(true)
		for validator in module.validators:
			if not Values.fields(validator,{"id":TYPE_STRING,"path":TYPE_STRING,"method":TYPE_STRING,"signature":TYPE_STRING,"stage":TYPE_STRING,"order":TYPE_INT}) or not bindings.has(validator.id) or validator.id in seen_validators: return {"ok":false,"code":"InvalidBuiltinValidator"}
			for key in ["path","method","signature","stage","order"]:
				if validator[key]!=bindings[validator.id][key]: return {"ok":false,"code":"InvalidBuiltinValidator"}
			if module.id!=bindings[validator.id].owner: return {"ok":false,"code":"InvalidBuiltinValidator"}
			seen_validators.append(validator.id)
		for entry in module.legacy_action_validators:
			if not Values.fields(entry,{"kind":TYPE_STRING,"path":TYPE_STRING,"method":TYPE_STRING,"signature":TYPE_STRING}) or not legacy.has(entry.kind) or entry.kind in seen_legacy: return {"ok":false,"code":"InvalidBuiltinValidator"}
			for key in ["path","method","signature"]:
				if entry[key]!=legacy[entry.kind][key]: return {"ok":false,"code":"InvalidBuiltinValidator"}
			if module.id!=legacy[entry.kind].owner: return {"ok":false,"code":"InvalidBuiltinValidator"}
			seen_legacy.append(entry.kind)
		for claim in module.owns:
			if not Values.fields(claim,{"container":TYPE_STRING,"scope":TYPE_STRING,"key":TYPE_STRING,"owner":TYPE_STRING}): return {"ok":false,"code":"InvalidStateOwnership"}
			var key: String=JSON.stringify([claim.container,claim.scope,claim.key])
			if claims.has(key): return {"ok":false,"code":"DuplicateStateOwner"}
			claims[key]=claim.owner

	for claim in _required_ownership():
		if claims.get(JSON.stringify([claim.container,claim.scope,claim.key]))!=claim.owner: return {"ok":false,"code":"MissingStateOwner"}
	if seen_legacy.size()!=legacy.size(): return {"ok":false,"code":"InvalidBuiltinValidator"}
	if seen_validators.size()!=bindings.size(): return {"ok":false,"code":"InvalidBuiltinValidator"}
	if contract.capability_modules.size()!=capability.modules.size(): return {"ok":false,"code":"InvalidCapabilityModuleMapping"}
	var expected: Dictionary={}
	for id in modules: expected[id]=[]
	for old_id in capability.modules:
		if not contract.capability_modules.has(old_id) or not contract.capability_modules[old_id] is String or not modules.has(contract.capability_modules[old_id]): return {"ok":false,"code":"InvalidCapabilityModuleMapping"}
	for entry in capability.capabilities:
		if not Values.fields(entry,{"capability_id":TYPE_STRING,"module_id":TYPE_STRING,"rule_version":TYPE_STRING}) or not contract.capability_modules.has(entry.module_id): return {"ok":false,"code":"InvalidCapabilityModuleMapping"}
		var target: String=contract.capability_modules[entry.module_id]
		if modules[target].version!=entry.rule_version or entry.capability_id in expected[target]: return {"ok":false,"code":"InvalidCapabilityModuleMapping"}
		expected[target].append(entry.capability_id)
	for id in modules:
		var declared: Array=modules[id].capabilities.duplicate(); declared.sort(); expected[id].sort()
		if declared!=expected[id]: return {"ok":false,"code":"InvalidCapabilityModuleMapping"}
	return {"ok":true}

## 每次从发行元数据构造新描述；经济、任务和军事人产合入唯一既有拥有权，其他旧命令保持显式兼容。
static func descriptors() -> Array:
	var parsed: Dictionary=Strict.parse_text(FileAccess.get_file_as_string(CONTRACT_PATH))
	var capabilities: Dictionary=Strict.parse_text(FileAccess.get_file_as_string(CAPABILITY_PATH))
	if not parsed.ok or not parsed.value is Dictionary or not capabilities.ok or not capabilities.value is Dictionary: return [{}]
	if not validate_metadata(parsed.value,capabilities.value).ok: return [{}]
	var result: Array=[]
	var bindings: Dictionary=_bindings()
	for metadata in parsed.value.modules:
		var descriptor: Dictionary=metadata.duplicate(true)
		for key in ["commands","actions","phases","observers","queries"]: descriptor[key]=[]
		if descriptor.id=="builtin":
			descriptor.phases=LegacyPhases.descriptors()
			descriptor.required_ownership=_required_ownership()
			descriptor.required_validators=_required_validators()
		if descriptor.id=="interaction":
			descriptor.observers=InteractionObservers.descriptors()
		if descriptor.id=="economy":
			var economy: Dictionary=EconomyModule.descriptor()
			descriptor.commands=economy.commands
			descriptor.actions=economy.actions
		if descriptor.id=="travel":
			var party: Dictionary=PartyModule.descriptor()
			descriptor.commands=party.commands
		if descriptor.id=="tasks":
			var tasks: Dictionary=TaskModule.descriptor()
			descriptor.commands=tasks.commands
			descriptor.actions=tasks.actions
		if descriptor.id=="cultivation":
			descriptor.commands=CultivationModule.descriptor().commands
		if descriptor.id=="family":
			descriptor.commands=FamilyCommands.descriptor().commands
		if descriptor.id=="estates":
			descriptor.commands=EstateCommands.descriptor().commands
		if descriptor.id=="military":
			var military: Dictionary=MilitaryModule.descriptor()
			descriptor.commands=military.commands
			descriptor.actions=military.actions
		for validator in descriptor.validators: validator.callback=bindings[validator.id].callback
		var legacy: Dictionary=_legacy_bindings()
		for entry in descriptor.legacy_action_validators: entry.validate=legacy[entry.kind].validate
		result.append(descriptor)
	return result

## 已迁移命令由各领域原集合独立锚定；组合根串联固定合同，不收紧可单独使用的泛型Registry。
static func command_contract_status(registry: Dictionary) -> Dictionary:
	var complete: Dictionary=Validation.contract_status(registry)
	if not complete.ok: return complete
	var military: Dictionary=MilitaryModule.command_contract_status(registry)
	if not military.ok: return military
	var party: Dictionary=PartyModule.command_contract_status(registry)
	if not party.ok: return party
	var cultivation: Dictionary=CultivationModule.command_contract_status(registry)
	if not cultivation.ok: return cultivation
	var family: Dictionary=FamilyCommands.command_contract_status(registry)
	if not family.ok: return family
	return EstateCommands.command_contract_status(registry)

## 必需事件观察保留唯一业务链首；只有通用观察图和命令合同先通过才允许开局。
static func observer_contract_status(registry: Dictionary) -> Dictionary:
	return InteractionObservers.contract_status(registry)
