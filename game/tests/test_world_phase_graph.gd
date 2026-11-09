## 明确阶段图、旧全状态边界及真实Kernel失败原子性；不以新包装器互比冒充旧行为。
extends SceneTree
const Kernel=preload("res://core/world_kernel.gd")
const Registry=preload("res://core/runtime/module_registry.gd")
const Builtin=preload("res://core/runtime/builtin_modules.gd")
const Codec=preload("res://core/save_codec.gd")
const IDS=["preflight","military_supply","siege_step","military_battle","battle_actor_changes","siege_battles_finished","estate_production","contact_detect","actions_step","contact_install","social_step","siege_results","campaign_step","release_invalid_commands","clock_commit","military_return_complete","family_complete"]
var checks: int=0
var failures: int=0
var hits: int=0
var Phases: GDScript
var Oracle: GDScript

## 延后运行使缺少接口表现为可读断言红灯，不制造缺文件解析崩溃。
func _init() -> void:
	call_deferred("run_checks")

## 保留每个失败阶段与原错误供收敛，不从通过总数推断未执行项目。
func check(value: bool, label: String) -> bool:
	checks+=1
	if not value: failures+=1; printerr("FAIL: ",label)
	return value

## 编译期间调用这个哨兵就是越界；正常编译不依赖世界和随机流。
func sentinel(_state: Dictionary, _data: Dictionary, _context: Dictionary) -> Dictionary:
	hits+=1
	return {"ok":true}

## 小图夹具明确拥有者与全部资源声明，不复刻生产编译算法。
func hook(id: String, reads: Array=[], writes: Array=[], before: Array=[], after: Array=[]) -> Dictionary:
	return {"id":id,"owner":"test","reads":reads,"writes":writes,"before":before,"after":after,"run":sentinel}

## 所有负向图须稳定返回指定错误，不泄漏部分可执行计划。
func reject(hooks: Array, code: String, label: String) -> void:
	var result: Dictionary=Phases.compile(hooks)
	check(not result.ok and result.get("code")==code and not result.has("hooks"),label+": "+str(result))

## 形状、路径语法、显式依赖优先级与传递冲突覆盖全部编译合同。
func compiler() -> void:
	var original: Array=[hook("b"),hook("a")]; var before: Array=original.duplicate(true)
	var built: Dictionary=Phases.compile(original)
	check(built.ok and built.order==["a","b"] and hits==0 and original==before,"独立钩子确定排序且不执行/修改描述")
	original[0].reads.append("state.actors")
	check(built.hooks[1].reads.is_empty(),"编译后元数据不共享可变数组")
	for key in hook("sample"):
		var bad: Dictionary=hook("sample"); bad.erase(key)
		reject([bad],"InvalidPhaseHook","缺字段 "+key)
	for bad in [null,3,[],{"id":"x"}]: reject([bad],"InvalidPhaseHook","非法钩子形状")
	for key in ["id","owner"]:
		for value in ["",null,1]:
			var bad: Dictionary=hook("sample"); bad[key]=value
			reject([bad],"InvalidPhaseHook","非法名称 "+key)
	for key in ["before","after","reads","writes"]:
		var bad: Dictionary=hook("sample"); bad[key]="state"
		reject([bad],"InvalidPhaseHook","非法集合 "+key)
	for path in ["","actors","data","State","state.","state..actors","state.*","state.actors[0]","state.1id","context.-bad","state.中文"]:
		reject([hook("x",[path])],"InvalidPhaseHook","非法资源路径 "+path)
	var bad: Dictionary=hook("x"); bad.run=Callable()
	reject([bad],"InvalidPhaseHook","无效执行绑定")
	reject([hook("x"),hook("x")],"DuplicatePhaseId","重复阶段")
	reject([hook("x",[],[],["unknown"])],"UnknownPhaseDependency","未知before")
	reject([hook("x",[],[],[],["unknown"])],"UnknownPhaseDependency","未知after")
	reject([hook("x",[],[],["x"])],"PhaseDependencyCycle","自环")
	reject([hook("a",[],[],["b"]),hook("b",[],[],["a"])],"PhaseDependencyCycle","一般环")
	reject([hook("a",[],[],["b","missing"]),hook("b",[],[],["a"])],"UnknownPhaseDependency","未知依赖先于环")
	reject([hook("a",[],["state"],["b"]),hook("b",[],["state"],["a"])],"PhaseDependencyCycle","环先于歧义")
	for resources in [["state.actors","state.actors"],["state","state.actors"],["state.actors","state.actors.anything"],["context","context.hold"]]:
		reject([hook("a",[],[resources[0]]),hook("b",[],[resources[1]])],"AmbiguousPhaseOrder","写写重叠")
		reject([hook("a",[],[resources[0]]),hook("b",[resources[1]])],"AmbiguousPhaseOrder","写读重叠")
		reject([hook("a",[resources[0]]),hook("b",[],[resources[1]])],"AmbiguousPhaseOrder","读写重叠")
	check(Phases.compile([hook("b",["state"]),hook("a",["state.actors"])]).ok,"读读可交换")
	for pair in [["state.actors","state.actors_extra"],["state.actors","state.actions"],["state","context"]]:
		check(Phases.compile([hook("b",[],[pair[0]]),hook("a",[],[pair[1]])]).ok,"分离根或兄弟路径可交换")
	var transitive: Array=[hook("c",["state.actors.child"]),hook("b",[],[],["c"]),hook("a",[],["state"],["b"])]
	check(Phases.compile(transitive).order==["a","b","c"],"显式传递路径证明冲突顺序")
	check(Phases.compile([]).ok,"最小begin专用注册允许空图")
	var production: Array=load("res://core/runtime/legacy_phases.gd").descriptors()
	var rng=RandomNumberGenerator.new(); rng.seed=140014
	for trial in 100:
		var shuffled: Array=production.duplicate(true)
		for index in range(shuffled.size()-1,0,-1):
			var other: int=rng.randi_range(0,index); var saved: Dictionary=shuffled[index]
			shuffled[index]=shuffled[other]; shuffled[other]=saved
		check(Phases.compile(shuffled).get("order")==IDS,"独立种子排列 "+str(trial))
	for index in production.size():
		var missing: Array=production.duplicate(true); missing.remove_at(index)
		reject(missing,"UnknownPhaseDependency","缺任何真实块不能静默缩短 "+IDS[index])

