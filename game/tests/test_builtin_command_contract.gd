## 完整启动锚覆盖经济任务漏网项；泛型注册保持开放，失败重开不能替换旧世界。
extends SceneTree
const Builtin=preload("res://core/runtime/builtin_modules.gd")
const Registry=preload("res://core/runtime/module_registry.gd")
const Kernel=preload("res://core/world_kernel.gd")
const Schema=preload("res://core/state_schema.gd")
var checks: int=0
var failures: int=0
var hits: int=0

## 只替换受信描述来源，仍经真实new_game与restore入口验收。
class ProbeKernel extends "res://core/world_kernel.gd":
	var source: Array=[]
	## 每次返回测试准备的独立描述，不改父类注册或状态提交逻辑。
	func _module_descriptors() -> Array:
		return source

## 延后执行以免混淆脚本构造和测试结果。
func _init() -> void:
	call_deferred("_run")

## 所有失败都有标签，进程最终非零退出。
func check(ok: bool, label: String) -> void:
	checks+=1
	if not ok:
		failures+=1
		printerr("FAIL: ",label)

## 新受信命令用于开放性对照；合同校验不应探测执行回调。
func sentinel() -> Dictionary:
	hits+=1
	return {"ok":true}

## 按固定身份找到描述，不依赖生产模块的数组顺序。
func module(source: Array, id: String) -> Dictionary:
	for entry in source:
		if entry.id==id: return entry
	return {}

## 只删除指定命令，保留行动、拥有权和全部状态校验器以隔离漏锚问题。
func remove_command(source: Array, kind: String) -> void:
	for descriptor in source:
		for i in range(descriptor.commands.size()-1,-1,-1):
			if descriptor.commands[i].kind==kind: descriptor.commands.remove_at(i)

## 独立重建默认来源后施加一处真实审计故障，绝不修改Builtin返回值以外的共享对象。
func mutated(name: String, strip_requirements: bool=false) -> Array:
	var source: Array=Builtin.descriptors()
	var economy: Dictionary=module(source,"economy")
	var tasks: Dictionary=module(source,"tasks")
	if name.begins_with("missing_"): remove_command(source,name.trim_prefix("missing_"))
	match name:
		"wrong_trade_gate": economy.commands[0].gate_id="bogus_gate"
		"wrong_trade_mode":
			economy.commands[0].mode="instant"
			economy.commands[0].apply=sentinel
		"wrong_cancel_owner":
			for i in range(tasks.commands.size()-1,-1,-1):
				if tasks.commands[i].kind=="cancel_task": module(source,"social").commands.append(tasks.commands.pop_at(i))
		"trade_downgrade_compat":
			economy.commands[0].compat_action="legacy_begin_action"
			economy.legacy_action_validators.append({"kind":"trade","signature":"state,data,action","validate":economy.actions[0].validate})
			economy.actions=[]
		"duplicate_trade": economy.commands.append(economy.commands[0].duplicate(true))
	if strip_requirements:
		for descriptor in source:
			descriptor.erase("required_ownership")
			descriptor.erase("required_validators")
	return source

## 七个独立故障必须被Builtin拒绝；删required声明不能规避固定预期。
func test_audit_cases() -> void:
	var cases: Dictionary={"missing_trade":"MissingBuiltinCommand","missing_accept_offer":"MissingBuiltinCommand","missing_cancel_task":"MissingBuiltinCommand","wrong_trade_gate":"InvalidBuiltinCommand","wrong_cancel_owner":"InvalidBuiltinCommand","wrong_trade_mode":"InvalidBuiltinCommand","trade_downgrade_compat":"InvalidBuiltinCommand","missing_family_adopt":"MissingBuiltinCommand"}
	var kernel=ProbeKernel.new()
	kernel.source=Builtin.descriptors()
	check(kernel.new_game().ok,"真实默认开局")
	var before: Dictionary=kernel.snapshot()
	var catalog: Dictionary=kernel.content()
	var registered: Dictionary=kernel._registry.duplicate(true)
	for name in cases:
		for stripped in [false,true]:
			var source: Array=mutated(name,stripped)
			var registry: Dictionary=Registry.build(source)
			var label: String=name+(" without required" if stripped else "")
			check(registry.ok,"泛型描述形状仍合法 "+label)
			if not registry.ok: continue
			var contract: Dictionary=Builtin.command_contract_status(registry)
			check(not contract.ok and contract.get("code")==cases[name],"Builtin拒绝 "+label+" "+str(contract))
			kernel.source=source
			var result: Dictionary=kernel.new_game()
			check(not result.ok and result.get("code")==cases[name],"真实启动拒绝 "+label+" "+str(result))
			check(kernel.snapshot()==before and kernel.content()==catalog and kernel._registry==registered,"失败保留state/catalog/registry "+label)
			check(kernel.restore(before).ok and kernel.snapshot()==before,"拒绝新组合后仍用原已验注册恢复 "+label)
	var duplicate: Dictionary=Registry.build(mutated("duplicate_trade"))
	check(duplicate.get("code")=="DuplicateCommand","重复命令仍由Registry优先拒绝")
	kernel.source=mutated("duplicate_trade")
	check(kernel.new_game().get("code")=="DuplicateCommand" and kernel.snapshot()==before and kernel.content()==catalog and kernel._registry==registered,"重复命令不开局且保留旧世界")
	var bad_saved: Dictionary=before.duplicate(true)
	bad_saved.erase("wallets")
	var expected: Dictionary=Schema.validate(bad_saved,catalog,registered)
	check(not expected.ok and kernel.restore(bad_saved).get("code")==expected.get("code") and kernel.snapshot()==before,"restore沿用原Schema错误与原子边界")

