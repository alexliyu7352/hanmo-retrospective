## 同行注册的真实调用、首错门位、禁止借门及候选回滚；旧真实源码轨迹另由party_registration_trace比较。
extends SceneTree
const Kernel=preload("res://core/world_kernel.gd")
const Builtin=preload("res://core/runtime/builtin_modules.gd")
const Module=preload("res://core/domains/travel/party_module.gd")
const Parties=preload("res://core/party_rules.gd")
const F=preload("res://tests/companion_fixture.gd")
var checks: int=0
var failures: int=0

## 包装唯一真实规则并只在测试对象记录调用，故障必须发生在真实候选已变化之后。
class Probe extends RefCounted:
	var calls: Dictionary={}
	var failure: String=""
	var changed: bool=false
	## 固定ABI记录每次生产preview命中；仍委托原Party适配器。
	func preview(s: Dictionary, d: Dictionary, actor: String, kind: String, params: Dictionary) -> Dictionary:
		calls[kind+":preview"]=calls.get(kind+":preview",0)+1
		return Module.preview(s,d,actor,kind,params)
	## 原同行写入后注入硬失败或Schema尾部失败，外层必须丢弃整个候选。
	func apply(s: Dictionary, d: Dictionary, actor: String, kind: String, params: Dictionary, offer: Dictionary) -> Dictionary:
		calls[kind+":apply"]=calls.get(kind+":apply",0)+1
		var before: Dictionary=s.duplicate(true)
		var result: Dictionary=Module.apply(s,d,actor,kind,params,offer)
		changed=s!=before
		if failure=="tail": s.actors[actor].fatigue=-1
		return {"ok":false,"code":"InjectedPartyApply"} if failure=="apply" else result

## 每次通过真实Builtin建立候选注册表；只针对Party两项替换Callable或注入合同错误。
class Observed extends Kernel:
	var probe=Probe.new()
	var mutation: String=""
	## 调用形状与原owner保留；错误在正式new_game的启动合同中被拒绝。
	func _module_descriptors() -> Array:
		var rows: Array=Builtin.descriptors()
		var borrowed: Dictionary={}
		for row in rows:
			if row.id!="travel": continue
			for command in row.commands:
				if command.kind in Parties.KINDS: command.preview=probe.preview; command.apply=probe.apply
			if mutation=="missing": row.commands.pop_back()
			if mutation=="duplicate": row.commands.append(row.commands[0].duplicate(true))
			if mutation=="gate": row.commands[0].gate_id="legacy_kernel"
			if mutation=="mode": row.commands[0].mode="action"
			if mutation=="owner": borrowed=row.commands.pop_back()
			if mutation in ["borrowed_kind","borrowed_owner"]:
				borrowed=row.commands[0].duplicate(true); borrowed.kind="borrowed_party_gate"
				if mutation=="borrowed_kind": row.commands.append(borrowed); borrowed={}
		if not borrowed.is_empty():
			for row in rows:
				if row.id=="tasks": row.commands.append(borrowed)
		return rows

## 延迟至引擎可读取资源后运行。
func _init() -> void:
	call_deferred("run_checks")

## 失败保留标签并影响退出码。
func check(value: bool, label: String) -> bool:
	checks+=1
	if not value: failures+=1; printerr("FAIL: ",label)
	return value

## 测试内容仍由既有同行夹具通过正式Catalog产生。
func fresh():
	var source=F.fresh()
	check(not source.snapshot().is_empty(),"原同行夹具真实加载")
	var k=Observed.new()
	check(k.new_game("scenario.lived_city","user://test-companions-false-role.carter.json").ok,"注册同行真实启动")
	return k

## 缺失、重复、错owner/模式/门位及陌生kind借早门必须关闭启动并保留旧三元组。
func contracts() -> void:
	var kinds: Array=[]
	for entry in Module.descriptor().commands: kinds.append(entry.kind)
	check(kinds==Parties.KINDS and Module.descriptor().actions.is_empty(),"原KINDS逐项即时登记")
	check(Module.preview({}, {}, "missing","unknown",{}).get("code")=="UnsupportedAction" and Module.apply({}, {}, "missing","unknown",{},{}).get("code")=="UnsupportedAction","未知kind在访问世界之前拒绝")
	var k=fresh(); var before: Dictionary=k.snapshot(); var catalog: Dictionary=k.content(); var registry: Dictionary=k._registry
	for mutation in ["missing","duplicate","gate","mode","owner","borrowed_kind","borrowed_owner"]:
		k.mutation=mutation
		var result: Dictionary=k.new_game()
		var expected: String="MissingBuiltinCommand" if mutation=="missing" else ("DuplicateCommand" if mutation=="duplicate" else ("UnknownCommandAction" if mutation=="mode" else "InvalidBuiltinCommand"))
		check(result.get("code")==expected,"真实启动拒绝 "+mutation+" "+str(result))
		check(k.snapshot()==before and k.content()==catalog and k._registry==registry,"失败保留原世界、内容和注册表 "+mutation)

