## 检验可信注册失败关闭、无回调执行及真实Kernel启动事务，不宣称领域已经迁移。
extends SceneTree
const News=preload("res://core/domains/social/news_commands.gd")
const Discovery=preload("res://core/domains/interaction/discovery_commands.gd")
const Movement=preload("res://core/domains/travel/movement_commands.gd")
const DialogueCommands=preload("res://core/domains/interaction/dialogue_commands.gd")
const Rest=preload("res://core/long_rest.gd")
const Journey=preload("res://core/journey_commands.gd")
const Personal=preload("res://core/personal_combat.gd")
const Campaign=preload("res://core/campaign_rules.gd")
const MilitaryCombat=preload("res://core/military_combat.gd")
const SiegeWorld=preload("res://core/siege_world.gd")
const MilitaryAssets=preload("res://core/military_assets.gd")
const Parties=preload("res://core/party_rules.gd")
const Encounters=preload("res://core/encounter_rules.gd")
const RuntimeCommands=preload("res://core/runtime/runtime_commands.gd")
const ServiceCommands=preload("res://core/domains/services/service_commands.gd")
const Cultivation=preload("res://core/cultivation_rules.gd")
const Family=preload("res://core/family_rules.gd")
const Estate=preload("res://core/estate_rules.gd")
var checks: int=0
var failures: int=0

## 延后执行以便输出独立的断言总数。
func _init() -> void:
	call_deferred("_run")

## 记录真实断言，失败令进程非零退出。
func check(value: bool, label: String) -> void:
	checks+=1
	if not value:
		failures+=1
		printerr("FAIL: ",label)

var hits: int=0

## 构造完整而无世界依赖的受信描述，用于负向变体。
func descriptor(id: String="base") -> Dictionary:
	return {"id":id,"version":"f10-architecture-1","requires":{},"optional_requires":{},"capabilities":[],"owns":[],"commands":[],"actions":[],"phases":[],"validators":[],"queries":[]}

## 若注册器错误执行回调，计数会立即揭露越界。
func sentinel() -> Dictionary:
	hits+=1
	return {"ok":true}

## 状态回调使用明确二参ABI；注册时仍必须保持零次执行。
func state_sentinel(_state: Dictionary, _data: Dictionary) -> Dictionary:
	hits+=1
	return {"ok":true}

## 所有权探针需要真实三参载荷绑定，但注册不能执行它。
func action_sentinel(_state: Dictionary, _data: Dictionary, _action: Dictionary) -> Dictionary:
	hits+=1
	return {"ok":true}

## 仅在注册成功后由测试显式调用，用以检查拒绝处理器。
func action_contract(registry) -> Dictionary:
	return {"kind":"work","prepare":sentinel,"start":sentinel,"step":sentinel,"finish":sentinel,"cancel":registry.refuse_cancel,"resume":registry.refuse_resume,"validate":sentinel,"interrupt_policy":{"cancel":"unsupported","resume":"unsupported"}}

## 所有坏描述必须稳定失败而不是抛出字段或类型错误。
func rejected(registry, entries: Array, code: String, label: String) -> void:
	var result: Dictionary=registry.build(entries)
	check(not result.ok and result.get("code")==code,label+"："+str(result))

