## 围城五命令必须命中真实登记入口；局部旧档、返程失败和NPC提案各自验证原子边界。
extends "res://tests/test_siege_timeout_return.gd"
const Builtin=preload("res://core/runtime/builtin_modules.gd")
const Registry=preload("res://core/runtime/module_registry.gd")
const Schedule=preload("res://core/military_schedule.gd")
const Commands=preload("res://core/domains/military/siege_commands.gd")
const Coordination=preload("res://core/domains/military/military_coordination.gd")
var legacy_path: String=""
var legacy=null
var compared: int=0

## 可选旧源码只从明确输入加载；未传入时不能计作独立旧源等价证据。
func _init() -> void:
	for arg in OS.get_cmdline_user_args():
		if arg.begins_with("--legacy-kernel="): legacy_path=arg.trim_prefix("--legacy-kernel=")
	call_deferred("run_checks")

## 只观察真实规则调用或在写候选后注错，不复制围城或行旅公式。
class Probe extends RefCounted:
	var calls: Dictionary={}
	var fail_kind: String=""
	## 真预览必须从登记Callable抵达，额外参数不得变成资产或控制权限。
	func preview(state: Dictionary,data: Dictionary,actor: String,kind: String,params: Dictionary) -> Dictionary:
		calls[kind+":preview"]=calls.get(kind+":preview",0)+1
		return Commands.preview(state,data,actor,kind,params)
	## 真apply先发生候选写入，才注入失败以验证外层完整丢弃。
	func apply(state: Dictionary,data: Dictionary,actor: String,kind: String,params: Dictionary,offer: Dictionary) -> Dictionary:
		calls[kind+":apply"]=calls.get(kind+":apply",0)+1
		var result: Dictionary=Commands.apply(state,data,actor,kind,params,offer)
		return {"ok":false,"code":"InjectedSiegeApply"} if result.ok and kind==fail_kind else result

## 协调顺序探针只观察已有真实返程请求，不创建第二套移动或受伤规则。
class JourneyOrderProbe extends RefCounted:
	var observed: bool=false
	## 必须在起步Callable前看到人物变化；固定拒绝使测试候选永远不会成为真世界。
	func begin(state: Dictionary,actor_id: String,_kind: String,_params: Dictionary,_offer: Dictionary) -> Dictionary:
		observed=state.actors[actor_id].injury>=1 and state.actors[actor_id].hp<=70
		return {"ok":false,"code":"ObservedSiegeJourneyOrder"}

## 测试只替换围城登记项；其他军事命令仍用发行代码绑定，避免扩大探针权限。
class TestKernel extends "res://core/world_kernel.gd":
	var probe=Probe.new()
	var mutation: String=""
	var selected: String="siege_plan"
	var fail_journey: bool=false
	## 启动故障通过正式new_game合同拒绝，不在测试外层另写覆盖校验。
	func _module_descriptors() -> Array:
		var rows: Array=super._module_descriptors()
		var moved: Dictionary={}
		for descriptor in rows:
			if descriptor.id!="military": continue
			for command in descriptor.commands.duplicate():
				if not command.kind in SiegeWorld.KINDS: continue
				command.preview=probe.preview; command.apply=probe.apply
				if command.kind!=selected: continue
				if mutation=="missing": descriptor.commands.erase(command)
				if mutation=="duplicate": descriptor.commands.append(command.duplicate(true))
				if mutation=="gate": command.gate_id="legacy_kernel"
				if mutation=="owner": moved=command; descriptor.commands.erase(command)
		if not moved.is_empty():
			for descriptor in rows:
				if descriptor.id=="tasks": descriptor.commands.append(moved)
		return rows
	## 行旅起步在真实行动创建后失败；围城、人物和资源都只能存在于被丢弃的候选。
	func _begin_action(state: Dictionary,actor_id: String,kind: String,params: Dictionary,offer: Dictionary) -> Dictionary:
		var result: Dictionary=super._begin_action(state,actor_id,kind,params,offer)
		return {"ok":false,"code":"InjectedSiegeJourney"} if fail_journey and kind=="travel" and result.ok else result

