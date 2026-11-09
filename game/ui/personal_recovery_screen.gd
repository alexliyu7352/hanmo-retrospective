## 本人伤势与原服务履约页，只消费有限原生来源；不会在UI减少伤势或补疗程。
extends RefCounted
const Guard=preload("res://ui/family_legacy_screen.gd")
const REASONS={"GrowthRecoveryOccupationHistoryUnavailable":"旧军事进退来源尚不能完整核验，请先完成原军旅返营。","GrowthRecoveryMilitaryOccupied":"目前仍在战斗、围城或羁押中，不能将这段时间计为安全休养。","GrowthRecoveryCancellationDenied":"该原服务已变更，或不是当前人物自己的疗程。","PersonalInjuryRecoveryUnavailable":"当前情景没有启用逐期康复。"}

## 取消仅停止原Action余下服务，已付费用和真实已挣恢复点按原账保留。
static func _cancel(host_ref: WeakRef,panel_ref: WeakRef,button_ref: WeakRef,status_ref: WeakRef,source: Dictionary,action_id: String) -> void:
	var button: Button=button_ref.get_ref(); var status: Label=status_ref.get_ref()
	if not is_instance_valid(button) or not is_instance_valid(status): return
	if not Guard._live(host_ref,panel_ref,source): button.disabled=true; status.text="人物或世界已经改变，请重新打开。"; return
	var host=host_ref.get_ref(); var panel: Control=panel_ref.get_ref()
	if button.disabled or panel.get_meta("recovery_pending",false): return
	var params={"action_id":action_id}; var quote: Dictionary=host.kernel.preview("recovery_cancel",params)
	if quote.get("code")=="NativeAsyncRequired": status.text="正在核对原服务，请稍候。"; return
	if not quote.get("ok",false) or quote.get("revision",-1)!=source.revision: status.text=REASONS.get(quote.get("code",""),"目前不能停止这份原服务，请核对当前行动。"); return
	if not button.get_meta("confirmed",false): button.set_meta("confirmed",true); button.text="确认停止余下服务"; status.text="再次确认：不退原预付费用，已经实际提供的恢复点保留，余下时间不再计点。"; return
	if not Guard._live(host_ref,panel_ref,source): return
	panel.set_meta("recovery_pending",true); button.disabled=true; host._perform("recovery_cancel",params)

