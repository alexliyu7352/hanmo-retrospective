## 公职事项留在当前场所；界面只消费原生有限DTO和正式报价，不写任命、位置或公库。
extends RefCounted

## 仅本局显式政治能力开放入口，旧档与未接原生能力的场景不伪造公务。
static func local_affairs(main, parent: VBoxContainer) -> void:
	if not main._resident_mode or not main._started: return
	if not main.kernel.has_method("politics_enabled") or not main.kernel.politics_enabled(): return
	var location: Dictionary=main.kernel.presentation_location()
	var view: Dictionary=main.kernel.politics_entries(location.get("place_id",""))
	if not view.get("ok",false):
		main._text(parent,"此时无法办理公务："+main._reason(view.get("code","Unknown")),15)
		return
	_rows(main,parent,view)
	if main.kernel.has_method("political_fiscal_enabled") and main.kernel.political_fiscal_enabled():
		var fiscal: Dictionary=main.kernel.political_fiscal_entries(location.get("place_id",""))
		if fiscal.get("ok",false): _rows(main,parent,fiscal)
	if main.kernel.has_method("political_occupation_fiscal_enabled") and main.kernel.political_occupation_fiscal_enabled():
		var occupation_fiscal: Dictionary=main.kernel.political_occupation_fiscal_entries(location.get("place_id",""))
		if occupation_fiscal.get("ok",false): _rows(main,parent,occupation_fiscal)
		else: main._text(parent,"此时无法查阅新税政："+_occupation_fiscal_reason(main,occupation_fiscal.get("code","Unknown")),15)
	if main.kernel.has_method("politics_governance_enabled") and main.kernel.politics_governance_enabled():
		var governance: Dictionary=main.kernel.politics_governance(location.get("place_id",""))
		if governance.get("ok",false): _rows(main,parent,governance)
		else: main._text(parent,"此时无法查阅安民责任："+_governance_reason(main,governance.get("code","Unknown")),15)

## 新签税政只影响后续税期，拒绝原因来自同一原生报价。
static func _occupation_fiscal_reason(main, code: String) -> String:
	match code:
		"OccupationFiscalPolicyRequired", "LegacyFiscalPolicyAfterOccupation": return "接管后须由当前有权者重新签发税政。"
		"OccupationFiscalGrantIncomplete", "OccupationFiscalGrantMismatch": return "当前官职未获完整的新税政公库授权。"
		"PoliticalTakeoverControlChanged", "PoliticalControlDenied": return "辖区控制已改变，不能再以原任期签发。"
		"StaleOccupationFiscalPolicy", "StalePoliticalJurisdiction": return "税政或辖区已变化，请刷新公文。"
		"FiscalPolicyRequiresLocalAuthority": return "须本人到治所办理。"
	return main._reason(code)

## 军官签受与实际交接各自保留拒因，受命不会跳过同营、离战和原指挥来源。
static func _military_appointment_reason(main, code: String) -> String:
	match code:
		"PoliticalMilitaryAnswerExpired", "PoliticalMilitaryArrivalExpired": return "这份调任公文的办理期限已过。"
		"PoliticalMilitaryOrderUnavailable", "PoliticalMilitaryOrderWithdrawn": return "该调任已撤回或不再有效。"
		"PoliticalMilitaryAppointmentPending": return "此部队或此人已有未办结调任，须先处理原公文。"
		"PoliticalMilitaryCandidateDeclinesDuty": return "此人当前未接受该组织的职责。"
		"PoliticalMilitaryOrderNotDelivered": return "须与受任本人在同一场所当面交付。"
		"PoliticalMilitaryGrantChanged", "PoliticalMilitaryAuthorityChanged", "InvalidActingAuthority": return "原公职或专项调任授权已经变化。"
		"PoliticalMilitarySourceMismatch", "MilitaryOfficerCommandSourceChanged", "MilitaryOfficerOrganizationChanged": return "部队归属或指挥来源已经变化，须重新核定。"
		"MilitaryOfficerAlreadyCommands": return "此人仍指挥另一部队，不能兼任本次交接。"
		"MilitaryOfficerUnitEngaged", "MilitaryOfficerUnitOccupied", "MilitaryOfficerPredecessorOccupied": return "部队或原指挥仍在出征、交战或其他占用中。"
		"PoliticalMilitaryActorBusy", "MilitaryOfficerCandidateUnavailable": return "须受命本人到营，且存活、空闲并脱离其他队伍占用。"
		"PoliticalMilitaryCampRouteUnavailable": return "尚无可通往原营地的合法路线。"
	return main._reason(code)

