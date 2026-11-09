## 注册状态校验的旧错误顺序、纯读、可扩展性和存读事务边界回归。
extends SceneTree
const Kernel=preload("res://core/world_kernel.gd")
const Schema=preload("res://core/state_schema.gd")
const Registry=preload("res://core/runtime/module_registry.gd")
const Builtin=preload("res://core/runtime/builtin_modules.gd")
const Codec=preload("res://core/save_codec.gd")
const Strict=preload("res://core/strict_json.gd")
const Family=preload("res://core/family_rules.gd")
const Road=preload("res://core/encounter_state.gd")
const P=preload("res://tests/policy_fixture.gd")
var checks: int=0
var failures: int=0

## 等待场景树初始化后执行独立的无界面回归。
func _init() -> void:
	call_deferred("run_checks")

## 保留断言标签，任意失败令进程非零退出。
func check(value: bool, label: String) -> bool:
	checks+=1
	if not value: failures+=1; printerr("FAIL: ",label)
	return value

## 基于旧Schema源码写定预期，不从新协调器产生期待码；保留人物和行动插入序。
func vectors(s: Dictionary) -> Array:
	var rows: Array=[]
	var actor: String=s.actors.keys()[0]; var second: String=s.actors.keys()[1]
	for key in ["actors","family","estates"]:
		var bad: Dictionary=s.duplicate(true); bad[key]=[]
		rows.append({"id":"root-"+key,"state":bad,"code":"InvalidWorldShape"})
	var bad: Dictionary=s.duplicate(true); bad.ruleset_id="old"; bad.tick=-1
	rows.append({"id":"version-before-counter","state":bad,"code":"IncompatibleContent"})
	bad=s.duplicate(true); bad.scenario_id="missing"; bad.tick=-1
	rows.append({"id":"scenario-before-counter","state":bad,"code":"InvalidScenario"})
	bad=s.duplicate(true); bad.tick=-1; bad.actors[actor].hp=-1
	rows.append({"id":"counter-before-actor","state":bad,"code":"InvalidCounter"})
	bad=s.duplicate(true); bad.actors[actor].hp=-1; bad.knowledge[actor]={"missing":{}}
	rows.append({"id":"actor-before-knowledge","state":bad,"code":"InvalidActorState"})
	bad=s.duplicate(true); bad.knowledge[actor]={"missing":{}}; bad.actors[second].hp=-1
	rows.append({"id":"first-knowledge-before-second-actor","state":bad,"code":"InvalidKnowledge"})
	bad=s.duplicate(true); bad.roles.values()[0].actor_id="missing"; bad.wallets.values()[0].balance=-1
	rows.append({"id":"roles-before-economy","state":bad,"code":"InvalidRoleAssignment"})
	bad=s.duplicate(true); bad.wallets.values()[0].balance=-1; bad.flags["missing"]=true
	rows.append({"id":"economy-before-flags","state":bad,"code":"InvalidWallet"})
	bad=s.duplicate(true); bad.flags["missing"]=true; bad.relations["bad"]={}
	rows.append({"id":"flags-before-relations","state":bad,"code":"InvalidFlag"})
	bad=s.duplicate(true); bad.relations["bad"]={}; bad.rng.world.seed="01"
	rows.append({"id":"relations-before-rng","state":bad,"code":"InvalidRelation"})
	bad=s.duplicate(true); bad.rng.world.seed="01"; bad.results["bad"]={}
	rows.append({"id":"rng-before-cache","state":bad,"code":"InvalidRandomState"})
	bad=s.duplicate(true); bad.results["bad"]={}; bad.history.append("action:2147483647")
	rows.append({"id":"cache-before-id-scan","state":bad,"code":"InvalidCachedResult"})
	bad=s.duplicate(true); bad.history.append("action:2147483647"); bad.battles=[]
	rows.append({"id":"id-scan-before-personal","state":bad,"code":"InstanceCounterCollision"})
	bad=s.duplicate(true); bad.battles=[]; bad.armies=[]
	rows.append({"id":"personal-before-military","state":bad,"code":"MissingCombatState"})
	bad=s.duplicate(true); bad.armies=[]; bad.social={}
	rows.append({"id":"military-before-social","state":bad,"code":"InvalidMilitaryState"})
	bad=s.duplicate(true); bad.family={}; bad.estates["unknown"]={}
	rows.append({"id":"family-header-before-estates","state":bad,"code":"InvalidFamilyState"})
	bad=s.duplicate(true); bad.player_actor_id="missing"; bad.family={}
	rows.append({"id":"identity-before-family","state":bad,"code":"InvalidPlayerIdentity"})
	bad=s.duplicate(true); bad.family.children={"missing":"bad"}; bad.tasks["bad"]={}
	rows.append({"id":"family-before-tasks","state":bad,"code":"InvalidFamilyChild"})
	bad=s.duplicate(true); bad.tasks["bad"]={}; bad.actions["bad"]={}
	rows.append({"id":"tasks-before-actions","state":bad,"code":"InvalidTask"})
	bad=s.duplicate(true); bad.actions["bad"]={}; bad.parties=[]
	rows.append({"id":"actions-before-parties","state":bad,"code":"InvalidAction"})
	bad=s.duplicate(true); bad.parties=[]; bad.encounters=[]
	rows.append({"id":"parties-before-road","state":bad,"code":"MissingParties"})
	bad=s.duplicate(true); bad.encounters={"bad":{}}; bad.itineraries=[]
	rows.append({"id":"itinerary-container-before-encounter","state":bad,"code":"MissingRoadState"})
	bad=s.duplicate(true); bad.encounters={"bad":{}}; bad.interactions[actor]={}
	rows.append({"id":"encounters-before-sessions","state":bad,"code":"UnsupportedEncounters"})
	bad=s.duplicate(true); bad.itineraries["missing"]={}; bad.interactions[actor]={}
	rows.append({"id":"itineraries-before-sessions","state":bad,"code":"UnexpectedItinerary"})
	bad=s.duplicate(true); bad.interactions[actor]={}; bad.events["bad"]={}
	rows.append({"id":"sessions-before-events","state":bad,"code":"InvalidInteractionSession"})
	bad=s.duplicate(true); bad.events["bad"]={}
	rows.append({"id":"bad-event","state":bad,"code":"InvalidSceneEvent"})
	bad=s.duplicate(true); bad.extra={"nested":[1,2]}; bad.actors[actor].extra=true; bad.facts["unowned"]={"kind":"future_unknown"}
	rows.append({"id":"open-fields-and-unreferenced-fact","state":bad,"code":"OK"})
	return rows

