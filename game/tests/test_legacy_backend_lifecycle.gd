## 原7652公共生命周期与读取的实际oracle；迁位仅换拥有者，保持首错、深副本、动态hook与三元组安装。
extends "res://tests/test_legacy_backend_ownership.gd"
const Backend=preload("res://core/backends/legacy_world_backend.gd")
const OLD_KERNEL_SHA="779d5f974349973b0be07098a4357958a71eb5762cdefc4a66bcb52ccf218fef"
const METHODS=["new_game","prepare_restore_registry","restore","restore_with_catalog","snapshot","content","save_context","preview"]

## 只修改受信描述的一个明确前置条件，正常部分仍继承真实最后validator与领域hook。
class LifecycleKernel extends HookKernel:
	var descriptor_mode: String=""
	var preview_actor: String=""
	var preview_state_alias: bool=false
	## 不调用任何规则来编造描述；用于固定描述失败先于内容读取的原顺序。
	func _module_descriptors() -> Array:
		var modules: Array=super._module_descriptors()
		if descriptor_mode=="empty":return []
		if descriptor_mode=="missing_wait":
			for module in modules:
				for index in range(module.commands.size()-1,-1,-1):
					if module.commands[index].kind=="wait":module.commands.remove_at(index)
		return modules
	## 记录动态callback拿到的真实actor和同一已提交state，不改报价或建立第二份状态。
	func _preview(current: Dictionary, kind: String, params: Dictionary, actor_id: String) -> Dictionary:
		preview_actor=actor_id;preview_state_alias=is_same(current,_state)
		return super._preview(current,kind,params,actor_id)

## 仅验证Kernel委托接线的测试拒绝器；所有生命周期均返回明确故障，绝不伪装真实世界成功。
class BoundaryBackend extends Backend:
	var seen: Array=[]
	## descriptor Callable只检查合法性，不能保存到测试成员形成持有环。
	func new_game(descriptors: Callable, _scenario: String="scenario.two_stories", _path: String="res://data/generated/two_stories.hmcatalog.json", _draft: Dictionary={}, _actor: String="") -> Dictionary:
		seen.append(["new_game",descriptors.is_valid()]);return {"ok":false,"code":"BackendNewGameProbe"}
	## 冷注册委托只返回测试拒绝码，不造一个伪注册成功值。
	func prepare_restore_registry(descriptors: Callable) -> Dictionary:
		seen.append(["prepare_restore_registry",descriptors.is_valid()]);return {"ok":false,"code":"BackendPrepareProbe"}
	## 恢复接线以明确拒绝验证，不替代真实Schema用例。
	func restore(_saved: Dictionary) -> Dictionary:
		seen.append(["restore",true]);return {"ok":false,"code":"BackendRestoreProbe"}
	## 三元组恢复接线不消费输入或安装任何值。
	func restore_with_catalog(_saved: Dictionary, _catalog_value: Dictionary, _registry_value: Variant=null) -> Dictionary:
		seen.append(["restore_with_catalog",true]);return {"ok":false,"code":"BackendCatalogProbe"}
	## 读取仍返回所持真实值副本，附加的计数只证明调用经过该所有者。
	func snapshot() -> Dictionary:seen.append(["snapshot",true]);return state.duplicate(true)
	## 目录读取不混入探针字段或改变真实目录身份。
	func content() -> Dictionary:seen.append(["content",true]);return catalog.duplicate(true)
	## 保存上下文只组合当前真实三元组，不运行Codec或物理存储。
	func save_context() -> Dictionary:seen.append(["save_context",true]);return {"state":state.duplicate(true),"catalog":catalog.duplicate(true),"registry":registry.duplicate(true)}
	## preview callback仅临时检验，明确故障不假造玩家报价。
	func preview(preview_hook: Callable, _kind: String, _params: Dictionary={}, _actor: String="") -> Dictionary:
		seen.append(["preview",preview_hook.is_valid()]);return {"ok":false,"code":"BackendPreviewProbe"}

