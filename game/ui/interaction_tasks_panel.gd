## 当前会面中的委托选择只消费有限原生查询；不建立任务副本、不代领酬劳。
extends RefCounted

## 页读准备与渲染共用本地选择，不把旧说话人、旧版本或旧页码绑定到新会面。
static func _draft(main, interaction: Dictionary) -> Dictionary:
	var identity: Dictionary=main._dialogue_identity(interaction)
	var source: Dictionary=main.kernel.source_identity()
	var draft: Dictionary=main.get_meta("interaction_tasks_draft",{})
	if draft.get("source")!=source or draft.get("dialogue")!=identity:
		draft={"source":source.duplicate(true),"dialogue":identity.duplicate(true),"cursor":0,"cancel_id":"","expanded":false}
		main.set_meta("interaction_tasks_draft",draft)
	return draft

## 只登记本页异步纯读需求；展开后的实际按钮报价全部就绪才允许发布页面。
static func prepare(main, interaction: Dictionary) -> bool:
	if not main._resident_mode or interaction.get("phase") not in ["say","choice"]: return true
	var draft: Dictionary=_draft(main,interaction)
	var view: Dictionary=main.kernel.demand_view("interaction_tasks",{"cursor":draft.cursor})
	if not view.get("ok",false): return view.get("code")!="NativeAsyncRequired"
	if not draft.get("expanded",false): return true
	var ready: bool=true
	for row in view.get("rows",[]):
		for action in row.get("actions",[]):
			var quote: Dictionary=main.kernel.preview(action.command,action.params,view.actor_id)
			if quote.get("code")=="NativeAsyncRequired": ready=false
	return ready

## 同一人物的不同委托都保留；换会话或读档后旧页码与取消确认失效。
static func conversation(main, parent: VBoxContainer, interaction: Dictionary) -> void:
	if not main._resident_mode or interaction.get("phase") not in ["say","choice"]: return
	var identity: Dictionary=main._dialogue_identity(interaction)
	var source: Dictionary=main.kernel.source_identity()
	var draft: Dictionary=_draft(main,interaction)
	var view: Dictionary=main.kernel.demand_view("interaction_tasks",{"cursor":draft.cursor})
	if not view.get("ok",false):
		if view.get("code")=="NativeAsyncRequired": main._text(parent,"正在核对眼前可办的委托…",15)
		elif view.get("code") not in ["NoConversation","RoadServiceDenied","ActorBusy"]: main._text(parent,"委托暂不能查看："+_reason(main,view.get("code","Unknown")),15)
		return
	if view.get("rows",[]).is_empty() and not view.has("next_cursor"): return
	var box=VBoxContainer.new(); box.name="InteractionTasks"; parent.add_child(box)
	main._button(box,"收起委托条款" if draft.get("expanded",false) else "核对这次会面的委托","interaction_tasks_toggle","",_toggle.bind(main,source,identity,main._scene_counter),main._advancing)
	if not draft.get("expanded",false): return
	var scroll=ScrollContainer.new(); scroll.name="InteractionTasksScroll"; scroll.custom_minimum_size.y=220; scroll.horizontal_scroll_mode=ScrollContainer.SCROLL_MODE_DISABLED; scroll.follow_focus=true; box.add_child(scroll)
	var entries=VBoxContainer.new(); entries.size_flags_horizontal=Control.SIZE_EXPAND_FILL; scroll.add_child(entries)
	for row in view.get("rows",[]):
		var item=VBoxContainer.new(); item.name="InteractionTask"; item.set_meta("task_row",row.duplicate(true)); entries.add_child(item)
		main._text(item,row.display_name+" · "+row.status_text,17)
		main._text(item,row.source_text+"\n"+row.objective_text+"\n回报："+row.get("report_text","依原约定"),15)
		if not row.get("response_text","").is_empty(): main._text(item,row.response_text,15)
		if not row.get("progress_text","").is_empty(): main._text(item,row.progress_text,15)
		if row.has("deadline_tick"):
			main._text(item,_deadline_text(row),15)
		elif not row.get("result_text","").is_empty():
			main._text(item,row.result_text,15)
		else:
			main._text(item,"受领办完后起算%d个半日；经费%d钱，约定报酬%d钱。"%[row.deadline_ticks,row.budget,row.reward],15)
		for action in row.get("actions",[]):
			var quote: Dictionary=main.kernel.preview(action.command,action.params,view.actor_id)
			var enabled: bool=action.get("available",false) and quote.get("ok",false)
			var label: String=action.text
			if action.command=="cancel_task" and draft.cancel_id==row.id:
				main._text(item,"确认放弃后，余款与受托货物按原合同清算；已经发生的花费不会退回，未办妥部分不能领取成功酬劳。",15)
				label="确认放弃并清算"
			var button: Button=main._button(item,label,action.command,row.id,func(): pass,not enabled or main._advancing,false)
			button.name="InteractionTaskAction"; button.set_meta("task_action",action.duplicate(true))
			button.pressed.connect(_request.bind(main,button,row.id,action,source,identity,main._scene_counter,view.actor_id))
			if enabled:
				main._text(item,"本次办理耗时%d个半日"%quote.get("duration",0),14)
			else:
				var code: String=quote.get("code",action.get("code","Unknown"))
				main._text(item,"正在核对条件…" if code=="NativeAsyncRequired" else "此时不能办理："+_reason(main,code),14)
	if draft.cursor>0: main._button(box,"返回首批委托","interaction_tasks_page","",_page.bind(main,0,source,identity,main._scene_counter),main._advancing)
	if view.has("next_cursor"): main._button(box,"查看更多已知委托","interaction_tasks_page","",_page.bind(main,view.next_cursor,source,identity,main._scene_counter),main._advancing)

