## 会战注册命令复用真实野战与节点围城链；只新增路由、候选回滚、原源码对照和入口守卫断言。
extends "res://tests/test_siege_world.gd"
const Commands=preload("res://core/domains/military/military_combat_commands.gd")
const MilitaryCombat=preload("res://core/military_combat.gd")
const Builtin=preload("res://core/runtime/builtin_modules.gd")
const Registry=preload("res://core/runtime/module_registry.gd")
const Coordination=preload("res://core/domains/military/military_coordination.gd")
var legacy_path: String=""
var legacy=null
var covered: Dictionary={}
var checked_refresh_failure: bool=false
var compared: int=0

## 测试探针只包装真实规则；故障发生在真正写计划或关闭之后，不以空成功替代apply。
class Probe extends RefCounted:
	var calls: Dictionary={}
	var failure: String=""
	## 记录当前命令真实执行次数，重复请求不得再次进入领域。
	func hit(kind: String, stage: String) -> void:
		var key: String=kind+":"+stage; calls[key]=calls.get(key,0)+1
	## 预览只委托唯一会战规则，记录早门确实由注册Callable执行。
	func preview(state: Dictionary,data: Dictionary,actor: String,kind: String,params: Dictionary) -> Dictionary:
		hit(kind,"preview"); return Commands.preview(state,data,actor,kind,params)
	## 真实apply后可拒绝、破坏尾部或围城载荷；外层须丢弃该次完整候选。
	func apply(state: Dictionary,data: Dictionary,actor: String,kind: String,params: Dictionary,offer: Dictionary) -> Dictionary:
		hit(kind,"apply")
		var result: Dictionary=Commands.apply(state,data,actor,kind,params,offer)
		if result.ok and failure=="apply": return {"ok":false,"code":"InjectedCombatApply"}
		if result.ok and failure=="tail": state.wallets[state.actors[actor].wallet_id].balance=-1
		if result.ok and failure=="refresh":
			for siege in state.sieges.values():
				if siege.phase=="active": siege.started_tick=-1
		return result

## 仅替换本任务真实kind；其他军事adapter和公共阶段保留生产绑定。
class TestKernel extends "res://core/world_kernel.gd":
	var probe=Probe.new()
	var mutation: String=""
	var selected: String=""
	## 正式new_game必须独立拒绝缺项、错误owner/mode/gate与重复登记。
	func _module_descriptors() -> Array:
		var rows: Array=super._module_descriptors(); var moved: Dictionary={}
		for descriptor in rows:
			if descriptor.id!="military": continue
			for command in descriptor.commands.duplicate():
				if not command.kind in MilitaryCombat.KINDS: continue
				command.preview=probe.preview; command.apply=probe.apply
				if command.kind!=selected: continue
				match mutation:
					"missing": descriptor.commands.erase(command)
					"duplicate": descriptor.commands.append(command.duplicate(true))
					"gate": command.gate_id="legacy_kernel"
					"mode": command.mode="action"
					"owner": moved=command; descriptor.commands.erase(command)
		if not moved.is_empty():
			for descriptor in rows:
				if descriptor.id=="tasks": descriptor.commands.append(moved)
		return rows

## 原Kernel源码可由显式参数加载；文件内容和SHA由本次验证manifest独立记录。
func _init() -> void:
	for arg in OS.get_cmdline_user_args():
		if arg.begins_with("--legacy-kernel="): legacy_path=arg.trim_prefix("--legacy-kernel=")
	call_deferred("run_checks")

## 每个真实来源kind均须即时注册，不允许删掉kind后悄悄走旧分支。
func coverage() -> void:
	var expected: Array=MilitaryCombat.KINDS.duplicate(); expected.sort()
	var actual: Array=[]
	for command in Commands.descriptor().commands: actual.append(command.kind)
	actual.sort(); check(actual==expected and Commands.command_kinds()==MilitaryCombat.KINDS,"会战描述由原真实KINDS发现")
	check(Commands.gate_id("invented").is_empty(),"未知kind不给会战早门")
	var registry: Dictionary=Registry.build(Builtin.descriptors())
	if not check(registry.ok,"真实完整描述可构建"): return
	for kind in expected:
		check(registry.commands.has(kind) and registry.commands[kind].owner=="military" and registry.commands[kind].mode=="instant" and registry.commands[kind].gate_id=="military_combat_early","真实会战命令注册 "+kind)
		for row in [["missing","MissingBuiltinCommand"],["duplicate","DuplicateCommand"],["gate","InvalidBuiltinCommand"],["mode","UnknownCommandAction"],["owner","InvalidBuiltinCommand"]]:
			var k=TestKernel.new()
			if not check(k.new_game().ok,"启动拒绝夹具先有原世界"): continue
			var before: Dictionary=k.snapshot(); var data: Dictionary=k.content(); k.mutation=row[0]; k.selected=kind
			var result: Dictionary=k.new_game()
			check(result.get("code")==row[1] and k.snapshot()==before and k.content()==data,"错误会战登记保留旧世界 "+kind+" "+row[0]+" "+str(result))

