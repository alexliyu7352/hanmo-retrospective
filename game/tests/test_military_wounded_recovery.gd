## 真实只读面板有限投影测试；不生成伤因、供给或回队事实。
extends SceneTree
const RecoveryPanel=preload("res://ui/military_recovery_panel.gd")
var checks: int=0
var failures: int=0
class Host extends Control:
	var lines: Array[String]=[]
	func _text(parent: Control, value: String, _size: int) -> Label:
		lines.append(value)
		var node=Label.new();node.text=value;parent.add_child(node);return node
## 只报告本控件断言，不能代原生World/存档验收。
func check(value: bool, message: String) -> void:
	checks+=1
	if not value: failures+=1;print("FAIL "+message)
func _initialize() -> void: call_deferred("_run")
## 逐状态调用实际面板，不把当前时钟推算成恢复进度。
func _run() -> void:
	for mode in ["disabled","invalid","empty","real","bounded"]:
		var host=Host.new();root.add_child(host);var parent=Control.new();host.add_child(parent)
		var view: Dictionary={"active":mode!="disabled","ok":mode!="invalid","observed_tick":99999,"units":[],"rule":"原生批次规则"}
		if mode in ["real","bounded"]:
			var batch: Dictionary={"wounded_tick":3,"initial_count":7,"recovered_count":2,"remaining_count":5,"qualified_half_days":2,"daily_quota":2,"last_supply_fact_id":"military:10","last_qualified_tick":8}
			var unit: Dictionary={"unit_id":"original_unit","fit_count":3,"wounded_count":5,"batches":[batch]}
			if mode=="bounded":
				for i in range(40):unit.batches.append(batch.duplicate(true))
				for i in range(50):view.units.append(unit.duplicate(true))
			else:view.units=[unit]
		var original: Dictionary=view.duplicate(true)
		RecoveryPanel.render(host,parent,view)
		check(view==original,"render never mutates original DTO "+mode)
		var combined: String="\n".join(host.lines)
		match mode:
			"disabled":check(combined.contains("未启用") and parent.get_child_count()==1,"disabled is explicit and finite")
			"invalid":check(combined.contains("来源暂不可用") and parent.get_child_count()==1,"bad native source has no progress")
			"empty":check(combined.contains("实际驻地"),"no remote army disclosure")
			"real":
				check(combined.contains("已累计 2 / 8") and combined.contains("供给 military:10"),"only actual credit and supply source shown")
				check(not combined.contains("99999"),"today tick never invents recovery credit")
			"bounded":
				var count: int=0
				for line in host.lines:
					if line.contains("已累计"):count+=1
				check(count==32*16,"projection caps actual rendered units and batches")
		for child in parent.get_children():check(child is Label,"read-only projection creates no action control")
		host.free()
	print("MILITARY_WOUNDED_RECOVERY_UI=%d FAILURES=%d SCOPE=READ_ONLY_CONTROL"%[checks,failures])
	quit(0 if failures==0 else 1)
