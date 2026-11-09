## 注册行动生命周期与旅行、休整、收养、编组返营的特殊时钟边界；不统一旧领域规则。
extends SceneTree
const Kernel=preload("res://core/world_kernel.gd")
const Registry=preload("res://core/runtime/module_registry.gd")
const Builtin=preload("res://core/runtime/builtin_modules.gd")
const Trade=preload("res://core/domains/economy/trade_action.gd")
const Step=preload("res://core/world_step.gd")
const Codec=preload("res://core/save_codec.gd")
const F=preload("res://tests/companion_fixture.gd")
var checks: int=0
var failures: int=0
var Runtime: GDScript
var Oracle: GDScript

## 缺失调度接口由断言报告，而不是未存在文件的解析失败。
func _init() -> void:
	call_deferred("run_checks")

## 精确计数并保留生命周期发生点，失败不能被退出码或总数掩盖。
func check(value: bool, label: String) -> bool:
	checks+=1
	if not value: failures+=1; printerr("FAIL: ",label)
	return value

## 包装实际交易实现，只有指定返回分支用于覆盖通用调度优先级。
class CountedTrade extends RefCounted:
	const Real=preload("res://core/domains/economy/trade_action.gd")
	var calls: Array=[]
	var mode: String=""
	var occupied_at_start: bool=false
	var context_keys: Array=[]
	var expected_action_id: String=""
	## 真实prepare只准备参数；不提前step或finish。
	func prepare(state: Dictionary,data: Dictionary,actor: String,kind: String,params: Dictionary,offer: Dictionary) -> Dictionary:
		calls.append("prepare")
		return Real.prepare(state,data,actor,kind,params,offer)
	## 已分配行动并占用全部参与者后才进入实际转账起点。
	func start(state: Dictionary,data: Dictionary,action: Dictionary,offer: Dictionary) -> Dictionary:
		calls.append("start"); occupied_at_start=state.actions.has(action.id) and action.id==expected_action_id
		for participant in action.participants: occupied_at_start=occupied_at_start and state.actors[participant].action_id==action.id
		return Real.start(state,data,action,offer)
	## 不完整和挂起分支是ABI探针，其余调用原交易step，outcome故意不能影响完成结果。
	func step(state: Dictionary,data: Dictionary,action: Dictionary,context: Dictionary) -> Dictionary:
		calls.append("step"); context_keys=context.keys(); context_keys.sort()
		if mode=="failure": return {"ok":false,"code":"InjectedStep","detail":[1,2]}
		if mode=="incomplete": return {"ok":true,"completed":false,"suspended":false,"outcome":{"ok":false,"code":"IgnoredStepOutcome"}}
		if mode=="suspended": return {"ok":true,"completed":true,"suspended":true,"outcome":{"ok":true,"success":false}}
		if mode=="alias":
			context.hold.append("test_hold"); context.contacted["test_contact"]=true
			return {"ok":true,"completed":false,"suspended":true}
		var result: Dictionary=Real.step(state,data,action,context)
		result.outcome={"ok":false,"code":"IgnoredStepOutcome"}
		return result
	## 真正完成调用原finish一次，业务success=false不变成世界失败。
	func finish(state: Dictionary,data: Dictionary,action: Dictionary,tick: int) -> Dictionary:
		calls.append("finish")
		var result: Dictionary=Real.finish(state,data,action,tick)
		if mode=="unsuccessful": result.success=false
		if mode=="finish_failure": return {"ok":false,"code":"InjectedFinish","detail":[3,4]}
		return result

