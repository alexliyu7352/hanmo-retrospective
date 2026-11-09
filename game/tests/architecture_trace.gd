## 架构基线专用真实Kernel轨迹与性能入口；不改变规则，不在辅助层结算玩法。
extends SceneTree
const Kernel=preload("res://core/world_kernel.gd")
const Codec=preload("res://core/save_codec.gd")
const Strict=preload("res://core/strict_json.gd")
const Interaction=preload("res://core/interaction_rules.gd")
const Encounters=preload("res://core/encounter_rules.gd")
const Sessions=preload("res://core/combat_session.gd")
const PersonalRound=preload("res://core/personal_round.gd")
const Plans=preload("res://tests/military_combat_fixture.gd")
const SiegeWorld=preload("res://core/siege_world.gd")
const Siege=preload("res://core/siege_rules.gd")

## 只递归排序Dictionary键，数组和所有原始标量原样保留并脱离输入引用。
static func normalize_state(state: Dictionary) -> Dictionary:
	return normalize_value(state)

## 递归复制JSON值；数组顺序属于游戏真值，不允许排序或过滤字段。
static func normalize_value(value: Variant) -> Variant:
	if value is Dictionary:
		var result: Dictionary={}; var keys: Array=value.keys(); keys.sort()
		for key in keys: result[key]=normalize_value(value[key])
		return result
	if value is Array:
		var result: Array=[]
		for row in value: result.append(normalize_value(row))
		return result
	return value

## 引擎JSON递归排序字典键并保留数组，等价于规范化；避免为每次摘要再复制全状态。
static func state_digest(state: Dictionary) -> String:
	return JSON.stringify(state,"",true,true).sha256_text()

## 唯一证据先写临时字节、flush并实际读回，原子重命名；已有最终或临时产物都不覆盖。
static func write_artifact(path: String, text: String) -> bool:
	var temporary: String=path+".tmp"
	if FileAccess.file_exists(path) or FileAccess.file_exists(temporary): return false
	var file=FileAccess.open(temporary,FileAccess.WRITE)
	if file==null: return false
	file.store_string(text); file.flush()
	var error: Error=file.get_error(); file.close()
	if error!=OK or FileAccess.get_file_as_bytes(temporary)!=text.to_utf8_buffer(): return false
	if DirAccess.rename_absolute(temporary,path)!=OK: return false
	return FileAccess.get_file_as_bytes(path)==text.to_utf8_buffer()

## 新目录只允许采集器明确新开的日志/启动回执，任何旧局部节点或临时产物都必须保留并拒绝。
static func output_error(folder: String, collector_log: String="") -> String:
	if folder.is_empty(): return "缺少独立输出目录"
	if DirAccess.make_dir_recursive_absolute(folder)!=OK: return "独立输出目录无法创建"
	var directory=DirAccess.open(folder)
	if directory==null: return "独立输出目录无法读取"
	var allowed: Array=[]
	if collector_log=="run.log": allowed=["run.log","run.started.json"]
	for name in directory.get_files():
		if not name in allowed: return "已有证据产物不得覆盖："+name
	if not directory.get_directories().is_empty(): return "已有证据子目录不得覆盖"
	return ""

