## 唯一联合节点的真实Main动态生活片；只使用调用方世界、原行旅/货单控件与同源有限DTO。
extends RefCounted

## 所有断言进入中央同一回执，失败停在实际页面，不另建GD影子世界。
static func check(harness, value: bool, label: String) -> bool:
	harness.rows.append({"stage":"dynamic_world_"+label,"ok":value})
	if not value: harness.failures.append("dynamic_world_"+label)
	return value

## 遍历当前活控件的语义标记，不复用已被异步刷新移除的旧按钮。
static func button(root: Node, kind: String, target: String="") -> Button:
	if root is Button and root.get_meta("command_kind","")==kind and (target.is_empty() or root.get_meta("target_id","")==target): return root
	for child in root.get_children():
		var found: Button=button(child,kind,target)
		if found!=null: return found
	return null

## 读取场所中实际渲染的有限行；不存在行时明确失败，不从debug世界补齐未知量。
static func rows(root: Node) -> Array:
	var result: Array=[]
	if root.has_meta("dynamic_row"): result.append(root.get_meta("dynamic_row").duplicate(true))
	for child in root.get_children(): result.append_array(rows(child))
	return result

## 点击只走当前可用产品按钮，后续同源pending由中央现有settled协程排空。
static func click(harness, main, kind: String, target: String="") -> bool:
	var current: Button=button(main.page,kind,target)
	if not check(harness,current!=null and not current.disabled and current.is_visible_in_tree(),kind+"_available"): return false
	current.pressed.emit()
	return check(harness,await harness.settled(main),kind+"_settled")

## 纯预览通过原异步读队列完成；保留拒绝原因并核同源，不把pending或正常拒绝当作提交成功。
static func preview(harness, main, kind: String, params: Dictionary={}) -> Dictionary:
	var source: Dictionary=main.kernel.source_identity()
	var quote: Dictionary=main.kernel.preview(kind,params)
	if quote.get("code")=="NativeAsyncRequired":
		if not check(harness,await harness.settled(main),kind+"_preview_settled"): return {}
		quote=main.kernel.preview(kind,params)
	if not check(harness,main.kernel.source_identity()==source and quote.get("code")!="NativeAsyncRequired",kind+"_preview_same_source"): return {}
	harness.rows.append({"stage":"dynamic_world_command_preview","command":kind,"params":params.duplicate(true),"quote":quote.duplicate(true),"source":source})
	return quote

## 只在已知期限内显式等待一半日；岗位忙碌不改NPC、权限或世界，每次之后重新读取真实页面。
static func wait_halfday(harness, main, deadline: int) -> bool:
	var before: int=main.kernel.source_identity()["tick"]
	if not check(harness,before<deadline,"opportunity_deadline_remaining"): return false
	var reopen: bool=main._dynamic_open
	if reopen and not await click(harness,main,"dynamic_close"): return false
	var kind: String="advance" if not main.kernel.action_context().get("action_id","").is_empty() else "wait"
	var quote: Dictionary=await preview(harness,main,kind)
	if not check(harness,quote.get("ok",false),"explicit_halfday_legal_"+quote.get("code","")): return false
	main._perform(kind,{})
	if not check(harness,await harness.settled(main),"explicit_halfday_settled"): return false
	if not check(harness,main.kernel.source_identity()["tick"]==before+1,"explicit_halfday_progress"): return false
	return await click(harness,main,"dynamic_open") if reopen else true

## 提交后只完成该项真实个人行动；期限或额外输入不满足时停止，不假定settled等于任务完成。
static func finish_action(harness, main, kind: String, deadline: int) -> bool:
	while main.kernel.action_context().get("action_kind")==kind:
		if not check(harness,not main.kernel.action_needs_input(),kind+"_no_unhandled_input"): return false
		if not await wait_halfday(harness,main,deadline): return false
	return check(harness,main.kernel.action_context().get("action_id","").is_empty(),kind+"_finished")

## 读取按钮所属的当前动态行；重绘后的旧控件不会被继续持有。
static func dynamic_row(control: Node) -> Node:
	while control!=null:
		if control.has_meta("dynamic_row"): return control
		control=control.get_parent()
	return null

