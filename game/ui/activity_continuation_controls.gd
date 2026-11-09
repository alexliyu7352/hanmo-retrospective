## 已接受行动的有限许可控件；只收集整数草案，实际收费与半日始终由原生run核验。
extends RefCounted
const PlanControls=preload("res://ui/player_plan_controls.gd")
const DecisionPresenter=preload("res://ui/activity_decision_presenter.gd")
const KINDS=["travel","long_rest","study_skill"]
const LABELS={"travel":"行旅","long_rest":"休整","study_skill":"修业"}
const STOP_LABELS={"awaiting_permit":"等待下一半日","awaiting_input":"请先处理当前输入","arrived":"原行动已结束","completed":"原行动已结束","action_replaced":"原行动已改变","paused":"原行动已暂停","stranded":"当前行程受阻","blocked":"当前条件不允许继续","cancelled":"已停止自动继续","restored":"已恢复存档，请确认后再继续","budget_exhausted":"半日预算已用完","money_budget_exhausted":"费用预算不足","failed":"本次继续未能完成"}

## 三种页面挂同一套控制；没有已接受行动时不显示，也不会帮玩家接新工作或换课。
static func render(host, parent: Control, kind: String) -> void:
	if not host._resident_mode or kind not in KINDS: return
	var context: Dictionary=host.kernel.action_context()
	if context.get("action_kind")!=kind or context.get("action_id","").is_empty(): return
	var box=VBoxContainer.new(); box.name="ActivityContinuation"; parent.add_child(box)
	host._text(box,"有限继续"+LABELS[kind],19)
	PlanControls.append(host,box,"continue_action",{"action_id":context.action_id},"把原行动继续记入生活安排")
	var state: Dictionary=host._activity_continuation_state()
	var running: bool=host._resident_adapter.auto_running
	var source: Dictionary=host.kernel.source_identity()
	var scene_epoch: int=host._scene_counter
	if not state.is_empty():
		var remaining: int=maxi(0,state.get("max_half_days",0)-state.get("committed_half_days",0))
		var money: int=maxi(0,state.get("max_gross_outflow",0)-state.get("gross_outflow",0))
		host._text(box,"本次许可剩余 %d 个半日 · 最多再支出 %d 钱"%[remaining,money],16).name="ActivityContinuationRemaining"
		if not running: host._text(box,STOP_LABELS.get(state.get("stop","cancelled"),"已停止自动继续"),15).name="ActivityContinuationStopReason"
		if not running: DecisionPresenter.render(host,box,context,state)
	if running:
		host._button(box,"停止自动继续","activity_stop","",host._stop_activity_continuation).name="ActivityContinuationStop"
		host._text(box,"已开始的半日会完成；随后停住，保留原行动。",15)
		return
	var busy: bool=host._advancing or host.kernel.is_busy() or host._resident_adapter.is_busy() or not host._storage_intent.is_empty() or not host._resident_short_action.is_empty() or host._resident_step_after_plan
	var needs_input: bool=host._activity_continuation_needs_input()
	var paused: bool=context.get("travel_paused",false) or host.kernel.study_activity().get("paused",false)
	if needs_input: host._text(box,"请先亲自处理原题目、会面或战斗；仍可使用上面的手动操作。",15)
	if paused: host._text(box,"请先用原行动的恢复按钮恢复，再确认继续预算。",15)
	if not state.is_empty() and state.get("max_half_days",0)>state.get("committed_half_days",0):
		host._button(box,"确认继续剩余预算","activity_resume","",host._resume_activity_continuation.bind(state.duplicate(true),source,scene_epoch),busy or needs_input or paused).name="ActivityContinuationResume"
	var draft: Dictionary=host._activity_budget_draft(context)
	for spec in [["half_days","最多再继续（半日）",1,"ActivityContinuationHalfDays"],["money","私人钱包最多支出（钱）",0,"ActivityContinuationMoney"]]:
		host._text(box,spec[1],15)
		var spin=SpinBox.new(); spin.name=spec[3]; spin.min_value=spec[2]; spin.max_value=2147483647; spin.step=1; spin.rounded=true; spin.value=draft[spec[0]]; spin.editable=not busy
		box.add_child(spin)
		spin.value_changed.connect(host._set_activity_budget.bind(spec[0],context.actor_id,context.action_id,source,scene_epoch))
	host._button(box,"确认本次预算并继续","activity_begin","",host._begin_activity_continuation.bind(context.actor_id,context.action_id,source,scene_epoch),busy or needs_input or paused).name="ActivityContinuationBegin"
	host._text(box,"只使用本人原私人钱包；收入不增加额度，预留不算付款。阅读、选择、失焦或保存都会停止自动继续。",15)
