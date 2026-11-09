## 个人战真注册的旧源码全态对照与故障门；仅覆盖注册迁移风险，不复制战斗规则或完整发行门。
extends SceneTree
const Commands=preload("res://core/domains/personal/personal_commands.gd")
const Personal=preload("res://core/personal_combat.gd")
const Builtin=preload("res://core/runtime/builtin_modules.gd")
const Registry=preload("res://core/runtime/module_registry.gd")
const F=preload("res://tests/companion_fixture.gd")
const Sessions=preload("res://core/combat_session.gd")
const Encounters=preload("res://core/encounter_rules.gd")
const Round=preload("res://core/personal_round.gd")
const Codec=preload("res://core/save_codec.gd")
var checks: int=0
var failures: int=0
var comparisons: int=0
var covered: Dictionary={}
var legacy_path: String=""
var legacy=null

## 只包装正式Callable并在其真实写候选之后注入拒绝，不以空成功代替业务执行。
class Probe extends RefCounted:
	var calls: Dictionary={}
	var failure: String=""
	## 记录真实回调次数，供只读预览和请求去重断言使用。
	func hit(kind: String, stage: String) -> void:
		var key: String=kind+":"+stage; calls[key]=calls.get(key,0)+1
	## 所有授权、能力与计划错误仍来自唯一原规则。
	func preview(state: Dictionary,data: Dictionary,actor: String,kind: String,params: Dictionary) -> Dictionary:
		hit(kind,"preview"); return Commands.preview(state,data,actor,kind,params)
	## 真正建战、解算轮次或关闭结果之后，分别模拟领域失败和Schema尾部拒绝。
	func apply(state: Dictionary,data: Dictionary,actor: String,kind: String,params: Dictionary,offer: Dictionary) -> Dictionary:
		hit(kind,"apply")
		var result: Dictionary=Commands.apply(state,data,actor,kind,params,offer)
		if result.ok and failure=="apply": return {"ok":false,"code":"InjectedPersonalApply"}
		if result.ok and failure=="tail": state.wallets[state.actors[actor].wallet_id].balance=-1
		return result

## 测试Kernel只替换真注册回调；仍使用生产事务、公共守卫、世界时钟和Codec。
class TestKernel extends "res://core/world_kernel.gd":
	var probe=Probe.new()
	var mutation: String=""
	var selected: String=""
	var advance_failure: String=""
	var holds: Array=[]
	## 启动变异不得破坏已有世界；错误owner和偷用gate均经正式完整合同拒绝。
	func _module_descriptors() -> Array:
		var rows: Array=super._module_descriptors(); var moved: Dictionary={}
		for descriptor in rows:
			if descriptor.id!="personal": continue
			for command in descriptor.commands.duplicate():
				command.preview=probe.preview; command.apply=probe.apply
				if command.kind!=selected: continue
				match mutation:
					"missing": descriptor.commands.erase(command)
					"duplicate": descriptor.commands.append(command.duplicate(true))
					"gate": command.gate_id="legacy_kernel"
					"mode": command.mode="action"
					"owner": moved=command; descriptor.commands.erase(command)
					"missing_apply": command.apply=null
					"new_kind":
						var extra: Dictionary=command.duplicate(true); extra.kind="invented_personal"; descriptor.commands.append(extra)
		if not moved.is_empty():
			for descriptor in rows:
				if descriptor.id=="tasks": descriptor.commands.append(moved)
		if mutation=="borrow_gate":
			for descriptor in rows:
				if descriptor.id=="economy": descriptor.commands[0].gate_id=Commands.GATE_ID
		return rows
	## 真半日推进后让来源失效，证明二次核验发生在begin扣费和建战之前。
	func _advance(state: Dictionary, hold: Array=[]) -> Dictionary:
		holds.append(hold.duplicate())
		var result: Dictionary=super._advance(state,hold)
		if not result.ok: return result
		if advance_failure=="advance": return {"ok":false,"code":"InjectedPersonalAdvance"}
		if advance_failure=="repreview": state.roles["role.wei"].enabled=false
		return result

## 旧源由命令行显式提供；缺少来源应失败，不能把新新比较算成迁移证据。
func _init() -> void:
	for arg in OS.get_cmdline_user_args():
		if arg.begins_with("--legacy-kernel="): legacy_path=arg.trim_prefix("--legacy-kernel=")
	call_deferred("run_checks")

## 每个断言都计入真实退出状态，脚本错误另由外部日志检查拒绝。
func check(value: bool, label: String) -> bool:
	checks+=1
	if not value: failures+=1; printerr("FAIL: ",label)
	return value

## 基于正式生活目录开局，四条注册所依赖的领域仍来自正式作者配置。
func fresh():
	var k=TestKernel.new()
	check(k.new_game("scenario.alpha_life","res://data/generated/alpha_life.hmcatalog.json").ok,"正式个人战测试开局")
	return k

