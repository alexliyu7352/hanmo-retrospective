## 验证真实UI控制器的许可与回调边界；transport替身不证明实际Main、世界规则或Bridge性能。
extends SceneTree
const Adapter=preload("res://ui/resident_journey_adapter.gd")
var checks: int=0
var failures: int=0

## 可控ticket仅模拟签收协议，所有规则结果由用例明确交入，不运行另一套世界。
class ProtocolBridge extends RefCounted:
	var requests: Array=[]
	var pending: Dictionary={}
	var completed: Dictionary={}
	var gates: Array=[]
	var counter: int=0
	var revoke_unstarted: bool=false
	var reject_cancel_once: bool=false
	var race_after_pending: Callable
	## 正常操作只有一槽，取消独占有界优先槽；相同取消返回同一ticket。
	func enqueue_live_operation(operation: Dictionary) -> Dictionary:
		if operation.kind=="cancel_run" and reject_cancel_once:
			reject_cancel_once=false; return {"ok":false,"code":"TestCancelRejected"}
		for id in pending:
			var prior: Dictionary=pending[id]
			if operation.kind=="cancel_run" and prior.kind=="cancel_run":
				if prior.payload==operation.payload: return {"ok":true,"reference":_reference(id,prior.source)}
				return {"ok":false,"code":"ResidentBridgeBusy"}
			if operation.kind!="cancel_run": return {"ok":false,"code":"ResidentBridgeBusy"}
		counter+=1; requests.append({"id":counter,"operation":operation.duplicate(true)})
		pending[counter]=operation.duplicate(true)
		return {"ok":true,"reference":_reference(counter,operation.source)}
	## pending不阻塞；完成后保独立缓存，取消不得覆盖grant结果。
	func poll_live_operation(reference: Dictionary) -> Dictionary:
		if completed.has(reference.operation_id):
			pending.erase(reference.operation_id)
			return completed[reference.operation_id].duplicate(true)
		if race_after_pending.is_valid():
			var callback: Callable=race_after_pending; race_after_pending=Callable(); callback.call()
		return {"ok":true,"ready":false}
	## 只在指定测试中撤销未开始grant，不替测试凭空提交半日。
	func set_reading_paused(reference: Dictionary, value: bool) -> Dictionary:
		gates.append({"reference":reference.duplicate(true),"paused":value})
		if value and revoke_unstarted:
			for id in pending:
				if pending[id].kind=="grant": finish(id,{"ok":false,"code":"OperationRevoked","stop":"revoked_before_start"},pending[id].source)
		return {"ok":true}
	## 完成顺序由用例决定，用于测试执行中取消和迟到回调。
	func finish(id: int, result: Dictionary, current: Dictionary) -> void:
		var operation: Dictionary=pending[id]
		completed[id]={"ok":true,"ready":true,"tag":operation.kind,"source_identity":operation.source.duplicate(true),"current_identity":current.duplicate(true),"result":result.duplicate(true)}
	## 构造冻结run结果，不包含世界推进算法。
	func finish_run(id: int, stop: String, count: int, permit: int, current: Dictionary) -> void:
		finish(id,{"ok":true,"identity":current.duplicate(true),"run_id":pending[id].payload.run_id,"committed_half_days":count,"permit_seq":permit,"stop":stop},current)
	## 查最后一个实际接收请求；没有请求时返回0，方便断言未暗中排队。
	func latest(kind: String) -> int:
		for i in range(requests.size()-1,-1,-1):
			if requests[i].operation.kind==kind: return requests[i].id
		return 0
	## 引用完全由该transport分配并带原世界身份。
	func _reference(id: int, source: Dictionary) -> Dictionary:
		return {"world_id":source.world_id,"execution_epoch":source.execution_epoch,"operation_id":id}

## 等场景树就绪后创建真实Timer和控制器。
func _init() -> void:
	call_deferred("_run")

## 每项失败都有具体边界标签，不把检查数当实际Main验收。
func check(ok: bool, label: String) -> void:
	checks+=1
	if not ok: failures+=1; printerr("FAIL: ",label)

## 小身份只为协议夹具，实际world_id仍由真实core创建。
func identity(revision: int=0, tick: int=0, epoch: int=1) -> Dictionary:
	return {"world_id":"0123456789abcdef0123456789abcdef","execution_epoch":epoch,"revision":revision,"tick":tick}

