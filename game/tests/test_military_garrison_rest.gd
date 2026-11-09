## 有限原休整来源控件；不推进时钟或把当前疲劳反推收益。
extends SceneTree
const Panel=preload("res://ui/military_garrison_rest_panel.gd")
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
	for mode in ["old","no_source","actual"]:
		var host=Host.new();root.add_child(host);var parent=Control.new();host.add_child(parent)
		var view: Dictionary={}
		if mode!="old":view={"fatigue":15,"period_count":1,"rule":"安全驻营真实足额供给半日减10","current_tick":99999}
		if mode=="actual":view.merge({"source_fact_id":"military_rest:17","supply_fact_id":"military:16","tick":3,"fatigue_before":25,"fatigue_after":15})
		var before=view.duplicate(true);Panel.render(host,parent,view);check(view==before,"pure projection "+mode)
		var text="\n".join(host.lines)
		check(not text.contains("99999"),"no elapsed time prediction")
		if mode=="old":check(parent.get_child_count()==0,"old profile has no extra row")
		elif mode=="actual":check(text.contains("25 → 15") and text.contains("military_rest:17") and text.contains("military:16"),"actual condition and exact two sources")
		else:check(parent.get_child_count()==2 and not text.contains("最近"),"no fabricated source")
		for node in parent.get_children():check(node is Label,"read only no action")
		host.free()
	print("MILITARY_GARRISON_REST_UI=%d FAILURES=%d SCOPE=FINITE_READ_ONLY"%[checks,failures]);quit(0 if failures==0 else 1)
