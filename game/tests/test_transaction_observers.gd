## 验证临时事务身份与受信观察合同；这里不冒充尚未接入Kernel的正式尾部。
extends SceneTree
var checks: int=0
var failures: int=0
var Context
var Observers
const Inventory=preload("res://core/inventory_rules.gd")
var calls: Array=[]
var payload_result: Variant={}

## 延后加载新合同，缺实现时产生明确断言失败而非解析失败。
func _init() -> void:
	call_deferred("_run")

## 每个实际条件独立计数，失败使进程返回非零。
func check(value: bool, label: String) -> void:
	checks+=1
	if not value:
		failures+=1
		printerr("FAIL: ",label)

## 夹具只包含上下文会读取的真实字段和不得变化的随机/编号快照。
func state_fixture() -> Dictionary:
	return {"player_actor_id":"actor.player","actors":{"actor.player":{},"actor.npc":{}},"revision":7,"tick":13,"rng":{"world":"fixed","event":"untouched"},"next_instance_id":91,"request_sequences":{"actor.player":2,"actor.npc":0},"results":{}}

## 使用正式命令信封，不让序号或版本从隐式全局状态推算。
func command_fixture() -> Dictionary:
	return {"kind":"wait","params":{},"request_seq":3,"expected_revision":7}

## 同一来源的correlation与创建顺序无关，所有输入只读且不共享可变引用。
func test_context_identity() -> void:
	var state: Dictionary=state_fixture(); var command: Dictionary=command_fixture()
	var before: Dictionary=state.duplicate(true); var command_before: Dictionary=command.duplicate(true)
	var first: Dictionary=Context.create(state,command)
	check(first.get("ok",false),"上下文创建成功")
	if not first.get("ok",false): return
	check(first.source_command_key=="actor.player:3","来源命令与Kernel去重键相同")
	check(first.correlation_id=="command:actor.player:3","correlation仅由来源派生")
	check(first.effects.is_empty() and first.fact_refs.is_empty(),"不把历史转换成新事实")
	check(first.from_revision==7 and first.from_tick==13,"记录候选开始边界")
	check(Context.create(state,command)==first,"重复创建逐字段稳定")
	command.actor_id="actor.npc"
	var other: Dictionary=Context.create(state,command)
	check(other.source_command_key=="actor.npc:3" and other.correlation_id!=first.correlation_id,"不同人物来源不串联")
	command.erase("actor_id")
	check(state==before and command==command_before,"创建不改输入、RNG或实例号")
	var view: Dictionary=Context.snapshot(first)
	view.effects.append({"result_id":"result.existing"})
	check(first.effects.is_empty() and state==before,"查询副本不能回写context或世界")
	first.effects.append({"details":{"count":1}})
	view=Context.snapshot(first); view.effects[0].details.count=99
	check(first.effects[0].details.count==1,"查询深层隔离")

## 失败保持输入完整，拒绝隐式类型转换、未知人物和错误命令版本。
func test_context_rejections() -> void:
	var cases: Array=[{"request_seq":0},{"request_seq":3.0},{"request_seq":"3"},{"request_seq":2147483648},{"actor_id":"missing"},{"actor_id":1},{"expected_revision":6},{"kind":""},{"params":[]}]
	for change in cases:
		var state: Dictionary=state_fixture(); var before: Dictionary=state.duplicate(true)
		var command: Dictionary=command_fixture(); command.merge(change,true)
		var command_before: Dictionary=command.duplicate(true)
		var result: Dictionary=Context.create(state,command)
		check(not result.get("ok",false),"拒绝坏命令 "+str(change))
		check(state==before and command==command_before,"坏命令无副作用 "+str(change))

## 描述仅由受信代码创建，所有依赖显式写出，不接受资源路径或字符串回调。
func descriptor(id: String, callback: Callable, after: Array=[]) -> Dictionary:
	return {"id":id,"owner":"test.domain","before":[],"after":after,"callback":callback}

## 沿现有Registry的observers容器形状提供受信描述，键与描述身份一致。
func registry(entries: Array) -> Dictionary:
	var result: Dictionary={"ok":true,"observers":{}}
	for entry in entries: result.observers[entry.id]=entry
	return result

## 模拟一个真实领域派生计数；只通过返回effects将临时通知传给下游。
func record_observation(candidate: Dictionary, _data: Dictionary, context: Dictionary) -> Dictionary:
	calls.append(context.observer_id)
	candidate["observed_count"]=candidate.get("observed_count",0)+1
	return {"ok":true,"effects":[{"observer_id":context.observer_id,"prior_effects":context.effects.size()}]}