## 基线记录器只提交实际命令、查询公开规则和保存真实稳定节点，失败保留已有证据。
class Recorder extends RefCounted:
	var kernel=Kernel.new()
	var trace: Dictionary={}
	var folder: String=""
	var content_path: String=""
	var restore_nodes: bool=false
	var last_command: Dictionary={}
	var checks: int=0
	var failure: String=""

	## 内部类通过同一脚本辅助接口计算摘要，不复制第二套规范化实现。
	func _digest(state: Dictionary) -> String:
		return load("res://tests/architecture_trace.gd").state_digest(state)

	## 复用无损规范化，内部类不能依靠隐式外层成员解析。
	func _normalize(state: Dictionary) -> Dictionary:
		return load("res://tests/architecture_trace.gd").normalize_state(state)

	## 从未修改的作者目录新建世界，固定seed直接来自对应scenario。
	func open(case: Dictionary, content_path: String, output: String, restore_each: bool) -> bool:
		folder=output; restore_nodes=restore_each; self.content_path=content_path
		if DirAccess.make_dir_recursive_absolute(folder)!=OK: return fail("节点目录无法创建")
		var result: Dictionary=kernel.new_game(case.scenario_id,content_path)
		if not result.ok: return fail("真实开局失败："+str(result))
		var content: Dictionary=kernel.content(); var state: Dictionary=kernel.snapshot()
		trace={"format":"HM-ARCH-TRACE/1","case_id":case.case_id,"content_id":state.content_id,"content_digest":state.content_digest,"seed":content.data.scenarios[case.scenario_id].seed_text,"initial_rng":state.rng.duplicate(true),"commands":case.commands.duplicate(true),"steps":[],"checkpoints":[],"ok":false}
		return true

	## 报告第一处实际失败，不继续依赖它产生假终局。
	func fail(message: String) -> bool:
		failure=message; printerr("FAIL: ",message)
		return false

	## 解析夹具中的当前实例引用，选择现有合法实体，不创建任务、军团或孩子。
	func resolve(value: Variant) -> Variant:
		if value is Dictionary:
			var result: Dictionary={}
			for key in value: result[key]=resolve(value[key])
			return result
		if value is Array:
			var result: Array=[]
			for row in value: result.append(resolve(row))
			return result
		if not value is String or not value.begins_with("@"): return value
		var state: Dictionary=kernel.snapshot(); var player: String=state.player_actor_id
		if value.begins_with("@task_budget:"):
			for task in state.tasks.values():
				if task.offer_id==value.trim_prefix("@task_budget:"): return task.budget_wallet_id
		if value=="@encounter": return Encounters.pending_for(state,player).get("id","")
		if value=="@battle": return Sessions.for_actor(state,player).get("id","")
		if value=="@siege": return SiegeWorld.for_actor(state,player)
		if value=="@child":
			if not state.family.children.is_empty(): return state.family.children.keys()[0]
		if value in ["@campaign","@army"]:
			for campaign in state.campaigns.values():
				if campaign.actor_id==player: return campaign.id if value=="@campaign" else campaign.army_id
		return value

	## 为每次实际提交记录完整前后摘要与真实结果；预览和失败都严格零副作用。
	func command(kind: String, params: Dictionary={}, expected_code: String="", revision_delta: int=0, replay_last: bool=false) -> bool:
		var before: Dictionary=kernel.snapshot(); var player: String=before.player_actor_id
		var input: Dictionary=last_command.duplicate(true) if replay_last else {"kind":kind,"params":params,"actor_id":player,"request_seq":before.request_sequences[player]+1,"expected_revision":before.revision+revision_delta}
		var preview: Dictionary=kernel.preview(input.kind,input.params,input.actor_id)
		checks+=1
		if kernel.snapshot()!=before: return fail("预览改变完整快照："+kind)
		var result: Dictionary=kernel.submit(input); var after: Dictionary=kernel.snapshot()
		trace.steps.append({"command":input.duplicate(true),"preview":preview,"result":result.duplicate(true),"before_sha256":_digest(before),"state_sha256":_digest(after)})
		last_command=input.duplicate(true)
		checks+=1
		if not result.ok and after!=before: return fail("失败命令有副作用："+kind)
		if not expected_code.is_empty():
			if result.get("code","")!=expected_code: return fail("失败诊断不符："+kind+" "+str(result))
		elif not result.ok: return fail("命令失败："+kind+" "+str(result))
		if replay_last and (after!=before or trace.steps[-2].result!=result): return fail("重复request_seq不等价")
		return true

	## 将完整HM-SAVE字节写到唯一节点，解码与新Kernel恢复必须保持所有字段一致。
	func checkpoint(label: String) -> bool:
		var before: Dictionary=kernel.snapshot(); var content: Dictionary=kernel.content()
		var text: String=Codec.encode(before,trace.checkpoints.size(),label,content)
		if text.is_empty(): return fail("节点无法保存："+label)
		var filename: String=label+".hmsave"
		var path: String=folder.path_join(filename)
		if not load("res://tests/architecture_trace.gd").write_artifact(path,text): return fail("无法持久写节点："+label)
		var persisted: String=FileAccess.get_file_as_string(path)
		var decoded: Dictionary=Codec.decode(persisted,content)
		checks+=1
		if not decoded.ok or _normalize(before)!=_normalize(decoded.get("state",{})): return fail("完整存读轨迹不一致："+label)
		var resumed=Kernel.new()
		if not resumed.new_game(before.scenario_id,content_path).ok or not resumed.restore(decoded.state).ok or _normalize(before)!=_normalize(resumed.snapshot()): return fail("新Kernel恢复不等价："+label)
		trace.checkpoints.append({"label":label,"step":trace.steps.size(),"tick":before.tick,"revision":before.revision,"save":filename,"save_sha256":FileAccess.get_sha256(path),"state_sha256":_digest(before),"after_restore_sha256":_digest(resumed.snapshot())})
		if restore_nodes: kernel=resumed
		return true

	## 逐半日沿真实道路行进；普通路遇按正常道别，指定可战目标时保留原接触。
	func travel_until(city: String, conflict: bool=false, military_contact: bool=false, limit: int=45) -> bool:
		for _half in range(limit):
			var state: Dictionary=kernel.snapshot(); var player: String=state.player_actor_id
			if military_contact:
				for battle in state.battles.values():
					if battle.get("kind")=="military" and battle.controller_actor_id==player and battle.phase!="closed": return true
			var meeting: Dictionary=Encounters.pending_for(state,player)
			if not meeting.is_empty():
				if conflict and kernel.preview("encounter_fight",{"encounter_id":meeting.id}).ok: return true
				if not command("encounter_leave",{"encounter_id":meeting.id}): return false
			state=kernel.snapshot()
			if not city.is_empty() and state.actors[player].location.get("settlement_id","")==city: return true
			if not command("advance"): return false
		return fail("真实旅行未在限定半日到达所需节点")

	## 测试操作者复用现有公开战术计划，逐轮通过同一Kernel战斗入口提交。
	func battle_until_result(policy_actor: String="actor.wei", military: bool=false) -> bool:
		for _round in range(40):
			var state: Dictionary=kernel.snapshot(); var battle: Dictionary=Sessions.for_actor(state,state.player_actor_id)
			if military:
				for candidate in state.battles.values():
					if candidate.get("kind")=="military" and candidate.controller_actor_id==state.player_actor_id and candidate.phase!="closed": battle=candidate; break
			if battle.is_empty(): return fail("未找到真实战斗")
			if battle.phase=="result": return true
			if military:
				if not command("military_plan",{"battle_id":battle.id,"orders":Plans.plans(battle,kernel.content().data)}) or not command("advance"): return false
			else:
				var policy: Dictionary=battle.duplicate(true); policy.controller_id=policy_actor
				var plan: Dictionary=PersonalRound.make_ai(policy,kernel.content().data)[state.player_actor_id]
				plan.battle_id=battle.id
				if not command("battle_plan",plan): return false
		return fail("战斗未在限定真实轮次完成")

	## 复用现有围城样本策略，经实际节点军议、会战和半日推进形成占领结果。
	func siege_until_result() -> bool:
		for _half in range(100):
			var state: Dictionary=kernel.snapshot(); var player: String=state.player_actor_id
			var siege_id: String=SiegeWorld.for_actor(state,player)
			if siege_id.is_empty(): return fail("未找到真实围城")
			var siege: Dictionary=state.sieges[siege_id]
			if siege.phase=="result": return true
			var closed: bool=false
			for battle in state.battles.values():
				if battle.get("kind")!="military" or battle.controller_actor_id!=player or battle.phase=="closed": continue
				if battle.phase=="result":
					if not command("military_close",{"battle_id":battle.id}): return false
					closed=true; break
				if not battle.plan_submitted and not command("military_plan",{"battle_id":battle.id,"orders":Plans.plans(battle,kernel.content().data)}): return false
			if closed: continue
			state=kernel.snapshot(); siege=state.sieges[siege_id]
			var obs: Dictionary=SiegeWorld.observe(state,kernel.content().data,siege)
			var ai_obs: Dictionary=obs.duplicate(true); ai_obs.controller_actor_id=""
			var ai: Dictionary=Siege.make_ai(siege,kernel.content().data,ai_obs)
			if not ai.ok: return fail("公开围城策略失败："+str(ai))
			var orders: Dictionary={}
			for id in obs.control_sets.get(player,[]):
				if ai.orders.has(id): orders[id]=ai.orders[id]
			if not orders.is_empty() and not siege.plan_submitted and not command("siege_plan",{"siege_id":siege_id,"orders":orders}): return false
			if not command("advance"): return false
		return fail("围城未在限定真实半日完成")

	## 将声明输入展开为实际命令；循环只提供操作者输入，不能直接改状态或推进tick。
	func execute(operations: Array) -> bool:
		for operation in operations:
			match operation.op:
				"checkpoint":
					if not checkpoint(operation.label): return false
				"submit":
					var params: Dictionary=resolve(operation.get("params",{}))
					if operation.has("context_role"): params.context=Interaction.for_role(kernel.snapshot(),kernel.content().data,operation.context_role)
					if not command(operation.kind,params,operation.get("expected_code",""),operation.get("revision_delta",0)): return false
				"repeat_last":
					if not command("",{},"",0,true): return false
				"advance":
					for _half in range(operation.get("ticks",1)):
						if not command("advance"): return false
				"travel_until":
					if not travel_until(operation.get("city",""),operation.get("conflict",false),operation.get("military_contact",false),operation.get("limit",45)): return false
				"battle_until_result":
					if not battle_until_result(operation.get("policy_actor","actor.wei"),operation.get("military",false)): return false
				"siege_until_result":
					if not siege_until_result(): return false
				_: return fail("未知测试输入操作："+str(operation))
		return true

