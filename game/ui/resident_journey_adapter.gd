## 常驻行旅的界面许可控制器；只持深拥有DTO和引用，不持世界、不计算路线、不直接推进规则。
extends Node
signal operation_completed(kind: String, result: Dictionary, source: Dictionary, current: Dictionary)
signal progress_changed(progress: Dictionary)
signal stopped(reason: String)
signal failed(error: Dictionary)

var interval_seconds: float=1.2
var auto_running: bool=false
var _bridge: Object
var _source: Dictionary={}
var _bound: bool=false
var _scene_epoch: int=0
var _selection_epoch: int=0
var _binding_epoch: int=0
var _run_counter: int=0
var _authorization_epoch: int=0
var _refresh_sequence: int=0
var _refresh_hold: Dictionary={}
var _run: Dictionary={}
var _run_plan_id: String=""
var _progress: Dictionary={}
var _job: Dictionary={}
var _cancel_job: Dictionary={}
var _timer: Timer
var _timer_callback: Callable
const STOPS=["awaiting_permit","awaiting_input","arrived","paused","stranded","blocked","cancelled","budget_exhausted","failed","money_budget_exhausted","action_replaced","restored","completed"]

## 沿用原单次1.2秒节奏；每帧仅轮询有限回执，不等待或join worker。
func _ready() -> void:
	_timer=Timer.new(); _timer.one_shot=true
	add_child(_timer)

## 成功换世界后由宿主绑定新身份；旧已接收ticket先消费，不能丢掉未读completion堵住worker。
func bind_context(bridge: Object, source: Dictionary, scene_epoch: int=0) -> Dictionary:
	if is_busy() or not _run.is_empty(): return _error("ResidentAdapterBusy")
	if not is_instance_valid(bridge) or not bridge.has_method("enqueue_live_operation") or not bridge.has_method("poll_live_operation") or not bridge.has_method("set_reading_paused"): return _error("ResidentAdapterInvalidBridge")
	if not _valid_source(source): return _error("ResidentAdapterInvalidSource")
	_binding_epoch+=1; _selection_epoch+=1
	_bridge=bridge; _source=source.duplicate(true); _bound=true; _scene_epoch=scene_epoch; _progress={}; _refresh_hold={}; _run_plan_id=""
	return {"ok":true}

## 同世界的宿主更新只接受不倒退版本；重绘代次仅使旧读页失效，不偷偷恢复自动授权。
func update_context(source: Dictionary, scene_epoch: int) -> Dictionary:
	if not _bound: return _error("ResidentAdapterNotBound")
	if not _valid_source(source) or not _same_world(source,_source): return _error("StaleWorld")
	if source.revision<_source.revision or source.tick<_source.tick: return _error("StalePreview")
	if source!=_source and auto_running: suspend("source_changed")
	_source=source.duplicate(true); _scene_epoch=scene_epoch
	return {"ok":true}

## 选择变化撤销许可和旧报价；屏幕坐标、选择值仍由原JourneyView持有。
func invalidate_selection() -> void:
	_selection_epoch+=1
	suspend("selection_changed")

## 仅转发纯preview/query；绝不把会提交的command当作读取，也不在自动许可间暗排读取队列。
func request_read(kind: String, payload: Dictionary) -> Dictionary:
	if kind not in ["preview","query"]: return _error("ResidentAdapterInvalidRead")
	if auto_running or not _run.is_empty(): return _error("ResidentAdapterRunning")
	return _enqueue(kind,payload)

## 宿主已完成实际resume/advance预览后才调用；begin只停驻，收到awaiting_permit后才开始计时。
func start(actor_id: String, action_id: String, max_half_days: int, max_gross_outflow: int=0, resume_saved_scope: bool=false) -> Dictionary:
	return _start_run(actor_id,action_id,"",max_half_days,max_gross_outflow,resume_saved_scope)

## 生活安排仍用原begin和逐份permit；不会把计划ID冒充已接受行动或恢复旧行动许可。
func start_plan(actor_id: String, plan_id: String, max_half_days: int, max_gross_outflow: int) -> Dictionary:
	return _start_run(actor_id,"",plan_id,max_half_days,max_gross_outflow,false)

