## 六个真实军事人产命令的注册集成；有限旧源码轨迹、阶段时序与事务失败各自独立验证。
extends SceneTree
const Kernel=preload("res://core/world_kernel.gd")
const Assets=preload("res://core/military_assets.gd")
const Module=preload("res://core/domains/military/military_module.gd")
const Action=preload("res://core/domains/military/military_action.gd")
const Codec=preload("res://core/save_codec.gd")
const Strict=preload("res://core/strict_json.gd")
const Interaction=preload("res://core/interaction_rules.gd")
var checks: int=0
var failures: int=0
var path: String=""
var write_trace: String=""
var compare_trace: String=""
var trace: Array=[]

## 计数只在测试对象中，所有成功和业务失败仍执行唯一真实领域实现。
class Probe extends RefCounted:
	var calls: Dictionary={}
	var failure: String=""
	## 记录真实回调命中，不能以descriptor存在替代执行证据。
	func hit(kind: String, stage: String) -> void:
		var key: String=kind+":"+stage; calls[key]=calls.get(key,0)+1
	## 预览仍走原权限、能力和字段顺序。
	func preview(state: Dictionary,data: Dictionary,actor: String,kind: String,params: Dictionary) -> Dictionary:
		hit(kind,"preview"); return Module.preview(state,data,actor,kind,params)
	## 即时结果和资产变化保留原实现，不能用空成功代替。
	func apply(state: Dictionary,data: Dictionary,actor: String,kind: String,params: Dictionary,offer: Dictionary) -> Dictionary:
		hit(kind,"apply"); return Module.apply(state,data,actor,kind,params,offer)
	## prepare只委托真实冻结与参与者计算。
	func prepare(state: Dictionary,data: Dictionary,actor: String,kind: String,params: Dictionary,offer: Dictionary) -> Dictionary:
		hit(kind,"prepare"); return Action.prepare(state,data,actor,kind,params,offer)
	## 在实际预留和事实建立后注入硬失败，证明外层丢弃整个候选。
	func start(state: Dictionary,data: Dictionary,action: Dictionary,offer: Dictionary) -> Dictionary:
		hit(action.kind,"start"); var result: Dictionary=Action.start(state,data,action,offer)
		return {"ok":false,"code":"InjectedMilitaryStart"} if failure=="start" and result.ok else result
	## 实际step保留原半日和hold语义。
	func step(state: Dictionary,data: Dictionary,action: Dictionary,context: Dictionary) -> Dictionary:
		hit(action.kind,"step"); return Action.step(state,data,action,context)
	## 真实结算之后可硬失败或损坏候选；业务中断则先使办理人失效再调用原规则。
	func finish(state: Dictionary,data: Dictionary,action: Dictionary,tick: int) -> Dictionary:
		hit(action.kind,"finish")
		if failure=="business": state.actors[action.actor_id].available=false
		var result: Dictionary=Action.finish(state,data,action,tick)
		if failure=="tail" and result.ok: state.wallets[state.actors[action.actor_id].wallet_id].balance=-1
		return {"ok":false,"code":"InjectedMilitaryFinish"} if failure=="finish" and result.ok else result
	## 原载荷validator独占规则；包装仅记录调用。
	func validate(state: Dictionary,data: Dictionary,action: Dictionary) -> Dictionary:
		hit(action.kind,"validate"); return Assets.validate_action(state,data,action)

