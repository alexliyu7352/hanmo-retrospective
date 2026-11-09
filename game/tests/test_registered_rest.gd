## 休整注册只核验1至3半日的真实供养、取消与原子失败；完整状态对照精确旧Kernel源码。
extends SceneTree
const Commands=preload("res://core/domains/services/rest_commands.gd")
const Rest=preload("res://core/long_rest.gd")
const Builtin=preload("res://core/runtime/builtin_modules.gd")
const Registry=preload("res://core/runtime/module_registry.gd")
const Codec=preload("res://core/save_codec.gd")
const Strict=preload("res://core/strict_json.gd")
var checks: int=0
var failures: int=0
var comparisons: int=0
var legacy_script=null
var legacy_path: String=""
var kernels: Array=[]

## 真实取消先改变来源事实；此刻原action和占用仍须存在，释放留给Kernel协调。
class Probe extends RefCounted:
	var calls: Dictionary={}
	var failure: String=""
	var cancel_before_release: bool=false
	## 只计真实绑定命中，不替换原只读报价或能力规则。
	func preview(state: Dictionary,data: Dictionary,actor: String,kind: String,params: Dictionary) -> Dictionary:
		calls[kind+":preview"]=calls.get(kind+":preview",0)+1
		return Commands.preview(state,data,actor,kind,params)
	## 真实取消后才注错，用完整候选快照证明已执行变更未泄漏。
	func apply(state: Dictionary,data: Dictionary,actor: String,kind: String,params: Dictionary,offer: Dictionary) -> Dictionary:
		calls[kind+":apply"]=calls.get(kind+":apply",0)+1
		var result: Dictionary=Commands.apply(state,data,actor,kind,params,offer)
		cancel_before_release=state.facts[offer.action_id].stop_reason=="cancelled" and state.actions.has(offer.action_id) and state.actors[actor].action_id==offer.action_id
		if result.ok and failure=="apply": return {"ok":false,"code":"InjectedRestCancel"}
		if result.ok and failure=="schema": state.wallets[state.actors[actor].wallet_id].balance=-1
		return result

## 继承生产Kernel，故障在真实注册或候选变更之后触发，不伪造休整生命周期。
class TestKernel extends "res://core/world_kernel.gd":
	var probe=Probe.new()
	var mutation: String=""
	var selected: String="long_rest"
	var fail_begin: bool=false
	var begin_hits: int=0
	var fail_phase: String=""
	var boundaries: Dictionary={}
	## 坏描述走完整启动失败；真实半日阶段仅增加观察快照与明确硬失败。
	func _module_descriptors() -> Array:
		var rows: Array=super._module_descriptors(); var moved: Dictionary={}
		for row in rows:
			if row.id=="services":
				for entry in row.commands.duplicate():
					if not entry.kind in Rest.KINDS: continue
					entry.preview=probe.preview
					if entry.mode=="instant": entry.apply=probe.apply
					if entry.kind!=selected: continue
					match mutation:
						"missing": row.commands.erase(entry)
						"duplicate": row.commands.append(entry.duplicate(true))
						"gate": entry.gate_id="legacy_kernel"
						"owner": moved=entry; row.commands.erase(entry)
						"mode": entry.mode="instant"; entry.apply=probe.apply; entry.erase("compat_action")
						"bridge": entry.erase("compat_action")
						"extra_bridge": entry.compat_action="legacy_begin_action"
						"foreign_gate": row.commands.append({"kind":"rest_invented","mode":"instant","gate_id":Commands.GATE_ID,"preview":probe.preview,"apply":probe.apply})
			for entry in row.phases:
				if not entry.id in ["estate_production","actions_step","clock_commit"]: continue
				var run: Callable=entry.run; var id: String=entry.id
				entry.run=func(state: Dictionary,data: Dictionary,context: Dictionary) -> Dictionary:
					var result: Dictionary=run.call(state,data,context)
					boundaries[id]=state.duplicate(true)
					if result.ok and fail_phase==id: return {"ok":false,"code":"InjectedRestPhase"}
					return result
		if not moved.is_empty():
			for row in rows:
				if row.id=="tasks": row.commands.append(moved)
		return rows
	## 真正begin写入占用、来源与ID之后失败，整个提交必须保持原状态和请求序号。
	func _begin_action(state: Dictionary,actor: String,kind: String,params: Dictionary,offer: Dictionary) -> Dictionary:
		var result: Dictionary=super._begin_action(state,actor,kind,params,offer)
		if kind=="long_rest":
			begin_hits+=1
			if result.ok and fail_begin: return {"ok":false,"code":"InjectedRestBegin"}
		return result

