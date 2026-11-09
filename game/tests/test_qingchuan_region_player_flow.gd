## 青川1.6正式Main恢复验证片；只点击当前产品控件，不提交Kernel命令或改写世界。
extends RefCounted
const Strict=preload("res://core/strict_json.gd")
const Art=preload("res://ui/art_catalog.gd")
const P="society::"

## 失败进入本次恢复证据；不能以更换人物、库存或期限继续。
static func check(h, value: bool, label: String) -> bool:
	return h.check(value,"qingchuan_"+label)

## 只在实际当前页中查找可见且存活的语义控件，隐藏菜单不能抢占结果。
static func control(node: Node, key: String, value: String) -> Control:
	if node is Control and node.is_inside_tree() and node.is_visible_in_tree() and not node.is_queued_for_deletion() and node.get_meta(key,"")==value: return node
	for child in node.get_children():
		var found: Control=control(child,key,value)
		if found!=null: return found
	return null

## 同命令多目标仍须精确命中当前可见按钮，不能借旧页节点越过原守卫。
static func button(node: Node, kind: String, target: String="") -> Button:
	if node is Button and node.is_inside_tree() and node.is_visible_in_tree() and not node.is_queued_for_deletion() and node.get_meta("command_kind","")==kind and (target.is_empty() or node.get_meta("target_id","")==target): return node
	for child in node.get_children():
		var found: Button=button(child,kind,target)
		if found!=null: return found
	return null

## 点击只发实际按钮的原信号，异步读写由Main正常帧循环处理。
static func click(h, main, kind: String, target: String="") -> bool:
	if not await h.settled(main): return false
	var current: Button=button(main.page,kind,target)
	if not check(h,current!=null and not current.disabled,"visible_click_"+kind+"_"+target): return false
	h.rows.append({"stage":"qingchuan_actual_control","command":kind,"target":target,"path":str(current.get_path()),"source":main.kernel.source_identity()})
	current.pressed.emit()
	return await h.settled(main)

## 冷世界只供验证和保存原始证据，从不作为恢复或选择未知目标的输入。
static func state(h, main) -> Dictionary:
	var exported: Dictionary=main.kernel.transport().debug_export_state()
	if not check(h,exported.get("ok",false) and exported.get("json") is String,"world_evidence_export"): return {}
	var parsed: Dictionary=Strict.parse_text(exported.json)
	if not check(h,parsed.get("ok",false) and parsed.get("value") is Dictionary,"world_evidence_shape"): return {}
	return parsed.value

## 外貌证据同时保存原九槽档案及唯一清单解析的半身/头像资源，读操作不重抽外貌。
static func appearance(h, main, ids: Dictionary) -> Dictionary:
	var before: Dictionary=main.kernel.source_identity()
	var world: Dictionary=state(h,main)
	if world.is_empty(): return {}
	var profile: Dictionary=world.actors[ids.player_actor_id].appearance
	var visible: Dictionary=main.kernel.character_input(ids.player_actor_id)
	if visible.get("code")=="NativeAsyncRequired":
		if not await h.settled(main): return {}
		visible=main.kernel.character_input(ids.player_actor_id)
	var half: Dictionary=Art.character_input(visible,"half_body")
	var avatar: Dictionary=Art.character_input(visible,"avatar")
	if not check(h,profile.get("source")=="custom" and profile.get("generation_seed")=="not-sampled" and profile.get("base_features",{}).get("face_shape")==ids.appearance_choice_id,"original_nondefault_profile"): return {}
	if not check(h,half.get("ok",false) and avatar.get("ok",false) and half.get("degradation",[]).is_empty() and avatar.get("degradation",[]).is_empty() and not half.get("layers",[]).is_empty() and not avatar.get("layers",[]).is_empty(),"same_profile_real_registered_views"): return {}
	if not check(h,before==main.kernel.source_identity(),"appearance_read_only"): return {}
	return {"profile":profile.duplicate(true),"half_body":half,"avatar":avatar}

