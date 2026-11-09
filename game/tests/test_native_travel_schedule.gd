## 固定旧Schedule的完整提案与真实Kernel短链；原生候选只与独立旧oracle比较，不跑存读矩阵。
extends SceneTree
const Kernel=preload("res://core/world_kernel.gd")
const F=preload("res://tests/companion_fixture.gd")
const Schedule=preload("res://core/travel_schedule.gd")
const Behavior=preload("res://core/domains/identity/actor_behavior.gd")
const Location=preload("res://core/world_location.gd")
const Strict=preload("res://core/strict_json.gd")
const Schema=preload("res://core/state_schema.gd")
const OLD_SOURCE="8f7ebc4f7af5b3e9bdfc81adde4d09d2fed00d93"
var checks: int=0
var failures: int=0
var fixture_path: String=""
var oracle_path: String=""
var record_mode: bool=false
var native_mode: bool=false
var unavailable_mode: bool=false
var extension_path: String=""
var build_path: String=""
var evidence_path: String=""
var evidence_rows: Array=[]
var expected_build: Dictionary={}
var Provider
var provider
var rows: Array=[]
var fixtures: Dictionary={}
var failure_fixture: Dictionary={}

## 只观察真实preview和开工调用；拒绝发生在修改前，旧continue语义仍由生产Backend执行。
class WitnessKernel extends Kernel:
	var trace: Array=[]
	var reject_begin: String=""
	var military_calls: int=0
	var advance_before: Dictionary={}
	var step_candidate: Dictionary={}
	var observer_calls: int=0
	var schema_calls: int=0
	## 只包装原validator观察调用次数；所有领域校验仍执行原callback。
	func _module_descriptors() -> Array:
		var modules: Array=super._module_descriptors().duplicate(true)
		for module in modules:
			for validator in module.validators: validator.callback=_validate_witness.bind(validator.callback)
			if module.id=="interaction": module.observers.append({"id":"test.travel_tail","owner":"interaction","before":[],"after":["interaction.events_reconcile"],"callback":_observe_witness})
		return modules
	## Schema探针不能放宽原规则或返回自造成功。
	func _validate_witness(state: Dictionary,data: Dictionary,original: Callable) -> Dictionary:
		schema_calls+=1;return original.call(state,data)
	## 业务Events之后才可出现本只读观察器。
	func _observe_witness(_state: Dictionary,_data: Dictionary,_context: Dictionary) -> Dictionary:
		observer_calls+=1;return {"ok":true}
	## 原动态preview出口逐条保序，记录所见抵达休整和全态，不能另造合法报价。
	func _preview(state: Dictionary,kind: String,params: Dictionary,actor: String) -> Dictionary:
		var result: Dictionary=super._preview(state,kind,params,actor)
		if kind=="travel": trace.append(["preview",actor,state.tick,params.duplicate(true),result.duplicate(true),state.itineraries.get(actor,{}).duplicate(true)])
		return result
	## 真实begin成功才能推进itinerary；明确失败不消费费用也不删除后续提案。
	func _begin_action(state: Dictionary,actor: String,kind: String,params: Dictionary,offer: Dictionary) -> Dictionary:
		if actor==reject_begin:
			trace.append(["begin_rejected",actor,state.tick]);return {"ok":false,"code":"InjectedTravelBegin"}
		var result: Dictionary=super._begin_action(state,actor,kind,params,offer)
		if kind=="travel": trace.append(["begin",actor,state.tick,result.duplicate(true),state.itineraries.get(actor,{}).duplicate(true)])
		return result
	## 此观测发生于Step之前，完整旧候选留给失败原子性证明。
	func _advance(state: Dictionary,hold: Array=[]) -> Dictionary:
		advance_before=state.duplicate(true)
		var result: Dictionary=super._advance(state,hold)
		step_candidate=state.duplicate(true)
		return result
	## 调度之后才会进入军事尾部，计数用于证明provider失败不能继续。
	func _schedule_military(state: Dictionary) -> Dictionary:
		military_calls+=1;step_candidate=state.duplicate(true)
		trace.append(["military",state.tick])
		return super._schedule_military(state)

