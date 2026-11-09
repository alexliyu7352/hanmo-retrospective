## 庄园八命令接线验证：复用正式规则和原Kernel源码，对照真实资产、建设阶段、首错与候选回滚。
extends SceneTree
const Commands=preload("res://core/domains/estates/estate_commands.gd")
const Estate=preload("res://core/estate_rules.gd")
const Builtin=preload("res://core/runtime/builtin_modules.gd")
const Registry=preload("res://core/runtime/module_registry.gd")
const F=preload("res://tests/companion_fixture.gd")
const Codec=preload("res://core/save_codec.gd")
const Inventory=preload("res://core/inventory_rules.gd")
var checks: int=0
var failures: int=0
var compared: int=0
var legacy_path: String=""
var contracts_only: bool=false
var legacy=null

## 测试探针只包真实适配器；注入失败发生在已经实际转钱、移货或写永久事实之后。
class Probe extends RefCounted:
	var calls: Dictionary={}
	var failure: String=""
	## 记录预览和提交入口，重复request不得再次执行。
	func hit(kind: String, stage: String) -> void:
		var key: String=kind+":"+stage; calls[key]=calls.get(key,0)+1
	## 原预览保持只读，测试不生成替代报价。
	func preview(state: Dictionary,data: Dictionary,actor: String,kind: String,params: Dictionary) -> Dictionary:
		hit(kind,"preview"); return Commands.preview(state,data,actor,kind,params)
	## 原瞬时操作后才注入硬拒绝或坏尾部，验证整个候选不会泄露。
	func apply(state: Dictionary,data: Dictionary,actor: String,kind: String,params: Dictionary,offer: Dictionary) -> Dictionary:
		hit(kind,"apply")
		var result: Dictionary=Commands.apply(state,data,actor,kind,params,offer)
		if result.ok and failure=="apply": return {"ok":false,"code":"InjectedEstateApply"}
		if result.ok and failure=="tail": state.wallets[state.actors[actor].wallet_id].balance=-1
		return result

## 仅替换庄园绑定与故障出口，原生产阶段、建设支付及验证规则都不替换。
class TestKernel extends "res://core/world_kernel.gd":
	var probe=Probe.new()
	var mutation: String=""
	var selected: String=""
	## 注册负例在重新开局时检测，不允许缺失登记又悄悄落回旧领域分支。
	func _module_descriptors() -> Array:
		var rows: Array=super._module_descriptors(); var moved: Dictionary={}
		for row in rows:
			if row.id!="estates": continue
			for entry in row.commands.duplicate():
				entry.preview=probe.preview
				if entry.mode=="instant": entry.apply=probe.apply
				if entry.kind!=selected: continue
				match mutation:
					"missing": row.commands.erase(entry)
					"duplicate": row.commands.append(entry.duplicate(true))
					"gate": entry.gate_id="estate_fake_gate"
					"owner": moved=entry; row.commands.erase(entry)
					"mode":
						entry.mode="instant"; entry.apply=probe.apply; entry.erase("compat_action")
					"bridge": entry.erase("compat_action")
					"extra":
						var extra: Dictionary=entry.duplicate(true); extra.kind="estate_invented"; row.commands.append(extra)
		if not moved.is_empty():
			for row in rows:
				if row.id=="tasks": row.commands.append(moved)
		return rows
	## 建设开始仍调用旧Kernel原文；真实支付、材料消费与占用之后故障必须全部回滚。
	func _begin_action(state: Dictionary,actor: String,kind: String,params: Dictionary,offer: Dictionary) -> Dictionary:
		var result: Dictionary=super._begin_action(state,actor,kind,params,offer)
		if kind!="estate_build": return result
		probe.hit(kind,"begin")
		if result.ok and probe.failure=="apply": return {"ok":false,"code":"InjectedEstateBegin"}
		if result.ok and probe.failure=="tail": state.wallets[state.actors[actor].wallet_id].balance=-1
		return result
	## 原世界阶段完成后注入硬失败，生产、设施、时钟、RNG与行动释放不得部分提交。
	func _advance(state: Dictionary,hold: Array=[]) -> Dictionary:
		var result: Dictionary=super._advance(state,hold)
		if result.ok and probe.failure=="advance": return {"ok":false,"code":"InjectedEstateAdvance"}
		return result

## 旧源码是验证输入，未提供时明确不计历史等价证据。
func _init() -> void:
	for arg in OS.get_cmdline_user_args():
		if arg=="--contracts-only": contracts_only=true
		if arg.begins_with("--legacy-kernel="): legacy_path=arg.trim_prefix("--legacy-kernel=")
	call_deferred("run_checks")