## 从真实粮铺报价进入ActionRuntime，保留领域转账、实例分配和占用次序。
func registered() -> void:
	var k=Kernel.new(); check(k.new_game().ok,"真实交易世界启动")
	check(F.send(k,"move_local",{"place_id":"west.grain"}).ok,"公开命令抵达粮铺")
	var params: Dictionary={"shop_id":"shop.grain","item_id":"item.grain","side":"buy","quantity":2,"payer_wallet_id":"wallet.player","context":F.context(k,"role.merchant")}
	var state: Dictionary=k.snapshot(); var data: Dictionary=k.content().data; var actor: String=state.player_actor_id
	var registry: Dictionary=Registry.build(Builtin.descriptors()); var counter=CountedTrade.new()
	counter.expected_action_id="action:"+str(state.next_instance_id)
	registry.actions.trade.prepare=counter.prepare; registry.actions.trade.start=counter.start; registry.actions.trade.step=counter.step; registry.actions.trade.finish=counter.finish
	var offer: Dictionary=k._preview(state,"trade",params,actor)
	var begun: Dictionary=Runtime.begin(state,data,actor,"trade",params,offer,registry)
	if not check(begun.ok,"真实prepare/start经统一入口"): return
	var id: String=begun.details.action_id; var action: Dictionary=state.actions[id]
	check(counter.calls==["prepare","start"] and counter.occupied_at_start and action.start_tick==state.tick and action.remaining_ticks==1,"prepare→实例→占用→start，不提前推进")
	var pending: Dictionary=state.duplicate(true)
	var context: Dictionary={"finish_tick":state.tick+1,"hold":[actor],"contacted":{actor:true}}
	counter.mode="incomplete"
	var result: Dictionary=Runtime.step(state,data,action,registry.actions,context)
	check(result.ok and not result.completed and not result.suspended and result.outcome=={"ok":true,"success":true} and not "finish" in counter.calls,"不完整不finish，忽略step.outcome")
	counter.mode="suspended"; result=Runtime.step(state,data,action,registry.actions,context)
	check(result.ok and result.suspended and not result.completed and not "finish" in counter.calls,"挂起优先于同时completed")
	counter.mode="failure"
	check(Runtime.step(state,data,action,registry.actions,context)=={"ok":false,"code":"InjectedStep","detail":[1,2]},"step首错完整原样透传")
	counter.mode="alias"; Runtime.step(state,data,action,registry.actions,context)
	check(context.hold==[actor,"test_hold"] and context.contacted.has("test_contact") and counter.context_keys==["contacted","finish_tick","hold"],"旧小context只保留三个键及内部别名")
	counter.mode="unsuccessful"; counter.calls=[]
	var caller_hold: Array=[actor]
	result=Step.advance(state,data,registry,caller_hold)
	check(result.ok and result.details.completed_actions[id]=={"ok":true,"success":false} and counter.calls==["step","finish"],"业务失败仍完成半日且finish恰一次")
	check(state.tick==pending.tick+1 and not state.actions.has(id) and state.actors[actor].action_id.is_empty() and caller_hold==[actor],"完成释放实际占用且不污染调用者hold")
	state=pending.duplicate(true); counter.mode="finish_failure"; counter.calls=[]
	check(Runtime.step(state,data,state.actions[id],registry.actions,context)=={"ok":false,"code":"InjectedFinish","detail":[3,4]} and counter.calls==["step","finish"],"finish失败原样透传且只调用一次")
	var absent: Dictionary=registry.duplicate(true); absent.actions.erase("trade"); state=pending.duplicate(true)
	check(Step.advance(state,data,absent).get("code")=="UnsupportedAction","已编译图读每次当前动作表，删trade不会被启动捕获掩盖")
	check(Runtime.step(state,data,state.actions[id],{},context).get("code")=="UnsupportedAction","通用调度拒绝不存在绑定")

## 精确取已经比较过的阶段，缺失即保留空值让上层断言失败。
func row(record: Dictionary, id: String) -> Dictionary:
	for boundary in record.rows:
		if boundary.id==id: return boundary
	return {}