## 统计实际完整快照规模，不把定义条目误当成运行实体。
static func counts(state: Dictionary) -> Dictionary:
	var entities: int=0
	for name in ["actors","wallets","inventories","roles","tasks","armies","military_units","sieges","battles"]: entities+=state.get(name,{}).size()
	return {"entities":entities,"actors":state.actors.size(),"actions":state.actions.size(),"facts":state.facts.size(),"history":state.history.size(),"tick":state.tick,"revision":state.revision}

## 用正式wait/advance提交度过所需半日；无合法入口时明确失败，不加tick或关闭校验。
static func lawful_advance(kernel, ticks: int) -> Dictionary:
	var kernel_us: int=0; var submit_count: int=0
	for index in range(ticks):
		if not kernel.preview("advance").ok:
			var state: Dictionary=kernel.snapshot(); var player: String=state.player_actor_id
			var began: int=Time.get_ticks_usec()
			var wait: Dictionary=kernel.submit({"kind":"wait","params":{},"actor_id":player,"request_seq":state.request_sequences[player]+1,"expected_revision":state.revision})
			kernel_us+=Time.get_ticks_usec()-began; submit_count+=1
			if not wait.ok: return {"ok":false,"step":index,"result":wait}
		var began: int=Time.get_ticks_usec()
		var result: Dictionary=kernel.advance_one_tick()
		kernel_us+=Time.get_ticks_usec()-began; submit_count+=1
		if not result.ok: return {"ok":false,"step":index,"result":result}
		if (index+1)%720==0: print("ARCH_PERF_PROGRESS halfdays=",index+1," kernel_us=",kernel_us," submit_count=",submit_count)
	return {"ok":true,"kernel_us":kernel_us,"submit_count":submit_count}