## 正常空标题先选精确安装章，再点击非默认整组成人代表和正式确认。
static func start(h, main, ids: Dictionary) -> bool:
	if not check(h,not main._started and not main._resident_mode and main.kernel.snapshot().is_empty(),"empty_title"): return false
	var start_button: Button=button(main.page,"start")
	if not check(h,start_button!=null and not start_button.disabled,"normal_start"): return false
	start_button.pressed.emit()
	if not check(h,is_instance_valid(main._scenario_dialog) and main._scenario_dialog.visible,"scenario_window"): return false
	var selected: Button=button(main._scenario_dialog,"scenario_select",ids.scenario_id)
	if not check(h,selected!=null and not selected.disabled and selected.get_meta("pack_set_digest","")==ids.pack_set_digest and selected.get_meta("player_actor_id","")==ids.player_actor_id,"exact_installed_chapter"): return false
	selected.pressed.emit()
	if not check(h,is_instance_valid(main._appearance_picker) and is_instance_valid(main._appearance_dialog) and main._appearance_dialog.visible and not main._started,"actual_appearance_picker"): return false
	var picker: Control=main._appearance_picker
	var initial: Dictionary=picker.draft()
	var choice: Button=control(picker,"appearance_choice_id",ids.appearance_choice_id) as Button
	if not check(h,choice!=null and not choice.disabled and initial.get("base_features",{}).get("face_shape")!=ids.appearance_choice_id,"nondefault_representative_visible"): return false
	choice.pressed.emit()
	var selected_draft: Dictionary=picker.draft()
	if not check(h,selected_draft.get("base_features",{}).get("face_shape")==ids.appearance_choice_id,"choice_installed_before_confirmation"): return false
	var confirm: Button=control(picker,"appearance_action","confirm") as Button
	if not check(h,confirm!=null and not confirm.disabled,"appearance_confirm"): return false
	confirm.pressed.emit()
	if not await h.settled(main): return false
	if not check(h,main._resident_mode and main.kernel.content().get("pack_set_digest")==ids.pack_set_digest and main.kernel.command_context().get("actor_id")==ids.player_actor_id and main.kernel.presentation_location().get("place_id")==P+"place.courtyard","normal_selected_world"): return false
	var shown: Dictionary=await appearance(h,main,ids)
	if shown.is_empty(): return false
	return check(h,shown.profile.base_features==selected_draft.base_features and shown.profile.attire_style==selected_draft.attire_style,"selected_full_tuple_persisted") and h.write_json("selected-appearance.json",shown)

## 院前真实介绍只读取本地压力，选择留下不承接任务或凭对白生成钱粮。
static func introduction(h, main) -> bool:
	var before: Dictionary=state(h,main)
	if not await click(h,main,"talk",P+"role.city_supply"): return false
	for step in range(4):
		var view: Dictionary=main.kernel.interaction_view()
		if view.get("phase")=="choice":
			if not await click(h,main,"dialogue_choose","stay"): return false
			var after: Dictionary=state(h,main)
			return conserved(h,before,after,"introduction") and check(h,main.kernel.interaction_view().get("phase")=="scene","introduction_closed")
		if not check(h,view.get("phase")=="say","authored_introduction_say"): return false
		h.rows.append({"stage":"qingchuan_courtyard_introduction","view":view.duplicate(true)})
		if not await click(h,main,"dialogue_next"): return false
	return check(h,false,"introduction_limit")

## 只比较钱粮、任务与原事实的域记录；对白会话自身允许产生原曝光与继续点。
static func conserved(h, before: Dictionary, after: Dictionary, label: String) -> bool:
	for key in ["tick","wallets","inventories","tasks","facts"]:
		if not check(h,before.get(key)==after.get(key),label+"_preserves_"+key): return false
	return true