## 承接前只按公开作者政策设等待上限；实际数量与冻结条款始终由同源预览决定。
static func accept_contract(harness, main, scenario_id: String) -> Dictionary:
	var control: Button=button(main.page,"accept_offer")
	if not check(harness,control!=null,"shared_need_still_available"): return {}
	var offered: Dictionary=control.get_meta("dynamic_action",{})
	var offer_id: String=offered.get("params",{}).get("offer_id","")
	var row: Dictionary=dynamic_row(control).get_meta("dynamic_row")
	var policies: Dictionary=main._data.get("scenarios",{}).get(scenario_id,{}).get("dynamic_world",{}).get("affairs",{}).get("need_policies",{})
	var deadline: int=0
	for policy in policies.values():
		if policy.get("accept_role")==offered["params"]["context"]["role_id"] and policy.get("item_id")==row.get("item_id"):
			deadline=main.kernel.source_identity()["tick"]+int(policy["deadline_ticks"])
	if not check(harness,deadline>main.kernel.source_identity()["tick"] and not offer_id.is_empty(),"authored_opportunity_bound"): return {}
	while main.kernel.source_identity()["tick"]<deadline:
		control=button(main.page,"accept_offer",offer_id)
		if not check(harness,control!=null,"no_opportunity_if_claimed_or_closed"): return {}
		var quantity: SpinBox=dynamic_row(control).find_child("DynamicQuantity",true,false)
		if not check(harness,quantity!=null,"actual_dynamic_quantity_control"): return {}
		var maximum: int=int(quantity.max_value)
		var busy: bool=false
		for amount in range(maximum,0,-1):
			control=button(main.page,"accept_offer",offer_id)
			quantity=dynamic_row(control).find_child("DynamicQuantity",true,false)
			quantity.value=amount
			if not check(harness,await harness.settled(main),"quantity_draft_settled"): return {}
			control=button(main.page,"accept_offer",offer_id)
			if not check(harness,control!=null,"fresh_offer_after_quantity"): return {}
			offered=control.get_meta("dynamic_action",{}).duplicate(true)
			if not check(harness,offered["params"]["quantity"]==amount,"rendered_quantity_preserved"): return {}
			var quote: Dictionary=await preview(harness,main,"accept_offer",offered["params"])
			if quote.get("code")=="ActorBusy": busy=true; break
			if quote.get("code") in ["IssuerInsufficientFunds","NeedShareUnavailable"]: continue
			if not check(harness,quote.get("ok",false),"accept_legal_"+quote.get("code","")): return {}
			var terms: Dictionary=quote.get("contract_terms",{})
			if not check(harness,terms.get("quantity")==amount and terms.get("deadline_tick",0)>main.kernel.source_identity()["tick"],"finite_frozen_request"): return {}
			control=button(main.page,"accept_offer",offer_id)
			if not check(harness,control!=null and control.get_meta("dynamic_action",{})==offered,"accept_fresh_action_matches_quote"): return {}
			if not await click(harness,main,"accept_offer",offer_id): return {}
			if not await finish_action(harness,main,"accept_offer",terms["deadline_tick"]): return {}
			for contract in rows(main.page):
				if contract.get("kind")=="contract" and contract.get("offer_id")==offer_id and contract.get("actor_id")==main.kernel.command_context().get("actor_id"):
					if not check(harness,contract.get("agreed_quantity")==amount and contract.get("deadline_tick")==terms["deadline_tick"],"own_frozen_contract_visible"): return {}
					return contract.duplicate(true)
			check(harness,false,"own_contract_visible"); return {}
		if not busy: check(harness,false,"no_affordable_visible_quantity"); return {}
		if not await wait_halfday(harness,main,deadline): return {}
	check(harness,false,"no_opportunity_before_authored_deadline"); return {}

## 原岗位对白入口和原按钮共同决定交易/交付可用性；仅忙碌拒绝允许期限内等待后重报。
static func quoted_click(harness, main, kind: String, params: Dictionary, deadline: int, target: String="") -> bool:
	while true:
		var quote: Dictionary=await preview(harness,main,kind,params)
		if quote.get("code")=="ActorBusy":
			if not await wait_halfday(harness,main,deadline): return false
			continue
		if not check(harness,quote.get("ok",false),kind+"_legal_"+quote.get("code","")): return false
		return await click(harness,main,kind,target)
	return false

