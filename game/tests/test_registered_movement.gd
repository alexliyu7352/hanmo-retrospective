## 移动注册短门：真实旧Kernel独立对照全部持久状态、早门首错、原行动桥与候选回滚。
extends SceneTree
const Commands=preload("res://core/domains/travel/movement_commands.gd")
const Legacy=preload("res://core/runtime/legacy_commands.gd")
const Builtin=preload("res://core/runtime/builtin_modules.gd")
const Registry=preload("res://core/runtime/module_registry.gd")
const Travel=preload("res://core/travel_rules.gd")
const Conditions=preload("res://core/condition_rules.gd")
const F=preload("res://tests/companion_fixture.gd")
const Codec=preload("res://core/save_codec.gd")
const Strict=preload("res://core/strict_json.gd")
var checks: int=0
var failures: int=0
var compared: int=0
var legacy_path: String=""
var old=null

## 测试仅包装真实规则以记录注册命中，故障注入发生在原规则写候选之后。
class Probe extends RefCounted:
	var calls: Dictionary={}
	var failure: String=""
	## 真实preview仍纯读，不以测试常量代替路线或准入报价。
	func preview(state: Dictionary,data: Dictionary,actor: String,kind: String,params: Dictionary) -> Dictionary:
		hit(kind,"preview"); return Commands.preview(state,data,actor,kind,params)
	## 原写入完成后再制造失败；不得让成功假回执替代实际救援或本地移动。
	func apply(state: Dictionary,data: Dictionary,actor: String,kind: String,params: Dictionary,offer: Dictionary) -> Dictionary:
		hit(kind,"apply")
		var result: Dictionary=Commands.apply(state,data,actor,kind,params,offer)
		if result.ok and failure=="apply": return {"ok":false,"code":"InjectedMovementApply"}
		if result.ok and failure=="tail": state.actors[actor].fatigue=-1
		return result
	## 调用次数保存在测试对象，不把探针字段放入世界存档。
	func hit(kind: String,stage: String) -> void:
		var key: String=kind+":"+stage; calls[key]=calls.get(key,0)+1

## 原Kernel事务及旧开工均不替换，只换受信Callable与受测故障出口。
class Observed extends "res://core/world_kernel.gd":
	var probe=Probe.new()
	var mutation: String=""
	var selected: String=""
	## 坏描述只作用于重开局的候选注册；失败必须留下原世界与可用注册。
	func _module_descriptors() -> Array:
		var rows: Array=super._module_descriptors(); var moved: Dictionary={}
		for row in rows:
			if row.id!="travel": continue
			for entry in row.commands.duplicate():
				if not entry.kind in Commands.KINDS: continue
				entry.preview=probe.preview
				if entry.mode=="instant": entry.apply=probe.apply
				if entry.kind!=selected: continue
				match mutation:
					"missing": row.commands.erase(entry)
					"duplicate": row.commands.append(entry.duplicate(true))
					"owner": moved=entry; row.commands.erase(entry)
					"gate": entry.gate_id="wrong_gate"
					"mode": entry.mode="instant"; entry.apply=probe.apply; entry.erase("compat_action")
					"bridge": entry.erase("compat_action")
					"borrowed_kind","borrowed_owner":
						var borrowed: Dictionary=entry.duplicate(true); borrowed.kind="borrowed_rescue_gate"
						if mutation=="borrowed_kind": row.commands.append(borrowed)
						else: moved=borrowed
		if not moved.is_empty():
			for row in rows:
				if row.id=="tasks": row.commands.append(moved)
		return rows
	## travel使用原Kernel开工，位置和参与者占用写入后的故障必须完整回滚。
	func _begin_action(state: Dictionary,actor: String,kind: String,params: Dictionary,offer: Dictionary) -> Dictionary:
		var result: Dictionary=super._begin_action(state,actor,kind,params,offer)
		if kind!="travel": return result
		probe.hit(kind,"begin")
		if result.ok and probe.failure=="apply": return {"ok":false,"code":"InjectedMovementBegin"}
		if result.ok and probe.failure=="tail": state.actors[actor].fatigue=-1
		return result
	## 半日已真实推进后返回硬失败，下一次重试不能保留收费、RNG或救援损耗。
	func _advance(state: Dictionary,hold: Array=[]) -> Dictionary:
		var result: Dictionary=super._advance(state,hold)
		if result.ok and probe.failure=="advance": return {"ok":false,"code":"InjectedMovementAdvance"}
		return result

