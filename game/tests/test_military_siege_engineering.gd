extends SceneTree
## 只核原围城页面消费原生有限报价；不通过客户端属性重算伤害或提交新资产命令。
func _initialize() -> void:
	var source: String=FileAccess.get_file_as_string("res://ui/siege_screen.gd")
	var checks: Array[bool]=[source.contains('if unit.has("fortification_level")'),source.contains('int(unit.fortification_level)'),source.contains('host.kernel.preview("siege_plan"'),source.contains('"siege_plan"'),source.contains('"siege_close"')]
	for check in checks:
		if not check: push_error("MilitarySiegeEngineeringViewSourceMissing"); quit(1); return
	quit(0)
