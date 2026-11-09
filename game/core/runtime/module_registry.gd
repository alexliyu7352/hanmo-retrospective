## 只检查和收集受信描述；不调用回调、不构造世界，也不加载具体领域或作者脚本。
extends RefCounted
const WorldPhases=preload("res://core/runtime/world_phases.gd")
const DomainObservers=preload("res://core/runtime/domain_observers.gd")
const Values=preload("res://core/base/value_checks.gd")
const Validation=preload("res://core/runtime/world_validation.gd")

## 无中断能力的行动必须提供这个明确拒绝处理器，不冒充成功取消。
static func refuse_cancel(_state: Dictionary={}, _data: Dictionary={}, _action: Dictionary={}, _reason: String="") -> Dictionary:
	return {"ok":false,"code":"ActionNotInterruptible"}

## 无恢复能力的行动必须提供这个明确拒绝处理器，不丢失未完成生命周期。
static func refuse_resume(_state: Dictionary={}, _data: Dictionary={}, _action: Dictionary={}, _params: Dictionary={}) -> Dictionary:
	return {"ok":false,"code":"ActionNotResumable"}

## 错误码稳定，细节只供诊断，失败不返回部分可运行注册结果。
static func _fail(code: String, detail: String="") -> Dictionary:
	return {"ok":false,"code":code,"detail":detail}

## 协议标识必须是非空字符串，不把其他类型强制变成名称。
static func _name(value: Variant) -> bool:
	return value is String and not value.is_empty()

## 只检查Callable有效性，注册期间绝不探测执行回调。
static func _callable(value: Variant) -> bool:
	return value is Callable and value.is_valid()

## 检查拥有权的固定四种范围；不是任意选择器语言，也不是静态写屏障。
static func _ownership(modules: Dictionary) -> Dictionary:
	var seen: Dictionary={}; var roots: Dictionary={}; var containers: Dictionary={}
	for module_id in modules:
		for claim in modules[module_id].owns:
			if not Values.fields(claim,{"container":TYPE_STRING,"scope":TYPE_STRING,"key":TYPE_STRING,"owner":TYPE_STRING,"schema_refs":TYPE_ARRAY}): return _fail("InvalidStateOwnership",module_id)
			if not _name(claim.container) or not claim.scope in ["root","envelope","kind","actor_field"] or claim.schema_refs.is_empty(): return _fail("InvalidStateOwnership",module_id)
			if claim.scope in ["root","envelope"] and not claim.key.is_empty(): return _fail("InvalidStateOwnership",module_id)
			if claim.scope in ["kind","actor_field"] and not _name(claim.key): return _fail("InvalidStateOwnership",module_id)
			if claim.scope=="actor_field" and claim.container!="actors": return _fail("InvalidStateOwnership",module_id)
			if not modules.has(claim.owner): return _fail("UnknownStateOwner",claim.owner)
			if claim.owner!=module_id: return _fail("MismatchedStateOwner",module_id)
			for reference in claim.schema_refs:
				if not Values.fields(reference,{"path":TYPE_STRING,"method":TYPE_STRING}) or not _name(reference.path) or not _name(reference.method): return _fail("InvalidStateOwnership",module_id)
			var key: String=JSON.stringify([claim.container,claim.scope,claim.key])
			if seen.has(key): return _fail("DuplicateStateOwner",key)
			if roots.has(claim.container) or (claim.scope=="root" and containers.has(claim.container)): return _fail("OverlappingStateOwner",claim.container)
			seen[key]=module_id; containers[claim.container]=true
			if claim.scope=="root": roots[claim.container]=true
	return {"ok":true}