## 必须显式传入独立旧源码；缺基线时失败而不将新实现自比冒充等价。
func _init() -> void:
	for arg in OS.get_cmdline_user_args():
		if arg.begins_with("--legacy-kernel="): legacy_path=arg.trim_prefix("--legacy-kernel=")
	call_deferred("run_checks")

## 所有断言都影响最终退出码，回执与状态差异不能只写到日志。
func check(value: bool,label: String) -> bool:
	checks+=1
	if not value: failures+=1; printerr("FAIL: ",label)
	return value

## 在旧作者地图上只改变开局预算或复用已有同行夹具，成功动作不直接编辑运行状态。
func fresh(mode: String="normal"):
	var scenario: String="scenario.two_stories"; var path: String="res://data/generated/two_stories.hmcatalog.json"
	if mode=="party":
		var source=F.fresh(); check(not source.snapshot().is_empty(),"复用已有同行作者夹具")
		scenario="scenario.lived_city"; path="user://test-companions-false-role.carter.json"
	elif mode=="poor":
		var header: Dictionary=Strict.parse_text(FileAccess.get_file_as_string(path)).value
		header.catalog.wallets["wallet.player"].initial_balance=8
		header.catalog_sha256=(JSON.stringify(header.catalog,"",true,true)+"\n").sha256_text()
		path="user://test-registered-movement-poor.json"
		var file=FileAccess.open(path,FileAccess.WRITE); file.store_string(JSON.stringify(header)); file.close()
	var k=Observed.new(); check(k.new_game(scenario,path).ok,"正式开局 "+mode)
	if old!=null:
		check(old.new_game(scenario,path).ok,"旧Kernel用相同作者内容开局 "+mode)
		for kind in Commands.KINDS: check(not old._registry.commands.has(kind),"旧源码没有移动注册，确保走独立内联分支 "+kind)
	return k

## 所有提交均通过真实版本、本人序号及公开命令，不手动调用成功apply。
func request(k,kind: String,params: Dictionary={}) -> Dictionary:
	var state: Dictionary=k.snapshot(); var actor: String=state.player_actor_id
	return {"kind":kind,"params":params,"actor_id":actor,"request_seq":state.request_sequences[actor]+1,"expected_revision":state.revision}

## 同一合法前态逐步送旧Kernel与新Kernel，比较回执、完整状态及可编码恢复。
func send(k,kind: String,params: Dictionary={}, inject: bool=false) -> Dictionary:
	var before: Dictionary=k.snapshot(); var req: Dictionary=request(k,kind,params)
	var quote: Dictionary=k.preview(kind,params)
	if not check(quote.ok,"原命令可用 "+kind+" "+str(quote)): return quote
	for _i in range(3): check(k.preview(kind,params)==quote and k.snapshot()==before,"重复preview保持状态与RNG "+kind)
	if inject:
		for failure in ["apply","tail"]:
			k.probe.failure=failure
			var bad: Dictionary=k.submit(req)
			var code: String="InjectedMovementBegin" if kind=="travel" else "InjectedMovementApply"
			check(not bad.ok and (failure!="apply" or bad.get("code")==code) and k.snapshot()==before,"写后整候选回滚 "+kind+" "+failure+" "+str(bad))
		k.probe.failure=""
	if kind=="advance":
		k.probe.failure="advance"
		check(k.submit(req).get("code")=="InjectedMovementAdvance" and k.snapshot()==before,"实际半日后失败整候选回滚")
		k.probe.failure=""
	check(old.restore(before).ok,"独立旧Kernel恢复合法前态 "+kind)
	var expected: Dictionary=old.submit(req)
	var stage: String="begin" if kind=="travel" else "apply"
	var hits: int=k.probe.calls.get(kind+":"+stage,0)
	var result: Dictionary=k.submit(req)
	if not check(result.ok,"正式提交成功 "+kind+" "+str(result)): return result
	if kind in Commands.KINDS: check(k.probe.calls.get(kind+":"+stage,0)==hits+1,"注册Callable或旧行动桥实际只执行一次 "+kind)
	check(result==expected and k.snapshot()==old.snapshot(),"旧源码回执与全持久状态相同 "+kind); compared+=1
	var after: Dictionary=k.snapshot(); var calls: Dictionary=k.probe.calls.duplicate(true)
	check(k.submit(req)==result and k.snapshot()==after and k.probe.calls==calls,"相同request不重收费、执行或分配ID "+kind)
	var stale: Dictionary=req.duplicate(true); stale.request_seq+=1
	check(k.submit(stale).get("code")=="StalePreview" and k.snapshot()==after and k.probe.calls==calls,"陈旧revision拒绝且不消耗序号 "+kind)
	var encoded: String=Codec.encode(after,0,"registered-movement",k.content())
	var decoded: Dictionary=Codec.decode(encoded,k.content())
	check(decoded.ok and k.restore(decoded.state).ok and k.snapshot()==after,"真实Codec全状态往返 "+kind)
	check(encoded==Codec.encode(old.snapshot(),0,"registered-movement",old.content()),"旧新Codec字节完全相同 "+kind)
	return result