## 显式旧源码路径由调用方从已提交对象提取；不以另写旧逻辑冒充源码等价。
func _init() -> void:
	for arg in OS.get_cmdline_user_args():
		if arg.begins_with("--legacy-kernel="): legacy_path=arg.trim_prefix("--legacy-kernel=")
	call_deferred("run_checks")

## 每项断言保留语义标签；只有真实失败为零时进程成功退出。
func check(value: bool,label: String) -> bool:
	checks+=1
	if not value: failures+=1; printerr("FAIL: ",label)
	return value

## 合法作者配置只改变初始私粮和庄务产量，不预填休整、年龄或世界日期。
func fresh(food: int=100,resident_owner: bool=false) -> Dictionary:
	var header: Dictionary=Strict.parse_text(FileAccess.get_file_as_string("res://data/generated/alpha_life.hmcatalog.json")).value
	for lot in header.catalog.inventories["inventory.estate"].goods:
		if lot.item_id=="item.grain": lot.quantity=food
	header.catalog.estates["estate.riverside"].output_per_worker=0
	if resident_owner:
		header.catalog.estates["estate.riverside"].resident_actor_ids.append("actor.player")
		header.catalog.actors["actor.player"].initial_place="west.estate"
	header.catalog_sha256=(JSON.stringify(header.catalog,"",true,true)+"\n").sha256_text()
	var path: String="user://registered-rest-"+str(food)+"-"+str(resident_owner)+".json"
	var file=FileAccess.open(path,FileAccess.WRITE); file.store_string(JSON.stringify(header)); file.close()
	var k=TestKernel.new(); kernels.append(k)
	check(k.new_game("scenario.alpha_life",path).ok,"原管线合法休整作者变体")
	var old=null
	if legacy_script!=null:
		old=legacy_script.new(); kernels.append(old)
		check(old.new_game("scenario.alpha_life",path).ok,"精确旧源装载同一作者变体")
	var pair: Dictionary={"current":k,"old":old}
	if not resident_owner: send(pair,"move_local",{"place_id":"west.estate"})
	return pair

## 请求只使用真实当前人物、版本和下一个序号，不越过正式提交入口。
func request(k,kind: String,params: Dictionary={}) -> Dictionary:
	var state: Dictionary=k.snapshot(); var actor: String=state.player_actor_id
	return {"kind":kind,"params":params,"actor_id":actor,"request_seq":state.request_sequences[actor]+1,"expected_revision":state.revision}

## 每次正式提交对照旧源回执及全部状态；重复请求不能再次进入注册处理器。
func send(pair: Dictionary,kind: String,params: Dictionary={}) -> bool:
	var k=pair.current; var old=pair.old; var command: Dictionary=request(k,kind,params); var old_result: Dictionary={}
	if old!=null:
		if not check(old.restore(k.snapshot()).ok,"旧源恢复同一合法状态 "+kind): return false
		old_result=old.submit(command)
	var prior: Dictionary=k.probe.calls.duplicate(true); var begins: int=k.begin_hits
	var result: Dictionary=k.submit(command)
	if not check(result.ok,"真实提交 "+kind+" "+str(result)): return false
	var after: Dictionary=k.snapshot(); var hits: Dictionary=k.probe.calls.duplicate(true)
	if kind in Rest.KINDS:
		check(hits.get(kind+":preview",0)==prior.get(kind+":preview",0)+1,"唯一注册preview命中 "+kind)
		if kind=="long_rest": check(k.begin_hits==begins+1,"唯一兼容begin命中")
		else: check(hits.get(kind+":apply",0)==prior.get(kind+":apply",0)+1 and k.probe.cancel_before_release,"真实cancel先于release且命中注册apply")
	check(k.submit(command)==result and k.snapshot()==after and k.probe.calls==hits and k.begin_hits==begins+(1 if kind=="long_rest" else 0),"重复请求完整幂等 "+kind)
	if old!=null:
		check(old_result==result and old.snapshot()==after,"旧源码完整state/result等价 "+kind); comparisons+=1
	return true