## 启动替换Callable仅用于观测/注错；缺登记必须由实际生产new_game而非测试自检拒绝。
class TestKernel extends "res://core/world_kernel.gd":
	var probe=Probe.new()
	var mutation: String=""
	var removed: String=""
	var phase_failure: String=""
	var capture_phases: bool=false
	var phases: Array=[]
	## 不修改完整元数据，按原真实集合替换回调并保留canonical中断接口。
	func _module_descriptors() -> Array:
		var rows: Array=super._module_descriptors()
		var moved_command: Dictionary={}
		for descriptor in rows:
			if descriptor.id=="military":
				for command in descriptor.commands:
					if not command.kind in Assets.KINDS: continue
					command.preview=probe.preview
					if command.mode=="instant": command.apply=probe.apply
				for action in descriptor.actions:
					for method in ["prepare","start","step","finish","validate"]: action[method]=Callable(probe,method)
				if mutation=="missing_command":
					for command in descriptor.commands.duplicate():
						if command.kind==removed: descriptor.commands.erase(command)
				if mutation=="duplicate": descriptor.commands.append(descriptor.commands[0].duplicate(true))
				if mutation=="missing_action": descriptor.actions.pop_back()
				if mutation=="extra_action":
					var extra: Dictionary=descriptor.actions[0].duplicate(true); extra.kind="unclaimed_military_action"; descriptor.actions.append(extra)
				if mutation=="wrong_owner":
					for command in descriptor.commands.duplicate():
						if not command.kind in Assets.ACTION_KINDS: moved_command=command; descriptor.commands.erase(command); break
				if mutation=="wrong_mode":
					for command in descriptor.commands:
						if command.kind in Assets.ACTION_KINDS: command.mode="instant"; command.apply=probe.apply; break
			for hook in descriptor.phases: hook.run=observe.bind(hook.run,hook.id)
		if not moved_command.is_empty():
			for descriptor in rows:
				if descriptor.id=="tasks": descriptor.commands.append(moved_command)
		return rows
	## 每个真实阶段执行后观察完整候选，Callable不进入轨迹；可在已写clock后注入原子失败。
	func observe(state: Dictionary,data: Dictionary,context: Dictionary,run: Callable,id: String) -> Dictionary:
		var result: Dictionary=run.call(state,data,context)
		if capture_phases and id in ["actions_step","clock_commit","military_return_complete"]:
			var visible: Dictionary=context.duplicate(true); visible.erase("_actions")
			phases.append({"id":id,"state":state.duplicate(true),"context":visible,"result":result.duplicate(true)})
		return {"ok":false,"code":"InjectedMilitaryPhase"} if result.ok and id==phase_failure else result

## 旧/新两项目用同一脚本和真实目录；写期望仅允许显式独立路径，不默认覆盖。
func _init() -> void:
	for arg in OS.get_cmdline_user_args():
		if arg.begins_with("--military-content="): path=arg.trim_prefix("--military-content=")
		if arg.begins_with("--write-trace="): write_trace=arg.trim_prefix("--write-trace=")
		if arg.begins_with("--compare-trace="): compare_trace=arg.trim_prefix("--compare-trace=")
	call_deferred("run_checks")

## 实际计数和失败决定进程退出，不能把跳过的轨迹或分支算作通过。
func check(value: bool,label: String) -> bool:
	checks+=1
	if not value: failures+=1; printerr("FAIL: ",label)
	return value

## 开局加载正式第二军事目录，不直接制造任命、兵员或军费。
func fresh():
	var k=TestKernel.new()
	check(not path.is_empty() and k.new_game("scenario.alpha_life",path).ok,"真实第二军事配置启动")
	return k

## 每个命令使用当前序号和revision，失败重试不伪造额外已提交请求。
func request(k,kind: String,params: Dictionary={}) -> Dictionary:
	var state: Dictionary=k.snapshot(); var actor: String=state.player_actor_id
	return {"kind":kind,"params":params,"actor_id":actor,"request_seq":state.request_sequences[actor]+1,"expected_revision":state.revision}

## 返回当前真实任命岗位的上下文。
func context(k) -> Dictionary:
	return Interaction.for_role(k.snapshot(),k.content().data,"role.military.a")

## 记录整个持久状态及请求结果，包含五随机流、所有钱包/库存、序号和事实。
func record(k,label: String,result: Dictionary) -> void:
	trace.append({"label":label,"result":result.duplicate(true),"state":k.snapshot(),"save_bytes":Codec.encode(k.snapshot(),0,"registered-military-trace",k.content()),"phases":k.phases.duplicate(true)})
	k.phases.clear()