## 旧实现先记录固定向量，后续实现使用相同期待而不是刷新golden。
func test_vectors(k) -> Array:
	var results: Array=[]
	for row in vectors(k.snapshot()):
		var outcome: Dictionary=Schema.validate(row.state,k.content(),k._registry)
		var code: String=outcome.get("code","OK")
		check(code==row.code,row.id+" expected "+row.code+" got "+str(outcome))
		results.append({"id":row.id,"expected":row.code,"actual":outcome,"state":row.state})
	return results

## 常规测试不写入证据；仅明确捕获参数允许记录旧实现结果。
func run_checks() -> void:
	var k=Kernel.new()
	if not check(k.new_game().ok,"真实新局"): quit(1); return
	var rows: Array=test_vectors(k)
	for arg in OS.get_cmdline_user_args():
		if arg.begins_with("--record-old="):
			var file=FileAccess.open(arg.trim_prefix("--record-old="),FileAccess.WRITE)
			file.store_string(JSON.stringify({"engine":Engine.get_version_info(),"catalog":k.content(),"vectors":rows},"",true,true)); file.close()
	test_registration()
	test_order_and_results(k)
	test_purity(k)
	test_save_errors(k)
	test_extension()
	test_action_interleaving(k)
	test_leaf_facades(k)
	test_requirement_carrier(k)
	test_short_domain_saves()
	test_independent_contract(k)
	print("CHECKS=",checks," FAILURES=",failures)
	quit(0 if failures==0 else 1)

## 测试专用模块真实声明独立root和行动；初始缺省为零，不为生产增加初始化分支。
class Probe extends RefCounted:
	var state_calls: int=0
	var action_calls: int=0
	var started: bool=false
	var order: Array=[]
	var response: Variant={"ok":true}
	## 注入的独立状态和行动均由本模块声明，没有修改中央Schema。
	func descriptor() -> Dictionary:
		var refs: Array=[{"path":"game/tests/test_registered_state.gd","method":"validate_state"}]
		return {"id":"state_probe","version":"f10-architecture-1","requires":{},"optional_requires":{},"capabilities":[],"owns":[{"container":"probe_region","scope":"root","key":"","owner":"state_probe","schema_refs":refs},{"container":"actions","scope":"kind","key":"probe_work","owner":"state_probe","schema_refs":refs}],"commands":[{"kind":"probe_work","mode":"action","gate_id":"legacy_kernel","preview":preview,"apply":null}],"actions":[{"kind":"probe_work","prepare":prepare,"start":start,"step":step,"finish":finish,"cancel":Registry.refuse_cancel,"resume":Registry.refuse_resume,"interrupt_policy":{"cancel":"unsupported","resume":"unsupported"},"validate":validate_action}],"phases":[],"validators":[{"id":"probe.state","stage":"before_actions","order":150,"signature":"state,data","callback":validate_state}],"queries":[]}
	## 预览只返回准备参数，不改变新状态区或世界随机流。
	func preview(_s: Dictionary, _d: Dictionary, _actor: String, _kind: String, _params: Dictionary) -> Dictionary:
		return {"ok":true,"duration":2}
	## 真实行动需要完整参与者、时长和隔离载荷。
	func prepare(_s: Dictionary, _d: Dictionary, actor: String, _kind: String, params: Dictionary, _offer: Dictionary) -> Dictionary:
		return {"ok":true,"participants":[actor],"duration":2,"params":params.duplicate(true)}
	## 先对候选写入独立状态，验证失败必须回滚这一实际写入。
	func start(s: Dictionary, _d: Dictionary, action: Dictionary, _offer: Dictionary) -> Dictionary:
		started=true; s.probe_region={"value":action.params.get("value",1)}
		return {"ok":true}
	## 新行动通过既有运行器减拍，保持实际中间态供保存回归。
	func step(_s: Dictionary, _d: Dictionary, action: Dictionary, _context: Dictionary) -> Dictionary:
		action.remaining_ticks-=1
		return {"ok":true,"completed":action.remaining_ticks==0,"suspended":false}
	## 完成效果写入同一候选状态，不绕过Kernel最终校验。
	func finish(s: Dictionary, _d: Dictionary, _action: Dictionary, _tick: int) -> Dictionary:
		s.probe_region.value+=1
		return {"ok":true,"success":true}
	## 状态ABI严格二参，坏区域由模块自己拒绝。
	func validate_state(s: Dictionary, _d: Dictionary) -> Dictionary:
		state_calls+=1
		var region: Variant=s.get("probe_region",{"value":0})
		if not region is Dictionary or not region.get("value") is int or region.value<0: return {"ok":false,"code":"InvalidProbeState"}
		return {"ok":true}
	## 载荷ABI独立三参，必须在状态校验后按行动位置执行。
	func validate_action(_s: Dictionary, _d: Dictionary, action: Dictionary) -> Dictionary:
		action_calls+=1
		return {"ok":false,"code":"InvalidProbeAction"} if action.params.get("reject",false) else {"ok":true}
	## 包装每个真实状态叶记录顺序，仍由原Callable决定结果。
	func observe(s: Dictionary, d: Dictionary, id: String, inner: Callable) -> Dictionary:
		order.append(id)
		return inner.call(s,d)
	## 非字典、缺错误码和坏ok类型仅用于证明回调结果失败关闭。
	func malformed(_s: Dictionary, _d: Dictionary) -> Variant:
		state_calls+=1
		return response
	## 与三参载荷ABI严格分离的坏返回探针。
	func malformed_action(_s: Dictionary, _d: Dictionary, _a: Dictionary) -> Variant:
		action_calls+=1
		return response