## 当前页有限行才是可选合同的来源；不搜索冷世界中的远方任务。
static func rows(node: Node, meta: String) -> Array:
	var result: Array=[]
	if node is Control and node.is_visible_in_tree() and not node.is_queued_for_deletion() and node.has_meta(meta): result.append(node.get_meta(meta).duplicate(true))
	for child in node.get_children(): result.append_array(rows(child,meta))
	return result

## 地图选择、确认与每半日均点击原控件；有未处理道路输入时原样停止。
static func travel(h, main, destination: String, deadline: int) -> bool:
	if not main.journey_view.opened and not await click(h,main,"map"): return false
	if not await click(h,main,"map_select",destination) or not await click(h,main,"map_confirm"): return false
	while main.kernel.action_context().get("action_kind")=="travel":
		if not check(h,not main.kernel.action_needs_input() and main.kernel.source_identity().tick<deadline,"legal_road_continuation"): return false
		var tick: int=main.kernel.source_identity().tick
		if not await click(h,main,"advance"): return false
		if not check(h,main.kernel.source_identity().tick==tick+1,"actual_road_halfday"): return false
	if main.journey_view.opened and not await click(h,main,"map_close"): return false
	var place: String=main.kernel.presentation_location().get("place_id","")
	return check(h,main._data.places.get(place,{}).get("settlement_id")==destination and main.kernel.action_context().get("action_id","").is_empty(),"actual_road_arrival")

## 经原近况页承接指定当地四粟公托；原报价冻结4/24半日期限，不缩量或换单。
static func accept(h, main, role_id: String, half_days: int) -> Dictionary:
	if not await click(h,main,"dynamic_open"): return {}
	var offered: Dictionary={}
	for row in rows(main.page,"dynamic_row"):
		if row.get("kind")=="opportunity" and row.get("action",{}).get("params",{}).get("context",{}).get("role_id")==role_id: offered=row; break
	if not check(h,not offered.is_empty(),"original_local_offer"): return {}
	var current: Button=button(main.page,"accept_offer",offered.id)
	if not check(h,current!=null,"original_accept_control"): return {}
	var row_node: Node=current
	while row_node!=null and not row_node.has_meta("dynamic_row"): row_node=row_node.get_parent()
	var quantity: SpinBox=row_node.find_child("DynamicQuantity",true,false) if row_node!=null else null
	if not check(h,quantity!=null and quantity.is_visible_in_tree() and quantity.max_value>=4,"four_original_units_available"): return {}
	quantity.value=4
	if not await h.settled(main): return {}
	current=button(main.page,"accept_offer",offered.id)
	if not check(h,current!=null,"fresh_accept_control"): return {}
	var action: Dictionary=current.get_meta("dynamic_action",{}).duplicate(true)
	var quote: Dictionary=await h.Flow.preview(h,main,"accept_offer",action.get("params",{}))
	var tick: int=main.kernel.source_identity().tick
	# 原公托先用报价duration完成承接，再从完成时刻计算作者4/24半日期限。
	var accept_duration: int=quote.get("duration",0)
	if not check(h,quote.get("ok",false) and accept_duration>0 and quote.get("contract_terms",{}).get("quantity")==4 and quote.contract_terms.get("deadline_tick")==tick+accept_duration+half_days,"original_four_unit_frozen_deadline"): return {}
	if not await click(h,main,"accept_offer",offered.id): return {}
	if not await finish(h,main,"accept_offer",quote.contract_terms.deadline_tick): return {}
	for row in rows(main.page,"dynamic_row"):
		if row.get("kind")=="contract" and row.get("offer_id")==action.params.offer_id and row.get("actor_id")==main.kernel.command_context().actor_id:
			if not check(h,row.get("agreed_quantity")==4 and row.get("purchase_budget_cap")==80 and row.get("deadline_tick")==quote.contract_terms.deadline_tick,"same_frozen_original_contract"): return {}
			if not await click(h,main,"dynamic_close"): return {}
			return row
	check(h,false,"accepted_contract_visible"); return {}

