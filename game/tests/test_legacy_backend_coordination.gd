## 原a7df协调短链的真实Kernel oracle；仅补递归、hold、军旅及调度hook，不重跑生命周期/存读矩阵。
extends "res://tests/test_legacy_backend_ownership.gd"
const Backend=preload("res://core/backends/legacy_world_backend.gd")
const Interaction=preload("res://core/interaction_rules.gd")
const OLD_COORDINATION_SHA="10ad510cf07523569add85c444f87ef6b6a56950f48bb4525b0b33efa744d48b"
var schedule_content: String=""

## 所有观测在原动态入口进行，故障只在真实成功候选之后明确拒绝。
class CoordinationKernel extends Kernel:
	var trace: Array=[]
	var fail_nested: String=""
	var fail_drive: bool=false
	var fail_begin: bool=false
	var fail_journey: bool=false
	var fail_npc: bool=false
	var in_npc: bool=false
	var changed: bool=false
	var last_hold: Array=[]
	var journey_failures: Array=[]
	var npc_before: Dictionary={}
	var npc_after: Dictionary={}
	var npc_rejected: Dictionary={}
	## 递归预览必须继续经过本Kernel，并使用原传入候选。
	func _preview(state: Dictionary,kind: String,params: Dictionary,actor: String) -> Dictionary:
		trace.append(["preview",kind,actor,state.tick,not is_same(state,_state)])
		return super._preview(state,kind,params,actor)
	## 内层开工及NPC子候选先真实应用；失败不能泄漏世界、ID或事实。
	func _apply(state: Dictionary,kind: String,params: Dictionary,actor: String,offer: Dictionary) -> Dictionary:
		trace.append(["apply",kind,actor,state.tick,not is_same(state,_state)])
		var before: Dictionary=state.duplicate(true)
		var result: Dictionary=super._apply(state,kind,params,actor,offer)
		if result.ok and (kind==fail_nested or (in_npc and fail_npc)):
			changed=state!=before
			if in_npc:npc_before=before;npc_rejected=state.duplicate(true)
			return {"ok":false,"code":"InjectedCoordinationApply"}
		return result
	## 兼容旅行真实安装后拒绝，证明新Backend仍调用原可覆盖开工口。
	func _begin_action(state: Dictionary,actor: String,kind: String,params: Dictionary,offer: Dictionary) -> Dictionary:
		trace.append(["begin",kind,actor,state.tick])
		var before: Dictionary=state.duplicate(true);var result: Dictionary=super._begin_action(state,actor,kind,params,offer)
		if result.ok and fail_begin:changed=state!=before;return {"ok":false,"code":"InjectedCoordinationBegin"}
		return result
	## 对白自动发现先真实写候选；再拒绝供外层事务整体丢弃。
	func _drive_dialogue(state: Dictionary,actor: String) -> Dictionary:
		trace.append(["drive",actor,state.tick]);var before: Dictionary=state.duplicate(true)
		var result: Dictionary=super._drive_dialogue(state,actor)
		if result.ok and fail_drive:changed=state!=before;return {"ok":false,"code":"InjectedCoordinationDrive"}
		return result
	## 记录原hold值后照常执行全部真实phase与尾部。
	func _advance(state: Dictionary,hold: Array=[]) -> Dictionary:
		trace.append(["advance",hold.duplicate(),state.tick]);last_hold=hold.duplicate()
		return super._advance(state,hold)
	## 真实Step产生的请求在原军旅边界注错，保留原suspended数组处理。
	func _military_journey(state: Dictionary,request: Dictionary) -> Dictionary:
		trace.append(["journey",request.duplicate(true),state.tick])
		if fail_journey:
			journey_failures.append(state.armies[request.army_id].commander_actor_id)
			return {"ok":false,"code":"InjectedCoordinationJourney"}
		return super._military_journey(state,request)
	## 旅人调度不造提案，只保留原尾调用次序。
	func _schedule_travel(state: Dictionary) -> Dictionary:
		trace.append(["schedule_travel",state.tick]);return super._schedule_travel(state)
	## NPC实际提案失败被原子候选隔离，退出前保留真实外层状态。
	func _schedule_military(state: Dictionary) -> Dictionary:
		trace.append(["schedule_military",state.tick]);in_npc=true
		var result: Dictionary=super._schedule_military(state);in_npc=false;npc_after=state.duplicate(true)
		return result

