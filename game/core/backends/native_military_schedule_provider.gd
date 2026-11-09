## 原军政调度的有限同步适配；仅持无工件Bridge、编译配置和本实例序号，不缓存世界或提案。
extends RefCounted
const Behavior=preload("res://core/domains/identity/actor_behavior.gd")
const Location=preload("res://core/world_location.gd")
const Combat=preload("res://core/combat_session.gd")
const Campaign=preload("res://core/campaign_rules.gd")
const Interaction=preload("res://core/interaction_rules.gd")
const SiegeWorld=preload("res://core/siege_world.gd")
const BRIDGE_CLASS="HanmoWorldBridge"
const METHOD="propose_military_schedule"
const REQUEST_FORMAT="HM-NATIVE-MILITARY-SCHEDULE/1"
const RESULT_FORMAT="HM-NATIVE-MILITARY-PROPOSALS/1"
const WIRE_LIMIT: int=64*1024*1024
const REQUIRED_BUILD=["profile","source_commit","source_sha256","core_source_sha256","dependency_lock_sha256","api_sha256"]
const BUILD_TYPES={"profile":TYPE_STRING,"source_commit":TYPE_STRING,"source_sha256":TYPE_STRING,"core_source_sha256":TYPE_STRING,"dependency_lock_sha256":TYPE_STRING,"api_sha256":TYPE_STRING,"godot_cpp_commit":TYPE_STRING,"trimmed_api_sha256":TYPE_STRING,"interface_sha256":TYPE_STRING,"profile_sha256":TYPE_STRING,"compiler":TYPE_STRING,"target":TYPE_STRING,"cpp_standard":TYPE_STRING,"godot_target":TYPE_STRING,"cmake_configuration":TYPE_STRING,"precision":TYPE_STRING,"real_t_bits":TYPE_INT,"pointer_bits":TYPE_INT,"builtin_configuration":TYPE_STRING}
var _expected_build: Dictionary={}
var _call_seq: int=0
var _bridge: RefCounted=null

## 配置先核类型再复制合法标量，错误只保存纯值首错；不持Object或Callable，调用时才报告。
func configure(expected_build: Dictionary) -> void:
	var checked: Dictionary=_check_build_configuration(expected_build)
	_expected_build=expected_build.duplicate(true) if checked.ok else {"_configuration_error":checked}
	_bridge=null

## 一次同步调用完整收集、原生运算与全批校验；任一步失败都不返回可执行的部分数组。
func propose(state: Dictionary, data: Dictionary) -> Dictionary:
	var configured: Dictionary=_expected_build._configuration_error.duplicate() if _expected_build.has("_configuration_error") else _check_build_configuration(_expected_build)
	if not configured.ok: return configured
	if _call_seq==9223372036854775807: return _failure("NativeMilitaryScopeExhausted","/scope_token")
	_call_seq+=1
	var collected: Dictionary=collect_request(state,data,str(_call_seq))
	if not collected.ok: return collected
	if _bridge==null:
		if not ClassDB.class_exists(BRIDGE_CLASS) or not ClassDB.can_instantiate(BRIDGE_CLASS) or not ClassDB.class_has_method(BRIDGE_CLASS,METHOD): return _failure("NativeMilitaryBridgeUnavailable","/native_military/bridge")
		var created: Variant=ClassDB.instantiate(BRIDGE_CLASS)
		if not created is RefCounted: return _failure("NativeMilitaryBridgeUnavailable","/native_military/bridge")
		_bridge=created
	if not _bridge.has_method(METHOD): return _failure("NativeMilitaryBridgeUnavailable","/native_military/bridge/"+METHOD)
	var result: Variant=_bridge.call(METHOD,collected.bytes)
	return validate_result(result,collected.request,_expected_build)

