## 本人辞任的有限首府办理页；只提交原生已报价命令，不把申请当作离任。
extends RefCounted

## 原生页及各具名报价未齐时只等待，没有自动提名或自动接受。
static func prepare(host, place_id: String) -> bool:
	var kernel=host.kernel
	if not kernel.has_method("political_office_resignations_enabled") or not kernel.political_office_resignations_enabled(): return true
	var page: Dictionary=kernel.political_office_resignations(place_id,int(host.get_meta("political_office_resignation_cursor",0)))
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

## 页只显示本人实际参与的申请与当面答复；身份或地点变化即拒绝旧控件。
static func render(host, parent: Control, page: Dictionary, place_id: String) -> void:
	if _pending(page): host._text(parent,"正在核对原任期与辞任来源…",17);return
	if not page.get("ok",false): host._text(parent,reason(page.get("code","")),17);return
	var context: Dictionary=host.kernel.command_context()
	if _pending(context) or not _current(host.kernel,page,context,place_id): host._text(parent,"交接来源已变化，请刷新。",17);return
	host._text(parent,"本人当面辞任",22)
	host._text(parent,"先由原任本人申请，再由原首长当面批准。原成员、Task、债权和公私资产不因辞任消失。",16)
	for row in page.get("rows",[]):
		host._text(parent,str(row.get("office_id","")),18)
		if row.get("kind","")=="known_request": host._text(parent,"原申请有效至半日 "+str(row.get("response_until",0))+"，须在此之前办理。",16)
		if row.get("kind","")=="witnessed_answer":
			host._text(parent,"原首长当面批准离职。" if row.get("accepted",false) else "原首长当面驳回申请，仍为原任。",17)
			if not str(row.get("reason","")).is_empty(): host._text(parent,reason(row.get("reason","")),16)
		if row.has("source_fact_id"): host._text(parent,"本人原办理凭据："+str(row.source_fact_id),16)
		for action in row.get("actions",[]):
			host._regional_button(parent,str(action.get("text","办理")),str(action.get("command","")),str(row.get("policy_id","")),action.get("params",{}),{},not action.get("available",false))
			if not action.get("available",false): host._text(parent,reason(action.get("code","")),16)
	if page.has("next_cursor"): host._button(parent,"后续本人来源","political_office_resignation_page","next",_page.bind(host,page.source_identity.duplicate(true),context.get("actor_id",""),place_id,int(page.next_cursor)))
	if int(page.get("cursor",0))>0: host._button(parent,"最前来源","political_office_resignation_page","first",_page.bind(host,page.source_identity.duplicate(true),context.get("actor_id",""),place_id,0))

## 游标仅改变本地纯查询状态，不能借旧页跨读档、换人或地点。
static func _page(host, identity: Dictionary, actor: String, place: String, cursor: int) -> void:
	if host.kernel.source_identity()!=identity: return
	var context: Dictionary=host.kernel.command_context()
	if _pending(context) or context.get("actor_id","")!=actor: return
	var page: Dictionary=host.kernel.political_office_resignations(place,int(host.get_meta("political_office_resignation_cursor",0)))
	if _pending(page) or not _current(host.kernel,page,context,place): return
	host.set_meta("political_office_resignation_cursor",cursor);host._render()

static func _current(kernel, page: Dictionary, context: Dictionary, place: String) -> bool:
	return page.get("source_identity",{})==kernel.source_identity() and page.get("observer_actor_id","")==context.get("actor_id","") and page.get("place_id","")==place

static func _pending(value: Dictionary) -> bool:
	return value.get("_pending",false) or value.get("code","")=="NativeAsyncRequired"

## 显示原生拒绝来源，不把申请解释为已经离任。
static func reason(code: String) -> String:
	return {"ServiceTermSatisfied":"原任期已达到作者明示的最低服务期限。","MinimumServiceRemaining":"原任期尚未达到作者明示的最低服务期限。","PoliticalOfficeResignationUnavailable":"当前不能办理此项辞任，请核原任期、本人在场及答复期限。","PoliticalOfficeResignationPolicyContinuityRequired":"原辖区或首府已经易手，须先有独立连续性制度。","PoliticalOfficeResignationLocalQueryRequired":"须本人到原审批首府办理。","PoliticalOfficeResignationClosed":"本申请已经关闭或超过答复期限。"}.get(code,"暂不能办理："+code)