## 行旅用原地图选择与确认；逐半日只授予既有行动，途中输入按真实失败边界交回中央。
static func travel(harness, main, destination: String) -> bool:
	main._open_map(); main._map_select(destination)
	if not check(harness,await harness.settled(main),"route_preview"): return false
	var visible_routes: Array=main._data.get("routes",{}).keys(); visible_routes.sort()
	var observed_routes: Array=main.kernel.map_view()["route_open"].keys(); observed_routes.sort()
	if not check(harness,visible_routes==observed_routes,"selected_roads_match_native_visibility"): return false
	if not check(harness,main._map_quote().get("ok",false),"route_legal"): return false
	var before: int=main.kernel.source_identity()["tick"]
	main._map_confirm()
	if not check(harness,await harness.settled(main),"route_started"): return false
	while main.kernel.action_context().get("action_kind")=="travel":
		if not check(harness,not main.kernel.action_needs_input(),"route_no_unhandled_input"): return false
		var prior_tick: int=main.kernel.source_identity()["tick"]
		main._perform("advance",{})
		if not check(harness,await harness.settled(main),"route_advanced"): return false
		if not check(harness,main.kernel.source_identity()["tick"]==prior_tick+1,"route_actual_half_day"): return false
	main._close_map()
	if not check(harness,await harness.settled(main),"route_arrival_page"): return false
	var location: Dictionary=main.kernel.presentation_location()
	return check(harness,location.get("kind")=="Settlement" and main._data.get("places",{}).get(location.get("place_id",""),{}).get("settlement_id")==destination and main.kernel.source_identity()["tick"]>before,"actual_cross_city_arrival")

## 沿原商户对白进入货单，按本人合同选择真实经费；数量依当前原生报价递减，不能假定最大值买得起。
static func purchase(harness, main, ids: Dictionary, contract: Dictionary) -> bool:
	var deadline: int=contract["deadline_tick"]
	if not await quoted_click(harness,main,"talk",{"role_id":ids["merchant_role"]},deadline,ids["merchant_role"]): return false
	if not await quoted_click(harness,main,"dialogue_next",{},deadline): return false
	if not await quoted_click(harness,main,"dialogue_choose",{"option_id":"trade"},deadline,"trade"): return false
	if not await quoted_click(harness,main,"dialogue_confirm",{},deadline): return false
	if not check(harness,main.kernel.interaction_view().get("phase")=="trade","actual_merchant_trade_session"): return false
	var sheet: Dictionary=main.kernel.trade_view(ids["shop"],ids["merchant"])
	if sheet.get("code")=="NativeAsyncRequired":
		if not check(harness,await harness.settled(main),"trade_sheet_settled"): return false
		sheet=main.kernel.trade_view(ids["shop"],ids["merchant"])
	var wallet_id: String=""
	for payer in sheet.get("task_payers",[]):
		if payer.get("offer_id")==contract["offer_id"]: wallet_id=payer["wallet_id"]
	if not check(harness,not wallet_id.is_empty(),"own_contract_payer_visible"): return false
	var remaining: int=contract["agreed_quantity"]-contract.get("delivered_quantity",0)
	var purchased: bool=false
	for amount in range(remaining,0,-1):
		if not check(harness,is_instance_valid(main._item) and is_instance_valid(main._payer) and is_instance_valid(main._quantity),"actual_trade_controls"): return false
		var item_index: int=-1
		var payer_index: int=-1
		for index in range(main._item.item_count):
			if main._item.get_item_metadata(index)==ids["item"]: item_index=index
		for index in range(main._payer.item_count):
			if main._payer.get_item_metadata(index)==wallet_id: payer_index=index
		if not check(harness,item_index>=0 and payer_index>=0,"real_stock_and_contract_choice"): return false
		main._item.select(item_index); main._payer.select(payer_index); main._quantity.value=amount; main._quote_changed(null)
		if not check(harness,await harness.settled(main),"task_budget_quote"): return false
		var request: Dictionary=main._trade_request.duplicate(true)
		var quote: Dictionary=await preview(harness,main,"trade",request)
		if quote.get("code") in ["InsufficientFunds","InsufficientStock","InsufficientCapacity"]: continue
		if not check(harness,quote.get("ok",false),"trade_legal_"+quote.get("code","")): return false
		if not check(harness,request.get("quantity")==amount and request.get("payer_wallet_id")==wallet_id and main._trade_request==request,"fresh_trade_controls_match_quote"): return false
		if not await click(harness,main,"trade"): return false
		if not await finish_action(harness,main,"trade",deadline): return false
		purchased=true; break
	if not check(harness,purchased,"some_actual_cargo_affordable"): return false
	# 成交后沿原页脚结束本人交谈；经办人可以接待其他NPC，不为退出再请求其对白续行或授予半日。
	if main.kernel.interaction_view().get("phase")=="trade":
		var exit_quote: Dictionary=await preview(harness,main,"end_interaction")
		if not check(harness,exit_quote.get("ok",false),"trade_return_legal_"+exit_quote.get("code","")): return false
		if not await click(harness,main,"end_interaction"): return false
	return check(harness,main.kernel.interaction_view().get("phase")=="scene","returned_from_actual_trade_session")

