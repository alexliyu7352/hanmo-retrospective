## 直接编译被测Main原函数体；UI/transport只作有限桩，不冒充实际军旅Main或像素验收。
extends SceneTree
var checks: int=0
var failures: int=0
const HOST: String='''extends RefCounted
class Kernel extends RefCounted:
	var context: Dictionary={"action_id":"npc_return","action_kind":"army_return"}
	func source_identity() -> Dictionary: return {"revision":1}
	func action_context() -> Dictionary: return context
	func has_pending_reads() -> bool: return false
class Adapter extends RefCounted:
	var auto_running: bool=false
	func update_context(_source: Dictionary,_epoch: int) -> void: pass
	func release_refresh(_hold: Dictionary) -> void: pass
class Journey extends RefCounted:
	var auto_running: bool=false
var kernel=Kernel.new()
var journey_view=Journey.new()
var _resident_adapter=Adapter.new()
var _resident_mode: bool=true
var _started: bool=true
var _scene_counter: int=7
var _advancing: bool=false
var _resident_result: Dictionary={}
var _message: String=""
var _courses_open: bool=false
var _resident_step_after_plan: bool=false
var _rest_auto: bool=false
var _resident_trade_receipt_pending: String=""
var _resident_rendering: bool=false
var _resident_hold: Dictionary={}
var _resident_quote: Dictionary={}
var _resident_short_action: String=""
var continuations: int=0
func _reason(code: String) -> String: return code
func _render() -> void: pass
func _resident_continue_short() -> void: continuations+=1
'''
## 所有分支都执行真实函数源码，断言只观察其短行动令牌。
func check(value: bool, message: String) -> void:
	checks+=1
	if not value: failures+=1; print("FAIL: "+message)
## 原函数按下一个顶层职责说明截取，不重写被测判定表达式。
func _initialize() -> void:
	var args:=OS.get_cmdline_user_args()
	var path: String=args[0] if not args.is_empty() else "res://ui/main.gd"
	var source:=FileAccess.get_file_as_string(path)
	var start:=source.find("func _resident_frame_ready(")
	var finish:=source.find("\n## ",start)
	check(start>=0 and finish>start,"精确定位原Main函数")
	if failures>0: quit(1); return
	var script:=GDScript.new();script.source_code=HOST+source.substr(start,finish-start)+"\n"
	check(script.reload()==OK,"原函数与有限边界桩可编译")
	if failures>0: quit(1); return
	for row in [
		["wait",{},"npc_return","army_return",true,"","附属wait不能继承NPC新返营行动"],
		["wait",{"action_id":"personal_wait"},"personal_wait","wait",true,"personal_wait","普通本人wait仍启动原首半日"],
		["wait",{},"","",true,"","无后继行动的附属wait无续时"],
		["wait",{},"travel","travel",true,"","附属wait不自动接旅行"],
		["advance",{},"npc_return","army_return",true,"","advance保持不接新短行动"],
		["trade",{"action_id":"trade_action"},"trade_action","trade",true,"trade_action","本人新交易仍原短行动"],
		["study_cancel",{},"new_npc_action","army_return",true,"","课程取消原边界不接新行动"],
		["wait",{},"npc_return","army_return",false,"","失败wait不接后继行动"]
	]:
		var host=script.new();host.kernel.context={"action_id":row[2],"action_kind":row[3]};host._resident_result={"kind":row[0],"result":{"ok":row[4],"details":row[1]}}
		host._resident_frame_ready({"revision":1},7)
		check(host._resident_short_action==row[5],row[6])
	var stale=script.new();stale._resident_result={"kind":"wait","result":{"ok":true,"details":{}}};stale._resident_short_action="unchanged";stale._resident_frame_ready({"revision":0},7)
	check(stale._resident_short_action=="unchanged" and stale.continuations==0,"过期source不改变续时令牌")
	stale._resident_frame_ready({"revision":1},6);check(stale._resident_short_action=="unchanged" and stale.continuations==0,"过期scene不改变续时令牌")
	print("MILITARY_ATTACHED_WAIT_CONTINUATION_CHECKS=%d FAILURES=%d SOURCE=EXACT_MAIN_FUNCTION GUI=NOT_TESTED"%[checks,failures])
	quit(0 if failures==0 else 1)