## 仅在已有可信组合上添加测试域；所有启动、提交和恢复都是真实Kernel。
class Extended extends Kernel:
	var probe=Probe.new()
	## 首次创建仍由Factory产生原世界，模块自定义区域的缺省由自身validator允许。
	func _module_descriptors() -> Array:
		var rows: Array=super._module_descriptors()
		rows.append(probe.descriptor())
		return rows

## 查找语义模块不依赖发行描述的枚举次序。
func module_at(rows: Array, id: String) -> Dictionary:
	for row in rows:
		if row.id==id: return row
	return {}

## 按原行动kind选取载荷校验器，避免依赖当前数组顺序或误破坏另一条已注册命令桥。
func legacy_at(rows: Array, id: String, kind: String) -> Dictionary:
	for entry in module_at(rows,id).legacy_action_validators:
		if entry.kind==kind: return entry
	return {}

## 独立纯注册断言，坏描述绝不被静默过滤或修补。
func refused(rows: Array, code: String, label: String) -> void:
	var result: Dictionary=Registry.build(rows)
	check(result.get("code")==code and not result.ok,label+" got "+str(result.get("code","OK")))

## 正反向所有权、ABI、确定顺序和缺失回调在执行前完成检查。
func test_registration() -> void:
	var probe=Probe.new(); var rows: Array=Builtin.descriptors(); rows.append(probe.descriptor())
	var untouched: Array=rows.duplicate(true); var registered: Dictionary=Registry.build(rows)
	if not check(registered.ok,"扩展注册成功"): return
	check(rows==untouched and probe.state_calls==0 and probe.action_calls==0,"build零回调且描述不变")
	for i in 100:
		var shuffled: Array=rows.duplicate(true)
		for j in i%rows.size(): shuffled.append(shuffled.pop_front())
		if i%2: shuffled.reverse()
		check(Registry.build(shuffled)==registered,"描述乱序稳定 "+str(i))
	var bad: Array=rows.duplicate(true); module_at(bad,"state_probe").owns[0].owner="absent"
	refused(bad,"UnknownStateOwner","不存在owner")
	bad=rows.duplicate(true); module_at(bad,"state_probe").owns[0].owner="tasks"
	refused(bad,"MismatchedStateOwner","不能冒用owner")
	bad=rows.duplicate(true); module_at(bad,"state_probe").owns.append(module_at(bad,"state_probe").owns[0])
	refused(bad,"DuplicateStateOwner","重复独立区域")
	bad=rows.duplicate(true); module_at(bad,"identity").owns.pop_front()
	refused(bad,"MissingStateOwner","固定要求不随输入claim删除")
	bad=rows.duplicate(true); module_at(bad,"tasks").validators=[]
	refused(bad,"MissingStateValidator","删除既有state callback拒绝")
	for key in ["required_ownership","required_validators"]:
		bad=rows.duplicate(true); module_at(bad,"builtin").erase(key)
		refused(bad,"InvalidModuleDescriptor","缺组合要求 "+key)
		bad=rows.duplicate(true); module_at(bad,"builtin")[key]=[]
		refused(bad,"InvalidModuleDescriptor","空组合要求 "+key)
		bad=rows.duplicate(true); module_at(bad,"builtin")[key][0]={}
		refused(bad,"InvalidStateOwnership" if key=="required_ownership" else "InvalidStateValidator","坏组合要求 "+key)
	for key in ["callback","signature","stage","order"]:
		bad=rows.duplicate(true); module_at(bad,"state_probe").validators[0].erase(key)
		refused(bad,"InvalidStateValidator","状态描述缺 "+key)
	for value in [null,true,"0",0.5,-1]:
		bad=rows.duplicate(true); module_at(bad,"state_probe").validators[0].order=value
		refused(bad,"InvalidStateValidator","非严格次序 "+str(value))
	bad=rows.duplicate(true); module_at(bad,"state_probe").validators[0].stage="during_actions"
	refused(bad,"InvalidStateValidator","未知阶段")
	bad=rows.duplicate(true); module_at(bad,"state_probe").validators[0].signature="state,data,action"
	refused(bad,"InvalidStateValidator","混合ABI标志")
	bad=rows.duplicate(true); module_at(bad,"state_probe").validators[0].callback=probe.validate_action
	refused(bad,"InvalidStateValidator","实际三参不能伪装二参")
	bad=rows.duplicate(true); module_at(bad,"state_probe").validators[0].order=140
	refused(bad,"DuplicateValidationOrder","同段次序冲突")
	bad=rows.duplicate(true); module_at(bad,"state_probe").validators[0].id="tasks.state"
	refused(bad,"DuplicateModuleCallback","既有重复ID错误保持")
	bad=rows.duplicate(true); module_at(bad,"state_probe").actions[0].erase("validate")
	refused(bad,"InvalidActionDescriptor","完整生命周期缺validate")
	check(registered.commands.travel.get("compat_action")=="legacy_begin_action" and registered.commands.rescue.mode=="instant" and not registered.commands.rescue.has("compat_action") and not legacy_at(rows,"travel","rescue").is_empty(),"保持真实travel兼容桥，独立测试无兼容桥的必需救援载荷")
	bad=rows.duplicate(true); module_at(bad,"travel").legacy_action_validators.erase(legacy_at(bad,"travel","rescue"))
	refused(bad,"MissingActionValidator","已声明旅行救援缺载荷")
	bad=rows.duplicate(true); module_at(bad,"services").legacy_action_validators.append(module_at(bad,"services").legacy_action_validators[0])
	refused(bad,"DuplicateActionValidator","过渡载荷重复")
	bad=rows.duplicate(true); module_at(bad,"state_probe").legacy_action_validators=[{"kind":"probe_work","signature":"state,data,action","validate":probe.validate_action}]
	refused(bad,"DuplicateActionValidator","完整与过渡不能双校验")
	bad=rows.duplicate(true); module_at(bad,"travel").legacy_action_validators[0].validate=probe.validate_state
	refused(bad,"InvalidLegacyActionValidator","二参不能伪装过渡三参")
	bad=rows.duplicate(true); legacy_at(bad,"travel","rescue").kind="unclaimed"
	refused(bad,"MissingStateOwner","过渡载荷必须有明确归属")
	bad=rows.duplicate(true); var moved: Dictionary=legacy_at(bad,"travel","rescue"); module_at(bad,"travel").legacy_action_validators.erase(moved); module_at(bad,"services").legacy_action_validators.append(moved)
	refused(bad,"MismatchedStateOwner","载荷owner不得冒用已声明旅行")
	check(probe.state_calls==0 and probe.action_calls==0 and rows==untouched,"所有坏注册仍零回调无写入")
	var metadata: Dictionary=Strict.parse_text(FileAccess.get_file_as_string(Builtin.CONTRACT_PATH)).value
	var capability: Dictionary=Strict.parse_text(FileAccess.get_file_as_string(Builtin.CAPABILITY_PATH)).value
	module_at(metadata.modules,"identity").owns.pop_front()
	check(Builtin.validate_metadata(metadata,capability).get("code")=="MissingStateOwner","固定要求先于可变metadata缺claim")

