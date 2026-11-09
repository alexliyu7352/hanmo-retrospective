## 实践认定实际控件的有限来源与重复提交门；运输桩不生成习得或训练。
extends SceneTree
const TacticPanel=preload("res://ui/military_tactic_learning_panel.gd")
var checks: int=0
var failures: int=0
class Kernel extends RefCounted:
	var source: Dictionary={}
	var actor_id: String="captain"
	var ready: bool=true
	var offer_ready: bool=true
	var previews: Array=[]
	func source_identity() -> Dictionary: return source.duplicate(true)
	func command_context() -> Dictionary: return {"ok":ready,"actor_id":actor_id,"revision":source.get("revision",-1)}
	func military_camp_offer(kind: String, params: Dictionary) -> Dictionary:
		previews.append({"kind":kind,"params":params.duplicate(true)})
		return {"ok":offer_ready,"quote":{"ok":offer_ready},"code":"" if offer_ready else "NativeAsyncRequired"}
class Host extends Control:
	var kernel=Kernel.new()
	var _advancing: bool=false
	var _scene_counter: int=3
	var _storage_intent: Dictionary={}
	var commands: Array=[]
	var texts: Array=[]
	func _perform(kind: String, params: Dictionary) -> void: commands.append({"kind":kind,"params":params.duplicate(true)})
	func _text(_parent: Control, value: String, _size: int) -> void: texts.append(value)
## 每项独立累计真实断言，不把控件通过计为World或GUI验收。
func check(value: bool, message: String) -> void:
	checks+=1
	if not value: failures+=1;print("FAIL "+message)
func _initialize() -> void: call_deferred("_run")
## 直接走实际按钮和pressed回调，旧页来源/异步/换人都必须拒绝。
func _run() -> void:
	var original: Dictionary={"world_id":"0".repeat(32),"execution_epoch":2,"revision":7,"tick":4}
	for condition in ["current","revision","world","epoch","actor","kernel","scene","load","busy","storage","disabled","hidden","detached","unready","async"]:
		var host=Host.new();root.add_child(host);host.kernel.source=original.duplicate(true);host.kernel.offer_ready=condition!="async"
		var parent=Control.new();host.add_child(parent)
		var params={"training_action_id":"action:8","tactic_id":"tactic.guard"}
		TacticPanel._command(host,parent,{"source":original.duplicate(true),"actor_id":"captain"},params)
		var button: Button=parent.get_node("MilitaryTacticPractice")
		check(host.kernel.previews.size()==1 and host.kernel.previews[0].kind=="military_tactic_practice","real preview "+condition)
		var expected=params.duplicate(true);params.tactic_id="forged-after-render"
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
			"hidden":parent.hide()
			"detached":parent.remove_child(button)
			"unready":host.kernel.ready=false
		button.pressed.emit();check(host.commands.size()==(1 if condition=="current" else 0),"stale/async guard "+condition)
		if condition=="current":
			check(host.commands[0].params==expected,"exact source parameters frozen")
			button.pressed.emit();check(host.commands.size()==1,"repeat press single dispatch")
		if condition=="detached":button.free()
		host.free()
	print("TACTIC_LEARNING_UI_CHECKS=%d FAILURES=%d SCOPE=CONTROL_STUB WORLD=NOT_TESTED GUI=NOT_TESTED"%[checks,failures])
	quit(0 if failures==0 else 1)
