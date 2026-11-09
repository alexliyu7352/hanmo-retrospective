## Main自动半日的窄策略门；只核已提交课程事实，题面输入/真实付款已由独立Kernel门覆盖。
extends SceneTree
const Policy=preload("res://ui/activity_policy.gd")
var checks: int=0
var failures: int=0

## 每种允许的持久题面分支都不得因退出或答完而失去手动窗口。
func _init() -> void:
	for entry in [["study_skill","challenge"],["study_skill","etiquette"],["military_train","drill"]]:
		var fact: Dictionary={"actor_id":"p","phase":"started","terms":{entry[1]:{"mode":"manual"}}}
		var state: Dictionary={"actors":{"p":{"action_id":"action:1"}},"actions":{"action:1":{"id":"action:1","kind":entry[0],"actor_id":"p"}},"facts":{"action:1":fact}}
		check(Policy.manual_window(state,"p"),"真实手动课程保持输入窗口")
		fact[entry[1]]={"phase":"completed"}
		check(Policy.manual_window(state,"p"),"题面完成不等于真实课程结束")
		fact.terms[entry[1]].mode="automatic"
		check(not Policy.manual_window(state,"p"),"自动课保留原短行动策略")
		fact.terms[entry[1]].mode="manual"; fact.phase="completed"
		check(not Policy.manual_window(state,"p"),"已结课不能用旧事实挡未来时钟")
		fact.phase="started"; fact.actor_id="other"
		check(not Policy.manual_window(state,"p"),"其他人物事实不授当前输入窗口")
	check(not Policy.manual_window({"actors":{"p":{"action_id":""}},"actions":{},"facts":{}},"p"),"空闲不制造课程")
	print("ACTIVITY_FLOW_POLICY_CHECKS=",checks," FAILURES=",failures)
	quit(0 if failures==0 else 1)

## 失败计入退出码，不能把编译成功当作策略验证。
func check(value: bool, label: String) -> void:
	checks+=1
	if not value: failures+=1; printerr("FAIL: ",label)
