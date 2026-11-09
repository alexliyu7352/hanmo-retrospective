## 唯一地方生活实际Main流程片；调用方拥有进程与证据，本片只驱动当前产品控件。
extends RefCounted
const JourneyFlow=preload("res://tests/test_dynamic_world_player_flow.gd")

## 所有判定交同一联合harness，失败不改世界或补造成功来源。
static func check(harness, value: bool, label: String) -> bool:
	harness.rows.append({"stage":"regional_life_"+label,"ok":value})
	if not value: harness.failures.append("regional_life_"+label)
	return value

## 只重新查找活语义控件，不依赖中文标签、旧Node或第一位演员。
static func button(root: Node, kind: String, target: String="") -> Button:
	if root is Button and not root.is_queued_for_deletion() and root.get_meta("command_kind","")==kind and (target.is_empty() or root.get_meta("target_id","")==target): return root
	for child in root.get_children():
		var found: Button=button(child,kind,target)
		if found!=null: return found
	return null

## 稳定等待由调用方统一拥有，点击不会私自推进世界或隐藏额外命令。
static func click(harness, main, kind: String, target: String="") -> bool:
	var current: Button=button(main.page,kind,target)
	if not check(harness,current!=null and current.is_inside_tree() and current.is_visible_in_tree() and not current.disabled,"click_"+kind+"_"+target): return false
	current.pressed.emit()
	return check(harness,await harness.settled(main),"settled_"+kind)

## 新局必须经过正常已安装剧本选择；显式命令行绕过列表不算本项通过。
static func start_selected(harness, main, ids: Dictionary) -> bool:
	if not check(harness,not main._started and not main._resident_mode,"empty_title_before_start"): return false
	var start: Button=button(main.page,"start")
	if not check(harness,start!=null and not start.disabled,"normal_start_control"): return false
	start.pressed.emit()
	var window: Window=main._scenario_dialog
	if not check(harness,is_instance_valid(window) and window.visible and not main._started,"normal_installed_scenario_picker"): return false
	var selected: Button=_scenario_button(window,ids)
	if not check(harness,selected!=null and not selected.disabled,"exact_installed_scenario_actor_pack"): return false
	selected.pressed.emit()
	if not check(harness,await harness.settled(main),"selected_native_world_ready"): return false
	return check(harness,main._resident_mode and main.kernel.content().get("pack_set_digest")==ids.pack_set_digest and main.kernel.command_context().get("actor_id")==ids.player_actor_id,"normal_picker_preserves_exact_binding")

## 剧本ID、可控人物和包锁共同选择，不能因为同名或另一个后继包而误启程。
static func _scenario_button(root: Node, ids: Dictionary) -> Button:
	if root is Button and root.get_meta("command_kind","")=="scenario_select" and root.get_meta("target_id","")==ids.scenario_id and root.get_meta("player_actor_id","")==ids.player_actor_id and root.get_meta("pack_set_digest","")==ids.pack_set_digest: return root
	for child in root.get_children():
		var selected: Button=_scenario_button(child,ids)
		if selected!=null: return selected
	return null

## 只读取实际人物名单中的有限行；冷世界导出不能替代玩家已看到的近况。
static func visible_rows(root: Node) -> Array:
	var rows: Array=[]
	if root.has_meta("regional_row"): rows.append(root.get_meta("regional_row").duplicate(true))
	for child in root.get_children(): rows.append_array(visible_rows(child))
	return rows

## 记录只读观察的身份与序号不变式，不从出现文字推断工资或任务已经结算。
static func observe_place(harness, main, label: String) -> Dictionary:
	if not check(harness,await harness.settled(main),label+"_ready"): return {}
	var source: Dictionary=main.kernel.source_identity()
	var sequence: int=main.kernel.command_context().next_request_seq
	var rows: Array=visible_rows(main.page)
	if not check(harness,source==main.kernel.source_identity() and sequence==main.kernel.command_context().next_request_seq,label+"_read_only"): return {}
	var observation: Dictionary={"source":source,"place_id":main.kernel.presentation_location().get("place_id",""),"rows":rows}
	harness.rows.append({"stage":"regional_life_"+label,"observation":observation.duplicate(true)})
	return observation

