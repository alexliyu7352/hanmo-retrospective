## 实际只读缺粮控件的小投影门；原生来源与正常World另验。
extends SceneTree
const Panel=preload("res://ui/military_supply_attrition_panel.gd")
var checks: int=0
var failures: int=0
class Host extends Control:
	var lines: Array[String]=[]
	func _text(parent: Control, value: String, _size: int) -> Label:
		lines.append(value)
		var node=Label.new();node.text=value;parent.add_child(node);return node
func check(value: bool, message: String) -> void:
	checks+=1
	if not value: failures+=1;print("FAIL "+message)
func _initialize() -> void: call_deferred("_run")
func _run() -> void:
	for mode in ["disabled","invalid","empty","real","bounded"]:
		var host=Host.new();root.add_child(host);var parent=Control.new();host.add_child(parent)
		var view: Dictionary={"active":mode!="disabled","ok":mode!="invalid","observed_tick":99999,"units":[],"rule":"原生匿名士卒供给规则"}
		if mode in ["real","bounded"]:
			var unit: Dictionary={"unit_id":"original_unit","fit_count":49,"deserted_count":2,"morale":50,"last_supply_tick":2,"last_supply_fact_id":"military:3","consecutive_shortage_half_days":2,"last_eligible_count":51,"last_supply_desertion_quota":2,"last_attrition_tick":2,"last_attrition_fact_id":"military_attrition:4","last_deserted_count":2}
			for i in range(50 if mode=="bounded" else 1):view.units.append(unit.duplicate(true))
		var original: Dictionary=view.duplicate(true);Panel.render(host,parent,view)
		check(view==original,"render preserves actual DTO "+mode)
		var combined: String="\n".join(host.lines)
		match mode:
			"disabled":check(parent.get_child_count()==0,"old disabled profile adds no controls")
			"invalid":check(combined.contains("来源暂不可用") and parent.get_child_count()==1,"invalid source does not invent shortage")
			"empty":check(combined.contains("实际驻营"),"no arbitrary remote roster disclosure")
			"real":
				check(combined.contains("连续缺粮 2") and combined.contains("military:3") and combined.contains("military_attrition:4"),"real supply and loss sources shown")
				check(not combined.contains("99999"),"today tick never derives missed loss periods")
			"bounded":
				var count: int=0
				for line in host.lines:
					if line.contains("供给来源"):count+=1
				check(count==32,"bounded actual units")
		for child in parent.get_children():check(child is Label,"read-only view has no mutating action")
		host.free()
	print("MILITARY_SUPPLY_ATTRITION_UI=%d FAILURES=%d SCOPE=READ_ONLY_CONTROL"%[checks,failures])
	quit(0 if failures==0 else 1)