## 测试组合根在build前包装真实回调；失败注入从不替代真实领域成功执行。
class FaultKernel extends "res://core/world_kernel.gd":
	var fail_phase: String=""
	var seen: Array=[]
	var boundaries: Dictionary={}
	var changed: bool=false
	var tail_failure: bool=false
	var schema_failure: bool=false
	var fail_journey: bool=false
	var journey_failures: Array=[]
	## 闭包持有各自实际回调；测试候选变更使用保守state声明，不修改生产元数据。
	func _module_descriptors() -> Array:
		var descriptors: Array=super._module_descriptors()
		for module in descriptors:
			for entry in module.phases:
				var run: Callable=entry.run; var id: String=entry.id
				entry.writes.append("state")
				entry.run=func(state: Dictionary,data: Dictionary,context: Dictionary) -> Dictionary:
					seen.append(id)
					var result: Dictionary=run.call(state,data,context)
					if not result.ok: return result
					boundaries[id]={"state":state.duplicate(true),"context":context.duplicate(true),"real_ok":true}
					if fail_phase==id:
						state.actors[state.player_actor_id].fatigue+=1
						changed=true
						return {"ok":false,"code":"InjectedPhase:"+id}
					return result
		return descriptors
	## 仅用于额外验证Step之后仍由同一个submit事务覆盖，不改变真实尾部执行。
	func _advance(state: Dictionary, hold: Array=[]) -> Dictionary:
		var result: Dictionary=super._advance(state,hold)
		if result.ok and schema_failure: state.actors[state.player_actor_id].fatigue=-1
		if result.ok and tail_failure:
			state.actors[state.player_actor_id].fatigue+=1
			return {"ok":false,"code":"InjectedAfterTail"}
		return result
	## 实际行程请求失败必须追加到Kernel已经持有的同一个挂起数组。
	func _military_journey(state: Dictionary, request: Dictionary) -> Dictionary:
		if fail_journey:
			journey_failures.append(state.armies[request.army_id].commander_actor_id)
			return {"ok":false,"code":"InjectedJourney"}
		return super._military_journey(state,request)

## 从已收录的合法作者目录与完整初态建立真实世界，不改tick、位置、胜负或年龄。
func restore_record(k, record: Dictionary) -> bool:
	var result: Dictionary=Oracle.restore_kernel(k,record)
	return check(result.ok,"原合法边界通过new_game与完整restore："+str(result))