## 注册计划只用于调用，模块也不能用非规范返回值伪造通过。
func test_order_and_results(k) -> void:
	var probe=Probe.new(); var rows: Array=Builtin.descriptors(); var expected: Array=[]
	var accepted: Dictionary=Registry.build(rows)
	for stage in ["before_actions","after_actions"]:
		for entry in accepted.validation_plan[stage]: expected.append(entry.id)
	for row in rows:
		for entry in row.validators: entry.callback=probe.observe.bind(entry.id,entry.callback)
	var observed: Dictionary=Registry.build(rows)
	check(observed.ok and Schema.validate(k.snapshot(),k.content(),observed)=={"ok":true} and probe.order==expected,"每个旧叶只调用一次并保持完整次序")
	probe.order.clear()
	var bad: Dictionary=k.snapshot(); bad.family={}
	check(Schema.validate(bad,k.content(),observed).get("code")=="InvalidFamilyState" and probe.order[-1]=="family.state" and not "estates.state" in probe.order,"首错停止不重复Identity或提前Estate")
	for value in [null,true,{"ok":1},{"ok":false},{"ok":false,"code":1},{"ok":false,"code":""}]:
		probe.response=value; rows=Builtin.descriptors(); var extension: Dictionary=probe.descriptor(); extension.validators[0].callback=probe.malformed; rows.append(extension)
		var result: Dictionary=Schema.validate(k.snapshot(),k.content(),Registry.build(rows))
		check(result.get("code")=="InvalidStateValidatorResult","坏状态回调返回失败关闭 "+str(value))
	rows=Builtin.descriptors(); var extension: Dictionary=probe.descriptor(); extension.validators[0].callback=probe.malformed; rows.append(extension); probe.response={"ok":false,"code":"FirstProbeFailure","reason":"retained"}
	check(Schema.validate(k.snapshot(),k.content(),Registry.build(rows))==probe.response,"领域失败结果完整保留")
	check(Schema.validate({},{} ,{}).get("code")=="InvalidModuleRegistry","显式空注册先于目录失败")
	check(Schema.validate({},{}).get("code")=="NoCatalog","默认组合后保留目录首错")
	var broken: Dictionary=k._registry.duplicate(true); broken.validators.erase("tasks.state"); broken.validation_plan.before_actions.remove_at(13)
	check(Schema.validate(k.snapshot(),k.content(),broken).get("code")=="InvalidModuleRegistry","同时删除计划和callback仍被固定要求拒绝")