## 系统性检查必需形状、依赖版本、唯一性、生命周期和所有权。
func test_registry(registry) -> void:
	var basic: Dictionary=descriptor()
	rejected(registry,[],"InvalidModuleDescriptor","空注册不能启动世界")
	check(registry.build([basic]).ok,"合法最小描述")
	for key in basic:
		var bad: Dictionary=basic.duplicate(true); bad.erase(key)
		rejected(registry,[bad],"InvalidModuleDescriptor","缺必需字段 "+key)
	for key in ["requires","optional_requires","capabilities","owns","commands","actions","phases","validators","queries"]:
		var bad: Dictionary=basic.duplicate(true); bad[key]=1
		rejected(registry,[bad],"InvalidModuleDescriptor","错误集合类型 "+key)
	rejected(registry,[3],"InvalidModuleDescriptor","非字典")
	rejected(registry,[basic,basic],"DuplicateModule","重复模块")
	var dependent: Dictionary=descriptor("dependent")
	dependent.requires={"base":"f10-architecture-1"}
	rejected(registry,[dependent],"UnknownModuleDependency","缺必需依赖")
	dependent.requires.base="f09-identity-1"
	rejected(registry,[basic,dependent],"IncompatibleModuleVersion","依赖精确版本不符")
	dependent.requires={}; dependent.optional_requires={"base":"f09-identity-1"}
	check(registry.build([dependent]).ok,"不存在可选依赖允许")
	rejected(registry,[basic,dependent],"IncompatibleModuleVersion","已安装可选依赖版本不符")
	dependent.optional_requires={}; dependent.requires={"base":"f10-architecture-1"}
	var cycle: Dictionary=basic.duplicate(true); cycle.requires={"dependent":"f10-architecture-1"}
	rejected(registry,[cycle,dependent],"ModuleDependencyCycle","模块环")
	cycle.requires={"base":"f10-architecture-1"}
	rejected(registry,[cycle],"ModuleDependencyCycle","自环")
	var complete: Dictionary=descriptor("work")
	complete.commands=[{"kind":"work","mode":"action","preview":sentinel,"apply":null,"gate_id":"legacy_kernel"}]
	complete.actions=[action_contract(registry)]
	complete.phases=[{"id":"phase","before":[],"after":[],"reads":[],"writes":[],"run":sentinel}]
	complete.validators=[{"id":"validator","stage":"before_actions","order":0,"signature":"state,data","callback":state_sentinel}]
	complete.queries=[{"id":"query","callback":sentinel}]
	complete.observers=[{"id":"observer","before":[],"after":[],"callback":action_sentinel}]
	var before: Dictionary=complete.duplicate(true)
	var built: Dictionary=registry.build([complete,basic,dependent])
	check(built.ok and hits==0 and complete==before,"注册收集时不执行回调也不改输入")
	var again: Dictionary=registry.build([dependent,basic,complete])
	check(built==again,"描述输入顺序不影响稳定结果")
	check(built.modules.keys()==["base","dependent","work"],"依赖拓扑同位按ID排序")
	check(built.actions.work.cancel.call().code=="ActionNotInterruptible","显式不可中断")
	check(built.actions.work.resume.call().code=="ActionNotResumable","显式不可恢复")
	for method in ["prepare","start","step","finish","cancel","resume","validate"]:
		var bad: Dictionary=complete.duplicate(true); bad.actions[0].erase(method)
		rejected(registry,[bad],"InvalidActionDescriptor","缺生命周期 "+method)
	var bad: Dictionary=complete.duplicate(true); bad.actions[0].cancel=sentinel
	rejected(registry,[bad],"InvalidInterruptPolicy","不允许成功空处理冒充拒绝")
	bad=complete.duplicate(true); bad.commands[0].preview=null
	rejected(registry,[bad],"InvalidCommandDescriptor","缺preview")
	bad=complete.duplicate(true); bad.commands[0].mode="instant"
	rejected(registry,[bad],"InvalidCommandDescriptor","即时命令缺apply")
	bad=complete.duplicate(true); bad.actions=[]
	rejected(registry,[bad],"UnknownCommandAction","动作命令缺实际生命周期")
	bad=complete.duplicate(true); bad.commands.append(bad.commands[0])
	rejected(registry,[bad],"DuplicateCommand","重复命令")
	bad=complete.duplicate(true); bad.actions.append(bad.actions[0])
	rejected(registry,[bad],"DuplicateAction","重复行动")
	for collection in ["validators","observers","queries"]:
		bad=complete.duplicate(true); bad[collection][0].callback=null
		rejected(registry,[bad],"InvalidStateValidator" if collection=="validators" else "InvalidModuleCallback","无效注册回调 "+collection)
	bad=complete.duplicate(true); bad.phases[0].run=null
	rejected(registry,[bad],"InvalidPhaseHook","阶段使用run并拒绝空绑定")
	bad=complete.duplicate(true); bad.phases[0].owner="base"
	rejected(registry,[bad],"InvalidPhaseHook","阶段owner不能冒用另一模块")
	bad=complete.duplicate(true); bad.phases.append(bad.phases[0])
	rejected(registry,[bad],"DuplicatePhaseId","跨注册唯一阶段ID")
	check(built.phases.phase.owner=="work" and built.phase_plan.order==["phase"],"注入真实包含模块owner并编译一次")
	var region: Dictionary={"container":"actions","scope":"envelope","key":"","owner":"base","schema_refs":[{"path":"game/core/state_schema.gd","method":"validate"}]}
	basic.owns=[region]
	check(registry.build([basic]).ok,"共享容器声明")
	bad=basic.duplicate(true); bad.owns[0].owner="missing"
	rejected(registry,[bad],"UnknownStateOwner","不存在state owner")
	bad=basic.duplicate(true); bad.owns.append(bad.owns[0])
	rejected(registry,[bad],"DuplicateStateOwner","重复区域")
	bad=basic.duplicate(true); bad.owns.append({"container":"actions","scope":"kind","key":"work","owner":"base","schema_refs":region.schema_refs})
	bad.legacy_action_validators=[{"kind":"work","signature":"state,data,action","validate":action_sentinel}]
	check(registry.build([bad]).ok,"容器与kind子区分属不同权限层")
	bad.owns[0].scope="root"
	rejected(registry,[bad],"OverlappingStateOwner","整个root不可与子区重复授权")

