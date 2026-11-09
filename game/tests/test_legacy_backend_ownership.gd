## 原Kernel事务与单后端拥有权局部门；冻结真03a行为、完整候选失败和受信子类派发，不扩Codec或物理存档。
extends SceneTree
const Kernel=preload("res://core/world_kernel.gd")
const Schema=preload("res://core/state_schema.gd")
const BASELINE_KERNEL_SHA256="85fd5521b2b25d6e0e48897e14fb664105329d3846e97a6676addde2969dc640"
const SCENARIO="base::scenario.alpha_life"
var checks: int=0
var failures: int=0
var content_path: String=""
var rows: Array=[]
var kernels: Array=[]

## 所有探针先走真实原实现；故障只作用于私有候选，原validator与observer契约不被删减。
class HookKernel extends Kernel:
	var mode: String=""
	var terminal_failure: bool=false
	var calls: Dictionary={"descriptors":0,"preview":0,"apply":0,"advance":0,"observer":0,"terminal":0}
	var observed: Dictionary={}
	## 原事件observer保持精确callback；在其后追加受信故障点，终验仅包装原最后validator。
	func _module_descriptors() -> Array:
		calls.descriptors+=1
		var modules: Array=super._module_descriptors()
		for module in modules:
			if module.id=="interaction": module.observers.append({"id":"test.backend_tail","owner":"interaction","before":[],"after":["interaction.events_reconcile"],"callback":_observer})
			for entry in module.validators:
				if entry.id!="interaction.sessions":continue
				var original: Callable=entry.callback
				entry.callback=func(candidate: Dictionary,data: Dictionary) -> Dictionary:
					var result: Dictionary=original.call(candidate,data)
					calls.terminal+=1
					if result.ok and terminal_failure:
						observed=candidate.duplicate(true)
						return {"ok":false,"code":"InjectedTerminalValidator"}
					return result
		return modules
	## 明确的真实DomainObservers尾调用，不伪装生产事件回调或跳过注册检查。
	func _observer(candidate: Dictionary, _data: Dictionary, _context: Dictionary) -> Dictionary:
		calls.observer+=1
		if mode=="observer":
			_taint(candidate);observed=candidate.duplicate(true)
			return {"ok":false,"code":"InjectedObserver"}
		if mode=="schema":observed=candidate.duplicate(true)
		return {"ok":true}
	## 私有失败候选额外改变钱、随机流及一批真实物品，检验全态回滚而非只比日期。
	func _taint(candidate: Dictionary) -> void:
		candidate.wallets[candidate.actors[candidate.player_actor_id].wallet_id].balance+=7
		candidate.rng.world.state="8675309"
		for inventory in candidate.inventories.values():
			if not inventory.lots.is_empty():inventory.lots[0].quantity+=1;break
	## 先取原报价，再用明确拒绝证明传入的是当前子类动态方法。
	func _preview(state: Dictionary, kind: String, params: Dictionary, actor_id: String) -> Dictionary:
		calls.preview+=1
		var result: Dictionary=super._preview(state,kind,params,actor_id)
		if result.ok and mode=="preview":return {"ok":false,"code":"InjectedPreview"}
		return result
	## 真实apply已分配行动后才注错；Schema分支只制造原类型/范围校验应拒绝的疲劳。
	func _apply(state: Dictionary, kind: String, params: Dictionary, actor_id: String, offer: Dictionary) -> Dictionary:
		calls.apply+=1
		var result: Dictionary=super._apply(state,kind,params,actor_id,offer)
		if not result.ok:return result
		if mode=="apply":_taint(state);observed=state.duplicate(true);return {"ok":false,"code":"InjectedApply"}
		if mode=="schema":state.actors[state.player_actor_id].fatigue=-1
		return result
	## 世界真实推进到尾部以后拒绝；backend必须保留Kernel原动态advance分派。
	func _advance(state: Dictionary, hold: Array=[]) -> Dictionary:
		calls.advance+=1
		var result: Dictionary=super._advance(state,hold)
		if result.ok and mode=="advance":observed=state.duplicate(true);return {"ok":false,"code":"InjectedAdvance"}
		return result

