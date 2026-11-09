## 普通技艺有限查询局部门：使用真实课程Runtime和原回执，不制造替代经验或课程公式。
extends "res://tests/test_skill_study_progress.gd"
const View=preload("res://core/views/ordinary_skills_view.gd")
const Levels=preload("res://core/domains/cultivation/skill_levels.gd")
const Kernel=preload("res://core/world_kernel.gd")

## 原课程夹具只补本人身份与名称；其真实支付、疲劳、释放和终态仍由继承工具完成。
func overview_fixture() -> Dictionary:
	var f: Dictionary=fixture()
	f.state.player_actor_id="student"
	f.data.actors={"student":{"name_key":"name.student"},"teacher":{"name_key":"name.teacher"}}
	f.data.places.school.name_key="name.school"
	f.data.texts={"name.student":"游学者","name.teacher":"授艺先生","name.school":"学舍"}
	return f

## 每个原门槛两侧与满级都派生原等级；未知经验不伪造成零，返回值不含教师账本。
func thresholds_and_isolation() -> void:
	var f: Dictionary=overview_fixture()
	for xp in [0,1,99,100,101,299,300,301,649,650,651,1199,1200]:
		f.state.actors.student.skills.rites=xp
		var before: Dictionary=f.state.duplicate(true)
		var result: Dictionary=View.query(f.state,f.data,"rites")
		var row: Dictionary=result.skills[Levels.SKILLS.find("rites")]
		var expected: int=Levels.level(xp)
		check(row.xp==xp and row.level==expected and row.next_threshold==(Levels.THRESHOLDS[expected+1] if expected<4 else null),"原门槛派生：%d"%xp)
		check(result.skills.size()==10 and result.course_records.is_empty() and before==f.state,"十项与无记录查询不写世界")
	var missing: Dictionary=View.query(f.state,f.data).skills[0]
	check(missing.xp==null and missing.level==-1 and missing.next_threshold==null,"未记录不补零或伪造下一门槛")
	check(View.query(f.state,f.data,"not_a_skill").selected_skill_id==Levels.SKILLS[0],"坏选择回到已有首项")
	var kernel=Kernel.new(); kernel._backend.state=f.state; kernel._backend.catalog={"data":f.data}
	check(kernel.ordinary_skills("rites")==View.query(f.state,f.data,"rites"),"正式Kernel与Backend消费同一查询")

## 正常、主动取消和来源失效终态都对白名单原回执；课中pending不当作已获得经验。
func settled_courses() -> void:
	for ending in ["complete","cancel","teacher_unavailable"]:
		var f: Dictionary=overview_fixture(); var action: Dictionary=begin(f)
		if action.is_empty(): continue
		advance(f,action)
		var pending: Dictionary=View.query(f.state,f.data,"rites")
		check(pending.course_records.is_empty() and pending.skills[Levels.SKILLS.find("rites")].xp==80,"在课经验未领取，半课不造终态记录")
		var original: Dictionary=advance(f,action).outcome if ending=="complete" else end_partial(f,action,ending)
		var before: Dictionary=f.state.duplicate(true)
		var result: Dictionary=View.query(f.state,f.data,"rites")
		if not check(result.course_records.size()==1,"只列选中本人终态 "+ending): continue
		var record: Dictionary=result.course_records[0]
		for key in ["success","code","xp_gained","completed_lessons","fee_paid","unprovided_released"]:
			check(record[key]==original[key],"终态原回执相等："+ending+"/"+key)
		check(record.teacher_name=="授艺先生" and record.place_name=="学舍" and record.xp_after==f.state.actors.student.skills.rites,"真实来源名和终态经验")
		check(not record.has("terms") and not record.has("teacher_xp") and not result.has("wallets") and not result.has("facts"),"白名单不泄露教师数值或权威容器")
		record.xp_gained=999; result.skills[0].xp=999
		check(f.state==before and View.query(f.state,f.data,"rites").course_records[0].xp_gained==original.xp_gained,"返回值改写不污染源或下次查询")
		var hidden: Dictionary=f.state.facts[action.id].duplicate(true); hidden.actor_id="teacher"; hidden.terms.student_id="teacher"; f.state.facts.foreign=hidden
		f.state.facts.other={"kind":Study.FACT_KIND,"phase":"completed","actor_id":"student","terms":{"student_id":"student","skill_id":"martial"}}
		f.state.facts.pending={"kind":Study.FACT_KIND,"phase":"started","actor_id":"student"}
		check(View.query(f.state,f.data,"rites").course_records.size()==1,"他人、其他技艺及未终态内容不展开")

## 聚焦新增查询和真实规则终态，不重复整世界或存读矩阵。
func run_checks() -> void:
	thresholds_and_isolation(); settled_courses()
	print("ORDINARY_SKILLS_VIEW_CHECKS=%d FAILURES=%d"%[checks,failures])
	quit(0 if failures==0 else 1)
