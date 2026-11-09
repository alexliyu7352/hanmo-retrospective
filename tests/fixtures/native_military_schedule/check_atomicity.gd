## 从冻结旧实际在途/编组保存点核军事provider整批失败；不重跑收养或Codec，不伪造native成功。
extends SceneTree
const Kernel=preload("res://core/world_kernel.gd")
const Strict=preload("res://core/strict_json.gd")
const Probe=preload("res://tests/native_oracle_probe.gd")
const MILITARY_KINDS=["office_accept","campaign_accept","army_form","campaign_depart","campaign_return","campaign_cancel","campaign_deliver","siege_start","siege_close","army_return","campaign_report"]
var checks: int=0
var failures: int=0
var fixture_path: String=""
var oracle_path: String=""
var build_path: String=""
var extension_path: String=""
var evidence_path: String=""
var missing_library: bool=false
var expected_build: Dictionary={}
var catalog_path: String=""
var checkpoint: Dictionary={}
var rows: Array=[]

## 只读记录旅行尾部和整候选；局部apply故障在真实写入后返回失败，其他行为走原实现。
class WitnessKernel extends Kernel:
	var travel_calls: int=0
	var travel_before: Dictionary={}
	var travel_after: Dictionary={}
	var candidate_after: Dictionary={}
	var military_previews: int=0
	var military_applies: int=0
	var observer_calls: int=0
	var schema_calls: int=0
	var reject_actor: String=""
	var rejected_candidate: Dictionary={}
	var rejected_calls: int=0
	var trace: Array=[]
	## 包装实际validator与只读observer，仅观察有无到达后续阶段，不放宽任何校验。
	func _module_descriptors() -> Array:
		var modules: Array=super._module_descriptors().duplicate(true)
		for module in modules:
			for validator in module.validators:validator.callback=_validate_witness.bind(validator.callback)
			if module.id=="interaction":module.observers.append({"id":"test.military_atomic_tail","owner":"interaction","before":[],"after":["interaction.events_reconcile"],"callback":_observe_witness})
		return modules
	## 原校验始终实际运行，计数在每次受测submit前重置。
	func _validate_witness(state: Dictionary,data: Dictionary,original: Callable) -> Dictionary:
		schema_calls+=1;return original.call(state,data)
	## 本观察器不改world，只证明provider失败是否越过应停止的尾部。
	func _observe_witness(_state: Dictionary,_data: Dictionary,_context: Dictionary) -> Dictionary:
		observer_calls+=1;return {"ok":true}
	## 所有军事正式preview均可见；预览失败仍由原Backend决定continue。
	func _preview(state: Dictionary,kind: String,params: Dictionary,actor: String) -> Dictionary:
		if kind in MILITARY_KINDS:military_previews+=1;trace.append(["preview",kind,actor])
		return super._preview(state,kind,params,actor)
	## 先执行真实子candidate写入，再注错，验证失败子candidate不泄漏且后续NPC继续。
	func _apply(state: Dictionary,kind: String,params: Dictionary,actor: String,offer: Dictionary) -> Dictionary:
		if kind in MILITARY_KINDS:military_applies+=1;trace.append(["apply",kind,actor])
		var result: Dictionary=super._apply(state,kind,params,actor,offer)
		if kind=="office_accept" and actor==reject_actor and result.ok:
			rejected_calls+=1;rejected_candidate=state.duplicate(true)
			return {"ok":false,"code":"InjectedMilitaryApplyAfterWrite"}
		return result
	## 原Step之后完整旅行尾部实际运行，保抵达游标修正，绝不代写“已完成”证据。
	func _schedule_travel(state: Dictionary) -> Dictionary:
		travel_calls+=1;travel_before=state.duplicate(true)
		var result: Dictionary=super._schedule_travel(state)
		travel_after=state.duplicate(true);trace.append(["travel_tail",state.tick])
		return result
	## 无论返回成功或失败都记录原协调器留下的实际隔离候选。
	func _advance(state: Dictionary,hold: Array=[]) -> Dictionary:
		var result: Dictionary=super._advance(state,hold)
		candidate_after=state.duplicate(true)
		return result
	## 只清本测试观察数据，不修改世界、请求序号、provider或原生缓存。
	func reset_witness() -> void:
		travel_calls=0;military_previews=0;military_applies=0;observer_calls=0;schema_calls=0
		travel_before={};travel_after={};candidate_after={};rejected_candidate={};rejected_calls=0;trace=[]

## 引擎完成SceneTree建立后才读明确参数并运行。
func _init() -> void:call_deferred("run_checks")

## 断言必须反映在非零退出码，原始错误信息一并保留。
func check(value: bool,label: String) -> bool:
	checks+=1
	if not value:failures+=1;printerr("FAIL: ",label)
	return value