## 完整状态、目录与已接受注册深副本共同证明100次validate和preview都纯读。
func test_purity(k) -> void:
	var s: Dictionary=k.snapshot(); var c: Dictionary=k.content(); var r: Dictionary=k._registry.duplicate(true)
	var before: Array=[s.duplicate(true),c.duplicate(true),r.duplicate(true)]
	for i in 100:
		check(Schema.validate(s,c,r).ok,"纯校验 "+str(i))
		check([s,c,r]==before,"全部输入纯读 "+str(i))
	var expected: Dictionary=k.preview("wait")
	for i in 100:
		check(k.preview("wait")==expected and k.snapshot()==s and k.content()==c and k._registry==r,"全部世界和注册预览纯读 "+str(i))
	for entry in r.validators.values():
		var status: Variant=entry.callback.call(s,c.data)
		check(status is Dictionary and status.get("ok",false),"真实二参执行 "+entry.id)
	check([s,c,r]==before,"逐叶二参调用不改任何输入")

## 先重新计算合法封装校验和，确保拒绝来自状态而非字节损坏。
func wrapped(bad: Dictionary, valid: Dictionary, catalog: Dictionary) -> String:
	var header: Dictionary=Strict.parse_text(Codec.encode(valid,1,"negative",catalog),Codec.MAX_BYTES).value
	header.payload_json=JSON.stringify(bad,"",true,true)
	header.payload_length=header.payload_json.to_utf8_buffer().size(); header.payload_sha256=header.payload_json.sha256_text()
	for key in ["ruleset_id","content_version","content_id","content_digest","engine_build","rng_backend"]: header[key]=bad[key]
	return JSON.stringify(header,"",true,true)

## 同一固定错误必须经过显式协调器、两种门面、读档及restore；失败保留整个旧世界。
func test_save_errors(k) -> void:
	var before: Dictionary=k.snapshot(); var catalog: Dictionary=k.content(); var registry: Dictionary=k._registry.duplicate(true)
	var validation=load("res://core/runtime/world_validation.gd")
	for row in vectors(before):
		if row.code=="OK" or row.id=="version-before-counter": continue
		check(validation.validate(row.state,catalog,registry).get("code")==row.code,row.id+" 显式协调器旧码")
		check(Schema.validate(row.state,catalog).get("code")==row.code,row.id+" 默认门面旧码")
		check(Codec.encode(row.state,1,"invalid",catalog).is_empty(),row.id+" 坏状态不可编码")
		check(Codec.decode(wrapped(row.state,before,catalog),catalog).get("code")==row.code,row.id+" 有效封装解码旧码")
		check(k.restore(row.state).get("code")==row.code and k.snapshot()==before and k.content()==catalog and k._registry==registry,row.id+" restore全状态回滚")