## 正常开局与所有事务仍用原实际Kernel，不使用接线拒绝器生成世界。
func life_open() -> LifecycleKernel:
	var k=LifecycleKernel.new();kernels.append(k)
	check(k.new_game(scenario_id,content_path).ok,"真实7652同内容生命周期开局")
	return k

## 空世界先NoWorld且不触发_preview，默认actor与revision仍由原公开入口处理。
func preview_contract() -> void:
	var empty=LifecycleKernel.new();kernels.append(empty)
	check(empty.preview("wait",{},"missing")=={"ok":false,"code":"NoWorld"} and empty.calls.preview==0,"空世界preview不追加revision或调用子类hook")
	check(empty.snapshot()=={} and empty.content()=={} and empty.save_context()=={"state":{},"catalog":{},"registry":{}},"空世界三种读取保持原空值形状")
	capture("empty_reads_preview",empty,empty.preview("wait"))
	var k=life_open();var before: Dictionary=k.save_context();var params: Dictionary={"unused":[1,2]};var params_before: Dictionary=params.duplicate(true)
	var quote: Dictionary=k.preview("wait",params)
	check(quote.ok and quote.revision==before.state.revision and k.preview_actor==before.state.player_actor_id and k.preview_state_alias,"默认actor/revision与子类state别名正确")
	check(k.preview("wait",params,before.state.player_actor_id)==quote and params==params_before and k.save_context()==before,"显式同actor报价一致且输入不变")
	var unknown: Dictionary=k.preview("wait",{},"missing")
	check(unknown.get("code")=="UnknownActor" and unknown.revision==before.state.revision,"非空世界失败仍附原revision")
	k.mode="preview";var rejected: Dictionary=k.preview("wait");k.mode=""
	check(rejected.get("code")=="InjectedPreview" and rejected.revision==before.state.revision,"公开preview仍动态调用当前子类")
	retained(k,before,[k._state,k._catalog,k._registry],"preview只读");capture("preview_actor_revision",k,rejected)

## 冷注册失败/成功不安装三元组；new_game每个原前置错误均保持已有世界。
func startup_and_registry() -> void:
	var k=life_open();check(k.submit(command(k)).ok,"生命周期前保留真实已开始行动")
	var before: Dictionary=k.save_context();var refs: Array=[k._state,k._catalog,k._registry]
	k.descriptor_mode="empty"
	var result: Dictionary=k.new_game("missing", "res://backend-lifecycle-missing.json")
	check(result.get("code")=="InvalidModuleDescriptor","描述构建失败先于内容缺失")
	retained(k,before,refs,"描述失败new_game");capture("new_game_descriptor_first",k,result)
	result=k.prepare_restore_registry();check(result.get("code")=="InvalidModuleDescriptor","冷恢复也动态采用描述hook")
	retained(k,before,refs,"描述失败prepare");capture("prepare_descriptor_failure",k,result)
	k.descriptor_mode="missing_wait";result=k.new_game("missing","res://backend-lifecycle-missing.json")
	check(result.get("code")=="MissingBuiltinCommand","正式command合同先于读取缺内容")
	retained(k,before,refs,"命令合同失败");capture("new_game_contract_first",k,result)
	k.descriptor_mode=""
	result=k.new_game("missing","res://backend-lifecycle-missing.json")
	check(result.get("code")=="MissingContent","内容缺失先于未知scenario")
	retained(k,before,refs,"内容缺失");capture("new_game_content_first",k,result)
	result=k.new_game("missing",content_path);check(result.get("code")=="UnknownScenario","有效内容才检查scenario")
	retained(k,before,refs,"未知scenario");capture("new_game_scenario_failure",k,result)
	k.terminal_failure=true;result=k.new_game(scenario_id,content_path);k.terminal_failure=false
	check(result.get("code")=="InjectedTerminalValidator" and k.observed.tick==0,"新世界真实最后validator完成后故障")
	retained(k,before,refs,"new_game尾失败");capture("new_game_terminal_failure",k,result)
	var prepared: Dictionary=k.prepare_restore_registry()
	check(prepared.ok and not prepared.registry.is_empty() and not is_same(prepared.registry,k._registry),"冷注册只返回独立受信registry")
	retained(k,before,refs,"冷注册成功仍不安装")
	prepared.registry.modules.clear();check(k.save_context()==before,"修改冷注册返回副本不污染已装registry")
	capture("prepare_success_unpublished",k)
	result=k.new_game(scenario_id,content_path,{},before.state.player_actor_id)
	check(result.ok and k.snapshot().tick==0 and k.snapshot().actions.is_empty(),"完整原参数成功重建世界")
	check(not is_same(k._state,refs[0]) and not is_same(k._catalog,refs[1]) and not is_same(k._registry,refs[2]),"成功new_game一次安装三个新容器")
	capture("new_game_success",k,result)

