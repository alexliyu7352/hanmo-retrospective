## 下属补任的有限首府办理页；只提交原生已报价命令，不把提名或答允当作就任。
extends RefCounted

## 原生页及各具名报价未齐时只等待，按钮仍由本人明确提交。
static func prepare(host, place_id: String) -> bool:
	var kernel=host.kernel
	if not kernel.has_method("political_subordinate_appointments_enabled") or not kernel.political_subordinate_appointments_enabled(): return true
	var page: Dictionary=kernel.political_subordinate_appointments(place_id,int(host.get_meta("political_subordinate_appointment_cursor",0)))
	if _pending(page): return false
	if not page.get("ok",false): return true
	var context: Dictionary=kernel.command_context()
	if _pending(context): return false
	if not _current(kernel,page,context,place_id): return true
	var ready=true
	for row in page.get("rows",[]):
		for action in row.get("actions",[]):
			if not action.get("available",false): continue
			var quote: Dictionary=kernel.preview(action.get("command",""),action.get("params",{}),context.get("actor_id",""))
			if _pending(quote): ready=false
	return ready

## 页只显示本人实际参与的提名与实际本人答复；身份或地点变化即拒绝旧控件。
static func render(host, parent: Control, page: Dictionary, place_id: String) -> void:
	if _pending(page): host._text(parent,"正在核对原任期与补任来源…",17);return
	if not page.get("ok",false): host._text(parent,reason(page.get("code","")),17);return
	var context: Dictionary=host.kernel.command_context()
	if _pending(context) or not _current(host.kernel,page,context,place_id): host._text(parent,"交接来源已变化，请刷新。",17);return
	host._text(parent,"本人当面补任",22)
	host._text(parent,"原首长当面提名，本人答允后仍须到原座所复核就任。原公库、任务和债权各循原账。",16)
	for row in page.get("rows",[]):
		host._text(parent,str(row.get("office_id","")),18)
		if row.get("kind","")=="known_offer": host._text(parent,"当面答复须在半日 "+str(row.get("response_until",0))+" 之前；目标座所："+str(row.get("target_place_id","")),16)
		if row.get("kind","") in ["own_answer","witnessed_acceptance"]: host._text(parent,"本人已答允，须在半日 "+str(row.get("arrival_until",0))+" 之前到原座所办理。" if row.get("accepted",false) else "本人已谢绝此次提名。",16)
		if row.get("kind","")=="own_arrival": host._text(parent,"本人已凭原职权就任，任期版本："+str(row.get("installed_epoch",0)),17)
		if row.has("source_fact_id"): host._text(parent,"本人原办理凭据："+str(row.source_fact_id),16)
		for action in row.get("actions",[]):
			host._regional_button(parent,str(action.get("text","办理")),str(action.get("command","")),str(row.get("policy_id","")),action.get("params",{}),{},not action.get("available",false))
			if not action.get("available",false): host._text(parent,reason(action.get("code","")),16)
	if page.has("next_cursor"): host._button(parent,"后续本人来源","political_subordinate_appointment_page","next",_page.bind(host,page.source_identity.duplicate(true),context.get("actor_id",""),place_id,int(page.next_cursor)))
	if int(page.get("cursor",0))>0: host._button(parent,"最前来源","political_subordinate_appointment_page","first",_page.bind(host,page.source_identity.duplicate(true),context.get("actor_id",""),place_id,0))

## 游标仅改变本地纯查询状态，不能借旧页跨读档、换人或地点。
static func _page(host, identity: Dictionary, actor: String, place: String, cursor: int) -> void:
	if host.kernel.source_identity()!=identity: return
	var context: Dictionary=host.kernel.command_context()
	if _pending(context) or context.get("actor_id","")!=actor: return
	var page: Dictionary=host.kernel.political_subordinate_appointments(place,int(host.get_meta("political_subordinate_appointment_cursor",0)))
	if _pending(page) or not _current(host.kernel,page,context,place): return
	host.set_meta("political_subordinate_appointment_cursor",cursor);host._render()

static func _current(kernel, page: Dictionary, context: Dictionary, place: String) -> bool:
	return page.get("source_identity",{})==kernel.source_identity() and page.get("observer_actor_id","")==context.get("actor_id","") and page.get("place_id","")==place

static func _pending(value: Dictionary) -> bool:
	return value.get("_pending",false) or value.get("code","")=="NativeAsyncRequired"

## 显示原生拒绝来源，不把答允解释为已经到任。
static func reason(code: String) -> String:
	return {"PoliticalSubordinateAppointmentUnavailable":"当前不能办理，请当面核原首职、空缺、本人意愿和期限。","PoliticalSubordinateAppointmentPolicyContinuityRequired":"原辖区已易手，须独立连续性制度。","PoliticalSubordinateLocalQueryRequired":"须本人到原首府或目标座所办理。","PoliticalSubordinateMeritHistoryUnavailable":"现有来源不能证明此项正功勋门槛。"}.get(code,"暂不能办理："+code)