## 每个用例独立控制器，自动_process停掉以明确决定何时消费回执；Timer仍真实运行。
func fixture() -> Dictionary:
	var adapter=Adapter.new(); root.add_child(adapter); adapter.set_process(false)
	var bridge=ProtocolBridge.new(); var events: Array=[]; var errors: Array=[]
	adapter.operation_completed.connect(func(kind,result,source,current): events.append({"kind":kind,"result":result,"source":source,"current":current}))
	adapter.failed.connect(func(error): errors.append(error))
	check(adapter.bind_context(bridge,identity(),7).ok,"绑定真实控制器与有限transport")
	return {"adapter":adapter,"bridge":bridge,"events":events,"errors":errors}

## 停驻到awaiting_permit；开始操作本身绝不由fixture赠送已提交半日。
func parked(f: Dictionary) -> void:
	check(f.adapter.start("actor.player","action:1",8).ok,"明确用户启动被接受")
	f.bridge.finish_run(f.bridge.latest("begin_run"),"awaiting_permit",0,0,identity())
	f.adapter.poll()

## 完成有界取消，供后续明确再启用或销毁使用。
func cancelled(f: Dictionary, current: Dictionary, count: int=0, permit: int=0) -> void:
	var id: int=f.bridge.latest("cancel_run")
	check(id>0,"取消实际进入独立ticket")
	if id>0:
		f.bridge.finish_run(id,"cancelled",count,permit,current); f.adapter.poll()

## 默认1.2秒从begin/上步完成起算，挂起中的操作不会积攒新许可。
func timing_and_step() -> void:
	var f: Dictionary=fixture(); var a=f.adapter; var b=f.bridge
	check(a.interval_seconds==1.2 and a._timer.one_shot,"原1.2秒one-shot设置")
	check(a.start("actor.player","action:1",8).ok,"begin接受")
	check(a._timer.is_stopped() and b.latest("grant")==0,"begin未完成时零推进且不启动Timer")
	b.finish_run(b.latest("begin_run"),"awaiting_permit",0,0,identity()); a.poll()
	check(not a._timer.is_stopped() and a._timer.wait_time==1.2,"begin完成才安排原1.2秒")
	await create_timer(.05).timeout
	check(b.latest("grant")==0,"未到间隔不能抢跑")
	await create_timer(1.25).timeout
	var grant: int=b.latest("grant")
	check(grant>0 and b.pending[grant].payload=={"run_id":a._run.run_id,"permit_seq":1,"grant_half_days":1},"真实Timer恰授一个半日")
	check(a._timer.is_stopped() and not a.request_step().ok and b.requests.size()==2,"运行中无新Timer或隐式积压")
	b.finish_run(grant,"awaiting_permit",1,1,identity(1,1)); a.poll()
	check(a.source_identity()==identity(1,1) and a._timer.time_left>1.0,"上一步完成后重新从1.2秒计时")
	check(a.request_step().ok,"下一明确许可可提交")
	grant=b.latest("grant"); check(b.pending[grant].source==identity(1,1) and b.pending[grant].payload.permit_seq==2,"新许可绑定最新已确认身份和单调序号")
	b.finish_run(grant,"arrived",2,2,identity(2,2)); a.poll()
	check(not a.auto_running and a._timer.is_stopped() and a._run.is_empty(),"抵达立即停止且解绑，不自动开启新run")
	check(f.errors.is_empty(),"正常自动流程无控制器错误")
	a.free()