## canonical拒绝支持零/四参，输入深快照和注册sentinel均不得改变。
func test_full_refusal_abi(registry) -> void:
	var supported: bool=true
	for method in registry.get_script_method_list():
		if method.name in ["refuse_cancel","refuse_resume"] and method.args.size()!=4: supported=false
	check(supported,"canonical refusal缺完整四参ABI")
	if not supported: return
	var state: Dictionary={"nested":[{"state":1}]}; var data: Dictionary={"nested":[{"data":2}]}
	var action: Dictionary={"params":{"list":[3]}}; var params: Dictionary={"nested":[4]}
	var before: Array=[state.duplicate(true),data.duplicate(true),action.duplicate(true),params.duplicate(true)]
	check(registry.refuse_cancel()=={"ok":false,"code":"ActionNotInterruptible"},"零参cancel兼容")
	check(registry.refuse_resume()=={"ok":false,"code":"ActionNotResumable"},"零参resume兼容")
	check(registry.refuse_cancel(state,data,action,"user")=={"ok":false,"code":"ActionNotInterruptible"},"四参cancel执行")
	check(registry.refuse_resume(state,data,action,params)=={"ok":false,"code":"ActionNotResumable"},"四参resume执行")
	check([state,data,action,params]==before and hits==0,"拒绝不改四个输入，build回调仍零次")

## 测试专用继承只替换可信描述来源；生产不增加外部脚本加载入口。
class TestKernel extends "res://core/world_kernel.gd":
	var bad_registration: bool=false
	var source: Array=[]
	## 为启动事务注入坏注册，其他行为仍为真实Kernel。
	func _module_descriptors() -> Array:
		return [{}] if bad_registration else source if not source.is_empty() else super._module_descriptors()