## 新域由唯一描述接入正常命令、状态与行动；坏start/restore不留下候选资产或缓存。
func test_extension() -> void:
	var k=Extended.new()
	if not check(k.new_game().ok,"新域通过真实Kernel启动"): return
	check(k.probe.state_calls==1 and k.probe.action_calls==0,"启动执行state但不误调用action")
	var before: Dictionary=k.snapshot(); var catalog: Dictionary=k.content(); var registry: Dictionary=k._registry.duplicate(true)
	var bad: Dictionary=before.duplicate(true); bad.probe_region={"value":-1}
	check(k.restore(bad).get("code")=="InvalidProbeState" and k.snapshot()==before,"新root坏恢复无中央分支")
	var bad_command: Dictionary=P.command(k,"probe_work",{"value":-1})
	check(k.submit(bad_command).get("code")=="InvalidProbeState" and k.probe.started,"真实候选写入后state拒绝")
	check(k.snapshot()==before and k.content()==catalog and k._registry==registry and k.probe.action_calls==0,"state首错先于action且完整rollback")
	check(k.submit(P.command(k,"probe_work",{"value":1,"reject":true})).get("code")=="InvalidProbeAction" and k.snapshot()==before and k.probe.action_calls==1,"载荷独立执行并保留全候选回滚")
	var command: Dictionary=P.command(k,"probe_work",{"value":2})
	check(k.submit(command).ok and k.snapshot().probe_region.value==2 and k.snapshot().actions.size()==1,"新命令行动及状态真实提交")
	var current: Dictionary=k.snapshot(); var accepted: Dictionary=k._registry.duplicate(true)
	for value in [true,{"ok":false},{"ok":1}]:
		k.probe.response=value
		var broken: Dictionary=accepted.duplicate(true); broken.actions.probe_work.validate=k.probe.malformed_action
		check(Schema.validate(current,catalog,broken).get("code")=="InvalidActionValidatorResult","坏载荷返回失败关闭")
	var restored=Extended.new(); check(restored.new_game().ok and restored.restore(current).ok,"同描述新世界恢复自定义root和行动")
	var next: Dictionary=P.command(k,"advance")
	var outcome: Dictionary=k.submit(next)
	check(outcome.ok and restored.submit(next)==outcome and restored.snapshot()==k.snapshot(),"新域同命令恢复后结果精确一致")
	var after: Dictionary=k.snapshot()
	check(restored.submit(next)==outcome and k.submit(next)==outcome and restored.snapshot()==after and k.snapshot()==after,"新域重复请求不重复推进或写入")
	check(k.advance_one_tick().ok and k.snapshot().probe_region.value==3 and k.snapshot().actions.is_empty(),"新行动通过真实phase完成释放")
	check(k.probe.state_calls>k.probe.action_calls and k.probe.action_calls>0,"全世界与载荷调用独立可观测")

## 两个行动的载荷和占用按原字典序交错，不按kind或阶段收集后再报错。
func test_action_interleaving(k) -> void:
	var s: Dictionary=k.snapshot(); var c: Dictionary=k.content(); var people: Array=s.actors.keys()
	var first: String="action:"+str(s.next_instance_id); var second: String="action:"+str(s.next_instance_id+1)
	s.next_instance_id+=2
	for i in 2:
		var id: String=first if i==0 else second
		s.actions[id]={"id":id,"kind":"wait","actor_id":people[i],"participants":[people[i]],"remaining_ticks":1,"start_tick":s.tick,"params":{}}
		s.actors[people[i]].action_id=id
	check(Schema.validate(s,c,k._registry).ok,"两个不同人物等待合法")
	var bad: Dictionary=s.duplicate(true); bad.actions[first].remaining_ticks=2; bad.actions[second]={}
	check(Schema.validate(bad,c,k._registry).get("code")=="InvalidWaitAction","首行动payload先于次行动envelope")
	bad=s.duplicate(true); bad.actions[first].participants.append(people[0]); bad.actions[second].remaining_ticks=2
	check(Schema.validate(bad,c,k._registry).get("code")=="InvalidOccupation","首行动参与者先于次行动payload")
	bad=s.duplicate(true); bad.actions[first].remaining_ticks=2; bad.actions[first].participants.append(people[0])
	check(Schema.validate(bad,c,k._registry).get("code")=="InvalidWaitAction","同一行动payload先于occupation")
	bad=s.duplicate(true); bad.actions[first].kind="future_unknown"
	check(Schema.validate(bad,c,k._registry).get("code")=="InvalidActionKind","未知行动仍拒绝，不因未知历史fact可接受而放开")

## 机械拆分保留公开叶门面与旧的条件庄园检查，成功附加详情完全相同。
func test_leaf_facades(k) -> void:
	var s: Dictionary=k.snapshot(); var d: Dictionary=k.content().data
	check(Family.validate_state(s,d)==Family.validate_tail(s,d),"完整家族成功结果含player及estate owner")
	var old_shaped: Dictionary=s.duplicate(true); old_shaped.erase("estates")
	var no_estates: Dictionary=d.duplicate(true); no_estates.estates={}
	check(Family.validate_state(old_shaped,no_estates).ok,"直接Family旧式调用仍条件性跳过空庄园")
	var bad: Dictionary=s.duplicate(true); bad.family={}; bad.estates={"unknown":{}}
	check(Family.validate_state(bad,d).get("code")=="InvalidFamilyState","Family公开入口仍header先于Estate")
	check(Road.validate(s,d)==Road.validate_itineraries(s,d) and Road.validate_encounters(s,d).ok,"公开Road仍两阶段组合")
	bad=s.duplicate(true); bad.itineraries=[]; bad.encounters={"bad":{}}
	check(Road.validate(bad,d).get("code")=="MissingRoadState" and Road.validate_encounters(bad,d).get("code")=="MissingRoadState","会面叶保留itinerary容器最早错误")

## 只替换描述来源，观察真实new_game在完整验证缺失时是否保持原世界。
class Configured extends Kernel:
	var source: Array=[]
	## 测试允许构造仅能用于行动运行器的泛型注册，不给生产添加兜底。
	func _module_descriptors() -> Array:
		return source