## 17个真实阶段各失败一次，同序号修复重试成功一次，再重放保持缓存与全部状态。
func rollback(records: Array) -> void:
	var family: Dictionary={}; var returned: Dictionary={}; var siege: Dictionary={}
	for record in records:
		if record.label=="family_adoption" and record.initial.tick==1: family=record
		for action in record.initial.actions.values():
			if action.kind=="army_return" and record.label=="test_siege_world": returned=record
		if record.label=="test_siege_world" and not record.rows.is_empty() and not record.rows[-1].context.get("_receipts",[]).is_empty(): siege=record
	if not check(not family.is_empty() and not returned.is_empty() and not siege.is_empty(),"存在真实收养/返营/占城边界"): return
	for index in IDS.size():
		var id: String=IDS[index]; var k=FaultKernel.new()
		var record: Dictionary=returned if id=="military_return_complete" else siege if id in ["siege_results","campaign_step","release_invalid_commands"] else family
		if not restore_record(k,record): continue
		k.fail_phase=id
		var before: Dictionary=k.snapshot(); var actor: String=before.player_actor_id
		var command: Dictionary={"kind":"advance","params":{},"actor_id":actor,"request_seq":before.request_sequences[actor]+1,"expected_revision":before.revision}
		var result: Dictionary=k.submit(command)
		check(result.get("code")=="InjectedPhase:"+id and k.seen==IDS.slice(0,index+1),"真实阶段首错停止 "+id+": "+str(result))
		check(k.changed and k.boundaries.get(id,{}).get("real_ok",false) and k.snapshot()==before,"候选真实成功且变更后失败，权威全状态回滚 "+id)
		if id in ["clock_commit","military_return_complete","family_complete"]: check(k.boundaries[id].state.tick==before.tick+1,"失败前确已跨过新时钟 "+id)
		if id=="family_complete": check(k.boundaries[id].state.actors.size()==before.actors.size()+1,"真实动态子女创建后整体回滚")
		if id=="military_return_complete": check(k.boundaries[id].state.campaigns!=before.campaigns,"真实返营军令完成后整体回滚")
		k.fail_phase=""; k.seen=[]
		result=k.submit(command)
		check(result.ok and k.seen==IDS,"修复后同序号只推进一次 "+id+": "+str(result))
		var after: Dictionary=k.snapshot(); var reached: Array=k.seen.duplicate()
		check(k.submit(command)==result and k.snapshot()==after and k.seen==reached,"重放缓存不二次回调 "+id)
		k._registry={}; k.boundaries={}
	var tail=FaultKernel.new()
	if restore_record(tail,family):
		tail.tail_failure=true
		var before: Dictionary=tail.snapshot(); var result: Dictionary=tail.advance_one_tick()
		check(result.get("code")=="InjectedAfterTail" and tail.seen==IDS and tail.snapshot()==before,"Step与真实尾部之后失败仍丢弃全部候选")
		tail._registry={}; tail.boundaries={}
	var schema=FaultKernel.new()
	if restore_record(schema,family):
		schema.schema_failure=true
		var before: Dictionary=schema.snapshot(); var result: Dictionary=schema.advance_one_tick()
		check(result.get("code")=="InvalidActorState" and schema.seen==IDS and schema.snapshot()==before,"真实尾部后最终Schema拒绝仍保留完整权威世界")
		schema._registry={}; schema.boundaries={}
	for record in records:
		if record.rows.is_empty() or record.rows[-1].context.get("journey_requests",[]).is_empty(): continue
		var k=FaultKernel.new()
		if not restore_record(k,record): continue
		k.fail_journey=true
		var result: Dictionary=k.advance_one_tick()
		check(result.ok and not k.journey_failures.is_empty(),"真实Kernel尾部消费旧行程请求")
		for request in record.rows[-1].context.journey_requests:
			var actor: String=record.final.armies[request.army_id].commander_actor_id
			check(actor in result.details.suspended_actors,"Step真实请求失败追加保持已有数组别名；NPC提案失败另由其候选丢弃")
		k._registry={}; k.boundaries={}
		break

## 实际图逐边界对照已冻结旧镜像，不允许更新期望来消除差异。
func parity() -> Array:
	var records: Array=Oracle.records()
	if not check(not records.is_empty(),"冻结旧边界证据可读"): return []
	var registry: Dictionary=Registry.build(Builtin.descriptors()); var before: Dictionary=registry.duplicate(true)
	for record in records:
		var compared: Dictionary=Oracle.compare(record,registry)
		check(compared.ok,"旧阶段全状态/输出/首错 "+record.label+": "+compared.difference)
		if not compared.ok: Oracle.write_difference(record,compared)
	check(registry==before,"正常运行不改注册动作、元数据或已编译计划")
	var state: Dictionary=records[0].initial.duplicate(true)
	for bad in [{},{"phase_plan":{}},{"phase_plan":{"format":"HM-WORLD-PHASES/1","hooks":[]}},{"phase_plan":{"format":"HM-WORLD-PHASES/1","hooks":[{}]}}]:
		var copy: Dictionary=state.duplicate(true)
		check(Phases.advance(copy,records[0].data,[],bad).get("code")=="MissingWorldPhaseGraph" and copy==state,"缺/坏/空图明确拒绝且不改候选")
	return records

## 新接口不存在时只报告合同红灯；存在才执行完整编译和迁移证明。
func run_checks() -> void:
	if not ResourceLoader.exists("res://core/runtime/world_phases.gd"):
		check(false,"缺少可执行显式阶段图合同"); finish(); return
	Phases=load("res://core/runtime/world_phases.gd")
	if not check(Phases.has_method("compile") and Phases.has_method("advance"),"阶段编译与执行接口完整"): finish(); return
	Oracle=load("res://tests/world_phase_oracle.gd")
	compiler()
	var migration: bool=false
	for argument in OS.get_cmdline_user_args():
		if argument=="--old-phase-evidence" or argument.begins_with("--old-phase-evidence="):
			migration=true
			Oracle.evidence_dir=argument.trim_prefix("--old-phase-evidence=") if "=" in argument else ""
	var records: Array=parity() if migration else Oracle.current_records()
	check(not records.is_empty(),"当前合法短夹具可运行："+str(Oracle.errors))
	rollback(records)
	finish()

## 统一机器汇总，不把聚合或历史档范围算入本局部测试。
func finish() -> void:
	print("WORLD_PHASE_GRAPH_CHECKS=",checks," FAILURES=",failures)
	quit(1 if failures else 0)
