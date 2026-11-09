## 逐项核对真实接管责任，选择只留在本页，提交仍走原命令报价与事务。
extends RefCounted

const COMMAND := "political_assume_public_responsibilities"
const LABELS := {
	"continue_under_original": "保留原执行责任",
	"assume_by_successor": "由接任组织承担执行",
	"suspend": "暂停执行，保留原欠责",
	"settlement_required": "留待清算，暂不支出",
}

## 预读屏障与控件复用同一完整参数，组合选择也必须先有当前原生报价。
static func command_params(main, row: Dictionary) -> Dictionary:
	var draft: Dictionary = main.get_meta("political_takeover_choices", {})
	var selections: Dictionary = {}
	if draft.get("source", {}) == main.kernel.source_identity():
		selections = draft.get("registrations", {}).get(row.registration_fact_id, {})
	var params: Dictionary = row.get("command_params", {}).duplicate(true)
	var decisions: Array = []
	var position := 0
	for responsibility in row.get("responsibilities", []):
		var chosen: String = selections.get(str(position), "suspend")
		var request: Dictionary = {}
		for choice in responsibility.get("choices", []):
			if choice.get("disposition", "") == chosen:
				request = choice.get("request", {}).duplicate(true)
				break
		decisions.append(request)
		position += 1
	params["decisions"] = decisions
	return params

static func local_affairs(main, parent: VBoxContainer) -> void:
	if not is_instance_valid(main) or not main._resident_mode or not main._started: return
	if not is_instance_valid(main.kernel) or not main.kernel.has_method("political_takeover_enabled") or not main.kernel.political_takeover_enabled(): return
	var location: Dictionary = main.kernel.presentation_location()
	var view: Dictionary = main.kernel.political_takeover_entries(location.get("place_id", ""))
	if not view.get("ok", false):
		main._text(parent, "此时无法核对接管责任：" + reason(main, view.get("code", "Unknown")), 15)
		return
	var source: Dictionary = main.kernel.source_identity()
	var draft: Dictionary = main.get_meta("political_takeover_choices", {})
	if draft.get("source", {}) != source:
		draft = {"source": source.duplicate(true), "registrations": {}}
		main.set_meta("political_takeover_choices", draft)
	for row in view.get("registrations", []):
		var box := VBoxContainer.new()
		box.name = "PoliticalTakeover"
		box.set_meta("takeover_registration", row.duplicate(true))
		parent.add_child(box)
		main._text(box, row.get("display_name", "本辖区") + " · 接管后的责任交接", 17)
		main._text(box, "公库已经交接，下面逐项决定如何执行原责任。旧欠额和原受益人继续保留。", 15)
		var params: Dictionary = command_params(main, row)
		var selected: Dictionary = draft.registrations.get(row.registration_fact_id, {})
		var incomplete: bool = row.get("truncated", false)
		var position := 0
		for responsibility in row.get("responsibilities", []):
			main._text(box, "%d. %s" % [position + 1, responsibility.get("display_name", "原责任")], 16)
			var previous: Variant = responsibility.get("decision")
			if previous is Dictionary:
				main._text(box, "上次裁定：" + LABELS.get(previous.get("disposition", ""), "待核对"), 14)
			if responsibility.has("selected_supply_policy_fact_id"):
				main._text(box, "上次采用的供给政策凭据：" + str(responsibility.selected_supply_policy_fact_id), 14)
			if responsibility.has("supply_policy"):
				var policy: Dictionary = responsibility.supply_policy
				main._text(box, "本次可选择的已签政策：%s；单份采购合同本金与报酬合计最多 %d 钱。" % [policy.policy_fact_id, policy.maximum_contract_commitment], 14)
				main._text(box, "批准后由这份新政策办理新采购；原需求、已受领合同及其受益人仍按原约保留。", 14)
			var picker := OptionButton.new()
			picker.name = "TakeoverResponsibilityChoice"
			box.add_child(picker)
			var chosen: String = selected.get(str(position), "suspend")
			var request: Dictionary = {}
			var index := 0
			for choice in responsibility.get("choices", []):
				picker.add_item(choice.get("label", LABELS.get(choice.disposition, "待核对")))
				picker.set_item_metadata(index, choice.duplicate(true))
				picker.set_item_disabled(index, not choice.get("available", false))
				if choice.disposition == chosen:
					picker.select(index)
					request = choice.get("request", {}).duplicate(true)
					if not choice.get("available", false):
						incomplete = true
						main._text(box, reason(main, choice.get("code", "Unknown")), 14)
				index += 1
			if request.is_empty(): incomplete = true
			picker.disabled = main._advancing or not row.get("available", false)
			picker.item_selected.connect(_choose.bind(weakref(main), weakref(picker), row.registration_fact_id, position, source.duplicate(true), main._scene_counter))
			position += 1
		var button: Button = main._regional_button(box, "确认这份责任交接决定", COMMAND, row.registration_fact_id, params, {}, incomplete or not row.get("available", false))
		button.name = "ConfirmPoliticalTakeover"
		if not row.get("available", false): main._text(box, reason(main, row.get("code", "Unknown")), 14)
	if view.get("truncated", false): main._text(parent, "当前只显示有界的本人职责记录，其余原账仍保留。", 14)

