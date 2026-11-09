## 已学武功/心法的实际修炼入口；只消费原报价并提交practice_skill，不自行计算或发放掌握度。
extends RefCounted

## 时长只存本域显示选择；换世界或人物即清空，不进入银行、不改变业务条款。
static func _choices(host) -> Dictionary:
	var context: Dictionary=host.kernel.command_context(); var identity: Dictionary=host.kernel.source_identity()
	var source: Dictionary={"world_id":identity.get("world_id",""),"actor_id":context.get("actor_id","")}
	var saved: Dictionary=host.get_meta("card_practice_choices",{})
	if saved.get("source",{})!=source:
		saved={"source":source,"ticks":{}}; host.set_meta("card_practice_choices",saved)
	return saved

## prepare和渲染调用同一参数函数，异步读屏障重绘不把用户选的时长变回默认。
static func params(host, row: Dictionary) -> Dictionary:
	var saved: Dictionary=_choices(host)
	return {"skill_id":str(row.get("personal_skill_id","")),"ticks":clampi(int(saved.ticks.get(row.get("personal_skill_id",""),1)),1,8)}

## 只为当前页本局已学且尚未满掌握的本领准备原报价。
static func requests(host, rows: Array) -> Array:
	var result: Array=[]
	for row in rows:
		if row.get("learned",false) and not str(row.get("personal_skill_id","")).is_empty() and int(row.get("mastery",100))<100: result.append(params(host,row))
	return result

## 每次选择与点击都核同一页面和源身份，已关闭/换人/换世界的旧控件不会重新绑定。
static func _live(host_ref: WeakRef, panel_ref: WeakRef, source: Dictionary, source_read: Callable) -> bool:
	var host=host_ref.get_ref(); var panel: Control=panel_ref.get_ref()
	return is_instance_valid(host) and is_instance_valid(panel) and panel.is_inside_tree() and panel.is_visible_in_tree() and not panel.is_queued_for_deletion() and not panel.get_meta("card_command_pending",false) and not host._advancing and source.get("ok",false) and source_read.is_valid() and source==source_read.call(host,source.actor_id)

## 完整学习报价来自原成长规则；界面核人物/技能/时长和当前掌握，不能用估算冒正式收益。
static func _refresh(host_ref: WeakRef, panel_ref: WeakRef, duration_ref: WeakRef, button_ref: WeakRef, status_ref: WeakRef, source: Dictionary, source_read: Callable, skill_id: String, mastery: int, binding: Dictionary) -> bool:
	var host=host_ref.get_ref(); var duration: OptionButton=duration_ref.get_ref(); var button: Button=button_ref.get_ref(); var status: Label=status_ref.get_ref()
	if not is_instance_valid(host) or not is_instance_valid(duration) or not is_instance_valid(button) or not is_instance_valid(status): return false
	button.disabled=true; binding.clear()
	if not _live(host_ref,panel_ref,source,source_read): status.text="人物状态已变化或正在行动，请稍后重新查看。"; return false
	var ticks: int=duration.get_item_metadata(duration.selected)
	var params: Dictionary={"skill_id":skill_id,"ticks":ticks}
	var quote: Dictionary=host.kernel.preview("practice_skill",params)
	if not quote.get("ok",false):
		var reasons: Dictionary={"TrainingNeedsRest":"请先恢复伤势或疲劳。","TrainingPlaceRequired":"需先到达可停留的场所。","ActorBusy":"请先结束当前行动。","SkillAlreadyMastered":"已达到当前掌握上限。","SkillNotLearned":"本局尚未学会这项本领。"}
		status.text=reasons.get(quote.get("code",""),"当前条件不满足，暂时无法修炼。"); return false
	var training: Variant=quote.get("training")
	if not training is Dictionary or quote.get("revision",-1)!=source.revision or quote.get("duration")!=ticks or training.get("duration")!=ticks or training.get("student_id")!=source.actor_id or training.get("skill_id")!=skill_id or training.get("mastery_before")!=mastery or not training.get("mastery_after") is int or training.mastery_after<mastery or training.mastery_after>100 or not training.get("student_fatigue") is int or not quote.get("cost") is int:
		status.text="修炼报价已变化，请重新查看本领。"; return false
	if not _live(host_ref,panel_ref,source,source_read): return false
	binding.params=params; button.disabled=false
	status.text="%d个半日 · 掌握 %d → %d / 100 · 疲劳增加%d · 费用%d钱"%[quote.duration,mastery,training.mastery_after,training.student_fatigue,quote.cost]
	return true

