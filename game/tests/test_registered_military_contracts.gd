## 军事人产合同与生产路由一致性；旧Kernel独立等价证据已冻结，本文件不替代整合前后轨迹比较。
extends SceneTree
const Kernel=preload("res://core/world_kernel.gd")
const Builtin=preload("res://core/runtime/builtin_modules.gd")
const Registry=preload("res://core/runtime/module_registry.gd")
const Runtime=preload("res://core/runtime/action_runtime.gd")
const Module=preload("res://core/domains/military/military_module.gd")
const Action=preload("res://core/domains/military/military_action.gd")
const Campaign=preload("res://core/campaign_rules.gd")
const MilitaryCombat=preload("res://core/military_combat.gd")
const SiegeWorld=preload("res://core/siege_world.gd")
const Assets=preload("res://core/military_assets.gd")
const Schema=preload("res://core/state_schema.gd")
const Codec=preload("res://core/save_codec.gd")
const Interaction=preload("res://core/interaction_rules.gd")
var path: String=""
var checks: int=0
var failures: int=0

## 外部目录必须由正式准备器创建；缺参数明确失败，不静默改用别的玩法夹具。
func _init() -> void:
	for arg in OS.get_cmdline_user_args():
		if arg.begins_with("--military-content="): path=arg.trim_prefix("--military-content=")
	call_deferred("run_checks")

## 记录每个真实断言并让任何失败产生非零进程退出。
func check(value: bool, label: String) -> bool:
	checks+=1
	if not value: failures+=1; printerr("FAIL: ",label)
	return value

## 只替换唯一military拥有权条目的代码绑定；旧validator与真实action不可重复注册。
func descriptions() -> Array:
	var result: Array=Builtin.descriptors()
	var fragment: Dictionary=Module.descriptor()
	for descriptor in result:
		if descriptor.id==fragment.id:
			descriptor.commands=fragment.commands; descriptor.actions=fragment.actions
			descriptor.legacy_action_validators=[]
	return result

## 从运行描述提取精确kind集合；期望值始终来自原领域公开集合。
func kinds(entries: Array) -> Array:
	var result: Array=[]
	for entry in entries: result.append(entry.kind)
	result.sort()
	return result

## 覆盖真实命令集合、唯一owner、规范拒绝回调以及缺失和重复生命周期的启动拒绝。
func test_coverage() -> Dictionary:
	var fragment: Dictionary=Module.descriptor()
	var expected: Array=Assets.KINDS+Campaign.KINDS+MilitaryCombat.KINDS+SiegeWorld.KINDS; expected.sort()
	var actions: Array=Assets.ACTION_KINDS.duplicate(); actions.sort()
	check(kinds(fragment.commands)==expected and kinds(fragment.actions)==actions,"描述由真实KINDS和ACTION_KINDS发现")
	check(not Registry.build([fragment]).ok,"片段不能冒充完整发行元数据")
	var registry: Dictionary=Registry.build(descriptions())
	if not check(registry.ok,"军事片段合入真实拥有权合同"): return registry
	for kind in expected:
		var command: Dictionary=registry.commands[kind]
		check(command.owner=="military" and command.mode==("action" if kind in actions else "instant"),"唯一拥有权与模式 "+kind)
	for kind in actions:
		var lifecycle: Dictionary=registry.actions[kind]
		check(lifecycle.validate==Assets.validate_action,"原validator唯一权威 "+kind)
		check(lifecycle.cancel==Registry.refuse_cancel and lifecycle.resume==Registry.refuse_resume,"规范不支持中断 "+kind)
		check(lifecycle.cancel.call({}, {}, {}, "user").get("code")=="ActionNotInterruptible" and lifecycle.resume.call({}, {}, {}, {}).get("code")=="ActionNotResumable","真实四参拒绝ABI "+kind)
	var duplicate: Array=descriptions()
	for descriptor in duplicate:
		if descriptor.id=="military": descriptor.commands.append(descriptor.commands[0].duplicate(true))
	check(Registry.build(duplicate).get("code")=="DuplicateCommand","重复真实军事kind启动拒绝")
	var missing: Array=descriptions()
	for descriptor in missing:
		if descriptor.id=="military": descriptor.actions.pop_back()
	check(Registry.build(missing).get("code")=="UnknownCommandAction","遗漏真实行动生命周期启动拒绝")
	return registry

## 准备状态只通过正式Kernel命令获得；不注入军团、钱包余额或任职事实。
func send(k, kind: String, params: Dictionary={}) -> bool:
	var state: Dictionary=k.snapshot(); var actor: String=state.player_actor_id
	var result: Dictionary=k.submit({"kind":kind,"params":params,"actor_id":actor,"request_seq":state.request_sequences[actor]+1,"expected_revision":state.revision})
	return check(result.ok,kind+" "+str(result))

