## 有限只读投影门；旧profile空行、原冻结值与无写入控件，不冒真实GUI。
extends SceneTree
const Panel=preload("res://ui/military_commanded_damage_panel.gd")
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
	for skill in ["","infantry","archery","cavalry"]:
		var host=Host.new();root.add_child(host);var parent=Control.new();host.add_child(parent)
		var view: Dictionary={}
		if not skill.is_empty(): view={"entry_tick":7,"skill_id":skill,"leadership":60,"skill_level":2,"training":20,"strength":79,"command_source_id":"action:4","current_leadership":99999}
		var original=view.duplicate(true);Panel.render(host,parent,view);check(view==original,"projection remains read only")
		var text="\n".join(host.lines);check(not text.contains("99999"),"today value cannot replace frozen entry")
		if skill.is_empty():check(parent.get_child_count()==0,"old profile has no new row")
		else:
			check(text.contains("60") and text.contains("2级") and text.contains("20") and text.contains("79") and text.contains("7 半日"),"exact source values")
			check(text.contains("action:4"),"real command source exposed")
		for node in parent.get_children():check(node is Label,"no write command")
		host.free()
	print("MILITARY_COMMANDED_DAMAGE_UI=%d FAILURES=%d SCOPE=FINITE_READ_ONLY"%[checks,failures]);quit(0 if failures==0 else 1)