## 通过同一个正常命令入口比较未中断状态和真实字节恢复后的短窗，不能只比RNG。
static func continuation(content_path: String, state: Dictionary, content: Dictionary, text: String, ticks: int) -> Dictionary:
	var original=Kernel.new(); var restored=Kernel.new()
	for kernel in [original,restored]:
		if not kernel.new_game(state.scenario_id,content_path).ok: return {"ok":false,"reason":"短窗新Kernel创建失败"}
	var decoded: Dictionary=Codec.decode(text,content)
	if not decoded.ok or not original.restore(state).ok or not restored.restore(decoded.state).ok: return {"ok":false,"reason":"短窗保存恢复失败"}
	var first: Dictionary=lawful_advance(original,ticks); var second: Dictionary=lawful_advance(restored,ticks)
	var first_state: Dictionary=original.snapshot(); var second_state: Dictionary=restored.snapshot()
	return {"ok":first.ok and second.ok and normalize_state(first_state)==normalize_state(second_state),"ticks":ticks,"uninterrupted_sha256":state_digest(first_state),"restored_sha256":state_digest(second_state)}

## 固定节点存完整字节并核验全部字段；独立短窗不能污染主耐久世界的日期和请求序号。
static func endurance_checkpoint(kernel, content_path: String, folder: String, continuation_ticks: int, index: int) -> Dictionary:
	var state: Dictionary=kernel.snapshot(); var content: Dictionary=kernel.content()
	var text: String=Codec.encode(state,index,"endurance-"+str(state.tick),content)
	if text.is_empty(): return {"ok":false,"reason":"耐久节点保存失败"}
	var filename: String="tick-"+str(state.tick)+".hmsave"
	var path: String=folder.path_join(filename)
	if not write_artifact(path,text): return {"ok":false,"reason":"耐久节点持久写入失败"}
	var persisted: String=FileAccess.get_file_as_string(path)
	var decoded: Dictionary=Codec.decode(persisted,content)
	if not decoded.ok or normalize_state(state)!=normalize_state(decoded.state): return {"ok":false,"reason":"耐久节点完整Codec不等价"}
	var compared: Dictionary=continuation(content_path,state,content,persisted,continuation_ticks)
	return {"ok":compared.ok,"tick":state.tick,"counts":counts(state),"save":filename,"save_sha256":FileAccess.get_sha256(path),"save_bytes":persisted.to_utf8_buffer().size(),"state_sha256":state_digest(state),"after_decode_sha256":state_digest(decoded.state),"continuation":compared}