var scenario_id: String=SCENARIO
var restart_scenario_id: String="base::scenario.two_stories"

## 独立内容准备完毕后运行，所有新接口缺失都转明确红灯而非解析崩溃。
func _init() -> void:call_deferred("run")

## 失败保留标签并进入进程退出码。
func check(value: bool, label: String) -> bool:
	checks+=1
	if not value:failures+=1;printerr("FAIL: ",label)
	return value

## 原目录正式开局，记录实例用于最终解除测试闭包环；不改变生产对象生命周期。
func opened() -> HookKernel:
	var k=HookKernel.new();kernels.append(k)
	check(k.new_game(scenario_id,content_path).ok,"真实03a内容Kernel开局")
	return k

## 命令序号和版本只来自当前快照，不擅自预占下一条请求。
func command(k, kind: String="wait") -> Dictionary:
	var s: Dictionary=k.snapshot();var actor: String=s.player_actor_id
	return {"kind":kind,"params":{},"actor_id":actor,"request_seq":s.request_sequences[actor]+1,"expected_revision":s.revision}

## 三元组值与当前容器引用分别保留；失败即使换入等值副本也不能悄悄发布。
func retained(k, prior: Dictionary, references: Array, label: String) -> void:
	check(k.save_context()==prior,label+" state/catalog/registry整值不变")
	check(is_same(k._state,references[0]) and is_same(k._catalog,references[1]) and is_same(k._registry,references[2]),label+" 三元组引用未发布")

## 注册Callable仅在oracle中转稳定方法形状，不保存进程对象ID；游戏状态仍完整保存作比对。
func plain(value: Variant) -> Variant:
	if value is Callable:return {"callable_method":str(value.get_method()),"argument_count":value.get_argument_count()}
	if value is Dictionary:
		var result: Dictionary={}
		for key in value:result[str(key)]=plain(value[key])
		return result
	if value is Array:
		var result: Array=[]
		for row in value:result.append(plain(row))
		return result
	return value

## 冻结真实输出、完整state及目录/注册形状摘要，避免只对新包装器自身相互比较。
func capture(label: String, k, result: Dictionary={}) -> void:
	rows.append({"label":label,"result":plain(result),"state":k.snapshot(),"catalog_sha256":JSON.stringify(plain(k.content()),"",true,true).sha256_text(),"registry_sha256":JSON.stringify(plain(k._registry),"",true,true).sha256_text(),"calls":k.calls.duplicate(true)})

## 空世界、命令形状、actor、缓存和revision按旧顺序短路；旧有效序号优先命中缓存。
func precedence_and_replay() -> void:
	var empty=HookKernel.new();kernels.append(empty)
	check(empty.submit({}).get("code")=="NoWorld","NoWorld先于坏命令形状")
	capture("empty_first",empty,empty.submit({}))
	var k=opened();var original: Dictionary=k.save_context()
	var bad: Dictionary=command(k);bad.kind=4;bad.actor_id="missing";bad.expected_revision=-1
	check(k.submit(bad).get("code")=="InvalidCommand","命令形状先于actor和revision")
	capture("shape_first",k,k.submit(bad))
	bad=command(k);bad.actor_id="missing";bad.expected_revision=-1
	check(k.submit(bad).get("code")=="UnknownActor","actor先于revision")
	capture("actor_first",k,k.submit(bad))
	bad=command(k,"missing");bad.expected_revision=-1
	check(k.submit(bad).get("code")=="StalePreview","revision先于领域首错")
	check(k.save_context()==original,"全部原前置拒绝无世界/目录/注册变化")
	capture("revision_first",k,k.submit(bad))
	var request: Dictionary=command(k);var result: Dictionary=k.submit(request)
	if not check(result.ok,"真实wait提交成功"):return
	var after: Dictionary=k.snapshot();var calls: Dictionary=k.calls.duplicate(true)
	var replay: Dictionary=request.duplicate(true);replay.expected_revision=-99;replay.kind="other_but_well_typed"
	check(k.submit(replay)==result and k.snapshot()==after and k.calls==calls,"先重放后revision，合法形状旧请求不再次preview/apply")
	var replayed: Dictionary=k.submit(request);replayed.details.test_only=[]
	check(k.snapshot()==after and k.submit(request)==result,"重放结果也是副本")
	bad=command(k,"missing")
	check(k.submit(bad).get("code")=="ActorBusy","原ActorBusy仍先于未知领域命令")
	capture("wait_replay_busy",k,k.submit(bad))
	check(k.submit(command(k,"advance")).ok,"真实advance完成wait")
	capture("advance_success",k)