## 固定覆盖与每一坏登记从new_game失败，禁止漏登记悄悄掉回原内联成功路径。
func contracts() -> void:
	var registry: Dictionary=Registry.build(Builtin.descriptors())
	check(registry.ok,"正式注册表构建")
	var k=fresh(); var before: Dictionary=k.snapshot(); var catalog: Dictionary=k.content()
	for kind in Commands.KINDS:
		var expected: Dictionary=Legacy.find(kind); var entry: Dictionary=registry.commands[kind]
		check(entry.owner==expected.owner and entry.mode==expected.mode and entry.gate_id==expected.gate_id,"历史描述与真实注册一致 "+kind)
		check(expected.source.path=="game/core/domains/travel/movement_commands.gd" and expected.source.constant=="KINDS","迁出内联项只从真实适配器集合取源 "+kind)
		for mutation in ["missing","duplicate","owner","gate"]:
			k.selected=kind; k.mutation=mutation
			var result: Dictionary=k.new_game()
			var code: String="MissingBuiltinCommand" if mutation=="missing" else ("DuplicateCommand" if mutation=="duplicate" else ("UnknownCompatAction" if mutation=="owner" and kind=="travel" else "InvalidBuiltinCommand"))
			check(result.get("code")==code and k.snapshot()==before and k.content()==catalog,"坏登记保留旧世界 "+kind+" "+mutation+" "+str(result))
			check(k.preview("move_local",{"place_id":"west.tavern"}).ok,"开局失败旧注册仍可用")
	for pair in [["travel","mode","InvalidBuiltinCommand"],["travel","bridge","UnknownCommandAction"],["rescue","borrowed_kind","InvalidBuiltinCommand"],["rescue","borrowed_owner","InvalidBuiltinCommand"]]:
		k.selected=pair[0]; k.mutation=pair[1]
		check(k.new_game().get("code")==pair[2] and k.snapshot()==before,"桥与救援借门明确拒绝 "+str(pair))
	check(Commands.preview({}, {},"missing","unknown",{}).get("code")=="UnsupportedAction" and Commands.apply({}, {},"missing","travel",{},{}).get("code")=="UnsupportedAction","未知kind或绕桥开工均先拒绝")

