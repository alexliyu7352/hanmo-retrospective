## 正式Kernel观察事务的故障矩阵；直接调用生产入口并对照接入前冻结的完整旧轨迹。
extends SceneTree
const Kernel=preload("res://core/world_kernel.gd")
const Codec=preload("res://core/save_codec.gd")
const Registry=preload("res://core/runtime/module_registry.gd")
const Builtin=preload("res://core/runtime/builtin_modules.gd")
const EVENT_KEY: String="actor.player|event.letter"
var checks: int=0
var failures: int=0
var Fixture
var Adapter
var ObservedKernel
var fault_mode: String=""
var hits: int=0
var observed_candidate: Dictionary={}
var observed_contexts: Array=[]

## 等引擎就绪后动态探测新增合同，缺模块用断言红灯定位。
func _init() -> void:
	call_deferred("_run")

## 局部正式测试每个实际条件单独计数，并保留非零失败退出。
func check(value: bool, label: String) -> bool:
	checks+=1
	if not value: failures+=1; printerr("FAIL: ",label)
	return value

## 每个注册观察器声明同一旧业务清理之后，不借名称顺序决定执行时机。
func observer(id: String, callback: Callable, after: Array=[]) -> Dictionary:
	return {"id":id,"owner":"interaction","before":[],"after":[Adapter.ID] if after.is_empty() else after,"callback":callback}

## 同一个正式命令信封供旧Kernel和正式测试Kernel使用，所有序号来自各自已提交快照。
func command(k, kind: String, params: Dictionary={}, actor_id: String="") -> Dictionary:
	var state: Dictionary=k.snapshot()
	if actor_id.is_empty(): actor_id=state.player_actor_id
	return {"kind":kind,"params":params,"actor_id":actor_id,"request_seq":state.request_sequences[actor_id]+1,"expected_revision":state.revision}

## 创建相同真实作者世界，附加回调仅限正式测试实例，不修改生产注册根。
func fresh(observed: bool=true, extras: Array=[]):
	var k=ObservedKernel.new() if observed else Kernel.new()
	if observed: k.observer_additions=extras
	if not check(k.new_game().ok,"真实two_stories启动"): return null
	return k

## 下游故障只能发生于真实结果/history与Events之后；记录候选以核对失败前已做的工作。
func tail_observer(candidate: Dictionary, _data: Dictionary, context: Dictionary) -> Dictionary:
	hits+=1
	observed_candidate=candidate
	observed_contexts.append(context.duplicate(true))
	if fault_mode=="observer": return {"ok":false,"code":"InjectedObserverFailure"}
	if fault_mode=="schema": candidate.wallets["wallet.player"].balance=-1
	return {"ok":true,"effects":[{"source_command_key":context.source_command_key,"result_id":candidate.results[context.source_command_key].result_id}]}

## 链预算探针不消费资源，只证明上限覆盖真实尾部已有Events观察步骤。
func count_observer(_candidate: Dictionary, _data: Dictionary, _context: Dictionary) -> Dictionary:
	hits+=1
	return {"ok":true}

## 冻结真实事件观察器和唯一owner，缺失或错误回调不能默认为合法无效果配置。
func test_business_registration() -> void:
	var described: Array=Adapter.descriptors(); var before: Array=described.duplicate(true)
	check(described.size()==1 and described[0].id=="interaction.events_reconcile" and described[0].owner=="interaction","真实事件业务由interaction唯一拥有")
	var source: Array=Fixture.describe(Builtin.descriptors(),[],"valid")
	var registered: Dictionary=Registry.build(source)
	check(registered.ok and Fixture.validate_registry(registered).ok,"发行描述加观察适配器可完整注册")
	check(described==before,"注册不得调用或改写业务描述")
	for mode in ["missing","wrong_callback"]:
		var bad: Dictionary=Registry.build(Fixture.describe(Builtin.descriptors(),[],mode))
		check(bad.ok and Fixture.validate_registry(bad).get("code")=="MissingEventObserver","必需业务绑定拒绝 "+mode)