## 当前规则内存恢复只验原顺序；拒绝与成功都检查三元组值/引用及调用者副本。
func restoration() -> void:
	var k=life_open();check(k.submit(command(k)).ok,"恢复前已有真实行动")
	var donor=life_open();check(donor.submit(command(donor)).ok and donor.submit(command(donor,"advance")).ok,"另一个正常世界提供当前规则输入")
	var before: Dictionary=k.save_context();var refs: Array=[k._state,k._catalog,k._registry];var supplied: Dictionary=donor.save_context()
	var result: Dictionary=k.restore_with_catalog({}, {}, {})
	check(result.get("code")=="InvalidModuleRegistry","冷恢复先拒registry，未读取坏state/catalog")
	retained(k,before,refs,"registry首错");capture("restore_registry_first",k,result)
	var invalid: Dictionary=supplied.state.duplicate(true);invalid.actors[invalid.player_actor_id].fatigue=-1;invalid.engine_build="old-engine"
	result=k.restore(invalid)
	check(result.get("code")=="InvalidActorState","真实Schema错误先于引擎兼容守卫")
	retained(k,before,refs,"restore Schema首错");capture("restore_schema_first",k,result)
	k.terminal_failure=true;result=k.restore(supplied.state);k.terminal_failure=false
	check(result.get("code")=="InjectedTerminalValidator","restore真实尾validator失败")
	retained(k,before,refs,"restore尾失败");capture("restore_terminal_failure",k,result)
	var wrong_engine: Dictionary=supplied.state.duplicate(true);wrong_engine.engine_build="old-engine";wrong_engine.rng_backend="godot:old-engine"
	check(Schema.validate(wrong_engine,supplied.catalog,supplied.registry).ok,"引擎尾失败输入先通过原Schema")
	result=k.restore(wrong_engine);check(result.get("code")=="IncompatibleEngine","restore原引擎尾拒绝")
	retained(k,before,refs,"restore engine");capture("restore_engine_failure",k,result)
	donor.terminal_failure=true;result=k.restore_with_catalog(supplied.state,supplied.catalog,supplied.registry);donor.terminal_failure=false
	check(result.get("code")=="InjectedTerminalValidator","冷恢复真实传入registry尾拒绝")
	retained(k,before,refs,"restore_with_catalog尾失败");capture("restore_catalog_terminal_failure",k,result)
	result=k.restore_with_catalog(wrong_engine,supplied.catalog,supplied.registry)
	check(result.get("code")=="IncompatibleEngine","冷恢复Schema通过后保原engine拒绝")
	retained(k,before,refs,"restore_with_catalog engine");capture("restore_catalog_engine_failure",k,result)
	result=k.restore(supplied.state)
	check(result.ok and is_same(k._catalog,refs[1]) and is_same(k._registry,refs[2]) and not is_same(k._state,refs[0]),"restore只安装新的深state，目录和注册原引用保留")
	var installed: Dictionary=k.save_context();var original_input: Dictionary=supplied.state.duplicate(true);supplied.state.wallets.clear();refs[0].actors[before.state.player_actor_id].fatigue=91
	check(k.save_context()==installed,"restore深副本不连输入或旧state alias")
	supplied.state=original_input;capture("restore_success",k,result)
	result=k.restore_with_catalog(supplied.state,supplied.catalog,supplied.registry)
	check(result.ok and not is_same(k._state,supplied.state) and not is_same(k._catalog,supplied.catalog) and not is_same(k._registry,supplied.registry),"冷恢复成功将三个深副本安装")
	installed=k.save_context();supplied.state.actors.clear();supplied.catalog.data.actors.clear();supplied.registry.modules.clear()
	check(k.save_context()==installed,"冷恢复不保存外部三元组可变引用")
	capture("restore_catalog_success",k,result)

