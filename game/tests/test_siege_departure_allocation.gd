## 独审安置反例及有界求解短门；复用真实任官/编组准备，不重跑原250组。
extends "res://tests/test_siege_safe_departure.gd"
const Allocation=preload("res://core/domains/siege/departure_allocation.gd")
const Assets=preload("res://core/military_assets.gd")
const Inventory=preload("res://core/inventory_rules.gd")
const Campaign=preload("res://core/campaign_rules.gd")
const Travel=preload("res://core/travel_rules.gd")

## 构造明确已经通过资格筛选的分配矩阵；不是游戏state，也不冒充正式军团。
func options(weights: Array, amounts: Array, room: Dictionary) -> Dictionary:
	var result: Dictionary={}
	for index in range(weights.size()):
		var id: String="army:"+str(index+1); result[id]=[]
		for camp_id in room:
			if weights[index]>room[camp_id].weight or amounts[index]>room[camp_id].money: continue
			result[id].append({"army_id":id,"campaign_id":"campaign:"+str(index+1),"garrison_id":camp_id,"destination_settlement_id":"destination","weight":weights[index],"money":amounts[index],"cost":1,"preferred":false})
	return result

## 检查完整分配的两个守恒约束，每个军团恰一次，不接受只给部分队伍的假成功。
func check_assignment(result: Dictionary, plans: Dictionary, room: Dictionary) -> void:
	if not check(result.ok,"容量矩阵存在完整解"): return
	var totals: Dictionary={}; var ids: Array=[]
	for row in result.destinations:
		check(plans.has(row.army_id) and not row.army_id in ids,"每支军团仅有一个接纳目标")
		ids.append(row.army_id)
		if not totals.has(row.garrison_id): totals[row.garrison_id]={"weight":0,"money":0}
		totals[row.garrison_id].weight+=row.weight; totals[row.garrison_id].money+=row.money
	check(ids.size()==plans.size(),"返回完整全队分配")
	for id in totals: check(totals[id].weight<=room[id].weight and totals[id].money<=room[id].money,"重量与金额同时满足接纳容量")

## 六件分配使largest-first首次放置失败，回溯必须找到真实组合；输入顺序与全局RNG不影响结果。
func check_solver() -> void:
	var room: Dictionary={"a":{"weight":10,"money":100},"b":{"weight":10,"money":100}}
	var plans: Dictionary=options([6,5,3,2,2,2],[0,0,0,0,0,0],room)
	var original: Dictionary=plans.duplicate(true); var original_room: Dictionary=room.duplicate(true)
	seed(182); var expected_random: int=randi(); seed(182)
	var solved: Dictionary=Allocation.solve(plans,room)
	check_assignment(solved,plans,room)
	check(solved.get("search_states",0)>plans.size(),"最大先放的首分支失败后确实回溯，非替换另一种贪心")
	check(plans==original and room==original_room and randi()==expected_random,"求解保持全部输入与RNG")
	var keys: Array=plans.keys(); keys.reverse(); var reversed: Dictionary={}
	for id in keys: reversed[id]=plans[id].duplicate(true); reversed[id].reverse()
	check(Allocation.solve(reversed,room)==solved,"军团与选项插入序不改变完整解及搜索步数")
	var money_room: Dictionary={"a":{"weight":100,"money":10},"b":{"weight":100,"money":10}}
	var money_plans: Dictionary=options([1,1,1,1,1,1],[6,5,3,2,2,2],money_room)
	check_assignment(Allocation.solve(money_plans,money_room),money_plans,money_room)
	var joint_room: Dictionary={"a":{"weight":10,"money":10},"b":{"weight":10,"money":10}}
	var joint: Dictionary=options([6,4,4,6],[2,8,6,4],joint_room)
	check_assignment(Allocation.solve(joint,joint_room),joint,joint_room)
	var impossible: Dictionary=options([6,6,6],[1,1,1],joint_room)
	check(Allocation.solve(impossible,joint_room).get("reason")=="SiegeReceptionCombinationUnavailable","总容量虽够但整队组合不可能时仅在穷尽后拒绝")
	var limited: Dictionary=Allocation.solve(plans,room,1)
	check(limited.get("code")=="SiegeDeparturePlanningLimit" and limited.get("feasibility")=="undetermined" and plans==original and room==original_room,"搜索预算耗尽明确未判定，输入无部分分配")
	check(Allocation.solve(plans,room)==solved,"预算中断没有把未判定缓存成永久无解")
	var too_many: Dictionary=options(range(1,18),range(0,17),{"camp":{"weight":1000,"money":1000}})
	check(Allocation.solve(too_many,{"camp":{"weight":1000,"money":1000}}).get("limit")=="armies","16军预算只限制本次自动安置")
	var many_camps: Dictionary={}
	for index in range(33): many_camps[str(index)]={"weight":10,"money":10}
	check(Allocation.solve(options([1],[1],many_camps),many_camps).get("limit")=="camps","32候选营预算明确拒绝自动求解")