## 旧控件、旧场所与旧世界的选择不能改变当前草稿或提交新职责。
static func _choose(index: int, main_ref: WeakRef, picker_ref: WeakRef, registration: String, position: int, source: Dictionary, scene_epoch: int) -> void:
	var main = main_ref.get_ref()
	var picker = picker_ref.get_ref()
	if not is_instance_valid(main) or not is_instance_valid(picker) or not is_instance_valid(main.kernel): return
	if not main.is_inside_tree() or not picker.is_inside_tree() or not picker.is_visible_in_tree() or picker.disabled: return
	if main._advancing or main._scene_counter != scene_epoch or main.kernel.source_identity() != source: return
	if index < 0 or index >= picker.item_count or picker.is_item_disabled(index): return
	var choice: Dictionary = picker.get_item_metadata(index)
	if not choice.get("available", false): return
	var draft: Dictionary = main.get_meta("political_takeover_choices", {}).duplicate(true)
	if draft.get("source", {}) != source: return
	var selections: Dictionary = draft.registrations.get(registration, {})
	selections[str(position)] = choice.disposition
	draft.registrations[registration] = selections
	main.set_meta("political_takeover_choices", draft)
	main._render()

static func reason(main, code: String) -> String:
	match code:
		"PoliticalResponsibilityPending": return "责任仍待有权者逐项裁定。"
		"PoliticalResponsibilitySuspended": return "这项责任暂缓执行，原欠责没有免除。"
		"PoliticalTakeoverNotKnown": return "须由本人到原治所履行当前接任职责。"
		"PoliticalTakeoverAuthorityDenied": return "当前公职授权不足以裁定这份交接。"
		"PoliticalTakeoverControlChanged", "PoliticalExecutionAccountChanged": return "控制权或指定公库已变化，须重新核对交接。"
		"PoliticalTakeoverRevisionMismatch", "PoliticalTakeoverDecisionChanged": return "情形或上次裁定已经变化，请刷新后核对。"
		"PoliticalSupplySuccessorPolicyRequired": return "新供给经营须另行确定需求政策、资金与报酬，原采购责任继续保留。"
		"PoliticalSupplyPolicyAuthorityChanged", "PoliticalSupplyControlChanged": return "这份供给政策的公职、控制权或公库许可已变化，请重新签发并核对。"
		"PoliticalSupplyPolicyExpired": return "这份供给政策已过期，须重新签发。"
		"PoliticalSupplyPolicyNoExecutableTick": return "这份政策剩余时间不足一个可执行半日，须重新签发。"
		"PoliticalSupplyPolicyNotEffective": return "这份政策尚未到生效半日。"
		"PoliticalFiscalReceivableAssignmentRequired": return "旧税款须另行确定收取权，暂不能直接承担征收。"
		"PoliticalTaskExecutionAdaptationRequired": return "任务资金与受托财物须按原责任办理，暂不能直接改派。"
		"PoliticalBeneficiaryAssignmentRequired": return "原受益人的收款权不能随公库接管改派。"
		"PoliticalResponsibilityPageLimit": return "这份交接超出当前完整展示范围，暂不能提交不完整决定。"
	return main._reason(code)