## 持真实新Bridge的窄故障代理；每次先得到真实原生值，再破坏指定返回边界。
class BridgeProxy extends RefCounted:
	var real: RefCounted
	var mode: String=""
	var calls: int=0
	var requests: Array=[]
	var original_result: Dictionary={}
	## 故障不造有效提案或改写世界，仅破坏实际so的输入或完整输出。
	func propose_travel_schedule(bytes: PackedByteArray) -> Variant:
		calls+=1
		var request: Dictionary=Strict.parse_text(bytes.get_string_from_utf8()).value
		requests.append(request.duplicate(true))
		if mode=="cpp_failure":
			request.tick="01"
			return real.call("propose_travel_schedule",JSON.stringify(request).to_utf8_buffer())
		var result: Dictionary=real.call("propose_travel_schedule",bytes)
		original_result=result.duplicate(true)
		if not result.get("ok",false):return result
		match mode:
			"late_bad": result.proposals.append({"actor_id":"late-invalid","kind":"travel","params":{"destination":"village.east"},"next_index":false})
			"scope": result.scope_token="prior-scope"
			"build": result.build.source_sha256="0".repeat(64)
			"shape": return []
		return result

## 参数显式选择旧捕获或新库；默认不在共享世界中裸跑。
func _init() -> void:
	for arg in OS.get_cmdline_user_args():
		if arg.begins_with("--dynamic-content="): fixture_path=arg.trim_prefix("--dynamic-content=")
		if arg.begins_with("--oracle="): oracle_path=arg.trim_prefix("--oracle=")
		if arg.begins_with("--extension="): extension_path=arg.trim_prefix("--extension=")
		if arg.begins_with("--build="): build_path=arg.trim_prefix("--build=")
		if arg.begins_with("--evidence="): evidence_path=arg.trim_prefix("--evidence=")
		if arg=="--record": record_mode=true
		if arg=="--native": native_mode=true
		if arg=="--unavailable": unavailable_mode=true
	call_deferred("run_checks")

## 每条可解释断言独立计数，不能把缺运行算作成功。
func check(value: bool,label: String) -> bool:
	checks+=1
	if not value: failures+=1;printerr("FAIL: ",label)
	return value

## JSON读取复用严格整数解析，避免MAX边界被float桥丢精度。
func read_json(path: String) -> Dictionary:
	var parsed: Dictionary=Strict.parse_text(FileAccess.get_file_as_string(path))
	return parsed.get("value",{}) if parsed.get("ok",false) and parsed.get("value") is Dictionary else {}

## 测试世界只走正式作者装载，native配置明确且不隐式更换原库。
func opened(variant: String="travel_enabled"):
	var k=WitnessKernel.new()
	if not check(k.new_game("scenario.alpha_life",fixtures[variant].catalog).ok,"正式动态作者开局 "+variant): return null
	if native_mode: k.call("configure_native_travel_schedule",true,expected_build)
	return k

## 正式命令信封保留请求幂等与版本守卫，错误由调用者明确验证。
func command(k,kind: String,params: Dictionary={},actor: String="") -> Dictionary:
	var state: Dictionary=k.snapshot()
	if actor.is_empty(): actor=state.player_actor_id
	return {"kind":kind,"params":params,"actor_id":actor,"request_seq":state.request_sequences[actor]+1,"expected_revision":state.revision}

## 成功链不绕过preview或直接写私有世界。
func send(k,kind: String,params: Dictionary={},actor: String="") -> Dictionary:
	var result: Dictionary=k.submit(command(k,kind,params,actor))
	check(result.ok,"真实命令 "+kind+" "+str(result))
	return result

## 无行动时先真实wait，再推进半日；不用裸advance绕过时间守卫。
func advance(k) -> Dictionary:
	if k.snapshot().actions.is_empty() and not send(k,"wait").ok: return {"ok":false}
	return send(k,"advance")

## 直接复用旧动态测试的真实当面两半日收养路径，不调用造人或Codec。
func adopt(k) -> String:
	if not send(k,"move_local",{"place_id":"west.estate"}).ok: return ""
	var before: Dictionary=k.snapshot()
	if not send(k,"family_adopt",{"context":F.context(k,"role.estate_steward"),"display_name":"新行者"}).ok: return ""
	if not advance(k).ok or not advance(k).ok: return ""
	for id in k.snapshot().family.children:
		if not before.family.children.has(id):
			var state: Dictionary=k.snapshot();var fact: Dictionary=state.facts[state.family.children[id]]
			check(fact.kind=="family_adoption" and fact.success and fact.child_actor_id==id,"真实完成事实绑定动态身份")
			return id
	check(false,"收养没有真实新身份");return ""