## 各真实kind缺失、重复、错误gate或借owner都必须阻断启动并保留已开局状态。
func coverage() -> void:
	var kinds: Array=[]
	for entry in Commands.descriptor().commands: kinds.append(entry.kind)
	check(kinds==SiegeWorld.KINDS and Commands.command_kinds()==SiegeWorld.KINDS,"围城描述精确来自原真实集合")
	check(Commands.gate_id("unknown").is_empty(),"未知kind没有围城早门权限")
	var registered: Dictionary=Registry.build(Builtin.descriptors())
	if not check(registered.ok,"发行完整注册表可构建"): return
	for kind in SiegeWorld.KINDS:
		var entry: Dictionary=registered.commands.get(kind,{})
		check(entry.get("owner")=="military" and entry.get("mode")=="instant" and entry.get("gate_id")=="military_siege_early","真实围城kind声明唯一owner、即时模式与原早门 "+kind)
	if failures: return
	for kind in SiegeWorld.KINDS:
		var k=TestKernel.new()
		if not check(k.new_game().ok,"启动缺项检查的旧世界"): return
		var before: Dictionary=k.snapshot(); var catalog: Dictionary=k.content()
		k.selected=kind; k.mutation="missing"
		check(k.new_game().get("code")=="MissingBuiltinCommand" and k.snapshot()==before and k.content()==catalog,"漏真实kind拒绝且旧世界不变 "+kind)
		check(k.preview("siege_plan",{}).get("code")=="UnknownSiege","失败启动仍使用旧围城登记")
	for pair in [["duplicate","DuplicateCommand"],["gate","InvalidBuiltinCommand"],["owner","InvalidBuiltinCommand"]]:
		var k=TestKernel.new(); check(k.new_game().ok,"坏登记检查前真实开局")
		var before: Dictionary=k.snapshot(); k.mutation=pair[0]
		check(k.new_game().get("code")==pair[1] and k.snapshot()==before,"坏围城登记拒绝 "+pair[0])

## 每个真实提交同时验证重复请求，失败不消耗请求序号或改变完整状态。
func send(k,kind: String,params: Dictionary={}) -> bool:
	var before: Dictionary=k.snapshot(); var actor: String=before.player_actor_id
	var command: Dictionary={"kind":kind,"params":params,"actor_id":actor,"request_seq":before.request_sequences[actor]+1,"expected_revision":before.revision}
	var old_result: Dictionary={}
	if legacy!=null and kind in SiegeWorld.KINDS:
		if not check(legacy.restore(before).ok,"原旧Kernel恢复同一合法围城状态"): return false
		old_result=legacy.submit(command)
	var result: Dictionary=k.submit(command)
	if not check(result.ok,kind+": "+str(result)): return false
	var after: Dictionary=k.snapshot()
	if legacy!=null and kind in SiegeWorld.KINDS:
		check(old_result==result and legacy.snapshot()==after,"原旧源码完整围城提交结果及状态等价 "+kind)
		compared+=1
	return check(k.submit(command)==result and k.snapshot()==after,"同请求重放完整幂等 "+kind)

## 在真实撤离结果的隔离候选中加入明确测试伤情，验证人物回写先于真实行旅起步请求。
func coordination_order(k,planned: Dictionary,params: Dictionary) -> void:
	var candidate: Dictionary=planned.duplicate(true); var actor: String=planned.player_actor_id
	var result: Dictionary=SiegeWorld.apply(candidate,k.content().data,actor,"siege_strategy",params)
	if not check(result.ok and not result.details.journey_requests.is_empty(),"顺序探针由真实撤军产生行旅请求"): return
	if not check(planned.actors[actor].hp>70 and planned.actors[actor].injury==0,"人物顺序探针有可辨别的未伤初态"): return
	result.details.actor_changes.append({"actor_id":actor,"status":"wounded"})
	var probe=JourneyOrderProbe.new()
	var coordinated: Dictionary=Coordination.after_siege_command(candidate,k.content().data,"siege_strategy",result,probe.begin)
	check(coordinated.get("code")=="ObservedSiegeJourneyOrder" and probe.observed,"人物变化必须先于行旅起步，行旅失败原样外抛")
	check(k.snapshot()==planned,"显式顺序探针不提交任何候选人物、资产或行旅变化")