## 校验精确版本与依赖图，按拓扑和同位ID生成确定性顺序，不改变世界执行顺序。
static func _order(modules: Dictionary) -> Dictionary:
	var dependencies: Dictionary={}
	for id in modules:
		dependencies[id]=[]
		for field in ["requires","optional_requires"]:
			for dependency in modules[id][field]:
				if not _name(dependency) or not _name(modules[id][field][dependency]): return _fail("InvalidModuleDescriptor",id)
				if not modules.has(dependency):
					if field=="requires": return _fail("UnknownModuleDependency",dependency)
					continue
				if modules[dependency].version!=modules[id][field][dependency]: return _fail("IncompatibleModuleVersion",dependency)
				if not dependency in dependencies[id]: dependencies[id].append(dependency)
	var ordered: Array=[]
	while ordered.size()<modules.size():
		var ready: Array=[]
		for id in modules:
			if id in ordered: continue
			var satisfied: bool=true
			for dependency in dependencies[id]:
				if not dependency in ordered: satisfied=false
			if satisfied: ready.append(id)
		if ready.is_empty(): return _fail("ModuleDependencyCycle")
		ready.sort()
		# 每次取最小可用ID，独立于输入字典的插入顺序。
		ordered.append(ready[0])
	return {"ok":true,"order":ordered}

## 校验行动的完整生命周期与中断声明；缺失或假拒绝均阻断注册。
static func _action(entry: Variant) -> bool:
	if not entry is Dictionary or not _name(entry.get("kind")) or not Values.fields(entry.get("interrupt_policy"),{"cancel":TYPE_STRING,"resume":TYPE_STRING}): return false
	for method in ["prepare","start","step","finish","cancel","resume","validate"]:
		if not _callable(entry.get(method)): return false
	return true

## 集合以ID排序后拷贝，不暴露调用者可变描述的引用。
static func _sorted(collection: Dictionary) -> Dictionary:
	var keys: Array=collection.keys(); keys.sort()
	var result: Dictionary={}
	for key in keys: result[key]=collection[key]
	return result