## 完整数组oracle来自旧Schedule，同一输入前后保持不变；native只读同输入不得fallback。
func proposal_case(label: String,state: Dictionary,data: Dictionary) -> void:
	var before: Dictionary=state.duplicate(true);var original: Dictionary=data.duplicate(true)
	var proposals: Array=Schedule.proposals(state,data)
	var ids: Array=Behavior.scheduled_ids(state,data,"travel_plan")
	check(state==before and data==original,label+"查询不改源")
	if native_mode:
		var actual: Dictionary=provider.propose(state,data)
		check(actual.get("ok",false) and actual.get("proposals")==proposals,label+"新库完整数组逐项同旧")
		check(state==before and data==original,label+"原生不改源")
	rows.append({"label":label,"ids":ids,"proposals":proposals})

## 静态和实际收养身份共用原排序/权限；只在只读副本设置明确过滤反例。
func queries(k,child: String) -> void:
	var base: Dictionary=k.snapshot();var data: Dictionary=k.content().data
	base.actors[child].location={"kind":"Settlement","settlement_id":"city.west","place_id":"west.street"}
	base.itineraries[child].ready_tick=base.tick
	proposal_case("actual_adopted_ready",base,data)
	for mode in ["player","missing_itinerary","busy","ready_later","ready_equal","party","dead","unavailable","road","interior","self_session","provider_session","failed_source","pending_source","foreign_child","foreign_creation"]:
		var state: Dictionary=base.duplicate(true)
		match mode:
			"player": state.player_actor_id=child
			"missing_itinerary": state.itineraries.erase(child)
			"busy": state.actors[child].action_id="occupied"
			"ready_later": state.itineraries[child].ready_tick=state.tick+1
			"party": state.parties["probe"]={"id":"probe","members":[child]}
			"dead": state.actors[child].alive=false
			"unavailable": state.actors[child].available=false
			"road": state.actors[child].location={"kind":"Road"}
			"interior": state.actors[child].location.place_id="west.estate"
			"self_session": state.interactions[child]={"context":{"provider_actor_id":"actor.shen"}}
			"provider_session": state.interactions["actor.shen"]={"context":{"provider_actor_id":child}}
			"failed_source": state.facts[state.family.children[child]].success=false
			"pending_source": state.facts[state.family.children[child]].kind="family_adoption_pending"
			"foreign_child": state.facts[state.family.children[child]].child_actor_id="actor.apprentice"
			"foreign_creation": state.facts[child].source="missing-source"
		proposal_case(mode,state,data)
	for destinations in [["city.west"],["village.east"],["city.west","city.west"],["city.west","city.west","village.east"],["city.west","village.east","village.east"]]:
		var changed: Dictionary=data.duplicate(true)
		changed.actors[base.actors[child].prototype_id].travel_plan.destinations=destinations
		proposal_case("destinations_"+str(destinations),base,changed)
	for index in [-1,-2]:
		var signed: Dictionary=base.duplicate(true);signed.itineraries[child].index=index
		proposal_case("negative_index_"+str(index),signed,data)
	var signed: Dictionary=base.duplicate(true);signed.itineraries[child].index=-3
	var signed_data: Dictionary=data.duplicate(true);signed_data.actors[base.actors[child].prototype_id].travel_plan.destinations=["village.east","city.west","village.east"]
	proposal_case("negative_remainder_next_index",signed,signed_data)
	var empty: Dictionary=base.duplicate(true);empty.player_actor_id=child
	var empty_data: Dictionary=data.duplicate(true);empty_data.actors[base.actors[child].prototype_id].travel_plan.destinations=[]
	proposal_case("unconsumed_empty_destinations",empty,empty_data)
	var high: Dictionary=base.duplicate(true);high.tick=2147483647;high.itineraries[child].travelling=true
	Schedule.finish_arrivals(high,data)
	check(high.itineraries[child].ready_tick==2147483649,"抵达ready允许暂超MAX交尾Schema")
	proposal_case("arrival_above_max",high,data)

## 冻结真实半日结果、完整状态和逐条动态trace，不以新native结果生成期望。
func record(label: String,k,result: Dictionary={}) -> void:
	rows.append({"label":label,"state":k.snapshot(),"trace":k.trace.duplicate(true),"result":result.duplicate(true)})
	k.trace.clear()

