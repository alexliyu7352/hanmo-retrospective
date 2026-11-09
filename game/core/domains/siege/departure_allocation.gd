## 安全离城的有界确定性全队安置求解；只比较已核合法选项，不创造营地或占用实际资产。
extends RefCounted
const MAX_ARMIES: int=16
const MAX_CAMPS: int=32
const MAX_SEARCH_STATES: int=8192

## 预算只限制本次自动求解，不表示全游戏军团/营地上限，更不把未判定伪装成无解。
static func limit(kind: String, maximum: int) -> Dictionary:
	return {"ok":false,"code":"SiegeDeparturePlanningLimit","limit":kind,"maximum":maximum,"feasibility":"undetermined"}

## 预核选项后穷尽回溯，重量/金额同时剪枝；只有完整解才返回全队分配。
static func solve(plans: Dictionary, room: Dictionary, search_budget: int=MAX_SEARCH_STATES) -> Dictionary:
	if plans.size()>MAX_ARMIES: return limit("armies",MAX_ARMIES)
	if room.size()>MAX_CAMPS: return limit("camps",MAX_CAMPS)
	if search_budget<0 or search_budget>MAX_SEARCH_STATES: return {"ok":false,"code":"InvalidSiegeDepartureBudget"}
	var order: Array=plans.keys()
	for army_id in order:
		if plans[army_id].is_empty(): return {"ok":false,"code":"SiegeSafeDepartureUnavailable","army_id":army_id,"reason":"CampaignNoFriendlyGarrison"}
	# 先解选择最少的军团只作剪枝启发；后续仍完整回溯，不拿贪心结果当可行性判定。
	order.sort_custom(func(a: String,b: String):
		if plans[a].size()!=plans[b].size(): return plans[a].size()<plans[b].size()
		if plans[a][0].weight!=plans[b][0].weight: return plans[a][0].weight>plans[b][0].weight
		return a<b)
	var camps: Array=room.keys(); camps.sort()
	var used: Dictionary={}; var options: Dictionary={}
	var total_weight: int=0; var total_money: int=0; var free_weight: int=0; var free_money: int=0
	for camp_id in camps:
		used[camp_id]={"weight":0,"money":0}; free_weight+=room[camp_id].weight; free_money+=room[camp_id].money
	for army_id in order:
		options[army_id]=plans[army_id].duplicate(true)
		options[army_id].sort_custom(func(a: Dictionary,b: Dictionary):
			if a.preferred!=b.preferred: return a.preferred
			if a.cost!=b.cost: return a.cost<b.cost
			return a.garrison_id<b.garrison_id)
		total_weight+=options[army_id][0].weight; total_money+=options[army_id][0].money
	if total_weight>free_weight: return {"ok":false,"code":"SiegeSafeDepartureUnavailable","army_id":"","reason":"GarrisonCapacityExceeded"}
	if total_money>free_money: return {"ok":false,"code":"SiegeSafeDepartureUnavailable","army_id":"","reason":"MoneyOverflow"}
	var context: Dictionary={"order":order,"camps":camps,"room":room,"used":used,"options":options,"selected":{},"memo":{},"steps":0,"budget":search_budget}
	var result: Dictionary=_search(context,0)
	if not result.ok: return result
	var ids: Array=result.selected.keys(); ids.sort()
	var destinations: Array=[]
	for army_id in ids:
		var destination: Dictionary=result.selected[army_id].duplicate(true); destination.erase("preferred"); destinations.append(destination)
	return {"ok":true,"destinations":destinations,"search_states":context.steps}

## 只把完整穷尽的余量状态缓存为无解；预算命中立即向上传递未判定，不污染缓存。
static func _search(context: Dictionary, index: int) -> Dictionary:
	if index==context.order.size(): return {"ok":true,"selected":context.selected.duplicate(true)}
	var remaining: Array=[index]
	for camp_id in context.camps:
		remaining.append(context.room[camp_id].weight-context.used[camp_id].weight)
		remaining.append(context.room[camp_id].money-context.used[camp_id].money)
	var key: String=JSON.stringify(remaining)
	if context.memo.has(key): return _no_assignment()
	if context.steps>=context.budget: return limit("search_states",context.budget)
	context.steps+=1
	var army_id: String=context.order[index]
	for option in context.options[army_id]:
		var used: Dictionary=context.used[option.garrison_id]; var capacity: Dictionary=context.room[option.garrison_id]
		if used.weight+option.weight>capacity.weight or used.money+option.money>capacity.money: continue
		used.weight+=option.weight; used.money+=option.money; context.selected[army_id]=option
		var result: Dictionary=_search(context,index+1)
		used.weight-=option.weight; used.money-=option.money; context.selected.erase(army_id)
		if result.ok or result.get("code")=="SiegeDeparturePlanningLimit": return result
	context.memo[key]=true
	return _no_assignment()

## 只有在当前预算内穷尽所有合法重量/金额组合后，才明确报告不存在全队分配。
static func _no_assignment() -> Dictionary:
	return {"ok":false,"code":"SiegeSafeDepartureUnavailable","army_id":"","reason":"SiegeReceptionCombinationUnavailable"}
