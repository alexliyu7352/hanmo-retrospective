## 联合节点复用正常Main人物、任务及物理菜单银行；本片从不修改世界、钱包、目录或知情。
extends RefCounted
const Life=preload("res://tests/test_regional_life_player_flow.gd")
const Flow=preload("res://tests/test_dynamic_world_player_flow.gd")

## 有限断言进入调用方同一证据；失败不通过重开局或更换人物掩盖。
static func check(harness, value: bool, label: String) -> bool:
	harness.rows.append({"stage":"named_membership_"+label,"ok":value})
	if not value: harness.failures.append("named_membership_"+label)
	return value

## 只取实际渲染的人物事项，不能用后台全世界导出冒充用户看见了结果。
static func visible_rows(root: Node) -> Array:
	var rows: Array=[]
	if root.has_meta("membership_row"): rows.append(root.get_meta("membership_row").duplicate(true))
	for child in root.get_children(): rows.append_array(visible_rows(child))
	return rows

## 不使用第一位演员或中文文本定位；正式命令和冻结字段须同时匹配。
static func action_button(root: Node, command: String, match_params: Dictionary={}) -> Button:
	if root is Button and root.has_meta("membership_action") and not root.is_queued_for_deletion():
		var action: Dictionary=root.get_meta("membership_action")
		var matched: bool=action.get("command")==command
		for key in match_params: matched=matched and action.get("params",{}).get(key)==match_params[key]
		if matched: return root
	for child in root.get_children():
		var found: Button=action_button(child,command,match_params)
		if found!=null: return found
	return null

## 已实际展开原条款才找邀请/接受按钮，报价和点击之间不得替换来源。
static func expand_terms(harness, main, row_id: String) -> bool:
	if main.get_meta("membership_terms_expanded",{}).get(row_id,false): return true
	if not await Life.click(harness,main,"membership_terms",row_id): return false
	return check(harness,main.get_meta("membership_terms_expanded",{}).get(row_id,false) and main.find_child("NamedMembershipTerms",true,false)!=null,"actual_terms_visible")

## 每个正式决定只能从当前有效控件点击；不从helper直接submit或替NPC答复。
static func click_action(harness, main, command: String, match_params: Dictionary={}) -> bool:
	if not await harness.settled(main): return false
	var button: Button=action_button(main.page,command,match_params)
	if not check(harness,button!=null and button.is_inside_tree() and button.is_visible_in_tree() and not button.disabled,"actual_control_"+command): return false
	var action: Dictionary=button.get_meta("membership_action").duplicate(true)
	var source: Dictionary=main.kernel.source_identity()
	if not check(harness,button.get_meta("regional_source",{})==source,"control_source_current"): return false
	button.pressed.emit()
	if not check(harness,await harness.settled(main),"command_settled_"+command): return false
	harness.rows.append({"stage":"named_membership_actual_command","action":action,"before":source,"after":main.kernel.source_identity()})
	return true

## 正常标题选择后向实际熟人发出一份明确条款，保存前NPC尚未被helper代答。
static func start_to_pending(harness, main, ids: Dictionary) -> Dictionary:
	if not await Life.start_selected(harness,main,ids): return {}
	return await invite_person(harness,main,ids)

## 当场旧合同可先正常运行；发邀请仍选择当前真实候选，不能绕过旧责。
static func invite_person(harness, main, ids: Dictionary) -> Dictionary:
	var candidate: Dictionary={}
	for row in visible_rows(main.page):
		if row.get("kind")=="membership_contact" and row.get("actor_id")==ids.invitee_actor_id and row.get("terms_id")==ids.terms_id: candidate=row
	if not check(harness,not candidate.is_empty(),"real_known_same_place_candidate"): return {}
	if not await expand_terms(harness,main,candidate.id): return {}
	if not await click_action(harness,main,"organization_offer_membership",{"target_actor_id":ids.invitee_actor_id,"terms_id":ids.terms_id}): return {}
	for row in visible_rows(main.page):
		if row.get("kind")=="membership_invitation" and row.get("actor_id")==ids.invitee_actor_id and row.get("status")=="pending":
			return {"offer":row.duplicate(true),"source":main.kernel.source_identity(),"place_id":main.kernel.presentation_location().get("place_id","")}
	check(harness,false,"actual_pending_invitation_missing"); return {}

