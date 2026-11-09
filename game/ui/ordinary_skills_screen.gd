## 本人普通技艺的只读面板；控件只选择或关闭，不能取整态、提交命令或改写经验。
extends RefCounted
const Art=preload("res://ui/art_catalog.gd")
const StudyScreen=preload("res://ui/study_screen.gd")

## 沿现有墨牌、纸面、按钮和滚动控件展示本profile技艺与单项终态来源；缺失值明确未记录。
static func build(host, parent: Control, view: Dictionary) -> void:
	var panel=PanelContainer.new(); panel.name="OrdinarySkillsScreen"; parent.add_child(panel)
	panel.anchor_left=.025; panel.anchor_right=.975; panel.anchor_top=.115; panel.anchor_bottom=.975
	panel.add_theme_stylebox_override("panel",Art.ink())
	var column=VBoxContainer.new(); column.add_theme_constant_override("separation",12); panel.add_child(column)
	var header=HBoxContainer.new(); header.add_theme_constant_override("separation",16); column.add_child(header)
	# 固定短标题不参与自动折行，避免身份栏扩张后逐字堆高整个表头。
	var title: Label=host._text(header,"普通技艺",30); title.name="OrdinarySkillsHeading"
	title.autowrap_mode=TextServer.AUTOWRAP_OFF; title.size_flags_vertical=Control.SIZE_SHRINK_CENTER
	var identity: Label=host._text(header,view.display_name+" · "+("十项技艺" if view.skills.size()==10 else "十六项技艺" if view.skills.size()==16 else str(view.skills.size())+"项技艺"),18); identity.size_flags_horizontal=Control.SIZE_EXPAND_FILL; identity.size_flags_vertical=Control.SIZE_SHRINK_CENTER
	var close: Button=host._button(header,"返回","ordinary_skills_close","",func():
		if is_instance_valid(panel) and panel.is_inside_tree() and not panel.is_queued_for_deletion(): host._close_ordinary_skills()); close.name="OrdinarySkillsClose"
	column.add_child(HSeparator.new())
	var body=HBoxContainer.new(); body.size_flags_vertical=Control.SIZE_EXPAND_FILL; body.add_theme_constant_override("separation",18); column.add_child(body)
	var list_scroll=ScrollContainer.new(); list_scroll.name="OrdinarySkillsListScroll"; list_scroll.custom_minimum_size.x=350; list_scroll.horizontal_scroll_mode=ScrollContainer.SCROLL_MODE_DISABLED; body.add_child(list_scroll)
	var list=VBoxContainer.new(); list.name="OrdinarySkillsRows"; list.size_flags_horizontal=Control.SIZE_EXPAND_FILL; list.add_theme_constant_override("separation",8); list_scroll.add_child(list)
	var selected: Dictionary={}
	for row in view.skills:
		var label: String=_skill_name(row.skill_id)+"\n"+_progress(row)
		var button: Button=host._button(list,label,"ordinary_skill",row.skill_id,func():
			if is_instance_valid(panel) and panel.is_inside_tree() and not panel.is_queued_for_deletion(): host._open_ordinary_skills(row.skill_id))
		button.name="OrdinarySkill_"+row.skill_id; button.alignment=HORIZONTAL_ALIGNMENT_LEFT; button.custom_minimum_size.y=64
		button.autowrap_mode=TextServer.AUTOWRAP_WORD_SMART
		button.disabled=row.skill_id==view.selected_skill_id
		if row.skill_id==view.selected_skill_id: selected=row
	var detail=PanelContainer.new(); detail.name="OrdinarySkillDetail"; detail.set_meta("paper",true); detail.size_flags_horizontal=Control.SIZE_EXPAND_FILL; detail.add_theme_stylebox_override("panel",Art.paper()); body.add_child(detail)
	var scroll=ScrollContainer.new(); scroll.name="OrdinarySkillDetailScroll"; scroll.horizontal_scroll_mode=ScrollContainer.SCROLL_MODE_DISABLED; detail.add_child(scroll)
	var content=VBoxContainer.new(); content.size_flags_horizontal=Control.SIZE_EXPAND_FILL; content.add_theme_constant_override("separation",12); scroll.add_child(content)
	host._text(content,_skill_name(selected.skill_id),28).name="OrdinarySkillTitle"
	host._text(content,_progress(selected),20).name="OrdinarySkillProgress"
	if selected.xp!=null:
		var bar=ProgressBar.new(); bar.name="OrdinarySkillXP"; bar.max_value=selected.next_threshold if selected.next_threshold!=null else selected.xp; bar.value=selected.xp; bar.show_percentage=false; bar.custom_minimum_size.y=12; content.add_child(bar)
	content.add_child(HSeparator.new())
	host._text(content,"学习记录",22)
	host._text(content,"这里只列已结束的课程。本人的当前经验显示在上方，每门课的实际所得单独列出。" if view.get("settlement_policy","")=="per_lesson_v1" else "仅记已结课的实际所得。进行中课时尚未结算；未记录的经验来源不作推断。",16)
	var records=VBoxContainer.new(); records.name="OrdinarySkillRecords"; records.add_theme_constant_override("separation",14); content.add_child(records)
	if view.course_records.is_empty(): host._text(records,"尚无这项技艺的结课记录。",19).name="OrdinarySkillsEmpty"
	for record in view.course_records: _record(host,records,record)

## 技艺名称复用既有课程词汇，不引入另一份技艺清单或等级规则。
static func _skill_name(skill_id: String) -> String:
	return StudyScreen.SKILL_NAMES.get(skill_id,skill_id)

## 未记录与满级分别说明；等级和下一门槛全部来自原规则投影。
static func _progress(row: Dictionary) -> String:
	if row.xp==null: return "经验未记录 · 等级未定"
	return "经验 %d · 等级 %d · %s"%[row.xp,row.level,"已达最高等级" if row.next_threshold==null else "下级需 %d"%row.next_threshold]

## 显示原回执实际课时、成长和费用；中断保留真实收益，但绝不冒充完整课程。
static func _record(host, parent: Control, record: Dictionary) -> void:
	var box=VBoxContainer.new(); box.name="OrdinaryCourseRecord"; box.set_meta("action_id",record.action_id); parent.add_child(box)
	var status: String="完整结课" if record.success else "主动中断" if record.code=="StudyCancelled" else "课程中断"
	var time: String="第%d日%s"%[record.completed_tick/2+1,"上半日" if record.completed_tick%2==0 else "下半日"]
	host._text(box,status+" · "+time,20)
	if record.get("settlement_policy","")=="per_lesson_v1":
		host._text(box,"本课程经验 +%d · 实授 %d / %d 个半日"%[record.xp_gained,record.completed_lessons,record.duration],18).name="OrdinaryCourseContribution"
		host._text(box,"开课时总经验 %d · 结束时总经验 %d"%[record.xp_before,record.xp_after],16).name="OrdinaryCourseTotalXP"
	else:
		host._text(box,"经验 +%d（%d → %d） · 实授 %d / %d 个半日"%[record.xp_gained,record.xp_before,record.xp_after,record.completed_lessons,record.duration],18)
	host._text(box,"授艺：%s · %s"%[record.teacher_name,record.place_name],17)
	host._text(box,"已付 %d 钱 · 未提供部分释放 %d 钱"%[record.fee_paid,record.unprovided_released],16)
	box.add_child(HSeparator.new())