## 观察失败发生在调用者已有库存转移之后，不在此复制来源交易或尝试回滚。
func failed_observation(candidate: Dictionary, _data: Dictionary, _context: Dictionary) -> Dictionary:
	candidate["failed_tail_fact"]={"id":"fact.tail"}
	return {"ok":false,"code":"TailRefused","detail":"fixture failure"}

## 坏返回必须变为可诊断错误，而不是运行类型错误或隐式成功。
func invalid_observation(_candidate: Dictionary, _data: Dictionary, _context: Dictionary) -> Variant:
	return ["not a result"]

## 回调不能通过自己的上下文视图重置去重计数或更换来源。
func mutating_context_observation(candidate: Dictionary, _data: Dictionary, context: Dictionary) -> Dictionary:
	candidate["observed_count"]=candidate.get("observed_count",0)+1
	context.source_command_key="forged"; context.observer_steps=0; context.processed_observers.clear()
	return {"ok":true,"effects":[{"value":{"count":1}}]}

## 返回独立可变载荷以证明运行器复制引用，并逐类检查坏输出。
func payload_observation(_candidate: Dictionary, _data: Dictionary, _context: Dictionary) -> Variant:
	return payload_result

## 启动编译保持纯读，未知依赖、环、无明确顺序和非Callable都拒绝。
func test_observer_registration() -> void:
	calls.clear()
	var first: Dictionary=descriptor("first",record_observation)
	var second: Dictionary=descriptor("second",record_observation,["first"])
	var rows: Array=[first,second]; var before: Array=rows.duplicate(true)
	var compiled: Dictionary=Observers.compile([second,first])
	check(compiled.get("ok",false) and compiled.get("order")==["first","second"],"依赖决定顺序，反插入仍一致")
	check(rows==before and calls.is_empty(),"编译不调用观察器、不改描述")
	check(Observers.compile([]).get("ok",false),"无观察器是合法无效果合同")
	var unknown: Dictionary=second.duplicate(true); unknown.after=["missing"]
	check(Observers.compile([first,unknown]).get("code")=="UnknownObserverDependency","未知依赖拒绝")
	var cycle: Dictionary=first.duplicate(true); cycle.after=["second"]
	check(Observers.compile([cycle,second]).get("code")=="ObserverDependencyCycle","循环拒绝")
	check(Observers.compile([first,first]).get("code")=="DuplicateObserverId","重复ID拒绝")
	var unordered: Dictionary=second.duplicate(true); unordered.after=[]
	check(Observers.compile([first,unordered]).get("code")=="AmbiguousObserverOrder","不按字母序掩盖缺失依赖")
	# 故意非法的回调是测试数据而非工程资源依赖；构造后传入与原负例相同的路径字符串。
	var script_callback: Dictionary=first.duplicate(true); script_callback.callback="res"+"://"+"untrusted.gd"
	check(Observers.compile([script_callback]).get("code")=="InvalidObserverDescriptor","字符串脚本不能成为观察器")
	var bad_signature: Dictionary=first.duplicate(true); bad_signature.callback=state_fixture
	check(Observers.compile([bad_signature]).get("code")=="InvalidObserverDescriptor","签名错误启动拒绝")
	var before_only: Dictionary=first.duplicate(true); before_only.before=["second"]
	check(Observers.compile([unordered,before_only]).get("order")==["first","second"],"before与after使用同一显式图")

## 同来源同观察器只执行一次，派生效果按依赖可见，通知是否读取不改候选。
func test_observer_idempotence() -> void:
	calls.clear()
	var candidate: Dictionary=state_fixture(); var context: Dictionary=Context.create(candidate,command_fixture())
	var entries: Dictionary=registry([descriptor("second",record_observation,["first"]),descriptor("first",record_observation)])
	var result: Dictionary=Observers.reconcile(candidate,{},context,entries)
	check(result.get("ok",false) and calls==["first","second"],"按依赖运行两个观察器")
	check(candidate.get("observed_count")==2 and context.observer_steps==2,"派生计数是实际回调次数")
	check(context.effects==[{"observer_id":"first","prior_effects":0},{"observer_id":"second","prior_effects":1}],"下游看到此前派生的临时效果")
	var after: Dictionary=candidate.duplicate(true); var saved_context: Dictionary=context.duplicate(true)
	check(Observers.reconcile(candidate,{},context,entries).get("ok",false),"同一上下文可重入查询式核对")
	check(candidate==after and context==saved_context and calls.size()==2,"重复来源和观察器不重复效果")
	result.effects[0].prior_effects=999
	check(context.effects[0].prior_effects==0,"返回效果不与context共享引用")
	var view: Dictionary=Context.snapshot(context); view.effects.clear()
	check(candidate==after and context.effects.size()==2,"丢弃通知消费者不影响世界或其他消费者")

