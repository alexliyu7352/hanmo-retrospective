## 原Main异步边界的窄协议测试；不冒充真实Bridge、GUI或完整行旅验收。
extends SceneTree
const Main=preload("res://ui/main.gd")
var checks: int=0
var failures: int=0
class Backend extends Node:
	var source: Dictionary={"world_id":"world","execution_epoch":2,"revision":4,"tick":3}
	var submitted: Array=[]
	var busy: bool=false
	var refresh_count: int=0
	var invalidations: int=0
	func invalidate() -> void: invalidations+=1
	func source_identity() -> Dictionary: return source.duplicate(true)
	func command_context() -> Dictionary: return {"ok":true,"actor_id":"actor.player","next_request_seq":5,"revision":4}
	func is_busy() -> bool: return busy
	func submit_async(command: Dictionary,_scene: int) -> Dictionary:
		submitted.append(command.duplicate(true)); return {"ok":true}
	func adopt_source(value: Dictionary) -> Dictionary:
		source=value.duplicate(true); return {"ok":true}
	func refresh(_scene: int) -> Dictionary:
		refresh_count+=1; busy=true; return {"ok":true}
class Adapter extends Node:
	var auto_running: bool=true
	var busy: bool=false
	var cancels: int=0
	var holds: int=0
	var updates: Array=[]
	var backend: Node
	var invalidate_at_cancel: int=0
	func is_busy() -> bool: return busy
	func suspend(_reason: String) -> Dictionary:
		invalidate_at_cancel=backend.invalidations
		cancels+=1; auto_running=false; busy=true; return {"ok":true}
	func update_context(source: Dictionary,_scene: int) -> Dictionary:
		updates.append(source.duplicate(true)); return {"ok":true}
	func hold_for_refresh(source: Dictionary) -> Dictionary:
		holds+=1; return {"ok":true,"token":{"source":source.duplicate(true)}}
func check(value: bool,label: String) -> void:
	checks+=1
	if not value: failures+=1; push_error(label)
func _initialize() -> void: call_deferred("_run")
func _run() -> void:
	var m=Main.new(); root.add_child(m); m.set_process(false)
	var backend=Backend.new(); m.add_child(backend); var adapter=Adapter.new(); m.add_child(adapter); adapter.backend=backend
	m.kernel=backend; m._resident_adapter=adapter; m._resident_mode=true; m._started=true
	m.journey_view.auto_running=true
	m._auto_step(); m.journey_view.step_requested.emit()
	check(backend.submitted.is_empty(),"旧Timer与旧信号均不能提交额外半日")
	adapter.busy=true
	m._perform("pause_travel",{})
	check(m._resident_command.is_empty() and adapter.cancels==0,"在途grant期间拒绝普通命令，不能抢同一个request_seq")
	adapter.busy=false
	m._perform("pause_travel",{})
	check(adapter.cancels==1 and m._resident_command.expected_revision==4,"空闲时实际Main撤销许可并保留已接纳请求身份")
	m._resident_process()
	check(backend.submitted.is_empty(),"取消票据未消费前不抢唯一正常槽")
	adapter.busy=false
	m._resident_process()
	check(backend.submitted.size()==1 and backend.submitted[0].expected_revision==4 and backend.submitted[0].request_seq==5,"已停驻取消后提交原封不动的唯一请求")
	m._resident_process(); check(backend.submitted.size()==1,"同一次请求只入队一次")
	var committed: Dictionary=backend.source.duplicate(true); committed.revision=6; committed.tick=5
	m._resident_run_completed("grant",{"ok":true},backend.source,committed)
	check(backend.source==committed and adapter.holds==1 and m._resident_frame_pending,"真实完成来源先收养，刷新期间不发新许可")
	m._resident_process()
	check(backend.refresh_count==1 and backend.busy,"原Main只安排一次完整有限frame刷新")
	m._notification(Node.NOTIFICATION_WM_WINDOW_FOCUS_OUT)
	check(adapter.cancels==2 and not adapter.auto_running,"失焦明确撤销自动许可")
	backend.busy=false; m._resident_frame_pending=false; m._storage_intent="save_list"
	m._resident_run_completed("cancel_run",{"ok":true},backend.source,backend.source)
	check(not m._resident_frame_pending,"零推进取消不重绘已打开的存槽确认页面")
	var slot_identity: Dictionary=m._slot_world_identity()
	check(slot_identity.world_id=="world" and slot_identity.execution_epoch==2 and slot_identity.revision==6,"存槽身份直接绑定resident epoch和revision，无GD快照")
	m._selected_slot="manual-1"; m._save_generation=3; m._resident_slot={"slot":"manual-2","generation":7,"mode":"save"}
	m._resident_slot_finished({"ok":false,"code":"NativeValidationMismatch"})
	check(m._selected_slot=="manual-1" and m._save_generation==3 and m._storage_intent.is_empty(),"失败保存不更换已选槽或代数")
	m._resident_hold={"source":backend.source.duplicate(true)}; adapter.auto_running=true
	m._resident_failed({"ok":false,"code":"UnsupportedLiveQuery"})
	check(not adapter.auto_running and m._resident_hold.is_empty(),"必需frame失败撤销授权和hold，不遗留无法重启的run")
	backend.busy=true; m._resident_frame_pending=false
	var invalidations_before: int=backend.invalidations
	m._stop_journey_auto()
	check(adapter.invalidate_at_cancel>invalidations_before and m._resident_frame_pending,"先撤销未完读链再cancel，排空后恢复有限frame")
	check(m._open_slots("load").get("code")=="ActorBusy","旧读或cancel未排空不打开身份会漂移的槽窗口")
	m._resident_mode=false; m.queue_free(); await process_frame
	print("RESIDENT_MAIN_ROUTING_CHECKS=",checks," FAILURES=",failures," PROTOCOL_ONLY=true")
	quit(0 if failures==0 else 1)