## 其它合法观察器不能用before指向必需Events而抢先运行；启动失败必须保留旧世界。
func test_event_observer_must_be_first() -> void:
	var early: Dictionary={"id":"test.before_events","owner":"interaction","before":[Adapter.ID],"after":[],"callback":count_observer}
	var registered: Dictionary=Registry.build(Fixture.describe(Builtin.descriptors(),[early],"valid"))
	check(registered.ok and Fixture.validate_registry(registered).get("code")=="InvalidEventObserverOrder","合法总序也不能将额外观察器放到Events之前")
	var observed=ObservedKernel.new(); observed.observer_additions=[early]; hits=0
	var started: Dictionary=observed.new_game()
	check(started.get("code")=="InvalidEventObserverOrder" and observed.snapshot().is_empty() and observed.content().is_empty() and observed._registry.is_empty(),"违规链首在空世界启动前被拒绝")
	if started.ok: observed.submit(command(observed,"end_interaction"))
	check(hits==0,"违规前置观察器从未在提交中执行")
	observed.observer_additions=[]
	if not check(observed.new_game().ok,"恢复合法链首可启动"): return
	var before: Dictionary=observed.snapshot(); var content: Dictionary=observed.content(); var accepted: Dictionary=observed._registry.duplicate(true)
	observed.observer_additions=[early]
	check(observed.new_game().get("code")=="InvalidEventObserverOrder" and observed.snapshot()==before and observed.content()==content and observed._registry==accepted,"旧世界重配违规链首不会覆盖已接受的状态或注册")

## 与接入前f034真实Kernel冻结摘要比整个结果/状态；不以两个新包装器互比冒充旧轨迹。
func test_representative_parity() -> void:
	var reference: Variant=JSON.parse_string(FileAccess.get_file_as_string("res://tests/fixtures/observer_tail_f034.json"))
	if not check(reference is Dictionary and reference.get("source_commit")=="f034c35fe0f98d77cc89271aad7b0a2af8f5e8fa" and reference.get("rows",[]).size()==9,"接入前完整旧轨迹来源明确且九步齐全"): return
	var normal=fresh(false); var observed=fresh()
	if normal==null or observed==null: return
	var commands: Array=[["move_local",{"place_id":"west.tavern"}],["end_interaction",{}],["move_local",{"place_id":"west.street"}],["move_local",{"place_id":"west.grain"}],["trade",trade_params()],["advance",{}],["move_local",{"place_id":"west.street"}],["wait",{}],["advance",{}]]
	for index in range(commands.size()):
		var row: Array=commands[index]; var old: Dictionary=reference.rows[index]
		var normal_result: Dictionary=normal.submit(command(normal,row[0],row[1]))
		var actual: Dictionary=observed.submit(command(observed,row[0],row[1]))
		check(actual.ok and actual==normal_result and old.kind==row[0] and JSON.stringify(actual,"",true,true).sha256_text()==old.result_sha256,"正式结果与旧基线逐字段相同 "+row[0])
		check(observed.snapshot()==normal.snapshot() and JSON.stringify(observed.snapshot(),"",true,true).sha256_text()==old.state_sha256,"完整状态/ID/RNG/事实与旧基线相同 "+row[0])
	check(observed.snapshot().events[EVENT_KEY].status=="cancelled","链首执行了真实离场事件取消业务")

## 同一原事件的九种语义失效都走真实restore与submit，效果/冷却不因包装而改变。
func test_event_cancellation_matrix() -> void:
	for reason in ["sender_moved","sender_dead","sender_unavailable","role_disabled","role_changed","player_moved","player_dead","player_unavailable","condition"]:
		var old=fresh(false); var observed=fresh()
		if old==null or observed==null: return
		check(old.submit(command(old,"move_local",{"place_id":"west.tavern"})).ok,"真实入场排队 "+reason)
		var state: Dictionary=old.snapshot(); var player: String=state.player_actor_id
		if reason=="sender_moved": state.actors["actor.shen"].location.place_id="west.street"
		if reason=="sender_dead": state.actors["actor.shen"].alive=false
		if reason=="sender_unavailable": state.actors["actor.shen"].available=false
		if reason=="role_disabled": state.roles["role.sender"].enabled=false
		if reason=="role_changed":
			state.roles["role.sender"].actor_id="actor.zheng"
			state.actors["actor.zheng"].location=state.actors["actor.shen"].location.duplicate(true)
		if reason=="player_moved": state.actors[player].location.place_id="west.street"
		if reason=="player_dead": state.actors[player].alive=false
		if reason=="player_unavailable": state.actors[player].available=false
		if reason=="condition": state.knowledge[player]["offer.letter"]={"source":{"kind":"actor","id":"actor.shen"},"tick":state.tick}
		if not check(old.restore(state).ok and observed.restore(state).ok,"两路恢复同一真实合法夹具 "+reason): continue
		var who: String="actor.chen" if reason in ["player_dead","player_unavailable"] else player
		var request: Dictionary=command(old,"end_interaction",{},who)
		var expected: Dictionary=old.submit(request); var actual: Dictionary=observed.submit(request)
		check(expected.ok and actual==expected and observed.snapshot()==old.snapshot(),"取消结果/全状态等价 "+reason)
		check(observed.snapshot().events[EVENT_KEY].status=="cancelled","真实失效被观察器取消 "+reason)