## 明确拒绝的接线哨兵仅证明九个委托；不参与任何世界成功或oracle生成。
class BoundaryBackend extends Backend:
	var seen: Array=[]
	## 正式preview递归callback必须是临时有效值。
	func preview_command(_state: Dictionary,_kind: String,_params: Dictionary,_actor: String,preview_hook: Callable) -> Dictionary:
		seen.append(["preview",preview_hook.is_valid()]);return {"ok":false,"code":"CoordinationBoundary"}
	## 四条原动态边都由Kernel显式传入，不保存Callable。
	func apply_command(_state: Dictionary,_kind: String,_params: Dictionary,_actor: String,_offer: Dictionary,advance_hook: Callable,begin_hook: Callable,drive_hook: Callable,apply_hook: Callable) -> Dictionary:
		seen.append(["apply",advance_hook.is_valid() and begin_hook.is_valid() and drive_hook.is_valid() and apply_hook.is_valid()]);return {"ok":false,"code":"CoordinationBoundary"}
	## 原兼容开工接线只拒绝，不分配伪行动。
	func begin_action(_state: Dictionary,_actor: String,_kind: String,_params: Dictionary,_offer: Dictionary) -> Dictionary:
		seen.append(["begin",true]);return {"ok":false,"code":"CoordinationBoundary"}
	## 原hold与四条尾hook传入Backend；哨兵不运行任何阶段。
	func advance_world(_state: Dictionary,_hold: Array,drive: Callable,journey: Callable,travel: Callable,military: Callable) -> Dictionary:
		seen.append(["advance",drive.is_valid() and journey.is_valid() and travel.is_valid() and military.is_valid()]);return {"ok":false,"code":"CoordinationBoundary"}
	## 原void释放入口只记录调用，不声称完成领域释放。
	func release_action(_state: Dictionary,_action: Dictionary) -> void:seen.append(["release",true])
	## 对白入口须得到原递归preview和apply。
	func drive_dialogue(_state: Dictionary,_actor: String,preview_hook: Callable,apply_hook: Callable) -> Dictionary:
		seen.append(["drive",preview_hook.is_valid() and apply_hook.is_valid()]);return {"ok":false,"code":"CoordinationBoundary"}
	## 旅人入口只记录有效临时callback。
	func schedule_travel(_state: Dictionary,preview_hook: Callable,begin_hook: Callable) -> Dictionary:
		seen.append(["travel",preview_hook.is_valid() and begin_hook.is_valid()]);return {"ok":false,"code":"CoordinationBoundary"}
	## 军旅入口只记录原开工callback，不造行程。
	func military_journey(_state: Dictionary,_request: Dictionary,begin_hook: Callable) -> Dictionary:
		seen.append(["journey",begin_hook.is_valid()]);return {"ok":false,"code":"CoordinationBoundary"}
	## 军政调度入口只检验临时preview/apply接线。
	func schedule_military(_state: Dictionary,preview_hook: Callable,apply_hook: Callable) -> Dictionary:seen.append(["military",preview_hook.is_valid() and apply_hook.is_valid()]);return {"ok":true}

## 新世界始终由同一正式目录创建；记录实例方便释放原测试资源。
func opened_coordination(path: String="") -> CoordinationKernel:
	var k=CoordinationKernel.new();kernels.append(k)
	check(k.new_game(scenario_id,content_path if path.is_empty() else path).ok,"真实协调目录开局")
	return k

## 公共命令使用当前真实版本/序号，明确预期失败也必须保完整候选不提交。
func send(k,kind: String,params: Dictionary={},actor: String="",expected: String="") -> Dictionary:
	var before: Dictionary=k.save_context();var id: String=before.state.player_actor_id if actor.is_empty() else actor
	var result: Dictionary=k.submit({"kind":kind,"params":params,"actor_id":id,"request_seq":before.state.request_sequences[id]+1,"expected_revision":before.state.revision})
	check(result.ok if expected.is_empty() else result.get("code")==expected,"公共"+kind+" "+str(result))
	if not expected.is_empty():check(k.save_context()==before,"真实失败完整三元组不变 "+kind)
	return result

## 只用现有真实岗位人员生成当面context。
func context(k,label: String) -> Dictionary:return Interaction.for_role(k.snapshot(),k.content().data,"base::"+label)

