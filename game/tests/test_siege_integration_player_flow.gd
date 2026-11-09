## 唯一联合节点的真实Main整编流程；调用方拥有进程、固定库及银行，本片只驱动正常产品控件。
extends RefCounted
const Life=preload("res://tests/test_regional_life_player_flow.gd")
const Flow=preload("res://tests/test_dynamic_world_player_flow.gd")
const SiegeFlow=preload("res://tests/test_siege_engine_player_flow.gd")
const Population=preload("res://tests/test_population_military_player_flow.gd")

## 断言进入同一联合回执；失败保留实际世界，不安装测试快照或改写NPC政策。
static func check(harness,value: bool,label: String) -> bool:
	harness.rows.append({"stage":"siege_integration_"+label,"ok":value})
	if not value: harness.failures.append("siege_integration_"+label)
	return value

## 有限页始终重新获取当前来源，读取不增加命令序号或推进行军。
static func page(harness,main) -> Dictionary:
	if not check(harness,await harness.settled(main),"page_settled"): return {}
	var source: Dictionary=main.kernel.source_identity()
	var sequence: int=main.kernel.command_context().next_request_seq
	var current: Dictionary=main.kernel.siege_page()
	if not check(harness,current.get("ok",false) and not current.get("siege_id","").is_empty(),"real_siege_page"): return {}
	if not check(harness,main.kernel.source_identity()==source and main.kernel.command_context().next_request_seq==sequence,"page_read_only"): return {}
	return current

## 只改变活营务页的现有数量控件；数值仍由正式整军报价核公款、公粮和容量。
static func quantity(harness,main,name: String,value: int) -> bool:
	var current: Variant=main.find_child(name,true,false)
	if not check(harness,is_instance_valid(current) and current is SpinBox and current.is_inside_tree(),"quantity_"+name): return false
	current.value=value
	return await harness.settled(main)

## 正常剧本选择后走真人任官、受令、编组与行军；作者输入只提供真实实体和数量。
static func start_to_siege(harness,main,ids: Dictionary) -> bool:
	if not await Life.start_selected(harness,main,ids): return false
	if main.kernel.presentation_location().get("place_id")!=ids.camp_place_id:
		if not await Life.click(harness,main,"move_local",ids.camp_place_id): return false
	if not await Life.click(harness,main,"camp_open"): return false
	if not await Life.click(harness,main,"office_accept",ids.office_id): return false
	if not await Flow.finish_action(harness,main,"office_accept",ids.deadline_tick): return false
	if not await Life.click(harness,main,"campaign_accept",ids.order_template_id): return false
	if not await Life.click(harness,main,"camp_tab","formation"): return false
	for raw_id in ids.attacker_unit_ids:
		var id: String=String(raw_id)
		var choice: CheckBox=Population.unit_control(main.page,id)
		if not check(harness,choice!=null and not choice.disabled,"actual_muster_unit_"+id): return false
		if not choice.button_pressed: choice.button_pressed=true
		if not await harness.settled(main): return false
	if not await quantity(harness,main,"CampFoodQuantity",ids.food_quantity) or not await quantity(harness,main,"CampMoneyQuantity",ids.money): return false
	var form: Button=Life.button(main.page,"army_form")
	if not check(harness,form!=null and not form.disabled,"formation_real_quote"): return false
	var campaign_id: String=String(form.get_meta("target_id",""))
	if not await Life.click(harness,main,"army_form",campaign_id): return false
	if not await Flow.finish_action(harness,main,"army_form",ids.deadline_tick): return false
	if not await Life.click(harness,main,"camp_close"): return false
	if main.kernel.presentation_location().get("place_id")!=ids.exit_place_id:
		if not await Life.click(harness,main,"move_local",ids.exit_place_id): return false
	if not await Life.click(harness,main,"camp_open") or not await Life.click(harness,main,"camp_tab","orders"): return false
	if not await Life.click(harness,main,"campaign_depart",campaign_id): return false
	if not await Flow.finish_action(harness,main,"travel",ids.deadline_tick): return false
	if main.journey_view.opened and not await Life.click(harness,main,"map_close"): return false
	if SiegeFlow.screen(main)==null:
		# 围城入口属于场所导航；营务页只办军令和归营，须先用实际关闭按钮回到城门。
		var entry_source: Dictionary=main.kernel.source_identity()
		if main._military_open and not await Life.click(harness,main,"camp_close"): return false
		var quote: Dictionary=await Flow.preview(harness,main,"siege_start",{"campaign_id":campaign_id})
		var entry: Button=Life.button(main.page,"siege_start",campaign_id)
		harness.rows.append({"stage":"siege_integration_actual_place_entry","source":main.kernel.source_identity(),"campaign_id":campaign_id,"quote":quote.duplicate(true),"button_present":entry!=null,"button_disabled":entry.disabled if entry!=null else true})
		if not check(harness,main.kernel.source_identity()==entry_source and quote.get("ok",false),"same_tick_real_siege_entry_quote"): return false
		if not await Life.click(harness,main,"siege_start",campaign_id): return false
	var current: Dictionary=await page(harness,main)
	return check(harness,not current.is_empty() and current.siege.phase=="active" and current.integration.enabled,"normal_main_active_siege")