## 以现有正式军营、河谷战、三半日期限走到围城；命中早门后再验证撤军的真实协作出口。
func run_checks() -> void:
	coverage()
	if failures: finish(); return
	world=TestKernel.new()
	if not open_fixture(): finish(); return
	if not legacy_path.is_empty():
		var script=load(legacy_path)
		if not check(script!=null,"显式原旧Kernel源码可以加载"): finish(); return
		legacy=script.new()
		var active=world; world=legacy
		var opened: bool=open_fixture(); world=active
		if not opened: finish(); return
	if not reach_siege(): finish(); return
	var k=world; var initial: Dictionary=k.snapshot(); var data: Dictionary=k.content().data; var actor: String=initial.player_actor_id
	var army: String=initial.campaigns[campaign_id].army_id
	for kind in SiegeWorld.KINDS:
		var params: Dictionary={"siege_id":siege_id,"campaign_id":campaign_id,"untrusted":{"army_id":"not-real","money":9999}}
		var expected: Dictionary=SiegeWorld.preview(initial,data,actor,kind,params)
		for _repeat in range(3): check(k._preview(initial,kind,params,actor)==expected,"早门原诊断与只读报价一致 "+kind)
		check(k.probe.calls.get(kind+":preview",0)>0,"正式Kernel调用登记preview "+kind)
		var old: Dictionary=initial.duplicate(true); var candidate: Dictionary=initial.duplicate(true)
		check(Commands.apply(candidate,data,actor,kind,params,{"ok":true})==SiegeWorld.apply(old,data,actor,kind,params) and candidate==old,"包括增援的原始apply重新核验及完整候选等价 "+kind)
	check(initial==k.snapshot() and data==k.content().data,"重复预览完整世界和目录零变异")
	check(k.preview("campaign_return",{"campaign_id":campaign_id}).get("code")=="SiegePending","围城专属早门不放宽普通命令占用")
	check(k.preview("siege_plan",{},"not-real").get("code")=="UnknownActor","未知人物仍先于围城门拒绝")
	var orders: Dictionary={}; var siege: Dictionary=initial.sieges[siege_id]
	var obs: Dictionary=SiegeWorld.observe(initial,data,siege)
	for id in obs.control_sets.get(actor,[]):
		if Siege._available(siege,obs,id): orders[id]={"action":"wait","node_id":siege.unit_nodes[id]}
	var params: Dictionary={"siege_id":siege_id,"orders":orders}
	k.probe.fail_kind="siege_plan"
	check(F.send(k,"siege_plan",params).get("code")=="InjectedSiegeApply" and k.snapshot()==initial,"真实计划写入后apply失败完整回滚")
	k.probe.fail_kind=""
	if not send(k,"siege_plan",params) or not restore_same("登记围城已交军议"): finish(); return
	var planned: Dictionary=k.snapshot()
	params={"siege_id":siege_id,"command":{"kind":"withdraw","army_id":army}}
	coordination_order(k,planned,params)
	k.probe.fail_kind="siege_strategy"
	check(F.send(k,"siege_strategy",params).get("code")=="InjectedSiegeApply" and k.snapshot()==planned,"真实撤军和军令改写后apply失败完整回滚")
	k.probe.fail_kind=""; k.fail_journey=true
	check(F.send(k,"siege_strategy",params).get("code")=="InjectedSiegeJourney" and k.snapshot()==planned,"真实撤军后行旅起步失败完整回滚")
	k.fail_journey=false
	if not send(k,"siege_strategy",params) or not restore_same("登记撤军结果与返程"): finish(); return
	var terminal: Dictionary=k.snapshot()
	check(terminal.sieges[siege_id].phase=="result" and terminal.sieges[siege_id].result.kind=="withdrawn","原领域生成真实撤军终局")
	check(terminal.wallets==planned.wallets and terminal.inventories==planned.inventories and terminal.tick==planned.tick,"撤军协作不提前扣粮、改资产或推进时钟")
	for proposal in Schedule.proposals(terminal,data):
		check(not (proposal.kind=="siege_close" and proposal.params.siege_id==siege_id),"NPC不得替本人关闭待确认围城结果")
	if not send(k,"siege_close",{"siege_id":siege_id}): finish(); return
	for kind in ["siege_start","siege_plan","siege_strategy","siege_close"]:
		check(k.probe.calls.get(kind+":apply",0)>0,"实际成功提交调用登记apply "+kind)
	check(k.advance_one_tick().ok,"关闭后原返程继续实际半日")
	if legacy!=null: check(compared==4,"独立原旧源码覆盖开展、军议、撤离和关闭四个成功命令")
	finish()

## 只声明本短测实际执行范围；全链占领/超时由相邻测试和旧源码轨迹补证。
func finish() -> void:
	print("REGISTERED_SIEGE_CHECKS=",checks," FAILURES=",failures)
	quit(1 if failures else 0)