## 只取旧域已授权人物和真实有限事实；C++独占人物筛选、最后军令、下一命令与BFS。
static func collect_request(state: Dictionary, data: Dictionary, scope_token: String) -> Dictionary:
	if scope_token.is_empty() or not state.get("player_actor_id") is String: return _failure("NativeMilitaryInput","/scope_token")
	for field in ["actors","offices","campaigns"]:
		if not state.get(field) is Dictionary: return _failure("NativeMilitaryInput","/"+field)
	for field in ["actors","offices","garrisons","military_orders","places","settlements","roles"]:
		if not data.get(field) is Dictionary: return _failure("NativeMilitaryInput","/data/"+field)
	var ids: Array=Behavior.scheduled_ids(state,data,"military_plan")
	var actors: Array=[]; var current_cities: Dictionary={}; var target_places: Dictionary={}
	for id in ids:
		var path: String="/actors/"+str(actors.size())
		var original: Dictionary=state.actors[id]
		if not _fields(original,{"alive":TYPE_BOOL,"available":TYPE_BOOL,"action_id":TYPE_STRING}): return _failure("NativeMilitaryInput",path)
		var plan: Dictionary=Behavior.definition(state,data,id).military_plan
		if not _fields(plan,{"office_id":TYPE_STRING,"order_id":TYPE_STRING,"unit_ids":TYPE_ARRAY,"food_quantity":TYPE_INT,"money":TYPE_INT}): return _failure("NativeMilitaryInput",path+"/plan")
		for unit_id in plan.unit_ids:
			if not unit_id is String: return _failure("NativeMilitaryInput",path+"/plan/unit_ids")
		var office: Variant=state.offices.get(plan.office_id)
		var office_definition: Variant=data.offices.get(plan.office_id)
		var order: Variant=data.military_orders.get(plan.order_id)
		if not _fields(office,{"enabled":TYPE_BOOL,"holder_actor_id":TYPE_STRING,"appointment_id":TYPE_STRING}) or not _fields(office_definition,{"garrison_id":TYPE_STRING,"appointing_role_id":TYPE_STRING}): return _failure("NativeMilitaryInput",path+"/office")
		if not _fields(order,{"issuer_role_id":TYPE_STRING,"objective":TYPE_DICTIONARY}) or not order.objective.get("kind") is String: return _failure("NativeMilitaryInput",path+"/order")
		var camp: Variant=data.garrisons.get(office_definition.garrison_id)
		if not _fields(camp,{"place_id":TYPE_STRING}): return _failure("NativeMilitaryInput",path+"/office/garrison_place_id")
		var resolved: Dictionary=Location.resolve(state,id)
		var location: Dictionary={"kind":resolved.get("kind",""),"place_id":resolved.get("place_id",""),"settlement_id":resolved.get("settlement_id","")}
		if not _fields(location,{"kind":TYPE_STRING,"place_id":TYPE_STRING,"settlement_id":TYPE_STRING}): return _failure("NativeMilitaryInput",path+"/location")
		if not location.settlement_id.is_empty(): current_cities[location.settlement_id]=true
		var appointing: Dictionary=_role_context(state,data,office_definition.appointing_role_id)
		var issuer: Dictionary=_role_context(state,data,order.issuer_role_id)
		if not appointing.ok: return appointing
		if not issuer.ok: return issuer
		for place_id in [camp.place_id,appointing.context.place_id,issuer.context.place_id]:
			if not place_id.is_empty(): target_places[place_id]=true
		var occupied: bool=Combat.occupied(state,id)
		var campaigns: Array=[]
		for current in state.campaigns.values():
			if not current is Dictionary or current.get("actor_id")!=id: continue
			if not _fields(current,{"id":TYPE_STRING,"actor_id":TYPE_STRING,"template_id":TYPE_STRING,"status":TYPE_STRING,"army_id":TYPE_STRING,"office_id":TYPE_STRING,"objective_result":TYPE_DICTIONARY}): return _failure("NativeMilitaryInput",path+"/campaigns")
			var authority: Dictionary=state.offices.get(current.office_id,{})
			var row: Dictionary={"id":current.id,"actor_id":current.actor_id,"template_id":current.template_id,"status":current.status,"army_id":current.army_id,"office_id":current.office_id,"office_enabled":authority.get("enabled",false),"office_holder_actor_id":authority.get("holder_actor_id",""),"objective_result_nonempty":not current.objective_result.is_empty(),"pending_siege":{"id":"","phase":"","player_siege_id":""},"return_choice":{"ok":false,"garrison_id":"","place_id":""},"report_roles":[]}
			# 原早退遮住的领域查询不提前执行；人物行仍完整交给C++再次按原顺序筛选。
			if id!=state.player_actor_id and original.alive and original.available and original.action_id.is_empty() and not occupied and current.template_id==plan.order_id and current.status!="closed":
				var pending: String=Campaign.pending_siege(state,current.army_id)
				if not pending.is_empty():
					row.pending_siege={"id":pending,"phase":state.sieges[pending].phase,"player_siege_id":SiegeWorld.for_actor(state,state.player_actor_id)}
				elif current.status=="returning":
					var choice: Dictionary=Campaign.return_destination(state,data,current.id)
					if choice.ok:
						var return_camp: Dictionary=data.garrisons[choice.return_garrison_id]
						row.return_choice={"ok":true,"garrison_id":return_camp.id,"place_id":return_camp.place_id}
						target_places[return_camp.place_id]=true
				elif current.status=="awaiting_report":
					for role_id in Campaign.report_roles(state,data,current.id):
						var role: Dictionary=data.roles[role_id]
						var report: Dictionary=_role_context(state,data,role_id)
						if not report.ok: return report
						row.report_roles.append({"role_id":role_id,"settlement_id":data.places[role.place_id].settlement_id,"context":report.context})
						target_places[role.place_id]=true
			campaigns.append(row)
		actors.append({"actor_id":id,"alive":original.alive,"available":original.available,"action_id":original.action_id,"combat_occupied":occupied,"is_static":data.actors.has(id),"location":location,"plan":{"office_id":plan.office_id,"order_id":plan.order_id,"unit_ids":plan.unit_ids.duplicate(),"food_quantity":str(plan.food_quantity),"money":str(plan.money)},"office":{"enabled":office.enabled,"holder_actor_id":office.holder_actor_id,"appointment_id":office.appointment_id,"garrison_place_id":camp.place_id,"appointing_context":appointing.context},"order":{"issuer_context":issuer.context,"objective_kind":order.objective.kind},"campaigns":campaigns})
	var places: Array=[]; var cities: Dictionary=current_cities.duplicate(); var included_places: Dictionary={}
	for place_id in data.places:
		var place: Dictionary=data.places[place_id]
		if not place.settlement_id in current_cities and not place_id in target_places: continue
		if not _fields(place,{"settlement_id":TYPE_STRING,"passages":TYPE_ARRAY}): return _failure("NativeMilitaryInput","/places/"+place_id)
		for passage in place.passages:
			if not passage is String: return _failure("NativeMilitaryInput","/places/"+place_id+"/passages")
		places.append({"id":place_id,"settlement_id":place.settlement_id,"passages":place.passages.duplicate()})
		included_places[place_id]=true
		cities[place.settlement_id]=true
	var settlements: Array=[]
	for city in cities:
		var settlement: Variant=data.settlements.get(city)
		if not _fields(settlement,{"entry_place":TYPE_STRING}): return _failure("NativeMilitaryInput","/settlements/"+city)
		settlements.append({"id":city,"entry_place":settlement.entry_place})
		# muster可能指向尚未进入的营地城口；只补该目标节点，不在GD寻找路径。
		if not included_places.has(settlement.entry_place):
			var entry: Variant=data.places.get(settlement.entry_place)
			if not _fields(entry,{"settlement_id":TYPE_STRING,"passages":TYPE_ARRAY}): return _failure("NativeMilitaryInput","/places/"+settlement.entry_place)
			for passage in entry.passages:
				if not passage is String: return _failure("NativeMilitaryInput","/places/"+settlement.entry_place+"/passages")
			places.append({"id":settlement.entry_place,"settlement_id":entry.settlement_id,"passages":entry.passages.duplicate()})
			included_places[settlement.entry_place]=true
	var request: Dictionary={"format":REQUEST_FORMAT,"scope_token":scope_token,"player_actor_id":state.player_actor_id,"actors":actors,"places":places,"settlements":settlements}
	var bytes: PackedByteArray=JSON.stringify(request,"",true,true).to_utf8_buffer()
	if bytes.size()>WIRE_LIMIT: return _failure("NativeMilitaryWireSizeLimit","/request")
	return {"ok":true,"request":request,"bytes":bytes}