## 一次真实连续耐久沿每年节点采证，不重复7200，也不声称整个耐久的两遍等价。
static func endurance(content_path: String, scenario: String, folder: String, ticks: int=7200, interval: int=720, continuation_ticks: int=30) -> Dictionary:
	var kernel=Kernel.new(); var opened: Dictionary=kernel.new_game(scenario,content_path)
	if not opened.ok: return {"ok":false,"reason":str(opened)}
	var report: Dictionary={"ok":false,"target_ticks":ticks,"interval":interval,"continuation_ticks":continuation_ticks,"content_digest":kernel.snapshot().content_digest,"checkpoints":[],"segments":[],"scope":"单seed单次连续耐久；仅节点后短窗保存恢复等价，不声明整段7200重放"}
	var initial: Dictionary=endurance_checkpoint(kernel,content_path,folder,continuation_ticks,0)
	report.checkpoints.append(initial)
	if not initial.ok: report.reason="初态短窗不等价"; return report
	var elapsed_total: int=0; var kernel_total: int=0
	for index in range(ticks/interval):
		var began: int=Time.get_ticks_usec(); var advanced: Dictionary=lawful_advance(kernel,interval)
		var elapsed: int=Time.get_ticks_usec()-began
		var segment: Dictionary={"start_tick":index*interval,"end_tick":kernel.snapshot().tick,"elapsed_us":elapsed,"kernel_us":advanced.get("kernel_us",0),"submit_count":advanced.get("submit_count",0),"ok":advanced.ok}
		report.segments.append(segment); elapsed_total+=elapsed; kernel_total+=segment.kernel_us
		if not advanced.ok: report.reason=str(advanced); return report
		var node: Dictionary=endurance_checkpoint(kernel,content_path,folder,continuation_ticks,index+1)
		report.checkpoints.append(node)
		print("ARCH_ENDURANCE_CHECKPOINT tick=",node.tick," elapsed_us=",elapsed_total," kernel_us=",kernel_total," equivalent=",node.ok)
		if not write_artifact(folder.path_join("checkpoint-"+str(index+1)+".json"),JSON.stringify(report,"\t",true,true)+"\n"):
			report.reason="耐久局部回执持久写入失败"; return report
		if not node.ok: report.reason="稳定节点或短窗不等价"; return report
	report.ok=kernel.snapshot().tick==ticks; report.elapsed_us=elapsed_total; report.kernel_us=kernel_total
	report.final_save=report.checkpoints[-1].save; report.final_state_sha256=state_digest(kernel.snapshot())
	return report