## 两类有限驱动共用同一传输和Timer；恰有一种目标，计划从不携带旧resume范围。
func _start_run(actor_id: String, action_id: String, plan_id: String, max_half_days: int, max_gross_outflow: int, resume_saved_scope: bool) -> Dictionary:
	if not is_instance_valid(_timer): return _error("ResidentAdapterNotReady")
	if not _refresh_hold.is_empty(): return _error("ResidentAdapterRefreshing")
	if is_busy() or not _run.is_empty(): return _error("ResidentAdapterBusy")
	if actor_id.is_empty() or action_id.is_empty()==plan_id.is_empty() or max_half_days<1 or max_gross_outflow<0 or (not plan_id.is_empty() and resume_saved_scope): return _error("InvalidRunRequest")
	_run_counter+=1
	var run_id: String="ui:%d:%d"%[get_instance_id(),_run_counter]
	var payload: Dictionary={"run_id":run_id,"actor_id":actor_id,"action_id":action_id,"max_half_days":max_half_days,"max_gross_outflow":max_gross_outflow,"resume_saved_scope":resume_saved_scope}
	if not plan_id.is_empty(): payload.plan_id=plan_id
	var accepted: Dictionary=_enqueue("begin_run",payload)
	if not accepted.ok: return accepted
	_run={"world_id":_source.world_id,"execution_epoch":_source.execution_epoch,"run_id":run_id}
	_run_plan_id=plan_id
	_progress={}; _authorization_epoch+=1; auto_running=true
	return accepted

## 每次Timer只发一个新许可；正在等待时不累计额度，不自行重试新序号。
func request_step() -> Dictionary:
	if not auto_running: return _error("ResidentAdapterNotAuthorized")
	if not _refresh_hold.is_empty(): return _error("ResidentAdapterRefreshing")
	if is_busy(): return _error("ResidentAdapterBusy")
	if _run.is_empty() or _progress.get("stop")!="awaiting_permit": return _error("ResidentAdapterRunStopped")
	_timer.stop()
	var accepted: Dictionary=_enqueue("grant",{"run_id":_run.run_id,"permit_seq":_progress.permit_seq+1,"grant_half_days":1})
	if not accepted.ok:
		suspend("grant_rejected"); failed.emit(accepted.duplicate(true))
	return accepted

## 阅读/失焦先关闭本地授权与未开始许可，再用唯一优先取消槽；已开始事务保持原子性。
func suspend(reason: String="reading") -> Dictionary:
	auto_running=false; _authorization_epoch+=1; _refresh_hold={}
	if is_instance_valid(_timer): _timer.stop()
	if _run.is_empty():
		stopped.emit(reason); return {"ok":true}
	if not _cancel_job.is_empty():
		var pending: Dictionary={"ok":true,"reference":_cancel_job.reference.duplicate(true)}
		stopped.emit(reason); return pending
	if not is_instance_valid(_bridge): return _error("ResidentAdapterInvalidBridge")
	var gate: Dictionary=_bridge.set_reading_paused(_run.duplicate(true),true)
	var accepted: Dictionary=_enqueue("cancel_run",{"run_id":_run.run_id},true)
	if not gate.get("ok",false): failed.emit(gate.duplicate(true))
	if not accepted.ok: failed.emit(accepted.duplicate(true))
	stopped.emit(reason)
	return accepted

## 显式取消不等于pause_travel；宿主的真实停步命令仍经同一个core提交。
func cancel() -> Dictionary:
	return suspend("cancelled")

## 换场景/世界清除展示授权，但保留已接受ticket直到消费；过期完成不再发给旧页面。
func reset() -> void:
	_binding_epoch+=1; _selection_epoch+=1; _bound=false
	suspend("reset")

## 返回隔离身份用于后续有限请求；调用者不能经Dictionary别名改变控制器当前源。
func source_identity() -> Dictionary:
	return _source.duplicate(true) if _bound else {}

## 仅暴露最近原生回执中的许可摘要；清除run引用后仍保留已用预算供明确继续。
func continuation_state() -> Dictionary:
	if not _bound or _progress.is_empty() or not _progress.has("actor_id") or not _progress.get("plan_id","").is_empty(): return {}
	var value: Dictionary=_progress.duplicate(true)
	value.source_identity=value.get("identity",_source).duplicate(true)
	return value