## 领域正常空Context保留为空地点等待，不将其变为未知命令或重新发授权。
static func _role_context(state: Dictionary, data: Dictionary, role_id: String) -> Dictionary:
	var source: Dictionary=Interaction.for_role(state,data,role_id)
	if source.is_empty(): return {"ok":true,"context":{"place_id":"","role_id":"","provider_actor_id":""}}
	if not _exact_fields(source,{"place_id":TYPE_STRING,"role_id":TYPE_STRING,"provider_actor_id":TYPE_STRING}): return _failure("NativeMilitaryInput","/context/"+role_id)
	return {"ok":true,"context":source.duplicate()}

## 全批先核身份、数量、原序和闭合命令引用；不在GD重算命令选择或BFS。
static func validate_result(result: Variant, request: Dictionary, expected_build: Dictionary) -> Dictionary:
	var configured: Dictionary=_check_build_configuration(expected_build)
	if not configured.ok: return configured
	if not result is Dictionary or not result.get("ok") is bool: return _failure("NativeMilitaryInvalidResult","/result")
	if not result.ok:
		if not _exact_fields(result,{"ok":TYPE_BOOL,"code":TYPE_STRING,"path":TYPE_STRING}) or result.code.is_empty(): return _failure("NativeMilitaryInvalidResult","/result")
		return _failure(result.code if result.code.begins_with("NativeMilitary") else "NativeMilitaryBridgeFailure",result.path)
	if not _exact_fields(result,{"ok":TYPE_BOOL,"format":TYPE_STRING,"scope_token":TYPE_STRING,"evaluated_count":TYPE_INT,"proposals":TYPE_ARRAY,"build":TYPE_DICTIONARY}): return _failure("NativeMilitaryInvalidResult","/result")
	if result.format!=RESULT_FORMAT: return _failure("NativeMilitaryInvalidResult","/format")
	if result.scope_token!=request.get("scope_token"): return _failure("NativeMilitaryScopeMismatch","/scope_token")
	if not request.get("actors") is Array or result.evaluated_count!=request.actors.size(): return _failure("NativeMilitaryInvalidResult","/evaluated_count")
	for field in expected_build:
		if typeof(result.build.get(field))!=typeof(expected_build[field]) or result.build[field]!=expected_build[field]: return _failure("NativeMilitaryBuildMismatch","/build/"+field)
	var positions: Dictionary={}
	for index in request.actors.size():
		var actor: Variant=request.actors[index]
		if not actor is Dictionary or not actor.get("actor_id") is String or positions.has(actor.actor_id): return _failure("NativeMilitaryInvalidResult","/request/actors")
		positions[actor.actor_id]=index
	var previous: int=-1
	for index in result.proposals.size():
		var proposal: Variant=result.proposals[index]; var path: String="/proposals/"+str(index)
		if not _exact_fields(proposal,{"actor_id":TYPE_STRING,"kind":TYPE_STRING,"params":TYPE_DICTIONARY}): return _failure("NativeMilitaryInvalidResult",path)
		if not positions.has(proposal.actor_id) or positions[proposal.actor_id]<=previous: return _failure("NativeMilitaryInvalidResult",path+"/actor_id")
		previous=positions[proposal.actor_id]
		if not _valid_params(proposal.kind,proposal.params,request.actors[previous],request): return _failure("NativeMilitaryInvalidResult",path+"/params")
	return {"ok":true,"proposals":result.proposals.duplicate(true)}