## 从相同完整起点实测加载、十次同输入、720窗和保存，预热不混入五次样本。
static func performance(content_path: String, scenario: String, folder: String, lengths: Array, repetitions: int=5, initial_save: String="") -> Dictionary:
	var source=Kernel.new(); var opened: Dictionary=source.new_game(scenario,content_path)
	if not opened.ok: return {"ok":false,"reason":str(opened)}
	if not initial_save.is_empty():
		var initial: Dictionary=Codec.decode(FileAccess.get_file_as_string(initial_save),source.content())
		if not initial.ok or not source.restore(initial.state).ok: return {"ok":false,"reason":"真实耐久终点输入无法恢复"}
	var state: Dictionary=source.snapshot(); var content: Dictionary=source.content()
	var text: String=Codec.encode(state,0,"architecture-performance",content)
	if text.is_empty(): return {"ok":false,"reason":"负载不能真实保存"}
	var initial_path: String=folder.path_join("initial.hmsave")
	if not write_artifact(initial_path,text): return {"ok":false,"reason":"性能起点持久写入失败"}
	var report: Dictionary={"ok":true,"initial_counts":counts(state),"initial_state_sha256":state_digest(state),"save_bytes":text.to_utf8_buffer().size(),"initial_save":{"save":"initial.hmsave","save_sha256":FileAccess.get_sha256(initial_path)},"initial_save_source":initial_save,"initial_save_source_sha256":"" if initial_save.is_empty() else FileAccess.get_sha256(initial_save),"samples":[],"warmups":[],"content_digest":state.content_digest}
	for repetition in range(-1,repetitions):
		var sample: Dictionary={"repetition":repetition}; var kernel=Kernel.new()
		var began: int=Time.get_ticks_usec()
		var loaded: Dictionary=kernel.new_game(scenario,content_path)
		var decoded: Dictionary=Codec.decode(FileAccess.get_file_as_string(folder.path_join("initial.hmsave")),kernel.content())
		if not loaded.ok or not decoded.ok or not kernel.restore(decoded.state).ok: return {"ok":false,"reason":"真实负载加载失败"}
		sample.load_us=Time.get_ticks_usec()-began
		sample.load_ok=true
		var command_samples: Array=[]; var command_result: Dictionary={}; var command_state: Dictionary={}
		for _command in range(10):
			if not kernel.restore(state).ok: return {"ok":false,"reason":"同输入起点恢复失败"}
			var player: String=state.player_actor_id
			var input: Dictionary={"kind":"end_interaction","params":{},"actor_id":player,"request_seq":state.request_sequences[player]+1,"expected_revision":state.revision}
			began=Time.get_ticks_usec(); var result: Dictionary=kernel.submit(input)
			command_samples.append(Time.get_ticks_usec()-began)
			if not result.ok: return {"ok":false,"reason":"同输入实际提交失败："+str(result)}
			if command_result.is_empty(): command_result=result; command_state=kernel.snapshot()
			elif command_result!=result or command_state!=kernel.snapshot(): return {"ok":false,"reason":"十次同输入完整结果不一致"}
		sample.command_10_us=command_samples
		sample.command_result=command_result; sample.command_state_sha256=state_digest(command_state)
		var advances: Dictionary={}
		for length in lengths:
			if not kernel.restore(state).ok: return {"ok":false,"reason":"推进起点恢复失败"}
			began=Time.get_ticks_usec(); var advanced: Dictionary=lawful_advance(kernel,int(length))
			advances[str(length)]={"start_tick":state.tick,"end_tick":kernel.snapshot().tick,"elapsed_us":Time.get_ticks_usec()-began,"kernel_us":advanced.get("kernel_us",0),"submit_count":advanced.get("submit_count",0),"ok":advanced.ok,"counts":counts(kernel.snapshot()),"state_sha256":state_digest(kernel.snapshot())}
			if not advanced.ok: return {"ok":false,"reason":str(advanced),"partial":advances}
		sample.advances=advances
		if not kernel.restore(state).ok: return {"ok":false,"reason":"保存起点恢复失败"}
		began=Time.get_ticks_usec(); var saved: String=Codec.encode(kernel.snapshot(),0,"architecture-performance",content)
		var save_name: String="saved-"+str(repetition)+".hmsave"; var save_path: String=folder.path_join(save_name)
		if not write_artifact(save_path,saved): return {"ok":false,"reason":"性能样本文件无法持久写出"}
		sample.save_us=Time.get_ticks_usec()-began
		if saved!=text or FileAccess.get_file_as_string(save_path)!=text: return {"ok":false,"reason":"相同完整起点持久保存字节不一致"}
		sample.save=save_name; sample.save_sha256=FileAccess.get_sha256(save_path); sample.save_ok=true
		if repetition<0: report.warmups.append(sample)
		else: report.samples.append(sample)
		if not write_artifact(folder.path_join("sample-"+str(repetition)+".json"),JSON.stringify(sample,"\t",true,true)+"\n"):
			return {"ok":false,"reason":"性能局部回执持久写入失败"}
		print("ARCH_PERF_SAMPLE repetition=",repetition," load_us=",sample.load_us," save_us=",sample.save_us," advances=",advances)
	return report

