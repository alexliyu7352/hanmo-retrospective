## 按受信注册计划组合完整校验；不载入具体领域、不修改世界、不缓存可变注册。
extends RefCounted
const Shape=preload("res://core/base/world_shape.gd")
const Values=preload("res://core/base/value_checks.gd")

const COMPOSITION_ID="builtin"
const COMPOSITION_VERSION="f10-architecture-1"

## 固定拥有权清单独立于输入metadata；每次展开新值，不共享可变数组。
static func required_ownership() -> Array:
	var grouped: Dictionary={
		"runtime":{"root":["ruleset_id","content_id","content_version","content_digest","engine_build","rng_backend","scenario_id","tick","revision","next_instance_id","rng","request_sequences","results","history"],"envelope":["actions","battles","facts"],"actor_field":["alive","available","hp","fatigue","merit","action_id"],"actions":["wait"]},
		"identity":{"root":["player_actor_id"],"envelope":["actors"],"actor_field":["id","prototype_id","display_name","birth_tick","created_tick","wallet_id","inventory_id","appearance"],"facts":["actor_creation"]},
		"economy":{"root":["wallets","inventories"],"actions":["trade"],"facts":["trade_bargain"]},
		"interaction":{"root":["roles","interactions","events","flags","encounters"]},
		"services":{"actions":["service","long_rest"],"facts":["long_rest"]},
		"tasks":{"root":["tasks","knowledge"],"actions":["accept_offer","report_offer","deliver_message"],"facts":["escort_arrival"]},
		"social":{"root":["relations","social"]},
		"travel":{"actor_field":["location"],"root":["parties","itineraries"],"actions":["travel","rescue"],"facts":["terrain_journey"]},
		"personal":{"actor_field":["injury"],"battles":["personal"],"facts":["personal_battle"]},
		"cultivation":{"actor_field":["attributes","skills","combat"],"actions":["learn_skill","practice_skill","teach_skill","study_skill"],"facts":["training","skill_study"]},
		"estates":{"root":["estates"],"actions":["estate_build"],"facts":["estate_production","estate_management","estate_transfer","estate_construction","estate_funding","estate_detachment","estate_muster"]},
		"family":{"root":["family"],"actions":["family_adopt"],"facts":["family_adoption_pending","family_adoption","family_designation","family_succession","family_adoption_failed"]},
		"military":{"root":["offices","garrisons","military_units","armies","campaigns","strongholds","sieges"],"battles":["military"],"actions":["office_accept","army_form","army_resupply","army_return","military_train"],"facts":["military_training","military_march","office_resignation","military_action","military_muster","military_supply","military_casualties","military_return","military_battle","military_contact","military_occupation","military_relief","siege_material","military_delivery","campaign_arrival","campaign_settlement"]},
	}
	var result: Array=[]
	for owner in grouped:
		for group in grouped[owner]:
			for name in grouped[owner][group]:
				var scope: String=group if group in ["root","envelope","actor_field"] else "kind"
				result.append({"owner":owner,"container":name if scope in ["root","envelope"] else "actors" if scope=="actor_field" else group,"scope":scope,"key":"" if scope in ["root","envelope"] else name})
	return result

## 固定状态叶的身份、owner与位置独立于待验描述；回调可以包装，但不能删除旧检查。
static func required_validators() -> Array:
	return [
		{"id":"interaction.roles","owner":"interaction","stage":"before_actions","order":10},
		{"id":"economy.state","owner":"economy","stage":"before_actions","order":20},
		{"id":"interaction.flags","owner":"interaction","stage":"before_actions","order":30},
		{"id":"social.relations","owner":"social","stage":"before_actions","order":40},
		{"id":"runtime.state","owner":"runtime","stage":"before_actions","order":50},
		{"id":"personal.state","owner":"personal","stage":"before_actions","order":60},
		{"id":"military.state","owner":"military","stage":"before_actions","order":70},
		{"id":"social.state","owner":"social","stage":"before_actions","order":80},
		{"id":"identity.state","owner":"identity","stage":"before_actions","order":90},
		{"id":"family.state","owner":"family","stage":"before_actions","order":100},
		{"id":"estates.state","owner":"estates","stage":"before_actions","order":110},
		{"id":"family.tail","owner":"family","stage":"before_actions","order":120},
		{"id":"long_rest.state","owner":"services","stage":"before_actions","order":130},
		{"id":"tasks.state","owner":"tasks","stage":"before_actions","order":140},
		{"id":"skill_study.state","owner":"cultivation","stage":"before_actions","order":150},
		{"id":"parties.state","owner":"travel","stage":"after_actions","order":10},
		{"id":"encounters.state","owner":"interaction","stage":"after_actions","order":20},
		{"id":"itineraries.state","owner":"travel","stage":"after_actions","order":30},
		{"id":"interaction.sessions","owner":"interaction","stage":"after_actions","order":40},
	]