## 每轮逐部通过地图选点和正式下拉布置封锁；不能以模拟粮荒或单改士气替代真实围困。
static func blockade_round(harness,main,ids: Dictionary) -> bool:
	var before: Dictionary=await page(harness,main)
	if not check(harness,not before.is_empty() and before.siege.phase=="active" and before.siege.pending_node_ids.is_empty(),"blockade_without_unresolved_battle"): return false
	for raw_id in ids.attacker_unit_ids:
		var id: String=String(raw_id)
		var view=SiegeFlow.screen(main)
		if not check(harness,is_instance_valid(view) and view.siege.unit_nodes.has(id),"blockade_actual_unit_"+id): return false
		view.canvas.unit_chosen.emit(id)
		if not await harness.settled(main): return false
		view=SiegeFlow.screen(main)
		if not check(harness,is_instance_valid(view) and view.selected_id==id and view.inspected_id==id,"blockade_selected_actual_unit_"+id): return false
		if not await SiegeFlow.target(harness,main,ids.supply_node_id) or not await SiegeFlow.choose(harness,main,"SiegeAction","blockade"): return false
	var ready=SiegeFlow.screen(main)
	if not check(harness,is_instance_valid(ready),"blockade_draft_page_present"): return false
	harness.rows.append({"stage":"siege_integration_blockade_draft","source":main.kernel.source_identity(),"selected_id":ready.selected_id,"orders":ready.orders.duplicate(true)})
	for raw_id in ids.attacker_unit_ids:
		var id: String=String(raw_id)
		if not check(harness,ready.orders.get(id,{})=={"action":"blockade","node_id":String(ids.supply_node_id)},"blockade_exact_order_"+id): return false
	var tick: int=main.kernel.source_identity().tick
	if not await Life.click(harness,main,"siege_submit",before.siege_id): return false
	return check(harness,main.kernel.source_identity().tick==tick+1,"blockade_one_real_halfday")

## 困城资格只由原预览裁定；未达真实饥饿和士气门槛时继续合法封锁，期限到即失败。
static func reach_negotiation(harness,main,ids: Dictionary) -> bool:
	while main.kernel.source_identity().tick<ids.deadline_tick:
		var current: Dictionary=await page(harness,main)
		if current.is_empty(): return false
		var quote: Dictionary=await Flow.preview(harness,main,"siege_strategy",{"siege_id":String(current.siege_id),"command":{"kind":"negotiate","terms":"integrate"}})
		if quote.get("ok",false): return true
		if not check(harness,quote.get("code") in ["SiegeNegotiationNotReady","SiegeTermsRejected","ParticipantBusy","ActorBusy"],"bounded_real_negotiation_gate_"+quote.get("code","")): return false
		if not await blockade_round(harness,main,ids): return false
	return check(harness,false,"negotiation_deadline")