## 泛型注册可构建不等于全世界可验证；整个要求载体删除也不能绕过领域检查。
func test_requirement_carrier(k) -> void:
	var minimal: Dictionary={"id":"minimal","version":"f10-architecture-1","requires":{},"optional_requires":{},"capabilities":[],"owns":[],"commands":[],"actions":[],"phases":[],"validators":[],"queries":[]}
	var generic: Dictionary=Registry.build([minimal])
	check(generic.ok and generic.validation_plan.required.is_empty(),"保留泛型最小build用于独立begin夹具")
	check(Schema.validate(k.snapshot(),k.content(),generic).get("code")=="InvalidModuleRegistry","泛型注册不能跳过完整世界领域验证")
	var configured=Configured.new(); configured.source=[minimal]
	check(configured.new_game().get("code")=="InvalidModuleRegistry" and configured.snapshot().is_empty() and configured.content().is_empty() and configured._registry.is_empty(),"真实启动拒绝缺完整composition且不提交半个世界")
	configured.source=Builtin.descriptors(); check(configured.new_game().ok,"真实完整composition启动")
	var before: Array=[configured.snapshot(),configured.content(),configured._registry.duplicate(true)]
	var missing: Array=[]
	for row in Builtin.descriptors():
		if row.id!="builtin": missing.append(row)
	check(Registry.build(missing).ok,"移除整个要求载体仍可作泛型注册")
	configured.source=missing
	check(configured.new_game().get("code")=="InvalidModuleRegistry" and [configured.snapshot(),configured.content(),configured._registry]==before,"移除整个载体不能使真实世界略过领域且保留旧世界")
	var no_requirements: Array=Builtin.descriptors()
	module_at(no_requirements,"builtin").erase("required_ownership"); module_at(no_requirements,"builtin").erase("required_validators")
	check(Registry.build(no_requirements).ok and Schema.validate(k.snapshot(),k.content(),Registry.build(no_requirements)).get("code")=="InvalidModuleRegistry","同时删除两项要求也不能成为全世界validation plan")

## 代表性真实中间态通过全部存读入口使用同一个固定领域错误码。
func reject_domain(k, bad: Dictionary, code: String, label: String) -> void:
	var before: Dictionary=k.snapshot(); var c: Dictionary=k.content(); var r: Dictionary=k._registry.duplicate(true)
	var validation=load("res://core/runtime/world_validation.gd")
	check(validation.validate(bad,c,r).get("code")==code and Schema.validate(bad,c,r).get("code")==code and Schema.validate(bad,c).get("code")==code,label+" 三种完整验证原码")
	check(Codec.encode(bad,1,"invalid",c).is_empty() and Codec.decode(wrapped(bad,before,c),c).get("code")==code,label+" 合法封装拒绝且不编码坏状态")
	check(k.restore(bad).get("code")==code and k.snapshot()==before and k.content()==c and k._registry==r,label+" 拒绝恢复保留整个世界")

## 真实新Kernel接收字节存档，同一后继与重复请求都比较全部字段。
func saved_next(k, other, label: String, kind: String="advance") -> void:
	var saved: Dictionary=k.snapshot(); var decoded: Dictionary=Codec.decode(Codec.encode(saved,1,"short-"+label,k.content()),k.content())
	if not check(decoded.ok and decoded.state==saved and other.restore(decoded.state).ok and other.snapshot()==saved,label+" 全量保存恢复"): return
	var command: Dictionary=P.command(k,kind); var result: Dictionary=k.submit(command)
	check(other.submit(command)==result and other.snapshot()==k.snapshot(),label+" 同一后继命令")
	var before: Dictionary=k.snapshot()
	check(other.submit(command)==k.submit(command) and k.snapshot()==before and other.snapshot()==before,label+" 重复请求无新消费和ID")

## 只推进收养两拍、休整一拍和道路两拍；不用长家族重放替代局部存档边界。
func test_short_domain_saves() -> void:
	var f=load("res://tests/companion_fixture.gd")
	var family=P.fresh(true)
	check(f.send(family,"move_local",{"place_id":"west.estate"}).ok and f.send(family,"family_adopt",{"display_name":"阿宁","context":f.context(family,"role.estate_steward")}).ok,"短窗真实收养手续")
	var s: Dictionary=family.snapshot(); var id: String=s.actors[s.player_actor_id].action_id
	var bad: Dictionary=s.duplicate(true); bad.actions[id].params={}
	reject_domain(family,bad,"InvalidFamilyAction","收养载荷")
	saved_next(family,P.fresh(true),"family-adoption")
	check(family.advance_one_tick().ok,"收养实际完工")
	s=family.snapshot(); var child: String=s.family.children.keys()[0]
	bad=s.duplicate(true); bad.actors[child].display_name="新名字"
	reject_domain(family,bad,"ActorCreationMismatch","已出生身份")
	check(f.send(family,"long_rest",{"estate_id":"estate.riverside","duration":2}).ok,"短窗开始真实长期休整")
	s=family.snapshot(); id=s.actors[s.player_actor_id].action_id; bad=s.duplicate(true); bad.actions[id].params={}
	reject_domain(family,bad,"RestClockMismatch","休整载荷")
	saved_next(family,P.fresh(true),"long-rest")
	var road=f.fresh(true); check(f.send(road,"travel",{"destination":"village.east"}).ok,"真实道路行程开始")
	s=road.snapshot(); id=s.actors[s.player_actor_id].action_id; bad=s.duplicate(true); bad.actions[id].params.path=[]
	reject_domain(road,bad,"InvalidTravelAction","旅行载荷")
	saved_next(road,f.fresh(true),"travel")
	check(road.advance_one_tick().ok and not road.snapshot().encounters.is_empty(),"两拍产生真实道路会面")
	s=road.snapshot(); bad=s.duplicate(true); bad.encounters.values()[0].leaders=[]
	reject_domain(road,bad,"InvalidEncounterMembers","会面成员")
	saved_next(road,f.fresh(true),"road-encounter")
	var military=P.fresh(true); s=military.snapshot(); var data: Dictionary=military.content().data
	var public_wallet: String=data.strongholds.values()[0].public_wallet_ids[0]
	bad=s.duplicate(true); bad.wallets[public_wallet].owner={"kind":"actor","id":s.player_actor_id}
	reject_domain(military,bad,"RequiredWalletMissing","公库不能私有化")
	saved_next(military,P.fresh(true),"military-public-assets","wait")

