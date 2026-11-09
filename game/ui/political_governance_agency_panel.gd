## 治理代理有限本人页；四种办理都重新预览原生命、成员、Office和专项Grant。
extends RefCounted

## 原生页及各具名报价未齐时只等待，没有自动提名或自动接受。
static func prepare(host, place_id: String) -> bool:
	var kernel=host.kernel
	if not kernel.has_method("political_governance_agencies_enabled") or not kernel.political_governance_agencies_enabled(): return true
	var page: Dictionary=kernel.political_governance_agencies(place_id,int(host.get_meta("political_governance_agency_cursor",0)))
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

## 页只显示同场可见候选与本人原签受凭据；换人或离场使旧控件失效。
static func render(host, parent: Control, page: Dictionary, place_id: String) -> void:
	if _pending(page): host._text(parent,"正在核对原职位与委任来源…",17);return
	if not page.get("ok",false): host._text(parent,reason(page.get("code","")),17);return
	var context: Dictionary=host.kernel.command_context()
	if _pending(context) or not _current(host.kernel,page,context,place_id): host._text(parent,"委任来源已变化，请刷新。",17);return
	host._text(parent,"治理代理签受",22)
	host._text(parent,"原现任签委任，候选本人答复。代理只签原有限安民模板，既有Task与公钱粮仍按原账办理。",16)
	for row in page.get("rows",[]):
		host._text(parent,str(row.get("policy_id",row.get("kind",""))),18)
		if row.has("candidate_actor_id"): host._text(parent,"同场候选："+str(row.candidate_actor_id),16)
		if row.has("executor_actor_id"): host._text(parent,"原执行人："+str(row.executor_actor_id),16)
		if row.has("source_fact_id"): host._text(parent,"本人原凭据："+str(row.source_fact_id),16)
		if row.has("authority_fact_id"): host._text(parent,"原唯一安民批准："+str(row.authority_fact_id),16)
		if row.has("response_before_tick"): host._text(parent,"答复须早于："+str(row.response_before_tick)+"；委任有效至："+str(row.valid_through_tick),16)
		for action in row.get("actions",[]):
			host._regional_button(parent,str(action.get("text","办理")),str(action.get("command","")),str(row.get("policy_id","")),action.get("params",{}),{},not action.get("available",false))
			if not action.get("available",false): host._text(parent,reason(action.get("code","")),16)
	if page.has("next_cursor"): host._button(parent,"后续本人来源","political_governance_agency_page","next",_page.bind(host,page.source_identity.duplicate(true),context.get("actor_id",""),place_id,int(page.next_cursor)))
	if int(page.get("cursor",0))>0: host._button(parent,"最前来源","political_governance_agency_page","first",_page.bind(host,page.source_identity.duplicate(true),context.get("actor_id",""),place_id,0))

## 游标仅改变本地纯查询状态，不能借旧页跨读档、换人或地点。
static func _page(host, identity: Dictionary, actor: String, place: String, cursor: int) -> void:
	if host.kernel.source_identity()!=identity: return
	var context: Dictionary=host.kernel.command_context()
	if _pending(context) or context.get("actor_id","")!=actor: return
	var page: Dictionary=host.kernel.political_governance_agencies(place,int(host.get_meta("political_governance_agency_cursor",0)))
	if _pending(page) or not _current(host.kernel,page,context,place): return
	host.set_meta("political_governance_agency_cursor",cursor);host._render()

static func _current(kernel, page: Dictionary, context: Dictionary, place: String) -> bool:
	return page.get("source_identity",{})==kernel.source_identity() and page.get("observer_actor_id","")==context.get("actor_id","") and page.get("place_id","")==place

static func _pending(value: Dictionary) -> bool:
	return value.get("_pending",false) or value.get("code","")=="NativeAsyncRequired"

## 仅显示原生有界原因，不借旧委任页探测另一人的远方私下状态。
static func reason(code: String) -> String:
	return {"PoliticalGovernanceAgencyUnavailable":"当前不能办理；须本人、原权源、期限和真实同场条件全部成立。","PoliticalGovernanceAgencyLocalQueryRequired":"须本人到原公署查看和办理。","PoliticalGovernanceAgencyProfileRequired":"此局未启用治理代理。"}.get(code,"暂不能办理："+code)