## 暂缓局先等真实部分采购交付与缺货，再当面邀请，不往测试世界塞一笔旧任务。
static func start_to_deferred(harness, main, ids: Dictionary) -> Dictionary:
	if not await Life.start_selected(harness,main,ids): return {}
	while main.kernel.source_identity().tick<ids.deadline_tick:
		var commitment: Dictionary={}; var contact: bool=false
		for row in visible_rows(main.page):
			if row.get("actor_id")!=ids.invitee_actor_id: continue
			if row.get("kind")=="membership_contact": contact=true
			if row.get("kind")=="membership_task" and row.get("delivered_quantity",0)>0 and row.get("delivered_quantity",0)<row.get("agreed_quantity",0): commitment=row
		if contact and not commitment.is_empty():
			var pending: Dictionary=await invite_person(harness,main,ids)
			if pending.is_empty(): return {}
			var answer: Dictionary=await wait_for_answer(harness,main,ids,pending.offer.id,"deferred")
			if answer.is_empty(): return {}
			return {"offer":answer,"original_task":commitment,"source":main.kernel.source_identity()}
		if not await Life.wait_halfday(harness,main,ids.deadline_tick): return {}
	check(harness,false,"real_old_task_deferral_not_reached"); return {}

## 第一个银行只从真菜单写，保存不推进期限、生成决定或给人发工资。
static func save_pending(harness, main, boundary: Dictionary, slot: String="manual-1") -> Dictionary:
	if not check(harness,not boundary.is_empty() and boundary.offer.status=="pending","pending_before_save"): return {}
	if not await Life.save_via_menu(harness,main,slot): return {}
	var result: Dictionary=boundary.duplicate(true); result.slot=slot
	harness.rows.append({"stage":"named_membership_pending_bank","boundary":result.duplicate(true)})
	return result

## 新空标题读取原物理bank后，邀请身份、原期限及原版本必须保持。
static func continue_pending(harness, main, ids: Dictionary, boundary: Dictionary) -> bool:
	if not await Life.continue_empty_title(harness,main,ids,boundary.slot): return false
	for row in visible_rows(main.page):
		if row.get("id")==boundary.offer.id and row.get("kind")=="membership_invitation":
			return check(harness,row==boundary.offer and same_restored_source(main.kernel.source_identity(),boundary.source),"pending_restored_exactly")
	return check(harness,false,"restored_pending_not_visible")

## 等待的是实际已知决定；只有正常半日可以让Director复评，不写同意标记。
static func wait_for_answer(harness, main, ids: Dictionary, offer_id: String, expected: String) -> Dictionary:
	while main.kernel.source_identity().tick<ids.deadline_tick:
		if not await harness.settled(main): return {}
		for row in visible_rows(main.page):
			if row.get("kind")=="membership_invitation" and row.get("id")==offer_id and row.get("status")==expected:
				if not check(harness,not row.get("source_fact_id","").is_empty(),"answer_has_real_fact_"+expected): return {}
				return row.duplicate(true)
		if not await Life.wait_halfday(harness,main,ids.deadline_tick): return {}
	check(harness,false,"answer_not_reached_"+expected); return {}

## 已接受的人从原Task委派入口领取实际采购，收款钱包由当前合法元数据确定。
static func delegate_contract(harness, main, ids: Dictionary) -> bool:
	var params: Dictionary={"executor_actor_id":ids.invitee_actor_id}
	if ids.has("contract_payee_wallet_id"): params.payee_wallet_id=ids.contract_payee_wallet_id
	return await click_action(harness,main,"organization_accept_offer",params)

## 只在当面已知的真实待释放阶段保存第二个银行，不把等待工资当作受托责任。
static func reach_release_bank(harness, main, ids: Dictionary, slot: String="manual-2", save_bank: bool=true) -> Dictionary:
	while main.kernel.source_identity().tick<ids.deadline_tick:
		if not await harness.settled(main): return {}
		for row in visible_rows(main.page):
			if row.get("kind")=="membership_episode" and row.get("actor_id")==ids.invitee_actor_id and row.get("status")=="awaiting_release":
				var boundary: Dictionary={"episode":row.duplicate(true),"source":main.kernel.source_identity(),"place_id":main.kernel.presentation_location().get("place_id",""),"slot":slot}
				if save_bank and not await Life.save_via_menu(harness,main,slot): return {}
				harness.rows.append({"stage":"named_membership_old_commitment_bank","boundary":boundary.duplicate(true)})
				return boundary
		if not await Life.wait_halfday(harness,main,ids.deadline_tick): return {}
	check(harness,false,"real_awaiting_release_not_reached"); return {}

