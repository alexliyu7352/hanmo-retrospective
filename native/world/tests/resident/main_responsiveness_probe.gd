## 仅测试附着的响应性探针；转发同一真实Bridge，不创建世界、worker、许可或规则路径。
extends Node
const LIMIT: int=8192
var _native: Object
var _backend: Node
var _adapter: Node
var _presenter: Node
var _phase: String=""
var _phase_started: int=0
var _previous_frame: int=0
var _started: int=0
var _calls: Array=[]
var _frames: Array=[]
var _publications: Array=[]
var _phases: Array=[]
var _operations: Dictionary={}
var _dropped: int=0

## 只能在原Main空闲边界替换三个消费者持有的引用；实际native对象与已安装世界不变。
func attach(backend: Node, adapter: Node) -> Dictionary:
	if is_instance_valid(_native): return {"ok":false,"code":"ProbeAlreadyAttached"}
	if not is_instance_valid(backend) or not is_instance_valid(adapter) or backend.is_busy() or adapter.is_busy(): return {"ok":false,"code":"ProbeRequiresIdle"}
	var actual: Object=backend.transport()
	if not is_instance_valid(actual) or adapter._bridge!=actual: return {"ok":false,"code":"ProbeTransportMismatch"}
	_native=actual; _backend=backend; _adapter=adapter; _presenter=backend._presenter
	_backend._bridge=self; _adapter._bridge=self
	if is_instance_valid(_presenter): _presenter._bridge=self
	_backend.frame_ready.connect(_frame_ready)
	_started=Time.get_ticks_usec(); process_priority=-1000
	return {"ok":true,"native_instance_id":_native.get_instance_id()}

## 每段只覆盖真实UI等待；原GD同步oracle、完整导出和文件写入须放在段外。
func begin_phase(label: String) -> void:
	end_phase(); _phase=label; _phase_started=Time.get_ticks_usec(); _previous_frame=0

## 结束段会丢弃跨段的帧间隙，不把oracle导出耗时算作绘制阻塞。
func end_phase() -> void:
	if not _phase.is_empty(): _append(_phases,{"phase":_phase,"wall_usec":Time.get_ticks_usec()-_phase_started})
	_phase=""; _previous_frame=0

## 额外包裹原Main同步按钮回调；Callable只调用一次，返回值不复制或改写。
func measure_call(label: String, action: Callable) -> Variant:
	var before: int=Time.get_ticks_usec()
	var value: Variant=action.call()
	_call(label,before,Time.get_ticks_usec(),true)
	return value

## 每帧只记本段同一采样点之间的墙钟间隔；它不是GPU呈现耗时或worker耗时。
func _process(_delta: float) -> void:
	if _phase.is_empty(): return
	var now: int=Time.get_ticks_usec()
	if _previous_frame>0: _append(_frames,{"phase":_phase,"gap_usec":now-_previous_frame,"pending_operations":_pending_count()})
	_previous_frame=now

## 原transport接纳前后仅读单调时钟；小请求与回执原样通过，不重发失败请求。
func enqueue_live_operation(operation: Dictionary) -> Dictionary:
	var before: int=Time.get_ticks_usec()
	var reply: Dictionary=_native.enqueue_live_operation(operation)
	var after: int=Time.get_ticks_usec()
	_call("enqueue:"+str(operation.get("kind","")),before,after,reply.get("ok",false))
	if not _phase.is_empty() and reply.get("ok",false) and reply.get("reference") is Dictionary:
		var key: String=_key(reply.reference)
		if not _operations.has(key):
			if _operations.size()>=LIMIT: _dropped+=1
			else: _operations[key]={"phase":_phase,"kind":operation.get("kind",""),"query_id":operation.get("payload",{}).get("query_id",""),"source":operation.get("source",{}).duplicate(true),"reference":reply.reference.duplicate(true),"accepted_usec":after-_started,"accepted_at":after,"enqueue_usec":after-before,"observed":false}
	return reply