## 只在隔离快照叠加多个错误，比较旧Kernel真实首错；坏快照不当作可玩或可保存证据。
func guard_order() -> void:
	var k=fresh(); var initial: Dictionary=k.snapshot(); var actor: String=initial.player_actor_id
	for kind in Commands.KINDS:
		for variant in ["domain","dead","busy","following","army","encounter","battle","siege","army_return"]:
			var state: Dictionary=initial.duplicate(true)
			match variant:
				"dead": state.actors[actor].alive=false; state.actors[actor].available=false
				"busy": state.actors[actor].action_id="probe"; state.actions.probe={"kind":"wait"}
				"following": state.actors[actor].location={"kind":"Following","party_id":"missing"}; state.actors[actor].available=false
				"army": state.actors[actor].location={"kind":"InArmy","army_id":"missing"}; state.actors[actor].available=false
				"encounter": state.encounters.probe={"status":"pending","participants":[actor]}; state.actors[actor].available=false
				"battle": state.battles.probe={"kind":"personal","phase":"planning","units":{actor:{}}}; state.actors[actor].available=false
				"siege":
					state.armies.probe={"status":"active","member_actor_ids":[actor]}; state.sieges.probe={"phase":"planning","armies":{"probe":0},"withdrawn_army_ids":[]}; state.actors[actor].available=false
				"army_return": state.armies.probe={"status":"active","member_actor_ids":[actor]}
			var before: Dictionary=state.duplicate(true); var result: Dictionary=k._preview(state,kind,{},actor)
			check(result==old._preview(state,kind,{},actor) and state==before,"旧首错且只读 "+kind+" "+variant)
		check(k._preview(initial,kind,{},"missing").get("code")=="UnknownActor","未知人物首拒 "+kind)
	var state: Dictionary=initial.duplicate(true); state.actors[actor].available=false
	check(k._preview(state,"rescue",{},actor).get("code")=="NotTravelling","救援保留actor可用前门")
	var hits: int=k.probe.calls.get("rescue:preview",0); k._registry.commands.rescue.owner="tasks"
	check(k._preview(state,"rescue",{},actor).get("code")=="ActorUnavailable" and k.probe.calls.get("rescue:preview",0)==hits,"错误owner不可借救援早门")
	var borrowed: Dictionary=k._registry.commands.rescue.duplicate(true); borrowed.owner="travel"; borrowed.kind="borrowed"
	k._registry.commands.borrowed=borrowed
	check(k._preview(state,"borrowed",{},actor).get("code")=="ActorUnavailable","同owner错误kind不可借救援门")

## 合法通道触发事件、离场清会话和事件硬失败均保留原先后，不新增地图或事件内容。
func local_flow() -> void:
	var k=fresh(); send(k,"move_local",{"place_id":"west.tavern"},true)
	var actor: String=k.snapshot().player_actor_id
	check(k.snapshot().events[actor+"|event.letter"].status=="queued","实际入酒肆排队旧事件")
	send(k,"talk",{"role_id":"role.sender"})
	check(k.snapshot().interactions.has(actor),"真实对白在场")
	var before: Dictionary=k.snapshot(); k._catalog.data.events["event.letter"].on_unavailable="unsupported"
	var candidate: Dictionary=before.duplicate(true)
	var direct: Dictionary=Travel.move_local(candidate,k._catalog.data,actor,"west.street")
	check(direct.get("code")=="UnsupportedEventPolicy" and candidate.actors[actor].location.place_id=="west.street" and not candidate.interactions.has(actor),"入场硬失败发生在真实位置变更及清会话之后")
	check(k.submit(request(k,"move_local",{"place_id":"west.street"})).get("code")=="UnsupportedEventPolicy" and k.snapshot()==before,"入场硬失败原样返回并恢复原位置与完整会话")
	k._catalog.data.events["event.letter"].on_unavailable="cancel"
	send(k,"move_local",{"place_id":"west.street"},true)
	check(not k.snapshot().interactions.has(actor) and k.snapshot().events[actor+"|event.letter"].status=="cancelled","先清会话且原观察器取消离场事件")
	before=k.snapshot()
	for pair in [[{},"UnknownPlace"],[{"place_id":"east.home"},"NoLocalPassage"]]:
		check(k.preview("move_local",pair[0]).get("code")==pair[1] and k.snapshot()==before,"本地参数首错 "+pair[1])

