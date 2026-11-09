## 自动继续停在本人决定时的只读提示；仅展示核心已给出的有限来源，不代办或建立第二份待办状态。
extends RefCounted

## 同一当前行动的停止理由才可展开；旧来源或缺失有限回复时不借静态目录猜邀请与地点。
static func render(host, parent: Control, context: Dictionary, state: Dictionary) -> bool:
	if state.get("stop")!="awaiting_input" or state.get("actor_id")!=context.get("actor_id") or state.get("action_id")!=context.get("action_id"): return false
	var source: Dictionary=host.kernel.source_identity()
	if state.get("source_identity",{})!=source: return false
	var view: Dictionary=host.kernel.continuation_state()
	if not view.get("ok",false) or view.get("source_identity",{})!=source: return false
	var rows: Array=visible_invitations(view,context.get("actor_id",""),source.get("tick",-1))
	if rows.is_empty(): return false
	var box=VBoxContainer.new(); box.name="ActivityPendingDecisions"; parent.add_child(box)
	host._text(box,"有本人收到的任职邀请尚待答复",17)
	for row in rows:
		var invitation=VBoxContainer.new(); invitation.name="ActivityMembershipInvitation"; box.add_child(invitation)
		invitation.set_meta("decision_id",row.id); invitation.set_meta("decision_source_fact_id",row.source_fact_id)
		host._text(invitation,"邀请人："+row.display_name,16).name="ActivityDecisionIssuer"
		var place_name: String=row.known_place_name if row.get("known_place_name") is String else ""
		var known_place: bool=row.get("known_place_id") is String and not row.known_place_id.is_empty() and not place_name.is_empty()
		host._text(invitation,"原邀请办理地点："+place_name if known_place else "办理地点尚不清楚，请从原场所人物事项核实。",15).name="ActivityDecisionPlace"
		host._text(invitation,"邀请在半日%d到期，请在到期前本人答复；仍须核验当前条款与在场条件。"%row.expires_tick,15).name="ActivityDecisionExpiry"
		var path: String="通过原通路到原邀请办理地点，在当地人物事项中查看邀请人是否在场，核对工资、职责与离开条款，再亲自选择接受、婉拒或暂缓。" if known_place else "找到原邀请人后，在当地人物事项里核对工资、职责与离开条款，再亲自选择接受、婉拒或暂缓。"
		if context.get("action_kind")=="study_skill":
			path="先使用本页原「暂停课程，保留进度与预留款」操作。"+path+"办妥后返回原授课地点，使用「恢复原课程」，再确认继续预算。"
		host._text(invitation,path,15).name="ActivityDecisionPath"
	return true

## 只收本人未过期的已知未决邀请；异常、他人或已终结的行不会因提示层而扩大可见范围。
static func visible_invitations(view: Dictionary, actor_id: String, tick: int) -> Array:
	var result: Array=[]
	if actor_id.is_empty() or tick<0 or not view.get("pending_decisions") is Array: return result
	for value in view.pending_decisions:
		if not value is Dictionary: continue
		var row: Dictionary=value
		if row.get("kind")!="membership_invitation" or row.get("actor_id")!=actor_id or row.get("status")!="pending": continue
		if not row.get("id") is String or row.id.is_empty() or not row.get("source_fact_id") is String or row.source_fact_id.is_empty(): continue
		if not row.get("issuer_actor_id") is String or row.issuer_actor_id.is_empty() or not row.get("display_name") is String or row.display_name.is_empty(): continue
		if typeof(row.get("expires_tick"))!=TYPE_INT or row.expires_tick<=tick: continue
		result.append(row.duplicate(true))
	return result