## 每条断言具备非零失败退出，不以进程成功代替真实通过。
func check(value: bool,label: String) -> bool:
	checks+=1
	if not value: failures+=1; printerr("FAIL: ",label)
	return value

## 正式Alpha作者包建立真实庄园，并沿正常场所通道抵达。
func fresh():
	var k=TestKernel.new()
	if not check(k.new_game("scenario.alpha_life","res://data/generated/alpha_life.hmcatalog.json").ok,"正式庄园开局"): return k
	check(F.send(k,"move_local",{"place_id":"west.estate"}).ok,"实际移动到庄园")
	return k

## 版本化命令沿同一玩家request序列，不替测试绕过Kernel事务。
func request(k,kind: String,params: Dictionary) -> Dictionary:
	var state: Dictionary=k.snapshot(); var actor: String=state.player_actor_id
	return {"kind":kind,"params":params,"actor_id":actor,"request_seq":state.request_sequences[actor]+1,"expected_revision":state.revision}

## 在无进行中长矩阵情况下执行真正Codec encode/decode/restore，完整比较资金与永久事实。
func roundtrip(k,label: String) -> void:
	var before: Dictionary=k.snapshot()
	var decoded: Dictionary=Codec.decode(Codec.encode(before,0,"registered-estates",k.content()),k.content())
	check(decoded.ok and k.restore(decoded.state).ok and k.snapshot()==before,"Codec完整往返 "+label)

## 直接取原KINDS核对覆盖并逐项破坏描述，开局失败保留原state、catalog和可用注册。
func coverage() -> void:
	var expected: Array=Estate.KINDS.duplicate(); expected.sort()
	var actual: Array=[]
	for entry in Commands.descriptor().commands: actual.append(entry.kind)
	actual.sort(); check(actual==expected and Commands.command_kinds()==Estate.KINDS,"命令覆盖来自原Estate.KINDS而非复制表")
	var registry: Dictionary=Registry.build(Builtin.descriptors())
	if not check(registry.ok,"正式含兼容桥的Registry可构建 "+str(registry.get("code"))): return
	for kind in expected:
		check(registry.commands.has(kind) and registry.commands[kind].owner=="estates" and registry.commands[kind].gate_id=="legacy_kernel","每条庄园命令真实注册 "+kind)
		for row in [["missing","MissingBuiltinCommand"],["duplicate","DuplicateCommand"],["gate","InvalidBuiltinCommand"],["owner","UnknownCompatAction" if kind=="estate_build" else "InvalidBuiltinCommand"]]:
			var k=fresh(); var before: Dictionary=k.snapshot(); var data: Dictionary=k.content()
			k.mutation=row[0]; k.selected=kind
			var result: Dictionary=k.new_game()
			check(result.get("code")==row[1] and k.snapshot()==before and k.content()==data,"坏登记不能换世界 "+kind+" "+row[0]+" "+str(result))
			check(k.preview("estate_fund",{"estate_id":"estate.riverside","quantity":1}).ok,"启动失败保留旧注册可用")
	for row in [["mode","InvalidBuiltinCommand"],["bridge","UnknownCommandAction"],["extra","UnknownCompatAction"]]:
		var k=fresh(); k.selected="estate_build"; k.mutation=row[0]
		check(k.new_game().get("code")==row[1],"建设模式或兼容桥不可伪造 "+row[0])

## 公共守卫与领域首错同原Kernel逐项比较；构造坏候选只用于预览，不作为合法存档。
func guard_order(k) -> void:
	var initial: Dictionary=k.snapshot(); var actor: String=initial.player_actor_id
	for kind in Estate.KINDS:
		for variant in ["domain","dead","busy","following","army","encounter","battle"]:
			var state: Dictionary=initial.duplicate(true)
			match variant:
				"dead": state.actors[actor].alive=false; state.actors[actor].available=false
				"busy": state.actors[actor].action_id="missing"
				"following": state.actors[actor].location={"kind":"Following","leader_actor_id":"actor.estate_steward"}
				"army": state.actors[actor].location={"kind":"InArmy","army_id":"missing"}
				"encounter": state.encounters["probe"]={"status":"pending","participants":[actor]}
				"battle": state.battles["probe"]={"kind":"personal","phase":"active","units":{actor:{}}}
			var before: Dictionary=state.duplicate(true)
			var result: Dictionary=k._preview(state,kind,{},actor)
			check(not result.ok and state==before,"错误预览只读 "+kind+" "+variant)
			if legacy!=null: check(result==legacy._preview(state,kind,{},actor),"旧源码首错保持 "+kind+" "+variant)
	var data: Dictionary=k.content().data; data.required_capabilities.erase("estates")
	check(Commands.preview(initial,data,actor,"estate_fund",{}).get("code")=="EstatesUnavailable","能力缺失先于庄园参数错误")
	check(k._preview(initial,"estate_fund",{},"unknown").get("code")=="UnknownActor","未知人物最先拒绝")
	check(k.preview("estate_invented").get("code")=="UnsupportedAction","未知kind无catch-all成功")

