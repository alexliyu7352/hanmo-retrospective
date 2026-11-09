## 普通课程资格说明的窄接口门；真实Main保持离树，仅查规则拒绝和文字，不运行GUI、存储或课程。
extends "res://tests/test_skill_study.gd"
const Main=preload("res://ui/main.gd")
const Activity=preload("res://ui/activity_session.gd")

## 不入场景树即可调用真实Main说明；把构造的空闲节点挂回宿主，只为结束时统一释放。
func message_host():
	var host=Main.new()
	for node in [host.storage,host.journey_view,host._ui_font_scale,host._ambience]:
		host.add_child(node)
	host._ambience.add_child(host._ambience.music)
	host._ambience.add_child(host._ambience.effect)
	return host

## 只核既有设施二级上限和教师同级不足，不推导新资格、费用或未冻结的课程时序。
func run_checks() -> void:
	var host=message_host()
	for mode in ["facility","teacher"]:
		var f: Dictionary=fixture(mode,300,0 if mode=="facility" else 649)
		var before: Dictionary=f.state.duplicate(true)
		var expected: String="StudyCourseLevelLimit" if mode=="facility" else "StudyTeacherNotQualified"
		var query: Dictionary=Study.courses(f.state,f.data,"student",f.params.context)
		var quote: Dictionary=Module.preview(f.state,f.data,"student",Study.KIND,f.params)
		check(query.ok and not query.courses[1].eligible and query.courses[1].code==expected and not quote.ok and quote.get("code")==expected,"当前课程查询与实际报价仍正确拒绝 "+mode)
		var original: Dictionary=quote.duplicate(true)
		var reason: String=host._reason(expected)
		var readable: bool=reason.contains("设施") and reason.contains("二级") if mode=="facility" else reason.contains("教师") and reason.contains("等级") and (reason.contains("不足") or reason.contains("不高于"))
		check(readable and reason!=expected,"真实Main中文解释既定资格原因 "+mode+"："+reason)
		var displayed: String=Activity.reason(host,quote)
		check(displayed==reason+"（"+expected+"）" and displayed.count(expected)==1,"实际Session显示可读说明并只附一次原诊断码 "+mode+"："+displayed)
		check(quote==original and f.state==before,"说明不改原拒绝、资格或世界 "+mode)
	check(not host.is_inside_tree() and not host.storage._busy and host.kernel.snapshot().is_empty(),"未挂载Main、不请求Native或启动世界")
	host.free()
	print("STUDY_FAILURE_MESSAGES_CHECKS=%d FAILURES=%d"%[checks,failures])
	quit(0 if failures==0 else 1)