## 冻结全态、真实结果与逐hook轨迹；不依赖后来新Backend产生期望。
func record(label: String,k,result: Dictionary={}) -> void:
	rows.append({"label":label,"state":k.snapshot(),"result":result.duplicate(true),"trace":k.trace.duplicate(true),"content_id":k.content().content_id})

## 原官署自动发现、递归确认、候选尾拒绝及行动完工对白恢复。
func dialogue() -> void:
	var k=opened_coordination()
	if not send(k,"move_local",{"place_id":"base::west.office"}).ok or not send(k,"talk",{"role_id":"base::role.office"}).ok or not send(k,"dialogue_next").ok:return
	k.fail_drive=true;send(k,"dialogue_choose",{"option_id":"ask"},"","InjectedCoordinationDrive")
	check(k.changed,"drive故障前真实自动发现改变候选");record("dialogue_drive_failure",k);k.fail_drive=false
	if not send(k,"dialogue_choose",{"option_id":"ask"}).ok:return
	check(k.snapshot().knowledge[k.snapshot().player_actor_id].has("base::offer.grain"),"真实自动discover_offer成功")
	if not send(k,"dialogue_next").ok or not send(k,"dialogue_choose",{"option_id":"accept"}).ok:return
	k.fail_nested="accept_offer";send(k,"dialogue_confirm",{},"","InjectedCoordinationApply")
	check(k.changed,"递归confirm内层实际开工后才失败");record("dialogue_nested_failure",k);k.fail_nested=""
	if not send(k,"dialogue_confirm").ok:return
	check(k.interaction_view().phase=="waiting","递归确认保原waiting会话")
	var result: Dictionary=send(k,"advance")
	check(k.interaction_view().phase=="say" and k.snapshot().tasks.size()==1,"实际完工后经原drive恢复对白")
	record("dialogue_completed",k,result)

## 两个真实旅人对向出发；本人hold不阻另一人完成，开始失败仍整体回滚。
func travel_hold() -> void:
	var k=opened_coordination();var actor: String=k.snapshot().player_actor_id
	if not send(k,"move_local",{"place_id":"base::east.entrance"},"base::actor.he").ok:return
	k.fail_begin=true;send(k,"travel",{"destination":"base::village.east"},"","InjectedCoordinationBegin")
	check(k.changed,"实际旅行安装后才拒绝");record("begin_failure",k);k.fail_begin=false
	if not send(k,"travel",{"destination":"base::village.east"}).ok or not send(k,"travel",{"destination":"base::city.west"},"base::actor.he").ok:return
	var before: Dictionary=k.snapshot();var result: Dictionary=send(k,"travel_wait")
	check(result.ok and k.last_hold==[actor] and k.snapshot().actors[actor].location==before.actors[actor].location and k.snapshot().tick==before.tick+1,"原单人hold仅推进一次且保本人位置")
	check(k.snapshot().actors["base::actor.he"].location.get("settlement_id")=="base::city.west","另一真实旅人按原阶段抵达")
	record("travel_hold",k,result)

## 作者原计划生成真实NPC提案；首拍故障丢子候选，次拍正常开始，旅人出发不提前结算。
func schedules() -> void:
	var k=opened_coordination(schedule_content);k.fail_npc=true
	if not send(k,"wait").ok or not send(k,"advance").ok:return
	var state: Dictionary=k.snapshot();var traveller: String="base::actor.walker";var military: String="base::actor.military.b.commander"
	check(k.changed and not k.npc_rejected.is_empty() and k.npc_before==k.npc_after,"NPC真实apply成功后拒绝只丢自己子候选")
	check(k.npc_rejected.next_instance_id>k.npc_before.next_instance_id and state.actors[military].action_id.is_empty(),"子候选实际分ID/行动而外层没有该行动")
	var action: Dictionary=state.actions.get(state.actors[traveller].action_id,{})
	check(action.get("kind")=="travel" and action.start_tick==state.tick and state.actors[traveller].location.progress==0,"旅人真实提案开始时不提前走下一拍")
	record("npc_rejected_traveller_started",k);k.fail_npc=false
	if not send(k,"wait").ok or not send(k,"advance").ok:return
	state=k.snapshot();check(state.actions.get(state.actors[military].action_id,{}).get("kind")=="office_accept","原NPC下一次真实提案成功开始")
	check(state.actors[traveller].location.get("settlement_id")=="base::city.west" and state.actors[traveller].action_id.is_empty(),"实际旅人下一拍才抵达")
	record("npc_started_traveller_arrived",k)
	if not send(k,"wait").ok or not send(k,"advance").ok:return
	check(k.snapshot().offices["base::office.military.b"].enabled and k.snapshot().campaigns.size()==1,"原军职完工同尾真实自动受令")
	record("npc_campaign_accepted",k)

