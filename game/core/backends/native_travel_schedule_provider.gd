## 原旅行调度的有限同步适配；仅持无工件Bridge、编译配置和本实例序号，不缓存世界或提案。
extends RefCounted
const Behavior=preload("res://core/domains/identity/actor_behavior.gd")
const BRIDGE_CLASS="HanmoWorldBridge"
const METHOD="propose_travel_schedule"
const REQUEST_FORMAT="HM-NATIVE-TRAVEL-SCHEDULE/1"
const RESULT_FORMAT="HM-NATIVE-TRAVEL-PROPOSALS/1"
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
	if _call_seq==9223372036854775807: return _failure("NativeTravelScopeExhausted","/scope_token")
	_call_seq+=1
	var collected: Dictionary=collect_request(state,data,str(_call_seq))
	if not collected.ok: return collected
	if _bridge==null:
		if not ClassDB.class_exists(BRIDGE_CLASS) or not ClassDB.can_instantiate(BRIDGE_CLASS) or not ClassDB.class_has_method(BRIDGE_CLASS,METHOD): return _failure("NativeTravelBridgeUnavailable","/native_travel/bridge")
		var created: Variant=ClassDB.instantiate(BRIDGE_CLASS)
		if not created is RefCounted: return _failure("NativeTravelBridgeUnavailable","/native_travel/bridge")
		_bridge=created
	if not _bridge.has_method(METHOD): return _failure("NativeTravelBridgeUnavailable","/native_travel/bridge/"+METHOD)
	var result: Variant=_bridge.call(METHOD,collected.bytes)
	return validate_result(result,collected.request,_expected_build)

## 只复用原Behavior授权列表；保玩家、死亡、忙碌、缺行程等候选，原生独占资格与目的地计算。
static func collect_request(state: Dictionary, data: Dictionary, scope_token: String) -> Dictionary:
	if scope_token.is_empty(): return _failure("NativeTravelInput","/scope_token")
	if typeof(state.get("tick"))!=TYPE_INT: return _failure("NativeTravelInput","/tick")
	if not state.get("player_actor_id") is String: return _failure("NativeTravelInput","/player_actor_id")
	if not state.get("actors") is Dictionary or not state.get("itineraries") is Dictionary or not data.get("actors") is Dictionary: return _failure("NativeTravelInput","/actors")
	var ids: Array=Behavior.scheduled_ids(state,data,"travel_plan")
	var actors: Array=[]
	for id in ids:
		var path: String="/actors/"+str(actors.size())
		var actor: Variant=state.actors.get(id)
		if not _fields(actor,{"action_id":TYPE_STRING,"alive":TYPE_BOOL,"available":TYPE_BOOL,"location":TYPE_DICTIONARY}): return _failure("NativeTravelInput",path)
		var original_location: Dictionary=actor.location
		if not original_location.get("kind") is String: return _failure("NativeTravelInput",path+"/location/kind")
		var location: Dictionary={"kind":original_location.kind}
		if original_location.kind=="Settlement":
			if not _fields(original_location,{"settlement_id":TYPE_STRING,"place_id":TYPE_STRING}): return _failure("NativeTravelInput",path+"/location")
			var settlements: Variant=data.get("settlements",{})
			if not settlements is Dictionary: return _failure("NativeTravelInput",path+"/location/entry_place")
			var settlement: Variant=settlements.get(original_location.settlement_id)
			if not settlement is Dictionary or not settlement.get("entry_place") is String: return _failure("NativeTravelInput",path+"/location/entry_place")
			location={"kind":"Settlement","settlement_id":original_location.settlement_id,"place_id":original_location.place_id,"entry_place":settlement.entry_place}
		var itinerary: Variant=null
		if state.itineraries.has(id):
			var original_itinerary: Variant=state.itineraries[id]
			if not _fields(original_itinerary,{"index":TYPE_INT,"ready_tick":TYPE_INT}): return _failure("NativeTravelInput",path+"/itinerary")
			itinerary={"index":str(original_itinerary.index),"ready_tick":str(original_itinerary.ready_tick)}
		var plan: Dictionary=Behavior.definition(state,data,id).get("travel_plan",{})
		if not plan.get("destinations") is Array: return _failure("NativeTravelInput",path+"/destinations")
		var destinations: Array=[]
		for destination in plan.destinations:
			if not destination is String: return _failure("NativeTravelInput",path+"/destinations/"+str(destinations.size()))
			destinations.append(destination)
		actors.append({"actor_id":id,"action_id":actor.action_id,"alive":actor.alive,"available":actor.available,"location":location,"itinerary":itinerary,"destinations":destinations})
	var parties: Array=[]
	var original_parties: Variant=state.get("parties",{})
	if not original_parties is Dictionary: return _failure("NativeTravelInput","/parties")
	for id in original_parties:
		var party: Variant=original_parties[id]
		if not party is Dictionary: continue
		var members: Variant=party.get("members",[])
		if not members is Array: return _failure("NativeTravelInput","/parties/members")
		var relevant: bool=false
		for member in members:
			if member in ids: relevant=true
		if not relevant: continue
		if not id is String: return _failure("NativeTravelInput","/parties/id")
		for member in members:
			if not member is String: return _failure("NativeTravelInput","/parties/"+str(parties.size())+"/members")
		parties.append({"id":id,"members":members.duplicate()})
	var interactions: Array=[]
	var original_interactions: Variant=state.get("interactions",{})
	if not original_interactions is Dictionary: return _failure("NativeTravelInput","/interactions")
	for actor_id in original_interactions:
		var session: Variant=original_interactions[actor_id]
		if not session is Dictionary or not session.get("context") is Dictionary: return _failure("NativeTravelInput","/interactions/context")
		var provider_actor_id: Variant=session.context.get("provider_actor_id")
		if not actor_id in ids and not provider_actor_id in ids: continue
		if not actor_id is String or not provider_actor_id is String: return _failure("NativeTravelInput","/interactions/provider_actor_id")
		interactions.append({"actor_id":actor_id,"provider_actor_id":provider_actor_id})
	var request: Dictionary={"format":REQUEST_FORMAT,"scope_token":scope_token,"tick":str(state.tick),"player_actor_id":state.player_actor_id,"actors":actors,"parties":parties,"interactions":interactions}
	var bytes: PackedByteArray=JSON.stringify(request,"",true,true).to_utf8_buffer()
	if bytes.size()>WIRE_LIMIT: return _failure("NativeTravelWireSizeLimit","/request")
	return {"ok":true,"request":request,"bytes":bytes}