## 原Codec保留整个状态，包括RNG、请求缓存、日期、事实及所有资源账目。
func roundtrip(k,label: String) -> void:
	var before: Dictionary=k.snapshot(); var saved: Dictionary=Codec.decode(Codec.encode(before,0,"registered-rest",k.content()),k.content())
	check(saved.ok and k.restore(saved.state).ok and k.snapshot()==before,"完整Codec往返 "+label)

## 固定集合、真实validator、同owner拥有权与专用早门在完整启动时独立核验。
func coverage() -> bool:
	var built: Dictionary=Registry.build(Builtin.descriptors())
	if not check(built.ok,"生产Registry可构建 "+str(built.get("code"))): return false
	var actual: Array=[]
	for kind in built.commands:
		if built.commands[kind].gate_id==Commands.GATE_ID: actual.append(kind)
	var expected: Array=Rest.KINDS.duplicate(); expected.sort()
	check(actual==expected and Commands.command_kinds()==Rest.KINDS,"真实Rest.KINDS且无额外早门命令")
	check(built.commands.long_rest.mode=="action" and built.commands.long_rest.get("compat_action")=="legacy_begin_action" and built.commands.long_rest.apply==null and not built.actions.has("long_rest") and built.legacy_action_validators.long_rest.validate==Rest.validate_action,"仅明确旧行动桥而不造生命周期")
	for kind in Rest.KINDS:
		for mutation in ["missing","duplicate","gate","owner"]:
			var k=TestKernel.new(); kernels.append(k); check(k.new_game().ok,"坏描述前已有真实世界")
			var before: Dictionary=k.snapshot(); var content: Dictionary=k.content(); k.selected=kind; k.mutation=mutation
			var result: Dictionary=k.new_game()
			var code: String={"missing":"MissingBuiltinCommand","duplicate":"DuplicateCommand","gate":"InvalidBuiltinCommand","owner":"UnknownCompatAction" if kind=="long_rest" else "InvalidBuiltinCommand"}[mutation]
			check(result.get("code")==code and k.snapshot()==before and k.content()==content,"坏注册拒绝并保留旧世界 "+kind+" "+mutation+" "+str(result))
	for row in [["long_rest","mode"],["long_rest","bridge"],["rest_cancel","extra_bridge"],["rest_cancel","foreign_gate"]]:
		var k=TestKernel.new(); kernels.append(k); k.selected=row[0]; k.mutation=row[1]
		check(not k.new_game().ok,"假行动或借早门拒绝 "+str(row))
	for mutation in ["validator","ownership"]:
		var rows: Array=Builtin.descriptors()
		for row in rows:
			if row.id!="services": continue
			if mutation=="validator": row.legacy_action_validators=row.legacy_action_validators.filter(func(entry): return entry.kind!="long_rest")
			else: row.owns=row.owns.filter(func(claim): return not (claim.container=="actions" and claim.scope=="kind" and claim.key=="long_rest"))
		check(Registry.build(rows).get("code")==("UnknownCompatAction" if mutation=="validator" else "MissingStateOwner"),"同owner真实旧验证及拥有权必需 "+mutation)
	return failures==0

