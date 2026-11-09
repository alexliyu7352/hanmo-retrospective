## 只验有限原资源投影，不启引擎GUI或另造命令。
extends SceneTree
const Panel=preload("res://ui/military_command_resources_panel.gd")
var checks: int=0
var failures: int=0
class Host extends Control:
	var lines: Array[String]=[]
	func _text(parent: Control,value: String,_size: int) -> Label:
		lines.append(value);var label=Label.new();label.text=value;parent.add_child(label);return label
func check(value: bool,why: String) -> void:
	checks+=1
	if not value: failures+=1;print("FAIL "+why)
func _initialize() -> void: call_deferred("run")
func run() -> void:
	for view in [{},{"queued":true,"before":40,"capacity":100,"recovered":6,"available":46,"current_tactics":99999},{"queued":false,"before":12,"capacity":100}]:
		var host=Host.new();root.add_child(host);var parent=Control.new();host.add_child(parent)
		var original=view.duplicate(true);Panel.render(host,parent,view);check(view==original,"no query mutation")
		var text="\n".join(host.lines);check(not text.contains("99999"),"no current XP fallback")
		if view.is_empty(): check(parent.get_child_count()==0,"old disabled absent")
		elif view.get("queued",false): check(text.contains("恢复 6") and text.contains("可用 46"),"actual bounded quote")
		else: check(text.contains("没有可执行回合") and not text.contains("可用"),"no hypothetical finished turn")
		for node in parent.get_children(): check(node is Label,"read only labels")
		host.free()
	print("MILITARY_COMMAND_RESOURCES_UI=%d FAILURES=%d SCOPE=FINITE_DTO"%[checks,failures]);quit(0 if failures==0 else 1)