## 原KINDS为唯一覆盖期望；每一命令的漏项、模式、owner、gate和重复都必须硬失败。
func contracts() -> void:
	var registry: Dictionary=Registry.build(Builtin.descriptors())
	check(registry.ok and Builtin.command_contract_status(registry).ok,"完整注册合同可构建")
	var actual: Array=[]
	for row in Commands.descriptor().commands: actual.append(row.kind)
	check(actual==Personal.KINDS,"命令集合直接等于原KINDS")
	check(Commands.preview({}, {}, "missing", "unknown", {}).get("code")=="UnsupportedAction" and Commands.apply({}, {}, "missing", "unknown", {}, {}).get("code")=="UnsupportedAction","适配拒绝未知kind且不进入道路战else")
	for kind in Personal.KINDS:
		for row in [["missing","MissingBuiltinCommand"],["duplicate","DuplicateCommand"],["gate","InvalidBuiltinCommand"],["mode","UnknownCommandAction"],["owner","InvalidBuiltinCommand"],["missing_apply","InvalidCommandDescriptor"]]:
			var k=TestKernel.new()
			if not check(k.new_game().ok,"硬失败夹具保留已存在世界"): continue
			var before: Dictionary=k.snapshot(); var data: Dictionary=k.content()
			k.mutation=row[0]; k.selected=kind
			var result: Dictionary=k.new_game()
			check(result.get("code")==row[1] and k.snapshot()==before and k.content()==data,"坏登记原世界不变 "+kind+" "+row[0]+" "+str(result))
	for mutation in ["new_kind","borrow_gate"]:
		var k=TestKernel.new(); k.mutation=mutation; k.selected="duel_start"
		check(k.new_game().get("code")=="InvalidBuiltinCommand","他域或未知kind不可借个人战早门 "+mutation)

## 每个保存节点必须完整Codec往返且restore不重放收费、战果、ID或随机数。
func roundtrip(k,label: String) -> void:
	var state: Dictionary=k.snapshot()
	var encoded: String=Codec.encode(state,1,"registered-personal",k.content())
	var decoded: Dictionary=Codec.decode(encoded,k.content())
	check(not encoded.is_empty() and decoded.ok and decoded.state==state and k.restore(decoded.state).ok and k.snapshot()==state,"完整Codec往返 "+label)

## 共存错误按真正旧Kernel排序；这些临时坏状态只用于只读守卫，不作为可存读世界。
func guard_order(k,kind: String,params: Dictionary) -> void:
	var state: Dictionary=k.snapshot(); var actor: String=state.player_actor_id
	for variant in ["original","dead","unavailable","busy","following","army","siege","encounter","personal_elsewhere","military","military_dead"]:
		var s: Dictionary=state.duplicate(true)
		if variant in ["dead","military_dead"]: s.actors[actor].alive=false
		if variant=="unavailable": s.actors[actor].available=false
		if variant=="busy": s.actors[actor].action_id="guard-only-action"
		if variant=="following": s.actors[actor].location={"kind":"Following","party_id":"guard-only-party"}
		if variant=="army": s.actors[actor].location={"kind":"InArmy","army_id":"guard-only-army"}
		if variant=="siege":
			s.armies["guard-only-army"]={"status":"active","member_actor_ids":[actor]}
			s.sieges["guard-only-siege"]={"phase":"active","armies":{"guard-only-army":{}},"withdrawn_army_ids":[]}
		if variant=="encounter": s.encounters["guard-only-encounter"]={"status":"pending","participants":[actor]}
		if variant=="personal_elsewhere": s.battles={"guard-only-battle":{"kind":"personal","phase":"planning","units":{"actor.chen":{}}}}
		if variant in ["military","military_dead"]:
			s.battles={"guard-only-battle":{"id":"guard-only-battle","kind":"military","phase":"planning","participant_actor_ids":[actor],"army_ids":[]}}
		var before: Dictionary=s.duplicate(true)
		var old: Dictionary=legacy._preview(s,kind,params,actor)
		var result: Dictionary=k._preview(s,kind,params,actor)
		check(result==old and s==before,"真实旧源公共守卫首错 "+kind+" "+variant)
		if variant.begins_with("military"): check(result.get("code")=="BattlePending","军事占用先于个人能力/存活/错误参数 "+kind)
	check(k._preview(state,kind,params,"missing").get("code")=="UnknownActor","UnknownActor仍在最外层 "+kind)
	var capability: Array=k._catalog.data.required_capabilities.duplicate()
	k._catalog.data.required_capabilities.erase("personal_combat")
	check(k._preview(state,kind,params,actor).get("code")=="PersonalCombatUnavailable","个人能力先于其余世界守卫 "+kind)
	var military: Dictionary=state.duplicate(true)
	military.battles={"guard-only-battle":{"id":"guard-only-battle","kind":"military","phase":"planning","participant_actor_ids":[actor],"army_ids":[]}}
	check(k._preview(military,kind,params,actor).get("code")=="BattlePending","军事占用也先于缺个人能力 "+kind)
	k._catalog.data.required_capabilities=capability

