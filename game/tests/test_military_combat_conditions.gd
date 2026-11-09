## 入战冻结只读控件；不读取今日疲劳，不发军令或预测下一轮。
extends SceneTree
const Panel=preload("res://ui/military_combat_conditions_panel.gd")
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
	for mode in ["old","neutral","initial","history"]:
		var host=Host.new();root.add_child(host);var parent=Control.new();host.add_child(parent)
		var view: Dictionary={}
		if mode!="old":view={"configured":mode!="neutral","entry_tick":5,"fatigue":70,"efficiency_bp":7500,"current_fatigue":99999}
		if mode=="history":view["last_condition_fact_id"]="military_rest:17"
		var before=view.duplicate(true);Panel.render(host,parent,view);check(view==before,"read only projection "+mode)
		var text="\n".join(host.lines);check(not text.contains("99999"),"no today value substituted")
		if mode=="old":check(parent.get_child_count()==0,"old profile has no row")
		elif mode=="neutral":check(text.contains("中性") and not text.contains("入战疲劳"),"unconfigured is explicit neutral")
		else:
			check(text.contains("70") and text.contains("75%") and text.contains("5 半日"),"frozen exact rule and time")
			check(text.contains("military_rest:17" if mode=="history" else "作者初态"),"original exact source")
		for node in parent.get_children():check(node is Label,"no write action")
		host.free()
	print("MILITARY_COMBAT_CONDITION_UI=%d FAILURES=%d SCOPE=FINITE_READ_ONLY"%[checks,failures]);quit(0 if failures==0 else 1)
