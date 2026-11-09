## 军中专项签受只消费有限原生页；受命与真实换帅分别提交，不在界面改军籍或军资。
extends RefCounted

## 准备阶段只读页面和可见动作报价，异步未齐时不提交任何命令。
static func prepare(host) -> bool:
	var kernel=host.kernel
	if not kernel.has_method("political_field_appointments_enabled") or not kernel.political_field_appointments_enabled(): return true
	var page: Dictionary=kernel.political_field_appointments(int(host.get_meta("field_appointment_cursor",0)))
	if page.get("_pending",false) or page.get("code","")=="NativeAsyncRequired": return false
	if not page.get("ok",true): return true
	var ready=true
	var context: Dictionary=kernel.command_context()
	if context.get("_pending",false) or context.get("code","")=="NativeAsyncRequired": return false
	for row in page.get("rows",[]):
		for action in row.get("actions",[]):
			if not action.get("available",false): continue
			var quote: Dictionary=kernel.preview(action.command,action.params,context.get("actor_id",""))
			if quote.get("_pending",false) or quote.get("code","")=="NativeAsyncRequired": ready=false
	return ready

## 旧world、旧人物、旧revision页完全不展示；按钮仍沿Main原冻结报价和再次核权路径。
static func render(host, parent: Control, page: Dictionary) -> void:
	host._text(parent,"军中专项任命",23)
	if page.get("_pending",false) or page.get("code","")=="NativeAsyncRequired":
		host._text(parent,"正在核对当前军令…",17);return
	if not page.get("ok",true):
		host._text(parent,reason(page.get("code","PoliticalFieldProfileRequired")),17);return
	var identity: Dictionary=host.kernel.source_identity()
	var context: Dictionary=host.kernel.command_context()
	if page.get("identity",{})!=identity or page.get("actor_id","")!=context.get("actor_id",""):
		host._text(parent,"军令来源已变化，请等当前页面刷新。",17);return
	host._text(parent,"接掌部队与接掌军团分两次专项军令。只有本人受命并完成真实交接后，指挥归属才改变。",16)
	for row in page.get("rows",[]):
		host._text(parent,row.get("display_name",""),20)
		host._text(parent,row.get("text",""),17)
		for action in row.get("actions",[]):
			var available: bool=action.get("available",false)
			host._regional_button(parent,action.get("text","办理"),action.get("command",""),row.get("id",""),action.get("params",{}),{},not available)
			if not available: host._text(parent,reason(action.get("code","")),16)
	if page.get("rows",[]).is_empty(): host._text(parent,"没有本人已知的野外任命或当前可签发目标。",17)
	var next: int=page.get("next_cursor",-1)
	if next>=0: host._button(parent,"后续军令","field_page","next",_page.bind(host,identity.duplicate(true),next))
	if page.get("cursor",0)>0: host._button(parent,"最近军令","field_page","first",_page.bind(host,identity.duplicate(true),0))

## 翻页只在原world来源内更改本地游标，换人或冷恢复后的旧控件不再影响新页。
static func _page(host, identity: Dictionary, cursor: int) -> void:
	if host.kernel.source_identity()!=identity: return
	host.set_meta("field_appointment_cursor",cursor);host._render()

## 未知错误保原码，已知拒绝指出真实军令、现场与期限约束。
static func reason(code: String) -> String:
	return {"PoliticalFieldAnswerExpired":"答复期限已过，需要原有权者另行签发。","PoliticalFieldArrivalExpired":"本次交接期限已过，不能沿用旧受命。","PoliticalFieldSpecificGrantRequired":"现有公职没有本类野外专项任命权。","PoliticalFieldCandidateNotCaptain":"接掌整军前，此人须先实际接掌本军一部。","PoliticalFieldCommandSourceChanged":"原指挥或形成来源已变化，请重新核对。","PoliticalFieldContactMismatch":"双方须在同军实际可接触；舟上还须同一真实载船。","PoliticalFieldActorBusy":"本人仍在行动或交战，须先结束原事务。","PoliticalFieldAppointmentPending":"此目标已有仍有效的受命流程。","MilitaryFieldAppointmentReadRequired":"尚不能取得完整的专项签受权源。"}.get(code,"暂不能办理："+code)