## 一次真实命令验证重复预览、写后回滚、原源码等价、请求重放和公库不串账。
func send_estate(k,kind: String,params: Dictionary) -> Dictionary:
	var before: Dictionary=k.snapshot(); var req: Dictionary=request(k,kind,params)
	var quote: Dictionary=k.preview(kind,params)
	if not check(quote.ok,"真实庄园预览 "+kind+" "+str(quote)): return quote
	for _i in range(5): check(k.preview(kind,params)==quote and k.snapshot()==before,"重复预览不消费RNG或资源 "+kind)
	for failure in ["apply","tail"]:
		k.probe.failure=failure
		var result: Dictionary=k.submit(req)
		var expected_failure: String="InjectedEstateBegin" if kind=="estate_build" else "InjectedEstateApply"
		check(not result.ok and (failure!="apply" or result.get("code")==expected_failure) and k.snapshot()==before,"写入后失败整候选回滚 "+kind+" "+failure)
	k.probe.failure=""
	var old_result: Dictionary={}
	if legacy!=null:
		check(legacy.restore(before).ok,"旧源码接受相同合法前态 "+kind)
		old_result=legacy.submit(req)
	var stage: String="begin" if kind=="estate_build" else "apply"
	var count: int=k.probe.calls.get(kind+":"+stage,0)
	var result: Dictionary=k.submit(req)
	if not check(result.ok,"真实庄园提交 "+kind+" "+str(result)): return result
	check(k.probe.calls.get(kind+":"+stage,0)==count+1,"注册真实入口只执行一次 "+kind)
	var after: Dictionary=k.snapshot(); var calls: Dictionary=k.probe.calls.duplicate(true)
	check(k.submit(req)==result and k.snapshot()==after and k.probe.calls==calls,"同request原回执且不重入 "+kind)
	var stale: Dictionary=req.duplicate(true); stale.request_seq+=1
	check(k.submit(stale).get("code")=="StalePreview" and k.snapshot()==after and k.probe.calls==calls,"旧revision的新request拒绝且不消耗序号 "+kind)
	check(after.tick==before.tick and after.rng==before.rng,"命令本身不偷推进世界 "+kind)
	for id in before.wallets:
		if before.wallets[id].owner.get("kind")=="organization" and (kind!="estate_build" or id!="wallet.shop"): check(after.wallets[id]==before.wallets[id],"不动其他公库 "+kind+" "+id)
	for id in before.inventories:
		if before.inventories[id].owner.get("kind")=="organization": check(after.inventories[id]==before.inventories[id],"不动公共实物 "+kind+" "+id)
	if legacy!=null:
		check(result==old_result and after==legacy.snapshot(),"旧源码回执与全部持久状态相同 "+kind); compared+=1
	roundtrip(k,kind)
	return result

## 以已受支持历史调兵来源构造合法存档，仅冻结来源，不代替muster/return真实操作。
func muster_roundtrip(k) -> void:
	var state: Dictionary=k.snapshot(); state.facts["source.estate.legacy"]={"kind":"estate_muster","estate_id":"estate.riverside"}
	check(k.restore(state).ok,"历史estate_muster来源仍可恢复")
	var params: Dictionary={"estate_id":"estate.riverside","actor_ids":["actor.estate_guard"],"source_id":"source.estate.legacy"}
	send_estate(k,"estate_muster",params)
	check(k.preview("estate_muster",params).get("code")=="EstateAlreadyDetached","借调同一真人不能重复")
	var before: Dictionary=k.snapshot(); var bad: Dictionary=params.duplicate(true); bad.source_id="wrong"
	check(not F.send(k,"estate_return",bad).ok and k.snapshot()==before,"错误来源不能释放庄园借调")
	send_estate(k,"estate_return",params)
	check(not k.snapshot().estates["estate.riverside"].detached.has("actor.estate_guard"),"归还后真实释放原居民，无复制人物")

