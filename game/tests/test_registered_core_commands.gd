## 核心三命令短对照：完整旧源状态、回执、Codec、支付与推进回滚，禁止临时控制请求冒充完成。
extends SceneTree
const Kernel=preload("res://core/world_kernel.gd")
const Builtin=preload("res://core/runtime/builtin_modules.gd")
const Registry=preload("res://core/runtime/module_registry.gd")
const RuntimeCommands=preload("res://core/runtime/runtime_commands.gd")
const ServiceCommands=preload("res://core/domains/services/service_commands.gd")
const Codec=preload("res://core/save_codec.gd")
const Interaction=preload("res://core/interaction_rules.gd")
var checks: int=0
var failures: int=0
var comparisons: int=0
var legacy_path: String=""
var legacy=null
var retained: Array=[]

## 生产调用上附加计数与可恢复错误，不替换任何成功领域结果。
class TestKernel extends "res://core/world_kernel.gd":
	var hits: Dictionary={}
	var override_control: bool=false
	var control_result: Dictionary={}
	var fail_advance: bool=false
	var fail_tail: bool=false
	var fail_begin: bool=false
	var advanced_tick: int=-1
	var last_hold: Array=[]
	var mutation: String=""
	var selected: String=""
	## 注册探针只包装原回调；错误描述通过真实启动合同拒绝。
	func _module_descriptors() -> Array:
		var rows: Array=super._module_descriptors()
		for descriptor in rows:
			for entry in descriptor.commands.duplicate():
				if not entry.kind in ["advance","wait","service"]: continue
				var kind: String=entry.kind; var original: Callable=entry.preview
				entry.preview=func(state: Dictionary,data: Dictionary,actor: String,command: String,params: Dictionary) -> Dictionary:
					hits[kind+":preview"]=hits.get(kind+":preview",0)+1
					return original.call(state,data,actor,command,params)
				if kind=="advance":
					var apply_original: Callable=entry.apply
					entry.apply=func(state: Dictionary,data: Dictionary,actor: String,command: String,params: Dictionary,offer: Dictionary) -> Dictionary:
						hits["advance:apply"]=hits.get("advance:apply",0)+1
						return control_result.duplicate(true) if override_control else apply_original.call(state,data,actor,command,params,offer)
				if kind!=selected: continue
				match mutation:
					"missing": descriptor.commands.erase(entry)
					"duplicate": descriptor.commands.append(entry.duplicate(true))
					"gate": entry.gate_id="wrong_core_gate"
					"bridge": entry.erase("compat_action")
					"fake_apply": entry.apply=RuntimeCommands.request_advance
		return rows
	## 真实开工已分配ID、占用与付款之后注入失败，检查候选事务是否完整放弃。
	func _begin_action(state: Dictionary,actor: String,kind: String,params: Dictionary,offer: Dictionary) -> Dictionary:
		var result: Dictionary=super._begin_action(state,actor,kind,params,offer)
		if kind in ["wait","service"]:
			hits[kind+":begin"]=hits.get(kind+":begin",0)+1
			if result.ok and fail_begin: return {"ok":false,"code":"InjectedCoreBegin"}
		return result
	## 原完整推进后注入硬失败或尾验破坏，记录hold证明未重建推进管线。
	func _advance(state: Dictionary,hold: Array=[]) -> Dictionary:
		last_hold=hold.duplicate(); hits["advance:world"]=hits.get("advance:world",0)+1
		var result: Dictionary=super._advance(state,hold)
		advanced_tick=state.tick
		if result.ok and fail_advance: return {"ok":false,"code":"InjectedCoreAdvance"}
		if result.ok and fail_tail: state.wallets[state.actors[state.player_actor_id].wallet_id].balance=-1
		return result

## 旧Kernel路径由固定SHA导出的测试执行器提供；缺失则失败而非悄悄跳过对照。
func _init() -> void:
	for arg in OS.get_cmdline_user_args():
		if arg.begins_with("--legacy-kernel="): legacy_path=arg.trim_prefix("--legacy-kernel=")
	call_deferred("run_checks")

## 每个边界保留明确失败与非零退出，不以打印数量替代实际断言。
func check(value: bool,label: String) -> bool:
	checks+=1
	if not value: failures+=1; printerr("FAIL: ",label)
	return value