## 只观察消费者本来就进行的首次完成poll；探针不会自行poll、join或导出世界。
func poll_live_operation(reference: Dictionary) -> Dictionary:
	var before: int=Time.get_ticks_usec()
	var reply: Dictionary=_native.poll_live_operation(reference)
	var after: int=Time.get_ticks_usec()
	_call("poll:"+("ready" if reply.get("ready",false) else "pending"),before,after,reply.get("ok",false))
	var key: String=_key(reference)
	if _operations.has(key) and not _operations[key].observed and (reply.get("ready",false) or not reply.get("ok",false)):
		var row: Dictionary=_operations[key]
		row.observed=true; row.observed_usec=after-_started; row.accepted_to_observed_usec=after-int(row.accepted_at); row.erase("accepted_at")
		row.completion_phase=_phase; row.crossed_phase=row.phase!=_phase; row.transport_ok=reply.get("ok",false); row.result_ok=reply.get("result",{}).get("ok",true); row.current_identity=reply.get("current_identity",{}).duplicate(true)
	return reply

## 只计原阅读撤权调用；不自行暂停、取消或授予后续许可。
func set_reading_paused(reference: Dictionary, paused: bool) -> Dictionary:
	var before: int=Time.get_ticks_usec()
	var reply: Dictionary=_native.set_reading_paused(reference,paused)
	_call("set_reading_paused",before,Time.get_ticks_usec(),reply.get("ok",false))
	return reply

## 显式诊断导出保持原返回值，并使跨导出的帧样本无效；测试仍应在段外调用。
func debug_export_state() -> Dictionary:
	var before: int=Time.get_ticks_usec()
	var reply: Dictionary=_native.debug_export_state()
	_append(_calls,{"phase":"diagnostic_export","call":"debug_export_state","wall_usec":Time.get_ticks_usec()-before,"ok":reply.get("ok",false),"excluded_from_ui":true})
	_previous_frame=0
	return reply

## 完整frame信号被测试观察到时仅记录四字段来源；没有像素呈现完成的假定。
func _frame_ready(source: Dictionary, scene_epoch: int) -> void:
	if _phase.is_empty(): return
	_append(_publications,{"phase":_phase,"observed_usec":Time.get_ticks_usec()-_started,"phase_elapsed_usec":Time.get_ticks_usec()-_phase_started,"scene_epoch":scene_epoch,"source":source.duplicate(true)})

## 报告只含小元数据/时间数组，明确本探针不能分解worker内部、GPU或冷安装成本。
func report() -> Dictionary:
	var operations: Array=[]
	for row in _operations.values():
		var copy: Dictionary=row.duplicate(true); copy.erase("accepted_at"); operations.append(copy)
	return {"format":"HM-RESIDENT-MAIN-RESPONSIVENESS/1","native_instance_id":_native.get_instance_id() if is_instance_valid(_native) else 0,"phases":_phases.duplicate(true),"calls":_calls.duplicate(true),"frame_gaps":_frames.duplicate(true),"operations":operations,"frame_ready_observations":_publications.duplicate(true),"pending_operations":_pending_count(),"dropped_records":_dropped,"timing_scope":"test wrapper synchronous wall, accepted-to-first-observed completion, process-frame gaps; no worker-internal/GPU/cold-install timing"}

## 结束采样不撤销原授权、不消费结果；只有原消费者空闲时才还原测试引用。
func detach() -> Dictionary:
	end_phase()
	if not is_instance_valid(_native): return {"ok":false,"code":"ProbeNotAttached"}
	if _backend.is_busy() or _adapter.is_busy(): return {"ok":false,"code":"ProbeRequiresIdle"}
	if _backend._bridge==self: _backend._bridge=_native
	if _adapter._bridge==self: _adapter._bridge=_native
	if is_instance_valid(_presenter) and _presenter._bridge==self: _presenter._bridge=_native
	if _backend.frame_ready.is_connected(_frame_ready): _backend.frame_ready.disconnect(_frame_ready)
	return {"ok":true}

## 小记录按固定上限保留；溢出显式计数，不能把截断样本当完整统计。
func _append(rows: Array, value: Dictionary) -> void:
	if rows.size()<LIMIT: rows.append(value)
	else: _dropped+=1

## 正常同步调用只归属当前UI段，段外oracle工作不进入热调用样本。
func _call(label: String, before: int, after: int, ok: bool) -> void:
	if not _phase.is_empty(): _append(_calls,{"phase":_phase,"call":label,"wall_usec":after-before,"ok":ok})

## 唯一复合票据作为测量关联键，epoch替换后不会与旧operation_id混淆。
func _key(reference: Dictionary) -> String:
	return str(reference.get("world_id",""))+":"+str(reference.get("execution_epoch",0))+":"+str(reference.get("operation_id",0))

## 只查看本探针的小记录，不访问worker或世界真值。
func _pending_count() -> int:
	var count: int=0
	for row in _operations.values():
		if not row.observed: count+=1
	return count