## 成功打开前核exact来源动作；异步报价由实际页面预取并重新获取活按钮。
static func open_topic(harness, main, topic_id: String) -> bool:
	if not await harness.settled(main): return false
	var current: Button=button(main.page,"talk_actor",topic_id)
	if current==null: current=button(main.page,"talk",topic_id)
	if not check(harness,current!=null and not current.disabled and current.has_meta("regional_action") and current.get_meta("regional_source",{})==main.kernel.source_identity(),"topic_exact_current_"+topic_id): return false
	var action: Dictionary=current.get_meta("regional_action").duplicate(true)
	var source: Dictionary=main.kernel.source_identity()
	var quote: Dictionary=main.kernel.preview(action.command,action.params)
	if quote.get("code")=="NativeAsyncRequired":
		if not await harness.settled(main): return false
		quote=main.kernel.preview(action.command,action.params)
	if not check(harness,quote.get("ok",false) and main.kernel.source_identity()==source,"topic_quote_current"): return false
	current=button(main.page,action.command,topic_id)
	if not check(harness,current!=null and current.get_meta("regional_action",{})==action,"topic_not_replaced_after_quote"): return false
	return await click(harness,main,action.command,topic_id)

## 作者的接受和稍后选项共用原say/choice/action流程；不能从helper直接提交任务。
static func choose_topic(harness, main, topic_id: String, option_id: String) -> bool:
	if not await open_topic(harness,main,topic_id): return false
	for step in range(16):
		var view: Dictionary=main.kernel.interaction_view()
		if view.get("phase")=="say":
			if not await click(harness,main,"dialogue_next"): return false
		elif view.get("phase")=="choice":
			if not await click(harness,main,"dialogue_choose",option_id): return false
			if main.kernel.interaction_view().get("phase")=="action": return await click(harness,main,"dialogue_confirm")
			return true
		else: return check(harness,false,"unexpected_topic_phase_"+view.get("phase",""))
	return check(harness,false,"authored_dialogue_step_limit")

## 关闭只走原会话出口；同一事实可再观察，不能把取消展示当作世界需求清算。
static func close_conversation(harness, main) -> bool:
	if main.kernel.interaction_view().get("phase")=="scene": return true
	var before_tick: int=main.kernel.source_identity().tick
	if not await click(harness,main,"end_interaction"): return false
	return check(harness,main.kernel.interaction_view().get("phase")=="scene" and main.kernel.source_identity().tick==before_tick,"close_without_world_tick")

## 只在期限内显式授一半日；结算先后由同一个原native时钟决定。
static func wait_halfday(harness, main, deadline_tick: int) -> bool:
	var before: int=main.kernel.source_identity().tick
	if not check(harness,before<deadline_tick,"explicit_wait_within_bound"): return false
	var kind: String="advance" if not main.kernel.action_context().get("action_id","").is_empty() else "wait"
	var quote: Dictionary=main.kernel.preview(kind,{})
	if quote.get("code")=="NativeAsyncRequired":
		if not await harness.settled(main): return false
		quote=main.kernel.preview(kind,{})
	if not check(harness,quote.get("ok",false),"legal_explicit_halfday"): return false
	main._perform(kind,{})
	return check(harness,await harness.settled(main) and main.kernel.source_identity().tick==before+1,"one_actual_halfday")

## 按实际在场人物未完活计寻找保存边界，不假定特定tick或改NPC占用。
static func reach_save_boundary(harness, main, ids: Dictionary) -> Dictionary:
	if main.kernel.presentation_location().get("place_id")!=ids.work_place_id:
		if not await click(harness,main,"move_local",ids.work_place_id): return {}
	while main.kernel.source_identity().tick<ids.deadline_tick:
		var observation: Dictionary=await observe_place(harness,main,"save_boundary_candidate")
		if observation.is_empty(): return {}
		for row in observation.rows:
			if row.get("kind")=="actor" and row.get("actor_id") in ids.livelihood_actor_ids and row.get("activity")=="service":
				return {"observer_source":observation.source,"place_id":observation.place_id,"actor_id":row.actor_id,"activity":row.activity,"rows":observation.rows}
		if not await wait_halfday(harness,main,ids.deadline_tick): return {}
	check(harness,false,"real_unfinished_livelihood_boundary_missing"); return {}