## 回报只操作当前本人合同的实际货物按钮；忙碌后重取行和报价，过期或无货明确停止。
static func report_contract(harness, main, contract: Dictionary) -> bool:
	var task_id: String=contract["task_id"]
	while main.kernel.source_identity()["tick"]<contract["deadline_tick"]:
		var control: Button=button(main.page,"report_offer",task_id)
		if not check(harness,control!=null,"actual_contract_cargo_report_visible"): return false
		var action: Dictionary=control.get_meta("dynamic_action",{}).duplicate(true)
		var quote: Dictionary=await preview(harness,main,"report_offer",action["params"])
		if quote.get("code")=="ActorBusy":
			if not await wait_halfday(harness,main,contract["deadline_tick"]): return false
			continue
		if not check(harness,quote.get("ok",false) and not quote.get("restitution",false),"actual_delivery_legal_"+quote.get("code","")): return false
		control=button(main.page,"report_offer",task_id)
		if not check(harness,control!=null and control.get_meta("dynamic_action",{})==action,"report_fresh_action_matches_quote"): return false
		if not await click(harness,main,"report_offer",task_id): return false
		if not await finish_action(harness,main,"report_offer",contract["deadline_tick"]): return false
		for row in rows(main.page):
			if row.get("task_id")==task_id: return check(harness,row.get("delivered_quantity",0)>contract.get("delivered_quantity",0),"actual_cargo_delivered")
		return check(harness,false,"reported_contract_visible")
	return check(harness,false,"delivery_deadline_elapsed")

## 拒绝错误固定创始人时保留原活世界身份；失败开局不能偷偷换默认人物或清空旧局。
static func rejected_selection(harness, main, scenario: String, path: String) -> bool:
	var before: Dictionary=main.kernel.source_identity()
	var failed: Dictionary=main.start_content(scenario,path,{},"actor.not_the_authored_founder")
	if not check(harness,not failed.get("ok",false),"fixed_founder_rejected"): return false
	if not check(harness,await harness.settled(main),"failed_start_preserves_page"): return false
	return check(harness,main.kernel.source_identity()==before,"failed_start_preserves_world")

## 已启动的正式情景跨地接单、采购和交付，再读取一处工资/成员后果；不自建验收harness。
static func run(harness, main, options: Dictionary={}) -> bool:
	if not check(harness,await harness.settled(main) and main._resident_mode and main.kernel.dynamic_world_enabled(),"native_catalog_entry"): return false
	var prefix: String=options.get("namespace","society")+"::"
	if options.has("catalog_path") and options.has("scenario_id"):
		if not await rejected_selection(harness,main,options["scenario_id"],options["catalog_path"]): return false
	var ids: Dictionary={"city":prefix+"settlement.qingchuan","sect":prefix+"settlement.sword_yard","shop":prefix+"shop.grain","shop_place":prefix+"place.grain","courtyard":prefix+"place.courtyard","merchant":prefix+"actor.merchant","merchant_role":prefix+"role.merchant","item":prefix+"item.millet"}
	if not await travel(harness,main,ids["sect"]): return false
	if not await click(harness,main,"dynamic_open"): return false
	var observed: Array=rows(main.page)
	if not check(harness,not observed.is_empty(),"actual_place_observation"): return false
	var contract: Dictionary=await accept_contract(harness,main,options.get("scenario_id",""))
	if contract.is_empty(): return false
	if not await click(harness,main,"dynamic_close"): return false
	if not await travel(harness,main,ids["city"]): return false
	if not await click(harness,main,"move_local",ids["shop_place"]): return false
	if not await purchase(harness,main,ids,contract): return false
	if not await click(harness,main,"move_local",ids["courtyard"]): return false
	if not await travel(harness,main,ids["sect"]): return false
	if not await click(harness,main,"dynamic_open"): return false
	if not await report_contract(harness,main,contract): return false
	if not await click(harness,main,"dynamic_close"): return false
	var target_tick: int=options.get("observation_tick",8)
	while main.kernel.source_identity()["tick"]<target_tick:
		var prior_tick: int=main.kernel.source_identity()["tick"]
		if not await click(harness,main,"wait"): return false
		if not check(harness,main.kernel.source_identity()["tick"]==prior_tick+1,"observed_real_half_day"): return false
	if not await click(harness,main,"dynamic_open"): return false
	var consequence: bool=false
	for row in rows(main.page):
		if row.get("kind")=="wage" and row.get("outstanding_amount",0)>0: consequence=true
		if row.get("kind")=="member" and row.get("response","") in ["payment_requested","refusing_new_duties","awaiting_release","departed"]: consequence=true
	return check(harness,consequence,"real_wage_or_member_consequence")
