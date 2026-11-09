## 本人原邀请的有限回复页；只读本地原双方来源，按钮沿原生预览与实际提交。
extends RefCounted

## 异步页和报价未齐时只等待，不自动谢绝或撤回。
static func prepare(host, place_id: String) -> bool:
	var kernel=host.kernel
	if not kernel.has_method("governance_invitation_replies_enabled") or not kernel.governance_invitation_replies_enabled(): return true
	var page: Dictionary=kernel.governance_invitation_replies(place_id,int(host.get_meta("governance_invitation_reply_cursor",0)))
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

## 同World身份、本人和实际地点同时相符才展示可办理控件。
static func render(host, parent: Control, page: Dictionary, place_id: String) -> void:
	if _pending(page): host._text(parent,"正在核对原邀请…",17);return
	if not page.get("ok",false): host._text(parent,reason(page.get("code","")),17);return
	if not page.get("enabled",false): return
	var identity: Dictionary=host.kernel.source_identity()
	var context: Dictionary=host.kernel.command_context()
	if _pending(context) or page.get("source_identity",{})!=identity or page.get("observer_actor_id","")!=context.get("actor_id","") or page.get("place_id","")!=place_id:
		host._text(parent,"邀请来源已变化，请刷新。",17);return
	host._text(parent,"安民邀请回复",22)
	host._text(parent,"原双方须当面办理。已受领的责任沿原任务，不在此关闭。",16)
	for row in page.get("rows",[]):
		host._text(parent,str(row.get("authority_fact_id","安民邀请")),18)
		host._text(parent,"签发人：%s · 受邀人：%s"%[str(row.get("issuer_actor_id","")),str(row.get("executor_actor_id",""))],16)
		if row.has("reply_kind"):
			host._text(parent,"已谢绝" if row.reply_kind=="declined" else "已撤回",16)
		elif row.has("command"):
			var label="谢绝原邀请" if row.command=="governance_decline" else "撤回原邀请"
			host._regional_button(parent,label,row.command,row.get("authority_fact_id",""),row.get("params",{}),{},not row.get("available",false))
			if not row.get("available",false): host._text(parent,reason(row.get("code","")),16)
	if page.has("next_cursor"): host._button(parent,"后续邀请","governance_invitation_reply_page","next",_page.bind(host,identity.duplicate(true),context.get("actor_id",""),place_id,int(page.next_cursor)))
	if int(page.get("cursor",0))>0: host._button(parent,"最前邀请","governance_invitation_reply_page","first",_page.bind(host,identity.duplicate(true),context.get("actor_id",""),place_id,0))

## 游标只是纯查询状态，旧控件不能跨换人、读档、修订或地点执行。
static func _page(host, identity: Dictionary, actor: String, place: String, cursor: int) -> void:
	if host.kernel.source_identity()!=identity: return
	var context: Dictionary=host.kernel.command_context()
	if _pending(context) or context.get("actor_id","")!=actor: return
	var page: Dictionary=host.kernel.governance_invitation_replies(place,int(host.get_meta("governance_invitation_reply_cursor",0)))
	if _pending(page) or page.get("source_identity",{})!=identity or page.get("observer_actor_id","")!=actor or page.get("place_id","")!=place: return
	host.set_meta("governance_invitation_reply_cursor",cursor);host._render()

static func _pending(value: Dictionary) -> bool:
	return value.get("_pending",false) or value.get("code","")=="NativeAsyncRequired"

static func reason(code: String) -> String:
	return {"GovernanceInvitationAlreadyAccepted":"原邀请已经受领，请办理原任务。", "GovernanceInvitationAlreadyClosed":"原邀请已有当面回复。", "GovernanceInvitationExpired":"原邀请已过期。", "GovernanceInvitationContactRequired":"原双方须到原治所当面办理。", "GovernanceInvitationWithdrawalGrantRequired":"当前原署缺少明确撤回许可。", "GovernanceInvitationNotKnown":"没有本人可办理的原邀请。"}.get(code,"暂不能办理："+code)