## 真菜单的保存按钮打开原槽窗口；不直接调用存储器或内部_open_slots便捷函数。
static func save_via_menu(harness, main, slot: String="manual-1") -> bool:
	var before: Dictionary=main.kernel.source_identity()
	var menu: Button=button(main.page,"system_menu")
	if not check(harness,menu!=null and not menu.disabled,"system_menu_exists"): return false
	menu.pressed.emit()
	var save: Button=button(main.page,"save")
	if not check(harness,save!=null and save.is_visible_in_tree() and not save.disabled,"visible_menu_save"): return false
	save.pressed.emit()
	if not await _wait_slot_window(harness,main): return false
	if not _confirm_slot(harness,main,slot): return false
	return check(harness,await harness.settled(main) and main.last_storage_result.get("ok",false) and main._selected_slot==slot and main.kernel.source_identity()==before,"physical_menu_save_preserves_source")

## consumer必须从新进程空标题点击地方继续；列槽前后均不能偷偷创建默认世界。
static func continue_empty_title(harness, main, ids: Dictionary, slot: String="manual-1", close_map: bool=true) -> bool:
	if not check(harness,not main._started and not main._resident_mode and main.kernel.snapshot().is_empty(),"fresh_consumer_empty_title"): return false
	var resume: Button=button(main.page,"continue_dynamic")
	if not check(harness,resume!=null and not resume.disabled,"normal_continue_dynamic"): return false
	resume.pressed.emit()
	if not await _wait_slot_window(harness,main): return false
	if not check(harness,not main._started and not main._resident_mode and not is_instance_valid(main._resident_load_candidate) and main.kernel.snapshot().is_empty(),"listing_does_not_install_world"): return false
	if not _confirm_slot(harness,main,slot) or not await harness.settled(main): return false
	if not check(harness,main.last_storage_result.get("ok",false) and main.kernel.content().get("pack_set_digest")==ids.pack_set_digest and not main._resident_adapter.auto_running,"exact_checkpoint_restored_without_auto"): return false
	if close_map and main.journey_view.opened and not await click(harness,main,"map_close"): return false
	return true

## 槽窗口异步等待只消费SceneTree帧，不启动世界或授予半日。
static func _wait_slot_window(harness, main) -> bool:
	var deadline: int=Time.get_ticks_usec()+30000000
	while Time.get_ticks_usec()<deadline:
		if is_instance_valid(main._slot_dialog) and main._slot_dialog.visible: return true
		await harness.process_frame
	return check(harness,false,"slot_window_timeout")

## 选择与确认仍分别走产品按钮；存档银行CAS由原产品完成。
static func _confirm_slot(harness, main, slot: String) -> bool:
	var choice: Button=button(main._slot_dialog,"slot_select",slot)
	if not check(harness,choice!=null and not choice.disabled,"slot_selectable"): return false
	choice.pressed.emit()
	var confirm: Button=button(main._slot_dialog,"slot_confirm")
	if not check(harness,confirm!=null and not confirm.disabled,"slot_confirmation_ready"): return false
	confirm.pressed.emit(); return true

## 保存前后两进程复用同一剩余行为；指定旧来源时，必须等到真正新的结果才能结束。
static func continue_from_boundary(harness, main, ids: Dictionary, boundary: Dictionary) -> bool:
	if not check(harness,main.kernel.presentation_location().get("place_id")==boundary.place_id,"continuation_same_actual_place"): return false
	var followed: bool=false
	while main.kernel.source_identity().tick<ids.deadline_tick:
		var topic: Button=button(main.page,"talk_actor",ids.work_topic_id)
		var current_source: Dictionary=topic.get_meta("regional_row",{}) if topic!=null else {}
		var old_fact: String=ids.get("exclude_source_fact_id","")
		var old_report: String=ids.get("exclude_source_report_id","")
		var new_source: bool=old_fact.is_empty() or (not current_source.get("source_fact_id","").is_empty() and current_source.get("source_fact_id")!=old_fact and not current_source.get("source_report_id","").is_empty() and current_source.get("source_report_id")!=old_report)
		if topic!=null and not topic.disabled and new_source:
			harness.rows.append({"stage":"regional_life_new_result_candidate","previous_fact_id":old_fact,"candidate":current_source.duplicate(true),"source":main.kernel.source_identity()})
			if not await open_topic(harness,main,ids.work_topic_id): return false
			if not check(harness,main.find_child("RegionalDialogueSource",true,false)!=null,"actual_dated_source_in_dialogue"): return false
			return await close_conversation(harness,main)
		var present: bool=false
		for row in visible_rows(main.page):
			if row.get("kind")=="actor" and row.get("actor_id")==boundary.actor_id: present=true
		if not present and not followed:
			if not await click(harness,main,"move_local",ids.follow_exit_place_id): return false
			if not await JourneyFlow.travel(harness,main,ids.follow_settlement_id): return false
			if main.kernel.presentation_location().get("place_id")!=ids.follow_place_id:
				if not await click(harness,main,"move_local",ids.follow_place_id): return false
			followed=true
			continue
		if not await wait_halfday(harness,main,ids.deadline_tick): return false
	return check(harness,false,"continuation_source_not_ready_before_bound")