## 错误组合直接比较旧只读预览；专用早门仍在军战、围城、行伍与遭遇之后。
func guards(pair: Dictionary,kind: String,params: Dictionary) -> void:
	var k=pair.current; var old=pair.old
	var state: Dictionary=k.snapshot(); var actor: String=state.player_actor_id
	var expected: Dictionary={"unavailable":"ActorBusy" if kind=="long_rest" else "NotResting","busy":"ActorBusy" if kind=="long_rest" else "NotResting","in_army":"NotArmyCommander","following":"NotPartyLeader","no_capability":"FamilyUnavailable","no_capability_busy":"FamilyUnavailable","encounter":"EncounterPending","battle":"BattlePending","siege":"SiegePending","malformed_unavailable":"InvalidRestDuration" if kind=="long_rest" else "NotResting"}
	for variant in expected:
		var candidate: Dictionary=state.duplicate(true); var arguments: Dictionary=params.duplicate(true)
		if variant in ["unavailable","malformed_unavailable"]: candidate.actors[actor].alive=false; candidate.actors[actor].available=false
		if variant=="malformed_unavailable": arguments.duration=0
		if variant in ["busy","no_capability_busy"]:
			candidate.actors[actor].action_id="busy"; candidate.actions.busy={"kind":"wait","actor_id":actor}
		if variant=="following": candidate.actors[actor].location={"kind":"Following","leader_actor_id":"actor.chen"}
		if variant=="in_army": candidate.actors[actor].location={"kind":"InArmy","army_id":"none"}
		if variant=="encounter": candidate.encounters.probe={"status":"pending","participants":[actor]}
		if variant=="battle": candidate.battles.probe={"kind":"military","phase":"active","participant_actor_ids":[actor]}
		if variant=="siege":
			candidate.armies.probe={"status":"active","member_actor_ids":[actor]}
			candidate.sieges.probe={"phase":"active","armies":{"probe":{}},"withdrawn_army_ids":[]}
		var data: Dictionary=k._catalog.data.duplicate(true); var old_data: Dictionary=old._catalog.data.duplicate(true) if old!=null else {}
		if variant in ["no_capability","no_capability_busy"]:
			k._catalog.data.required_capabilities.erase("family")
			if old!=null: old._catalog.data.required_capabilities.erase("family")
		var before: Dictionary=candidate.duplicate(true); var result: Dictionary=k._preview(candidate,kind,arguments,actor)
		check((old==null or result==old._preview(candidate,kind,arguments,actor)) and result.get("code")==expected[variant] and candidate==before,"原错误优先且预览只读 "+kind+" "+variant+" "+str(result))
		k._catalog.data=data
		if old!=null: old._catalog.data=old_data
	check(k._preview(state,kind,params,"missing").get("code")=="UnknownActor","未知人物优先于Rest预览")
	var borrowed: Dictionary=built_probe(); k._registry.commands.rest_invented=borrowed
	state.actors[actor].available=false
	check(k._preview(state,"rest_invented",{},actor).get("code")=="ActorUnavailable","其他kind不能借Rest门绕不可用")
	state.actors[actor].available=true; state.actors[actor].action_id="busy"
	check(k._preview(state,"rest_invented",{},actor).get("code")=="ActorBusy","其他kind不能借Rest门绕忙碌")
	k._registry.commands.erase("rest_invented")

## 泛型测试命令只用来证实Kernel按owner与原kind共同约束早门。
func built_probe() -> Dictionary:
	return {"owner":"services","mode":"instant","gate_id":Commands.GATE_ID,"preview":Commands.preview,"apply":Commands.apply}

## 三半日完成沿原庄务先供养、休整后供养、最后时钟提交，并比较旧完整状态。
func completion() -> void:
	var pair: Dictionary=fresh(); var k=pair.current; var params: Dictionary={"estate_id":"estate.riverside","duration":3}
	guards(pair,"long_rest",params); guards(pair,"rest_cancel",{})
	var before: Dictionary=k.snapshot(); k.fail_begin=true
	check(k.submit(request(k,"long_rest",params)).get("code")=="InjectedRestBegin" and k.snapshot()==before,"真实begin失败不泄漏来源占用ID和序号")
	k.fail_begin=false
	if not send(pair,"long_rest",params): return
	var state: Dictionary=k.snapshot(); var id: String=state.actors[state.player_actor_id].action_id
	check(state.tick==0 and state.facts[id].elapsed==0 and state.facts[id].consumed==0,"begin不预付不跳时")
	roundtrip(k,"started")
	for tick in range(1,4):
		before=k.snapshot(); k.fail_phase="actions_step"
		check(k.submit(request(k,"advance")).get("code")=="InjectedRestPhase" and k.snapshot()==before and k.boundaries.actions_step.facts[id].elapsed==tick,"真实供养与完工后硬失败回滚全部候选")
		k.fail_phase=""
		if not send(pair,"advance"): return
		state=k.snapshot()
		check(state.tick==tick and state.facts[id].elapsed==tick and state.facts[id].consumed==tick,"逐半日私粮实付且日期相同 "+str(tick))
		check(k.boundaries.estate_production.facts[id].elapsed==tick-1 and k.boundaries.actions_step.facts[id].elapsed==tick and k.boundaries.actions_step.tick==tick-1 and k.boundaries.clock_commit.tick==tick,"庄务→休整→时钟原先后 "+str(tick))
		roundtrip(k,"tick "+str(tick))
	check(state.facts[id].stop_reason=="duration" and state.facts[id].completed_tick==3 and not state.actions.has(id) and state.actors[state.player_actor_id].action_id.is_empty(),"三半日正常完工一次释放")

