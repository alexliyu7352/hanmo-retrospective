## 测量器自身的有限协议检查；fixture没有世界，结果不能替代真实Main响应性数据。
extends SceneTree
const Probe=preload("main_responsiveness_probe.gd")
class Native:
	extends RefCounted
	var calls: int=0
	var ready: bool=false
	var received: Dictionary={}
	var reference: Dictionary={"world_id":"1".repeat(32),"execution_epoch":1,"operation_id":1}
	func enqueue_live_operation(operation: Dictionary) -> Dictionary:
		calls+=1; received=operation.duplicate(true); return {"ok":true,"reference":reference.duplicate(true)}
	func poll_live_operation(_reference: Dictionary) -> Dictionary:
		calls+=1
		return {"ok":true,"ready":ready,"result":{"ok":true},"current_identity":received.source.duplicate(true)}
	func set_reading_paused(_reference: Dictionary, _paused: bool) -> Dictionary:
		calls+=1; return {"ok":true}
	func debug_export_state() -> Dictionary:
		calls+=1; return {"ok":true,"json":"{\"tick\":0}"}
class Consumer:
	extends Node
	signal frame_ready(source: Dictionary, scene_epoch: int)
	var _bridge: Object
	var _presenter: Node
	var busy: bool=false
	func transport() -> Object: return _bridge
	func is_busy() -> bool: return busy
var checks: int=0
var failures: int=0
## 检查失败完整计数；不依赖性能阈值或不稳定的微秒长度。
func check(value: bool, label: String) -> void:
	checks+=1
	if not value: failures+=1; push_error(label)
## 独立帧循环只检验转发次数、采样边界与深拥有报告，没有世界阶段执行。
func _initialize() -> void: call_deferred("_run")
## 原生调用仅由显式消费者发起，重复poll只记录一次完成，导出被单独标注。
func _run() -> void:
	var native=Native.new(); var backend=Consumer.new(); var adapter=Consumer.new(); var presenter=Consumer.new(); var probe=Probe.new()
	backend._bridge=native; adapter._bridge=native; presenter._bridge=native; backend._presenter=presenter
	root.add_child(backend); root.add_child(adapter); backend.add_child(presenter); root.add_child(probe)
	backend.busy=true; check(not probe.attach(backend,adapter).ok,"busy attach rejected"); backend.busy=false
	check(probe.attach(backend,adapter).ok,"idle attach accepted")
	check(backend.transport()==probe and adapter._bridge==probe and presenter._bridge==probe,"same consumers routed through test wrapper")
	check(native.calls==0,"attach performs no native calls")
	probe.begin_phase("ui")
	var packet: Dictionary={"kind":"query","source":{"world_id":"1".repeat(32),"execution_epoch":1,"revision":9223372036854775807,"tick":0},"payload":{"query_id":"command_context","args":{}}}
	var accepted: Dictionary=backend.transport().enqueue_live_operation(packet)
	check(accepted.ok and native.received==packet,"request and int64 metadata unchanged")
	var pending: Dictionary=adapter._bridge.poll_live_operation(accepted.reference)
	check(pending.ready==false,"original pending reply returned")
	await process_frame; await process_frame; await process_frame
	native.ready=true
	check(adapter._bridge.poll_live_operation(accepted.reference).ready,"original ready reply returned")
	adapter._bridge.poll_live_operation(accepted.reference)
	backend.frame_ready.emit(packet.source,7)
	check(probe.set_reading_paused({},true).ok,"reading delegated")
	check(probe.measure_call("button",func(): return 37)==37,"button return unchanged")
	probe.end_phase()
	check(probe.debug_export_state().json=="{\"tick\":0}","diagnostic export exact")
	var report: Dictionary=probe.report()
	check(native.calls==6,"sampler issued no extra polls or operations")
	check(report.operations.size()==1 and report.operations[0].observed and not report.operations[0].crossed_phase,"first completion recorded once")
	check(report.pending_operations==0 and report.dropped_records==0,"all accepted work observed without dropped data")
	check(report.frame_gaps.size()>=1 and report.frame_ready_observations.size()==1,"process gaps and signal observation recorded")
	check(report.phases.size()==1 and report.calls.back().excluded_from_ui,"oracle export separate from UI phase")
	report.operations[0].source.tick=999
	check(probe.report().operations[0].source.tick==0,"report metadata is deeply owned")
	backend.busy=true; check(not probe.detach().ok,"busy detach refused"); backend.busy=false
	check(probe.detach().ok and backend.transport()==native and adapter._bridge==native and presenter._bridge==native,"original one transport restored")
	probe.queue_free(); adapter.queue_free(); backend.queue_free(); await process_frame
	print("MAIN_RESPONSIVENESS_PROBE_CHECKS=",checks," FAILURES=",failures," ACTUAL_MAIN_MEASURED=false")
	quit(0 if failures==0 else 1)