## 完整世界的信任锚来自固定代码，不由传入required列表或空操作模块自证。
static func contract_status(registry: Dictionary) -> Dictionary:
	if not Values.fields(registry,{"modules":TYPE_DICTIONARY,"validators":TYPE_DICTIONARY}) or not registry.modules.has(COMPOSITION_ID): return {"ok":false,"code":"InvalidModuleRegistry"}
	var composition: Variant=registry.modules[COMPOSITION_ID]
	if not composition is Dictionary or composition.get("version")!=COMPOSITION_VERSION: return {"ok":false,"code":"InvalidModuleRegistry"}
	var claims: Dictionary={}
	for id in registry.modules:
		var module: Variant=registry.modules[id]
		if not Values.fields(module,{"id":TYPE_STRING,"owns":TYPE_ARRAY}) or module.id!=id: return {"ok":false,"code":"InvalidStateOwnership"}
		for claim in module.owns:
			if not Values.fields(claim,{"container":TYPE_STRING,"scope":TYPE_STRING,"key":TYPE_STRING,"owner":TYPE_STRING}) or claim.owner!=id: return {"ok":false,"code":"InvalidStateOwnership"}
			var key: String=JSON.stringify([claim.container,claim.scope,claim.key])
			if claims.has(key): return {"ok":false,"code":"DuplicateStateOwner"}
			claims[key]=claim.owner
	for requirement in required_ownership():
		var key: String=JSON.stringify([requirement.container,requirement.scope,requirement.key])
		if not claims.has(key): return {"ok":false,"code":"MissingStateOwner","detail":key}
		if claims[key]!=requirement.owner: return {"ok":false,"code":"MismatchedStateOwner","detail":key}
	for requirement in required_validators():
		var entry: Variant=registry.validators.get(requirement.id)
		if entry==null: return {"ok":false,"code":"MissingStateValidator","detail":requirement.id}
		if not entry is Dictionary: return {"ok":false,"code":"InvalidStateValidator","detail":requirement.id}
		for key in ["id","owner","stage","order"]:
			if entry.get(key)!=requirement[key]: return {"ok":false,"code":"InvalidStateValidator","detail":requirement.id}
	return {"ok":true}

## 已接受注册被损坏时关闭入口；缺少某个运行行动仍留到原行动位置报错。
static func valid_registry(registry: Variant) -> bool:
	if not Values.fields(registry,{"ok":TYPE_BOOL,"actions":TYPE_DICTIONARY,"legacy_action_validators":TYPE_DICTIONARY,"validators":TYPE_DICTIONARY,"validation_plan":TYPE_DICTIONARY}) or not registry.ok: return false
	if not contract_status(registry).ok: return false
	if not Values.fields(registry.validation_plan,{"before_actions":TYPE_ARRAY,"after_actions":TYPE_ARRAY,"required":TYPE_ARRAY}): return false
	if registry.validation_plan.required.is_empty(): return false
	for requirement in registry.validation_plan.required:
		if not Values.fields(requirement,{"id":TYPE_STRING,"owner":TYPE_STRING,"stage":TYPE_STRING,"order":TYPE_INT}): return false
		var required: Variant=registry.validators.get(requirement.id)
		if not required is Dictionary: return false
		for key in ["owner","stage","order"]:
			if required.get(key)!=requirement[key]: return false
	var seen: Dictionary={}
	for stage in ["before_actions","after_actions"]:
		var previous: int=-1
		for entry in registry.validation_plan[stage]:
			if not Values.fields(entry,{"id":TYPE_STRING,"stage":TYPE_STRING,"order":TYPE_INT,"signature":TYPE_STRING,"callback":TYPE_CALLABLE}) or entry.stage!=stage or entry.signature!="state,data" or entry.order<=previous or not entry.callback.is_valid() or entry.callback.get_argument_count()!=2: return false
			if seen.has(entry.id) or registry.validators.get(entry.id)!=entry: return false
			previous=entry.order; seen[entry.id]=true
	return seen.size()==registry.validators.size()

