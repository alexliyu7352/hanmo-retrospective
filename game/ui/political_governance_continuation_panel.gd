## 原安民任务的明确续办办理页；控件只提交原预览，不修改剩余工时或钱粮。
extends RefCounted

## 预读所有可用报价，异步未齐时不写任何命令。
static func prepare(host, place_id: String) -> bool:
	var kernel=host.kernel
	if not kernel.has_method("political_governance_continuations_enabled") or not kernel.political_governance_continuations_enabled(): return true
	var page: Dictionary=kernel.political_governance_continuations(place_id,int(host.get_meta("governance_continuation_cursor",0)))
	if _pending(page): return false
	if not page.get("ok",false) or not page.get("enabled",false): return true
	var context: Dictionary=kernel.command_context()
	if _pending(context): return false
	if page.get("source_identity",{})!=kernel.source_identity() or page.get("observer_actor_id","")!=context.get("actor_id","") or page.get("place_id","")!=place_id: return true
	var ready=true
	for row in page.get("rows",[]):
		if not row.get("available",false): continue
		var quote: Dictionary=kernel.preview(row.get("command",""),row.get("params",{}),context.get("actor_id",""))
		if _pending(quote): ready=false
	return ready

## 同一身份、本人及实际地点相符才展示动作；原执行人只有进度来源，不冒署中签权。
static func render(host, parent: Control, page: Dictionary, place_id: String) -> void:
	if _pending(page): host._text(parent,"正在核对原安民责任…",17);return
	if not page.get("ok",false): host._text(parent,reason(page.get("code","")),17);return
	if not page.get("enabled",false): return
	var identity: Dictionary=host.kernel.source_identity()
	var context: Dictionary=host.kernel.command_context()
	if _pending(context) or page.get("source_identity",{})!=identity or page.get("observer_actor_id","")!=context.get("actor_id","") or page.get("place_id","")!=place_id:
		host._text(parent,"公职或责任来源已变化，请刷新。",17);return
	host._text(parent,"安民责任续办",22)
	host._text(parent,"保留原执行人、剩余劳动、原钱粮预约与受益人。具名接任署确认后沿原任务继续。",16)
	for row in page.get("rows",[]):
		host._text(parent,str(row.get("task_id","安民任务"))+" · "+str(row.get("executor_actor_id","")),18)
		host._text(parent,"原截止：第 %d 半日"%int(row.get("deadline_tick",0)),16)
		if row.get("continuing",false): host._text(parent,"续办来源已成立，原任务按原半日流程推进。",16)
		elif not str(row.get("continuity_code","")).is_empty(): host._text(parent,reason(row.continuity_code),16)
		if row.has("command"):
			host._regional_button(parent,row.get("label","接续原安民责任"),row.command,row.get("task_id",""),row.get("params",{}),{},not row.get("available",false))
			if not row.get("available",false): host._text(parent,reason(row.get("code","")),16)
	if page.has("next_cursor"): host._button(parent,"后续责任","governance_continuation_page","next",_page.bind(host,identity.duplicate(true),context.get("actor_id",""),place_id,int(page.next_cursor)))
	if int(page.get("cursor",0))>0: host._button(parent,"最前责任","governance_continuation_page","first",_page.bind(host,identity.duplicate(true),context.get("actor_id",""),place_id,0))

## 翻页只改变当前查询游标，旧控件不能跨人、跨地或跨修订调用。
static func _page(host, identity: Dictionary, actor: String, place: String, cursor: int) -> void:
	if host.kernel.source_identity()!=identity: return
	var context: Dictionary=host.kernel.command_context()
	if _pending(context) or context.get("actor_id","")!=actor: return
	var page: Dictionary=host.kernel.political_governance_continuations(place,int(host.get_meta("governance_continuation_cursor",0)))
	if _pending(page) or page.get("source_identity",{})!=identity or page.get("place_id","")!=place: return
	host.set_meta("governance_continuation_cursor",cursor);host._render()

static func _pending(value: Dictionary) -> bool:
	return value.get("_pending",false) or value.get("code","")=="NativeAsyncRequired"

static func reason(code: String) -> String:
	return {"GovernanceOfficeVacant":"原署空缺，须由实际接任署明确续办。", "GovernanceControlOrAssetsChanged":"原辖区或公库已换属，须核当前完整责任裁定。", "GovernanceContinuationAlreadyRecorded":"本次责任决定已有明确续办来源。", "GovernanceSuccessorOfficeVacant":"接任署当前空缺，原劳动暂缓。", "GovernanceContinuationGrantDenied":"缺少覆盖原钱粮与执行人的续办专项权。", "GovernanceContinuationIssuerNotAtSite":"办理人须实际到原治所。", "GovernanceContinuationTakeoverUnavailable":"本次责任尚无完整批准来源。"}.get(code,"暂不能续办："+code)