## 上下文来自当前真实岗位，不把过期人物或权限写死到适配器。
func context(k) -> Dictionary:
	return Interaction.for_role(k.snapshot(),k.content().data,"role.military.a")

## 比较完整候选、开工结果、原validator和真正Codec；finish只核对资产域边界，不代替世界协调。
func action_parity(k, registry: Dictionary, kind: String, params: Dictionary) -> bool:
	var state: Dictionary=k.snapshot(); var data: Dictionary=k.content().data; var actor: String=state.player_actor_id
	params=params.duplicate(true); params.adapter_probe={"nested":["untouched"]}
	var original: Array=[state.duplicate(true),data.duplicate(true),params.duplicate(true)]
	var offer: Dictionary=k._preview(state,kind,params,actor)
	if not check(offer.ok,"真实私有报价 "+kind+" "+str(offer)): return false
	for _repeat in range(20): check(Module.preview(state,data,actor,kind,params)==offer,"只读报价重复等价 "+kind)
	var frozen: Dictionary=offer.duplicate(true)
	var prepared: Dictionary=Action.prepare(state,data,actor,kind,params,offer)
	check(prepared.ok and prepared.participants==offer.participants and prepared.duration==offer.duration,"prepare参与者和时间等价 "+kind)
	prepared.params.adapter_probe.nested.append("test-only"); prepared.participants.append("test-only")
	check(state==original[0] and data==original[1] and params==original[2] and offer==frozen,"prepare深拷贝且不占用或消费RNG "+kind)
	var old: Dictionary=state.duplicate(true); var candidate: Dictionary=state.duplicate(true)
	var legacy: Dictionary=k._apply(old,kind,params,actor,offer)
	var registered: Dictionary=Runtime.begin(candidate,data,actor,kind,params,offer,registry)
	if not check(legacy.ok and legacy==registered and old==candidate,"完整开工候选与真实注册Kernel路由一致 "+kind): return false
	check(Schema.validate(candidate,k.content(),registry).ok,"注册validator完整世界有效 "+kind)
	var decoded: Dictionary=Codec.decode(Codec.encode(candidate,0,"registered-military-contract",k.content()),k.content())
	check(decoded.ok and decoded.state==candidate,"真实Codec保存待办军事行动 "+kind)
	var id: String=registered.details.action_id
	var before_validation: Dictionary=candidate.duplicate(true)
	check(registry.actions[kind].validate.call(candidate,data,candidate.actions[id])==Assets.validate_action(candidate,data,candidate.actions[id]) and candidate==before_validation,"原validator只读且结果不变 "+kind)
	var raw_old: Dictionary=old.duplicate(true); var raw_new: Dictionary=candidate.duplicate(true)
	raw_old.actions[id].remaining_ticks-=1
	var old_outcome: Dictionary=Assets.finish(raw_old,data,raw_old.actions[id])
	var stepped: Dictionary=Runtime.step(raw_new,data,raw_new.actions[id],registry.actions,{"finish_tick":state.tick+1,"hold":[actor],"contacted":{actor:true}})
	check(stepped.ok and stepped.completed and not stepped.suspended and stepped.outcome==old_outcome and raw_old==raw_new,"资产finish与旧实现完整等价，hold不改变手续 "+kind)
	check(raw_new.tick==state.tick and raw_new.actions.has(id),"finish不提前改时钟或释放世界占用 "+kind)
	var interrupted_old: Dictionary=old.duplicate(true); var interrupted_new: Dictionary=candidate.duplicate(true)
	interrupted_old.actors[actor].available=false; interrupted_new.actors[actor].available=false
	interrupted_old.actions[id].remaining_ticks-=1
	var interrupted: Dictionary=Assets.finish(interrupted_old,data,interrupted_old.actions[id])
	var interrupted_step: Dictionary=Runtime.step(interrupted_new,data,interrupted_new.actions[id],registry.actions,{"finish_tick":state.tick+1})
	check(interrupted.ok and not interrupted.success and interrupted_step.ok and interrupted_step.outcome==interrupted and interrupted_old==interrupted_new,"真实业务中断保留失败回执和资源释放 "+kind)
	var invalid: Dictionary=candidate.duplicate(true); invalid.actions[id].params.erase("terms")
	var bad_original: Dictionary=invalid.duplicate(true)
	check(registry.actions[kind].validate.call(invalid,data,invalid.actions[id])==Assets.validate_action(invalid,data,invalid.actions[id]) and invalid==bad_original,"坏载荷仍走唯一原validator且不变异 "+kind)
	if kind=="army_return" and old_outcome.get("success",false):
		check(raw_new.campaigns[old_outcome.details.order_id].status==state.campaigns[old_outcome.details.order_id].status,"归营原结果保留，军令确认仍留新时钟阶段")
	var stale: Dictionary=offer.duplicate(true); stale.duration+=1
	var old_bad: Dictionary=state.duplicate(true); var new_bad: Dictionary=state.duplicate(true)
	var rejected: Dictionary=k._apply(old_bad,kind,params,actor,stale)
	var rejected_new: Dictionary=Runtime.begin(new_bad,data,actor,kind,params,stale,registry)
	check(not rejected.ok and rejected==rejected_new and old_bad==new_bad,"真实开工拒绝后的候选等价 "+kind)
	check(k.snapshot()==state,"直接合同测试不提交真实世界 "+kind)
	return true