## 两实例互不共享容器，所有公开导出保持深副本而不是只拷根字典。
func copies_and_instances() -> void:
	var a=opened();var b=opened();var before: Dictionary=a.save_context();var other: Dictionary=b.save_context()
	check(not is_same(a._state,b._state) and not is_same(a._catalog,b._catalog) and not is_same(a._registry,b._registry),"两Kernel三元组各自独立")
	var snapshot: Dictionary=a.snapshot();snapshot.actors[snapshot.player_actor_id].skills.clear();snapshot.rng.world.state="changed"
	var catalog: Dictionary=a.content();catalog.data.actors.clear()
	var context: Dictionary=a.save_context();context.state.wallets.clear();context.catalog.data.scenarios.clear();context.registry.modules[context.registry.modules.keys()[0]].owns.clear()
	check(a.save_context()==before and b.save_context()==other,"snapshot/content/save_context嵌套更改不泄露")
	check(a.submit(command(a)).ok and b.save_context()==other,"第一实例实际提交不改变第二实例")
	capture("instance_a",a);capture("instance_b",b)

## 每个明确失败都发生在真实阶段，保三元组并可同请求修复重试、成功后不二次派发。
func transaction_failures() -> void:
	for mode in ["preview","apply","observer","schema","advance"]:
		var k=opened()
		if mode=="advance" and not check(k.submit(command(k)).ok,"advance故障前建立真实wait"):continue
		var before: Dictionary=k.save_context();var references: Array=[k._state,k._catalog,k._registry]
		var request: Dictionary=command(k,"advance" if mode=="advance" else "wait");k.mode=mode
		var result: Dictionary=k.submit(request);var expected: String="InvalidActorState" if mode=="schema" else "Injected"+mode.capitalize()
		check(not result.ok and result.get("code")==expected,"真实阶段失败首错 "+mode+" "+str(result))
		retained(k,before,references,mode)
		if mode in ["apply","observer"]:check(k.observed.wallets!=before.state.wallets and k.observed.rng!=before.state.rng and k.observed.inventories!=before.state.inventories,"故障候选确曾改钱/RNG/物品 "+mode)
		if mode in ["observer","schema"]:check(k.observed.revision==before.state.revision+1 and k.observed.history.size()==before.state.history.size()+1 and k.observed.results!=before.state.results,"observer/Schema故障前实际结果与历史已写候选 "+mode)
		if mode=="advance":check(k.observed.tick==before.state.tick+1 and k.calls.advance>0,"真实世界已越过时钟尾部后失败")
		capture("failure_"+mode,k,result)
		k.mode="";result=k.submit(request)
		if not check(result.ok,"同失败请求解除原因后成功 "+mode):continue
		var after: Dictionary=k.save_context();var calls: Dictionary=k.calls.duplicate(true)
		check(k.submit(request)==result and k.save_context()==after and k.calls==calls,"成功原请求重放唯一结算 "+mode)
		capture("retry_"+mode,k,result)