## 原建设从真实支付到多个半日完工逐步比较旧源，失败重试不得二次支付或重复扩容。
func build_flow(k) -> void:
	var started: Dictionary=send_estate(k,"estate_build",{"estate_id":"estate.riverside","facility":"granary"})
	if not started.ok: return
	var id: String=started.details.action_id; var ticks: int=k.snapshot().actions[id].remaining_ticks
	for i in range(ticks):
		var before: Dictionary=k.snapshot(); var req: Dictionary=request(k,"advance",{})
		k.probe.failure="advance"
		check(k.submit(req).get("code")=="InjectedEstateAdvance" and k.snapshot()==before,"生产及建设阶段后的硬失败完整回滚 "+str(i))
		k.probe.failure=""
		var old: Dictionary={}
		if legacy!=null:
			check(legacy.restore(before).ok,"旧源码恢复施工中态")
			old=legacy.submit(req)
		var result: Dictionary=k.submit(req)
		check(result.ok,"原建设半日推进 "+str(result))
		if legacy!=null:
			check(result==old and k.snapshot()==legacy.snapshot(),"生产在释放占用前，建设/资产/时钟/RNG完全等价 "+str(i)); compared+=1
		roundtrip(k,"build-tick-"+str(i))
	check(k.snapshot().facts[id].phase=="completed" and k.snapshot().facts[id].success and not k.snapshot().actions.has(id),"建设永久完成事实且只释放一次")
	check(k.snapshot().estates["estate.riverside"].facilities.granary==2,"真实完工才升级仓廪")

## 管事持有私粮仍属庄主，公物不能没有永久调拨来源便转成庄园私产。
func asset_boundaries(k) -> void:
	var before: Dictionary=k.snapshot(); var data: Dictionary=k.content().data; var actor: String=before.player_actor_id
	var quote: Dictionary=Commands.preview(before,data,actor,"estate_fund",{"estate_id":"estate.riverside","quantity":10})
	var candidate: Dictionary=before.duplicate(true); quote.cash.quantity=11
	check(Commands.apply(candidate,data,actor,"estate_fund",{},quote).get("code")=="StaleEstateQuote" and candidate==before,"被改资金报价不产生部分转账")
	var manager: String=before.estates["estate.riverside"].manager_actor_id
	check(F.send(k,"estate_withdraw",{"estate_id":"estate.riverside","item_id":"item.grain","quantity":2},manager).ok,"管事受托领真实私粮")
	var now: Dictionary=k.snapshot()
	check(Inventory.count(now.inventories[now.actors[manager].inventory_id],"item.grain",[{"kind":"actor","id":actor}])==2,"管事领粮不改变庄主产权")
	candidate=before.duplicate(true)
	var bag: String=candidate.actors[actor].inventory_id
	candidate.inventories[bag].lots=[]
	Inventory.add(candidate,bag,"item.grain",5,{"kind":"organization","id":"org.office"})
	var bad: Dictionary=Commands.preview(candidate,data,actor,"estate_deposit",{"estate_id":"estate.riverside","item_id":"item.grain","quantity":1})
	check(bad.get("code")=="InsufficientOwnedEstateGoods","没有调拨来源不能把公粮存成私产")

## 执行正式八命令与共享桥相邻路径；独立旧源码只作原行为预期，不从新adapter构造预期。
func run_checks() -> void:
	if contracts_only: coverage(); finish(); return
	if not legacy_path.is_empty():
		var script=load(legacy_path)
		if not check(script!=null,"独立旧Kernel源码可加载"): finish(); return
		legacy=script.new()
		if not check(legacy.new_game("scenario.alpha_life","res://data/generated/alpha_life.hmcatalog.json").ok,"独立旧源码正式开局"): finish(); return
		for kind in Estate.KINDS:
			if not check(not legacy._registry.commands.has(kind),"旧组合根没有庄园注册，预览必须走原源码分支 "+kind): finish(); return
	coverage()
	var k=fresh()
	if failures: finish(); return
	guard_order(k)
	send_estate(k,"estate_fund",{"estate_id":"estate.riverside","quantity":100})
	send_estate(k,"estate_withdraw",{"estate_id":"estate.riverside","item_id":"item.grain","quantity":10})
	send_estate(k,"estate_deposit",{"estate_id":"estate.riverside","item_id":"item.grain","quantity":5})
	send_estate(k,"estate_appoint",{"estate_id":"estate.riverside","manager_actor_id":"actor.estate_steward"})
	send_estate(k,"estate_assign",{"estate_id":"estate.riverside","resident_actor_id":"actor.estate_guard","duty":"guard"})
	muster_roundtrip(k)
	build_flow(k)
	asset_boundaries(k)
	if legacy!=null: check(compared>=8,"八命令及建设中间态均经旧源码独立对照")
	finish()

## 输出真实断言和旧源码比较数量，未跑长矩阵不能由本测试替代。
func finish() -> void:
	print("REGISTERED_ESTATES_LEGACY_COMPARISONS=",compared)
	print("REGISTERED_ESTATES_CHECKS=",checks," FAILURES=",failures)
	quit(1 if failures else 0)
