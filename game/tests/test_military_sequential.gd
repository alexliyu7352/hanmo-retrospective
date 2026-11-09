## 有限只读顺序投影；无按钮、无权源补值，不代GUI或实际轮次。
extends SceneTree
const Panel=preload("res://ui/military_sequential_panel.gd")
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
	for enabled in [false,true]:
		var host=Host.new();root.add_child(host);var parent=Control.new();host.add_child(parent)
		var view: Dictionary={}
		if enabled: view={"round":3,"ordinal":2,"initiative":7,"current_leadership":99999}
		var original=view.duplicate(true);Panel.render(host,parent,view);check(view==original,"read only projection")
		var text="\n".join(host.lines);check(not text.contains("99999"),"no current attribute fallback")
		if enabled: check(text.contains("第 3 轮") and text.contains("第 2 位") and text.contains("先手 7"),"actual source fields")
		else: check(parent.get_child_count()==0,"old profile no row")
		for node in parent.get_children(): check(node is Label,"no commands")
		host.free()
	print("MILITARY_SEQUENTIAL_UI=%d FAILURES=%d SCOPE=FINITE_READ_ONLY"%[checks,failures]);quit(0 if failures==0 else 1)
