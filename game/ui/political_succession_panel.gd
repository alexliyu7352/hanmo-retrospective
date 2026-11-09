## 原首职继位的有限本地办理页；只提交原生已报价命令，不自行签字或继承资产。
extends RefCounted

## 原生页及各具名报价未齐时只等待，没有自动提名或自动接受。
static func prepare(host, place_id: String) -> bool:
	var kernel=host.kernel
	if not kernel.has_method("political_successions_enabled") or not kernel.political_successions_enabled(): return true
	var page: Dictionary=kernel.political_successions(place_id,int(host.get_meta("political_succession_cursor",0)))
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

## 页只显示本人已知提名与原先实际交付的文书；身份或地点变化即拒绝旧控件。
static func render(host, parent: Control, page: Dictionary, place_id: String) -> void:
	if _pending(page): host._text(parent,"正在核对原首职与本人来源…",17);return
	if not page.get("ok",false): host._text(parent,reason(page.get("code","")),17);return
	var context: Dictionary=host.kernel.command_context()
	if _pending(context) or not _current(host.kernel,page,context,place_id): host._text(parent,"继位来源已变化，请刷新。",17);return
	host._text(parent,"原首职正式继位",22)
	host._text(parent,"提名与接受仅记录本人意愿。真实空缺及本人已收文书齐备后，才能另办正式就任。",16)
	for row in page.get("rows",[]):
		host._text(parent,str(row.get("policy_id",""))+" · "+str(row.get("candidate_actor_id","")),18)
		if row.has("nomination_fact_id"): host._text(parent,"原提名："+str(row.nomination_fact_id),16)
		for answer in row.get("known_answers",[]): host._text(parent,"本人已接受" if answer.get("accepted",false) else "本人已谢绝或撤回意愿",16)
		if row.get("formally_acceded",false): host._text(parent,"已正式就任原首职。",16)
		if row.has("own_withdrawal_fact_id"): host._text(parent,"本人已撤回原提名。",16)
		for action in row.get("actions",[]):
			host._regional_button(parent,str(action.get("text","办理")),str(action.get("command","")),str(row.get("nomination_fact_id",row.get("policy_id",""))),action.get("params",{}),{},not action.get("available",false))
			if not action.get("available",false): host._text(parent,reason(action.get("code","")),16)
	if page.has("next_cursor"): host._button(parent,"后续本人来源","political_succession_page","next",_page.bind(host,page.source_identity.duplicate(true),context.get("actor_id",""),place_id,int(page.next_cursor)))
	if int(page.get("cursor",0))>0: host._button(parent,"最前来源","political_succession_page","first",_page.bind(host,page.source_identity.duplicate(true),context.get("actor_id",""),place_id,0))

## 游标仅改变本地纯查询状态，不能借旧页跨读档、换人或地点。
static func _page(host, identity: Dictionary, actor: String, place: String, cursor: int) -> void:
	if host.kernel.source_identity()!=identity: return
	var context: Dictionary=host.kernel.command_context()
	if _pending(context) or context.get("actor_id","")!=actor: return
	var page: Dictionary=host.kernel.political_successions(place,int(host.get_meta("political_succession_cursor",0)))
	if _pending(page) or not _current(host.kernel,page,context,place): return
	host.set_meta("political_succession_cursor",cursor);host._render()

static func _current(kernel, page: Dictionary, context: Dictionary, place: String) -> bool:
	return page.get("source_identity",{})==kernel.source_identity() and page.get("observer_actor_id","")==context.get("actor_id","") and page.get("place_id","")==place

static func _pending(value: Dictionary) -> bool:
	return value.get("_pending",false) or value.get("code","")=="NativeAsyncRequired"

static func reason(code: String) -> String:
	return {"PoliticalSuccessionPolicyContinuityRequired":"此辖区已经历接管，须另有继位连续性来源。","PoliticalSuccessionDeputyHandoverRequired":"原首职已有其他交接，不能凭旧提名就任。","PoliticalSuccessionDeathNotKnown":"须先本人实际收到原首任的死亡文书。","PoliticalSuccessionLocalQueryRequired":"须本人到原首府办理。"}.get(code,"暂不能办理："+code)