## 即时命令继续使用领域重新核验的原报价，不接受调用参数替换已经批准的合同。
func instant_parity(k, registry: Dictionary, kind: String, params: Dictionary) -> bool:
	var state: Dictionary=k.snapshot(); var data: Dictionary=k.content().data; var actor: String=state.player_actor_id
	var offer: Dictionary=k._preview(state,kind,params,actor)
	if not check(offer.ok,"即时真实报价 "+kind+" "+str(offer)): return false
	var old: Dictionary=state.duplicate(true); var candidate: Dictionary=state.duplicate(true)
	var legacy: Dictionary=Assets.apply(old,data,actor,kind,offer)
	var applied: Dictionary=registry.commands[kind].apply.call(candidate,data,actor,kind,{"ignored":"not quoted"},offer)
	check(legacy.ok and applied==legacy and candidate==old,"即时完整候选保留真实资产规则 "+kind)
	return true

## 四个耗时资产动作串接真实准备流程；战略和世界phase仍使用当前已接受生产路径。
func run_checks() -> void:
	var registry: Dictionary=test_coverage()
	if not registry.ok: finish(); return
	var k=Kernel.new()
	if not check(not path.is_empty() and k.new_game("scenario.alpha_life",path).ok,"第二军事内容正式开局"): finish(); return
	var params: Dictionary={"office_id":"office.military.a","context":context(k)}
	if not action_parity(k,registry,"office_accept",params) or not send(k,"office_accept",params) or not check(k.advance_one_tick().ok,"原生产受任完成"): finish(); return
	if not instant_parity(k,registry,"office_resign",{"office_id":"office.military.a","context":context(k)}): finish(); return
	if not send(k,"campaign_accept",{"template_id":"order.military.a","context":context(k)}): finish(); return
	var state: Dictionary=k.snapshot(); var order: String=state.campaigns.keys()[0]
	if not send(k,"move_local",{"place_id":"west.estate"}): finish(); return
	if not instant_parity(k,registry,"army_pledge",{"estate_id":"estate.riverside","order_id":order,"actor_ids":["actor.estate_guard"]}): finish(); return
	if not send(k,"move_local",{"place_id":"west.street"}): finish(); return
	params={"order_id":order,"unit_ids":["unit.military.a"],"food_quantity":80,"money":50}
	if not action_parity(k,registry,"army_form",params) or not send(k,"army_form",params) or not check(k.advance_one_tick().ok,"原生产编组完成"): finish(); return
	state=k.snapshot(); var army: String=state.campaigns[order].army_id
	params={"army_id":army,"food_quantity":20,"money":10}
	if not action_parity(k,registry,"army_resupply",params) or not send(k,"army_resupply",params) or not check(k.advance_one_tick().ok,"原生产补给完成"): finish(); return
	if not action_parity(k,registry,"army_return",{"army_id":army}): finish(); return
	check(Module.preview(k.snapshot(),k.content().data,state.player_actor_id,"unknown_military",{}).get("code")=="UnsupportedAction","未知kind不通过兼容兜底")
	var no_capability: Dictionary=k.content().data; no_capability.required_capabilities.erase("military")
	check(Module.preview(k.snapshot(),no_capability,state.player_actor_id,"office_accept",{}).get("code")=="MilitaryUnavailable","军事能力守卫先于领域参数错误")
	finish()

## 此结果只代表合同和当前路由一致性；全状态旧新比较与生产失败矩阵由集成测试另行证明。
func finish() -> void:
	print("REGISTERED_MILITARY_CONTRACTS_CHECKS=",checks," FAILURES=",failures)
	quit(1 if failures else 0)
