## 实际只读溃退控件；来源由原生页提供，不替代World轮次/存档验证。
extends SceneTree
const Panel=preload("res://ui/military_rout_panel.gd")
var checks: int=0
var failures: int=0
class Host extends Control:
	var lines: Array[String]=[]
	func _text(parent: Control,value: String,_size: int) -> Label:
		lines.append(value)
		var label=Label.new();label.text=value;parent.add_child(label);return label
func check(value: bool,message: String) -> void:
	checks+=1
	if not value: failures+=1;print("FAIL "+message)
func _initialize() -> void: call_deferred("run")
func run() -> void:
	for mode in ["old","exited","routing"]:
		var host=Host.new();root.add_child(host);var parent=Control.new();host.add_child(parent)
		var unit: Dictionary={"morale":0,"fit_count":12}
		if mode!="old": unit.rout={"active":mode=="routing","started_round":2,"blocked_rounds":1,"source_fact_id":"military_rout:17","source_round":3}
		var original=unit.duplicate(true);Panel.render(host,parent,unit)
		check(original==unit,"read-only exact projection "+mode)
		if mode=="routing":
			check(host.lines.size()==2 and host.lines[0].contains("连续受阻 1 / 2") and host.lines[1].contains("military_rout:17"),"exact progress and original source shown")
			for node in parent.get_children(): check(node is Label,"no action control or write")
		else: check(parent.get_child_count()==0,"old or exited state adds no fake active route")
		host.free()
	print("MILITARY_ROUT_UI=%d FAILURES=%d SCOPE=FINITE_READ_ONLY"%[checks,failures]);quit(0 if failures==0 else 1)