## 真正命令前后与独立旧Kernel比较全世界及回执；同请求不能再次进入任何注册回调。
func send_personal(k,kind: String,params: Dictionary) -> bool:
	var before: Dictionary=k.snapshot(); var actor: String=before.player_actor_id
	var command: Dictionary={"kind":kind,"params":params,"actor_id":actor,"request_seq":before.request_sequences[actor]+1,"expected_revision":before.revision}
	if not covered.has(kind):
		guard_order(k,kind,params); roundtrip(k,"before-"+kind)
		for failure in ["apply","tail"]:
			k.probe.failure=failure
			var rejected: Dictionary=k.submit(command)
			check(not rejected.ok and k.snapshot()==before,"真实apply写入后失败整候选回滚 "+kind+" "+failure)
		k.probe.failure=""
		covered[kind]=true
	if not check(legacy.restore(before).ok,"真实旧Kernel恢复同一输入 "+kind): return false
	var quote: Dictionary=k.preview(kind,params)
	check(quote==legacy.preview(kind,params) and k.preview(kind,params)==quote and k.snapshot()==before,"旧新预览全值等价且只读 "+kind)
	var old_result: Dictionary=legacy.submit(command)
	var calls: int=k.probe.calls.get(kind+":apply",0)
	var result: Dictionary=k.submit(command)
	if not check(result.ok,"注册真实提交 "+kind+" "+str(result)): return false
	var after: Dictionary=k.snapshot(); var invocation: Dictionary=k.probe.calls.duplicate(true)
	check(result==old_result and after==legacy.snapshot(),"真实旧源全部state/result/RNG/ID/资产等价 "+kind); comparisons+=1
	check(k.probe.calls.get(kind+":apply",0)==calls+1 and k.probe.calls.get(kind+":preview",0)>0,"真实登记preview/apply均命中 "+kind)
	check(k.submit(command)==result and k.snapshot()==after and k.probe.calls==invocation,"重复请求返回原回执且不重入 "+kind)
	var stale: Dictionary=command.duplicate(true); stale.request_seq+=1
	check(k.submit(stale).get("code")=="StalePreview" and k.snapshot()==after,"旧revision不重放个人战 "+kind)
	roundtrip(k,"after-"+kind)
	return true

## 二次预览失效与半日硬失败均须在真实注册apply之前返回并回滚已推进的整个候选。
func check_repreview(k,params: Dictionary) -> void:
	var before: Dictionary=k.snapshot(); var quote: Dictionary=k.preview("duel_start",params)
	for row in [["advance","InjectedPersonalAdvance"],["repreview","RoleNotAuthorized"]]:
		k.advance_failure=row[0]; var calls: int=k.probe.calls.get("duel_start:apply",0)
		var result: Dictionary=F.send(k,"duel_start",params)
		check(result.get("code")==row[1] and k.snapshot()==before and k.probe.calls.get("duel_start:apply",0)==calls,"原半日/二次预览硬失败发生于真实begin之前 "+str(result))
		check(k.holds.back()==quote.sides.keys(),"半日推进准确hold原报价双方，不丢NPC")
	k.advance_failure=""

## 完整打到结果并由本人关闭；NPC无论计划或结果阶段都不能接管玩家的确认。
func resolve_battle(k,opponent: String) -> void:
	var player: String=k.snapshot().player_actor_id
	var battle: Dictionary=Sessions.for_actor(k.snapshot(),player)
	check(k.preview("battle_close",{"battle_id":battle.id}).get("code")=="NoBattleResult","战术计划阶段不能提前关闭")
	check(k.preview("battle_plan",{"battle_id":battle.id},opponent).get("code")=="NotBattleController","NPC不得提交玩家计划")
	for turn in range(26):
		if battle.phase!="planning": break
		var policy: Dictionary=battle.duplicate(true); policy.controller_id=opponent
		var plan: Dictionary=Round.make_ai(policy,k.content().data)[player]; plan.battle_id=battle.id
		if not send_personal(k,"battle_plan",plan): return
		battle=Sessions.for_actor(k.snapshot(),player)
	if not check(battle.phase=="result" and battle.applied and k.snapshot().facts.has(battle.id),"真实轮次只生成一次永久战果，仍待玩家确认"): return
	var before: Dictionary=k.snapshot()
	check(F.send(k,"battle_close",{"battle_id":battle.id},opponent).get("code")=="NotBattleController" and k.snapshot()==before,"NPC不得替玩家关闭结果页")
	check(k.preview("wait").get("code")=="BattlePending" and k.preview("battle_plan",{"battle_id":battle.id}).get("code")=="BattleAlreadyResolved","待确认阶段仍阻断世界且不可重算轮次")
	if not send_personal(k,"battle_close",{"battle_id":battle.id}): return
	check(k.preview("battle_close",{"battle_id":battle.id}).get("code")=="NoBattleResult","新请求不得再次关闭已确认结果")