## 输入只能来自发行组合根；形状、依赖和所有权通过后才返回完整注册表。
static func build(descriptors: Array) -> Dictionary:
	if descriptors.is_empty(): return _fail("InvalidModuleDescriptor")
	var required: Dictionary={"id":TYPE_STRING,"version":TYPE_STRING,"requires":TYPE_DICTIONARY,"optional_requires":TYPE_DICTIONARY,"capabilities":TYPE_ARRAY,"owns":TYPE_ARRAY,"commands":TYPE_ARRAY,"actions":TYPE_ARRAY,"phases":TYPE_ARRAY,"validators":TYPE_ARRAY,"queries":TYPE_ARRAY}
	var modules: Dictionary={}
	for descriptor in descriptors:
		if not Values.fields(descriptor,required): return _fail("InvalidModuleDescriptor")
		if not _name(descriptor.id) or not _name(descriptor.version) or not descriptor.get("observers",[]) is Array or not descriptor.get("legacy_action_validators",[]) is Array or not descriptor.get("required_ownership",[]) is Array or not descriptor.get("required_validators",[]) is Array: return _fail("InvalidModuleDescriptor")
		if descriptor.has("required_ownership") or descriptor.has("required_validators"):
			if descriptor.get("required_ownership",[]).is_empty() or descriptor.get("required_validators",[]).is_empty(): return _fail("InvalidModuleDescriptor",descriptor.id)
		if modules.has(descriptor.id): return _fail("DuplicateModule",descriptor.id)
		var capabilities: Array=[]
		for capability in descriptor.capabilities:
			if not _name(capability) or capability in capabilities: return _fail("InvalidModuleDescriptor",descriptor.id)
			capabilities.append(capability)
		for dependency in descriptor.requires:
			if descriptor.optional_requires.has(dependency): return _fail("InvalidModuleDescriptor",descriptor.id)
		modules[descriptor.id]=descriptor.duplicate(true)
	var order: Dictionary=_order(modules)
	if not order.ok: return order
	var ownership: Dictionary=_ownership(modules)
	if not ownership.ok: return ownership
	var result: Dictionary={"ok":true,"modules":{},"commands":{},"actions":{},"phases":{},"validators":{},"legacy_action_validators":{},"observers":{},"queries":{}}
	for id in order.order:
		var descriptor: Dictionary=modules[id]
		result.modules[id]=descriptor
		for entry in descriptor.commands:
			if not entry is Dictionary or not _name(entry.get("kind")) or not entry.get("mode") in ["instant","action"] or not _name(entry.get("gate_id")) or not _callable(entry.get("preview")) or not entry.has("apply"): return _fail("InvalidCommandDescriptor",id)
			if (entry.mode=="instant" and not _callable(entry.apply)) or (entry.apply!=null and not _callable(entry.apply)): return _fail("InvalidCommandDescriptor",id)
			# 兼容只声明旧行动启动桥；门位与生命周期不能借此伪装或放宽。
			if entry.has("compat_action") and (not _name(entry.compat_action) or entry.compat_action!="legacy_begin_action" or entry.mode!="action" or entry.apply!=null): return _fail("InvalidCompatActionDescriptor",entry.kind)
			if result.commands.has(entry.kind): return _fail("DuplicateCommand",entry.kind)
			var registered: Dictionary=entry.duplicate(true); registered.owner=id
			result.commands[entry.kind]=registered
		for entry in descriptor.actions:
			if not _action(entry): return _fail("InvalidActionDescriptor",id)
			for method in ["cancel","resume"]:
				var policy: String=entry.interrupt_policy[method]
				var refusal: Callable=refuse_cancel if method=="cancel" else refuse_resume
				if not policy in ["supported","unsupported"] or (policy=="unsupported" and entry[method]!=refusal) or (policy=="supported" and entry[method]==refusal): return _fail("InvalidInterruptPolicy",entry.kind)
			if result.actions.has(entry.kind): return _fail("DuplicateAction",entry.kind)
			var registered: Dictionary=entry.duplicate(true); registered.owner=id
			result.actions[entry.kind]=registered
		for entry in descriptor.phases:
			if not entry is Dictionary or not _name(entry.get("id")) or (entry.has("owner") and entry.owner!=id): return _fail("InvalidPhaseHook",id)
			if result.phases.has(entry.id): return _fail("DuplicatePhaseId",entry.id)
			var registered: Dictionary=entry.duplicate(true); registered.owner=id
			result.phases[entry.id]=registered
		for entry in descriptor.validators:
			if not Values.fields(entry,{"id":TYPE_STRING,"stage":TYPE_STRING,"order":TYPE_INT,"signature":TYPE_STRING,"callback":TYPE_CALLABLE}) or not _name(entry.id) or not entry.stage in ["before_actions","after_actions"] or entry.order<0 or entry.signature!="state,data" or not _callable(entry.callback) or entry.callback.get_argument_count()!=2: return _fail("InvalidStateValidator",id)
			if result.validators.has(entry.id): return _fail("DuplicateModuleCallback",entry.id)
			var registered: Dictionary=entry.duplicate(true); registered.owner=id
			result.validators[entry.id]=registered
		for entry in descriptor.get("legacy_action_validators",[]):
			if not Values.fields(entry,{"kind":TYPE_STRING,"signature":TYPE_STRING,"validate":TYPE_CALLABLE}) or not _name(entry.kind) or entry.signature!="state,data,action" or not _callable(entry.validate) or entry.validate.get_argument_count()!=3: return _fail("InvalidLegacyActionValidator",id)
			if result.legacy_action_validators.has(entry.kind): return _fail("DuplicateActionValidator",entry.kind)
			var registered: Dictionary=entry.duplicate(true); registered.owner=id
			result.legacy_action_validators[entry.kind]=registered
		for collection in ["observers","queries"]:
			for entry in descriptor.get(collection,[]):
				if not entry is Dictionary or not _name(entry.get("id")) or not _callable(entry.get("callback")): return _fail("InvalidModuleCallback",collection)
				if result[collection].has(entry.id): return _fail("DuplicateModuleCallback",entry.id)
				var registered: Dictionary=entry.duplicate(true); registered.owner=id
				result[collection][entry.id]=registered
	for kind in result.commands:
		var command: Dictionary=result.commands[kind]
		if command.has("compat_action"):
			var legacy: Dictionary=result.legacy_action_validators.get(kind,{})
			if legacy.get("owner")!=command.owner or result.actions.has(kind): return _fail("UnknownCompatAction",kind)
		elif command.mode=="action" and (not result.actions.has(kind) or result.actions[kind].owner!=command.owner): return _fail("UnknownCommandAction",kind)
	for collection in ["commands","actions","phases","validators","legacy_action_validators","observers","queries"]: result[collection]=_sorted(result[collection])
	var coverage: Dictionary=_validation_coverage(result)
	if not coverage.ok: return coverage
	if result.modules.has(Validation.COMPOSITION_ID):
		var complete: Dictionary=Validation.contract_status(result)
		if not complete.ok: return complete
	var validation: Dictionary=_validation_plan(result.validators)
	if not validation.ok: return validation
	result.validation_plan=validation.plan
	result.validation_plan.required=[]
	for module in result.modules.values(): result.validation_plan.required.append_array(module.get("required_validators",[]).duplicate(true))
	var compiled: Dictionary=WorldPhases.compile(result.phases.values())
	if not compiled.ok: return compiled
	result.phase_plan=compiled
	var observed: Dictionary=DomainObservers.compile(result.observers.values())
	if not observed.ok: return observed
	result.observer_plan=observed
	return result

