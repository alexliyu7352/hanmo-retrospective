## 启动必须包含默认世界阶段；通用小图仍开放，失败重开完整保留原世界。
extends SceneTree
const Builtin=preload("res://core/runtime/builtin_modules.gd")
const Registry=preload("res://core/runtime/module_registry.gd")
const Phases=preload("res://core/runtime/world_phases.gd")
var checks: int=0
var failures: int=0
var hits: int=0

## 仅替换受信描述来源，仍使用真实注册、开局及事务入口。
class ProbeKernel extends "res://core/world_kernel.gd":
	var source: Array=[]
	## 返回独立测试描述，不修改父类候选验证或提交规则。
	func _module_descriptors() -> Array:
		return source

## 场景树准备好后执行，避免脚本构造错误掩盖合同断言。
func _init() -> void:
	call_deferred("_run")

## 每个失败保留标签并使最终退出码非零。
func check(ok: bool, label: String) -> void:
	checks+=1
	if not ok:
		failures+=1
		printerr("FAIL: ",label)

## 扩展阶段只记录调用，证明注册不执行回调且推进恰好调用一次。
func sentinel(_state: Dictionary, _data: Dictionary, _context: Dictionary) -> Dictionary:
	hits+=1
	return {"ok":true}

## 按稳定身份找描述，避免依赖模块注册数组位置。
func module(source: Array, id: String) -> Dictionary:
	for entry in source:
		if entry.id==id: return entry
	return {}

## 仅提供合法空受信域，用来检验泛型与完整世界的不同边界。
func extra_module() -> Dictionary:
	return {"id":"trusted_extra","version":"v1","requires":{},"optional_requires":{},"capabilities":[],"owns":[],"commands":[],"actions":[],"phases":[],"validators":[],"queries":[]}

## 删除阶段后重接其相邻依赖，证明通用图合法不能代替世界完整性。
func remove_phase(source: Array, id: String) -> void:
	var removed: Dictionary={}
	for descriptor in source:
		for index in range(descriptor.phases.size()-1,-1,-1):
			if descriptor.phases[index].id==id: removed=descriptor.phases.pop_at(index)
	for descriptor in source:
		for phase in descriptor.phases:
			for field in ["before","after"]:
				if id in phase[field]:
					phase[field].erase(id)
					phase[field].append_array(removed[field])

## 缺任何真实块都在new_game拒绝；删自报required字段或伪造required_phases也不能规避。
func test_missing_phases() -> void:
	var valid: Dictionary=Registry.build(Builtin.descriptors())
	var kernel=ProbeKernel.new()
	kernel.source=Builtin.descriptors()
	check(kernel.new_game().ok,"默认真实开局")
	var before: Dictionary=kernel.snapshot()
	var catalog: Dictionary=kernel.content()
	var registered: Dictionary=kernel._registry.duplicate(true)
	var ids: Array=valid.phase_plan.order.duplicate()
	ids.push_front("")
	for id in ids:
		for stripped in [false,true]:
			var source: Array=Builtin.descriptors()
			if id.is_empty():
				for descriptor in source: descriptor.phases=[]
			else: remove_phase(source,id)
			if stripped:
				for descriptor in source:
					descriptor.erase("required_ownership")
					descriptor.erase("required_validators")
					descriptor.required_phases=[]
			var registry: Dictionary=Registry.build(source)
			var label: String=("all" if id.is_empty() else id)+(" without required" if stripped else "")
			check(registry.ok,"缺项重接后仍是合法通用图 "+label)
			kernel.source=source
			var result: Dictionary=kernel.new_game()
			check(not result.ok and result.get("code")=="MissingBuiltinPhase","真实启动拒绝缺阶段 "+label+" "+str(result))
			if not id.is_empty(): check(result.get("detail")==id,"明确指出缺失阶段 "+label)
			check(kernel.snapshot()==before and kernel.content()==catalog and kernel._registry==registered,"失败保留state/catalog/registry "+label)
			check(kernel.restore(before).ok and kernel.snapshot()==before,"失败后仍使用原注册恢复 "+label)
	# 拒绝后的当前source仍损坏；推进必须继续使用上次已提交的完整注册。
	check(kernel.submit({"kind":"wait","params":{"ticks":1},"request_seq":1,"expected_revision":0}).ok,"失败重开后原世界仍可开始等待")
	check(kernel.advance_one_tick().ok and kernel.snapshot().tick==before.tick+1,"失败重开后原世界仍完整推进")