## 仅完成已经由原控件接受的行动；不替缺失按钮提交advance或越过需要输入。
static func finish(h, main, kind: String, deadline: int) -> bool:
	while main.kernel.action_context().get("action_kind")==kind:
		if not check(h,main.kernel.source_identity().tick<deadline and not main.kernel.action_needs_input(),"original_"+kind+"_continuable"): return false
		if not await click(h,main,"advance"): return false
	return check(h,main.kernel.action_context().get("action_id","").is_empty(),"original_"+kind+"_finished")

## 真分层HitArea走原激活信号；不预热热点报价或调用场景activate便捷入口。
static func hotspot(h, main, id: String) -> bool:
	if not await h.settled(main): return false
	var target: Control=control(main.page,"hotspot_id",id)
	if not check(h,target!=null and target.has_signal("activated") and target.get("enabled")==true,"actual_hitarea_"+id): return false
	h.rows.append({"stage":"qingchuan_actual_hitarea","hotspot_id":id,"path":str(target.get_path()),"source":main.kernel.source_identity(),"gui_mouse_evidence":false})
	target.emit_signal("activated")
	return await h.settled(main)

## 只在当面明确忙碌时由本人等一个真实半日，原提供人仍忙则保留失败。
static func merchant_ready(h, main, deadline: int) -> bool:
	var quote: Dictionary=await h.Flow.preview(h,main,"talk",{"role_id":P+"role.merchant"})
	if quote.get("code")=="ActorBusy":
		h.rows.append({"stage":"qingchuan_original_provider_busy","quote":quote,"source":main.kernel.source_identity()})
		if not check(h,main.kernel.source_identity().tick+1<deadline,"provider_wait_keeps_contract_window"): return false
		if not await click(h,main,"wait"): return false
		quote=await h.Flow.preview(h,main,"talk",{"role_id":P+"role.merchant"})
	return check(h,quote.get("ok",false),"original_provider_available_"+str(quote.get("code","")))

## 当面热点进原粮单，四粟全部使用同一合同预算；任何缺货或拒绝都不能改数量通关。
static func purchase(h, main, contract: Dictionary) -> bool:
	if not await merchant_ready(h,main,contract.deadline_tick) or not await hotspot(h,main,"grain.talk"): return false
	if not check(h,main.kernel.interaction_view().get("phase")=="say","hitarea_opened_original_merchant"): return false
	if not await click(h,main,"dialogue_next") or not await click(h,main,"dialogue_choose","trade") or not await click(h,main,"dialogue_confirm"): return false
	if not check(h,main.kernel.interaction_view().get("phase")=="trade","original_trade_sheet"): return false
	var sheet: Dictionary=main.kernel.trade_view(P+"shop.grain",P+"actor.merchant")
	var wallet_id: String=""
	for payer in sheet.get("task_payers",[]):
		if payer.get("offer_id")==contract.offer_id: wallet_id=payer.wallet_id
	if not check(h,not wallet_id.is_empty(),"same_contract_budget_visible"): return false
	if not await select_option(h,main,"_item",P+"item.millet") or not await select_option(h,main,"_payer",wallet_id): return false
	if not check(h,is_instance_valid(main._quantity) and main._quantity.is_visible_in_tree(),"actual_trade_quantity"): return false
	main._quantity.value=4
	if not await h.settled(main): return false
	var request: Dictionary=main._trade_request.duplicate(true)
	if not check(h,request.get("quantity")==4 and request.get("payer_wallet_id")==wallet_id,"four_units_from_original_contract_budget"): return false
	if not await click(h,main,"trade") or not await finish(h,main,"trade",contract.deadline_tick): return false
	if not await click(h,main,"end_interaction"): return false
	if not check(h,main.kernel.interaction_view().get("phase")=="scene","original_trade_closed"): return false
	if not await hotspot(h,main,"grain.return"): return false
	return check(h,main.kernel.presentation_location().get("place_id")==P+"place.courtyard","return_hitarea_reaches_real_courtyard")