## 实际命令行参数控制唯一证据目录，不覆盖已有回执或历史玩家存档。
func _init() -> void:
	call_deferred("run_entry")

## 运行轨迹或性能模式；任何失败均写诊断并以非零退出供采集器识别。
func run_entry() -> void:
	var args: Dictionary={}
	for argument in OS.get_cmdline_user_args():
		if argument.begins_with("--") and argument.contains("="):
			var pair: PackedStringArray=argument.trim_prefix("--").split("=",true,1); args[pair[0]]=pair[1]
	var folder: String=args.get("out","")
	var destination_error: String=output_error(folder,args.get("collector-log",""))
	if not destination_error.is_empty(): printerr("FAIL: ",destination_error); quit(1); return
	var final_name: String=args.get("mode")+".json" if args.get("mode","trace") in ["performance","endurance"] else "trace.json"
	var report: Dictionary={}; var checks: int=1
	if args.get("mode","trace") in ["performance","endurance"] and not args.has("content"):
		printerr("FAIL: 缺少性能内容入口"); quit(1); return
	if args.get("mode","trace")=="performance":
		var lengths: Array=[]
		for value in args.get("lengths","720").split(","): lengths.append(int(value))
		report=performance(args.content,args.get("scenario","scenario.alpha_life"),folder,lengths,int(args.get("repetitions","5")),args.get("initial-save",""))
	elif args.get("mode","")=="endurance":
		report=endurance(args.content,args.get("scenario","scenario.alpha_life"),folder,int(args.get("ticks","7200")),int(args.get("interval","720")),int(args.get("continuation-ticks","30")))
	else:
		var parsed: Dictionary=Strict.parse_text(FileAccess.get_file_as_string(args.get("case","")))
		if not parsed.ok: printerr("FAIL: 输入夹具无法解析"); quit(1); return
		var case: Dictionary=parsed.value; var path: String=case.get("content","res://data/generated/alpha_life.hmcatalog.json")
		if path=="@military": path=args.get("military-content","")
		var recorder=Recorder.new()
		var opened: bool=recorder.open(case,path,folder,args.get("restore-nodes","false")=="true")
		var done: bool=opened and recorder.execute(case.commands)
		report=recorder.trace; report.ok=done; report.reason=recorder.failure; checks=recorder.checks
		if done: report.final_state_sha256=state_digest(recorder.kernel.snapshot()); report.final_counts=counts(recorder.kernel.snapshot())
	if not write_artifact(folder.path_join(final_name),JSON.stringify(report,"\t",true,true)+"\n"):
		printerr("FAIL: 证据回执持久写出失败"); quit(1); return
	print("ARCHITECTURE_TRACE_CHECKS=",checks," FAILURES=",0 if report.get("ok",false) else 1)
	quit(0 if report.get("ok",false) else 1)
