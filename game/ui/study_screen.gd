## 岗位课程与驻军训练的功能块：候选和收费沿正式查询，实际报名、取消和课时统一提交Kernel。
extends RefCounted
const Session=preload("res://ui/activity_session.gd")
const Challenge=preload("res://ui/challenge_screen.gd")
const Location=preload("res://core/world_location.gd")
const CampPage=preload("res://core/views/military_camp_page.gd")
const PlanControls=preload("res://ui/player_plan_controls.gd")
const SKILL_NAMES={"infantry":"步兵","cavalry":"骑兵","archery":"弓术","martial":"武艺","tactics":"兵法","commerce":"商术","negotiation":"议价","rites":"礼法","debate":"辩论","transport":"运输","naval":"水战","espionage":"谍报","fortification":"筑城","agriculture":"农政","medicine":"医术","forging":"锻造"}

## 挂在已经选择的合法岗位上下文中；不会扫描远方教师，也不从名字猜课程或直接开课。
static func render_courses(host, parent: Control, context: Dictionary) -> Control:
	var box=VBoxContainer.new(); box.name="StudyCourses"; parent.add_child(box)
	box.set_meta("activity_context",context.duplicate(true))
	var session=Session.create(host,box)
	host._text(box,"普通技艺课程",22)
	var ticks=OptionButton.new(); ticks.name="StudyDuration"; box.add_child(ticks)
	for value in range(2,7): ticks.add_item("%d个半日"%value); ticks.set_item_metadata(ticks.item_count-1,value)
	var saved: Dictionary=host.get_meta("study_course_ticks",{})
	var selected_ticks: int=clampi(saved.get(context.role_id,2),2,6)
	ticks.select(selected_ticks-2)
	var rows=VBoxContainer.new(); rows.name="StudyCourseRows"; box.add_child(rows)
	_course_rows(host,rows,context,selected_ticks)
	ticks.item_selected.connect(func(index: int):
		if not session.guard().ok: return
		var selection: Dictionary=host.get_meta("study_course_ticks",{}).duplicate()
		selection[context.role_id]=ticks.get_item_metadata(index); host.set_meta("study_course_ticks",selection)
		for child in rows.get_children(): rows.remove_child(child); child.queue_free()
		_course_rows(host,rows,context,ticks.get_item_metadata(index)))
	return box

## 时长选择只重新查询当前岗位；每次候选块有独立生命周期，旧按钮不借新参数报名。
static func _course_rows(host, parent: Control, context: Dictionary, ticks: int) -> void:
	var box=VBoxContainer.new(); parent.add_child(box); var session=Session.create(host,box)
	var actor: String=host.kernel.command_context().actor_id
	var courses: Dictionary=host.kernel.study_courses(actor,context.duplicate(true),ticks)
	if not courses.ok:
		host._text(box,Session.reason(host,courses),16); return
	if courses.courses.is_empty(): host._text(box,"这个岗位没有普通技艺课程。",17)
	for row in courses.courses:
		host._text(box,SKILL_NAMES.get(row.skill_id,row.skill_id)+" · 本人经验 %d · 等级 %d"%[row.xp,row.level],19)
		if row.next_threshold!=null: host._text(box,"下一等级累计经验：%d"%row.next_threshold,15)
		if row.eligible:
			var quote: Dictionary=row.offer
			host._text(box,"%d个半日 · 总费用%d钱 · 自动基线预计经验+%d"%[quote.duration,quote.cost,quote.expected_xp],17)
			if quote.has("learning_bonus_bp"): host._text(box,"本人这门技艺的先天学习增益：%.1f%%。只随实际课时入账；未知禀赋不补猜。"%[float(quote.learning_bonus_bp)/100.0],15)
			host._text(box,"先预留全额；每实际课时支付并获得经验，暂停保留已学所得，未提供部分释放。" if quote.get("settlement_policy","") in ["per_lesson_v1","innate_per_lesson_v1"] else "先预留全额；每实际课时支付，未提供部分释放。中断不发完整课程经验。",15)
		var automatic: Dictionary=row.params.duplicate(true); automatic["challenge_mode"]="automatic"
		session.command(box,"报名 · 自动修业","study_skill",automatic).set_meta("study_skill_id",row.skill_id)
		PlanControls.append(host,box,"course",automatic,"将这门原课程加入生活安排")
		if row.get("offer",{}).has("challenge") or row.get("offer",{}).has("etiquette"):
			var manual: Dictionary=row.params.duplicate(true); manual["challenge_mode"]="manual"
			session.command(box,"报名 · 亲自完成练习","study_skill",manual).set_meta("study_skill_id",row.skill_id)
		box.add_child(HSeparator.new())

## 营务只列本人现任公职与本营真实部队；每行资格、单位占用、公共费用和教官条件交原训练报价。
static func render_training(host, parent: Control) -> Control:
	var box=VBoxContainer.new(); box.name="StudyMilitaryTraining"; parent.add_child(box)
	var session=Session.create(host,box); var state: Dictionary=host.kernel.snapshot()
	_training_rows(host,box,session,CampPage.training_rows(state,host._data))
	return box