## 检验真实启动拒绝保持状态、目录与随机流，并回归实际错误优先级。
func test_startup(builtin, registry) -> void:
	var descriptors: Array=builtin.descriptors()
	var built: Dictionary=registry.build(descriptors)
	check(built.ok and not built.modules.is_empty(),"发行内建注册有效")
	var expected_commands: Array=["accept_offer","cancel_task","deliver_message","report_offer","trade"]+MilitaryAssets.KINDS+Campaign.KINDS+MilitaryCombat.KINDS+SiegeWorld.KINDS+Parties.KINDS+Cultivation.KINDS+Family.KINDS+Estate.KINDS+Personal.KINDS+Journey.KINDS+Rest.KINDS+Encounters.KINDS+RuntimeCommands.KINDS+ServiceCommands.KINDS+News.KINDS+Discovery.KINDS+Movement.KINDS+DialogueCommands.KINDS; expected_commands.sort()
	var expected_actions: Array=["accept_offer","deliver_message","report_offer","trade"]+MilitaryAssets.ACTION_KINDS; expected_actions.sort()
	check(built.commands.keys()==expected_commands and built.actions.keys()==expected_actions,"trade/tasks、军事、同行及修业迁移，精确集合不掩盖漏接")
	for kind in ["accept_offer","deliver_message","report_offer"]:
		check(built.commands[kind].owner=="tasks" and built.actions[kind].owner=="tasks","真实任务命令行动归tasks："+kind)
	check(built.commands.cancel_task.mode=="instant" and built.commands.cancel_task.apply.is_valid(),"cancel_task为真实即时绑定")
	check(built.validators.has("tasks.state") and built.validators["tasks.state"].callback.is_valid(),"tasks.state有真实固定绑定")
	var fresh=TestKernel.new(); fresh.bad_registration=true
	check(not fresh.new_game().ok and fresh.snapshot().is_empty() and fresh.content().is_empty(),"坏注册不创建世界")
	fresh.bad_registration=false
	check(fresh.new_game().ok,"恢复正确注册可建立世界")
	var state: Dictionary=fresh.snapshot(); var catalog: Dictionary=fresh.content()
	fresh.bad_registration=true
	check(fresh.new_game("missing","user://a11-unavailable-content.json").get("code")=="InvalidModuleDescriptor","注册先于内容和世界构造失败")
	check(fresh.snapshot()==state and fresh.content()==catalog,"失败保留既有状态和目录全部字节值")
	check(fresh.snapshot().rng==state.rng and fresh.snapshot().next_instance_id==state.next_instance_id,"失败不耗随机数和实例ID")
	fresh.bad_registration=false
	check(fresh.preview("wait").ok,"启动接缝不改旧命令路径")
	var roots: Array=[]; var actor_fields: Array=[]
	for module in descriptors:
		for claim in module.owns:
			if claim.scope in ["root","envelope"]: roots.append(claim.container)
			if claim.scope=="actor_field": actor_fields.append(claim.key)
	roots.sort()
	var actual_roots: Array=[]
	for key in state: actual_roots.append(str(key))
	actual_roots.sort()
	check(roots==actual_roots,"当前全部根状态有唯一root/envelope authority："+str(roots)+" / "+str(actual_roots))
	for person in state.actors.values():
		for field in person: check(field in actor_fields,"已有actor字段声明 "+field)
	var player: String=state.player_actor_id
	var before: Dictionary=state.duplicate(true)
	state.actors[player].location={"kind":"InArmy"}; state.actors[player].available=false
	check(fresh._preview(state,"wait",{},player).code=="NotArmyCommander","军队占用先于不可用")
	state.encounters["pending"]={"status":"pending","participants":[player]}
	check(fresh._preview(state,"wait",{},player).code=="NotArmyCommander","军队守卫实际先于遭遇，保留源码顺序")
	state.actors[player].location={"kind":"Following"}
	check(fresh._preview(state,"wait",{},player).code=="NotPartyLeader","同行占用先于不可用")
	state.battles["pending"]={"kind":"personal","phase":"planning","units":{}}
	check(fresh._preview(state,"wait",{},player).code=="BattlePending","战斗先于同行和遭遇")
	state=before.duplicate(true); state.encounters["pending"]={"status":"pending","participants":[player]}; state.actors[player].available=false
	check(fresh._preview(state,"wait",{},player).code=="EncounterPending","遭遇先于人物不可用")
	state=before.duplicate(true); state.actors[player].available=false; state.actors[player].action_id="busy"
	check(fresh._preview(state,"wait",{},player).code=="ActorUnavailable","不可用先于忙碌")
	state.actors[player].available=true
	check(fresh._preview(state,"wait",{},player).code=="ActorBusy","普通领域命令保留忙碌守卫")
	check(fresh.snapshot()==before,"预览夹具未污染真实世界")
	var invalid_state: Dictionary=before.duplicate(true); invalid_state.family={}
	check(not built.validators["family.state"].callback.call(invalid_state,catalog.data).ok,"注册的当前validator真实拒绝坏家族状态")