## 三军真实受任编组；B外出而B2留守，合法交涉易帜后B在下一真实phase请求返营。
func military_tail() -> void:
	var k=opened_coordination();var campaigns: Dictionary={}
	for label in ["a","b","b2"]:
		if not send(k,"office_accept",{"office_id":"base::office.military."+label,"context":context(k,"role.military."+label)},"base::actor.military."+label+".commander").ok:return
	if not send(k,"advance").ok:return
	for label in ["a","b","b2"]:
		var actor: String="base::actor.military."+label+".commander"
		var accepted: Dictionary=send(k,"campaign_accept",{"template_id":"base::order.military."+label,"context":context(k,"role.military."+label)},actor)
		if not accepted.ok:return
		campaigns[label]=accepted.details.campaign_id
		if not send(k,"army_form",{"order_id":campaigns[label],"unit_ids":["base::unit.military."+label],"food_quantity":160 if label=="a" else 0,"money":20},actor).ok:return
	if not send(k,"advance").ok:return
	for label in ["a","b","b2"]:
		if not send(k,"campaign_depart",{"campaign_id":campaigns[label]},"base::actor.military."+label+".commander").ok:return
	if not send(k,"travel",{"destination":"base::village.east"},"base::actor.military.b.commander").ok or not send(k,"advance").ok:return
	var started: Dictionary=send(k,"siege_start",{"campaign_id":campaigns.a})
	if not started.ok:return
	var siege: String=started.details.siege_id;var params: Dictionary={"siege_id":siege,"command":{"kind":"negotiate","terms":"safe_departure"}}
	var ready: bool=false
	for _half in range(6):
		if k.preview("siege_strategy",params).ok:ready=true;break
		if not send(k,"siege_plan",{"siege_id":siege,"orders":{"base::unit.military.a":{"action":"blockade","node_id":"supply"}}}).ok or not send(k,"advance").ok:return
	if not check(ready,"同原作者有限围困达到真实安全离城条款"):return
	if not send(k,"siege_strategy",params).ok or not send(k,"siege_close",{"siege_id":siege}).ok:return
	k.fail_journey=true;var result: Dictionary=send(k,"advance")
	check(not k.journey_failures.is_empty(),"真实占领失权在Campaign.step生成尾军旅行程请求")
	for actor in k.journey_failures:check(actor in result.get("details",{}).get("suspended_actors",[]),"真实请求失败保持原suspended数组别名")
	check(k.snapshot().campaigns[campaigns.b].status=="returning","原失权军令仍进入真实返营状态")
	record("military_tail_suspended",k,result)