## 普通submit的重复请求必须返回同结果且完整世界不变。
func send(k,kind: String,params: Dictionary={},capture: bool=false) -> bool:
	var command: Dictionary=request(k,kind,params); var result: Dictionary=k.submit(command)
	if not check(result.ok,kind+" "+str(result)): return false
	var after: Dictionary=k.snapshot()
	check(k.submit(command)==result and k.snapshot()==after,"真实重复请求幂等 "+kind)
	if capture: record(k,kind,result)
	return true

## 真实pending Codec往返后在原Kernel恢复，下一步仍由生产路由执行一次。
func save_pending(k) -> bool:
	var before: Dictionary=k.snapshot()
	var text: String=Codec.encode(before,0,"registered-military",k.content())
	var decoded: Dictionary=Codec.decode(text,k.content())
	return check(decoded.ok and k.restore(decoded.state).ok and k.snapshot()==before,"真实待办Codec往返")

## 有限完整开工/完成链覆盖六命令，世界时钟只由advance推进。
func journey(k,capture: bool=true) -> bool:
	k.capture_phases=capture
	if capture: record(k,"initial",{"ok":true})
	var office: Dictionary={"office_id":"office.military.a","context":context(k)}
	for index in range(2):
		if not send(k,"office_accept",office,capture) or not save_pending(k): return false
		var result: Dictionary=k.advance_one_tick()
		if not check(result.ok,"真实受任完成"): return false
		if capture: record(k,"office_finish_"+str(index),result)
		if index==0 and not send(k,"office_resign",office,capture): return false
	if not send(k,"campaign_accept",{"template_id":"order.military.a","context":context(k)},capture): return false
	var state: Dictionary=k.snapshot(); var order: String=state.campaigns.keys()[0]
	if not send(k,"move_local",{"place_id":"west.estate"},capture): return false
	if not send(k,"army_pledge",{"estate_id":"estate.riverside","order_id":order,"actor_ids":["actor.estate_guard"]},capture): return false
	if not send(k,"move_local",{"place_id":"west.street"},capture): return false
	if not send(k,"army_form",{"order_id":order,"unit_ids":["unit.military.a"],"food_quantity":80,"money":50},capture) or not save_pending(k): return false
	var result: Dictionary=k.advance_one_tick()
	if not check(result.ok,"真实编组完成"): return false
	if capture: record(k,"form_finish",result)
	state=k.snapshot(); var army: String=state.campaigns[order].army_id
	check(not army.is_empty() and state.armies.has(army),"编组原半日即时绑定军令")
	if not send(k,"army_resupply",{"army_id":army,"food_quantity":20,"money":10},capture) or not save_pending(k): return false
	result=k.advance_one_tick()
	if not check(result.ok,"真实补给完成"): return false
	if capture: record(k,"resupply_finish",result)
	if not send(k,"campaign_cancel",{"campaign_id":order},capture): return false
	if not send(k,"army_return",{"army_id":army},capture) or not save_pending(k): return false
	var old_tick: int=k.snapshot().tick
	result=k.advance_one_tick()
	if not check(result.ok,"真实归营完成"): return false
	if capture: record(k,"return_finish",result)
	state=k.snapshot()
	check(state.tick==old_tick+1 and state.armies[army].status=="closed" and state.campaigns[order].status=="awaiting_report","释放后新tick确认归营")
	if not send(k,"campaign_report",{"campaign_id":order,"context":context(k)},capture): return false
	if not send(k,"office_resign",{"office_id":"office.military.a","context":context(k)},capture): return false
	return true

## 从真实源集合逐kind删命令，启动必须失败并保留原世界、目录和已接受注册。
func coverage() -> void:
	for kind in Assets.KINDS:
		var k=fresh(); var before: Dictionary=k.snapshot(); var catalog: Dictionary=k.content()
		k.mutation="missing_command"; k.removed=kind
		check(k.new_game("scenario.alpha_life",path).get("code")=="MissingBuiltinCommand" and k.snapshot()==before and k.content()==catalog,"漏真实kind启动拒绝且原世界不变 "+kind)
		check(k.preview("office_accept",{"office_id":"office.military.a","context":context(k)}).ok,"失败启动仍保留先前合法registry "+kind)
	for row in [["duplicate","DuplicateCommand"],["missing_action","UnknownCommandAction"],["wrong_mode","InvalidBuiltinCommand"],["wrong_owner","InvalidBuiltinCommand"],["extra_action","InvalidBuiltinAction"]]:
		var k=fresh(); var before: Dictionary=k.snapshot(); k.mutation=row[0]
		check(k.new_game("scenario.alpha_life",path).get("code")==row[1] and k.snapshot()==before,"坏军事注册拒绝 "+row[0])