## 当面切磋消耗半日与真实费用，真实旧Kernel比较也覆盖多轮回写和关闭。
func duel_flow() -> void:
	var k=fresh()
	if not check(F.send(k,"move_local",{"place_id":"west.training"}).ok,"走到真实武馆"): return
	var params: Dictionary={"context":F.context(k,"role.wei")}
	check_repreview(k,params)
	var before: Dictionary=k.snapshot()
	if not send_personal(k,"duel_start",params): return
	check(k.snapshot().tick==before.tick+1 and k.snapshot().wallets["wallet.player"].balance==before.wallets["wallet.player"].balance-12 and k.snapshot().wallets["wallet.wei"].balance==before.wallets["wallet.wei"].balance+12,"原报价实际向授艺人付费一次并仅推进一个半日")
	resolve_battle(k,"actor.wei")

## 护卫道路样本通过正式接约、同行、旅行进入，保留原travel行动与受保护者约束。
func encounter_flow() -> void:
	var k=fresh()
	check(F.send(k,"move_local",{"place_id":"west.tavern"}).ok,"到真实委托人所在地")
	var params: Dictionary={"offer_id":"offer.escort","context":F.context(k,"role.sender")}
	check(F.send(k,"discover_offer",params).ok and F.send(k,"accept_offer",params).ok and k.advance_one_tick().ok,"正式接护卫委托并完成确认")
	check(F.send(k,"join_party",{"context":F.context(k,"role.carter")}).ok,"保护对象成为真实同行人")
	check(F.send(k,"move_local",{"place_id":"west.street"}).ok and F.send(k,"travel",{"destination":"village.east"}).ok,"沿真实道路出发")
	var meeting: Dictionary={}
	for step in range(12):
		var state: Dictionary=k.snapshot(); meeting=Encounters.pending_for(state,state.player_actor_id)
		if not meeting.is_empty():
			if k.preview("encounter_fight",{"encounter_id":meeting.id}).ok: break
			F.send(k,"encounter_leave",{"encounter_id":meeting.id}); meeting={}
		if not check(k.advance_one_tick().ok,"推进真实道路到接触点"): return
	if not check(not meeting.is_empty(),"实际发现持械旅人接触"): return
	var before: Dictionary=k.snapshot(); var advances: int=k.holds.size()
	if not send_personal(k,"encounter_fight",{"encounter_id":meeting.id}): return
	check(k.snapshot().tick==before.tick and k.holds.size()==advances and k.snapshot().actions==before.actions,"道路入战不重复推进半日或改变被挂起原行程")
	var battle: Dictionary=Sessions.for_actor(k.snapshot(),before.player_actor_id)
	check(battle.objective_id=="objective.escort" and battle.units["actor.carter"].protected,"原委托保护对象真实进战")
	resolve_battle(k,"actor.bandit")
	check(k.snapshot().encounters[meeting.id].status=="resolved" and k.snapshot().tasks.values()[0].status=="active","战果仅恢复道路接触且护卫仍须真实送达")

## 只运行声明的注册风险门；旧源码缺失即终止，避免扩大有限证据。
func run_checks() -> void:
	contracts()
	if not check(not legacy_path.is_empty(),"必须显式提供真实旧Kernel源码"): finish(); return
	var script=load(legacy_path)
	if not check(script!=null,"真实旧Kernel源码可加载"): finish(); return
	legacy=script.new()
	if not check(legacy.new_game("scenario.alpha_life","res://data/generated/alpha_life.hmcatalog.json").ok,"独立旧Kernel正式开局"): finish(); return
	duel_flow(); encounter_flow()
	for kind in Personal.KINDS: check(covered.has(kind),"真实流程覆盖全部原KINDS "+kind)
	check(comparisons>=8,"旧源对照覆盖双战源、轮次与结果确认")
	finish()

## 汇总真实断言和完整旧源比较次数，不声称运行全量或图形验收。
func finish() -> void:
	print("REGISTERED_PERSONAL_LEGACY_COMPARISONS=",comparisons)
	print("REGISTERED_PERSONAL_CHECKS=",checks," FAILURES=",failures)
	quit(1 if failures else 0)