## 旧callback保留为负例，在同一来源重绘后不能产生新的世界命令或更换选择。
static func check_stale_choice(harness, main, topic_id: String) -> bool:
	var old: Button=button(main.page,"talk_actor",topic_id)
	if old==null: old=button(main.page,"talk",topic_id)
	if not check(harness,old!=null and not old.disabled,"stale_test_has_real_control"): return false
	var calls: Array=old.pressed.get_connections()
	main._render()
	if not await harness.settled(main): return false
	var source: Dictionary=main.kernel.source_identity()
	var seq: int=main.kernel.command_context().next_request_seq
	for connection in calls:
		var callback: Callable=connection.callable
		if callback.is_valid(): callback.call()
	if not await harness.settled(main): return false
	return check(harness,source==main.kernel.source_identity() and seq==main.kernel.command_context().next_request_seq and main.kernel.interaction_view().get("phase")=="scene","stale_callback_never_submits")

## 原商户主题进入真实货单；只有明确忙碌才在期限内等待，不能临时更换办理人。
static func open_merchant_trade(harness, main, ids: Dictionary) -> bool:
	if not await JourneyFlow.quoted_click(harness,main,"talk",{"role_id":ids.merchant_role},ids.deadline_tick,ids.merchant_role): return false
	if not await JourneyFlow.quoted_click(harness,main,"dialogue_next",{},ids.deadline_tick): return false
	if not await JourneyFlow.quoted_click(harness,main,"dialogue_choose",{"option_id":"trade"},ids.deadline_tick,"trade"): return false
	if not await JourneyFlow.quoted_click(harness,main,"dialogue_confirm",{},ids.deadline_tick): return false
	return check(harness,main.kernel.interaction_view().get("phase")=="trade","ordinary_merchant_trade_open")

## 只以在场空闲者与原货单零库存寻找介入边界；不读取隐蔽计划来替玩家作选择。
static func wait_for_scarcity(harness, main, ids: Dictionary) -> Dictionary:
	while main.kernel.source_identity().tick<ids.deadline_tick:
		var observed: Dictionary=await observe_place(harness,main,"scarcity_candidate")
		if observed.is_empty(): return {}
		var waiting: Dictionary={}
		var prior_source: Dictionary={}
		for row in observed.rows:
			if row.get("kind")=="actor" and row.get("actor_id")==ids.scarcity_actor_id and row.get("activity")=="idle": waiting=row
			if row.get("topic_id")==ids.scarcity_topic_id and row.get("actor_id")==ids.scarcity_actor_id and not row.get("source_fact_id","").is_empty(): prior_source=row.duplicate(true)
		if not waiting.is_empty():
			if not await open_merchant_trade(harness,main,ids): return {}
			var sheet: Dictionary=main.kernel.trade_view(ids.shop,ids.merchant)
			if sheet.get("code")=="NativeAsyncRequired":
				if not await harness.settled(main): return {}
				sheet=main.kernel.trade_view(ids.shop,ids.merchant)
			var stock: int=-1
			for item in sheet.get("items",[]):
				if item.get("item_id")==ids.item_id: stock=item.stock
			var draft: Dictionary=main._regional_draft()
			var local: Dictionary=main.kernel.regional_life_topics(draft.place_id,draft.cursor)
			if local.get("code")=="NativeAsyncRequired":
				if not await harness.settled(main): return {}
				local=main.kernel.regional_life_topics(draft.place_id,draft.cursor)
			var still_waiting: bool=false
			for person in local.get("rows",[]):
				if person.get("kind")=="actor" and person.get("actor_id")==ids.scarcity_actor_id and person.get("activity")=="idle": still_waiting=true; waiting=person
			# 进货单若曾因忙碌经过半日，就回场景重新取得来源，不用旧观察冒充卖出前身份。
			if stock==0 and still_waiting and observed.source.tick==main.kernel.source_identity().tick and not prior_source.is_empty():
				var boundary: Dictionary={"actor_id":ids.scarcity_actor_id,"place_id":observed.place_id,"shop_id":ids.shop,"item_id":ids.item_id,"stock":stock,"source":main.kernel.source_identity(),"visible_actor":waiting.duplicate(true),"pre_sale_topic_source":prior_source.duplicate(true),"pre_sale_observation_source":observed.source.duplicate(true)}
				harness.rows.append({"stage":"regional_life_actual_empty_market","boundary":boundary.duplicate(true)})
				var on_scarcity: Callable=ids.get("on_scarcity",Callable())
				if on_scarcity.is_valid() and not await on_scarcity.call(boundary.duplicate(true)): return {}
				return boundary
			if not await close_conversation(harness,main): return {}
		if not await wait_halfday(harness,main,ids.deadline_tick): return {}
	check(harness,false,"visible_scarcity_boundary_not_reached"); return {}