## 使用最新正式请求序号；失败不占序号，成功重复返回原回执。
func command(k,kind: String,params: Dictionary) -> Dictionary:
	var state: Dictionary=k.snapshot(); var actor: String=state.player_actor_id
	return {"kind":kind,"params":params,"actor_id":actor,"request_seq":state.request_sequences[actor]+1,"expected_revision":state.revision}

## 会战早预览必须先于可用性、忙碌、随军和BattlePending，未知人物仍由最外层先拒绝。
func guard_order(k,kind: String,params: Dictionary) -> void:
	var state: Dictionary=k.snapshot(); var data: Dictionary=k.content().data; var actor: String=state.player_actor_id
	for variant in ["original","unavailable","busy","following","in_army"]:
		var candidate: Dictionary=state.duplicate(true)
		if variant=="unavailable": candidate.actors[actor].alive=false; candidate.actors[actor].available=false
		if variant=="busy": candidate.actors[actor].action_id="not-a-real-action"
		if variant=="following": candidate.actors[actor].location={"kind":"Following","leader_actor_id":"actor.chen"}
		if variant=="in_army": candidate.actors[actor].location={"kind":"InArmy","army_id":"unrelated"}
		var before: Dictionary=candidate.duplicate(true)
		var expected: Dictionary=MilitaryCombat.preview(candidate,data,actor,kind,params)
		check(k._preview(candidate,kind,params,actor)==expected and candidate==before,"原会战首错及只读入口 "+kind+" "+variant)
	check(k._preview(state,kind,params,"missing").get("code")=="UnknownActor","UnknownActor仍先于会战注册早门")
	check(k.preview(kind,{"battle_id":"missing"}).get("code")=="UnknownMilitaryBattle","未知会战不被BattlePending遮盖")
	check(k.preview(kind,params,"actor.qingchuan.bowman").get("code")=="MilitaryCommanderUnauthorized","NPC不能代玩家提交或关闭结果页")

## 已计划、战后结果和关闭态均经真正Codec往返；不重演伤亡、消费或RNG。
func save_roundtrip(k,label: String) -> void:
	var before: Dictionary=k.snapshot()
	var decoded: Dictionary=Codec.decode(Codec.encode(before,0,"registered-combat",k.content()),k.content())
	check(decoded.ok and k.restore(decoded.state).ok and k.snapshot()==before,"真实会战状态Codec往返 "+label)

## 直接旧领域重新核验params而非信任offer；原apply拒绝时双方候选同样不变。
func offer_revalidation(k,kind: String,params: Dictionary) -> void:
	var state: Dictionary=k.snapshot(); var data: Dictionary=k.content().data; var actor: String=state.player_actor_id
	var offer: Dictionary=Commands.preview(state,data,actor,kind,params)
	var old: Dictionary=state.duplicate(true); var candidate: Dictionary=state.duplicate(true)
	var bad: Dictionary={"battle_id":"missing"}
	check(Commands.apply(candidate,data,actor,kind,bad,offer)==MilitaryCombat.apply(old,data,actor,kind,bad) and candidate==old and candidate==state,"apply按当前原参数重新核验 "+kind)

## 只对真实会战命令插入失败和对照；其余完整围城流程直接复用原测试操作者。
func send(k,kind: String,params: Dictionary={}) -> bool:
	if not kind in MilitaryCombat.KINDS: return super.send(k,kind,params)
	return send_combat(k,kind,params)

## 围城双分支仍保留完整状态比较；新分支的会战命令额外证明注册路由和原子性。
func send_same(k,uninterrupted,kind: String,params: Dictionary) -> bool:
	if not kind in MilitaryCombat.KINDS: return super.send_same(k,uninterrupted,kind,params)
	var before: Dictionary=k.snapshot()
	if not send_combat(k,kind,params): return false
	var result: Dictionary=F.send(uninterrupted,kind,params)
	return check(result.ok and k.snapshot()==uninterrupted.snapshot() and k.snapshot().tick==before.tick,"围城会战两分支完整一致 "+kind)