## 保留缺授权原型对照；真实工作带来旅费，出发半日不补走或扣下一步费用。
func real_flows() -> void:
	var no=opened("travel_default")
	if no==null:return
	var unauthorized: String=adopt(no)
	if unauthorized.is_empty():return
	check(not no.snapshot().itineraries.has(unauthorized),"默认收养不借原型取得行程")
	proposal_case("actual_unauthorized_adoption",no.snapshot(),no.content().data)
	var k=opened();if k==null:return
	var child: String=adopt(k);if child.is_empty():return
	queries(k,child)
	if not send(k,"move_local",{"place_id":"west.street"},child).ok:return
	check(k.preview("travel",{"destination":"village.east"},child).get("code")=="InsufficientFunds","真实无私产preview拒绝旅费")
	while k.snapshot().tick<5:
		if not advance(k).ok:return
	check(k.snapshot().actors[child].action_id.is_empty() and not k.snapshot().itineraries[child].travelling,"缺旅费的提案continue不改游标")
	record("insufficient_funds",k)
	if not send(k,"move_local",{"place_id":"west.tavern"},child).ok:return
	if not send(k,"service",{"activity":"work","context":F.context(k,"role.host")},child).ok or not advance(k).ok:return
	check(k.snapshot().wallets[k.snapshot().actors[child].wallet_id].balance==80,"本人真实工作获得80旅费")
	if not send(k,"move_local",{"place_id":"west.street"},child).ok:return
	k.reject_begin=child
	var before: Dictionary=k.snapshot();if not advance(k).ok:return
	check(k.snapshot().actors[child].action_id.is_empty() and k.snapshot().itineraries[child]==before.itineraries[child],"开工拒绝保持原continue游标")
	record("begin_rejected",k);k.reject_begin=""
	failure_fixture={"state":k.snapshot(),"child":child}
	var funds: int=k.snapshot().wallets[k.snapshot().actors[child].wallet_id].balance
	if not advance(k).ok:return
	var state: Dictionary=k.snapshot();var action: Dictionary=state.actions.get(state.actors[child].action_id,{})
	if not check(action.get("kind")=="travel","真实行程开始"):return
	check(action.start_tick==state.tick and state.wallets[state.actors[child].wallet_id].balance==funds,"出发不补走半日或预扣下一步旅费")
	record("departed",k)
	if not advance(k).ok:return
	check(k.snapshot().wallets[k.snapshot().actors[child].wallet_id].balance<funds,"下一半日才扣本人旅费")
	record("progressed",k)
	var last_travelling: Dictionary={}
	for _half in range(24):
		if k.snapshot().actors[child].action_id.is_empty():break
		last_travelling=k.snapshot()
		if not advance(k).ok:return
	state=k.snapshot()
	check(Location.resolve(state,child).get("settlement_id")=="village.east" and not state.itineraries[child].travelling and state.itineraries[child].ready_tick==state.tick+2,"真实抵达只登记两拍休整")
	record("arrived_dwell",k)
	if not advance(k).ok:return
	check(k.snapshot().actors[child].action_id.is_empty(),"休整未满不出发")
	if not advance(k).ok:return
	check(not k.snapshot().actors[child].action_id.is_empty(),"休整到期才第二次出发")
	record("departed_again",k)
	max_tick_arrival(last_travelling,child)


## 真正在途副本同步提高世界及现行领域时标，经完整恢复准入后才核MAX抵达尾错。
func max_tick_arrival(last_travelling: Dictionary,child: String) -> void:
	var k=opened();if k==null:return
	var state: Dictionary=last_travelling.duplicate(true);state.tick=2147483646
	state.social.last_tick=state.tick
	for camp in state.garrisons.values():camp.last_supply_tick=state.tick
	for estate in state.estates.values():
		estate.last_tick=state.tick
		if not estate.last_production_id.is_empty():state.facts[estate.last_production_id].tick=state.tick
	var restored: Dictionary=k.restore(state)
	if not check(restored.ok,"MAX前一拍的真实在途快照通过原恢复门 "+str(restored)):return
	var before: Dictionary=k.snapshot();k.trace.clear();k.schema_calls=0;k.observer_calls=0
	var result: Dictionary=k.submit(command(k,"advance"))
	check(not result.ok and k.snapshot()==before,"MAX抵达尾错误完整候选不发布 "+str(result))
	check(k.step_candidate.get("tick")==2147483647 and k.step_candidate.itineraries[child].ready_tick==2147483649,"实际Step抵达后finish_arrivals保int64超MAX")
	check(k.observer_calls==1 and k.schema_calls>0,"原尾observer与Schema仍在既定错误位置")
	record("actual_max_arrival_tail",k,result)


