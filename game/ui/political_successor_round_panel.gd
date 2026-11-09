## 原首职继位的有限本地办理页；只提交原生已报价命令，不自行签字或继承资产。
extends RefCounted

## 原生页及各具名报价未齐时只等待，没有自动提名或自动接受。
static func prepare(host, place_id: String) -> bool:
	var kernel=host.kernel
	if not kernel.has_method("political_successor_rounds_enabled") or not kernel.political_successor_rounds_enabled(): return true
	var page: Dictionary=kernel.political_successor_rounds(place_id,int(host.get_meta("political_successor_round_cursor",0)))
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

## 页只显示本人已知的新轮意愿与实际就任；身份或地点变化即拒绝旧控件。
static func render(host, parent: Control, page: Dictionary, place_id: String) -> void:
	if _pending(page): host._text(parent,"正在核对原首职与本人来源…",17);return
	if not page.get("ok",false): host._text(parent,reason(page.get("code","")),17);return
	var context: Dictionary=host.kernel.command_context()
	if _pending(context) or not _current(host.kernel,page,context,place_id): host._text(parent,"本任来源已变化，请刷新。",17);return
	host._text(parent,"本任首长的下一轮继位",22)
	host._text(parent,"真实已就任的首长另行提名下一任。本人接受仅是未来意愿；原首长真实死亡、本人收到文书并形成原空职后，才能另行办理就任。",16)
	for row in page.get("rows",[]):
		host._text(parent,str(row.get("policy_id",""))+" · "+str(row.get("candidate_actor_id","")),18)
		if row.has("nomination_fact_id"): host._text(parent,"原新轮提名："+str(row.nomination_fact_id),16)
		for answer in row.get("known_answers",[]): host._text(parent,"本人已接受未来任职意愿" if answer.get("accepted",false) else "本人已谢绝或撤回未来意愿",16)
		if row.has("own_withdrawal_fact_id"): host._text(parent,"本人已撤回该次新轮提名。",16)
		if row.get("formally_acceded",false): host._text(parent,"本人已依原空职正式就任。",16)
		if row.has("known_notice_binding_fact_id"): host._text(parent,"本人已收死亡文书："+str(row.known_notice_binding_fact_id),16)
		for action in row.get("actions",[]):
			host._regional_button(parent,str(action.get("text","办理")),str(action.get("command","")),str(row.get("nomination_fact_id",row.get("policy_id",""))),action.get("params",{}),{},not action.get("available",false))
			if not action.get("available",false): host._text(parent,reason(action.get("code","")),16)
	if page.has("next_cursor"): host._button(parent,"后续本人来源","political_successor_round_page","next",_page.bind(host,page.source_identity.duplicate(true),context.get("actor_id",""),place_id,int(page.next_cursor)))
	if int(page.get("cursor",0))>0: host._button(parent,"最前来源","political_successor_round_page","first",_page.bind(host,page.source_identity.duplicate(true),context.get("actor_id",""),place_id,0))

## 游标仅改变本地纯查询状态，不能借旧页跨读档、换人或地点。
static func _page(host, identity: Dictionary, actor: String, place: String, cursor: int) -> void:
	if host.kernel.source_identity()!=identity: return
	var context: Dictionary=host.kernel.command_context()
	if _pending(context) or context.get("actor_id","")!=actor: return
	var page: Dictionary=host.kernel.political_successor_rounds(place,int(host.get_meta("political_successor_round_cursor",0)))
	if _pending(page) or not _current(host.kernel,page,context,place): return
	host.set_meta("political_successor_round_cursor",cursor);host._render()

static func _current(kernel, page: Dictionary, context: Dictionary, place: String) -> bool:
	return page.get("source_identity",{})==kernel.source_identity() and page.get("observer_actor_id","")==context.get("actor_id","") and page.get("place_id","")==place

static func _pending(value: Dictionary) -> bool:
	return value.get("_pending",false) or value.get("code","")=="NativeAsyncRequired"

static func reason(code: String) -> String:
	return {"PoliticalSuccessorRoundPolicyContinuityRequired":"此辖区已经历接管，须另有新轮继位连续性来源。","PoliticalSuccessorRoundDeputyDispositionRequired":"原首职已有副手，须先有独立明确处置。","PoliticalSuccessorRoundNominationExpired":"原新轮提名已过有限答复期限。","PoliticalSuccessorRoundSpecificGrantRequired":"本次专项提名权限当前不成立。","PoliticalSuccessorRoundUnavailable":"当前不能办理这份新轮提名。","PoliticalSuccessorRoundLocalQueryRequired":"须本人到原首府办理。"}.get(code,"暂不能办理："+code)