## 原撤职的专项来源不足时保留原生拒因，不把失败表现为已免职。
static func _dismissal_reason(main, code: String) -> String:
	match code:
		"PoliticalDismissalJurisdictionNotCovered": return "此辖区没有明确的撤职来源条款。"
		"PoliticalDismissalOriginalGrantRequired", "PoliticalDismissalSourcePolicyRequired": return "原上级未持本次撤职的完整专项授权。"
		"PoliticalDismissalMemberDeclinesDuty": return "本人当前未接受这项组织职责。"
		"PoliticalDismissalSourcePolicyContinuityRequired": return "辖区控制已变化，须先有明确的后续任免制度。"
	return main._reason(code)

## 钱粮不足按原生可用资源解释；总余额不能代替扣除其它责任后的预留能力。
static func _governance_reason(main, code: String) -> String:
	match code:
		"InsufficientReliefFunds": return "公库可用钱不足。"
		"InsufficientReliefGrain": return "公粮可用数量不足。"
		"ReliefAlreadyReserved", "GovernanceAuthorityAlreadyAccepted": return "这项委任已经受领。"
		"GovernanceAuthorityExpired": return "这项委任的受领期限已过。"
		"GovernanceExecutorNotAtSite": return "须本人实际到治所受领。"
		"GovernanceExecutorTemporarilyUnavailable": return "执行人暂时无法继续，已做的劳动保留。"
		"GovernanceControlOrAssetsChanged": return "辖区或公库权属已变化，责任等待重新安排。"
		"StaleGovernanceViewIndex", "ReliefPreviewRevisionMismatch": return "情形已经变化，请刷新。"
	return main._reason(code)

## 展开只改本地阅读状态，离场/换世界后的旧回调不能重开或下令。
static func _toggle(main_ref: WeakRef, key: String, source: Dictionary, scene_epoch: int) -> void:
	var main=main_ref.get_ref()
	if not is_instance_valid(main) or not main.is_inside_tree() or not is_instance_valid(main.kernel): return
	if main._scene_counter!=scene_epoch or main._advancing or main.kernel.source_identity()!=source: return
	var expanded: Dictionary=main.get_meta("politics_terms_expanded",{}).duplicate(true)
	expanded[key]=not expanded.get(key,false)
	main.set_meta("politics_terms_expanded",expanded)
	main._render()

## 所有动作使用Main既有的身份、版本、可见控件和二次报价守卫。
static func _rows(main, parent: VBoxContainer, view: Dictionary) -> void:
	for row in view.get("rows",[]):
		var box=VBoxContainer.new()
		box.name="PoliticalAffair"
		parent.add_child(box)
		box.set_meta("politics_row",row.duplicate(true))
		main._text(box,row.get("display_name","")+" · "+row.get("text",""),16)
		if row.has("observed_tick"):
			main._text(box,"这条消息发生于半日%d。"%row.observed_tick,14)
		var key: String=row.get("id","")+":"+row.get("source_fact_id","")
		var expanded: bool=main.get_meta("politics_terms_expanded",{}).get(key,false)
		main._button(box,"收起职责与交接" if expanded else "核对职责与交接","politics_terms",key,_toggle.bind(weakref(main),key,main.kernel.source_identity(),main._scene_counter),main._advancing)
		if expanded:
			main._text(box,row.get("terms_text",""),15)
			if row.has("expires_tick"):
				main._text(box,"办理时限：半日%d前。"%row.expires_tick,15)
		for action in row.get("actions",[]):
			if not expanded: continue
			var button: Button=main._regional_button(box,action.text,action.command,row.id,action.params,{},not action.get("available",false))
			button.name="PoliticalChoice"
			button.set_meta("politics_action",action.duplicate(true))
			if action.command=="political_dismiss" and not action.get("available",false):
				main._text(box,_dismissal_reason(main,action.get("code","Unknown")),14)
			if action.command in ["political_military_officer_issue","political_military_officer_answer","political_military_officer_withdraw","military_officer_assign"] and not action.get("available",false):
				main._text(box,_military_appointment_reason(main,action.get("code","Unknown")),14)
			if action.command=="political_set_occupation_tax" and not action.get("available",false):
				main._text(box,_occupation_fiscal_reason(main,action.get("code","Unknown")),14)
			if action.command in ["political_commission_governance","governance_accept","governance_cancel"] and not action.get("available",false):
				main._text(box,_governance_reason(main,action.get("code","Unknown")),14)
	if view.get("truncated",false):
		main._text(parent,"当前先列出最近与眼前公务，未显示的记录仍保留。",15)