## 六十四次有界派生允许；第六十五次不执行并要求外层丢弃整个候选。
func test_observer_step_limit() -> void:
	for count in [64,65]:
		calls.clear()
		var entries: Array=[]
		for index in range(count):
			entries.append(descriptor("observer."+str(index),record_observation,[] if index==0 else ["observer."+str(index-1)]))
		entries.reverse()
		var committed: Dictionary=state_fixture(); var before: Dictionary=committed.duplicate(true)
		var candidate: Dictionary=committed.duplicate(true); var context: Dictionary=Context.create(committed,command_fixture())
		var result: Dictionary=Observers.reconcile(candidate,{},context,registry(entries))
		if count==64:
			check(result.get("ok",false) and calls.size()==64 and context.observer_steps==64,"64次派生全部成功")
		else:
			check(result.get("code")=="ObserverStepLimit" and calls.size()==64,"65次超限且不调用第65个")
			check(committed==before and result.get("discard_candidate",false),"超限完整外层快照不变，明确丢候选")
			check(Observers.reconcile(candidate,{},context,registry(entries)).get("code")=="ObserverContextFailed","失败context不可继续或重试部分候选")

## 用生产库存转移制造尾部失败；外层候选所有权保留RNG/编号/预留/事实/序号原值。
func test_candidate_failure_boundary() -> void:
	var committed: Dictionary=state_fixture()
	var owner: Dictionary={"kind":"actor","id":"actor.player"}
	committed.inventories={"source":{"revision":0,"lots":[{"id":"lot:90","item_id":"item.grain","quantity":3,"owner":owner,"reserved_by":"action:1"}]},"destination":{"revision":0,"lots":[]}}
	committed.wallets={"private":{"balance":10,"reserved":{"action:1":3}}}
	committed.facts={"prior":{"id":"fact.prior"}}
	var before: Dictionary=committed.duplicate(true); var candidate: Dictionary=committed.duplicate(true)
	var context: Dictionary=Context.create(committed,command_fixture())
	check(Inventory.transfer(candidate,"source","destination","item.grain",2,[owner],"action:1").ok,"正式库存接口已经转移实物")
	candidate.tick+=1; candidate.rng.world="advanced"; candidate.request_sequences["actor.player"]=3
	var result: Dictionary=Observers.reconcile(candidate,{},context,registry([descriptor("failure",failed_observation)]))
	check(result.get("code")=="TailRefused" and result.get("observer_id")=="failure","保留原领域失败码与观察器来源")
	check(candidate!=before and candidate.inventories.destination.lots.size()==1,"失败没有假装局部自动回滚")
	check(committed==before and result.get("discard_candidate",false),"提交者丢弃候选即完整保留状态")
	var fresh: Dictionary=Context.create(committed,command_fixture())
	check(fresh.source_command_key==context.source_command_key and fresh.observer_steps==0,"失败后从真实原状态可建新候选重试")

## 不合格输出封闭失败；视图篡改不能重置来源或成功去重。
func test_invalid_results_and_context_isolation() -> void:
	var candidate: Dictionary=state_fixture(); var context: Dictionary=Context.create(candidate,command_fixture())
	var result: Dictionary=Observers.reconcile(candidate,{},context,registry([descriptor("invalid",invalid_observation)]))
	check(result.get("code")=="InvalidObserverResult" and context.observer_status=="failed","非字典回调结果明确失败")
	context=Context.create(candidate,command_fixture())
	var entries: Dictionary=registry([descriptor("isolated",mutating_context_observation)])
	check(Observers.reconcile(candidate,{},context,entries).get("ok",false),"上下文视图修改不影响合法返回")
	check(context.source_command_key=="actor.player:3" and context.observer_steps==1,"来源和计数保持受控")
	check(Observers.reconcile(candidate,{},context,entries).get("ok",false) and candidate.observed_count==1,"回调清空自身视图不会重复执行")
	var malformed: Dictionary=Context.create(candidate,command_fixture()); malformed.correlation_id="forged"
	var before: Dictionary=candidate.duplicate(true)
	check(Observers.reconcile(candidate,{},malformed,entries).get("code")=="InvalidTransactionContext" and candidate==before,"坏context在回调前拒绝")