## 委托拒绝器只运行在真实短链oracle之后，不能伪造被接受状态。
func delegation() -> void:
	var original_backend=Backend.new();var missing: bool=false
	for method in ["preview_command","apply_command","begin_action","advance_world","release_action","drive_dialogue","schedule_travel","military_journey","schedule_military"]:
		if not check(original_backend.has_method(method),"Backend提供原协调主体 "+method):missing=true
	if missing:return
	var k=Kernel.new();check(k.new_game(scenario_id,content_path).ok,"接线哨兵前仍用真实世界")
	var idle: Dictionary=k.snapshot();var actor: String=idle.player_actor_id;var quote: Dictionary=k.preview("wait")
	check(k.submit(command(k)).ok,"接线释放/推进输入来自真实wait")
	var waiting: Dictionary=k.snapshot();var action: Dictionary=waiting.actions[waiting.actors[actor].action_id]
	var probe=BoundaryBackend.new();probe.publish(k._state,k._catalog,k._registry);k._backend=probe
	check(k._preview(idle.duplicate(true),"wait",{},actor).get("code")=="CoordinationBoundary","preview主体经过Backend")
	check(k._apply(idle.duplicate(true),"wait",{},actor,quote).get("code")=="CoordinationBoundary","apply主体经过Backend")
	check(k._begin_action(idle.duplicate(true),actor,"wait",{},quote).get("code")=="CoordinationBoundary","begin主体经过Backend")
	check(k._advance(waiting.duplicate(true)).get("code")=="CoordinationBoundary","advance主体经过Backend")
	k._release_action(waiting.duplicate(true),action);check(not probe.seen.is_empty() and probe.seen.back()[0]=="release","release入口经过Backend")
	check(k._drive_dialogue(kernels[0].snapshot(),actor).get("code")=="CoordinationBoundary","drive主体经过Backend")
	k._schedule_travel(idle.duplicate(true));check(not probe.seen.is_empty() and probe.seen.back()[0]=="travel","travel调度主体经过Backend")
	var military: Dictionary=kernels.back().snapshot();var campaign: Dictionary={}
	for current in military.campaigns.values():
		if current.actor_id=="base::actor.military.b.commander":campaign=current
	if check(not campaign.is_empty(),"军旅接线使用真实已形成军团"):
		check(k._military_journey(military,{"army_id":campaign.army_id,"destination_settlement_id":"base::village.east"}).get("code")=="CoordinationBoundary","journey入口经过Backend")
	k._schedule_military(idle.duplicate(true));check(not probe.seen.is_empty() and probe.seen.back()[0]=="military","military调度主体经过Backend")
	check(probe.seen==[["preview",true],["apply",true],["begin",true],["advance",true],["release",true],["drive",true],["travel",true],["journey",true],["military",true]],"九个主体各调用一次且临时callback有效")

## typed Variant保存原完整状态与输出，只有精确旧Kernel可首次生成，后续只读逐行比较。
func oracle(args: PackedStringArray) -> void:
	for argument in args:
		if argument.begins_with("--record-oracle="):
			var path: String=argument.trim_prefix("--record-oracle=")
			if not check(failures==0 and FileAccess.get_file_as_string("res://core/world_kernel.gd").sha256_text()==OLD_COORDINATION_SHA and not FileAccess.file_exists(path),"只能由无失败原a7df首次冻结"):return
			var file=FileAccess.open(path,FileAccess.WRITE);file.store_var({"kernel_sha256":OLD_COORDINATION_SHA,"rows":rows},false);file.close();return
		if argument.begins_with("--oracle="):
			var file=FileAccess.open(argument.trim_prefix("--oracle="),FileAccess.READ);var expected: Variant=file.get_var(false) if file!=null else null
			if not check(expected is Dictionary and expected.get("kernel_sha256")==OLD_COORDINATION_SHA,"原协调oracle身份"):return
			check(rows.size()==expected.rows.size(),"原协调短链记录数一致")
			for index in mini(rows.size(),expected.rows.size()):check(rows[index]==expected.rows[index],"原完整协调记录相等 "+rows[index].label)
			return
	check(false,"必须明确原oracle读取/首次记录路径")

## 同一正式作者目录及独立原计划变体均显式输入，不依赖安装索引顺序。
func run() -> void:
	var args: PackedStringArray=OS.get_cmdline_user_args()
	if not configure_input(args):finish_coordination();return
	for arg in args:
		if arg.begins_with("--schedule-content="):schedule_content=arg.trim_prefix("--schedule-content=")
	if not check(not schedule_content.is_empty() and FileAccess.file_exists(schedule_content),"明确提供正式调度变体目录"):finish_coordination();return
	dialogue();travel_hold();schedules();military_tail();oracle(args)
	if "--backend-required" in args:delegation()
	finish_coordination()

## 不运行Codec/Native，也不拿此门冒充完整领域或UI验收。
func finish_coordination() -> void:
	var backends: Array=[];var references: Array=[]
	for index in kernels.size():backends.append(kernels[index]._backend);references.append(weakref(kernels[index]))
	kernels.clear()
	for index in references.size():
		var fields: Array=[]
		for property in backends[index].get_property_list():
			if property.usage&PROPERTY_USAGE_SCRIPT_VARIABLE:fields.append(property.name)
		fields.sort()
		check(references[index].get_ref()==null and fields==["catalog","registry","state"] and not backends[index].snapshot().is_empty(),"成功/失败真实短链的临时hook均释放，Backend只持三元组 "+str(index))
	print("LEGACY_BACKEND_COORDINATION_CHECKS=",checks," FAILURES=",failures);quit(0 if failures==0 else 1)