## 未开始撤销与已在执行的最后一笔分别保留结果，阅读关闭不会恢复授权。
func reading_and_inflight() -> void:
	var f: Dictionary=fixture(); var a=f.adapter; var b=f.bridge; parked(f)
	b.revoke_unstarted=true; a.request_step(); var grant: int=b.latest("grant")
	var stop: Dictionary=a.suspend("reading"); var again: Dictionary=a.cancel()
	check(stop.reference==again.reference and b.requests.size()==3,"阅读与重复取消只占一个优先槽")
	check(not a.auto_running and a._timer.is_stopped() and b.gates.back().paused,"阅读立刻停止Timer并关闭未开始许可")
	a.poll(); check(f.events.back().result.code=="OperationRevoked" and f.errors.is_empty(),"被撤grant有独立终态且不冒充世界失败")
	cancelled(f,identity()); check(not a.is_busy() and b.completed.has(grant),"取消与grant回执均被消费并各自保留")
	var old_timer: Callable=a._timer_callback; check(a.start("actor.player","action:1",8).ok,"用户可明确重新开启新run")
	var begin: int=b.latest("begin_run"); b.finish_run(begin,"awaiting_permit",0,0,identity()); a.poll()
	var count: int=b.requests.size(); old_timer.call(); check(b.requests.size()==count,"旧Timer回调不能给新run授额度")
	b.revoke_unstarted=false; a.request_step(); grant=b.latest("grant"); a.suspend("focus_lost")
	b.finish_run(grant,"awaiting_permit",1,1,identity(1,1)); a.poll()
	check(a.source_identity()==identity(1,1) and not a.auto_running and a._timer.is_stopped(),"失焦时在途一笔可完成但无下一Timer")
	cancelled(f,identity(1,1),1,1); a._notification(Node.NOTIFICATION_APPLICATION_FOCUS_IN)
	check(not a.auto_running,"恢复焦点不补发半日")
	a.free()

## begin延迟完成、输入边界与重置期间回调都不能重新授权。
func cancellation_and_reset() -> void:
	var f: Dictionary=fixture(); var a=f.adapter; var b=f.bridge
	a.start("actor.player","action:1",8); a.cancel()
	b.finish_run(b.latest("begin_run"),"awaiting_permit",0,0,identity()); a.poll()
	check(not a.auto_running and a._timer.is_stopped(),"begin→cancel顺序下迟到begin不启动Timer")
	cancelled(f,identity()); parked(f); a.request_step()
	var prior_cancel: int=b.latest("cancel_run")
	b.finish_run(b.latest("grant"),"awaiting_input",1,1,identity(1,1)); a.poll()
	check(not a.auto_running and a._timer.is_stopped() and a._run.is_empty() and b.latest("cancel_run")==prior_cancel,"真实输入停止后续许可且不额外取消")
	check(a._progress.get("stop")=="awaiting_input","自然输入停点保留原原因")
	check(a.start("actor.player","action:1",8).ok,"输入结束只由显式start重新开启")
	b.finish_run(b.latest("begin_run"),"awaiting_permit",0,0,identity(1,1)); a.poll(); a.request_step()
	var grant: int=b.latest("grant"); var events: int=f.events.size(); a.reset()
	check(not a.bind_context(b,identity(0,0,2),1).ok,"reset保留未消费ticket，不偷偷换绑定")
	b.finish_run(grant,"awaiting_permit",1,1,identity(2,2)); a.poll()
	cancelled(f,identity(2,2),1,1)
	check(f.events.size()==events and a.source_identity().is_empty(),"旧绑定完成被消费但不发给新页面")
	check(a.bind_context(b,identity(0,0,2),1).ok,"边界清空后可绑定新epoch")
	check(not a.update_context(identity(2,2),1).ok,"旧epoch不能更新新世界上下文")
	a.free()

## 旧选择/旧scene报价必须丢弃，纯读取不能调用会提交的command。
func finite_reads() -> void:
	var f: Dictionary=fixture(); var a=f.adapter; var b=f.bridge
	check(not a.request_read("command",{}).ok and b.requests.is_empty(),"读取入口拒绝写命令")
	var payload: Dictionary={"actor_id":"actor.player","kind":"travel","params":{"destination":"village.east"}}
	check(a.request_read("preview",payload).ok,"纯preview经实际签收job")
	payload.params.destination="changed"; var id: int=b.latest("preview")
	check(b.pending[id].payload.params.destination=="village.east","请求为深拥有值")
	a.update_context(identity(),8); b.finish(id,{"ok":true,"duration":2},identity()); a.poll()
	check(f.events.is_empty(),"旧scene报价不送到新页面")
	a.request_read("preview",payload); id=b.latest("preview"); a.invalidate_selection()
	b.finish(id,{"ok":true,"duration":3},identity()); a.poll(); check(f.events.is_empty(),"旧选点报价丢弃")
	a.request_read("query",{"query_id":"action_context","args":{}}); id=b.latest("query")
	b.finish(id,{"ok":true,"action_id":"action:1"},identity()); a.poll()
	check(f.events.size()==1 and f.events[0].kind=="query","当前有限query仅回显示值")
	var copied: Dictionary=a.source_identity(); copied.revision=999; check(a.source_identity().revision==0,"宿主不能经身份DTO别名改版本")
	a.request_read("preview",payload); id=b.latest("preview"); a.update_context(identity(1,0),8)
	b.finish(id,{"ok":true,"duration":2},identity()); a.poll()
	check(f.events.size()==1 and f.errors.is_empty(),"版本前进后的旧报价静默丢弃，不伪报规则失败")
	a.free()