## 从冻结旧真实开工前世界恢复；无须为每个故障重跑收养链或重新生成钱粮。
func failure_kernel():
	if failure_fixture.is_empty():
		for row in read_json(oracle_path).get("rows",[]):
			if row.label=="begin_rejected":failure_fixture={"state":row.state,"child":row.state.family.children.keys()[0]}
	var k=opened();if k==null:return null
	if not check(k.restore(failure_fixture.state).ok,"正式恢复旧oracle实际出发前世界"):return null
	if k.snapshot().actions.is_empty() and not send(k,"wait").ok:return null
	k.trace.clear();k.military_calls=0;k.observer_calls=0;k.schema_calls=0
	return k

## 失败必须在真实Step改变候选后返回，且不执行任何travel preview、军事或提交尾处理。
func assert_failed_candidate(k,before: Dictionary,result: Dictionary,label: String) -> void:
	check(not result.get("ok",true) and str(result.get("code","")).begins_with("NativeTravel"),label+"明确provider错误 "+str(result))
	check(k.step_candidate.get("tick")==before.tick+1 and k.step_candidate!=before,label+"真实Step已改变隔离candidate")
	check(k.step_candidate.wallets!=before.wallets and k.step_candidate.inventories!=before.inventories,label+"候选真实旅费与庄园钱粮已变")
	check(k.snapshot()==before,label+"旧state/RNG/tick/请求序号/结果缓存/钱粮完整不发")
	check(k.military_calls==0 and k.observer_calls==0 and k.schema_calls==0,label+"军事/observer/Schema全部未执行")
	check(k.trace.is_empty(),label+"全批校验在第一条preview和begin之前")
	evidence_rows.append({"case":label,"result":result.duplicate(true),"committed_unchanged":k.snapshot()==before,"candidate_tick":k.step_candidate.get("tick"),"committed_tick":before.tick,"wallets_changed_in_candidate":k.step_candidate.wallets!=before.wallets,"inventories_changed_in_candidate":k.step_candidate.inventories!=before.inventories,"military_calls":k.military_calls,"observer_calls":k.observer_calls,"schema_calls":k.schema_calls,"trace":k.trace.duplicate(true)})

## 实际新so故障后用同request恢复并重放，provider序号变化不泄漏为世界请求缓存。
func native_failures() -> void:
	for mode in ["late_bad","scope","build","shape","cpp_failure","wrong_api","hold_scope"]:
		var k=failure_kernel();if k==null:return
		if mode=="hold_scope":
			if not send(k,"move_local",{"place_id":"west.street"}).ok or not send(k,"travel",{"destination":"village.east"}).ok:return
			k.trace.clear();k.military_calls=0;k.observer_calls=0;k.schema_calls=0
		var proxy=BridgeProxy.new();proxy.real=ClassDB.instantiate("HanmoWorldBridge");proxy.mode="scope" if mode=="hold_scope" else mode
		var helper=k._backend.get("_native_travel_schedule_provider")
		if not check(helper!=null,"中央Backend唯一provider持有接缝"):return
		helper._bridge=RefCounted.new() if mode=="wrong_api" else proxy
		var request: Dictionary=command(k,"travel_wait" if mode=="hold_scope" else "advance");var before: Dictionary=k.snapshot()
		var result: Dictionary=k.submit(request)
		assert_failed_candidate(k,before,result,mode)
		if mode=="hold_scope":check(k.step_candidate.actors[before.player_actor_id].location==before.actors[before.player_actor_id].location,"原Journey hold仍保本人位置并外抛provider失败")
		if mode=="late_bad":check(proxy.original_result.get("ok",false) and not proxy.original_result.get("proposals",[]).is_empty(),"坏后项之前确有可开工真实完整前项")
		check(proxy.calls==(0 if mode=="wrong_api" else 1),mode+"没有fallback或再次调用")
		proxy.mode="";helper._bridge=proxy
		var count: int=proxy.calls
		var retried: Dictionary=k.submit(request)
		check(retried.ok and proxy.calls==count+1 and k.snapshot().tick==before.tick+1,mode+"同request恢复provider可成功重试")
		if proxy.requests.size()>1:check(proxy.requests[-1].scope_token!=proxy.requests[0].scope_token,mode+"失败重试新scope不混用")
		var committed: Dictionary=k.snapshot();count=proxy.calls
		check(k.submit(request)==retried and k.snapshot()==committed and proxy.calls==count,mode+"重复成功request不二次provider或扣钱")
		evidence_rows[-1].recovered=retried.ok;evidence_rows[-1].provider_calls=proxy.calls
		evidence_rows[-1].scope_tokens=[]
		for input in proxy.requests:evidence_rows[-1].scope_tokens.append(input.scope_token)
		proxy.real=null;helper._bridge=null