## 固定高度内的有限列表自适应换行；页码、原工单ID和世界读戳一并守护迟到回调。
static func build(host,parent: Control,view: Dictionary,on_close: Callable,on_page: Callable) -> Control:
	var panel=PanelContainer.new(); panel.name="PersonalRecoveryScreen"; parent.add_child(panel)
	panel.anchor_left=.04; panel.anchor_right=.96; panel.anchor_top=.12; panel.anchor_bottom=.96
	var column=VBoxContainer.new(); panel.add_child(column); column.add_theme_constant_override("separation",10)
	var source: Dictionary=Guard._source(host)
	if view.get("actor_id")!=source.actor_id or view.get("source_identity")!=source.identity: source.ok=false
	var host_ref=weakref(host); var panel_ref=weakref(panel)
	var header=HBoxContainer.new(); column.add_child(header); host._text(header,"本人休养",25).size_flags_horizontal=Control.SIZE_EXPAND_FILL
	host._button(header,"返回","recovery_close","",func():
		var current: Control=panel_ref.get_ref()
		if is_instance_valid(current) and current.is_inside_tree() and current.is_visible_in_tree() and on_close.is_valid(): current.hide(); on_close.call())
	var injury: int=int(view.get("injury",0)); host._text(column,"%s · 体力上限%d"%[{0:"无伤",1:"轻伤",2:"中伤",3:"重伤"}.get(injury,"待核伤势"),int(view.get("hp_limit",100))],21)
	host._text(column,"本级已实际恢复%d点，完全康复还需%d点。"%[int(view.get("points_in_grade",0)),int(view.get("required_points",0))],18)
	var status: Label=host._text(column,view.get("explanation","只有实际提供的半日服务才计恢复点。"),17)
	if view.get("active") is Dictionary:
		var active: Dictionary=view.active
		host._text(column,"原服务尚余%d半日，每个实际半日%d点；继续行动仍走原世界推进。"%[int(active.get("remaining_ticks",0)),int(active.get("points_per_half_day",0))],17)
		var binding={"button":null}; var button: Button=host._button(column,"停止本次休养","recovery_cancel","",func(): _cancel(host_ref,panel_ref,binding.button,weakref(status),source,str(active.get("action_id",""))))
		binding.button=weakref(button); button.disabled=not source.ok; button.name="PersonalRecoveryCancel"
	else: host._text(column,"可在当前真正提供住宿或医馆治疗的人物处使用原服务入口。",17)
	var scroll=ScrollContainer.new(); scroll.horizontal_scroll_mode=ScrollContainer.SCROLL_MODE_DISABLED; scroll.size_flags_vertical=Control.SIZE_EXPAND_FILL; column.add_child(scroll)
	var body=VBoxContainer.new(); body.size_flags_horizontal=Control.SIZE_EXPAND_FILL; scroll.add_child(body)
	for row in view.get("rows",[]):
		var description="实际提供%d恢复点"%int(row.get("provided_points",0)) if row.get("kind")=="credit" else {"completed":"原疗程已经履约完成","cancelled":"本人停止余下服务","patient_dead":"原人物死亡，余下服务停止","provider_dead":"提供者死亡，余下服务停止"}.get(row.get("reason",""),"原服务终结")
		host._text(body,"第%d半日 · %s · %s"%[int(row.get("tick",0)),"安全住宿" if row.get("method")=="safe_lodging" else "合格医馆",description],17)
	var pager=HBoxContainer.new(); column.add_child(pager)
	for delta in [-1,1]:
		var target: int=int(view.get("page",0))+delta
		var button: Button=host._button(pager,"上一页" if delta<0 else "下一页","recovery_page","",func():
			if Guard._live(host_ref,panel_ref,source) and on_page.is_valid(): on_page.call(target))
		button.disabled=not source.ok or target<0 or target>62500 or target*16>=int(view.get("total",0))
	return panel

## 仅本人显式打开的有限休养页，读档后仍重新取同世界来源。
static func selected(host) -> bool:
	return host._resident_mode and host.get_meta("personal_recovery_open",false) and host.kernel.has_method("personal_recovery_enabled") and host.kernel.personal_recovery_enabled()

## 导航只改变页面，不能开始疗程或修改身体状态。
static func open(host) -> void:
	host.set_meta("personal_recovery_open",true); host.set_meta("personal_recovery_page",0); host._render()

## 关闭清掉临时确认控件，不取消原服务。
static func _return(host_ref: WeakRef) -> void:
	var host=host_ref.get_ref()
	if is_instance_valid(host): host.set_meta("personal_recovery_open",false); host._render()

## 原生有限查询和本人操作共享同一读戳，翻页不得跨人物或世界。
static func render(host,parent: Control) -> void:
	var source: Dictionary=Guard._source(host)
	if host.get_meta("personal_recovery_source",{})!=source: host.set_meta("personal_recovery_source",source.duplicate(true)); host.set_meta("personal_recovery_page",0)
	var ref: WeakRef=weakref(host)
	var view: Dictionary=host.kernel.personal_recovery_page(int(host.get_meta("personal_recovery_page",0)))
	build(host,parent,view,_return.bind(ref),func(page: int):
		if is_instance_valid(ref.get_ref()) and selected(ref.get_ref()) and not ref.get_ref()._advancing and page>=0 and page<=62500: ref.get_ref().set_meta("personal_recovery_page",page); ref.get_ref()._render())