## 同源完整画面收集中只停待发Timer，保留本人授权；不取消run，也不向worker授予或积攒许可。
func hold_for_refresh(source: Dictionary) -> Dictionary:
	if not _bound or source!=_source: return _error("StalePreview")
	if is_busy(): return _error("ResidentAdapterBusy")
	_refresh_sequence+=1
	_refresh_hold={"controller_id":get_instance_id(),"binding_epoch":_binding_epoch,"authorization_epoch":_authorization_epoch,"refresh_sequence":_refresh_sequence,"source":_source.duplicate(true)}
	if is_instance_valid(_timer): _timer.stop()
	return {"ok":true,"token":_refresh_hold.duplicate(true)}

## 宿主发布匹配的完整frame后才释放；旧页、阅读、失焦或新run的旧token不能恢复自动授权。
func release_refresh(token: Dictionary) -> Dictionary:
	if _refresh_hold.is_empty() or token!=_refresh_hold or not _bound or token.source!=_source or token.binding_epoch!=_binding_epoch or token.authorization_epoch!=_authorization_epoch: return _error("StaleRefreshReference")
	_refresh_hold={}
	var schedule: bool=auto_running and not is_busy() and not _run.is_empty() and _progress.get("stop")=="awaiting_permit"
	if schedule: _schedule_next()
	return {"ok":true,"scheduled":schedule}

## 两个槽分别保留普通操作与优先取消；不能因为取消覆盖正在完成的grant。
func is_busy() -> bool:
	return not _job.is_empty() or not _cancel_job.is_empty()

## 一帧最多读取两个已接受ticket；poll仅取有限DTO，不隐含命令、导出或许可。
func poll() -> void:
	if not is_instance_valid(_bridge): return
	_poll_slot(false)
	_poll_slot(true)
	if not _bound and not is_busy() and _run.is_empty(): _source={}; _bridge=null

## 只在同世界/同场景/同选择的读结果上发信号；自动结果仍接受已完成的一笔并撤销后续许可。
func _poll_slot(control: bool) -> void:
	var job: Dictionary=_cancel_job if control else _job
	if job.is_empty(): return
	var reply: Dictionary=_bridge.poll_live_operation(job.reference.duplicate(true))
	if reply.get("ok",false) and not reply.get("ready",false): return
	if control: _cancel_job={}
	else: _job={}
	if not reply.get("ok",false):
		if job.binding_epoch==_binding_epoch:
			if control: failed.emit(reply.duplicate(true))
			else: _stop_failure(reply)
		elif control and not _run.is_empty(): failed.emit(reply.duplicate(true))
		return
	if job.binding_epoch!=_binding_epoch or not _bound:
		if control: _finish_retired_cancel(reply,job)
		elif job.kind=="begin_run" and reply.get("result") is Dictionary and not reply.result.get("ok",false): _run={}; _run_plan_id=""
		return
	if job.kind in ["preview","query"] and (job.scene_epoch!=_scene_epoch or job.selection_epoch!=_selection_epoch or job.source!=_source): return
	if not _valid_completion(reply,job):
		if control: failed.emit(_error("ResidentAdapterInvalidCompletion"))
		else: _stop_failure(_error("ResidentAdapterInvalidCompletion"))
		return
	if job.kind in ["preview","query"]:
		operation_completed.emit(job.kind,reply.result.duplicate(true),reply.source_identity.duplicate(true),reply.current_identity.duplicate(true))
		return
	_source=reply.current_identity.duplicate(true)
	if control:
		if reply.result.get("ok",false) and not _valid_cancel(reply.result,job):
			failed.emit(_error("ResidentAdapterInvalidProgress")); return
		if reply.result.get("ok",false): _run={}; _run_plan_id=""; _progress=reply.result.duplicate(true)
		operation_completed.emit(job.kind,reply.result.duplicate(true),reply.source_identity.duplicate(true),_source.duplicate(true))
		if job.binding_epoch==_binding_epoch and not reply.result.get("ok",false): failed.emit(reply.result.duplicate(true))
		return
	var result: Dictionary=reply.result
	if not result.get("ok",false):
		if job.kind=="begin_run": _run={}; _run_plan_id=""
		if result.get("code")=="OperationRevoked" and result.get("stop")=="revoked_before_start":
			suspend("revoked_before_start")
		else: _stop_failure(result)
		if job.binding_epoch==_binding_epoch: operation_completed.emit(job.kind,result.duplicate(true),reply.source_identity.duplicate(true),reply.current_identity.duplicate(true))
		return
	if not _valid_progress(result,job):
		_stop_failure(_error("ResidentAdapterInvalidProgress")); return
	var active: bool=_run.get("run_id")==job.payload.run_id
	if active:
		_progress=result.duplicate(true)
		if result.stop=="awaiting_permit":
			if auto_running and _cancel_job.is_empty() and _refresh_hold.is_empty(): _schedule_next()
		else:
			auto_running=false; _timer.stop()
			# 原生非等待许可回执已经detach；保留原停点，不再用cancel覆盖原因。
			_run={}
			_run_plan_id=""
	if job.binding_epoch!=_binding_epoch: return
	if active: progress_changed.emit(result.duplicate(true))
	if job.binding_epoch!=_binding_epoch: return
	operation_completed.emit(job.kind,result.duplicate(true),reply.source_identity.duplicate(true),reply.current_identity.duplicate(true))
	if job.binding_epoch==_binding_epoch and result.stop!="awaiting_permit": stopped.emit(result.stop)