## 选项按实际元数据找值并发原用户选择信号，重绘后下一项重新取控件。
static func select_option(h, main, field: String, value: String) -> bool:
	var option: OptionButton=main.get(field)
	if not check(h,is_instance_valid(option) and option.is_visible_in_tree(),"visible_trade_"+field): return false
	for index in range(option.item_count):
		if option.get_item_metadata(index)==value:
			option.select(index); option.item_selected.emit(index)
			return await h.settled(main)
	return check(h,false,"missing_original_trade_option_"+field)

## 原经办人按钮处理同一合同实货，之后从真实合同详情核足额交付和一次结算。
static func report(h, main, contract: Dictionary) -> bool:
	if not await click(h,main,"dynamic_open"): return false
	var current: Button=button(main.page,"report_offer",contract.task_id)
	if not check(h,current!=null and current.get_meta("dynamic_action",{}).get("params",{}).get("offer_id")==contract.offer_id,"same_contract_report_control"): return false
	if not await click(h,main,"report_offer",contract.task_id) or not await finish(h,main,"report_offer",contract.deadline_tick): return false
	var factual: Dictionary=await contract_detail(h,main,contract)
	if not check(h,factual.get("delivered_quantity")==4 and factual.get("settlement_status")=="settled" and factual.get("paid_reward")==8,"actual_four_grain_delivery_and_reward"): return false
	if not h.write_json("fulfilled-contract.json",factual): return false
	return await click(h,main,"dynamic_close")

## 详情只能从当前可见的本人合同按钮进入；读取后严格核task和contract没有换人换单。
static func contract_detail(h, main, contract: Dictionary) -> Dictionary:
	if not await click(h,main,"dynamic_contract",contract.task_id): return {}
	var detail: Dictionary=main.kernel.dynamic_contract(contract.task_id)
	if not check(h,detail.get("task_id")==contract.task_id and detail.get("contract_id")==contract.contract_id,"exact_original_contract_detail"): return {}
	return detail

## 回访只按当前同源topic；核钱粮与原合同不变，保存时保留真实say页继续点。
static func outcome(h, main, contract: Dictionary, topic_id: String, expired: bool) -> Dictionary:
	var current: Button=button(main.page,"talk",topic_id)
	var observed: Dictionary={}
	for row in rows(main.page,"regional_row"):
		if row.get("topic_id")==topic_id: observed=row; break
	if not check(h,current!=null and not current.disabled and current.get_meta("regional_source",{})==main.kernel.source_identity() and observed.get("task_id")==contract.task_id and observed.get("contract_id")==contract.contract_id,"same_contract_current_outcome_topic"): return {}
	var before: Dictionary=state(h,main)
	if not await click(h,main,"talk",topic_id): return {}
	var detail: Label=main.find_child("RegionalDialogueSource",true,false)
	if not check(h,detail!=null and detail.is_visible_in_tree() and main.kernel.interaction_view().get("phase")=="say","visible_factual_outcome_dialogue"): return {}
	var text: String=detail.text
	if not check(h,text.contains("约定4份") and text.contains("实际已交0份" if expired else "实际已交4份") and text.contains("原期限已过" if expired else "约定数量已经实交齐"),"factual_outcome_matches_actual_route"): return {}
	if not conserved(h,before,state(h,main),"outcome_read_only"): return {}
	var evidence: Dictionary={"topic":observed,"text":text,"interaction":main.kernel.interaction_view().duplicate(true),"before":before}
	h.rows.append({"stage":"qingchuan_readonly_contract_outcome","topic":observed,"text":text})
	return evidence

