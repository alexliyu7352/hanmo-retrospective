## 教官口粮并集修复定点：复用真实任命/课程夹具，只核非队长及已有队长的唯一供给，不跑原191门。
extends "res://tests/test_military_training.gd"

## 现有人物到营沿审查原针的显式宏观边界，不声称本门验证行旅路线。
func supply_case(external_instructor: bool) -> void:
	var f: Dictionary=fresh()
	if external_instructor:
		f.actor="actor.he"
		f.state.actors[f.actor].location=Factory.settlement_location(f.data,f.data.garrisons[f.camp].place_id)
	commission(f)
	var action: Dictionary=start(f)
	if action.is_empty(): return
	var camp: Dictionary=f.data.garrisons[f.camp]
	var food_before: int=0
	for lot in f.state.inventories[camp.inventory_id].lots:
		if lot.item_id==camp.food_item_id: food_before+=lot.quantity
	var supplied: Dictionary=Assets.step(f.state,f.data,f.state.tick+1)
	check(supplied.ok,"唯一原供给正常生成")
	if not supplied.ok: return
	var supply: Dictionary={}
	for id in supplied.events:
		if f.state.facts[id].garrison_id==f.camp: supply=f.state.facts[id]
	check(not supply.is_empty() and supply.mouth_ids.count(f.actor)==1,"合法教官恰出现一次："+f.actor)
	var food_after: int=0
	for lot in f.state.inventories[camp.inventory_id].lots:
		if lot.item_id==camp.food_item_id: food_after+=lot.quantity
	check(food_before-food_after==supply.consumed and supply.required==supply.mouth_ids.size()*camp.ration_per_person and supply.shortage==0,"真实消耗只按一次去重并集收费")
	var inventories: Dictionary=f.state.inventories.duplicate(true); var rng: Dictionary=f.state.rng.duplicate(true)
	var result: Dictionary=Runtime.step(f.state,f.data,action,registry().actions,{"finish_tick":f.state.tick+1})
	check(result.ok and not result.completed and action.remaining_ticks==3,"教官真实供给后课程首半日成功推进")
	check(f.state.wallets[camp.training_wallet_id].balance==37 and f.state.wallets[camp.wallet_id].reserved.get(action.id)==113,"该课仅支付37、保留未来113")
	check(f.state.inventories==inventories and f.state.rng==rng,"训练不再次扣粮或抽随机")
	f.state.tick+=1
	check(Training.validate_state(f.state,f.data).ok and Assets.validate_state(f.state,f.data).ok,"补教官后真实稳定来源仍合法")

## 只跑原actor.he反例及已有队长去重对照，后者不能被新并集喂两份。
func _run() -> void:
	supply_case(true); supply_case(false)
	print("MILITARY_TRAINING_SUPPLY_CHECKS=",checks," FAILURES=",failures)
	quit(0 if failures==0 else 1)