## 校验同人物有界来源引用及参数类型；不重复现任资格、campaign优先级或移动路径算法。
static func _valid_params(kind: String, params: Dictionary, actor: Dictionary, request: Dictionary) -> bool:
	match kind:
		"office_accept":
			return _exact_fields(params,{"office_id":TYPE_STRING,"context":TYPE_DICTIONARY}) and params.office_id==actor.plan.office_id and params.context==actor.office.appointing_context
		"campaign_accept":
			return _exact_fields(params,{"template_id":TYPE_STRING,"context":TYPE_DICTIONARY}) and params.template_id==actor.plan.order_id and params.context==actor.order.issuer_context
		"army_form":
			if not _exact_fields(params,{"order_id":TYPE_STRING,"unit_ids":TYPE_ARRAY,"food_quantity":TYPE_INT,"money":TYPE_INT}): return false
			return not _campaign(actor,params.order_id).is_empty() and params.unit_ids==actor.plan.unit_ids and params.food_quantity==int(actor.plan.food_quantity) and params.money==int(actor.plan.money)
		"campaign_depart","campaign_return","campaign_cancel","campaign_deliver","siege_start":
			return _exact_fields(params,{"campaign_id":TYPE_STRING}) and not _campaign(actor,params.campaign_id).is_empty()
		"campaign_report":
			if not _exact_fields(params,{"campaign_id":TYPE_STRING,"context":TYPE_DICTIONARY}): return false
			var campaign: Dictionary=_campaign(actor,params.campaign_id)
			for role in campaign.get("report_roles",[]):
				if params.context==role.context: return true
			return false
		"army_return":
			if not _exact_fields(params,{"army_id":TYPE_STRING,"garrison_id":TYPE_STRING}): return false
			for campaign in actor.campaigns:
				if campaign.actor_id==actor.actor_id and campaign.template_id==actor.plan.order_id and campaign.army_id==params.army_id and campaign.return_choice.ok and campaign.return_choice.garrison_id==params.garrison_id: return true
			return false
		"siege_close":
			if not _exact_fields(params,{"siege_id":TYPE_STRING}) or params.siege_id.is_empty(): return false
			for campaign in actor.campaigns:
				if campaign.actor_id==actor.actor_id and campaign.template_id==actor.plan.order_id and campaign.pending_siege.id==params.siege_id: return true
			return false
		"move_local":
			if not _exact_fields(params,{"place_id":TYPE_STRING}): return false
			for place in request.places:
				if place.id==actor.location.place_id: return params.place_id in place.passages
			return false
		"travel":
			if not _exact_fields(params,{"destination":TYPE_STRING}): return false
			for settlement in request.settlements:
				if settlement.id==params.destination: return true
			return false
	return false