## 旧责/欠薪存档同样从独立空标题继续；旧episode和本人决定来源不得被重建。
static func continue_release_bank(harness, main, ids: Dictionary, boundary: Dictionary) -> bool:
	if not await Life.continue_empty_title(harness,main,ids,boundary.slot): return false
	for row in visible_rows(main.page):
		if row.get("kind")=="membership_episode" and row.get("id")==boundary.episode.id:
			return check(harness,row==boundary.episode and same_restored_source(main.kernel.source_identity(),boundary.source),"old_commitment_restored_exactly")
	return check(harness,false,"restored_release_not_visible")

## 短缺局最多出售作者初态真实持有的四粟；保留私人竞争，真钱成交后由原NPC合同续行。
static func finish_old_commitment(harness, main, ids: Dictionary) -> Dictionary:
	if main.kernel.presentation_location().get("place_id")!=ids.market_place_id:
		if not await Life.click(harness,main,"move_local",ids.market_place_id): return {}
	for unit in range(ids.get("restock_owned_units",2)):
		if not await Life.open_merchant_trade(harness,main,ids): return {}
		if not await Life.sell_owned_grain(harness,main,ids): return {}
	if not await Life.click(harness,main,"move_local",ids.membership_place_id): return {}
	while main.kernel.source_identity().tick<ids.deadline_tick:
		for row in visible_rows(main.page):
			if row.get("kind")=="membership_episode" and row.get("actor_id")==ids.invitee_actor_id and row.get("status")=="departed":
				if not check(harness,not row.get("source_fact_id","").is_empty(),"real_departure_after_old_task"): return {}
				return {"episode":row.duplicate(true),"source":main.kernel.source_identity()}
		if not await Life.wait_halfday(harness,main,ids.deadline_tick): return {}
	check(harness,false,"departure_after_actual_delivery_not_reached"); return {}

## 离职后沿原做工/采买近况继续普通生活；必须看见新的真实结果来源。
static func continue_ordinary_life(harness, main, ids: Dictionary, departure: Dictionary) -> bool:
	if not check(harness,not departure.is_empty() and departure.episode.status=="departed","departed_before_ordinary_life"): return false
	if main.kernel.presentation_location().get("place_id")!=ids.work_place_id:
		if not await Life.click(harness,main,"move_local",ids.work_place_id): return false
	var boundary: Dictionary={"actor_id":ids.invitee_actor_id,"place_id":ids.work_place_id}
	return await Life.continue_from_boundary(harness,main,ids,boundary)

## 玩家自己做答只点本人邀请的接受/拒绝；NPC职事产生邀请后也不能预写结果。
static func answer_as_player(harness, main, ids: Dictionary, answer: String) -> Dictionary:
	while main.kernel.source_identity().tick<ids.deadline_tick:
		for row in visible_rows(main.page):
			if row.get("kind")=="membership_invitation" and row.get("status")=="pending":
				if not await expand_terms(harness,main,row.id): return {}
				if not await click_action(harness,main,"organization_answer_membership",{"actor_id":ids.player_actor_id,"offer_id":row.id,"answer":answer}): return {}
				return {"offer_id":row.id,"answer":answer,"source":main.kernel.source_identity()}
		if not await Life.wait_halfday(harness,main,ids.deadline_tick): return {}
	check(harness,false,"player_invitation_not_received"); return {}

## 旧回调释放后不许重答或延长期限，也不能借重绘更换目标。
static func stale_choice(harness, main, command: String, params: Dictionary={}) -> bool:
	var button: Button=action_button(main.page,command,params)
	if not check(harness,button!=null and not button.disabled,"stale_original_control"): return false
	var callbacks: Array=button.pressed.get_connections()
	main._render()
	if not await harness.settled(main): return false
	var source: Dictionary=main.kernel.source_identity(); var sequence: int=main.kernel.command_context().next_request_seq
	for callback in callbacks:
		if callback.callable.is_valid(): callback.callable.call()
	if not await harness.settled(main): return false
	return check(harness,source==main.kernel.source_identity() and sequence==main.kernel.command_context().next_request_seq,"stale_callback_never_decides")

## 独立恢复只更换运行epoch；逻辑世界身份、修订和tick须与原银行相同。
static func same_restored_source(current: Dictionary, saved: Dictionary) -> bool:
	return current.get("world_id")==saved.get("world_id") and current.get("revision")==saved.get("revision") and current.get("tick")==saved.get("tick")
