## 公共接待岗位的有限本地办理页；只提交原生已报价命令，不自行签字或继承资产。
extends RefCounted

## 原生页及各具名报价未齐时只等待，没有自动提名或自动接受。
static func prepare(host, place_id: String) -> bool:
	var kernel=host.kernel
	if not kernel.has_method("political_office_receptions_enabled") or not kernel.political_office_receptions_enabled(): return true
	var page: Dictionary=kernel.political_office_receptions(place_id,int(host.get_meta("political_office_reception_cursor",0)))
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

## 页只显示本人原接待凭据与当前原柜台；身份或地点变化即拒绝旧控件。
static func render(host, parent: Control, page: Dictionary, place_id: String) -> void:
	if _pending(page): host._text(parent,"正在核对原职位与柜台来源…",17);return
	if not page.get("ok",false): host._text(parent,reason(page.get("code","")),17);return
	var context: Dictionary=host.kernel.command_context()
	if _pending(context) or not _current(host.kernel,page,context,place_id): host._text(parent,"交接来源已变化，请刷新。",17);return
	host._text(parent,"公署接待岗位",22)
	host._text(parent,"现任本人以专项权限接任原柜台。旧文书、私约和资金权源仍逐项核准。",16)
	for row in page.get("rows",[]):
		host._text(parent,str(row.get("role_id","")),18)
		if row.get("kind","")=="current_counter": host._text(parent,"柜台登记："+str(row.get("assigned_actor_id","")),16)
		if row.has("source_fact_id"): host._text(parent,"本人原接任凭据："+str(row.source_fact_id),16)
		for action in row.get("actions",[]):
			host._regional_button(parent,str(action.get("text","办理")),str(action.get("command","")),str(row.get("policy_id","")),action.get("params",{}),{},not action.get("available",false))
			if not action.get("available",false): host._text(parent,reason(action.get("code","")),16)
	if page.has("next_cursor"): host._button(parent,"后续本人来源","political_office_reception_page","next",_page.bind(host,page.source_identity.duplicate(true),context.get("actor_id",""),place_id,int(page.next_cursor)))
	if int(page.get("cursor",0))>0: host._button(parent,"最前来源","political_office_reception_page","first",_page.bind(host,page.source_identity.duplicate(true),context.get("actor_id",""),place_id,0))

## 游标仅改变本地纯查询状态，不能借旧页跨读档、换人或地点。
static func _page(host, identity: Dictionary, actor: String, place: String, cursor: int) -> void:
	if host.kernel.source_identity()!=identity: return
	var context: Dictionary=host.kernel.command_context()
	if _pending(context) or context.get("actor_id","")!=actor: return
	var page: Dictionary=host.kernel.political_office_receptions(place,int(host.get_meta("political_office_reception_cursor",0)))
	if _pending(page) or not _current(host.kernel,page,context,place): return
	host.set_meta("political_office_reception_cursor",cursor);host._render()

static func _current(kernel, page: Dictionary, context: Dictionary, place: String) -> bool:
	return page.get("source_identity",{})==kernel.source_identity() and page.get("observer_actor_id","")==context.get("actor_id","") and page.get("place_id","")==place

static func _pending(value: Dictionary) -> bool:
	return value.get("_pending",false) or value.get("code","")=="NativeAsyncRequired"

## 显示原生拒绝来源，不把岗位认领解释为新增任职或承包权。
static func reason(code: String) -> String:
	return {"PoliticalOfficeReceptionPolicyContinuityRequired":"原辖区已变更，须另有明确接待连续性来源。","PoliticalOfficeReceptionDutyDispositionRequired":"该岗位已有原组织职责，须先独立处置。","PoliticalOfficeReceptionGrantExpired":"接待专项权限当前已过期。","PoliticalOfficeReceptionCurrentOfficeRequired":"须本人仍真实持有原职位及同一版本。","PoliticalOfficeReceptionCounterBusy":"新旧接待人仍有真实会话或原服务占用。","PoliticalOfficeReceptionLocalQueryRequired":"须本人到原公署柜台办理。"}.get(code,"暂不能办理："+code)