## 同行移动和两半日旅行全部使用原命令，开工不预收费、参与者真实占用并且完工释放。
func party_trip() -> void:
	var k=fresh("party"); send(k,"move_local",{"place_id":"west.tavern"}); send(k,"join_party",{"context":F.context(k)})
	send(k,"discover_offer",{"offer_id":"offer.letter","context":F.context(k,"role.sender")})
	var state: Dictionary=k.snapshot(); var actor: String=state.player_actor_id
	state.actors["actor.carter"].available=false
	check(k._preview(state,"move_local",{"place_id":"west.street"},actor)==old._preview(state,"move_local",{"place_id":"west.street"},actor) and k._preview(state,"move_local",{"place_id":"west.street"},actor).get("code")=="ParticipantBusy","同行不可用先于场所准入")
	var data: Dictionary=k.content().data; data.places["west.street"].admission=[{"op":"task_state","offer_id":"offer.letter","state":"known"}]
	check(Conditions.all_true(k.snapshot(),actor,data.places["west.street"].admission) and not Conditions.all_true(k.snapshot(),"actor.carter",data.places["west.street"].admission),"本人经真实发现取得准入，同行仍未取得")
	var prior: Array=old._catalog.data.places["west.street"].admission
	old._catalog.data.places["west.street"].admission=data.places["west.street"].admission
	var denied: Dictionary=Commands.preview(k.snapshot(),data,actor,"move_local",{"place_id":"west.street"})
	check(denied.get("code")=="AudienceDenied" and denied==old._preview(k.snapshot(),"move_local",{"place_id":"west.street"},actor),"不得只验领队，同行准入首错与旧Kernel一致")
	old._catalog.data.places["west.street"].admission=prior
	send(k,"move_local",{"place_id":"west.street"},true)
	var before: Dictionary=k.snapshot(); var quote: Dictionary=k.preview("travel",{"destination":"village.east"})
	check(quote.duration==2 and quote.cost==28,"原路线含真实同行费用")
	var started: Dictionary=send(k,"travel",{"destination":"village.east"},true)
	if not started.ok: return
	state=k.snapshot(); var action: Dictionary=state.actions[started.details.action_id]
	check(action.participants==[actor,"actor.carter"] and state.actors["actor.carter"].action_id==action.id and state.wallets==before.wallets,"旧开工绑定全体且不预收费用")
	check(k.preview("rescue").get("code")=="NotStranded","钱足时救援不绕过原缺口检查")
	send(k,"advance"); send(k,"advance")
	state=k.snapshot()
	check(state.actors[actor].location.place_id=="east.entrance" and state.actors[actor].action_id.is_empty() and state.actors["actor.carter"].action_id.is_empty(),"原两半日到站并释放全体")
	check(state.wallets["wallet.player"].balance==before.wallets["wallet.player"].balance-28,"每半日只收既有同行费用")

## 贫困作者开局经正常一次收费真实搁浅；救援原地替换行动并按原两半日返城。
func rescue_flow() -> void:
	var k=fresh("poor"); var started: Dictionary=send(k,"travel",{"destination":"village.east"},true)
	if not started.ok: return
	send(k,"advance")
	var before: Dictionary=k.snapshot(); var actor: String=before.player_actor_id; var id: String=started.details.action_id
	check(k.preview("advance").get("code")=="Stranded" and k.snapshot()==before,"一次真实收费耗尽8钱后搁浅")
	var altered: Dictionary=before.duplicate(true); altered.actors[actor].available=false
	check(k._preview(altered,"rescue",{},actor)==old._preview(altered,"rescue",{},actor) and k._preview(altered,"rescue",{},actor).ok,"救援不错误后移到ActorUnavailable或Busy")
	send(k,"rescue",{},true)
	var state: Dictionary=k.snapshot(); var action: Dictionary=state.actions[id]
	check(action.kind=="rescue" and action.remaining_ticks==2 and action.params=={"origin":"city.west"} and action.participants==before.actions[id].participants and action.start_tick==before.actions[id].start_tick,"救援只替换既有kind、剩余时长与params")
	check(state.actors[actor].action_id==id and state.next_instance_id==before.next_instance_id+1 and state.wallets==before.wallets,"不新建行动、不退款，只分配一次提交回执ID")
	check(k.preview("rescue").get("code")=="NotTravelling","新的救援请求不能再次替换")
	send(k,"advance"); send(k,"advance")
	state=k.snapshot()
	check(state.tick==3 and state.actors[actor].location.place_id=="west.street" and not state.actions.has(id),"原救援完整两半日返城并释放行动")

## 执行范围严格限于三项注册、合法短旅程、局部事件与错误首序，不跑长世界或全量门。
func run_checks() -> void:
	if not check(not legacy_path.is_empty(),"必须指定独立旧Kernel源码"): finish(); return
	var source=load(legacy_path)
	if not check(source!=null,"独立旧Kernel可加载"): finish(); return
	old=source.new()
	contracts(); guard_order(); local_flow(); party_trip(); rescue_flow()
	check(compared==15,"全部三命令与短中间态都有旧源码对照")
	finish()

## 同时报告断言与全态比较数，不能把局部通过标成A1.7或整体发行完成。
func finish() -> void:
	print("REGISTERED_MOVEMENT_LEGACY_COMPARISONS=",compared)
	print("REGISTERED_MOVEMENT_CHECKS=",checks," FAILURES=",failures)
	quit(1 if failures else 0)