## 正常菜单存档保持对话或课程原继续点，写盘完全由产品槽位窗口完成。
static func save(h, main) -> bool:
	var before: Dictionary=main.kernel.source_identity()
	if not await click(h,main,"system_menu") or not await click(h,main,"save"): return false
	if not await h.Life._wait_slot_window(h,main): return false
	var choice: Button=button(main._slot_dialog,"slot_select","manual-1")
	if not check(h,choice!=null and not choice.disabled,"save_slot_visible"): return false
	choice.pressed.emit()
	var confirm: Button=button(main._slot_dialog,"slot_confirm")
	if not check(h,confirm!=null and not confirm.disabled,"save_confirm_visible"): return false
	confirm.pressed.emit()
	return check(h,await h.settled(main) and main.last_storage_result.get("ok",false) and main.kernel.source_identity()==before,"physical_menu_save_exact_boundary")

## 新消费者只能从空标题地方继续列槽，列槽不创建临时世界。
static func restore(h, main, ids: Dictionary) -> bool:
	if not check(h,not main._started and not main._resident_mode and main.kernel.snapshot().is_empty(),"fresh_consumer_empty_title"): return false
	var resume: Button=button(main.page,"continue_dynamic")
	if not check(h,resume!=null and not resume.disabled,"normal_title_continue"): return false
	resume.pressed.emit()
	if not await h.Life._wait_slot_window(h,main): return false
	if not check(h,not main._started and not main._resident_mode and main.kernel.snapshot().is_empty(),"slot_listing_never_creates_world"): return false
	var choice: Button=button(main._slot_dialog,"slot_select","manual-1")
	if not check(h,choice!=null and not choice.disabled,"original_bank_visible"): return false
	choice.pressed.emit()
	var confirm: Button=button(main._slot_dialog,"slot_confirm")
	if not check(h,confirm!=null and not confirm.disabled,"restore_confirm_visible"): return false
	confirm.pressed.emit()
	if not await h.settled(main): return false
	# 原restore先开舆图；只在当前页确有关闭按钮时回到已保存对白，课程页不点隐藏按钮。
	if button(main.page,"map_close")!=null and not await click(h,main,"map_close"): return false
	return check(h,main.last_storage_result.get("ok",false) and main.kernel.content().get("pack_set_digest")==ids.pack_set_digest and not main._resident_adapter.auto_running,"original_bank_loaded_without_auto_permission")

## 讲舍原六半日自动课程，报名后的原Main首半日是唯一生产者课时。
static func course(h, main) -> Dictionary:
	if not await click(h,main,"move_local",P+"place.school") or not await click(h,main,"courses_open"): return {}
	var duration: OptionButton=main.find_child("StudyDuration",true,false)
	if not check(h,duration!=null and duration.is_visible_in_tree(),"original_course_duration_control"): return {}
	var index: int=-1
	for candidate in range(duration.item_count):
		if duration.get_item_metadata(candidate)==6: index=candidate
	if not check(h,index>=0,"six_halfday_course_option"): return {}
	duration.select(index); duration.item_selected.emit(index)
	if not await h.settled(main): return {}
	if not await h.Study.click(h,main,"study_skill",{"skill_id":"rites","ticks":6,"challenge_mode":"automatic"},false): return {}
	var activity: Dictionary=main.kernel.study_activity().duplicate(true)
	if not check(h,activity.get("completed_lessons")==1 and activity.get("settlement_policy")=="per_lesson_v1","one_actual_original_lesson_before_save"): return {}
	return activity

## 任职答复复用既有membership_action元数据，精确匹配本人、原邀请和答复；隐藏按钮不参与。
static func membership_answer(node: Node, actor_id: String, offer_id: String) -> Button:
	if node is Button and node.is_visible_in_tree() and not node.is_queued_for_deletion():
		var action: Dictionary=node.get_meta("membership_action",{})
		var params: Dictionary=action.get("params",{})
		if action.get("command")=="organization_answer_membership" and params.get("actor_id")==actor_id and params.get("offer_id")==offer_id and params.get("answer")=="decline": return node
	for child in node.get_children():
		var found: Button=membership_answer(child,actor_id,offer_id)
		if found!=null: return found
	return null