## startup和restore尾验失败都不得提前替换state/catalog/registry；只测试原内存契约，不扩大存档工程。
func publication_failures() -> void:
	var k=opened();check(k.submit(command(k)).ok,"先保一个已有真实世界")
	var before: Dictionary=k.save_context();var references: Array=[k._state,k._catalog,k._registry]
	k.terminal_failure=true
	var result: Dictionary=k.new_game(restart_scenario_id,content_path)
	check(result.get("code")=="InjectedTerminalValidator" and k.observed.scenario_id==restart_scenario_id,"new_game真实最后validator拒绝完整新世界")
	retained(k,before,references,"new_game尾失败");capture("new_game_terminal_failure",k,result)
	k.terminal_failure=false
	var donor=opened();check(donor.submit(command(donor)).ok and donor.submit(command(donor,"advance")).ok,"另一个真实世界提供合法恢复输入")
	var source: Dictionary=donor.save_context();var source_before: Dictionary=source.duplicate(true)
	k.terminal_failure=true;result=k.restore(source.state)
	check(result.get("code")=="InjectedTerminalValidator","restore原最后validator失败")
	retained(k,before,references,"restore尾失败");capture("restore_terminal_failure",k,result);k.terminal_failure=false
	donor.terminal_failure=true;result=k.restore_with_catalog(source.state,source.catalog,source.registry)
	check(result.get("code")=="InjectedTerminalValidator","restore_with_catalog传入真实registry尾失败")
	retained(k,before,references,"restore_with_catalog尾失败");capture("restore_catalog_terminal_failure",k,result);donor.terminal_failure=false
	var wrong_engine: Dictionary=source.state.duplicate(true);wrong_engine.engine_build="old-test-engine";wrong_engine.rng_backend="godot:old-test-engine"
	check(Schema.validate(wrong_engine,source.catalog,source.registry).ok,"旧引擎输入先通过真实Schema后才到原引擎尾守卫")
	result=k.restore_with_catalog(wrong_engine,source.catalog,source.registry)
	check(result.get("code")=="IncompatibleEngine","restore_with_catalog原引擎尾失败")
	retained(k,before,references,"restore引擎尾失败");capture("restore_engine_failure",k,result)
	check(source==source_before,"全部restore失败不修改输入三元组")
	check(k.restore_with_catalog(source.state,source.catalog,source.registry).ok,"合法restore原子发布三元组")
	var installed: Dictionary=k.save_context();source.state.actors[source.state.player_actor_id].skills.clear();source.catalog.data.actors.clear();source.registry.modules.clear()
	check(k.save_context()==installed,"恢复成功安装深副本，不持外部可变三元组")
	capture("restored_success",k)

## 原64条结果缓存边界用真实命令经过，不向私有状态塞伪造结果或改截断常数。
func bounded_cache() -> void:
	var k=opened();var first: Dictionary=command(k);check(k.submit(first).ok,"缓存首个真实请求")
	for index in range(65):
		if not check(k.submit(command(k,"advance" if index%2==0 else "wait")).ok,"真实缓存边界命令 "+str(index)):return
	check(k.snapshot().results.size()==64,"原缓存容量仍64")
	var result: Dictionary=k.submit(first);check(result.get("code")=="AlreadyApplied","已淘汰旧序号沿用原AlreadyApplied")
	capture("cache_eviction",k,result)