## 从正式战场状态执行旧源同一请求；失败注入、重复请求均不得污染真实世界。
func send_combat(k,kind: String,params: Dictionary) -> bool:
	var request: Dictionary=command(k,kind,params); var before: Dictionary=k.snapshot()
	if not covered.has(kind):
		guard_order(k,kind,params); offer_revalidation(k,kind,params); save_roundtrip(k,"before-"+kind)
		for failure in ["apply","tail"]:
			k.probe.failure=failure
			var rejected: Dictionary=k.submit(request)
			check(not rejected.ok and k.snapshot()==before,"真实apply之后失败整候选回滚 "+kind+" "+failure)
		k.probe.failure=""
		covered[kind]=true
	if kind=="military_close" and not checked_refresh_failure:
		for siege in before.sieges.values():
			if siege.phase!="active": continue
			var candidate: Dictionary=before.duplicate(true); var data: Dictionary=k.content().data
			var outcome: Dictionary=Commands.apply(candidate,data,before.player_actor_id,kind,params,{})
			candidate.sieges[siege.id].started_tick=-1
			var direct: Dictionary=candidate.duplicate(true)
			var expected_failure: Dictionary=SiegeWorld.refresh_plans(direct,data)
			var coordinated: Dictionary=Coordination.after_command(candidate,data,kind,outcome,Callable())
			check(not expected_failure.ok and coordinated==expected_failure and candidate==direct,"共同协调直接返回原围城刷新失败，不依赖尾部Schema拒绝")
			k.probe.failure="refresh"
			var rejected: Dictionary=k.submit(request)
			check(rejected==expected_failure and k.snapshot()==before,"关闭后围城刷新失败丢弃关闭与全部候选 "+str(rejected))
			k.probe.failure=""; checked_refresh_failure=true; break
	var old_result: Dictionary={}
	if legacy!=null:
		if not check(legacy.restore(before).ok,"旧源码恢复同一合法命令前状态"): return false
		old_result=legacy.submit(request)
	var calls_before: int=k.probe.calls.get(kind+":apply",0)
	var result: Dictionary=k.submit(request)
	if not check(result.ok,"真实注册会战提交 "+kind+" "+str(result)): return false
	check(k.probe.calls.get(kind+":apply",0)==calls_before+1 and k.probe.calls.get(kind+":preview",0)>0,"真实注册preview/apply命中 "+kind)
	var after: Dictionary=k.snapshot(); var calls: Dictionary=k.probe.calls.duplicate(true)
	check(k.submit(request)==result and k.snapshot()==after and k.probe.calls==calls,"重复会战请求原回执且不重入领域 "+kind)
	check(after.tick==before.tick and after.rng==before.rng and after.wallets==before.wallets and after.inventories==before.inventories,"计划或关闭不推进时钟、不重复军产消费或RNG "+kind)
	if legacy!=null:
		check(old_result==result and legacy.snapshot()==after,"原始旧Kernel命令回执及完整持久状态等价 "+kind); compared+=1
	if kind=="military_plan": check(k.preview(kind,params).get("code")=="MilitaryPlanAlreadySubmitted","新请求不能覆盖已冻结计划")
	else: check(k.preview(kind,params).get("code")=="MilitaryResultNotSettled","新请求不能重复关闭结果")
	save_roundtrip(k,"after-"+kind)
	return true

## 独立登记与完整实际野战/围城复用；旧源码对照可选，但不存在时明确不计旧源证据。
func run_checks() -> void:
	coverage()
	world=TestKernel.new()
	if not legacy_path.is_empty():
		var script=load(legacy_path)
		if not check(script!=null,"显式旧源码可加载"): complete(); return
		legacy=script.new()
		if not check(legacy.new_game("scenario.alpha_life","res://data/generated/alpha_life.hmcatalog.json").ok,"原始旧Kernel正式开局"): complete(); return
	super.run_checks()
	for kind in MilitaryCombat.KINDS: check(covered.has(kind),"真实世界实际覆盖会战kind "+kind)
	check(checked_refresh_failure,"真实节点会战确实覆盖关闭后的围城协调失败")
	if legacy!=null: check(compared>2,"原始旧源码比较覆盖野战与围城节点多次命令")
	complete()

## 复用父流程时中间finish只返回，由最外层汇总一次全部真实断言。
func finish() -> void:
	pass

## 可机读汇总区分旧源码比较数量，不将未提供的基线算成通过。
func complete() -> void:
	print("REGISTERED_MILITARY_COMBAT_LEGACY_COMPARISONS=",compared)
	print("REGISTERED_MILITARY_COMBAT_CHECKS=",checks," FAILURES=",failures)
	quit(1 if failures else 0)