## 严格读取固定oracle/build与正式作者索引，保整数类型与来源语义。
func read_json(path: String) -> Dictionary:
	if path.is_empty() or not FileAccess.file_exists(path):return {}
	var parsed: Dictionary=Strict.parse_text(FileAccess.get_file_as_string(path),16777216)
	return parsed.value if parsed.get("ok",false) and parsed.get("value") is Dictionary else {}

## 每个故障复用同一真实旧保存点，经原Schema恢复；不构造新的旅费或军团。
func opened(use_checkpoint: bool=true):
	var k=WitnessKernel.new()
	if not check(k.new_game("scenario.alpha_life",catalog_path).ok,"原军事作者正式开局"):return null
	if use_checkpoint and not check(k.restore(checkpoint).ok and k.snapshot()==checkpoint,"完整恢复冻结旧实际在途/编组点"):return null
	return k

## 真实命令信封保原修订号与永久本人请求序列；重试复用同一完整信封。
func request(k,kind: String) -> Dictionary:
	var state: Dictionary=k.snapshot();var actor: String=state.player_actor_id
	return {"kind":kind,"params":{},"actor_id":actor,"request_seq":state.request_sequences[actor]+1,"expected_revision":state.revision}

## 在真实到达后的候选阻断军政provider，整已提交状态和全部后续阶段均应保持未发布。
func provider_failure(fault: String) -> void:
	var k=opened();if k==null:return
	k.call("configure_native_military_schedule",true,expected_build)
	var provider=k._backend.get("_native_military_schedule_provider")
	if not check(provider!=null,"中央唯一军政provider接缝存在"):return
	var proxy
	if not missing_library:
		proxy=Probe.FaultBridge.new();proxy.real=ClassDB.instantiate("HanmoWorldBridge");proxy.mode=fault;provider._bridge=proxy
	var command: Dictionary=request(k,"advance");var before: Dictionary=k.snapshot()
	var traveler: String="actor.bandit";var old_action: String=before.actors[traveler].action_id
	if not check(not old_action.is_empty() and before.actions[old_action].kind=="travel" and before.itineraries[traveler].travelling,"旧实际来源确有将在本半日完成的旅行"):return
	k.reset_witness()
	var result: Dictionary=k.submit(command)
	check(not result.get("ok",true) and str(result.get("code","")).begins_with("NativeMilitary"),fault+"真实provider失败原样外抛："+str(result))
	check(k.snapshot()==before,fault+"已提交state/RNG/请求缓存/钱粮/行动逐字段不变")
	check(k.travel_calls==1 and k.candidate_after.get("tick")==before.tick+1,fault+"原Step及完整旅行尾已实际运行")
	check(not k.travel_after.get("actions",{}).has(old_action) and k.travel_after.get("actors",{}).get(traveler,{}).get("location")!=before.actors[traveler].location,fault+"旅行行动真实完成且候选位置已抵达")
	check(k.travel_before.get("itineraries")!=k.travel_after.get("itineraries") and not k.travel_after.itineraries[traveler].travelling,fault+"finish_arrivals已真实改变抵达游标")
	check(k.candidate_after.wallets!=before.wallets and k.candidate_after.inventories!=before.inventories,fault+"候选已有真实钱粮变化而整批回滚")
	check(k.military_previews==0 and k.military_applies==0 and k.observer_calls==0 and k.schema_calls==0,fault+"完整批次拒绝早于首军事preview/apply及observer/Schema")
	if fault=="bad_late":check(proxy.original.get("ok",false) and not proxy.original.get("proposals",[]).is_empty(),"坏后项之前确有真实有效native前项")
	if missing_library:check(result.get("code")=="NativeMilitaryBridgeUnavailable","独立进程真实缺库错误")
	var row: Dictionary={"case":fault,"result":result,"committed_unchanged":k.snapshot()==before,"candidate_tick":k.candidate_after.get("tick"),"before_tick":before.tick,"travel_calls":k.travel_calls,"traveler_before":before.actors[traveler].duplicate(true),"traveler_candidate":k.travel_after.actors[traveler].duplicate(true),"itinerary_before":k.travel_before.itineraries[traveler].duplicate(true),"itinerary_after":k.travel_after.itineraries[traveler].duplicate(true),"military_previews":k.military_previews,"military_applies":k.military_applies,"observer_calls":k.observer_calls,"schema_calls":k.schema_calls,"wallets_changed_in_candidate":k.candidate_after.wallets!=before.wallets,"inventories_changed_in_candidate":k.candidate_after.inventories!=before.inventories,"trace":k.trace.duplicate(true)}
	if missing_library:k.call("configure_native_military_schedule",false,{})
	else:proxy.mode=""
	var count: int=0 if missing_library else proxy.calls
	var recovered: Dictionary=k.submit(command)
	check(recovered.ok and k.snapshot().tick==before.tick+1,fault+"相同request恢复真实provider或显式legacy后可重试")
	if not missing_library:check(proxy.calls==count+1 and proxy.scopes[-1]!=proxy.scopes[0],fault+"重试恰调用一次新scope")
	var committed: Dictionary=k.snapshot();count=0 if missing_library else proxy.calls
	check(k.submit(command)==recovered and k.snapshot()==committed and (missing_library or proxy.calls==count),fault+"成功request重放不再次provider或结算")
	row.recovered=recovered.ok;rows.append(row)
	provider._bridge=null
	if proxy!=null:proxy.real=null