## 公开读取需真正转到backend；拒绝器只证明委托，不参与行为oracle或成功事务证明。
func delegation() -> void:
	var k=life_open();var saved: Dictionary=k.save_context();var probe=BoundaryBackend.new();probe.publish(saved.state,saved.catalog,saved.registry);k._backend=probe
	check(k.new_game(scenario_id,content_path).get("code")=="BackendNewGameProbe","Kernel new_game交给backend")
	check(k.prepare_restore_registry().get("code")=="BackendPrepareProbe","Kernel冷注册交给backend")
	check(k.restore(saved.state).get("code")=="BackendRestoreProbe","Kernel restore交给backend")
	check(k.restore_with_catalog(saved.state,saved.catalog,saved.registry).get("code")=="BackendCatalogProbe","Kernel冷恢复交给backend")
	check(k.snapshot()==saved.state and not probe.seen.is_empty() and probe.seen.back()[0]=="snapshot","Kernel快照通过backend")
	check(k.content()==saved.catalog and not probe.seen.is_empty() and probe.seen.back()[0]=="content","Kernel目录通过backend")
	check(k.save_context()==saved and not probe.seen.is_empty() and probe.seen.back()[0]=="save_context","Kernel三元组读取通过backend")
	check(k.preview("wait").get("code")=="BackendPreviewProbe","Kernel preview交给backend")
	check(probe.seen==[["new_game",true],["prepare_restore_registry",true],["restore",true],["restore_with_catalog",true],["snapshot",true],["content",true],["save_context",true],["preview",true]],"每个公共入口委托一次且两个callback有效")

## 只从原7652源码记录普通值Variant；新版本只能逐条对照真实旧state/输出/registry/hook行为。
func oracle(args: PackedStringArray) -> void:
	for argument in args:
		if argument.begins_with("--record-oracle="):
			var path: String=argument.trim_prefix("--record-oracle=")
			if not check(FileAccess.get_file_as_string("res://core/world_kernel.gd").sha256_text()==OLD_KERNEL_SHA and not FileAccess.file_exists(path),"仅原7652首次冻结oracle"):return
			var file=FileAccess.open(path,FileAccess.WRITE)
			if not check(file!=null,"生命周期oracle路径可写"):return
			file.store_var({"kernel_sha256":OLD_KERNEL_SHA,"rows":rows},false);file.close();return
		if argument.begins_with("--oracle="):
			var file=FileAccess.open(argument.trim_prefix("--oracle="),FileAccess.READ);var saved: Variant=file.get_var(false) if file!=null else null
			if not check(saved is Dictionary and saved.get("kernel_sha256")==OLD_KERNEL_SHA and saved.get("rows") is Array,"读取原7652 oracle"):return
			check(saved.rows.size()==rows.size(),"生命周期oracle记录数一致")
			for index in mini(rows.size(),saved.rows.size()):check(rows[index]==saved.rows[index],"原7652生命周期逐项相等 "+rows[index].label)
			return
	check(false,"必须明确record或只读oracle路径")

## 复用同正式prepared输入，先记录旧公共行为，再单独要求新的拥有者边界。
func run() -> void:
	var args: PackedStringArray=OS.get_cmdline_user_args()
	if not configure_input(args):finish_lifecycle();return
	preview_contract();startup_and_registry();restoration();copies_and_instances();oracle(args)
	if "--backend-required" in args:delegation()
	finish_lifecycle()

## 释放受信测试闭包环并报告本门，不重复原submit矩阵或完整存读。
func finish_lifecycle() -> void:
	for k in kernels:k._registry={}
	kernels.clear();print("LEGACY_BACKEND_LIFECYCLE_CHECKS=",checks," FAILURES=",failures);quit(0 if failures==0 else 1)