## 创建正常作者内容世界；测试只通过公开恢复安装合法状态。
func fresh(scenario: String="lived_city"):
	var k=TestKernel.new(); retained.append(k)
	check(k.new_game("scenario."+scenario,"res://data/generated/"+scenario+".hmcatalog.json").ok,"创建真实世界 "+scenario)
	return k

## 同一演员序号和版本进入正式事务，不绕开重复请求缓存。
func request(k,kind: String,params: Dictionary={},actor: String="") -> Dictionary:
	var state: Dictionary=k.snapshot()
	if actor.is_empty(): actor=state.player_actor_id
	return {"kind":kind,"params":params,"actor_id":actor,"request_seq":state.request_sequences[actor]+1,"expected_revision":state.revision}

## 逐条比较完整状态和回执，包括随机数、ID、历史、占用与缓存，不删除可变字段。
func send(k,kind: String,params: Dictionary={},actor: String="") -> bool:
	var before: Dictionary=k.snapshot(); var command: Dictionary=request(k,kind,params,actor)
	if not check(legacy.restore(before).ok,"旧源码恢复 "+kind): return false
	var old_result: Dictionary=legacy.submit(command); var result: Dictionary=k.submit(command)
	if not check(result.ok,"真实命令 "+kind+" "+str(result)): return false
	var after: Dictionary=k.snapshot(); var hits: Dictionary=k.hits.duplicate(true)
	check(old_result==result and legacy.snapshot()==after,"完整旧源state/result相等 "+kind); comparisons+=1
	check(k.submit(command)==result and k.snapshot()==after and k.hits==hits,"重复请求不重入不推进 "+kind)
	var encoded: String=Codec.encode(after,0,"registered-core",k.content())
	var decoded: Dictionary=Codec.decode(encoded,k.content())
	check(decoded.ok and k.restore(decoded.state).ok and k.snapshot()==after,"完整Codec往返 "+kind)
	return true

## 检验唯一集合和三命令实注册；新旧行动不能用假apply或未知桥相互降级。
func contracts() -> void:
	var registry: Dictionary=Registry.build(Builtin.descriptors())
	if not check(registry.ok,"真实Registry可构造"): return
	check(RuntimeCommands.KINDS==["advance","wait"] and ServiceCommands.KINDS==["service"],"唯一核心与服务集合")
	for kind in ["advance","wait","service"]:
		check(registry.commands.has(kind),"真正注册 "+kind)
		if kind!="advance": check(registry.commands[kind].apply==null and registry.commands[kind].compat_action=="legacy_begin_action" and not registry.actions.has(kind),"唯一旧行动桥 "+kind)
		for mutation in ["missing","duplicate","gate"]:
			var k=fresh(); var before: Dictionary=k.snapshot(); k.selected=kind; k.mutation=mutation
			check(not k.new_game().ok and k.snapshot()==before,"注册拒绝保留世界 "+kind+" "+mutation)
	for kind in ["wait","service"]:
		for mutation in ["bridge","fake_apply"]:
			var k=fresh(); k.selected=kind; k.mutation=mutation
			check(not k.new_game().ok,"兼容桥错误拒绝 "+kind+" "+mutation)