## 单条合法native提案apply后拒绝仅丢本子candidate，后续原序NPC仍办理。
func apply_continue() -> void:
	var k=opened(false);if k==null:return
	if not check(k.submit(request(k,"wait")).ok,"真实等待为原半日门提供当前动作"):return
	k.call("configure_native_military_schedule",true,expected_build)
	var before: Dictionary=k.snapshot();k.reject_actor="actor.military.a.commander";k.reset_witness()
	var result: Dictionary=k.submit(request(k,"advance"));var state: Dictionary=k.snapshot()
	check(result.ok and k.rejected_calls==1,"第一条真实军事apply写后失败不升级为整世界失败")
	check(not k.rejected_candidate.actors[k.reject_actor].action_id.is_empty() and state.actors[k.reject_actor].action_id.is_empty(),"第一NPC实际创建的子candidate行动没有泄漏")
	check(not state.actors["actor.military.b.commander"].action_id.is_empty() and k.military_previews==2 and k.military_applies==2,"第二NPC沿原完整批次继续通过正式preview/apply")
	check(k.observer_calls==1 and k.schema_calls>0 and state.tick==before.tick+1,"单条continue仍完成原尾观察/Schema及一次发布")
	rows.append({"case":"apply_after_write_continue","result":result,"rejected_calls":k.rejected_calls,"trace":k.trace,"first_actor_committed_action":state.actors[k.reject_actor].action_id,"first_actor_rejected_action":k.rejected_candidate.actors[k.reject_actor].action_id,"second_actor_action":state.actors["actor.military.b.commander"].action_id,"observer_calls":k.observer_calls,"schema_calls":k.schema_calls})
	k._backend.get("_native_military_schedule_provider")._bridge=null

## 缺共享接口是明确红门；接口与真实新库都齐备后才开始该对应分支。
func run_checks() -> void:
	for arg in OS.get_cmdline_user_args():
		if arg.begins_with("--dynamic-content="):fixture_path=arg.trim_prefix("--dynamic-content=")
		if arg.begins_with("--oracle="):oracle_path=arg.trim_prefix("--oracle=")
		if arg.begins_with("--build="):build_path=arg.trim_prefix("--build=")
		if arg.begins_with("--extension="):extension_path=arg.trim_prefix("--extension=")
		if arg.begins_with("--evidence="):evidence_path=arg.trim_prefix("--evidence=")
		if arg=="--missing-library":missing_library=true
	var sample=Kernel.new()
	if not check(sample.has_method("configure_native_military_schedule"),"中央军事共享hook接口必须存在"):finish();return
	sample=null
	var fixtures: Dictionary=read_json(fixture_path);var oracle: Dictionary=read_json(oracle_path);expected_build=read_json(build_path)
	if not check(fixtures.has("military") and oracle.get("source_commit")=="a4fa824f308b5b33bd602a7286dae19219dde50f" and not expected_build.is_empty(),"明确原作者/固定旧oracle/预冻结build"):finish();return
	catalog_path=fixtures.military.catalog
	for row in oracle.rows:
		if row.label=="actual_checkpoint/动态真实驻军编组中":checkpoint=row.state
	if not check(not checkpoint.is_empty(),"旧oracle存在真实在途/编组检查点"):finish();return
	if missing_library:
		if not check(not ClassDB.class_exists("HanmoWorldBridge"),"独立缺库进程没有Bridge类"):finish();return
		provider_failure("missing_library")
	else:
		if not check(not extension_path.is_empty() and GDExtensionManager.load_extension(extension_path)==OK and ClassDB.class_has_method("HanmoWorldBridge","propose_military_schedule"),"冻结真实军事so可装载且具有实际API"):finish();return
		for fault in ["bad_late","scope","cpp_failure"]:provider_failure(fault)
		apply_continue()
	finish()

## 回执只存本次真实结果，旧oracle只读；未实际运行不得计通过。
func finish() -> void:
	if not evidence_path.is_empty():
		if check(not FileAccess.file_exists(evidence_path),"本次回执不得覆盖"):
			var file=FileAccess.open(evidence_path,FileAccess.WRITE)
			if check(file!=null,"本次回执可写"):
				file.store_string(JSON.stringify({"format":"HM-MILITARY-ATOMICITY/1","checks":checks,"failures":failures,"missing_library":missing_library,"build":expected_build,"rows":rows},"\t",true,true)+"\n");file.close()
	print("NATIVE_MILITARY_ATOMICITY_CHECKS=",checks," FAILURES=",failures," CASES=",rows.size())
	quit(1 if failures else 0)