## 全批核形状、同次scope、构建身份、顺序和已授引用；不在GD复算谁有资格或如何轮转目的地。
static func validate_result(result: Variant, request: Dictionary, expected_build: Dictionary) -> Dictionary:
	var configured: Dictionary=_check_build_configuration(expected_build)
	if not configured.ok: return configured
	if not result is Dictionary or not result.get("ok") is bool: return _failure("NativeTravelInvalidResult","/result")
	if not result.ok:
		if not _exact_fields(result,{"ok":TYPE_BOOL,"code":TYPE_STRING,"path":TYPE_STRING}) or result.code.is_empty(): return _failure("NativeTravelInvalidResult","/result")
		return _failure(result.code if result.code.begins_with("NativeTravel") else "NativeTravelBridgeFailure",result.path)
	if not _exact_fields(result,{"ok":TYPE_BOOL,"format":TYPE_STRING,"scope_token":TYPE_STRING,"evaluated_count":TYPE_INT,"proposals":TYPE_ARRAY,"build":TYPE_DICTIONARY}): return _failure("NativeTravelInvalidResult","/result")
	if result.format!=RESULT_FORMAT: return _failure("NativeTravelInvalidResult","/format")
	if result.scope_token!=request.get("scope_token"): return _failure("NativeTravelScopeMismatch","/scope_token")
	if not request.get("actors") is Array or result.evaluated_count!=request.actors.size(): return _failure("NativeTravelInvalidResult","/evaluated_count")
	for field in expected_build:
		if typeof(result.build.get(field))!=typeof(expected_build[field]) or result.build[field]!=expected_build[field]: return _failure("NativeTravelBuildMismatch","/build/"+field)
	var actor_positions: Dictionary={}
	for index in request.actors.size():
		var actor: Variant=request.actors[index]
		if not actor is Dictionary or not actor.get("actor_id") is String or not actor.get("destinations") is Array or actor_positions.has(actor.actor_id): return _failure("NativeTravelInvalidResult","/request/actors/"+str(index))
		actor_positions[actor.actor_id]=index
	var previous: int=-1
	for index in result.proposals.size():
		var path: String="/proposals/"+str(index)
		var proposal: Variant=result.proposals[index]
		if not _exact_fields(proposal,{"actor_id":TYPE_STRING,"kind":TYPE_STRING,"params":TYPE_DICTIONARY,"next_index":TYPE_INT}): return _failure("NativeTravelInvalidResult",path)
		if proposal.kind!="travel" or not _exact_fields(proposal.params,{"destination":TYPE_STRING}): return _failure("NativeTravelInvalidResult",path+"/params")
		if not actor_positions.has(proposal.actor_id): return _failure("NativeTravelInvalidResult",path+"/actor_id")
		var position: int=actor_positions[proposal.actor_id]
		if position<=previous: return _failure("NativeTravelInvalidResult",path+"/actor_id")
		previous=position
		if not request.actors[position].get("itinerary") is Dictionary: return _failure("NativeTravelInvalidResult",path+"/actor_id")
		var destinations: Array=request.actors[position].destinations
		if not proposal.params.destination in destinations: return _failure("NativeTravelInvalidResult",path+"/params/destination")
		if proposal.next_index <= -destinations.size() or proposal.next_index>=destinations.size(): return _failure("NativeTravelInvalidResult",path+"/next_index")
	return {"ok":true,"proposals":result.proposals.duplicate(true)}

## 必需来源键不可省略；可选已知编译字段只在显式配置时绑定，不建立第二套build schema。
static func _check_build_configuration(expected: Dictionary) -> Dictionary:
	for field in REQUIRED_BUILD:
		if not expected.get(field) is String or expected[field].is_empty(): return _failure("NativeTravelBuildConfiguration","/build/"+field)
	if expected.profile!="native_travel_schedule": return _failure("NativeTravelBuildConfiguration","/build/profile")
	for field in expected:
		if not field is String or not BUILD_TYPES.has(field) or typeof(expected[field])!=BUILD_TYPES[field]: return _failure("NativeTravelBuildConfiguration","/build")
		if expected[field] is String and expected[field].is_empty(): return _failure("NativeTravelBuildConfiguration","/build/"+field)
	for field in ["source_commit","source_sha256","core_source_sha256","dependency_lock_sha256","api_sha256"]:
		if not _hex(expected[field],40 if field=="source_commit" else 64): return _failure("NativeTravelBuildConfiguration","/build/"+field)
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