## 旧路径特殊边界通过实际新图重放，并逐点断言真实占用、时钟与领域结果。
func special(records: Array) -> void:
	var coverage: Dictionary={}
	var registry: Dictionary=Registry.build(Builtin.descriptors())
	for old in records:
		if not old.result.ok: continue
		var replay: Dictionary=Oracle.compare(old,registry)
		if not check(replay.ok,"特殊行动阶段重放全状态 "+old.label): continue
		var record: Dictionary=old.duplicate(true); record.rows=replay.rows
		var actions: Dictionary=row(record,"actions_step"); var clock: Dictionary=row(record,"clock_commit")
		var final: Dictionary=replay.final
		for action in old.initial.actions.values():
			if action.kind=="family_adopt" and action.remaining_ticks==1:
				coverage.family=true
				check(actions.state.actions.has(action.id) and clock.state.actions.has(action.id),"收养减至零仍保留到新时钟")
				for participant in action.participants: check(actions.state.actors[participant].action_id==action.id and clock.state.actors[participant].action_id==action.id,"收养本人和管事仍占用")
				check(clock.state.tick==old.initial.tick+1 and not final.actions.has(action.id) and final.actors.size()==old.initial.actors.size()+1,"阶段17才创建子女并释放")
			if action.kind=="army_form":
				coverage.form=true
				var completed: Dictionary=actions.context.outcomes.get(action.id,{})
				check(completed.get("success",false) and actions.state.campaigns[completed.details.order_id].army_id==completed.details.army_id,"编组完成在actions块立即绑定军令")
			if action.kind=="army_return":
				coverage.returned=true
				check(not actions.state.actions.has(action.id) and actions.state.tick==old.initial.tick and not actions.context.military_returns.is_empty(),"返营preclock释放行动并保留回执")
				var details: Dictionary=actions.context.military_returns[-1]
				check(row(record,"military_return_complete").state.campaigns[details.order_id].status=="awaiting_report","postclock才确认真实返营")
			if action.kind=="long_rest" and actions.context.outcomes.get(action.id,{}).get("success",true)==false:
				coverage.shortage=true
				check(row(record,"estate_production").state.inventories!=old.initial.inventories and actions.state.tick==old.initial.tick and final.tick==old.initial.tick+1,"庄务先吃粮，休整缺粮仍正常过半日")
		if not old.hold.is_empty() and old.label=="test_world_step":
			coverage.hold=true
			for actor in old.hold: check(final.actors[actor].location==old.initial.actors[actor].location,"hold本人真实行旅不移动")
			check(final.actors["actor.he"].location!=old.initial.actors["actor.he"].location,"另一真实旅人继续进度并到达")
		var detected: Dictionary=row(record,"contact_detect")
		if not detected.context.contacts.is_empty() and not detected.context.military_contacts.is_empty():
			coverage.contacts=true
			check(row(record,"contact_install").state.battles.size()>old.initial.battles.size(),"普通与军事接触同半日按原安装顺序")
			for id in row(record,"contact_install").state.battles:
				if not old.initial.battles.has(id): check(row(record,"contact_install").state.battles[id].round==1 and row(record,"contact_install").state.battles[id].round_receipt_ids.is_empty(),"新战场创建半日不偷跑一轮")
		var receipts: Array=row(record,"siege_results").context.get("_receipts",[])
		if not receipts.is_empty():
			coverage.receipts=true
			check(row(record,"campaign_step").context._receipts==receipts,"围城回执在军令观察之前保留原序")
		if old.label.begins_with("registered_hold"):
			coverage.registered=true
			check(not actions.context.outcomes.is_empty() and actions.context.suspended_actors.is_empty(),"旅行hold不暂停当前注册任务或交易")
			var missing: Dictionary=registry.duplicate(true)
			for action in old.initial.actions.values():
				if missing.actions.has(action.kind): missing.actions.erase(action.kind)
			check(Step.advance(old.initial.duplicate(true),old.data,missing,old.hold).get("code")=="UnsupportedAction","已编译图观察任务动作表移除")
	for key in ["family","form","returned","shortage","hold","contacts","receipts","registered"]: check(coverage.has(key),"真实特殊边界覆盖 "+key)

## 记录实际尾部begin返回时的完整新行动，用于证明本半日没有偷跑step。
class TailKernel extends "res://core/world_kernel.gd":
	var begun: Dictionary={}
	## 注册行动不经过旧begin门面，观察真实start回调才能覆盖尾部NPC军务而不修改生产入口。
	func _module_descriptors() -> Array:
		var rows: Array=super._module_descriptors()
		for module in rows:
			for action in module.actions: action.start=_registered_start.bind(action.start)
		return rows
	## 共用runtime已建信封并占用；只在真实领域start成功返回时冻结结果。
	func _registered_start(state: Dictionary, data: Dictionary, action: Dictionary, offer: Dictionary, start: Callable) -> Dictionary:
		var result: Dictionary=start.call(state,data,action,offer)
		if result.ok: _record_started(state,action,offer.duration)
		return result
	## 注册和兼容两种入口共享同一测试观察，不伪造持续时间、位置或军务完成。
	func _record_started(state: Dictionary, action: Dictionary, duration: int) -> void:
		begun[action.id]={"action":action.duplicate(true),"location":state.actors[action.actor_id].location.duplicate(true),"duration":duration}
	## 原实现先分配/占用/领域start；测试只在成功返回之后保留完整信封副本。
	func _begin_action(state: Dictionary, actor_id: String, kind: String, params: Dictionary, offer: Dictionary) -> Dictionary:
		var result: Dictionary=super._begin_action(state,actor_id,kind,params,offer)
		if result.ok:
			var id: String=result.details.action_id
			_record_started(state,state.actions[id],offer.duration)
		return result