## 坏run回执不能启动Timer；取消失败保留引用以便明确重试，不自行反复排取消。
func malformed_and_cancel_failure() -> void:
	var f: Dictionary=fixture(); var a=f.adapter; var b=f.bridge
	a.start("actor.player","action:1",8)
	var begin: int=b.latest("begin_run")
	b.finish(begin,{"ok":true,"identity":identity(),"run_id":b.pending[begin].payload.run_id,"committed_half_days":1,"permit_seq":0,"stop":"awaiting_permit"},identity()); a.poll()
	check(not a.auto_running and a._timer.is_stopped() and f.errors.back().code=="ResidentAdapterInvalidProgress","begin谎报已推进被拒绝")
	var cancel_id: int=b.latest("cancel_run"); var request_count: int=b.requests.size()
	b.finish(cancel_id,{"ok":false,"code":"TestCancelFailure","kind":"internal"},identity()); a.poll(); a.poll()
	check(b.requests.size()==request_count and not a._run.is_empty() and not a.is_busy(),"取消失败不自动重试或遗失run引用")
	check(not a.start("actor.player","action:1",8).ok,"未确认取消前不能启动第二run")
	check(a.cancel().ok,"用户明确重试取消仍可使用原引用")
	cancelled(f,identity()); a.free()

## reset不会把取消错误当成功清空；旧页面结果仍丢弃，但宿主可安全重试撤销。
func reset_cancel_failure() -> void:
	var f: Dictionary=fixture(); var a=f.adapter; var b=f.bridge; parked(f)
	a.reset(); var id: int=b.latest("cancel_run")
	b.finish(id,{"ok":false,"code":"TestCancelFailure","kind":"internal"},identity()); a.poll()
	check(not a._run.is_empty() and a.source_identity().is_empty() and not a.is_busy(),"reset取消失败保留私有撤销引用，不暴露旧显示身份")
	check(not a.bind_context(b,identity(0,0,2)).ok,"未确认结束旧run不能换世界绑定")
	check(a.cancel().ok,"reset后显式取消重试仍使用原完整世界引用")
	cancelled(f,identity())
	check(a.bind_context(b,identity(0,0,2)).ok,"真实取消确认后新绑定才可成功")
	a.free()
	var rejected: Dictionary=fixture(); parked(rejected); rejected.bridge.reject_cancel_once=true; rejected.adapter.reset(); rejected.adapter.poll()
	check(not rejected.adapter._run.is_empty() and rejected.adapter.cancel().ok,"reset取消入队被拒后仍保留可重试引用")
	cancelled(rejected,identity()); rejected.adapter.free()

## 两次poll之间worker可完成两笔；先读到cancel也不能抹掉未读grant的独立验证上下文。
func independent_ticket_race() -> void:
	var f: Dictionary=fixture(); var a=f.adapter; var b=f.bridge; parked(f); a.request_step(); a.cancel()
	var grant: int=b.latest("grant"); var stop: int=b.latest("cancel_run")
	b.race_after_pending=func():
		b.finish_run(grant,"awaiting_permit",1,1,identity(1,1))
		b.finish_run(stop,"cancelled",1,1,identity(1,1))
	a.poll(); check(not a._job.is_empty() and a._cancel_job.is_empty(),"真实观察次序可先消费cancel再消费grant")
	a.poll(); check(not a.is_busy() and f.errors.is_empty() and not a.auto_running,"迟读grant仍合法且不复活已取消run")
	check(f.events[-2].kind=="cancel_run" and f.events[-1].kind=="grant","两张ticket各自回传真实终态")
	check(a.start("actor.player","action:2",8).ok,"消费两张ticket后可明确新启")
	b.finish_run(b.latest("begin_run"),"awaiting_permit",0,0,identity(1,1)); a.poll(); a.request_step(); a.cancel()
	b.finish_run(b.latest("grant"),"arrived",1,1,identity(2,2)); b.finish_run(b.latest("cancel_run"),"cancelled",1,1,identity(2,2)); a.poll()
	check(f.errors.is_empty() and not a.is_busy(),"grant先抵达也不损坏后续cancel回执验证")
	a.free()