## 默认阶段不能转借拥有者或缩窄依赖与资源声明；通用编译成功并不证明发行合同完整。
func test_phase_metadata() -> void:
	var kernel=ProbeKernel.new()
	for field in ["owner","before","after","reads","writes"]:
		var source: Array=Builtin.descriptors()
		var phases: Array=module(source,"builtin").phases
		for index in phases.size():
			if phases[index].id!="social_step": continue
			if field=="owner":
				var extra: Dictionary=extra_module()
				var moved: Dictionary=phases.pop_at(index)
				moved.owner=extra.id
				extra.phases.append(moved)
				source.append(extra)
			else: phases[index][field]=[]
			break
		var registry: Dictionary=Registry.build(source)
		check(registry.ok,"元数据缩窄仍是合法通用图 "+field)
		kernel.source=source
		var result: Dictionary=kernel.new_game()
		check(result.get("code")=="InvalidBuiltinPhase" and result.get("detail")=="social_step","完整世界拒绝默认元数据缩窄 "+field+" "+str(result))
		check(kernel.snapshot().is_empty() and kernel.content().is_empty() and kernel._registry.is_empty(),"首次开局失败不留下半个世界 "+field)

## 原图编译、command、observer错误先于新增缺阶段错误，不改变既有启动诊断。
func test_error_priority() -> void:
	var kernel=ProbeKernel.new()
	var source: Array=Builtin.descriptors()
	module(source,"builtin").phases.append(module(source,"builtin").phases[0].duplicate(true))
	kernel.source=source
	check(kernel.new_game().get("code")=="DuplicatePhaseId","重复阶段保留通用图首错")
	source=Builtin.descriptors()
	module(source,"builtin").phases[0].before=["missing"]
	kernel.source=source
	check(kernel.new_game().get("code")=="UnknownPhaseDependency","未知边保留通用图首错")
	source=Builtin.descriptors()
	module(source,"builtin").phases=[]
	module(source,"economy").commands=[]
	kernel.source=source
	check(kernel.new_game().get("code")=="MissingBuiltinCommand","既有命令错误优先于缺阶段")
	source=Builtin.descriptors()
	module(source,"builtin").phases=[]
	module(source,"interaction").observers=[]
	kernel.source=source
	check(kernel.new_game().get("code")=="MissingEventObserver","既有观察器错误优先于缺阶段")
	source=Builtin.descriptors()
	module(source,"builtin").phases=[]
	module(source,"economy").validators=[]
	kernel.source=source
	check(kernel.new_game().get("code")=="MissingStateValidator","既有状态叶错误优先于缺阶段")

## 额外阶段显式插入原链；保留原阶段回调包装与保守资源扩充用于事务故障注入。
func test_extensions() -> void:
	check(Phases.compile([]).ok,"通用编译允许空图")
	var extra: Dictionary=extra_module()
	check(Registry.build([extra]).ok,"泛型空模块无需世界阶段")
	extra.phases=[{"id":"extra_phase","owner":"trusted_extra","before":[],"after":[],"reads":[],"writes":[],"run":sentinel}]
	check(Registry.build([extra]).ok,"泛型自定义阶段无需默认集合")
	extra.phases[0].before=["social_step"]
	extra.phases[0].after=["contact_install"]
	extra.phases[0].writes=["state"]
	var source: Array=Builtin.descriptors()
	source.append(extra)
	for phase in module(source,"builtin").phases:
		if phase.id!="social_step": continue
		var original: Callable=phase.run
		phase.writes.append("state")
		phase.after.append("extra_phase")
		phase.run=func(state: Dictionary,data: Dictionary,context: Dictionary) -> Dictionary:
			return original.call(state,data,context)
	var registry: Dictionary=Registry.build(source)
	check(registry.ok,"显式扩展图合法且保留默认阶段")
	var expected: Array=Registry.build(Builtin.descriptors()).phase_plan.order
	var actual: Array=registry.phase_plan.order.duplicate()
	actual.erase("extra_phase")
	check(actual==expected,"扩展不改变原阶段相对顺序")
	var kernel=ProbeKernel.new()
	kernel.source=source
	check(kernel.new_game().ok and hits==0,"完整世界允许扩展和包装且启动不执行回调")
	check(kernel.submit({"kind":"wait","params":{"ticks":1},"request_seq":1,"expected_revision":0}).ok,"扩展世界开始真实等待")
	check(kernel.advance_one_tick().ok and kernel.snapshot().tick==1 and hits==1,"扩展阶段真实推进且恰好执行一次")
	kernel._registry={}

## 只运行启动完整性及单半日相邻路径，不触发长世界或全量发行。
func _run() -> void:
	test_missing_phases()
	test_phase_metadata()
	test_error_priority()
	test_extensions()
	print("BUILTIN_PHASE_CONTRACT_CHECKS=",checks," FAILURES=",failures)
	quit(1 if failures else 0)