## 精确区分可序列化临时效果与坏返回；任何失败都封闭候选而非发布部分效果。
func test_payload_contract() -> void:
	var entries: Dictionary=registry([descriptor("payload",payload_observation)])
	var invalid: Array=[null,{}, {"ok":1},{"ok":false},{"ok":false,"code":1},{"ok":true,"effects":{}},{"ok":true,"effects":["bad"]},{"ok":true,"effects":[{"callback":payload_observation}]},{"ok":true,"effects":[{"number":INF}]},{"ok":true,"fact_refs":[{1:"bad key"}]}]
	for response in invalid:
		payload_result=response
		var candidate: Dictionary=state_fixture(); var context: Dictionary=Context.create(candidate,command_fixture())
		var result: Dictionary=Observers.reconcile(candidate,{},context,entries)
		check(result.get("code")=="InvalidObserverResult" and context.observer_status=="failed","坏载荷拒绝 "+str(response))
	payload_result={"ok":true,"effects":[{"nested":{"value":1},"nullable":null}],"fact_refs":[{"owner":"social","id":"event.existing"}]}
	var candidate: Dictionary=state_fixture(); var context: Dictionary=Context.create(candidate,command_fixture())
	var result: Dictionary=Observers.reconcile(candidate,{},context,entries)
	check(result.get("ok",false) and context.fact_refs==[{"owner":"social","id":"event.existing"}],"临时既有事实引用只转交，不创建事实")
	payload_result.effects[0].nested.value=2; payload_result.fact_refs[0].id="changed"
	check(context.effects[0].nested.value==1 and context.fact_refs[0].id=="event.existing","回调返回容器在接收边界复制")
	result.fact_refs[0].id="also changed"
	check(context.fact_refs[0].id=="event.existing" and not candidate.has("social"),"返回引用隔离且不构造持久事实库")

## 失败信封只保留可诊断普通字段，不复制未提交效果、对象或自引用字典。
func test_failure_envelope_is_bounded() -> void:
	var entries: Dictionary=registry([descriptor("payload",payload_observation)])
	var candidate: Dictionary=state_fixture(); var context: Dictionary=Context.create(candidate,command_fixture())
	payload_result={"ok":false,"code":"DomainRejected","detail":"original diagnostic","effects":[{"callback":payload_observation}],"fact_refs":[{"id":"uncommitted"}],"discard_candidate":false,"observer_id":"forged"}
	var result: Dictionary=Observers.reconcile(candidate,{},context,entries)
	check(result=={"ok":false,"code":"DomainRejected","detail":"original diagnostic","observer_id":"payload","discard_candidate":true},"失败不能携带Callable或未提交效果，保留原错误码与纯文本诊断")
	context=Context.create(candidate,command_fixture())
	var recursive: Dictionary={"ok":false,"code":"RecursiveFailure","detail":{"not":"text"}}
	recursive.cycle=recursive; payload_result=recursive
	result=Observers.reconcile(candidate,{},context,entries)
	check(result=={"ok":false,"code":"RecursiveFailure","observer_id":"payload","discard_candidate":true},"循环错误不复制且非文本detail不外传")
	recursive.erase("cycle"); payload_result={}

## 注册预检失败不进入回调或改候选，空注册是与关闭通知同样的无业务作用边界。
func test_registry_preflight() -> void:
	var candidate: Dictionary=state_fixture(); var before: Dictionary=candidate.duplicate(true)
	var context: Dictionary=Context.create(candidate,command_fixture()); var original: Dictionary=context.duplicate(true)
	check(Observers.reconcile(candidate,{},context,{}).get("code")=="MissingObserverRegistry","缺注册表明确拒绝")
	var wrong: Dictionary=registry([descriptor("proper",record_observation)])
	wrong.observers["proper"].id="mismatch"
	check(Observers.reconcile(candidate,{},context,wrong).get("code")=="InvalidObserverDescriptor","注册字典键必须与描述ID一致")
	check(candidate==before and context==original,"注册预检失败保持候选与context不变")
	check(Observers.reconcile(candidate,{},context,registry([])).get("ok",false) and candidate==before and context.observer_steps==0,"无观察器不引入状态或RNG变化")
	context=Context.create(candidate,command_fixture()); context.observer_status="running"
	check(Observers.reconcile(candidate,{},context,registry([])).get("code")=="ObserverReentry","正在运行的context不能递归重入")

## 局部合同测试不要求生成内容和UI启动，缺模块仍有明确非零结果。
func _run() -> void:
	var path: String="res://core/runtime/transaction_context.gd"
	check(ResourceLoader.exists(path),"transaction_context合同存在")
	if ResourceLoader.exists(path):
		Context=load(path)
		test_context_identity()
		test_context_rejections()
		path="res://core/runtime/domain_observers.gd"
		check(ResourceLoader.exists(path),"domain_observers合同存在")
		if ResourceLoader.exists(path):
			Observers=load(path)
			test_observer_registration()
			test_observer_idempotence()
			test_observer_step_limit()
			test_candidate_failure_boundary()
			test_invalid_results_and_context_isolation()
			test_payload_contract()
			test_failure_envelope_is_bounded()
			test_registry_preflight()
	print("CHECKS=",checks," FAILURES=",failures)
	quit(0 if failures==0 else 1)