## 只按本人及模板核campaign引用存在，不替C++选择最后一项或推演下一状态。
static func _campaign(actor: Dictionary, campaign_id: String) -> Dictionary:
	for campaign in actor.campaigns:
		if campaign.id==campaign_id and campaign.actor_id==actor.actor_id and campaign.template_id==actor.plan.order_id: return campaign
	return {}

## 必需来源键不可省略；可选已知编译字段只在显式配置时绑定，不建立第二套build schema。
static func _check_build_configuration(expected: Dictionary) -> Dictionary:
	for field in REQUIRED_BUILD:
		if not expected.get(field) is String or expected[field].is_empty(): return _failure("NativeMilitaryBuildConfiguration","/build/"+field)
	if expected.profile!="native_military_schedule": return _failure("NativeMilitaryBuildConfiguration","/build/profile")
	for field in expected:
		if not field is String or not BUILD_TYPES.has(field) or typeof(expected[field])!=BUILD_TYPES[field]: return _failure("NativeMilitaryBuildConfiguration","/build")
		if expected[field] is String and expected[field].is_empty(): return _failure("NativeMilitaryBuildConfiguration","/build/"+field)
	for field in ["source_commit","source_sha256","core_source_sha256","dependency_lock_sha256","api_sha256"]:
		if not _hex(expected[field],40 if field=="source_commit" else 64): return _failure("NativeMilitaryBuildConfiguration","/build/"+field)
	return {"ok":true}

## 来源哈希仅核已提供配置的规范格式，不扫描世界、不生成新的哈希绑定体系。
static func _hex(value: String, size: int) -> bool:
	if value.length()!=size: return false
	for index in value.length():
		if not value[index] in "0123456789abcdef": return false
	return true

## 开放源字典只核实际消费字段的原生类型，不提前调用尾Schema或校验无关extra。
static func _fields(value: Variant, fields: Dictionary) -> bool:
	if not value is Dictionary: return false
	for field in fields:
		if typeof(value.get(field))!=fields[field]: return false
	return true

## 原生返回是闭合协议；未知键、宽松数字等值或多余命令参数均不能穿过边界。
static func _exact_fields(value: Variant, fields: Dictionary) -> bool:
	return _fields(value,fields) and value.size()==fields.size()

## 所有失败均带明确码与路径，不附空提案、旧provider或可被误用的部分结果。
static func _failure(code: String, path: String) -> Dictionary:
	return {"ok":false,"code":code,"path":path}