## failed回调可明确换绑并新启；旧失败收尾不能随后撤销替代run。
func failure_rebind() -> void:
	var f: Dictionary=fixture(); var a=f.adapter; var b=f.bridge
	a.failed.connect(func(error):
		if error.code=="NotTravelling":
			check(a.bind_context(b,identity(0,0,2),9).ok,"失败处理可绑定替代世界")
			check(a.start("actor.player","action:new",8).ok,"失败处理可明确开启替代run"))
	a.start("actor.player","action:old",8); var first: int=b.latest("begin_run")
	b.finish(first,{"ok":false,"code":"NotTravelling","kind":"rule"},identity()); a.poll()
	check(a.auto_running and b.latest("begin_run")!=first and b.latest("cancel_run")==0,"旧失败不会取消回调中新建的run")
	b.finish_run(b.latest("begin_run"),"awaiting_permit",0,0,identity(0,0,2)); a.poll(); a.cancel()
	cancelled(f,identity(0,0,2)); a.free()

## reset的stopped同步回调可明确建立替代绑定；reset返回后不能把新run再次标为旧世界。
func reset_signal_rebind() -> void:
	var f: Dictionary=fixture(); var a=f.adapter; var b=f.bridge
	a.stopped.connect(func(reason):
		if reason=="reset":
			check(a.bind_context(b,identity(0,0,2),9).ok,"reset通知中可绑定替代世界")
			check(a.start("actor.player","action:new",8).ok,"reset通知中可明确开启替代run"))
	a.reset()
	check(a.source_identity()==identity(0,0,2) and a.auto_running,"reset先发布退休状态，不在回调后抹掉新绑定")
	b.finish_run(b.latest("begin_run"),"awaiting_permit",0,0,identity(0,0,2)); a.poll()
	check(a._bound and not a._timer.is_stopped() and f.errors.is_empty(),"替代begin真实完成后仍属于有效绑定")
	a.cancel(); cancelled(f,identity(0,0,2)); a.free()

## 同源完整画面未就绪时只暂缓Timer；新token或取消后旧release不能碰当前授权。
func refresh_holds() -> void:
	var f: Dictionary=fixture(); var a=f.adapter; var b=f.bridge; parked(f)
	var first: Dictionary=a.hold_for_refresh(identity())
	check(first.ok and a.auto_running and a._timer.is_stopped(),"刷新hold只停Timer保留本人授权")
	check(not a.request_step().ok and b.latest("grant")==0,"收页期间不能发送半日许可")
	var wrong: Dictionary=first.token.duplicate(true); wrong.source.revision=99
	check(not a.release_refresh(wrong).ok and a._timer.is_stopped(),"非同源release不能启动Timer")
	var second: Dictionary=a.hold_for_refresh(identity())
	check(not a.release_refresh(first.token).ok,"更早一轮frame token失效")
	check(a.release_refresh(second.token).scheduled and a._timer.wait_time==1.2 and not a._timer.is_stopped(),"完整匹配frame发布后重计1.2秒")
	var reading: Dictionary=a.hold_for_refresh(identity()); a.suspend("reading")
	check(not a.release_refresh(reading.token).ok and not a.auto_running and a._timer.is_stopped(),"阅读取消后迟到frame不能复活许可")
	cancelled(f,identity()); parked(f)
	var obsolete: Dictionary=a.hold_for_refresh(identity()); a.update_context(identity(1,0),8)
	check(not a.release_refresh(obsolete.token).ok and not a.auto_running,"外部已提交版本使旧刷新token失效")
	cancelled(f,identity(1,0)); a.free()

