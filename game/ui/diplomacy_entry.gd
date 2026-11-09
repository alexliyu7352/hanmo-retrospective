## 外交只展示本人已知来源与原生报价；接触、受领、实际交接和条约接受分别表述。
extends RefCounted

## 此叶等待具名共同注册；目录中出现作者条款本身不能打开功能。
static func local_affairs(main, parent: VBoxContainer) -> void:
	if not is_instance_valid(main) or not main._resident_mode or not main._started: return
	if not is_instance_valid(main.kernel) or not main.kernel.has_method("diplomacy_enabled") or not main.kernel.diplomacy_enabled(): return
	var location: Dictionary = main.kernel.presentation_location()
	var view: Dictionary = main.kernel.diplomacy_entries(location.get("place_id", ""))
	if not view.get("ok", false):
		main._text(parent, "此时无法查阅外交往来：" + reason(main, view.get("code", "Unknown")), 15)
		return
	for row in view.get("rows", []):
		var box := VBoxContainer.new()
		box.name = "DiplomaticAffair"
		box.set_meta("diplomacy_row", row.duplicate(true))
		parent.add_child(box)
		main._text(box, row.get("display_name", "") + " · " + row.get("text", ""), 16)
		var key: String = row.get("id", "") + ":" + row.get("source_fact_id", "")
		var expanded: bool = main.get_meta("diplomacy_expanded", {}).get(key, false)
		main._button(box, "收起条款与办理事项" if expanded else "核对条款与办理事项", "diplomacy_terms", key, _toggle.bind(weakref(main), key, main.kernel.source_identity(), main._scene_counter), main._advancing)
		if not expanded: continue
		main._text(box, row.get("terms_text", ""), 15)
		if not row.get("navigation_code", "").is_empty():
			main._text(box, "当前不能启程：" + reason(main, row.navigation_code), 15)
		if row.has("expires_tick"):
			main._text(box, "原提议递交截止：半日 %d 前；已递交后的返报责任另行保留。" % row.expires_tick, 15)
		for action in row.get("actions", []):
			var button: Button = main._regional_button(box, action.text, action.command, row.id, action.params, {}, not action.get("available", false))
			button.name = "DiplomaticChoice"
			button.set_meta("diplomacy_action", action.duplicate(true))
			if not action.get("available", false):
				main._text(box, reason(main, action.get("code", "Unknown")), 14)
	if view.get("truncated", false):
		main._text(parent, "先列出本人已知的当前往来，其余记录仍保留。", 15)

## 已销毁宿主、离场、世界替换和旧版本控件不能重新展开或间接下令。
static func _toggle(main_ref: WeakRef, key: String, source: Dictionary, scene_epoch: int) -> void:
	var main = main_ref.get_ref()
	if not is_instance_valid(main) or not main.is_inside_tree() or not is_instance_valid(main.kernel): return
	if main._scene_counter != scene_epoch or main._advancing or main.kernel.source_identity() != source: return
	var expanded: Dictionary = main.get_meta("diplomacy_expanded", {}).duplicate(true)
	expanded[key] = not expanded.get(key, false)
	main.set_meta("diplomacy_expanded", expanded)
	main._render()

## 等待、失权与真实期限分别说明；界面不能将呈递成功称为条约生效。
static func reason(main, code: String) -> String:
	match code:
		"InsufficientFunds": return "本人可用私钱不足，请先备妥旅费；公库余额不是可直接花用的私钱。"
		"DiplomaticOfferNotKnown", "DiplomaticMissionNotKnown": return "本人尚未掌握这项往来的有效来源。"
		"DiplomaticOfferExpired", "EnvoyDeliveryExpired": return "原提议的递交期限已过。"
		"DiplomaticMissionAcceptanceExpired": return "这项使者责任的受领期限已过。"
		"DiplomaticMissionAtWrongSite": return "须本人回到原发令场所受领。"
		"DiplomaticRepresentativeChanged", "DiplomaticRepresentativeUnavailable": return "对方代表已变化，须重新找到有权接收者会面。"
		"DiplomaticRepresentativeUnauthorized": return "当前代表已无本项接收权限，原责任等待重新会面。"
		"DiplomaticAuthorityChanged": return "原签发权源已变化，责任等待重新安排。"
		"DiplomaticEnvoyUnavailable": return "本人暂时无法继续，原行动进度保留。"
		"DiplomaticEnvoyDead", "DiplomaticPrincipalDissolved": return "执行人或责任主体已无法继续，原记录保留待处理。"
		"DiplomaticReportReceiverChanged", "DiplomaticReportContactChanged": return "原报告公职的接收人已变化，请重新与现任合法接收者会面。"
		"DiplomaticReportReceiverUnauthorized", "DiplomaticReportOfficeUnavailable": return "当前报告公职暂不能接收，已完成递交和本人返报责任保留。"
		"DiplomaticDelegationAlreadyIssued": return "这项具名委任已经实际说明，请查阅本人已知使命。"
		"DiplomaticDelegationRevisionMismatch": return "当前情形已变化，请重新查看。"
		"DiplomaticSelfReportUnsupported": return "本人亲自签发的使者责任不能再要求向自己返报。"
		"FaceToFaceContactRequired": return "须与当前合法代表实际同场会面。"
		"StaleDiplomacyViewIndex", "DiplomaticOfferChanged", "StaleContactProof": return "情形已经变化，请重新查看并核对条款。"
	return main._reason(code)