## 只重放短稳定点，保存不带图/Callable；下一次真实submit等价且不二次完成或扣粮。
func restore_and_tail(records: Array) -> void:
	var selected: Dictionary={}; var scheduled: bool=false
	for record in records:
		if not record.result.ok: continue
		for action in record.initial.actions.values():
			var kind: String=action.kind
			if not kind in ["travel","long_rest","family_adopt","trade","accept_offer","army_return"] or selected.has(kind): continue
			selected[kind]=true
			var a=TailKernel.new(); var b=Kernel.new()
			if not check(Oracle.restore_kernel(a,record).ok and Oracle.restore_kernel(b,record).ok,"短边界真实恢复 "+kind): continue
			var encoded: String=Codec.encode(a.snapshot(),0,"phase-"+kind,a.content())
			check(not encoded.is_empty() and not '"phase_plan"' in encoded and not '"from_tick"' in encoded and not 'Callable' in encoded,"存档没有执行图或临时context "+kind)
			var decoded: Dictionary=Codec.decode(encoded,a.content())
			if not check(decoded.ok and b.restore(decoded.state).ok,"正式Codec短恢复 "+kind): continue
			var first: Dictionary=a.advance_one_tick(); var second: Dictionary=b.advance_one_tick()
			check(first.ok and second==first and a.snapshot()==b.snapshot(),"下一半日完整结果/状态一致 "+kind)
			var after: Dictionary=a.snapshot()
			for id in after.actions:
				if record.final.actions.has(id): continue
				scheduled=true
				var fresh: Dictionary=after.actions[id]
				if not check(fresh.start_tick==after.tick and a.begun.has(id) and fresh==a.begun[id].action and fresh.remaining_ticks==a.begun[id].duration,"尾部新行动与实际start返回的完整信封/原始工期完全相同"): continue
				check(after.actors[fresh.actor_id].location==a.begun[id].location,"尾部新行动保持start的真实位置/进度，不偷跑本半日")
				if fresh.kind=="travel":
					check(fresh.params.get("travel_credit",0)==0,"尾部新行旅没有提前获得移动信用")
	for key in ["travel","long_rest","family_adopt","trade","accept_offer","army_return"]: check(selected.has(key),"短Codec边界覆盖 "+key)
	check(scheduled,"真实Kernel尾部确有新行动而非空断言")

## 先验证已接受接口，再运行真实生命周期、特殊旧边界与有限读档延续。
func run_checks() -> void:
	Runtime=load("res://core/runtime/action_runtime.gd")
	if not check(Runtime.has_method("step"),"注册step/finish必须进入共同调度接口"): finish(); return
	Oracle=load("res://tests/world_phase_oracle.gd")
	registered()
	var migration: bool=false
	for argument in OS.get_cmdline_user_args():
		if argument=="--old-phase-evidence" or argument.begins_with("--old-phase-evidence="):
			migration=true
			Oracle.evidence_dir=argument.trim_prefix("--old-phase-evidence=") if "=" in argument else ""
	var records: Array=Oracle.records() if migration else Oracle.current_records()
	check(not records.is_empty(),"请求的阶段证据/当前合法夹具可加载："+str(Oracle.errors))
	special(records); restore_and_tail(records)
	finish()

## 输出明确的局部机器证据，不宣称整个人生或新的AI已验收。
func finish() -> void:
	print("ACTION_LIFECYCLE_CHECKS=",checks," FAILURES=",failures)
	quit(1 if failures else 0)