## 真实Bridge消费模式只执行冻结post-travel来源的一半日，不重跑协议组或整趟压力链。
func real_bridge(oracle_root: String) -> void:
	check(ClassDB.class_exists("HanmoResidentBridge"),"真实扩展类已注册")
	if failures: return
	var index: Dictionary=JSON.parse_string(FileAccess.get_file_as_string(oracle_root.path_join("oracle-index.json")))
	check(index.commands[0].command.kind=="travel" and index.commands[1].command.kind=="advance","限定真实启程后的第一半日")
	if failures: return
	var seed_path: String=oracle_root.path_join(index.commands[0].after.file)
	var seed: Dictionary=JSON.parse_string(FileAccess.get_file_as_string(seed_path))
	var bridge: Object=ClassDB.instantiate("HanmoResidentBridge")
	var installed: Dictionary=bridge.install(FileAccess.get_file_as_bytes(oracle_root.path_join("expanded-catalog.json")),FileAccess.get_file_as_bytes(seed_path))
	check(installed.get("ok",false) and installed.get("identity") is Dictionary,"实际core安装post-travel原状态字节")
	if failures: return
	var actor_id: String=seed.player_actor_id
	var action_id: String=seed.actors[actor_id].action_id
	var a=Adapter.new(); root.add_child(a); a.set_process(false)
	var events: Array=[]; var errors: Array=[]
	a.operation_completed.connect(func(kind,result,source,current): events.append({"kind":kind,"result":result,"source":source,"current":current}))
	a.failed.connect(func(error): errors.append(error))
	check(a.bind_context(bridge,installed.identity,1).ok,"控制器绑定实际安装身份")
	check(a.request_read("query",{"query_id":"action_context","args":{}}).ok,"有限真实行动query入队")
	check(await wait_real_adapter(a),"有限真实query完成")
	check(not events.is_empty() and events.back().result.action_id==action_id and events.back().result.action_kind=="travel","有限query消费实际行动")
	check(real_digest(bridge)==index.commands[0].after.sha256,"query不改变完整来源状态")
	if failures: a.free(); return
	check(a.start(actor_id,action_id,1).ok and await wait_real_adapter(a),"实际begin只停驻")
	var old_timer: Callable=a._timer_callback
	check(a._timer.wait_time==1.2 and a.source_identity().tick==installed.identity.tick,"实际begin完成才启动原1.2秒Timer")
	check(a.suspend("reading").ok and await wait_real_adapter(a),"实际阅读闸门与取消完成")
	check(not a.auto_running and real_digest(bridge)==index.commands[0].after.sha256,"阅读取消保全真实世界字节")
	check(a.start(actor_id,action_id,1).ok and await wait_real_adapter(a),"同一实际core可明确新启另一run")
	old_timer.call()
	await create_timer(.05).timeout
	a.poll()
	check(not a.is_busy() and a.source_identity().tick==installed.identity.tick,"旧Timer不会替新run提前发许可")
	check(await wait_real_adapter(a,true),"原Timer触发的唯一半日完成并停止")
	check(a.source_identity().tick==installed.identity.tick+1 and a.source_identity().revision==installed.identity.revision+1,"真实核心只提交一半日")
	check(real_digest(bridge)==index.commands[1].after.sha256,"整个真实世界值/类型/顺序与冻结第一半日SHA精确一致")
	check(errors.is_empty(),"实际Bridge/core回执被控制器正确消费")
	check(not a.request_step().ok and not a.auto_running and a._timer.is_stopped(),"预算或输入边界停止后不能暗发下一半日")
	a.free()

## 仅等待控制器已有ticket和显式Timer，不用debug导出探测worker是否完成。
func wait_real_adapter(adapter: Node, until_stopped: bool=false) -> bool:
	var deadline: int=Time.get_ticks_usec()+15000000
	while Time.get_ticks_usec()<deadline:
		adapter.poll()
		if not adapter.is_busy() and (not until_stopped or not adapter.auto_running): return true
		await process_frame
	return false

## 全导出仅在已静止的测试oracle边界读取，不进入controller或正常有限查询。
func real_digest(bridge: Object) -> String:
	var value: Dictionary=bridge.debug_export_state()
	return str(value.get("json","")).sha256_text() if value.get("ok",false) else "export_failed"