## 固定预期涵盖默认组合里的每一命令，后续域追加自动获得缺项保护。
func test_all_default_commands() -> void:
	var valid: Dictionary=Registry.build(Builtin.descriptors())
	check(valid.ok and Builtin.command_contract_status(valid).ok,"正常默认组合完整通过")
	for kind in valid.commands:
		var missing: Dictionary=valid.duplicate(true)
		missing.commands.erase(kind)
		check(Builtin.command_contract_status(missing).get("code")=="MissingBuiltinCommand","每个默认kind都受启动锚保护 "+kind)
	for key in ["kind","owner","mode","gate_id","compat_action"]:
		var wrong: Dictionary=valid.duplicate(true)
		wrong.commands.trade[key]="wrong"
		check(Builtin.command_contract_status(wrong).get("code")=="InvalidBuiltinCommand","独立比对命令字段 "+key)
	for owner in ["", "social"]:
		var wrong: Dictionary=valid.duplicate(true)
		if owner.is_empty(): wrong.actions.erase("trade")
		else: wrong.actions.trade.owner=owner
		check(Builtin.command_contract_status(wrong).get("code")=="MissingBuiltinAction","正式action不可缺失或借owner "+owner)
	var nullable: Dictionary=valid.duplicate(true)
	nullable.commands.trade.compat_action=null
	check(Builtin.command_contract_status(nullable).get("code")=="InvalidBuiltinCommand","compat字段存在性不能由null值冒充未声明")
	var doubled: Dictionary=valid.duplicate(true)
	doubled.legacy_action_validators.trade=valid.actions.trade.duplicate(true)
	check(Builtin.command_contract_status(doubled).get("code")=="InvalidBuiltinAction","正式action不能同时冒领旧validator")
	doubled=valid.duplicate(true)
	doubled.actions.family_adopt=valid.legacy_action_validators.family_adopt.duplicate(true)
	check(Builtin.command_contract_status(doubled).get("code")=="InvalidBuiltinAction","旧action桥不能同时冒领正式生命周期")

## 开放附加受信命令及泛型空表，既有完整WorldValidation错误仍优先于缺命令。
func test_controls() -> void:
	var extra: Dictionary={"id":"trusted_extra","version":"v1","requires":{},"optional_requires":{},"capabilities":[],"owns":[],"commands":[],"actions":[],"phases":[],"validators":[],"queries":[]}
	check(Registry.build([extra]).ok,"泛型合法空commands不依赖Builtin")
	extra.commands.append({"kind":"trusted_extra","mode":"instant","preview":sentinel,"apply":sentinel,"gate_id":"trusted_extra_gate"})
	check(Registry.build([extra]).ok,"泛型合法自定义命令")
	var source: Array=Builtin.descriptors()
	source.append(extra)
	var registry: Dictionary=Registry.build(source)
	check(registry.ok and Builtin.command_contract_status(registry).ok,"默认组合允许额外受信模块和命令")
	var kernel=ProbeKernel.new()
	kernel.source=source
	check(kernel.new_game().ok,"带额外受信模块真实开局")
	check(hits==0,"注册与完整命令比较均不执行命令回调")
	var missing: Dictionary=Registry.build(mutated("missing_trade"))
	var two_missing: Dictionary=missing.duplicate(true)
	two_missing.commands.erase("family_adopt")
	var prior: Dictionary=Builtin.command_contract_status(two_missing)
	check(prior.get("code")=="MissingBuiltinCommand" and prior.get("detail")=="family_adopt","原领域缺命令优先于新默认组合锚")
	missing.validators.erase("economy.state")
	check(Builtin.command_contract_status(missing).get("code")=="MissingStateValidator","完整世界状态叶错误优先于缺命令")
	missing.modules.erase("builtin")
	check(Builtin.command_contract_status(missing).get("code")=="InvalidModuleRegistry","完整组合身份错误优先于缺命令")
	var broken: Array=mutated("missing_trade",true)
	module(broken,"economy").validators=[]
	check(Registry.build(broken).get("code")=="MissingStateValidator","删required后真实Registry仍优先拒绝缺状态叶")
	kernel.source=broken
	check(kernel.new_game().get("code")=="MissingStateValidator","真实启动保留完整世界错误优先")

## 只运行命令完整性及相邻保存边界，不触发长时模拟或全量发行。
func _run() -> void:
	test_audit_cases()
	test_all_default_commands()
	test_controls()
	print("BUILTIN_COMMAND_CONTRACT_CHECKS=",checks," FAILURES=",failures)
	quit(1 if failures else 0)
