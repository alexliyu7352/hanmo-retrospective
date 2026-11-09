## 请求界面有限门：载入真实全屏脚本，传输/控件当前性作桩，不冒充Main或战场规则验收。
extends SceneTree
var checks: int=0
var failures: int=0
class Host extends RefCounted:
	var commands: Array=[]
	func _perform(kind: String,params: Dictionary) -> void: commands.append({"kind":kind,"params":params.duplicate(true)})
## 完整脚本照常解析；仅隔离活树与后端运输，真实请求构造和草案恢复仍来自正式方法。
const EXTENSION: String='''extends "res://ui/military_battle_screen.gd"
var current: bool=true
func _control_current(_button: Variant) -> bool: return current and not _committing
'''
func check(value: bool, message: String) -> void:
	checks+=1
	if not value: failures+=1;print("FAIL: "+message)
func fixture(script: GDScript):
	var screen=script.new();screen.host=Host.new()
	screen.identity={"actor_id":"player","revision":7,"battle_id":"battle:1","round":2,"phase":"planning"}
	screen.header={"controller_side":0,"command_scope":"unit"}
	screen.units={"own":{"controllable":true,"side":0,"withdrawn":false,"fit_count":10},"enemy":{"controllable":false,"side":1,"withdrawn":false,"fit_count":10}}
	screen.page_view={"requests":{"boundary_open":true,"used_this_round":0,"per_actor_round_limit":1,"history":[]}}
	screen.selected_id="own";screen.inspected_id="enemy";screen.orders={"own":{"action":"attack","target_id":"enemy","private_draft":true}}
	screen._bound_kernel_id=3;screen._bound_load_generation=4;screen._bound_source={"world_id":"0".repeat(32),"execution_epoch":1,"revision":7,"tick":4}
	screen._draft_signature={"kernel_id":3,"load_generation":4,"source":screen._bound_source.duplicate(true),"identity":screen.identity.duplicate(true)}
	return screen
func _initialize() -> void:
	var script=GDScript.new();script.source_code=EXTENSION
	check(script.reload()==OK,"真实完整屏幕及依赖可解析")
	if failures>0:quit(1);return
	for kind in ["support","withdrawal","focus_target"]:
		var screen=fixture(script);var button=Button.new()
		screen._send_request(kind,button)
		check(screen.host.commands.size()==1,"每个意图只提交一次 "+kind)
		var params: Dictionary=screen.host.commands[0].params
		check(params.request_kind==kind and params.unit_id=="own" and params.battle_id=="battle:1","保持独立意图和本人范围 "+kind)
		check(params.target_unit_id==("enemy" if kind=="focus_target" else "") and not params.has("orders") and not params.has("commander_actor_id"),"不泄露草案或选择统帅 "+kind)
		screen._send_request(kind,button);check(screen.host.commands.size()==1,"原按钮重复不能再发 "+kind)
		screen.free();button.free()
	for condition in ["stale","quota","closed","disabled","friendly","unknown"]:
		var screen=fixture(script);var button=Button.new();var kind="focus_target"
		match condition:
			"stale":screen.current=false
			"quota":screen.page_view.requests.used_this_round=1
			"closed":screen.page_view.requests.boundary_open=false
			"disabled":button.disabled=true
			"friendly":screen.inspected_id="own"
			"unknown":kind="order_army"
		screen._send_request(kind,button);check(screen.host.commands.is_empty(),"无权来源不提交 "+condition)
		screen.free();button.free()
	for condition in ["same","other_round","other_revision","other_world","other_epoch","other_tick","missing_receipt"]:
		var screen=fixture(script);var button=Button.new();screen._send_request("support",button)
		var key: String=screen.host.commands[0].params.request_key
		screen.identity.revision=8;screen._bound_source.revision=8;screen.page_view.requests.history=[{"request_key":key,"round":2}]
		match condition:
			"other_round":screen.identity.round=3
			"other_revision":screen.identity.revision=9
			"other_world":screen._bound_source.world_id="1".repeat(32)
			"other_epoch":screen._bound_source.execution_epoch=2
			"other_tick":screen._bound_source.tick=5
			"missing_receipt":screen.page_view.requests.history=[]
		screen.orders={};var restored: bool=screen._restore_request_draft()
		check(restored==(condition=="same"),"只恢复同次已裁定请求草案 "+condition)
		if restored:check(screen.orders.own.private_draft and not screen.host.has_meta("military_request_draft"),"恢复真实本地草案且消费一次元数据")
		screen.free();button.free()
	print("MILITARY_REQUEST_UI_CHECKS=%d FAILURES=%d SOURCE=ACTUAL_SCREEN_WITH_TRANSPORT_STUB GUI=NOT_TESTED" % [checks,failures])
	quit(0 if failures==0 else 1)