## 通过真实受任受令准备编组，后续故障不能靠空资产候选假证明回滚。
func ready():
	var k=fresh()
	if not send(k,"office_accept",{"office_id":"office.military.a","context":context(k)}) or not check(k.advance_one_tick().ok,"故障夹具真实任官"): return null
	if not send(k,"campaign_accept",{"template_id":"order.military.a","context":context(k)}): return null
	return k

## 同一个有真实预留的编组覆盖start/finish/尾部和新clock后硬失败；业务失败仍完成释放。
func rollback() -> void:
	for stage in ["start","finish","tail","clock"]:
		var k=ready()
		if k==null: continue
		var order: String=k.snapshot().campaigns.keys()[0]
		var params: Dictionary={"order_id":order,"unit_ids":["unit.military.a"],"food_quantity":80,"money":50}
		if stage=="start":
			var before: Dictionary=k.snapshot(); k.probe.failure=stage
			check(k.submit(request(k,"army_form",params)).get("code")=="InjectedMilitaryStart" and k.snapshot()==before,"start已预留后整候选回滚")
			k.probe.failure=""
			check(k.submit(request(k,"army_form",params)).ok and k.advance_one_tick().ok and k.snapshot().armies.size()==1,"start硬失败同请求序号可真实重试且无重复军团")
			continue
		if not send(k,"army_form",params): continue
		var before: Dictionary=k.snapshot()
		if stage=="clock": k.phase_failure="clock_commit"
		else: k.probe.failure=stage
		var result: Dictionary=k.advance_one_tick()
		check(not result.ok and k.snapshot()==before,"真实完成/共同协调/尾部失败整候选回滚 "+stage+" "+str(result.get("code")))
		k.phase_failure=""; k.probe.failure=""
		check(k.advance_one_tick().ok and k.snapshot().armies.size()==1,"失败后正常重试只建立一个军团 "+stage)
	var returning=ready()
	if returning!=null:
		var order: String=returning.snapshot().campaigns.keys()[0]
		if send(returning,"army_form",{"order_id":order,"unit_ids":["unit.military.a"],"food_quantity":80,"money":50}) and check(returning.advance_one_tick().ok,"归营故障夹具真实编组"):
			var army: String=returning.snapshot().campaigns[order].army_id
			if send(returning,"army_return",{"army_id":army}):
				var before: Dictionary=returning.snapshot(); returning.phase_failure="military_return_complete"
				check(returning.advance_one_tick().get("code")=="InjectedMilitaryPhase" and returning.snapshot()==before,"返营完成新tick军令确认后硬失败仍整候选回滚")
				returning.phase_failure=""
				check(returning.advance_one_tick().ok and returning.snapshot().campaigns[order].status=="awaiting_report","返营硬失败重试只清算一次")
	var k=ready()
	if k==null: return
	var params: Dictionary={"order_id":k.snapshot().campaigns.keys()[0],"unit_ids":["unit.military.a"],"food_quantity":80,"money":50}
	if not send(k,"army_form",params): return
	var pending: Dictionary=k.snapshot(); var id: String=pending.actors[pending.player_actor_id].action_id
	k.probe.failure="business"
	var result: Dictionary=k.advance_one_tick(); var state: Dictionary=k.snapshot()
	check(result.ok and not result.details.completed_actions[id].success and state.armies.is_empty() and not state.actions.has(id),"原规则业务中断仍完成半日，不建幽灵军团")
	for actor in pending.actions[id].participants: check(state.actors[actor].action_id.is_empty(),"业务中断释放实际参与者 "+actor)
	check(state.wallets["wallet.military.a"].reserved.is_empty() and state.facts[id].phase=="completed" and not state.facts[id].success,"业务中断释放预留并保留失败事实")