## 成功预览只读；原报价唯一决定写入，重复请求不执行第二次apply。
func successful_paths() -> void:
	var k=fresh(); check(F.send(k,"move_local",{"place_id":"west.tavern"}).ok,"实际进入酒肆")
	var state: Dictionary=k.snapshot(); var actor: String=state.player_actor_id; var data: Dictionary=k.content().data
	var params: Dictionary={"context":F.context(k)}; var offer: Dictionary=Parties.preview(state,data,actor,"join_party",params)
	for _repeat in range(10): check(k.preview("join_party",params).ok and k.snapshot()==state,"注册预览不变异或消费RNG")
	check(k.probe.calls.get("join_party:preview",0)==10,"early preview实际命中注册Callable")
	var old: Dictionary=state.duplicate(true); var candidate: Dictionary=state.duplicate(true)
	var expected: Dictionary=Parties.apply(old,actor,"join_party",offer)
	check(Module.apply(candidate,data,actor,"join_party",{"context":{"provider_actor_id":"wrong"}},offer)==expected and candidate==old,"只使用原私有offer，忽略替换params")
	var command: Dictionary={"kind":"join_party","params":params,"actor_id":actor,"request_seq":state.request_sequences[actor]+1,"expected_revision":state.revision}
	var joined: Dictionary=k.submit(command); var after: Dictionary=k.snapshot()
	check(joined.ok and k.probe.calls.get("join_party:apply",0)==1,"实际注册apply加入同行")
	check(k.submit(command)==joined and k.snapshot()==after and k.probe.calls.get("join_party:apply",0)==1,"幂等重试不再次操作同行")
	check(F.send(k,"dismiss_companion",{"actor_id":"actor.carter"}).ok and k.probe.calls.get("dismiss_companion:apply",0)==1,"解除同行同样实际进入注册apply")
	check(k.snapshot().wallets==state.wallets and k.snapshot().inventories==state.inventories,"即时邀请解除不串钱包与私人物品")

## 唯有正确owner、固定kind和专用gate可走前置门，测试篡改不成为生产可接受合同。
func early_gate() -> void:
	var k=fresh(); var state: Dictionary=k.snapshot(); var actor: String=state.player_actor_id
	state.actors[actor].available=false
	check(k._preview(state,"join_party",{},actor).get("code")=="ActorBusy","原领域ActorBusy先于中央ActorUnavailable")
	var hits: int=k.probe.calls.get("join_party:preview",0)
	k._registry.commands.join_party.owner="tasks"
	check(k._preview(state,"join_party",{},actor).get("code")=="ActorUnavailable" and k.probe.calls.get("join_party:preview",0)==hits,"错owner不得借前置门")
	var stolen: Dictionary=k._registry.commands.dismiss_companion.duplicate(true); stolen.kind="borrowed_party_gate"
	k._registry.commands.borrowed_party_gate=stolen
	check(k._preview(state,"borrowed_party_gate",{},actor).get("code")=="ActorUnavailable","同owner陌生kind也不得借前置门")

## 原规则先真正修改同行再失败，确保submit丢弃位置、ID、回执与其他候选变动。
func rollback() -> void:
	for failure in ["apply","tail"]:
		var k=fresh(); check(F.send(k,"move_local",{"place_id":"west.tavern"}).ok,"回滚夹具到场")
		var before: Dictionary=k.snapshot(); k.probe.failure=failure
		var result: Dictionary=F.send(k,"join_party",{"context":F.context(k)})
		check(not result.ok and k.probe.changed and k.probe.calls.get("join_party:apply",0)==1,"真实apply后故障触发 "+failure+" "+str(result))
		check(k.snapshot()==before,"候选完整回滚 "+failure)

## 范围仅为本片注册与相邻行为，不把本地通过写成完整A1.7完成。
func run_checks() -> void:
	contracts(); successful_paths(); early_gate(); rollback()
	print("REGISTERED_PARTY_CHECKS=",checks," FAILURES=",failures)
	quit(1 if failures else 0)