## 无扩展独立进程实证缺库，失败后显式恢复legacy而不是偷偷fallback。
func unavailable_failure() -> void:
	if not check(not ClassDB.class_exists("HanmoWorldBridge"),"缺库进程确无Bridge类"):return
	var k=failure_kernel();if k==null:return
	k.call("configure_native_travel_schedule",true,expected_build)
	var request: Dictionary=command(k,"advance");var before: Dictionary=k.snapshot()
	var result: Dictionary=k.submit(request)
	assert_failed_candidate(k,before,result,"missing_library")
	check(result.get("code")=="NativeTravelBridgeUnavailable","真实缺库明确拒绝")
	k.call("configure_native_travel_schedule",false,{})
	check(k.submit(request).ok,"同request显式恢复legacy后可重试")

## 小工程必须显式导入完毕；新库运行先核明确路径与build，旧oracle文件绝不覆盖。
func run_checks() -> void:
	if not check(ProjectSettings.get_setting("application/config/native_travel_schedule_isolated",false),"专用隔离工程标记"):finish();return
	fixtures=read_json(fixture_path)
	if not check(fixtures.has("travel_enabled") and fixtures.has("travel_default") and not oracle_path.is_empty(),"显式原作者索引与oracle路径"):finish();return
	if unavailable_mode:
		expected_build=read_json(build_path)
		unavailable_failure();finish();return
	if native_mode:
		check(not record_mode,"native不能重写旧oracle")
		expected_build=read_json(build_path)
		if not extension_path.is_empty():check(GDExtensionManager.load_extension(extension_path)==OK,"实际新so显式装载")
		if not check(ClassDB.class_exists("HanmoWorldBridge"),"实际Bridge存在"):finish();return
		Provider=load("res://core/backends/native_travel_schedule_provider.gd");provider=Provider.new();provider.configure(expected_build)
	real_flows()
	if native_mode:native_failures()
	if record_mode and failures==0:
		if check(not FileAccess.file_exists(oracle_path),"旧oracle只能首次捕获"):
			var file=FileAccess.open(oracle_path,FileAccess.WRITE)
			file.store_string(JSON.stringify({"format":"HM-TRAVEL-OLD-ORACLE/1","source_commit":OLD_SOURCE,"rows":rows},"\t",true,true)+"\n");file.close()
	elif not record_mode:
		var oracle: Dictionary=read_json(oracle_path)
		check(oracle.get("source_commit")==OLD_SOURCE and oracle.get("rows")==rows,"全部旧数组/真实状态/trace逐项等价")
	finish()

## 未运行分支不能冒充通过，完整检查数与失败数随退出码保留。
func finish() -> void:
	if not evidence_path.is_empty():
		if check(not FileAccess.file_exists(evidence_path),"证据写入只允许新路径"):
			var file=FileAccess.open(evidence_path,FileAccess.WRITE)
			file.store_string(JSON.stringify({"format":"HM-TRAVEL-NATIVE-EVIDENCE/1","checks":checks,"failures":failures,"native":native_mode,"build":expected_build,"cases":evidence_rows},"\t",true,true)+"\n");file.close()
	print("NATIVE_TRAVEL_SCHEDULE_CHECKS=",checks," FAILURES=",failures," ROWS=",rows.size()," NATIVE=",native_mode)
	quit(1 if failures else 0)