## 核心失败与同步信号内换世界不允许恢复Timer或错误复用旧run。
func failures_and_reentrancy() -> void:
	var f: Dictionary=fixture(); var a=f.adapter; var b=f.bridge
	a.start("actor.player","action:1",8)
	b.finish(b.latest("begin_run"),{"ok":false,"code":"NotTravelling","kind":"rule"},identity()); a.poll()
	check(not a.auto_running and a._run.is_empty() and b.latest("cancel_run")==0,"begin失败解除UI绑定，不制造不存在run的取消")
	check(f.errors.size()==1 and f.events.back().result.code=="NotTravelling","真实失败保留原code并作为完成回传")
	parked(f); a.request_step()
	a.operation_completed.connect(func(kind,_result,_source,_current):
		if kind=="grant": a.reset())
	b.finish_run(b.latest("grant"),"awaiting_permit",1,1,identity(1,1)); a.poll()
	check(not a.auto_running and a._timer.is_stopped() and a.source_identity().is_empty(),"完成信号内reset不被之后代码重新授权")
	cancelled(f,identity(1,1),1,1)
	check(f.errors.size()==1,"重入reset未制造InvalidProgress或多余失败")
	a.free()

## 原生自然停点已经detach，真实适配器必须保留原reason，不能再排取消或旧Timer许可。
func natural_stops_keep_reasons() -> void:
	for kind in ["begin_run","grant"]:
		for reason in ["awaiting_input","paused","stranded","blocked"]:
			var f: Dictionary=fixture(); var a=f.adapter; var b=f.bridge; var stops: Array=[]
			a.stopped.connect(func(value: String): stops.append(value))
			if kind=="grant":
				parked(f); check(a.request_step().ok,"自然停止前存在真实单许可 "+reason)
			else: check(a.start("actor.player","action:1",8).ok,"begin已被正式签收 "+reason)
			var old_timer: Callable=a._timer_callback
			var count: int=b.requests.size()
			b.finish_run(b.latest(kind),reason,1 if kind=="grant" else 0,1 if kind=="grant" else 0,identity(1,1) if kind=="grant" else identity())
			a.poll()
			check(not a.auto_running and a._run.is_empty() and not a.is_busy() and a._timer.is_stopped(),"自然终态稳定清run "+kind+"/"+reason)
			check(a._progress.get("stop")==reason and stops==[reason] and f.events.back().result.get("stop")==reason,"原停点回执和信号保留 "+kind+"/"+reason)
			check(b.latest("cancel_run")==0 and b.requests.size()==count,"无第二cancel覆盖原因 "+kind+"/"+reason)
			check(not a.request_step().ok,"自然终态拒绝新grant "+kind+"/"+reason)
			if old_timer.is_valid(): old_timer.call()
			await create_timer(.15).timeout
			check(b.requests.size()==count and a._progress.get("stop")==reason and f.errors.is_empty(),"迟到Timer和等待均不增加许可 "+kind+"/"+reason)
			a.free()

## 只运行控制器及其实际Timer；实际Main、物理槽和世界规则由整合验收另证。
func _run() -> void:
	var args: PackedStringArray=OS.get_cmdline_user_args()
	if args==PackedStringArray(["--natural-stops-only"]):
		await natural_stops_keep_reasons()
		print("RESIDENT_NATURAL_STOPS_CHECKS=",checks," FAILURES=",failures," PROTOCOL_ONLY=true")
		quit(0 if failures==0 else 1); return
	if args.size()==2 and args[0]=="--real-bridge":
		await real_bridge(args[1])
		print("RESIDENT_JOURNEY_ADAPTER_REAL_CHECKS=",checks," FAILURES=",failures," MAX_HALF_DAYS=1 MAIN_ACCEPTANCE=false")
		quit(0 if failures==0 else 1); return
	await timing_and_step()
	reading_and_inflight(); cancellation_and_reset(); finite_reads(); failures_and_reentrancy(); malformed_and_cancel_failure(); reset_cancel_failure(); independent_ticket_race(); failure_rebind(); reset_signal_rebind(); refresh_holds()
	print("RESIDENT_JOURNEY_ADAPTER_CHECKS=",checks," FAILURES=",failures," PROTOCOL_ONLY=true")
	quit(0 if failures==0 else 1)