## 玩家只出售自己行囊中一份粮，使用原货单的卖出/个人钱袋/正式报价和真实回执。
static func sell_owned_grain(harness, main, ids: Dictionary) -> bool:
	if not check(harness,main.kernel.interaction_view().get("phase")=="trade","sale_uses_actual_trade_session"): return false
	var sheet: Dictionary=main.kernel.trade_view(ids.shop,ids.merchant)
	if sheet.get("code")=="NativeAsyncRequired":
		if not await harness.settled(main): return false
		sheet=main.kernel.trade_view(ids.shop,ids.merchant)
	var item_index: int=-1; var payer_index: int=-1
	for index in range(main._item.item_count):
		if main._item.get_item_metadata(index)==ids.item_id: item_index=index
	for index in range(main._payer.item_count):
		if main._payer.get_item_metadata(index)==sheet.personal_wallet_id: payer_index=index
	if not check(harness,item_index>=0 and payer_index>=0,"owned_grain_and_personal_payer_controls"): return false
	main._item.select(item_index); main._payer.select(payer_index); main._side.select(1); main._quantity.value=1; main._quote_changed(null)
	if not await harness.settled(main): return false
	var request: Dictionary=main._trade_request.duplicate(true)
	var quote: Dictionary=await JourneyFlow.preview(harness,main,"trade",request)
	if not check(harness,quote.get("ok",false) and request.get("side")=="sell" and request.get("quantity")==1 and request.get("payer_wallet_id")==sheet.personal_wallet_id and request.get("item_id")==ids.item_id,"one_owned_grain_sale_is_legal"): return false
	if not await click(harness,main,"trade"): return false
	if not await JourneyFlow.finish_action(harness,main,"trade",ids.deadline_tick): return false
	var receipt: Dictionary=main._trade_receipt.get("quote",{})
	if not check(harness,receipt.get("side")=="sell" and receipt.get("quantity")==1 and receipt.get("item_id")==ids.item_id and main.find_child("TradeReceipt",true,false)!=null,"actual_personal_sale_receipt"): return false
	return await close_conversation(harness,main)

## 两进程共用缺货、真实卖粮和同一人结果续程，不能将工资对白冒充缺货已经解除。
static func continue_scarce_market(harness, main, ids: Dictionary, saved_boundary: Dictionary) -> bool:
	if not check(harness,main.kernel.presentation_location().get("place_id")==saved_boundary.place_id,"scarce_continuation_starts_at_saved_place"): return false
	var scarcity: Dictionary=await wait_for_scarcity(harness,main,ids)
	if scarcity.is_empty() or not await sell_owned_grain(harness,main,ids): return false
	var follow: Dictionary=ids.duplicate(true)
	follow.work_topic_id=ids.scarcity_topic_id; follow.follow_settlement_id=ids.scarcity_return_settlement_id; follow.follow_place_id=ids.scarcity_return_place_id
	follow.exclude_source_fact_id=scarcity.pre_sale_topic_source.source_fact_id
	follow.exclude_source_report_id=scarcity.pre_sale_topic_source.source_report_id
	return await continue_from_boundary(harness,main,follow,scarcity)