## 提议按钮只产生不可变提议；没有真实决定或资产凭据时必须保持围城active。
static func propose(harness,main) -> Dictionary:
	if not await SiegeFlow.choose(harness,main,"SiegeTerms","integrate"): return {}
	var before: Dictionary=await page(harness,main)
	if before.is_empty() or not await Life.click(harness,main,"siege_negotiate",before.siege_id): return {}
	var current: Dictionary=await page(harness,main)
	var integration: Dictionary=current.get("integration",{})
	var proposal: Dictionary=integration.get("proposal",{})
	if not check(harness,not proposal.is_empty() and integration.get("status")=="pending" and current.siege.phase=="active" and current.siege.result.is_empty() and integration.get("completion_source_id","").is_empty(),"proposal_does_not_occupy"): return {}
	if not check(harness,proposal.required_officers.size()>=2 and not proposal.terms.population_policy_ids.is_empty(),"nontrivial_officers_and_registered_service"): return {}
	return {"proposal":proposal.duplicate(true),"page":current.duplicate(true),"source":main.kernel.source_identity()}

## 保存真实提议和部分决定各一个银行边界；读取、关闭菜单及重绘都不能制造第二次接受。
static func save_partial(harness,main,ids: Dictionary,slot: String="manual-2") -> Dictionary:
	var boundary: Dictionary=await propose(harness,main)
	if boundary.is_empty(): return {}
	if not await Life.save_via_menu(harness,main,ids.get("proposal_slot","manual-1")): return {}
	if not await blockade_round(harness,main,ids): return {}
	var current: Dictionary=await page(harness,main)
	var integration: Dictionary=current.get("integration",{})
	var accepted: int=0; var pending: int=0
	for row in integration.get("officers",[]):
		if row.get("decision")=="accept": accepted+=1
		elif row.get("decision")=="pending": pending+=1
	if not check(harness,accepted>0 and pending>0 and integration.get("status")=="pending" and integration.get("proposal",{})==boundary.proposal and current.siege.phase=="active","actual_partial_decisions"): return {}
	boundary["partial_page"]=current.duplicate(true); boundary["partial_source"]=main.kernel.source_identity(); boundary["slot"]=slot
	if not await Life.save_via_menu(harness,main,slot): return {}
	if not await stale_callbacks(harness,main): return {}
	harness.rows.append({"stage":"siege_integration_saved_partial","boundary":boundary.duplicate(true)})
	return boundary

## 在原控件释放之后调用旧信号连接，必须安静拒绝且不改命令序号、提议或当前来源。
static func stale_callbacks(harness,main) -> bool:
	var view=SiegeFlow.screen(main)
	if not check(harness,is_instance_valid(view),"stale_screen_exists"): return false
	var control: Button=Life.button(view,"siege_integration_cancel")
	if not check(harness,control!=null and not control.disabled,"stale_cancel_available"): return false
	var callbacks: Array=control.pressed.get_connections()
	main._render()
	if not await harness.settled(main): return false
	var source: Dictionary=main.kernel.source_identity(); var sequence: int=main.kernel.command_context().next_request_seq
	for callback in callbacks:
		if callback.callable.is_valid(): callback.callable.call()
	if not await harness.settled(main): return false
	return check(harness,source==main.kernel.source_identity() and sequence==main.kernel.command_context().next_request_seq,"freed_callbacks_cannot_cancel_or_accept")

## 新进程只走正常继续与槽确认；围城页优先于地图，不点击被其遮蔽的地图关闭按钮。
static func continue_partial(harness,main,ids: Dictionary,boundary: Dictionary) -> bool:
	if not await Life.continue_empty_title(harness,main,ids,boundary.slot,false): return false
	var current: Dictionary=await page(harness,main)
	return check(harness,not current.is_empty() and current.get("integration",{})==boundary.partial_page.integration and current.siege==boundary.partial_page.siege,"fresh_title_restores_exact_partial")

