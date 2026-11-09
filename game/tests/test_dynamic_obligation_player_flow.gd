## 公托归粮的正常Main操作片；中央只传正式新局与现有等待器，不注入任务货物或战斗结果。
extends RefCounted
const Flow=preload("res://tests/test_dynamic_world_player_flow.gd")

## 从当前有限行选择本人实际执行的组织动作；组织、权限和执行人均由原生来源指定。
static func organization_action(main) -> Dictionary:
	var actor: String=main.kernel.command_context().get("actor_id","")
	for row in Flow.rows(main.page):
		if row.get("kind")=="organization_opportunity" and row.get("executor_actor_id")==actor:
			return row.duplicate(true)
	return {}

## 读取真实已发布份额；请求只需offer_ref，关联合同用有限行的offer_id，不假定冗余别名。
static func accept_organization(harness, main, publication_deadline: int) -> Dictionary:
	var row: Dictionary=organization_action(main)
	while row.is_empty() and main.kernel.source_identity()["tick"]<publication_deadline:
		if not await Flow.wait_halfday(harness,main,publication_deadline): return {}
		row=organization_action(main)
	if not Flow.check(harness,not row.is_empty(),"organization_offer_published"): return {}
	var action: Dictionary=row["action"]
	var request: Dictionary=action["params"]
	var actor: String=main.kernel.command_context()["actor_id"]
	if not Flow.check(harness,request["authority"]["actor_id"]==actor and request["executor_actor_id"]==actor and request["task"]["context"]["provider_actor_id"]!=actor and not request.get("payee_wallet_id","").is_empty(),"organization_actor_and_provider_are_real"): return {}
	var quote: Dictionary=await Flow.preview(harness,main,action["command"],request)
	if not Flow.check(harness,quote.get("ok",false),"organization_accept_legal_"+quote.get("code","")): return {}
	var terms: Dictionary=quote.get("contract_terms",{})
	if not Flow.check(harness,terms.get("quantity")==4 and terms.get("deadline_tick",0)>main.kernel.source_identity()["tick"],"organization_four_unit_frozen_terms"): return {}
	if not await Flow.click(harness,main,action["command"],row["id"]): return {}
	if not await Flow.finish_action(harness,main,"accept_offer",terms["deadline_tick"]): return {}
	for contract in Flow.rows(main.page):
		if contract.get("kind")=="contract" and contract.get("offer_id")==row["offer_id"] and contract.get("actor_id")==actor:
			if not Flow.check(harness,contract.get("assignee_kind")=="organization" and contract.get("assignee_id")==request["authority"]["organization_id"] and contract.get("executor_actor_id")==actor and contract.get("payee_wallet_id")==request["payee_wallet_id"],"organization_responsibility_keeps_actual_executor"): return {}
			return contract.duplicate(true)
	Flow.check(harness,false,"organization_contract_visible")
	return {}

## 战斗逐轮点击现有等待和提交按钮，败北必须由真实对手行动算出；回合上限来自当前目标。
static func lose_actual_battle(harness, main, encounter: Dictionary) -> bool:
	if not await Flow.click(harness,main,"encounter_fight",encounter["encounter_id"]): return false
	var screen: Control=main.find_child("PersonalBattleScreen",true,false)
	if not Flow.check(harness,screen!=null,"cargo_real_personal_battle_screen"): return false
	var battle_id: String=screen.battle["id"]
	var maximum: int=main._data["personal_objectives"][screen.battle["objective_id"]]["max_rounds"]
	for unused in range(maximum):
		screen=main.find_child("PersonalBattleScreen",true,false)
		if not Flow.check(harness,screen!=null and screen.battle["id"]==battle_id,"cargo_same_actual_battle"): return false
		if screen.battle["phase"]!="planning": break
		var before: int=screen.battle["round"]
		if not await Flow.click(harness,main,"battle_action","wait"): return false
		if not await Flow.click(harness,main,"battle_submit"): return false
		screen=main.find_child("PersonalBattleScreen",true,false)
		if not Flow.check(harness,screen!=null and (screen.battle["round"]>before or screen.battle["phase"]=="result"),"cargo_battle_real_round_progress"): return false
	screen=main.find_child("PersonalBattleScreen",true,false)
	if not Flow.check(harness,screen!=null and screen.battle["phase"]=="result" and screen.battle["outcome"]=="defeat","cargo_actual_defeat_result"): return false
	harness.rows.append({"stage":"dynamic_world_cargo_actual_battle","encounter":encounter.duplicate(true),"battle":screen.battle.duplicate(true),"source":main.kernel.source_identity()})
	return await Flow.click(harness,main,"battle_close",battle_id)