## 精确复用独审正式作者原件：小军12货应入16营、大军36货入40营，真实提交不能误拒。
func check_review_counterexample() -> void:
	content_path=ProjectSettings.globalize_path("res://").trim_suffix("/").get_base_dir().path_join("build/safe-departure-allocation.hmcatalog.json")
	var k=Kernel.new(); var opened: Dictionary=k.new_game("scenario.alpha_life",content_path)
	if not check(opened.ok,"独审原作者经正式prepare启动 "+JSON.stringify(opened)): return
	var prepared: Dictionary=prepare(k)
	if prepared.is_empty(): return
	var state: Dictionary=k.snapshot(); var data: Dictionary=k.content().data; var siege: Dictionary=state.sieges[prepared.siege_id]
	var ids: Array=Departure.defenders(siege); var weights: Array=[]
	for id in ids: weights.append(Inventory.weight(state.inventories[state.armies[id].inventory_id],data))
	check(weights==[12,36],"复现独审两支真实军团12/36负载")
	var result: Dictionary=Departure.quote(state,data,siege)
	check(result.ok and result.destinations.size()==2 and k.snapshot()==state,"全队分配只读找到可行安置，不沿旧贪心误拒")
	if not result.ok: return
	var expected: Array=["garrison.refuge2","garrison.refuge"]
	for index in range(result.destinations.size()):
		var destination: Dictionary=result.destinations[index]; var army: Dictionary=state.armies[destination.army_id]
		check(destination.army_id==ids[index] and destination.garrison_id==expected[index],"12→16与36→40，按军ID输出")
		check(Assets._return_assets(state,data,army,destination.garrison_id).ok and Campaign.return_destination(state,data,army.order_id,destination.garrison_id).ok and Travel.plan(state,data,army.commander_actor_id,destination.destination_settlement_id).ok,"每个分配都由原资产/军令/行旅接口证实合法")
	var params: Dictionary={"siege_id":prepared.siege_id,"command":{"kind":"negotiate","terms":"safe_departure"}}
	check(k.preview("siege_strategy",params).ok and k.snapshot()==state,"真实交涉预览接受完整可行安排且只读")
	if not send(k,"siege_strategy",params): return
	var after: Dictionary=k.snapshot()
	for index in range(ids.size()):
		var army: Dictionary=after.armies[ids[index]]
		check(after.campaigns[army.order_id].return_garrison_id==expected[index] and after.actors[army.commander_actor_id].location.kind=="OnRoute","真实交接后两军按各自合法方案起行")

## 仅运行分配反例和独立矩阵，不重演安全离城250门或长围城流程。
func run() -> void:
	if not ProjectSettings.get_setting("application/config/siege_departure_isolated",false): quit(1); return
	check_solver()
	check_review_counterexample()
	print("SIEGE_DEPARTURE_ALLOCATION checks=",checks," failures=",failures)
	quit(0 if failures==0 else 1)