## 图先于目录装载，后续目录/Schema失败同样保留整套已接受注册和真实Callable。
func test_graph_startup(builtin, registry) -> void:
	var source: Array=builtin.descriptors(); var builtin_index: int=-1
	for index in source.size():
		if source[index].id=="builtin": builtin_index=index
	check(builtin_index>=0 and source[builtin_index].phases.size()==17,"真实组合根有17个执行块")
	var fresh=TestKernel.new(); fresh.source=source.duplicate(true)
	fresh.source[builtin_index].phases[0].before=["missing_phase"]
	check(fresh.new_game("missing","user://phase-missing-content.json").get("code")=="UnknownPhaseDependency" and fresh.snapshot().is_empty() and fresh.content().is_empty() and fresh._registry.is_empty(),"空世界坏图先于坏内容且不留下部分注册")
	fresh.source=source.duplicate(true)
	check(fresh.new_game().ok,"合法图世界启动")
	var state: Dictionary=fresh.snapshot(); var catalog: Dictionary=fresh.content(); var accepted: Dictionary=fresh._registry.duplicate(true)
	var callback: Callable=fresh._registry.phase_plan.hooks[0].run
	fresh.source[builtin_index].phases[0].run=sentinel
	fresh.source[builtin_index].phases[0].reads.append("context.changed")
	check(fresh._registry==accepted and fresh._registry.phase_plan.hooks[0].run==callback,"修改未来描述不改变已接受图和回调")
	fresh.source[builtin_index].phases[0].before=["missing_phase"]
	check(fresh.new_game().get("code")=="UnknownPhaseDependency" and fresh.snapshot()==state and fresh.content()==catalog and fresh._registry==accepted,"坏图保持旧state/catalog/registry/plan")
	fresh.source=source.duplicate(true)
	check(not fresh.new_game("missing","user://phase-missing-content.json").ok and fresh.snapshot()==state and fresh.content()==catalog and fresh._registry==accepted,"图编译后内容装载失败仍保持整个旧世界")
	var strict=load("res://core/strict_json.gd")
	var header: Dictionary=strict.parse_text(FileAccess.get_file_as_string("res://data/generated/two_stories.hmcatalog.json")).value
	header.catalog.inventories["inventory.shop"].capacity=1
	header.catalog_sha256=(JSON.stringify(header.catalog,"",true,true)+"\n").sha256_text()
	var path: String="user://phase-overcapacity-startup.json"
	var file=FileAccess.open(path,FileAccess.WRITE); file.store_string(JSON.stringify(header)); file.close()
	var loaded: Dictionary=load("res://core/content_catalog.gd").read_prepared(path)
	check(loaded.ok,"复用A1.2b店铺容量夹具，通过目录装载后在Schema失败")
	check(fresh.new_game("scenario.two_stories",path).get("code")=="OverCapacity" and fresh.snapshot()==state and fresh.content()==catalog and fresh._registry==accepted,"后续Schema失败保留旧图及全部世界")
	check(hits==0,"build和失败启动从未执行阶段run")

## 元数据坏引用、能力别名和无owner模块须在真实组合根被拒绝。
func test_metadata(builtin, registry) -> void:
	var metadata: Dictionary=load("res://core/strict_json.gd").parse_text(FileAccess.get_file_as_string(builtin.CONTRACT_PATH)).value
	var capabilities: Dictionary=JSON.parse_string(FileAccess.get_file_as_string(builtin.CAPABILITY_PATH))
	check(builtin.validate_metadata(metadata,capabilities).ok,"固定绑定与旧能力完全对应")
	var bad: Dictionary=metadata.duplicate(true); bad.capability_modules.world_factory="missing"
	check(not builtin.validate_metadata(bad,capabilities).ok,"过期语义别名目标拒绝")
	bad=metadata.duplicate(true); bad.capability_modules.erase("world_factory")
	check(not builtin.validate_metadata(bad,capabilities).ok,"缺旧能力映射拒绝")
	bad=metadata.duplicate(true)
	for module in bad.modules:
		if not module.validators.is_empty():
			module.validators[0].method="missing"
			break
	check(not builtin.validate_metadata(bad,capabilities).ok,"不能解析的validator引用拒绝")
	var descriptors: Array=builtin.descriptors()
	for module in descriptors:
		if module.id=="identity":
			module.owns[0].owner="missing"
			break
	rejected(registry,descriptors,"UnknownStateOwner","真实内建坏状态owner拒绝")
	var values=load("res://core/base/value_checks.gd")
	var schema=load("res://core/state_schema.gd")
	for value in [null, true, 0.5, -1, 0, 5, 2147483647, 2147483648]:
		check(values.whole(value)==schema.whole(value),"整数检查兼容转发 "+str(value))
	for value in [null, [], {}, {"id":1}, {"id":"x"}]:
		check(values.fields(value,{"id":TYPE_STRING})==schema.fields(value,{"id":TYPE_STRING}),"字段检查兼容转发")

## 缺实现为明确红灯；有实现后执行完整矩阵并保留真实引擎汇总。
func _run() -> void:
	if not ResourceLoader.exists("res://core/runtime/module_registry.gd"):
		check(false,"缺少可信模块注册器")
	else:
		var registry=load("res://core/runtime/module_registry.gd")
		var builtin=load("res://core/runtime/builtin_modules.gd")
		test_registry(registry)
		test_full_refusal_abi(registry)
		test_startup(builtin,registry); test_graph_startup(builtin,registry)
		test_metadata(builtin,registry)
	print("CHECKS=%d FAILURES=%d" % [checks,failures])
	quit(1 if failures else 0)