## 只解释本域明确的规则拒绝；原code与报价不改，其它原因仍交原Main翻译口。
static func _reason(main, code: String) -> String:
	if code=="MessageNotDelivered": return "信件尚未亲手交给约定收信人"
	if code=="StaleTaskConversation": return "这次会面的办理条件已经变化，请重新核对"
	return main._reason(code)

## 原合同截止的对象由任务域提供；不以抵达或持物替代交付/回报，也不推算新的宽限。
static func _deadline_text(row: Dictionary) -> String:
	var purpose: String="原约定办妥截止"
	if row.get("deadline_scope","")=="delivery_completion": purpose="原约定完整交付截止"
	elif row.get("deadline_scope","")=="report_completion": purpose="原约定回报截止"
	var inclusive: String="（含截止半日）" if row.get("deadline_inclusive",false) else ""
	return "%s：第%d半日%s。%s"%[purpose,row.deadline_tick,inclusive,"原期限已过，请核对当前补交或清算条件。" if row.get("overdue",false) else "须留出本次报价所需的办理时间。"]

## 普通对白默认保留阅读空间，展开条款才显示有界滚动区，不推动世界。
static func _toggle(main, source: Dictionary, dialogue: Dictionary, epoch: int) -> void:
	if not is_instance_valid(main) or main.is_queued_for_deletion() or not main.is_inside_tree(): return
	if main._advancing or main._scene_counter!=epoch or main.kernel.source_identity()!=source or main._dialogue_identity(main.kernel.interaction_view())!=dialogue: return
	var draft: Dictionary=main.get_meta("interaction_tasks_draft",{}).duplicate(true)
	draft.expanded=not draft.get("expanded",false); draft.cancel_id=""
	main.set_meta("interaction_tasks_draft",draft); main._render()

## 翻页只改显示选择，世界版本或说话人改变时不沿用旧游标。
static func _page(main, cursor: int, source: Dictionary, dialogue: Dictionary, epoch: int) -> void:
	if not is_instance_valid(main) or main.is_queued_for_deletion() or not main.is_inside_tree(): return
	if main._advancing or main._scene_counter!=epoch or main.kernel.source_identity()!=source or main._dialogue_identity(main.kernel.interaction_view())!=dialogue: return
	main.set_meta("interaction_tasks_draft",{"source":source.duplicate(true),"dialogue":dialogue.duplicate(true),"cursor":cursor,"cancel_id":"","expanded":true})
	main._render()

## 取消需在同源条款下再点一次；任何办理均重新报价并通过原具名命令提交。
static func _request(main, button: Variant, row_id: String, action: Dictionary, source: Dictionary, dialogue: Dictionary, epoch: int, actor_id: String) -> Dictionary:
	if not is_instance_valid(main) or main.is_queued_for_deletion() or not main.is_inside_tree(): return {"ok":false,"code":"StaleSceneRequest"}
	if not is_instance_valid(button) or button.is_queued_for_deletion() or not button.is_inside_tree() or not button.is_visible_in_tree() or button.disabled: return {"ok":false,"code":"StaleSceneRequest"}
	if main._advancing or not main._storage_intent.is_empty() or main._scene_counter!=epoch or main.kernel.source_identity()!=source: return {"ok":false,"code":"StalePreview"}
	if main._dialogue_identity(main.kernel.interaction_view())!=dialogue: return {"ok":false,"code":"StaleDialogueChoice"}
	var context: Dictionary=main.kernel.command_context()
	if not context.get("ok",false) or context.get("actor_id")!=actor_id or context.get("revision")!=source.get("revision"): return {"ok":false,"code":"StalePreview"}
	var quote: Dictionary=main.kernel.preview(action.command,action.params,actor_id)
	if not quote.get("ok",false):
		button.disabled=true; main._message="此时不能办理："+_reason(main,quote.get("code","Unknown")); main._render(); return quote
	if quote.get("revision")!=source.revision: return {"ok":false,"code":"StalePreview"}
	if action.command=="cancel_task":
		var draft: Dictionary=main.get_meta("interaction_tasks_draft",{}).duplicate(true)
		if draft.get("cancel_id","")!=row_id:
			draft.cancel_id=row_id; main.set_meta("interaction_tasks_draft",draft); main._render(); return {"ok":true,"confirmation_pending":true}
	button.disabled=true
	return main._resident_submit_frozen({"kind":action.command,"params":action.params.duplicate(true),"actor_id":actor_id,"request_seq":context.next_request_seq,"expected_revision":source.revision})