## 只编译两段严格顺序；同段重复位置拒绝，不靠模块或ID排序决定首错。
static func _validation_plan(validators: Dictionary) -> Dictionary:
	var plan: Dictionary={"before_actions":[],"after_actions":[]}
	var orders: Dictionary={"before_actions":{},"after_actions":{}}
	for entry in validators.values():
		if orders[entry.stage].has(entry.order): return _fail("DuplicateValidationOrder",entry.id)
		orders[entry.stage][entry.order]=true
		plan[entry.stage].append(entry.duplicate(true))
	for stage in plan: plan[stage].sort_custom(_validator_before)
	return {"ok":true,"plan":plan}

## 次序已证明唯一，比较器没有可变状态或领域知识。
static func _validator_before(left: Dictionary, right: Dictionary) -> bool:
	return left.order<right.order

## 检查已声明行动归属和组合根固定要求；不将保存字段变成封闭白名单。
static func _validation_coverage(registry: Dictionary) -> Dictionary:
	var claims: Dictionary={}; var actions: Dictionary={}
	for module in registry.modules.values():
		for claim in module.owns:
			claims[JSON.stringify([claim.container,claim.scope,claim.key])]=claim.owner
			if claim.container=="actions" and claim.scope=="kind": actions[claim.key]=claim.owner
	for kind in registry.legacy_action_validators:
		if registry.actions.has(kind): return _fail("DuplicateActionValidator",kind)
		if not actions.has(kind): return _fail("MissingStateOwner",kind)
	for kind in actions:
		var entry: Variant=registry.actions.get(kind,registry.legacy_action_validators.get(kind))
		if entry==null: return _fail("MissingActionValidator",kind)
		if entry.owner!=actions[kind]: return _fail("MismatchedStateOwner",kind)
	for module in registry.modules.values():
		for requirement in module.get("required_ownership",[]):
			if not Values.fields(requirement,{"container":TYPE_STRING,"scope":TYPE_STRING,"key":TYPE_STRING,"owner":TYPE_STRING}): return _fail("InvalidStateOwnership",module.id)
			var key: String=JSON.stringify([requirement.container,requirement.scope,requirement.key])
			if not claims.has(key): return _fail("MissingStateOwner",key)
			if claims[key]!=requirement.owner: return _fail("MismatchedStateOwner",key)
		for requirement in module.get("required_validators",[]):
			if not Values.fields(requirement,{"id":TYPE_STRING,"owner":TYPE_STRING,"stage":TYPE_STRING,"order":TYPE_INT}): return _fail("InvalidStateValidator",module.id)
			var entry: Variant=registry.validators.get(requirement.id)
			if entry==null: return _fail("MissingStateValidator",requirement.id)
			for key in ["owner","stage","order"]:
				if entry[key]!=requirement[key]: return _fail("InvalidStateValidator",requirement.id)
	return {"ok":true}