## 原地图启程并处理真正的路遇；去程合法道别，载货回程只在指定对手真实相遇时应战。
static func travel(harness, main, destination: String, deadline: int, raider: String="") -> bool:
	main._open_map(); main._map_select(destination)
	if not Flow.check(harness,await harness.settled(main),"cargo_route_preview"): return false
	if not Flow.check(harness,main._map_quote().get("ok",false),"cargo_route_legal"): return false
	if not await Flow.click(harness,main,"map_confirm"): return false
	var fought: bool=false
	while main.kernel.action_context().get("action_kind")=="travel":
		if not Flow.check(harness,main.kernel.source_identity()["tick"]<deadline,"cargo_route_within_contract_deadline"): return false
		var sidebar: Dictionary=main.kernel.journey_sidebar()
		var encounter: Dictionary=sidebar.get("encounter",{})
		if not encounter.is_empty():
			if not raider.is_empty() and not fought and encounter.get("provider_actor_id")==raider:
				if not await lose_actual_battle(harness,main,encounter): return false
				fought=true
			elif not await Flow.click(harness,main,"encounter_leave",encounter["encounter_id"]): return false
			continue
		if sidebar.get("travel",{}).get("paused",false):
			if not await Flow.click(harness,main,"resume_travel"): return false
		if not await Flow.click(harness,main,"advance"): return false
	main._close_map()
	if not Flow.check(harness,await harness.settled(main),"cargo_arrival_page"): return false
	if not Flow.check(harness,raider.is_empty() or fought,"cargo_loaded_route_reached_actual_source"): return false
	var location: Dictionary=main.kernel.presentation_location()
	return Flow.check(harness,location.get("kind")=="Settlement" and main._data["places"][location["place_id"]]["settlement_id"]==destination,"cargo_actual_arrival")

## 同源本人合同查询只作有限结果核对，不读取整世界或重算任何损失与清算规则。
static func own_contract(harness, main, task_id: String) -> Dictionary:
	var result: Dictionary=main.kernel.dynamic_contract(task_id)
	if result.get("code")=="NativeAsyncRequired":
		if not Flow.check(harness,await harness.settled(main),"cargo_contract_read_settled"): return {}
		result=main.kernel.dynamic_contract(task_id)
	if not Flow.check(harness,result.get("ok",true) and result.get("code")!="NativeAsyncRequired","cargo_contract_read_available"): return {}
	return result

## 既有底栏可折行容纳新入口；不再用浮动坐标覆盖场景内的人物按钮。
static func entry_layout(harness, main) -> bool:
	var control: Button=Flow.button(main.page,"dynamic_open")
	return Flow.check(harness,control!=null and control.get_parent().name=="SceneNavigationActions","dynamic_entry_in_existing_navigation")

## 一个正式组织合同经过真采购、真败北、余货交付及比例清算；未满足任何入口则明确停在失败页。
static func run(harness, main, options: Dictionary={}) -> bool:
	if not Flow.check(harness,await harness.settled(main) and main._resident_mode and main.kernel.dynamic_world_enabled(),"obligation_native_main_entry"): return false
	if not entry_layout(harness,main): return false
	var prefix: String=options.get("namespace","society")+"::"
	var ids: Dictionary={"city":prefix+"settlement.qingchuan","sect":prefix+"settlement.sword_yard","shop":prefix+"shop.grain","shop_place":prefix+"place.grain","courtyard":prefix+"place.courtyard","merchant":prefix+"actor.merchant","merchant_role":prefix+"role.merchant","item":prefix+"item.millet"}
	if not Flow.check(harness,main.kernel.presentation_location().get("place_id")==prefix+"place.sword_gate","obligation_formal_authored_start"): return false
	if not await Flow.click(harness,main,"dynamic_open"): return false
	var contract: Dictionary=await accept_organization(harness,main,main.kernel.source_identity()["tick"]+6)
	if contract.is_empty(): return false
	if not await Flow.click(harness,main,"dynamic_close"): return false
	if not await travel(harness,main,ids["city"],contract["deadline_tick"]): return false
	if not await Flow.click(harness,main,"move_local",ids["shop_place"]): return false
	if not await Flow.purchase(harness,main,ids,contract): return false
	if not await Flow.click(harness,main,"move_local",ids["courtyard"]): return false
	if not await travel(harness,main,ids["sect"],contract["deadline_tick"],prefix+"actor.road_raider"): return false
	if not await Flow.click(harness,main,"dynamic_open"): return false
	var damaged: Dictionary=await own_contract(harness,main,contract["task_id"])
	if not Flow.check(harness,damaged.get("transferred_quantity")==1 and damaged.get("destroyed_quantity")==1 and damaged.get("delivered_quantity")==0,"cargo_transfer_and_sink_are_not_delivery"): return false
	if not await Flow.report_contract(harness,main,contract): return false
	if not await Flow.click(harness,main,"dynamic_contract",contract["task_id"]): return false
	var settled: Dictionary=await own_contract(harness,main,contract["task_id"])
	if not Flow.check(harness,settled.get("payee_wallet_id")==prefix+"wallet.sect","organization_reward_frozen_public_account"): return false
	if not Flow.check(harness,settled.get("settlement_status")=="settled" and settled.get("delivered_quantity")==2 and settled.get("paid_reward")==4 and settled.get("refunded_budget",0)>0 and settled.get("refunded_reward")==4,"cargo_actual_partial_delivery_and_prorata_settlement"): return false
	harness.rows.append({"stage":"dynamic_world_obligation_contract_finished","contract":settled.duplicate(true),"source":main.kernel.source_identity()})
	return true
