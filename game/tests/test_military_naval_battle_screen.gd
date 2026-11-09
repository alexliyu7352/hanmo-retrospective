## 实际舟战控件的过期/重复点击门，运输替身不产生World事实；正式海战银行另验。
extends SceneTree
const Screen=preload("res://ui/military_naval_battle_screen.gd")
var checks: int=0
var failures: int=0
class Kernel extends RefCounted:
	var source: Dictionary={}
	var actor_id: String="captain"
	func source_identity() -> Dictionary: return source.duplicate(true)
	func command_context() -> Dictionary: return {"ok":true,"actor_id":actor_id,"revision":source.get("revision",0)}
class Host extends Control:
	var kernel=Kernel.new()
	var _advancing: bool=false
	var _scene_counter: int=7
	var _storage_intent: Dictionary={}
	var commands: Array=[]
	func _perform(kind: String,params: Dictionary) -> void: commands.append({"kind":kind,"params":params.duplicate(true)})
## 每项独立判错，退出码随真实控件断言。
func check(value: bool,message: String) -> void:
	checks+=1
	if not value: failures+=1;print("FAIL "+message)
func _initialize() -> void: call_deferred("_run")
## 不调用命令内核或推进时间，仅检查实际屏幕的运输与来源守卫。
func _run() -> void:
	var original: Dictionary={"world_id":"0".repeat(32),"execution_epoch":2,"revision":3,"tick":4}
	for condition in ["current","revision","world","epoch","actor","kernel","scene","load","busy","storage","disabled","hidden","detached"]:
		var host=Host.new();root.add_child(host);host.kernel.source=original.duplicate(true)
		var screen=Screen.new();host.add_child(screen);screen.host=host;screen._kernel=host.kernel;screen._scene=host._scene_counter;screen._source=original.duplicate(true);screen.view={"actor_id":"captain","battle_id":"naval_battle:7"}
		var button=Button.new();screen.add_child(button)
		var key: String=screen._key("capture:ship")
		check(key==screen._key("capture:ship"),"same source keeps business key")
		match condition:
			"revision":host.kernel.source.revision+=1
			"world":host.kernel.source.world_id="1".repeat(32)
			"epoch":host.kernel.source.execution_epoch+=1
			"actor":host.kernel.actor_id="other"
			"kernel":host.kernel=Kernel.new()
			"scene":host._scene_counter+=1
			"load":host.set_meta("personal_battle_generation",1)
			"busy":host._advancing=true
			"storage":host._storage_intent={"kind":"load"}
			"disabled":button.disabled=true
			"hidden":screen.hide()
			"detached":screen.remove_child(button)
		var params: Dictionary={"battle_id":"naval_battle:7","request_key":key}
		screen._send(button,"naval_capture",params)
		check(host.commands.size()==(1 if condition=="current" else 0),"actual screen source guard "+condition)
		if condition=="current":
			check(host.commands[0].params==params and host.commands[0].kind=="naval_capture","exact command draft forwarded")
			screen._send(button,"naval_capture",params)
			check(host.commands.size()==1,"repeat click cannot dispatch again")
		if condition=="detached": button.free()
		host.free()
	for code in ["NavalCaptureEconomicResponsibilityPending","NavalCaptureNeedResponsibilityPending","NavalCaptureTaskResponsibilityPending","NavalCaptureAssetBoundaryRequired"]:
		check(not Screen.reason(code).begins_with("当前规则拒绝："),"actual responsibility refusal is described "+code)
	print("NAVAL_BATTLE_SCREEN_CHECKS=%d FAILURES=%d SCOPE=ACTUAL_CONTROL_WITH_TRANSPORT_STUB WORLD=NOT_TESTED GUI=NOT_TESTED"%[checks,failures])
	quit(0 if failures==0 else 1)