## 结构风险红只要求唯一backend及兼容别名，不误称对受信代码建立私有写屏障。
func ownership() -> void:
	var a=opened();var b=opened();var found: bool=false
	for property in a.get_property_list():
		if property.name=="_backend":found=true
	if not check(found,"Kernel必须持有唯一legacy backend"):return
	var backend=a.get("_backend");var second=b.get("_backend")
	check(backend!=null and second!=null and backend!=second,"每个Kernel独立backend实例")
	check(is_same(a._state,backend.state) and is_same(a._catalog,backend.catalog) and is_same(a._registry,backend.registry),"受信别名直接指唯一backend容器")
	var state: Dictionary=a._state;var catalog: Dictionary=a._catalog;var registry: Dictionary=a._registry
	a._state=state.duplicate(true);a._catalog=catalog.duplicate(true);a._registry=registry.duplicate(true)
	check(is_same(a._state,backend.state) and is_same(a._catalog,backend.catalog) and is_same(a._registry,backend.registry),"受信别名setter同步同一个backend而非第二份拥有者")
	var owned: Array=["state","catalog","registry","_native_source_generation","_native_growth_enabled","_native_growth_source_commit","_native_growth_cache","_native_travel_schedule_enabled","_native_travel_schedule_provider","_native_military_schedule_enabled","_native_military_schedule_provider"]
	for property in backend.get_property_list():
		if not property.usage&PROPERTY_USAGE_SCRIPT_VARIABLE:continue
		check(property.name in owned,"backend只有三权威容器与明确私有native字段 "+property.name)
		if property.name in ["_native_source_generation","_native_growth_enabled","_native_growth_source_commit","_native_travel_schedule_enabled"]:check(_plain_cache_value(backend.get(property.name)),"private标量不持Kernel或Callable "+property.name)
	if "_native_growth_cache" in backend:
		var cache=backend.get("_native_growth_cache")
		check(cache is RefCounted and not cache is Kernel,"唯一adapter不反持Kernel")
		for property in cache.get_property_list():
			if not property.usage&PROPERTY_USAGE_SCRIPT_VARIABLE:continue
			check(property.name in ["_generation","_attempted","_bridge","_action","_counts","_total_usec","_last_usec"],"adapter不能留state/catalog/registry/payload "+property.name)
			var value: Variant=cache.get(property.name)
			if property.name=="_bridge":check(value==null or value.get_class()=="HanmoWorldBridge","adapter只可持一个正式Bridge")
			else:check(_plain_cache_value(value),"adapter派生值不能持Kernel或永久Callable "+property.name)
	var provider=backend.get("_native_travel_schedule_provider")
	check(provider is RefCounted and not provider is Kernel,"旅行provider不反持Kernel")
	for property in provider.get_property_list():
		if not property.usage&PROPERTY_USAGE_SCRIPT_VARIABLE:continue
		check(property.name in ["_expected_build","_call_seq","_bridge"],"旅行provider不持世界或提案 "+property.name)
		var value: Variant=provider.get(property.name)
		if property.name=="_bridge":check(value==null or value.get_class()=="HanmoWorldBridge","旅行provider只持无工件Bridge")
		else:check(_plain_cache_value(value),"旅行配置与同步序号不持Object/Callable "+property.name)
	var military_provider=backend.get("_native_military_schedule_provider")
	check(military_provider is RefCounted and not military_provider is Kernel,"军政provider不反持Kernel")
	for property in military_provider.get_property_list():
		if not property.usage&PROPERTY_USAGE_SCRIPT_VARIABLE:continue
		check(property.name in ["_expected_build","_call_seq","_bridge"],"军政provider不持世界或提案 "+property.name)
		var value: Variant=military_provider.get(property.name)
		if property.name=="_bridge":check(value==null or value.get_class()=="HanmoWorldBridge","军政provider只持无工件Bridge")
		else:check(_plain_cache_value(value),"军政配置与同步序号不持Object/Callable "+property.name)
	var before: Dictionary=b.save_context();check(a.submit(command(a)).ok and b.save_context()==before,"backend实例实际提交仍隔离")

## 私有缓存只容许拥有的普通值；原registry中的受信领域Callable不在派生缓存边界内。
func _plain_cache_value(value: Variant) -> bool:
	if value is Callable or value is Object:return false
	if value is Dictionary:
		for key in value:
			if not _plain_cache_value(key) or not _plain_cache_value(value[key]):return false
	if value is Array:
		for child in value:
			if not _plain_cache_value(child):return false
	return true

