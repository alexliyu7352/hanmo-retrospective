## 原有限来源控件，不把静态投影断言称作实际战果或军级授权验证。
extends SceneTree
const Panel=preload("res://ui/military_command_shock_panel.gd")
var checks: int=0
var failures: int=0
class Host extends Control:
	var lines: Array[String]=[]
	func _text(parent: Control,value: String,_size: int) -> Label:
		lines.append(value);var label=Label.new();label.text=value;parent.add_child(label);return label
func check(value: bool,label: String) -> void:
	checks+=1
	if not value: failures+=1;print("FAIL "+label)
func _initialize() -> void: call_deferred("run")
func run() -> void:
	for mode in ["old","source_missing","actual"]:
		var host=Host.new();root.add_child(host);var parent=Control.new();host.add_child(parent)
		var unit: Dictionary={}
		if mode!="old": unit.command_shock={"source_fact_id":"military_rout:17" if mode=="actual" else "","source_round":3,"morale_change":-10}
		var before=unit.duplicate(true);Panel.render(host,parent,unit);check(unit==before,"pure display "+mode)
		if mode=="actual":
			check(host.lines.size()==2 and host.lines[0].contains("士气 -10") and host.lines[1].contains("military_rout:17"),"exact cause source and amount")
			for node in parent.get_children(): check(node is Label,"no action control")
		else: check(parent.get_child_count()==0,"no invented event")
		host.free()
	print("MILITARY_COMMAND_SHOCK_UI=%d FAILURES=%d SCOPE=FINITE_READ_ONLY"%[checks,failures]);quit(0 if failures==0 else 1)