## Timer回调绑定当时run与已确认许可；同一Node复用后迟到信号不能替新旅程授予半日。
func _schedule_next() -> void:
	if _timer_callback.is_valid() and _timer.timeout.is_connected(_timer_callback): _timer.timeout.disconnect(_timer_callback)
	_timer_callback=_timer_fired.bind(_binding_epoch,_run.run_id,_progress.permit_seq)
	_timer.timeout.connect(_timer_callback,CONNECT_ONE_SHOT)
	_timer.start(maxf(.1,interval_seconds))

## 仅当前停驻run的当前Timer可进入公开单步入口，旧回调无副作用。
func _timer_fired(binding_epoch: int, run_id: String, permit_seq: int) -> void:
	if binding_epoch!=_binding_epoch or _run.get("run_id")!=run_id or _progress.get("permit_seq")!=permit_seq: return
	request_step()

## 限定两类ticket的形状与源身份；引用由真实Bridge分配，不在脚本中虚构operation_id。
func _enqueue(kind: String, payload: Dictionary, control: bool=false) -> Dictionary:
	if not is_instance_valid(_bridge) or not _valid_source(_source) or (not control and not _bound): return _error("ResidentAdapterNotBound")
	if (control and not _cancel_job.is_empty()) or (not control and is_busy()): return _error("ResidentAdapterBusy")
	var source: Dictionary=_source.duplicate(true)
	var result: Dictionary=_bridge.enqueue_live_operation({"kind":kind,"source":source.duplicate(true),"payload":payload.duplicate(true)})
	if not result.get("ok",false): return result
	var reference: Variant=result.get("reference")
	if not reference is Dictionary or not _same_world(reference,source) or typeof(reference.get("operation_id"))!=TYPE_INT or reference.operation_id<1: return _error("ResidentAdapterInvalidReference")
	var job: Dictionary={"kind":kind,"source":source,"payload":payload.duplicate(true),"prior_progress":_progress.duplicate(true),"reference":reference.duplicate(true),"binding_epoch":_binding_epoch,"scene_epoch":_scene_epoch,"selection_epoch":_selection_epoch}
	job.plan_id=payload.get("plan_id","") if kind=="begin_run" else _run_plan_id
	if control: _cancel_job=job
	else: _job=job
	return result.duplicate(true)

## 回执必须来自原请求且仍属当前世界；值错误不能偷偷驱动下一次Timer。
func _valid_completion(reply: Dictionary, job: Dictionary) -> bool:
	return reply.get("tag")==job.kind and reply.get("source_identity")==job.source and reply.get("result") is Dictionary and reply.get("current_identity") is Dictionary and _valid_source(reply.current_identity) and _same_world(reply.current_identity,_source) and reply.current_identity.revision>=_source.revision and reply.current_identity.tick>=_source.tick

