## 所选据点只读本人受领条款；到城、完成目标与最终回报分开显示。
extends RefCounted
const META: StringName=&"map_route_obligations"

## 原野点不推断合同目的地，所选城必须仍在当前已装作者目录。
static func _source(host) -> Dictionary:
	var selected: String=host.journey_view.selected
	if selected.is_empty() or not host.journey_view.selected_point.is_empty() or not host._data.settlements.has(selected):return {}
	return {"identity":host.kernel.source_identity(),"actor_id":host.kernel.command_context().get("actor_id",""),"destination_settlement_id":selected}

## 原页面读屏障逐页准备；切换城邑或世界身份立即丢弃旧游标和旧行。
static func prepare(host) -> bool:
	if host.get("_resident_mode")!=true or not host.kernel.has_method("demand_view"):return true
	var source: Dictionary=_source(host)
	if source.is_empty():host.remove_meta(META);return true
	var draft: Dictionary=host.get_meta(META,{})
	if draft.get("source")!=source:draft={"source":source,"cursor":{},"page":{}}
	host.set_meta(META,draft)
	var args: Dictionary={"destination_settlement_id":source.destination_settlement_id}
	if not draft.cursor.is_empty():args.cursor=draft.cursor.duplicate(true)
	var page: Dictionary=host.kernel.demand_view("task_route_obligations",args)
	if page.get("code")=="NativeAsyncRequired":return false
	if page.get("ok",false):
		var checked: Dictionary=_validate(page,source)
		if not checked.ok:page=checked
	draft.page=page.duplicate(true);host.set_meta(META,draft);return true

## 文案取原任务及原场所名称；身份和有限文本保持所属域边界。
static func _text(value: Variant, required: bool=true) -> bool:
	return value is String and value.to_utf8_buffer().size()<=4096 and (not required or not value.is_empty())

## 完整身份、目的城和封闭目标语义缺一不可；不把坏来源当成没有事务。
static func _validate(page: Dictionary,source: Dictionary) -> Dictionary:
	if page.get("ok")!=true or page.get("version")!=1 or page.get("identity")!=source.identity or page.get("observer_actor_id")!=source.actor_id or page.get("destination_settlement_id")!=source.destination_settlement_id:return {"ok":false,"code":"StaleTaskRouteObligations"}
	if not page.get("cursor") is int or page.cursor<0 or page.cursor>9223372036854775551 or not page.get("scanned") is int or page.scanned<0 or page.scanned>256 or not page.get("rows") is Array or page.rows.size()>64 or page.rows.size()>page.scanned:return {"ok":false,"code":"InvalidTaskRouteObligations"}
	var seen: Dictionary={}
	for row in page.rows:
		if not row is Dictionary:return {"ok":false,"code":"InvalidTaskRouteObligation"}
		for key in ["task_id","place_id","place_name","display_name"]:
			if not _text(row.get(key),key not in ["display_name","place_name"]):return {"ok":false,"code":"InvalidTaskRouteObligation"}
		if row.get("template_id") not in ["procurement","message","escort"] or row.get("task_status") not in ["active","ready"] or row.get("target_kind") not in ["report","escort_arrival"] or not row.get("deadline_tick") is int or row.deadline_tick<0 or not row.get("deadline_inclusive") is bool or not row.deadline_inclusive or not row.get("minimum_handover_ticks") is int or row.minimum_handover_ticks!=1 or not row.get("requires_prior_objective") is bool:return {"ok":false,"code":"InvalidTaskRouteObligation"}
		if row.get("deadline_scope")!=("delivery_completion" if row.template_id=="procurement" else "report_completion") or (row.target_kind=="escort_arrival" and row.template_id!="escort"):return {"ok":false,"code":"InvalidTaskRouteObligation"}
		var key: String=row.task_id+"/"+row.target_kind
		if seen.has(key):return {"ok":false,"code":"InvalidTaskRouteObligation"}
		seen[key]=true
	if page.has("next_cursor"):
		var cursor: Variant=page.next_cursor
		if not cursor is Dictionary or cursor.get("identity")!=source.identity or cursor.get("observer_actor_id")!=source.actor_id or cursor.get("destination_settlement_id")!=source.destination_settlement_id or not cursor.get("offset") is int or cursor.offset!=page.cursor+page.scanned or cursor.offset<=page.cursor:return {"ok":false,"code":"InvalidTaskRouteCursor"}
	return {"ok":true}

