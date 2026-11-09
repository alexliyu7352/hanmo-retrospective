## 实际屏幕只展示内核的航向/水流报价；过期来源不会变成可提交军议。
extends SceneTree
const Screen=preload("res://ui/military_naval_battle_screen.gd")
var checks: int=0
var failures: int=0
class Kernel extends RefCounted:
	var source: Dictionary={}
	var quote: Dictionary={}
	func source_identity() -> Dictionary: return source.duplicate(true)
	func command_context() -> Dictionary: return {"ok":true,"actor_id":"captain","revision":source.revision}
	func naval_battle_draft(_battle,_selected,_orders) -> Dictionary: return quote.duplicate(true)
class Host extends Control:
	var kernel=Kernel.new()
	var _advancing: bool=false
	var _scene_counter: int=7
	var _storage_intent: Dictionary={}
class TacticalCanvasStub extends Control:
	var selected_id: String=""
	var orders: Dictionary={}
	var movement: Array=[]
func check(value: bool,label: String) -> void:
	checks+=1
	if not value: failures+=1;print("FAIL "+label)
func _initialize() -> void: call_deferred("_run")
func _run() -> void:
	var source: Dictionary={"world_id":"0".repeat(32),"execution_epoch":2,"revision":3,"tick":4}
	var host=Host.new();root.add_child(host);host.kernel.source=source.duplicate(true)
	var screen=Screen.new();host.add_child(screen);screen.host=host;screen._kernel=host.kernel;screen._scene=7;screen._source=source.duplicate(true)
	screen.view={"actor_id":"captain","battle_id":"battle","can_plan":true,"tactical_navigation":{"profile":"hex_helm_current_v1","quarter_turn_cost":1,"upstream_cost":1}}
	screen.selected="unit";screen.orders={"unit":{"action":"move","target_unit_id":"","destination":[1,0],"heading":3}}
	screen.canvas=TacticalCanvasStub.new();screen.add_child(screen.canvas);screen.feedback=Label.new();screen.add_child(screen.feedback);screen.submit=Button.new();screen.add_child(screen.submit)
	host.kernel.quote={"source":source.duplicate(true),"battle_id":"battle","ok":true,"code":"","movement":[{"position":[1,0],"cost":4}]}
	var original: Dictionary=screen.orders.duplicate(true);screen._refresh()
	check(screen.feedback.text.contains("合计4移动点"),"display exact quote instead of client-calculated heading price")
	check(screen.orders==original,"quote display does not rewrite order")
	check(screen._valid_draft and not screen.submit.disabled,"actual current quote permits existing submit")
	host.kernel.quote.source.revision=2;screen._refresh()
	check(not screen._valid_draft and screen.submit.disabled,"old navigation quote disables submit")
	check(screen.feedback.text.contains("已过期"),"stale source remains visible refusal")
	check(not Screen.reason("NavalDestinationUnreachable").begins_with("当前规则拒绝"),"unaffordable heading is described")
	host.free();print("NAVAL_TACTICAL_NAVIGATION_UI_CHECKS=%d FAILURES=%d SCOPE=READ_ONLY_QUOTE_WITH_TRANSPORT_STUB"%[checks,failures]);quit(0 if failures==0 else 1)