## 回调失败必须有明确错误码；坏返回不能被真值转换为校验成功。
static func _result(value: Variant, code: String, id: String) -> Dictionary:
	if not value is Dictionary or not value.get("ok") is bool or (not value.ok and (not value.get("code") is String or value.code.is_empty())): return {"ok":false,"code":code,"detail":id}
	return value

## 各叶只调用一次，首错立即停止，成功叶的附加信息不写入世界。
static func _run(state: Dictionary, data: Dictionary, entries: Array) -> Dictionary:
	for entry in entries:
		var result: Dictionary=_result(entry.callback.call(state,data),"InvalidStateValidatorResult",entry.id)
		if not result.ok: return result
	return {"ok":true}

## 共有形状、领域前段、行动占用和领域后段使用同一个完整入口。
static func validate(state: Dictionary, catalog: Dictionary, registry: Variant) -> Dictionary:
	if not valid_registry(registry): return {"ok":false,"code":"InvalidModuleRegistry"}
	var shape: Dictionary=Shape.validate(state,catalog)
	if not shape.ok: return shape
	var data: Dictionary=catalog.data
	var before: Dictionary=_run(state,data,registry.validation_plan.before_actions)
	if not before.ok: return before
	var actions: Dictionary=validate_actions(state,data,registry)
	if not actions.ok: return actions
	return _run(state,data,registry.validation_plan.after_actions)

## 完整行动优先使用唯一生命周期校验；过渡叶只提供载荷检查，不冒充生命周期。
static func action_parameters(state: Dictionary, data: Dictionary, action: Dictionary, registry: Dictionary) -> Dictionary:
	var entry: Variant=registry.actions.get(action.kind,registry.legacy_action_validators.get(action.kind))
	if not entry is Dictionary or not entry.get("validate") is Callable or not entry.validate.is_valid() or entry.validate.get_argument_count()!=3: return {"ok":false,"code":"InvalidActionKind"}
	return _result(entry.validate.call(state,data,action),"InvalidActionValidatorResult",action.kind)

## 按行动插入序先载荷后参与者，保留反向占用及道路人物的旧跨域约束。
static func validate_actions(state: Dictionary, data: Dictionary, registry: Dictionary) -> Dictionary:
	for id in state.actions:
		var action: Variant=state.actions[id]
		if not Values.fields(action,{"id":TYPE_STRING,"kind":TYPE_STRING,"actor_id":TYPE_STRING,"participants":TYPE_ARRAY,"remaining_ticks":TYPE_INT,"start_tick":TYPE_INT,"params":TYPE_DICTIONARY}) or action.id!=id or not state.actors.has(action.actor_id) or not Values.whole(action.remaining_ticks,1) or not Values.whole(action.start_tick): return {"ok":false,"code":"InvalidAction"}
		if (not registry.actions.has(action.kind) and not registry.legacy_action_validators.has(action.kind)) or (not action.actor_id in action.participants and not (action.kind=="study_skill" and action.participants.is_empty())): return {"ok":false,"code":"InvalidActionKind"}
		var specific: Dictionary=action_parameters(state,data,action,registry)
		if not specific.ok: return specific
		var seen: Array=[]
		for actor_id in action.participants:
			if actor_id in seen or not state.actors.has(actor_id) or state.actors[actor_id].action_id!=id: return {"ok":false,"code":"InvalidOccupation"}
			seen.append(actor_id)
	for actor in state.actors.values():
		if not actor.action_id.is_empty() and (not state.actions.has(actor.action_id) or not actor.id in state.actions[actor.action_id].participants): return {"ok":false,"code":"DanglingOccupation"}
		if actor.location.kind=="OnRoute" and (actor.action_id.is_empty() or not state.actions[actor.action_id].kind in ["travel","rescue"]): return {"ok":false,"code":"RouteWithoutTravel"}
	return {"ok":true}
