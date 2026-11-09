## 原有限目标提示只读DTO；无新战果、人物属性或随机，不能代Native实际轮。
extends SceneTree
const Screen = preload("res://ui/personal_battle_screen.gd")
var checks: int = 0
var failures: int = 0
## 仅记录公开文本契约的必要正反例。
func check(value: bool, label: String) -> void:
	checks += 1
	if not value:
		failures += 1
		push_error(label)
## 旧缺省提示不变，新策略只在精确公开码时出现。
func _initialize() -> void:
	var view = Screen.new()
	var goal = {"goal_rule":"defeat_opponents","timeout_rule":"draw","allow_withdrawal":false}
	check(view._goal_text(goal) == "击倒所有对手。", "old goal exact unchanged")
	goal["mutual_defeat_rule"] = "draw_v1"
	var message: String = view._goal_text(goal)
	check(message.contains("本人和全部对手同时倒地时判平局"), "selected mutual defeat named")
	check(message.contains("撤离不算同时倒地"), "withdrawal distinguished")
	goal["mutual_defeat_rule"] = "unknown"
	check(not view._goal_text(goal).contains("判平局"), "unknown code not interpreted")
	view.free()
	print("PERSONAL_SIMULTANEOUS_UI=", checks, " FAILURES=", failures)
	quit(1 if failures else 0)