## 原课程先正常暂停，回院前核原邀请条款并由本人婉拒；不代NPC决策或清除自动守卫。
static func decline_pending_invitation(h, main, saved: Dictionary, original: Dictionary) -> bool:
	if not await h.Study.click(h,main,"study_pause",{"action_id":saved.action_id}): return false
	if not check(h,main.kernel.study_activity().get("paused",false) and main.kernel.study_activity().get("action_id")==saved.action_id,"same_course_paused_for_personal_decision"): return false
	if button(main.page,"courses_close")!=null and not await click(h,main,"courses_close"): return false
	if button(main.page,"map_close")!=null and not await click(h,main,"map_close"): return false
	if not await click(h,main,"move_local",P+"place.courtyard"): return false
	var candidates: Array=[]
	for row in rows(main.page,"membership_row"):
		if row.get("kind")=="membership_invitation" and row.get("status")=="pending" and row.get("actor_id")==P+"actor.city_steward" and row.get("terms_id")=="terms.city_contract": candidates.append(row)
	if not check(h,candidates.size()==1,"one_visible_original_county_invitation"): return false
	var invitation: Dictionary=candidates[0]
	var prior: Dictionary=original.get("organization",{}).get("membership_offers",{}).get(invitation.id,{})
	if not check(h,prior.get("status")=="pending" and prior.get("target_actor_id")==saved.actor_id and prior.get("source_fact_id")==invitation.source_fact_id and prior.get("revision")==invitation.offer_revision and prior.get("expires_tick")==invitation.expires_tick,"visible_invitation_is_exact_saved_personal_offer"): return false
	if not await click(h,main,"membership_terms",invitation.id): return false
	if not check(h,main.get_meta("membership_terms_expanded",{}).get(invitation.id,false),"original_membership_terms_read"): return false
	var current: Button=membership_answer(main.page,saved.actor_id,invitation.id)
	if not check(h,current!=null and not current.disabled and current.get_meta("regional_source",{})==main.kernel.source_identity(),"current_personal_decline_control"): return false
	var action: Dictionary=current.get_meta("membership_action").duplicate(true)
	if not check(h,action.params.get("expected_offer_revision")==prior.revision and action.params.get("source_fact_id")==prior.source_fact_id,"decline_keeps_exact_original_offer_revision_and_fact"): return false
	var before: Dictionary=main.kernel.source_identity()
	current.pressed.emit()
	if not await h.settled(main): return false
	var answered: Dictionary=state(h,main)
	var actual: Dictionary=answered.organization.membership_offers.get(invitation.id,{})
	var decision: Dictionary=answered.organization.membership_decisions.get(actual.get("decision_fact_id",""),{})
	if not check(h,main.kernel.source_identity().tick==before.tick and actual.get("status")=="declined" and actual.get("revision")==prior.revision+1 and decision.get("kind")=="decline" and decision.get("offer_id")==invitation.id and decision.get("target_actor_id")==saved.actor_id and decision.get("source_fact_ids",[])==[prior.source_fact_id],"actual_personal_decline_of_same_invitation"): return false
	if not check(h,answered.wallets==original.wallets and answered.facts[saved.action_id].terms==original.facts[saved.action_id].terms and answered.facts[saved.action_id].lessons==original.facts[saved.action_id].lessons,"personal_decision_keeps_course_progress_terms_and_money"): return false
	if not h.write_json("course-personal-invitation-declined.json",{"visible_invitation":invitation,"original_offer":prior,"action":action,"actual_offer":actual,"decision":decision,"source":main.kernel.source_identity()}): return false
	if not await click(h,main,"move_local",P+"place.school") or not await h.Study.open_paused(h,main,saved.action_id): return false
	return await click(h,main,"study_resume",saved.action_id)