## 固定完整合同必须独立于输入的自证要求；同步删掉声明和callback仍应失败。
func test_independent_contract(k) -> void:
	var bad: Dictionary=k.snapshot(); bad.tasks["bad"]={}
	check(Schema.validate(bad,k.content()).get("code")=="InvalidTask","默认完整门保留坏任务原码")
	var partial: Array=Builtin.descriptors(); module_at(partial,"tasks").validators=[]
	var requirements: Array=module_at(partial,"builtin").required_validators
	for index in range(requirements.size()-1,-1,-1):
		if requirements[index].id=="tasks.state": requirements.remove_at(index)
	check(Registry.build(partial).get("code")=="MissingStateValidator","同步删除callback与自证要求仍在正式build拒绝")
	var configured=Configured.new(); configured.source=partial
	check(configured.new_game().get("code")=="MissingStateValidator" and configured.snapshot().is_empty(),"真实Kernel不能使用自减完整要求")
	var broken: Dictionary=k._registry.duplicate(true)
	broken.validators.erase("tasks.state")
	for index in range(broken.validation_plan.before_actions.size()-1,-1,-1):
		if broken.validation_plan.before_actions[index].id=="tasks.state": broken.validation_plan.before_actions.remove_at(index)
	for index in range(broken.validation_plan.required.size()-1,-1,-1):
		if broken.validation_plan.required[index].id=="tasks.state": broken.validation_plan.required.remove_at(index)
	check(Schema.validate(bad,k.content(),broken).get("code")=="InvalidModuleRegistry","显式注册同时删实际叶与要求不能放过坏任务")
	var probe=Probe.new(); var custom: Dictionary=probe.descriptor()
	custom.commands=[]; custom.actions=[]; custom.owns.pop_back()
	custom.validators[0].callback=probe.malformed; probe.response={"ok":true}
	custom.required_ownership=custom.owns.duplicate(true)
	custom.required_validators=[{"id":"probe.state","owner":"state_probe","stage":"before_actions","order":150}]
	var generic: Dictionary=Registry.build([custom])
	check(generic.ok,"泛型自声明要求可用于独立注册单测")
	check(Schema.validate(bad,k.content(),generic).get("code")=="InvalidModuleRegistry","自封一条noop完整要求不能代替固定世界门")
	configured=Configured.new(); configured.source=[custom]
	check(configured.new_game().get("code")=="InvalidModuleRegistry" and configured.snapshot().is_empty() and configured.restore(bad).get("code")=="InvalidModuleRegistry","generic noop不能启动或恢复真实世界")
	broken=k._registry.duplicate(true); broken.modules.erase("builtin")
	check(Schema.validate(bad,k.content(),broken).get("code")=="InvalidModuleRegistry","移除组合身份不能借剩余自证要求成为完整注册")
	check(probe.state_calls==0,"假完整generic在任何noop callback执行前拒绝")
	var validation=load("res://core/runtime/world_validation.gd")
	for requirement in validation.required_validators():
		partial=Builtin.descriptors()
		var owner: Dictionary=module_at(partial,requirement.owner)
		for index in range(owner.validators.size()-1,-1,-1):
			if owner.validators[index].id==requirement.id: owner.validators.remove_at(index)
		requirements=module_at(partial,"builtin").required_validators
		for index in range(requirements.size()-1,-1,-1):
			if requirements[index].id==requirement.id: requirements.remove_at(index)
		check(Registry.build(partial).get("code")=="MissingStateValidator","独立固定合同覆盖每个叶 "+requirement.id)
	for original in Builtin.descriptors():
		if original.owns.is_empty(): continue
		partial=Builtin.descriptors(); var claim: Dictionary=module_at(partial,original.id).owns.pop_front()
		requirements=module_at(partial,"builtin").required_ownership
		for index in range(requirements.size()-1,-1,-1):
			var expected: Dictionary=requirements[index]
			if expected.container==claim.container and expected.scope==claim.scope and expected.key==claim.key: requirements.remove_at(index)
		check(Registry.build(partial).get("code")=="MissingStateOwner","独立拥有权要求覆盖每个实际owner "+original.id)