## 等待与服务从原开工到半日完工逐步比对，保留收费时点、提供者占用与工资来源。
func actions() -> void:
	var k=fresh()
	check(k.preview("advance").get("code")=="NoCurrentAction","无行动推进拒绝")
	var before: Dictionary=k.snapshot(); k.fail_begin=true
	check(k.submit(request(k,"wait")).get("code")=="InjectedCoreBegin" and k.snapshot()==before,"wait开工后失败回滚ID与占用")
	k.fail_begin=false
	if not send(k,"wait",{"note":"opaque-old-params"}): return
	check(k.hits.get("wait:begin")==2 and k.hits.get("wait:preview")==2,"wait真实注册与唯一开工")
	if not send(k,"advance"): return
	check(k.snapshot().tick==before.tick+1 and k.hits.get("advance:world")==1,"推进一次且实际消费控制请求")
	if not send(k,"move_local",{"place_id":"west.inn"}): return
	var context: Dictionary=Interaction.for_role(k.snapshot(),k.content().data,"role.inn")
	var params: Dictionary={"activity":"rest","context":context}; before=k.snapshot(); k.fail_begin=true
	check(k.submit(request(k,"service",params)).get("code")=="InjectedCoreBegin" and k.snapshot()==before,"service真实收费后失败回滚全部状态")
	k.fail_begin=false
	if not send(k,"service",params): return
	var state: Dictionary=k.snapshot(); var actor: String=state.player_actor_id; var provider: String=context.provider_actor_id
	check(state.wallets[state.actors[actor].wallet_id].balance==before.wallets[before.actors[actor].wallet_id].balance-20 and state.actors[provider].action_id==state.actors[actor].action_id,"费用即时支付并占用真实提供者")
	if not send(k,"advance"): return
	check(k.snapshot().actors[provider].action_id.is_empty(),"服务完成释放提供者")
	if not send(k,"move_local",{"place_id":"west.street"}) or not send(k,"move_local",{"place_id":"west.tavern"}): return
	params={"activity":"work","context":Interaction.for_role(k.snapshot(),k.content().data,"role.host")}; before=k.snapshot()
	if not send(k,"service",params) or not send(k,"advance"): return
	state=k.snapshot()
	check(state.wallets["wallet.host"].balance==before.wallets["wallet.host"].balance-80,"真实雇主支付工资一次")

## 畸形控制请求、假空成功及其他owner均不能推进或提交任何状态。
func controls_and_rollback() -> void:
	var k=fresh()
	if not send(k,"wait"): return
	var before: Dictionary=k.snapshot(); k.override_control=true
	for result in [{"ok":true},{"ok":true,"control":{}},{"ok":true,"control":{"advance_world":false}},{"ok":true,"control":{"advance_world":1}},{"ok":1,"control":{"advance_world":true}},{"ok":true,"control":{"advance_world":true,"script":"forbidden-control-metadata"}},{"ok":true,"control":{"advance_world":true},"details":{}},{"ok":true,"control":"advance_world"}]:
		k.control_result=result
		check(k.submit(request(k,"advance")).get("code")=="InvalidCommandControl" and k.snapshot()==before and k.hits.get("advance:world",0)==0,"畸形control不可提交 "+str(result))
	k.override_control=false
	var registered: Dictionary=k._registry.commands.advance
	for field in ["owner","gate_id"]:
		var original: String=registered[field]; registered[field]="foreign"
		var candidate: Dictionary=before.duplicate(true)
		check(k._apply(candidate,"advance",{},before.player_actor_id,{"ok":true}).get("code")=="InvalidCommandControl" and candidate==before,"control锁拒绝 "+field)
		registered[field]=original
	k._registry.commands["foreign_advance"]=registered
	var candidate: Dictionary=before.duplicate(true)
	check(not k._apply(candidate,"foreign_advance",{},before.player_actor_id,{"ok":true}).ok and candidate==before,"不同kind不能借advance描述")
	k._registry.commands.erase("foreign_advance")
	k.fail_advance=true
	check(k.submit(request(k,"advance")).get("code")=="InjectedCoreAdvance" and k.advanced_tick==before.tick+1 and k.snapshot()==before,"真实推进后硬失败回滚tick/RNG/ID/回执")
	k.fail_advance=false; k.fail_tail=true
	check(not k.submit(request(k,"advance")).ok and k.advanced_tick==before.tick+1 and k.snapshot()==before,"推进后Schema尾验失败回滚全部状态")
	k.fail_tail=false
	check(send(k,"advance"),"失败后同序号能正常推进")