## 剩余半日只反映合同原截止；路径报价不能许诺目标完成、回报成功或奖励。
static func lines(row: Dictionary,source: Dictionary,quote: Dictionary) -> PackedStringArray:
	var text: PackedStringArray=[row.display_name if not row.display_name.is_empty() else "受领事务"]
	var place: String=row.place_name if not row.place_name.is_empty() else row.place_id
	if row.target_kind=="escort_arrival":text.append("护送至 "+place+"；到达后仍须回报")
	else:text.append(("交付地点 · " if row.deadline_scope=="delivery_completion" else "回报地点 · ")+place)
	var left: int=row.deadline_tick-source.identity.tick
	text.append(("交付完成截止" if row.deadline_scope=="delivery_completion" else "回报完成截止")+" · "+("还剩%d个半日（含截止半日）"%left if left>=0 else "原截止已过%d个半日"%(-left)))
	if row.requires_prior_objective and row.target_kind=="report":text.append("须先办妥原目标，再到此处回报")
	if quote.get("ok",false) and quote.get("revision")==source.identity.revision and quote.get("duration") is int and quote.duration>=0 and quote.duration<=100000:
		if left<quote.duration:text.append("按当前报价，到城时已超过原截止")
		else:
			var after_arrival: int=left-quote.duration
			text.append("按当前报价，到城后至原截止剩%d个半日"%after_arrival)
			if row.target_kind=="report" and after_arrival<row.minimum_handover_ticks:text.append("余时不足最低办理耗时")
	text.append("办理至少%d个半日；另计城内移动、等待及未完成事项"%row.minimum_handover_ticks)
	return text

## 翻页保原游标，不计算下一页；释放、换城或换来源后的按钮均失效。
static func _page(host_ref: WeakRef,source: Dictionary,cursor: Dictionary,guard: Callable) -> void:
	var host=host_ref.get_ref()
	if host==null or not guard.is_valid() or not guard.call() or _source(host)!=source:return
	var draft: Dictionary=host.get_meta(META,{})
	if draft.get("source")!=source:return
	draft.cursor=cursor.duplicate(true);draft.page={};host.set_meta(META,draft);host._render()

## 旧树退出不可清掉新页面草稿；关闭地图后本代页立即失效。
static func _exit(host_ref: WeakRef,source: Dictionary,scene_epoch: int) -> void:
	var host=host_ref.get_ref()
	if host==null:return
	var draft: Dictionary=host.get_meta(META,{})
	if not host.journey_view.opened and draft.get("source")==source and draft.get("scene_epoch")==scene_epoch:host.remove_meta(META)

## 路线侧栏仅消费屏障已准备的有限页，不在绘制阶段发起原生读取。
static func append(host,parent: Control,quote: Dictionary,guard: Callable) -> void:
	if host.get("_resident_mode")!=true:return
	var draft: Dictionary=host.get_meta(META,{})
	if draft.is_empty() or draft.get("source")!=_source(host) or draft.get("page",{}).is_empty():return
	draft.scene_epoch=host._scene_counter;host.set_meta(META,draft)
	parent.tree_exiting.connect(_exit.bind(weakref(host),draft.source,draft.scene_epoch))
	var page: Dictionary=draft.page
	host._text(parent,"此城相关委托",20)
	if not page.get("ok",false):
		var unavailable: Label=host._text(parent,"委托期限资料暂不可用，请重新选择地点。",16)
		unavailable.set_meta("map_route_error",page.get("code","InvalidTaskRouteObligations"));return
	for row in page.rows:
		var label: Label=host._text(parent,"\n".join(lines(row,draft.source,quote)),16)
		label.set_meta("map_route_obligation",row.duplicate(true))
	if page.rows.is_empty():host._text(parent,"本页没有与此城相关的已受领事务。",16)
	var buttons=HFlowContainer.new();parent.add_child(buttons)
	if not draft.cursor.is_empty():host._button(buttons,"从头查看","map_obligations_first","",_page.bind(weakref(host),draft.source,{},guard))
	if page.has("next_cursor"):host._button(buttons,"继续查看","map_obligations_next","",_page.bind(weakref(host),draft.source,page.next_cursor,guard))