## 所有真人决定齐备后才点击最终交接；拒绝分支必须原军围城继续，不能靠更改政策求成功。
static func finish_decisions(harness,main,ids: Dictionary,expect_refusal: bool=false) -> Dictionary:
	while main.kernel.source_identity().tick<ids.deadline_tick:
		var current: Dictionary=await page(harness,main)
		if current.is_empty(): return {}
		var integration: Dictionary=current.integration
		if integration.status=="rejected":
			if not check(harness,expect_refusal and current.siege.phase=="active" and current.siege.result.is_empty() and integration.completion_source_id.is_empty(),"real_refusal_preserves_siege"): return {}
			var refuses: Array=integration.officers.filter(func(row): return row.decision=="refuse" and not row.get("decision_source_id","").is_empty() and not row.get("actor_action_source_id","").is_empty())
			if not check(harness,not refuses.is_empty(),"actual_officer_refusal_source"): return {}
			return {"refused":true,"page":current}
		if not check(harness,integration.status=="pending","attempt_pending_before_apply"): return {}
		var all_accepted: bool=true
		for officer in integration.officers:
			if officer.decision!="accept": all_accepted=false
		if all_accepted:
			if not check(harness,not expect_refusal and integration.asset_quote.get("ok",false),"all_actual_acceptances_and_assets_ready"): return {}
			if not await Life.click(harness,main,"siege_integration_apply",integration.proposal.id): return {}
			var completed: Dictionary=await page(harness,main)
			if not check(harness,completed.siege.phase=="result" and completed.integration.status=="completed" and not completed.integration.completion_source_id.is_empty(),"actual_complete_handover"): return {}
			var transfer: Dictionary=completed.integration.get("completed_transfer",{})
			if not check(harness,transfer.get("id")==completed.integration.completion_source_id and transfer.get("recipient_units_after",[]).size()>transfer.get("recipient_units_before",[]).size() and not transfer.get("population_receipt_ids",[]).is_empty(),"real_added_units_and_population_continuity"): return {}
			return {"refused":false,"page":completed}
		if not await blockade_round(harness,main,ids): return {}
	check(harness,false,"decision_deadline"); return {}

## 已接收军团沿既有军令真实返程、营地交接和回报；途中时间与补给由原行旅规则消费。
static func march_and_return(harness,main,ids: Dictionary,completed: Dictionary) -> bool:
	var proposal: Dictionary=completed.page.integration.proposal
	var army_id: String=proposal.recipient.id
	var campaign_id: String=proposal.campaign_id
	if not await Life.click(harness,main,"siege_close",proposal.siege_id): return false
	# 围城关闭后恢复原地图视图，先走其真实关闭入口再寻找地方军营。
	if main.journey_view.opened and not await Life.click(harness,main,"map_close"): return false
	if not main._military_open and not await Life.click(harness,main,"camp_open"): return false
	if not await Life.click(harness,main,"camp_tab","orders"): return false
	var returning: Button=Life.button(main.page,"campaign_return",campaign_id)
	if returning!=null and returning.disabled and returning.get_meta("camp_quote",{}).get("code")=="NotAtCityExit":
		if not await Life.click(harness,main,"camp_close") or not await Life.click(harness,main,"move_local",ids.target_exit_place_id): return false
		if not await Life.click(harness,main,"camp_open") or not await Life.click(harness,main,"camp_tab","orders"): return false
	var before: int=main.kernel.source_identity().tick
	if not await Life.click(harness,main,"campaign_return",campaign_id): return false
	if not check(harness,main.kernel.action_context().get("action_kind")=="travel","received_army_actual_march_started"): return false
	if not await Flow.finish_action(harness,main,"travel",ids.deadline_tick): return false
	if not check(harness,main.kernel.source_identity().tick>before,"received_army_march_consumed_real_time"): return false
	if main.journey_view.opened and not await Life.click(harness,main,"map_close"): return false
	if main._military_open and not await Life.click(harness,main,"camp_close"): return false
	if main.kernel.presentation_location().get("place_id")!=ids.camp_place_id:
		if not await Life.click(harness,main,"move_local",ids.camp_place_id): return false
	if not await Life.click(harness,main,"camp_open") or not await Life.click(harness,main,"camp_tab","supply"): return false
	if not await Life.click(harness,main,"army_return",army_id): return false
	if not await Flow.finish_action(harness,main,"army_return",ids.deadline_tick): return false
	if not await Life.click(harness,main,"camp_tab","orders") or not await Life.click(harness,main,"campaign_report",campaign_id): return false
	var current: Dictionary=await Population.camp_page(harness,main,"orders")
	if not check(harness,not current.is_empty(),"received_army_real_return_page"): return false
	for campaign in current.get("campaigns",[]):
		if campaign.campaign_id==campaign_id: return check(harness,false,"reported_campaign_still_open")
	var repeated: Dictionary=await Flow.preview(harness,main,"campaign_report",{"campaign_id":campaign_id})
	return check(harness,current.get("own_army_count",-1)==0 and repeated.get("code")=="CampaignClosed","received_army_campaign_closed_after_report")
