## 眼前的动态需粮机会与本人过去回应分列；所有操作仍走原报价和具名命令。
extends RefCounted
const META="runtime_task_offers_draft"
const QUERY="runtime_interaction_tasks"

static func _draft(main, interaction: Dictionary) -> Dictionary:
	var source: Dictionary=main.kernel.source_identity()
	var dialogue: Dictionary=main._dialogue_identity(interaction)
	var draft: Dictionary=main.get_meta(META,{})
	if draft.get("source")!=source or draft.get("dialogue")!=dialogue:
		draft={"source":source.duplicate(true),"dialogue":dialogue.duplicate(true),"cursor":0,"expanded":false}
		main.set_meta(META,draft)
	return draft

## 每个实际按钮先完成同源纯读报价；未启用v2的情景不显示新入口。
static func prepare(main, interaction: Dictionary) -> bool:
	if not main._resident_mode or interaction.get("phase") not in ["say","choice"]: return true
	var draft: Dictionary=_draft(main,interaction)
	var page: Dictionary=main.kernel.demand_view(QUERY,{"cursor":draft.cursor})
	if not page.get("ok",false): return page.get("code")!="NativeAsyncRequired"
	if not draft.expanded: return true
	var ready: bool=true
	for row in page.get("rows",[]):
		for action in row.get("actions",[]):
			if main.kernel.preview(action.command,action.params,page.actor_id).get("code")=="NativeAsyncRequired": ready=false
	return ready

static func conversation(main, parent: VBoxContainer, interaction: Dictionary) -> void:
	if not main._resident_mode or interaction.get("phase") not in ["say","choice"]: return
	var draft: Dictionary=_draft(main,interaction)
	var page: Dictionary=main.kernel.demand_view(QUERY,{"cursor":draft.cursor})
	if not page.get("ok",false):
		if page.get("code")=="RuntimeOfferResponseRequiresV2": return
		main._text(parent,"正在核对公开委托…" if page.get("code")=="NativeAsyncRequired" else "公开委托暂不能查看："+_reason(main,page.get("code","Unknown")),15)
		return
	if page.get("rows",[]).is_empty() and not page.has("next_cursor"): return
	var box=VBoxContainer.new(); box.name="RuntimeTaskOffers"; parent.add_child(box)
	main._button(box,"收起公开委托" if draft.expanded else "询问公开委托与此前回应","runtime_task_offers_toggle","",_change.bind(main,not draft.expanded,draft.cursor,draft.source,draft.dialogue,main._scene_counter),main._advancing)
	if not draft.expanded: return
	for row in page.get("rows",[]):
		var item=VBoxContainer.new(); box.add_child(item)
		main._text(item,row.display_name+" · "+row.status_text,17)
		main._text(item,row.source_text,15)
		if not row.get("response_text","").is_empty(): main._text(item,row.response_text,15)
		if not row.get("progress_text","").is_empty(): main._text(item,row.progress_text,15)
		for action in row.get("actions",[]):
			var quote: Dictionary=main.kernel.preview(action.command,action.params,page.actor_id)
			var enabled: bool=action.get("available",false) and quote.get("ok",false)
			var terms: Dictionary=quote.get("contract_terms",{})
			if not terms.is_empty(): main._text(item,"承接%d份，经费上限%d钱，约定报酬%d钱；完整交付截止第%d半日（含该半日）。"%[terms.quantity,terms.purchase_budget_cap,terms.reward_total,terms.deadline_tick],15)
			var button: Button=main._button(item,action.text,action.command,row.id,func(): pass,main._advancing or not enabled,false)
			button.name="RuntimeTaskOfferAction";button.set_meta("task_action",action.duplicate(true))
			button.pressed.connect(_submit.bind(main,button,action,draft.source,draft.dialogue,main._scene_counter,page.actor_id))
			if enabled: main._text(item,"本次办理耗时%d个半日"%quote.get("duration",0),14)
			else: main._text(item,"正在核对条件…" if quote.get("code")=="NativeAsyncRequired" else "此时不能办理："+_reason(main,quote.get("code",action.get("code","Unknown"))),14)
	if draft.cursor>0: main._button(box,"返回首批","runtime_task_offers_page","",_change.bind(main,true,0,draft.source,draft.dialogue,main._scene_counter),main._advancing)
	if page.has("next_cursor"): main._button(box,"查看更多","runtime_task_offers_page","",_change.bind(main,true,page.next_cursor,draft.source,draft.dialogue,main._scene_counter),main._advancing)

static func _reason(main, code: String) -> String:
	if code=="StaleNeedRevision": return "需粮份额已有变化，请重新询问"
	if code in ["NeedNotOpen","NeedAlreadyClaimed","OfferUnavailable"]: return "这份公开委托此时已不能承接"
	if code=="SourceUnknown": return "本人尚未得知这份委托的实际来源"
	if code=="StaleRuntimeOfferResponseQuote": return "这次会面的条款已经变化，请重新询问"
	return main._reason(code)

static func _current(main, source: Dictionary, dialogue: Dictionary, epoch: int) -> bool:
	if not is_instance_valid(main) or main.is_queued_for_deletion() or not main.is_inside_tree(): return false
	return not main._advancing and main._storage_intent.is_empty() and main._scene_counter==epoch and main.kernel.source_identity()==source and main._dialogue_identity(main.kernel.interaction_view())==dialogue

## 只更新页内选择；人物、会话、世界或舞台变化后旧Callable失效。
static func _change(main, expanded: bool, cursor: int, source: Dictionary, dialogue: Dictionary, epoch: int) -> void:
	if not _current(main,source,dialogue,epoch): return
	main.set_meta(META,{"source":source.duplicate(true),"dialogue":dialogue.duplicate(true),"cursor":cursor,"expanded":expanded});main._render()

static func _submit(main, button: Variant, action: Dictionary, source: Dictionary, dialogue: Dictionary, epoch: int, actor_id: String) -> Dictionary:
	if not _current(main,source,dialogue,epoch): return {"ok":false,"code":"StaleSceneRequest"}
	if not is_instance_valid(button) or button.is_queued_for_deletion() or not button.is_inside_tree() or not button.is_visible_in_tree() or button.disabled: return {"ok":false,"code":"StaleSceneRequest"}
	var context: Dictionary=main.kernel.command_context()
	if not context.get("ok",false) or context.get("actor_id")!=actor_id or context.get("revision")!=source.revision: return {"ok":false,"code":"StalePreview"}
	var quote: Dictionary=main.kernel.preview(action.command,action.params,actor_id)
	if not quote.get("ok",false): main._message="此时不能办理："+_reason(main,quote.get("code","Unknown"));main._render();return quote
	if quote.get("revision")!=source.revision: return {"ok":false,"code":"StalePreview"}
	button.disabled=true
	return main._resident_submit_frozen({"kind":action.command,"params":action.params.duplicate(true),"actor_id":actor_id,"request_seq":context.next_request_seq,"expected_revision":source.revision})