## 从另一引擎进程保存的原始HM-SAVE字节恢复每个待办点，下一真实半日与旧完整结果和阶段一致。
func restored_trace(rows: Array) -> void:
	var covered: Dictionary={}
	for index in range(rows.size()-1):
		var row: Dictionary=rows[index]
		if not row.label in Assets.ACTION_KINDS: continue
		var k=fresh(); k.capture_phases=true
		var decoded: Dictionary=Codec.decode(row.save_bytes,k.content())
		if not check(decoded.ok and k.restore(decoded.state).ok and k.snapshot()==row.state,"新Kernel恢复旧进程实际待办存档 "+row.label): continue
		var advanced: Dictionary=k.advance_one_tick(); var expected: Dictionary=rows[index+1]
		check(advanced==expected.result and k.snapshot()==expected.state and k.phases==expected.phases,"恢复后下一真实半日完整状态/结果/阶段相等 "+row.label)
		covered[row.label]=true
	for kind in Assets.ACTION_KINDS: check(covered.has(kind),"跨进程持久字节恢复覆盖 "+kind)

## 确认注册回调实际命中与旧入口关闭；capture模式只在冻结旧项目执行有限轨迹。
func run_checks() -> void:
	var k=fresh()
	if not journey(k): finish(); return
	if not write_trace.is_empty():
		if not check(not FileAccess.file_exists(write_trace) and not FileAccess.file_exists(write_trace+".meta.json"),"旧期望输出不得覆盖"): finish(); return
		var file=FileAccess.open(write_trace,FileAccess.WRITE)
		if check(file!=null,"旧轨迹可保存"): file.store_string(JSON.stringify(trace,"",true,true)); file.close()
		var metadata=FileAccess.open(write_trace+".meta.json",FileAccess.WRITE)
		if check(metadata!=null,"旧进程身份可保存"):
			metadata.store_string(JSON.stringify({"producer_pid":OS.get_process_id(),"engine":Engine.get_version_info().string,"script_sha256":FileAccess.get_file_as_string("res://tests/test_registered_military.gd").sha256_text()})); metadata.close()
		finish(); return
	if not compare_trace.is_empty():
		var old: Dictionary=Strict.parse_text(FileAccess.get_file_as_string(compare_trace),67108864)
		check(old.ok and old.value==trace,"冻结旧源码与新生产完整结果/状态/三阶段轨迹相等")
		var metadata: Dictionary=Strict.parse_text(FileAccess.get_file_as_string(compare_trace+".meta.json"))
		check(metadata.ok and metadata.value.producer_pid!=OS.get_process_id() and metadata.value.script_sha256==FileAccess.get_file_as_string("res://tests/test_registered_military.gd").sha256_text(),"同一测试脚本在不同引擎进程保存和恢复")
		if old.ok: restored_trace(old.value)
		if old.ok and old.value!=trace:
			for index in mini(old.value.size(),trace.size()):
				if old.value[index]!=trace[index]: printerr("FIRST_TRACE_DIFFERENCE=",index," ",trace[index].label); break
	for kind in Assets.KINDS:
		check(k.probe.calls.get(kind+":preview",0)>0,"真实注册preview "+kind)
		if kind in Assets.ACTION_KINDS:
			for stage in ["prepare","start","step","finish","validate"]: check(k.probe.calls.get(kind+":"+stage,0)>0,"真实注册生命周期 "+kind+" "+stage)
		else: check(k.probe.calls.get(kind+":apply",0)>0,"真实注册instant apply "+kind)
	var state: Dictionary=k.snapshot(); var untouched: Dictionary=state.duplicate(true)
	check(k._begin_action(state,state.player_actor_id,"office_accept",{},{"duration":1}).get("code")=="UnsupportedAction" and state==untouched,"旧军事begin成功分支已删除")
	coverage(); rollback(); finish()

## 当前计数不代表19命令全部迁移，旧轨迹缺失时不声称独立兼容比较已经执行。
func finish() -> void:
	print("REGISTERED_MILITARY_CHECKS=",checks," FAILURES=",failures)
	quit(1 if failures else 0)