## 冻结仅普通值Variant以保整数/StringName类型，不编码对象；原03a首次写，后续只读且不覆盖期望。
func oracle(args: PackedStringArray) -> void:
	for argument in args:
		if argument.begins_with("--record-oracle="):
			var path: String=argument.trim_prefix("--record-oracle=")
			if not check(FileAccess.get_file_as_string("res://core/world_kernel.gd").sha256_text()==BASELINE_KERNEL_SHA256 and not FileAccess.file_exists(path),"只能从原03a精确源码首次冻结oracle"):return
			var file=FileAccess.open(path,FileAccess.WRITE)
			if not check(file!=null,"可写独立绝对oracle路径"):return
			file.store_var({"format":"HM-LEGACY-BACKEND-ORACLE/1","kernel_sha256":BASELINE_KERNEL_SHA256,"rows":rows},false);file.close();print("ORACLE_RECORDED=",path);return
		if argument.begins_with("--oracle="):
			var path: String=argument.trim_prefix("--oracle=");var file=FileAccess.open(path,FileAccess.READ)
			var saved: Variant=file.get_var(false) if file!=null else null
			if not check(saved is Dictionary and saved.get("kernel_sha256")==BASELINE_KERNEL_SHA256 and saved.get("rows") is Array,"读取真实03a冻结oracle"):return
			check(saved.rows.size()==rows.size(),"原oracle记录数相同")
			for index in mini(saved.rows.size(),rows.size()):check(saved.rows[index]==rows[index],"原Kernel精确state/输出/目录/注册/hook顺序 "+rows[index].label)
			return
	check(false,"必须明确record或只读oracle路径")

## 测试输入仅解析普通JSON对象；缺文件/坏格式转check失败，不能依赖未tracked本地pin。
func input_object(path: String) -> Dictionary:
	if path.is_empty() or not FileAccess.file_exists(path):check(false,"正式prepared输入不存在："+path);return {}
	var parser=JSON.new()
	if parser.parse(FileAccess.get_file_as_string(path))!=OK or not parser.data is Dictionary or parser.data.is_empty():check(false,"正式prepared输入必须是JSON对象："+path);return {}
	return parser.data

## 显式content/scenario可在任意正式准备目录重建旧新对照；省略content时才兼容原独立pin。
func configure_input(args: PackedStringArray) -> bool:
	var explicit_content: bool=false
	for argument in args:
		if argument.begins_with("--content="):content_path=argument.trim_prefix("--content=");explicit_content=true
		elif argument.begins_with("--scenario="):scenario_id=argument.trim_prefix("--scenario=")
	if not explicit_content:
		var pin: Dictionary=input_object("res://data/safe-departure-button-fixture.json")
		if pin.is_empty():return false
		if not pin.get("directory") is String or pin.directory.is_empty():check(false,"默认pin缺有效directory，或请使用--content");return false
		content_path="res://data/generated/"+pin.directory+"/catalog.hmcatalog.json"
	var header: Dictionary=input_object(content_path)
	if header.is_empty():return false
	if not header.get("format") is String or header.format!="HM-CONTENT-INDEX" or not (header.get("format_version") is int or header.get("format_version") is float) or header.format_version!=1 or not header.get("catalog") is Dictionary or not header.catalog.get("scenarios") is Dictionary:
		check(false,"--content必须为正式prepared HM-CONTENT-INDEX目录");return false
	if scenario_id.is_empty() or not header.catalog.scenarios.has(scenario_id):check(false,"正式prepared内容没有请求的scenario："+scenario_id);return false
	if not header.catalog.scenarios.has(restart_scenario_id):restart_scenario_id=scenario_id
	print("LEGACY_BACKEND_INPUT={catalog:",content_path,",scenario:",scenario_id,"}")
	return true

## 使用已经正式准备且pin明确的tiny内容；不重跑军政矩阵或任何Codec/Native路径。
func run() -> void:
	print("LEGACY_BACKEND_STORAGE={Codec:false,Native:false,user_data_dir:",OS.get_user_data_dir(),"}")
	var args: PackedStringArray=OS.get_cmdline_user_args()
	if not configure_input(args):
		print("LEGACY_BACKEND_OWNERSHIP_CHECKS=",checks," FAILURES=",failures);quit(1);return
	if "--ownership-only" in args:ownership()
	else:
		precedence_and_replay();copies_and_instances();transaction_failures();publication_failures();bounded_cache()
		oracle(args)
		if "--ownership-required" in args:ownership()
	for k in kernels:k._registry={}
	kernels.clear()
	print("LEGACY_BACKEND_OWNERSHIP_CHECKS=",checks," FAILURES=",failures);quit(0 if failures==0 else 1)