## 首错比较使用隔离预览快照；故意缺失状态只定位守卫，不伪装成合法存档。
func guard_order() -> void:
	var k=fresh(); var base: Dictionary=k.snapshot(); var actor: String=base.player_actor_id
	check(legacy.restore(base).ok,"守卫旧源同步")
	for kind in ["advance","wait","service"]:
		for variant in ["base","unavailable","busy","following","army","meeting","battle","military_and_siege","siege"]:
			var state: Dictionary=base.duplicate(true)
			if variant=="unavailable": state.actors[actor].alive=false; state.actors[actor].available=false
			if variant=="busy": state.actors[actor].action_id="missing"
			if variant=="following": state.actors[actor].location={"kind":"Following","leader_actor_id":"actor.chen"}
			if variant=="army": state.actors[actor].location={"kind":"InArmy","army_id":"missing"}
			if variant=="meeting": state.encounters["test"]={"status":"pending","participants":[actor]}
			if variant=="battle": state.battles["test"]={"kind":"personal","phase":"planning"}
			if variant=="military_and_siege": state.battles["test"]={"kind":"military","phase":"planning","created_tick":0,"last_tick":-1}
			if variant in ["military_and_siege","siege"]: state.sieges["test"]={"phase":"active","last_tick":-1}
			var before: Dictionary=state.duplicate(true); var result: Dictionary=k._preview(state,kind,{},actor)
			check(result==legacy._preview(state,kind,{},actor) and state==before,"首错与预览纯只读 "+kind+" "+variant)
			if kind=="advance" and variant=="military_and_siege": check(result.get("code")=="MilitarySkippedStep","军战先于围城拒绝")
			if kind=="advance" and variant=="siege": check(result.get("code")=="SiegeSkippedStep","围城先于无行动拒绝")
		check(k._preview(base,kind,{},"missing").get("code")=="UnknownActor","未知actor优先 "+kind)

## 两位真实旅人并行推进，单人hold只暂停本人；行旅拒绝保留首错与所有世界随机数。
func held_travel() -> void:
	var k=fresh("two_stories")
	check(legacy.new_game("scenario.two_stories","res://data/generated/two_stories.hmcatalog.json").ok,"旧源码切换原双故事内容")
	var state: Dictionary=k.snapshot(); var actor: String=state.player_actor_id
	state.wallets[state.actors["actor.he"].wallet_id].balance=80
	check(k.restore(state).ok,"合法NPC旅费快照")
	if not send(k,"move_local",{"place_id":"east.entrance"},"actor.he"): return
	if not send(k,"travel",{"destination":"village.east"}) or not send(k,"travel",{"destination":"city.west"},"actor.he"): return
	var funded: Dictionary=k.snapshot(); state=funded.duplicate(true)
	state.wallets[state.actors[actor].wallet_id].balance=0
	check(k.restore(state).ok and legacy.restore(state).ok,"真实旅途缺旅费状态合法")
	var before: Dictionary=k.snapshot(); var command: Dictionary=request(k,"advance")
	var result: Dictionary=k.submit(command)
	check(result.get("code")=="Stranded" and result==legacy.submit(command) and k.snapshot()==before and legacy.snapshot()==before,"旅费拒绝不推进不消耗RNG且旧首错相同")
	check(k.restore(funded).ok,"恢复真实原旅费")
	if not send(k,"advance") or not send(k,"pause_travel"): return
	before=k.snapshot(); command=request(k,"advance")
	check(legacy.restore(before).ok,"暂停旅行旧源同步")
	result=k.submit(command)
	check(result.get("code")=="TravelPaused" and result==legacy.submit(command) and k.snapshot()==before,"暂停行旅拒绝完整回滚")
	if not send(k,"travel_wait"): return
	state=k.snapshot()
	check(k.last_hold==[actor] and state.actors[actor].location==before.actors[actor].location and state.tick==before.tick+1,"hold演员保位置且仅前进一步")
	check(state.actors["actor.he"].location.get("settlement_id")=="city.west","held主角不阻碍另一真实旅人到达")

## 运行所有局部门；旧源码缺失不计为通过，不运行长期成长或全量发行门。
func run_checks() -> void:
	if not check(not legacy_path.is_empty() and ResourceLoader.exists(legacy_path),"固定旧源码夹具存在"): finish(); return
	legacy=load(legacy_path).new(); retained.append(legacy)
	if not check(legacy.new_game("scenario.lived_city","res://data/generated/lived_city.hmcatalog.json").ok,"原源码真实开局"): finish(); return
	contracts(); actions(); controls_and_rollback(); guard_order(); held_travel(); finish()

## 结束前断开测试Callable捕获，输出可机器复核的检查与完整对照数量。
func finish() -> void:
	for k in retained: k._registry={}
	retained.clear(); legacy=null
	print("REGISTERED_CORE_COMMANDS_CHECKS=",checks," COMPARED_COMMANDS=",comparisons," FAILURES=",failures)
	quit(0 if failures==0 else 1)