## 只接受冻结run状态枚举与真整数序号，begin不会伪报推进，grant最多新增一笔。
func _valid_progress(result: Dictionary, job: Dictionary) -> bool:
	if result.get("identity")!=_source or result.get("run_id")!=job.payload.run_id or result.get("stop") not in STOPS: return false
	if result.get("plan_id","")!=job.get("plan_id",""): return false
	if typeof(result.get("permit_seq"))!=TYPE_INT or typeof(result.get("committed_half_days"))!=TYPE_INT or result.permit_seq<0 or result.committed_half_days<0: return false
	if result.has("gross_outflow"):
		for field in ["gross_outflow","remaining_half_days","remaining_gross_outflow","max_half_days","max_gross_outflow"]:
			if typeof(result.get(field))!=TYPE_INT or result[field]<0: return false
		if result.committed_half_days+result.remaining_half_days!=result.max_half_days or result.gross_outflow+result.remaining_gross_outflow!=result.max_gross_outflow: return false
	if job.kind=="begin_run": return result.permit_seq==0 and _source==job.source and (job.payload.get("resume_saved_scope",false) or result.committed_half_days==0)
	if not job.get("plan_id","").is_empty():
		var delta: int=result.committed_half_days-job.prior_progress.get("committed_half_days",0)
		if delta<0 or delta>1 or _source.tick-job.source.tick!=delta: return false
	return result.permit_seq==job.payload.permit_seq and result.committed_half_days>=job.prior_progress.get("committed_half_days",0) and result.committed_half_days<=job.prior_progress.get("committed_half_days",0)+1

## 取消回执仍须属于实际run；普通传输ok不能冒充已到提交边界。
func _valid_cancel(result: Dictionary, job: Dictionary) -> bool:
	return result.get("identity")==_source and result.get("run_id")==job.payload.run_id and result.get("plan_id","")==job.get("plan_id","") and result.get("stop")=="cancelled" and typeof(result.get("permit_seq"))==TYPE_INT and result.permit_seq>=job.prior_progress.get("permit_seq",0) and typeof(result.get("committed_half_days"))==TYPE_INT and result.committed_half_days>=job.prior_progress.get("committed_half_days",0)

## reset丢弃旧显示结果但仍完成取消握手；失败保留原run/source，允许显式重试且阻止新绑定。
func _finish_retired_cancel(reply: Dictionary, job: Dictionary) -> void:
	if not _valid_completion(reply,job):
		failed.emit(_error("ResidentAdapterInvalidCompletion")); return
	_source=reply.current_identity.duplicate(true)
	if not reply.result.get("ok",false):
		if not _run.is_empty(): failed.emit(reply.result.duplicate(true))
		return
	if not _valid_cancel(reply.result,job):
		failed.emit(_error("ResidentAdapterInvalidProgress")); return
	_run={}
	_run_plan_id=""

## 错误只撤销界面后续授权并尝试有界取消，不回滚已完成世界或伪造领域成功。
func _stop_failure(error: Dictionary) -> void:
	var binding: int=_binding_epoch
	auto_running=false
	if is_instance_valid(_timer): _timer.stop()
	if not _run.is_empty() and _cancel_job.is_empty(): suspend("failed")
	if binding==_binding_epoch: failed.emit(error.duplicate(true))

## 身份值必须保int64；浮点序号或不完整world引用不能成为新许可来源。
func _valid_source(value: Dictionary) -> bool:
	return value.get("world_id") is String and not value.world_id.is_empty() and typeof(value.get("execution_epoch"))==TYPE_INT and value.execution_epoch>0 and typeof(value.get("revision"))==TYPE_INT and value.revision>=0 and typeof(value.get("tick"))==TYPE_INT and value.tick>=0

## 运行和操作引用只在world/epoch一致时可见，revision由各操作原合同单独处理。
func _same_world(a: Dictionary, b: Dictionary) -> bool:
	return a.get("world_id")==b.get("world_id") and a.get("execution_epoch")==b.get("execution_epoch")

## 本地控制错误保持独立代码，不伪造任何游戏规则拒绝。
func _error(code: String) -> Dictionary:
	return {"ok":false,"code":code}

## UI轮询不改变现实等待与游戏半日的对应关系。
func _process(_delta: float) -> void:
	poll()

## 失焦只撤销现有授权；焦点恢复没有自动start或grant。
func _notification(what: int) -> void:
	if what==NOTIFICATION_WM_WINDOW_FOCUS_OUT or what==NOTIFICATION_APPLICATION_FOCUS_OUT: suspend("focus_lost")

## 宿主应在移除前消费取消回执；离树仍先撤销未开始许可，但不join世界worker。
func _exit_tree() -> void:
	suspend("exit_tree")
