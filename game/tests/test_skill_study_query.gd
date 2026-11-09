## 普通技艺查询只核当前在场来源、本人进度、同规则报价与返回值隔离，不运行课程长套件。
extends "res://tests/test_skill_study.gd"

## 用原课程最小夹具检查只读投影，查询不可泄露教师精确经验或暗中授权远方来源。
func run_checks() -> void:
	var f: Dictionary=fixture()
	var before: Dictionary=f.state.duplicate(true)
	var result: Dictionary=Study.courses(f.state,f.data,"student",f.params.context)
	if not check(result.ok and result.courses.size()==2,"查询岗位显式两课程"): quit(1); return
	check(f.state==before,"查询不改变费用、占用、时间、经验或事实")
	var row: Dictionary=result.courses[1]
	var expected: Dictionary=Study.preview(f.state,f.data,"student",f.params)
	check(row.skill_id=="rites" and row.xp==80 and row.level==0 and row.next_threshold==100,"投影本人唯一XP及派生门槛")
	check(row.eligible and row.offer.cost==expected.cost and row.offer.expected_xp==expected.expected_xp and row.offer.payment_policy==expected.payment_policy,"候选使用同一权威预览")
	check(not row.has("teacher_xp") and not row.has("study") and not row.offer.has("teacher_xp") and not row.offer.has("study"),"投影不带教师精确XP或内部条款")
	result.context.place_id="elsewhere"; row.params.context.role_id="fake"; row.offer.cost=0
	check(Study.courses(f.state,f.data,"student",f.params.context).courses[1].offer.cost==60 and f.state==before,"返回值可改不污染源或下次查询")
	f.state.actors.student.skills.rites=300; f.state.actors.teacher.skills.rites=649
	result=Study.courses(f.state,f.data,"student",f.params.context,3)
	check(result.ok and not result.courses[1].eligible and result.courses[1].code=="StudyTeacherNotQualified" and not result.courses[1].has("offer"),"教师同等级具体拒绝，不伪造可执行报价")
	f=fixture("facility",300,0)
	result=Study.courses(f.state,f.data,"student",f.params.context)
	check(result.ok and result.courses[1].code=="StudyCourseLevelLimit" and result.courses[1].next_threshold==650,"设施达上限仍显示本人下一门槛和不可用原因")
	f=fixture(); f.state.actors.teacher.location.place_id="elsewhere"
	check(Study.courses(f.state,f.data,"student",f.params.context).get("code")=="PersonAbsent","远方教师不列成当地可学习来源")
	f=fixture(); f.state.roles["role.school"].enabled=false
	check(Study.courses(f.state,f.data,"student",f.params.context).get("code")=="RoleNotAuthorized","失效岗位查询关闭")
	f=fixture(); f.state.actors.student.injury=3
	result=Study.courses(f.state,f.data,"student",f.params.context)
	check(result.ok and result.courses[0].code=="StudyNeedsRecovery" and result.courses[1].eligible,"同场课程可按伤势分别解释重训拒绝与读书允许")
	f.data.roles["role.school"].skill_courses.append(f.data.roles["role.school"].skill_courses[0].duplicate(true))
	check(Study.courses(f.state,f.data,"student",f.params.context).get("code")=="InvalidStudyCourse","重复声明不能输出歧义课程")
	check(Study.courses(f.state,f.data,"student",f.params.context,7).get("code")=="InvalidStudyRequest","查询与提交采用同一2至6课时范围")
	print("SKILL_STUDY_QUERY_CHECKS=%d FAILURES=%d"%[checks,failures])
	quit(0 if failures==0 else 1)