## 用真实粮铺买入完成覆盖库存转移后两种尾部失败，不用伪造余额或奖励来冒充交易。
func trade_params() -> Dictionary:
	return {"shop_id":"shop.grain","item_id":"item.grain","side":"buy","quantity":2,"payer_wallet_id":"wallet.player","context":{"place_id":"west.grain","provider_actor_id":"actor.zheng","role_id":"role.merchant"}}

## 真实trade在start转移资产，advance才完成技能与占用；两个阶段各注入观察或Schema失败。
func test_trade_tail_failure_matrix() -> void:
	for operation in ["trade","advance"]:
		for mode in ["observer","schema"]:
			var label: String=operation+"/"+mode
			fault_mode=""; hits=0; observed_contexts=[]
			var observed=fresh(true,[observer("test.tail",tail_observer)])
			if observed==null: return
			check(observed.submit(command(observed,"move_local",{"place_id":"west.grain"})).ok,"进入真实粮铺 "+label)
			if operation=="advance": check(observed.submit(command(observed,"trade",trade_params())).ok,"正式trade开始已转移资产 "+label)
			var before: Dictionary=observed.snapshot(); var request: Dictionary=command(observed,operation,trade_params() if operation=="trade" else {})
			fault_mode=mode; hits=0; observed_contexts=[]
			var rejected: Dictionary=observed.submit(request)
			check(not rejected.ok and (rejected.get("code")=="InjectedObserverFailure" if mode=="observer" else true),"真实尾部失败 "+label)
			check(observed.snapshot()==before,"失败全快照含RNG/预留/事实/序号回滚 "+label)
			if operation=="trade":
				check(observed_candidate.inventories!=before.inventories and observed_candidate.tick==before.tick,"观察之前真实库存已转移，start不加tick "+label)
			else:
				check(observed_candidate.inventories==before.inventories and observed_candidate.tick==before.tick+1 and observed_candidate.actions!=before.actions,"完成阶段真实推进/释放，绝不重复转物 "+label)
			var source: String=request.actor_id+":"+str(request.request_seq)
			check(observed_candidate.results.has(source) and observed_candidate.history[-1].result_id==observed_candidate.results[source].result_id,"result/history已先于观察存在 "+label)
			check(hits==1 and observed_contexts[0].source_command_key==source and observed_contexts[0].correlation_id=="command:"+source,"整个命令唯一稳定上下文 "+label)
			fault_mode=""; var result: Dictionary=observed.submit(request); var after: Dictionary=observed.snapshot()
			check(result.ok and after!=before and hits==2,"原请求在新候选正常重试 "+label)
			check(observed.submit(request)==result and observed.snapshot()==after and hits==2,"重复请求复用原回执、不重跑观察或资产结算 "+label)
			var saved: String=Codec.encode(after,0,"observer-integration",observed.content())
			var decoded: Dictionary=Codec.decode(saved,observed.content())
			check(decoded.get("ok",false),"原存档合同可编解码观察后的状态 "+label)
			var resumed=fresh(true,[observer("test.tail",tail_observer)])
			if resumed!=null and decoded.get("ok",false):
				check(resumed.restore(decoded.state).ok,"恢复已提交交易 "+label)
				var prior_hits: int=hits
				check(resumed.submit(request)==result and resumed.snapshot()==after and hits==prior_hits,"存读后重复源命令仍不再观察/结算 "+label)