## 取消在第一个真实半日之后，不退款、不倒退日期，apply或尾验失败都保留完整旧状态。
func cancellation() -> void:
	var pair: Dictionary=fresh(); var k=pair.current
	if not send(pair,"long_rest",{"estate_id":"estate.riverside","duration":3}) or not send(pair,"advance"): return
	var before: Dictionary=k.snapshot(); var id: String=before.actors[before.player_actor_id].action_id
	for failure in ["apply","schema"]:
		k.probe.failure=failure
		check(not k.submit(request(k,"rest_cancel")).ok and k.snapshot()==before and k.probe.cancel_before_release,"真实取消后硬失败完整回滚 "+failure)
	k.probe.failure=""
	if not send(pair,"rest_cancel"): return
	var after: Dictionary=k.snapshot()
	check(after.tick==before.tick and after.facts[id].elapsed==1 and after.facts[id].consumed==1 and after.facts[id].stop_reason=="cancelled" and after.inventories==before.inventories and not after.actions.has(id) and after.actors[after.player_actor_id].action_id.is_empty(),"取消保持真实日期粮账并释放行动")
	check(k.submit(request(k,"rest_cancel")).get("code")=="NotResting" and k.snapshot()==after,"新序号重复取消拒绝不改世界")
	roundtrip(k,"cancelled")

## 庄務先吃尽三份粮后触发休整缺粮的软终局；居民庄主只由休整吃一份。
func feeding_edges() -> void:
	var pair: Dictionary=fresh(3); var k=pair.current
	if not send(pair,"long_rest",{"estate_id":"estate.riverside","duration":3}): return
	var id: String=k.snapshot().actors[k.snapshot().player_actor_id].action_id
	if not send(pair,"advance"): return
	var state: Dictionary=k.snapshot()
	check(state.tick==1 and state.facts[id].elapsed==1 and state.facts[id].consumed==0 and state.facts[id].stop_reason=="RestFoodShortage" and not state.actions.has(id),"真实缺粮是保留本半日的软终局")
	check(k.preview("long_rest",{"estate_id":"estate.riverside","duration":1}).get("code")=="RestFoodShortage" and k.snapshot()==state,"缺粮不得重启且无负库存")
	roundtrip(k,"food shortage")
	pair=fresh(100,true); k=pair.current
	if not send(pair,"long_rest",{"estate_id":"estate.riverside","duration":1}) or not send(pair,"advance"): return
	state=k.snapshot(); var fact: Dictionary={}
	for row in state.facts.values():
		if row.get("kind")=="long_rest": fact=row
	check(fact.get("consumed")==1 and not state.player_actor_id in state.facts[state.estates["estate.riverside"].last_production_id].fed,"登记居民的庄主只吃一份真实私粮")
	roundtrip(k,"resident owner")

## 仅运行有限休整命令、首错和回滚探针；不运行成年成长或完整聚合。
func run_checks() -> void:
	if not coverage(): finish(); return
	if not legacy_path.is_empty():
		legacy_script=load(legacy_path)
		if not check(legacy_script!=null,"精确旧Kernel源码可加载"): finish(); return
	completion(); cancellation(); feeding_edges(); finish()

## 清理持有测试闭包的注册表，输出独立旧源对照次数及真实失败总数。
func finish() -> void:
	for k in kernels: k._registry={}
	kernels.clear(); legacy_script=null
	print("REGISTERED_REST_LEGACY_COMPARISONS=",comparisons)
	print("REGISTERED_REST_CHECKS=",checks," FAILURES=",failures)
	quit(1 if failures else 0)