## 提交前再次沿同一规则报价，一页只发一次命令；实际推进和完成仍由原Main处理。
static func _submit(host_ref: WeakRef, panel_ref: WeakRef, duration_ref: WeakRef, button_ref: WeakRef, status_ref: WeakRef, source: Dictionary, source_read: Callable, skill_id: String, mastery: int, binding: Dictionary) -> void:
	if not _refresh(host_ref,panel_ref,duration_ref,button_ref,status_ref,source,source_read,skill_id,mastery,binding): return
	var host=host_ref.get_ref(); var panel: Control=panel_ref.get_ref(); var button: Button=button_ref.get_ref()
	if not is_instance_valid(host) or not is_instance_valid(panel) or not is_instance_valid(button): return
	panel.set_meta("card_command_pending",true); button.disabled=true
	host._perform("practice_skill",binding.params.duplicate(true))

## 时长变更先保存显示选择，再走原Main读屏障；有限测试宿主可直接刷新报价。
static func _selected(host_ref: WeakRef, panel_ref: WeakRef, duration_ref: WeakRef, button_ref: WeakRef, status_ref: WeakRef, source: Dictionary, source_read: Callable, skill_id: String, mastery: int, binding: Dictionary) -> void:
	if not _live(host_ref,panel_ref,source,source_read): return
	var host=host_ref.get_ref(); var duration: OptionButton=duration_ref.get_ref()
	if not is_instance_valid(host) or not is_instance_valid(duration): return
	var saved: Dictionary=_choices(host); saved.ticks[skill_id]=duration.get_item_metadata(duration.selected); host.set_meta("card_practice_choices",saved)
	if host.has_method("_render"): host._render()
	else: _refresh(host_ref,panel_ref,duration_ref,button_ref,status_ref,source,source_read,skill_id,mastery,binding)

## 时长沿原规则1至8半日，收益/疲劳全部读原preview；图鉴发现而未学会的卡不会调用此入口。
static func build(host, parent: Control, panel: Control, row: Dictionary, source: Dictionary, source_read: Callable) -> void:
	if int(row.get("mastery",100))>=100: host._text(parent,"已达到当前掌握上限。",16); return
	var box=VBoxContainer.new(); box.name="CardPracticeControls"; parent.add_child(box)
	var duration=OptionButton.new(); duration.name="CardPracticeDuration"; box.add_child(duration)
	for ticks in range(1,9): duration.add_item("修炼%d个半日"%ticks); duration.set_item_metadata(duration.item_count-1,ticks)
	duration.select(params(host,row).ticks-1)
	var status: Label=host._text(box,"",15); status.name="CardPracticeStatus"
	var host_ref: WeakRef=weakref(host); var panel_ref: WeakRef=weakref(panel); var duration_ref: WeakRef=weakref(duration); var status_ref: WeakRef=weakref(status)
	var binding: Dictionary={}; var button_refs: Dictionary={}; var skill_id: String=row.personal_skill_id; var mastery: int=row.mastery
	var button: Button=host._button(box,"开始修炼","practice_skill",skill_id,func(): _submit(host_ref,panel_ref,duration_ref,button_refs.button_ref,status_ref,source,source_read,skill_id,mastery,binding))
	button.name="CardPractice"; button_refs.button_ref=weakref(button)
	duration.item_selected.connect(func(_index: int): _selected(host_ref,panel_ref,duration_ref,button_refs.button_ref,status_ref,source,source_read,skill_id,mastery,binding))
	_refresh(host_ref,panel_ref,duration_ref,button_refs.button_ref,status_ref,source,source_read,skill_id,mastery,binding)