## 事件取消已经发生后再失败也要回滚；关闭临时效果消费者不会撤销规则世界。
func test_event_tail_rollback_and_notifications() -> void:
	fault_mode=""; hits=0
	var observed=fresh(true,[observer("test.tail",tail_observer)])
	if observed==null: return
	check(observed.submit(command(observed,"move_local",{"place_id":"west.tavern"})).ok,"尾部回滚先真实入场")
	var before: Dictionary=observed.snapshot(); before.actors["actor.shen"].available=false
	check(observed.restore(before).ok,"合法失效夹具保留queued")
	var request: Dictionary=command(observed,"end_interaction")
	fault_mode="observer"
	check(not observed.submit(request).ok and observed.snapshot()==before,"取消后的观察失败完整回滚")
	check(observed_candidate.events[EVENT_KEY].status=="cancelled","后继观察确实在Events真实业务之后")
	fault_mode=""; var result: Dictionary=observed.submit(request); var after: Dictionary=observed.snapshot()
	check(result.ok and after.events[EVENT_KEY].status=="cancelled","无通知消费者也完成规则取消")
	check(not result.has("effects") and not after.has("transaction_context"),"未提交临时通知/context不泄入结果缓存或保存真值")

## 一份context预算包含Events链首；总64允许，总65必须丢候选且不得保存部分结果。
func test_kernel_step_limit() -> void:
	for total in [64,65]:
		var entries: Array=[]
		for index in range(total-1):
			entries.append(observer("test.bound."+str(index),count_observer,[Adapter.ID] if index==0 else ["test.bound."+str(index-1)]))
		var observed=fresh(true,entries)
		if observed==null: return
		var before: Dictionary=observed.snapshot(); hits=0
		var result: Dictionary=observed.submit(command(observed,"end_interaction"))
		if total==64:
			check(result.ok and hits==63,"含真实Events在内64步允许")
		else:
			check(result.get("code")=="ObserverStepLimit" and hits==63 and observed.snapshot()==before,"65步失败、最后回调未运行、全候选不提交")

## 坏启动优先于内容读入且保留旧世界和旧注册，预览/过期命令不调用尾部。
func test_startup_and_preflight() -> void:
	var observed=fresh(true,[observer("test.tail",tail_observer)])
	if observed==null: return
	var before: Dictionary=observed.snapshot(); var content: Dictionary=observed.content(); var accepted: Dictionary=observed._registry.duplicate(true)
	for mode in ["missing","unknown_dependency","wrong_callback"]:
		observed.observer_mode=mode
		observed.observer_additions=[] if mode=="missing" else [observer("test.tail",tail_observer)]
		var result: Dictionary=observed.new_game("missing","user://observer-nonexistent.hmcatalog.json")
		var code: String="UnknownObserverDependency" if mode=="unknown_dependency" else "MissingEventObserver"
		check(result.get("code")==code and observed.snapshot()==before and observed.content()==content and observed._registry==accepted,"坏图/必需绑定在新世界前失败且保留旧局 "+mode)
	observed.observer_mode="valid"; hits=0
	for _index in range(100): observed.preview("end_interaction")
	var stale: Dictionary=command(observed,"end_interaction"); stale.expected_revision-=1
	check(observed.submit(stale).get("code")=="StalePreview" and hits==0 and observed.snapshot()==before,"100次预览和旧版本不建观察效果/不改RNG")

## 全部矩阵直接使用正式Kernel，测试子类仅注入受信描述，不生成第二套提交实现。
func _run() -> void:
	var adapter_path: String="res://core/domains/interaction/interaction_observers.gd"
	var fixture_path: String="res://tests/observer_integration_fixture.gd"
	if check(ResourceLoader.exists(adapter_path) and ResourceLoader.exists(fixture_path),"真实业务适配器与正式Kernel测试子类存在"):
		Adapter=load(adapter_path); Fixture=load(fixture_path); ObservedKernel=Fixture
		var built: Dictionary=Registry.build(Builtin.descriptors())
		if check(built.get("ok",false) and Builtin.observer_contract_status(built).ok,"生产注册根必须实际绑定唯一事件观察入口"):
			print("PRODUCTION_KERNEL_SHA256=",FileAccess.get_sha256("res://core/world_kernel.gd"))
			test_business_registration()
			test_event_observer_must_be_first()
			test_representative_parity()
			test_event_cancellation_matrix()
			test_trade_tail_failure_matrix()
			test_event_tail_rollback_and_notifications()
			test_kernel_step_limit()
			test_startup_and_preflight()
	print("OBSERVER_INTEGRATION_CHECKS=",checks," FAILURES=",failures)
	quit(0 if failures==0 else 1)