## 首次自动许可依法停在本人未决邀请；本人处理后只继续同课程实际余课，不绕过输入守卫。
static func finish_course(h, main, saved: Dictionary) -> bool:
	var activity: Dictionary=main.kernel.study_activity()
	if not check(h,activity.get("action_id")==saved.action_id and activity.get("completed_lessons")==1 and activity.get("remaining_ticks")==5,"original_remaining_course_restored"): return false
	var original: Dictionary=state(h,main)
	var original_fact: Dictionary=original.facts[saved.action_id]
	var tick: int=main.kernel.source_identity().tick
	if not await h.budget(5,150) or not await h.stopped(): return false
	var stopped: Dictionary=main._activity_continuation_state()
	if not h.write_json("course-personal-decision-stop.json",{"source":main.kernel.source_identity(),"continuation":stopped,"activity":main.kernel.study_activity()}): return false
	if not check(h,stopped.get("stop")=="awaiting_input" and main.kernel.source_identity().tick==tick and main.kernel.study_activity().get("completed_lessons")==1,"automatic_course_preserves_required_personal_decision"): return false
	if not await decline_pending_invitation(h,main,saved,original): return false
	var resumed: Dictionary=main.kernel.study_activity()
	var taught: int=resumed.get("completed_lessons",0)
	var remaining: int=resumed.get("remaining_ticks",0)
	if not check(h,resumed.get("action_id")==saved.action_id and not resumed.get("paused",true) and taught in [1,2] and remaining==6-taught and main.kernel.source_identity().tick==tick+taught-1,"same_course_resumed_actual_remaining_ticks"): return false
	if not await h.budget(remaining,remaining*30) or not await h.stopped(): return false
	var world: Dictionary=state(h,main)
	var fact: Dictionary=world.facts.get(saved.action_id,{})
	if not check(h,main.kernel.source_identity().tick==tick+5 and fact.get("lessons",[]).size()==6 and fact.get("success",false) and fact.get("phase")=="completed" and not world.actions.has(saved.action_id),"exact_original_five_remaining_lessons"): return false
	var xp: int=0
	for lesson in fact.lessons: xp+=lesson.get("segment",{}).get("xp_gained",0)
	var payer: String=original_fact.terms.payer_wallet_id
	if not check(h,fact.terms==original_fact.terms and fact.terms.fee==180 and fact.terms.duration==6 and original.wallets[payer].balance-world.wallets[payer].balance==180-saved.paid and not world.wallets[payer].reserved.has(saved.action_id) and xp==120 and world.actors[saved.actor_id].skills.rites-fact.terms.xp_before==120,"original_course_total_fee180_and_experience120"): return false
	if not h.write_json("completed-course-fact.json",fact): return false
	if button(main.page,"courses_close")!=null and not await click(h,main,"courses_close"): return false
	if button(main.page,"map_close")!=null and not await click(h,main,"map_close"): return false
	return await click(h,main,"move_local",P+"place.courtyard")

## 六课后原县仓合同应实际逾期零交付，未用采购金与报酬沿原清算退回。
static func expired_contract(h, main, contract: Dictionary) -> bool:
	if not await click(h,main,"dynamic_open"): return false
	var detail: Dictionary=await contract_detail(h,main,contract)
	if not check(h,detail.get("status")=="failed" and main.kernel.source_identity().tick>contract.deadline_tick and detail.get("delivered_quantity")==0 and detail.get("settlement_status")=="settled" and detail.get("paid_reward")==0 and detail.get("refunded_budget")==80 and detail.get("refunded_reward")==8,"original_expired_zero_delivery_refund"): return false
	if not h.write_json("expired-contract.json",detail): return false
	return await click(h,main,"dynamic_close")

## 继续原已恢复对话只走say和leave；回访结束不会重复钱粮或合同结算。
static func close_outcome(h, main) -> bool:
	var before: Dictionary=state(h,main)
	if not check(h,main.kernel.interaction_view().get("phase")=="say","restored_original_outcome_say"): return false
	if not await click(h,main,"dialogue_next") or not await click(h,main,"dialogue_choose","leave"): return false
	return check(h,main.kernel.interaction_view().get("phase")=="scene","continued_original_outcome_to_end") and conserved(h,before,state(h,main),"outcome_continuation")