## 营务有限候选沿原功能块位置创建Session；共用控件主体不再另取整态。
static func render_training_view(host, parent: Control, rows: Array) -> Control:
	var box=VBoxContainer.new(); box.name="StudyMilitaryTraining"; parent.add_child(box)
	var session=Session.create(host,box)
	_training_rows(host,box,session,rows)
	return box

## 同一训练文字与按钮主体保逐行自动报价、再次command报价及可选手动报价的原序。
static func _training_rows(host, box: Control, session, rows: Array) -> void:
	host._text(box,"驻军训练",22)
	for row in rows:
		var definition: Dictionary=host._data.offices[row.office_id]; var camp: Dictionary=host._data.garrisons[row.garrison_id]
		host._text(box,host._name(definition)+" · "+row.unit_id+" · 可战%d人"%row.fit_count,18)
		if row.has("profile"): host._text(box,"实际训练%d / 100 · 单位疲劳%d / 100"%[row.profile.training,row.profile.fatigue],16)
		var params: Dictionary={"office_id":row.office_id,"unit_id":row.unit_id,"challenge_mode":"automatic"}
		var quoted: Dictionary=host.kernel.preview("military_train",params,row.actor_id)
		if quoted.ok:
			host._text(box,"%d个半日 · 本营公共费用%d钱 · 自动基线训练+%d"%[quoted.duration,quoted.cost,quoted.expected_training_gain],17)
			host._text(box,"实际提供半日才付款与增长；军粮仍由原供给扣取，中断释放未提供款项。",15)
		session.command(box,"开课 · 自动训练","military_train",params)
		if camp.has("training_drill_id"):
			params=params.duplicate(true); params.challenge_mode="manual"
			session.command(box,"开课 · 首课前亲自操练","military_train",params)
	if rows.is_empty(): host._text(box,"需先完成真实任职，并在提供训练的本营选择实际驻军。",16)

## 当前行动入口独立于原岗位对白，保存后返回仍从同一事实恢复；继续只提交原advance。
static func render_active(host, parent: Control) -> Control:
	return render_active_view(host,parent,host.kernel.study_activity())

## 当前活动有限输入共用原控件主体；未选中的活动先返回，外壳可传同一次查询而不二读来源。
static func render_active_view(host, parent: Control, activity: Dictionary) -> Control:
	if not activity.active: return null
	var box=VBoxContainer.new(); box.name="StudyActiveAction"; parent.add_child(box); var session=Session.create(host,box)
	if PlanControls.enabled(host): host._button(box,"查看生活安排","player_plan_open","",host._open_player_plan).name="StudyPlayerPlanOpen"
	var paused: bool=activity.get("paused",false)
	var segmented: bool=activity.get("settlement_policy","")=="per_lesson_v1"
	host._text(box,("普通技艺修业" if activity.kind=="study_skill" else "驻军训练")+(" · 已暂停" if paused else "")+" · 剩余%d个半日"%activity.remaining_ticks,22)
	host._text(box,"已提供%d课时 · 已支付%d钱 · 尚预留%d钱"%[activity.completed_lessons,activity.paid,activity.reserved],17).name="StudyPaymentStatus"
	if segmented:
		host._text(box,"本课程已获得经验 +%d；暂停或结束课程都保留已学所得。"%activity.xp_gained,17).name="StudyGrowthStatus"
		if activity.get("challenge_pending",false):
			host._text(box,"可以先继续听课，也可完成原练习。答完后只补已听课时应得的表现加成。",15).name="StudyChallengeProgress"
	if activity.kind=="study_skill" and activity.get("pause_supported",false):
		host._text(box,"暂停累计%d个半日。暂停释放师徒占用，未用款继续预留；恢复须原教师到场且双方有空。"%activity.get("paused_ticks",0),15).name="StudyPauseStatus"
		if paused:
			session.command(box,"恢复原课程","study_resume",{"action_id":activity.action_id}).name="StudyResume"
		else:
			session.command(box,"暂停课程，保留进度与预留款","study_pause",{"action_id":activity.action_id}).name="StudyPause"
	if not paused:
		for scope in activity.challenge_scopes: Challenge.render(host,box,scope,activity.action_id)
		session.command(box,"继续一个实际半日","advance",{}).name="StudyAdvance"
		if host._resident_mode and activity.kind=="study_skill": host.ActivityContinuationControls.render(host,box,"study_skill")
	host._text(box,"结束课程保留已获得经验并释放未提供课时的预留款，之后不能恢复本课程。" if segmented else "取消将结算已提供课时的经验并释放余款，之后不能恢复本课程。",15)
	session.command(box,"结束课程，释放未提供款项","study_cancel" if activity.kind=="study_skill" else "military_train_cancel",{"action_id":activity.action_id}).name="StudyCancel"
	return box
