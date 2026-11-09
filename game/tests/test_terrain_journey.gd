## 地形行旅纯模块回归：整数截断、低预算积累、成本变化轨迹、冻结余程与接触恢复均调用真实规则。
extends SceneTree
const Journey = preload("res://core/terrain_journey.gd")
const Travel = preload("res://core/travel_rules.gd")
const Terrain = preload("res://core/world_terrain.gd")
const Query = preload("res://core/terrain_query.gd")
const Contact = preload("res://core/terrain_contact.gd")
const Maps = preload("res://tests/test_world_terrain.gd")
var checks: int = 0
var failures: int = 0

## 引擎初始化后执行纯规则，不启动游戏或使用玩家存档。
func _init() -> void:
	call_deferred("run_checks")

## 记录独立行为断言，任何失败都以非零状态退出。
func check(value: bool, label: String) -> void:
	checks += 1
	if not value: failures += 1; printerr("FAIL: ", label)

## 构造最小有效领域输入；真实begin记录计划依据，测试不伪造动作完成或地图位置。
static func fixture(map: Dictionary, points: Array, budget: int = 250) -> Dictionary:
	map = map.duplicate(true)
	map.budgets = {"foot": budget, "army": budget, "boat": budget}; map.money_per_tick = 7; map.fatigue_per_tick = 1
	var action: Dictionary = {"id": "action:a", "actor_id": "a", "kind": "travel", "start_tick": 0, "remaining_ticks": 1, "params": {}}
	var actor: Dictionary = {"id": "a", "alive": true, "available": true, "action_id": "action:a", "fatigue": 0, "wallet_id": "wallet:a", "inventory_id": "inventory:a", "location": {"kind": "Wilderness", "map_id": "map.test", "point": points[0].duplicate(), "origin": "settlement.origin"}}
	var state: Dictionary = {"scenario_id": "scenario.test", "tick": 0, "next_instance_id": 1, "actors": {"a": actor}, "actions": {"action:a": action}, "parties": {}, "flags": {}, "facts": {}, "wallets": {"wallet:a": {"balance": 100000, "reserved": {}}}, "inventories": {"inventory:a": {"lots": []}}}
	var data: Dictionary = {"world_maps": {"map.test": map}, "scenarios": {"scenario.test": {"world_map_id": "map.test"}}, "settlements": {"settlement.origin": {"map_xy": points[0].duplicate()}}, "items": {}}
	var quote: Dictionary = {"map_id": "map.test", "points": points.duplicate(true), "destination": points[-1].duplicate(), "origin": "settlement.origin", "mode": "foot", "budget": budget, "starting_load": 0, "duration": 1, "payer_wallet_id": "wallet:a", "money_per_tick": 7, "fatigue_per_tick": 1}
	Journey.begin(state, data, action, quote)
	return {"state": state, "data": data, "action": action}

## 在本地字典结算一个半日，模拟世界统一时钟边界后再校验行程，不接触实际内核实例。
static func advance(f: Dictionary) -> Dictionary:
	var result: Dictionary = Journey.step(f.state, f.data, f.action)
	if result.ok: f.state.tick += 1
	return result

## 山角反例要求前缀与后缀同时合法；逐步推进后仍能恢复原始计划并最终抵达。
func check_corner_and_suffix() -> void:
	var map: Dictionary = Maps.plain_map(60, 60, 20); map.cells[1].terrain = "mountain"
	var f: Dictionary = fixture(map, [[1, 2], [41, 40]])
	check(Terrain.segment_cost(map, [1, 2], [41, 40]).get("cost") == 552, "原山角反例的整条路线合法且成本固定")
	var motion: Dictionary = Journey.motion(f.state, f.data, f.action)
	check(Terrain.segment_cost(map, [1, 2], motion.end).ok and Terrain.segment_cost(map, motion.end, [41, 40]).ok, "部分落点前后两段均合法，不返回会损坏余程的[19,19]")
	check(Contact.validate_trace(motion).ok, "山角部分轨迹满足连续接触输入约定")
	var result: Dictionary = advance(f)
	check(result.ok and not result.completed and Journey.validate_action(f.state, f.data, f.action).ok, "截断后的行程可通过保存边界校验")
	var clone: Dictionary = f.duplicate(true)
	clone.action = clone.state.actions["action:a"]
	var finished: bool = false
	for _tick in range(20):
		var a: Dictionary = advance(f); var b: Dictionary = advance(clone)
		if a != b or f.state != clone.state: break
		if a.get("completed", false): finished = true; break
	check(finished and f.state == clone.state and f.state.actors.a.location.point == [41, 40], "恢复相同输入后得到完全一致的最终位置、信用和费用")
	f = fixture(map, [[41, 40], [1, 2]])
	motion = Journey.motion(f.state, f.data, f.action)
	check(Terrain.segment_cost(map, [41, 40], motion.end).ok and Terrain.segment_cost(map, motion.end, [1, 2]).ok, "反向整数截断同样保证前后段都合法")

## 低预算每次累积真实通行能力，暂停不积攒，达到整数移动成本后确实抵达。
func check_low_budget_and_pause() -> void:
	var f: Dictionary = fixture(Maps.plain_map(40, 40, 20), [[10, 10], [11, 10]], 1)
	for _tick in range(3): advance(f)
	check(f.state.actors.a.location.point == [10, 10] and f.action.params.travel_credit == 3 and Journey.validate_action(f.state, f.data, f.action).ok, "不足一步的三次预算作为可保存信用累积")
	check(f.state.wallets["wallet:a"].balance == 99979, "积累预算仍只收三个实际半日费用")
	f.action.params.paused = true
	var before: Dictionary = f.state.duplicate(true)
	var held: Dictionary = Journey.motion(f.state, f.data, f.action)
	check(not held.moving and held.end_credit == 3 and held.budget == 4 and Contact.validate_trace(held).ok, "暂停轨迹保持原信用且仍完整覆盖当前半日")
	check(not Journey.step(f.state, f.data, f.action).ok and f.state == before, "暂停不能偷偷扣费、增长信用或移动")
	f.action.params.paused = false
	check(not Journey.motion(f.state, f.data, f.action, ["a"]).moving, "外部统一hold同样保持原地")
	var done: bool = false
	for _tick in range(7):
		var result: Dictionary = advance(f)
		done = result.get("completed", false)
	check(done and f.state.tick == 10 and f.state.actors.a.location.point == [11, 10], "预算1在第十个半日走完成本10的真实距离")
	check(f.action.params.travel_credit == 0 and f.state.wallets["wallet:a"].balance == 99930, "完成后没有可重复兑现的信用，费用恰好十次")
	f = fixture(Maps.plain_map(40, 40, 20), [[10, 10], [30, 10]], 1)
	advance(f); f.state.wallets["wallet:a"].balance = 0
	before = f.state.duplicate(true); held = Journey.motion(f.state, f.data, f.action)
	check(not held.moving and held.end_credit == 1 and not Journey.step(f.state, f.data, f.action).ok and f.state == before, "无钱滞留不能继续获得通行信用")

## 成本变化直接改变半日内的速度，格界浮点交点只保留在临时轨迹，最终人物位置仍是整数。
func check_profiles_and_contact_time() -> void:
	var map: Dictionary = Maps.plain_map(120, 20, 20)
	for index in range(6): map.cells[index].terrain = "road" if index < 3 else "forest"
	var profile: Dictionary = Terrain.segment_profile(map, [10, 10], [110, 10])
	check(profile.ok and profile.cost == 1300 and profile.spans.size() == 2, "同一积分输出道路和林地两个真实成本区间")
	check(is_equal_approx(profile.spans[0].end_cost, 400.0) and profile.spans[0].to[0] == 60.0 and profile.spans[0].to[1] == 10.0 and profile.spans[1].rate == 18, "道路区间到x60只消耗400，余段采用林地成本")
	var f: Dictionary = fixture(map, [[10, 10], [110, 10]], 1300)
	var motion: Dictionary = Journey.motion(f.state, f.data, f.action)
	check(Contact.validate_trace(motion).ok and motion.segments.size() == 2, "真实行旅使用同源成本区间而非整条混合地形匀速")
	var b: Dictionary = {"actor_id": "b", "action_id": "action:b", "map_id": "map.test", "origin": "settlement.origin", "budget": 100, "params": {"points": [[100, 10]], "paused": true}, "segments": [{"from": [78, 10], "to": [78, 10], "start_cost": 0, "end_cost": 100, "point_index": 0}]}
	var hit: Dictionary = Contact.intersection(motion, b)
	check(not hit.is_empty() and absf(float(hit.time_num) / hit.time_den - 652.0 / 1300.0) < 0.000000001 and hit.anchors.a.point == [74, 10], "林地内首次接触按652成本发生，不使用错误匀速时刻")
	map = Maps.plain_map(60, 60, 20); map.cells[1].terrain = "forest"
	profile = Terrain.segment_profile(map, [5, 5], [55, 25])
	check(profile.ok and profile.cost == Terrain.segment_cost(map, [5, 5], [55, 25]).cost, "斜向格界小数点与成本查询来自同一次分类算法")
	f = fixture(map, [[5, 5], [55, 25]], 2000)
	motion = Journey.motion(f.state, f.data, f.action)
	check(Contact.validate_trace(motion).ok and motion.end == [55, 25], "含浮点格界和不足一成本原地尾额的轨迹仍连续有效")
	check(profile.cost == Terrain.segment_profile(map, [55, 25], [5, 5]).cost, "反向成本变化积分保持相同总成本")

## 远方临时关口不损坏尚可行走的前段或存档，到达受阻路段才停下且不扣费。
func check_distant_gate() -> void:
	var map: Dictionary = Maps.plain_map(120, 20, 20)
	map.cells[5].blocked_flag = "gate.later"
	var f: Dictionary = fixture(map, [[10, 10], [50, 10], [90, 10], [110, 10]], 100)
	f.state.flags = {"gate.later": true}
	var result: Dictionary = advance(f)
	check(result.ok and f.state.actors.a.location.point == [20, 10] and f.action.remaining_ticks == 9, "远方封关仍可走当前前段，余程使用永久地形时耗")
	check(Journey.validate_action(f.state, f.data, f.action).ok, "尚未抵达封闭关口的真实行程可以保存")
	for _tick in range(7): result = advance(f)
	check(result.ok and f.state.actors.a.location.point == [90, 10], "半日预算恰好走完前段时不查询下一段封关成本")
	var before: Dictionary = f.state.duplicate(true)
	result = advance(f)
	check(result.get("code") == "TerrainBlocked" and f.state == before, "进入被封余段以前停止，失败不移动、不扣钱或积累信用")

## 改道保留实际位置与已积累信用，创建新的冻结路线索引；任意删改后缀或虚构信用均被拒绝。
func check_replan_and_validation() -> void:
	var f: Dictionary = fixture(Maps.plain_map(120, 120, 20), [[10, 10], [10, 90], [110, 90], [110, 10]], 1)
	for _tick in range(3): advance(f)
	var old_plan: String = f.action.params.plan_id
	var quote: Dictionary = Journey.plan(f.state, f.data, "a", [30, 10])
	check(quote.ok, "可从实际途中整数点规划新的自由目标")
	Journey.begin(f.state, f.data, f.action, quote)
	check(f.action.params.travel_credit == 3 and f.action.params.route_index == 1 and f.action.params.plan_id != old_plan and f.state.actors.a.location.point == [10, 10], "整体替换计划仍保留信用和当前位置，冻结索引从新计划起点开始")
	check(Journey.validate_action(f.state, f.data, f.action).ok, "改道后既有信用能通过保存校验")
	var invalid: Dictionary = f.duplicate(true); invalid.action.params.travel_credit = -1
	check(Journey.validate_action(invalid.state, invalid.data, invalid.action).get("code") == "InvalidTerrainCredit", "负信用拒绝")
	invalid = f.duplicate(true); invalid.action.params.travel_credit = 9223372036854775807
	check(Journey.validate_action(invalid.state, invalid.data, invalid.action).get("code") == "InvalidTerrainCredit", "溢出级信用在加预算以前拒绝")
	invalid = f.duplicate(true); invalid.action.params.travel_credit = 4
	check(Journey.validate_action(invalid.state, invalid.data, invalid.action).get("code") == "InvalidTerrainCredit", "仅经历三个半日不能凭空拥有四份预算")
	f = fixture(Maps.plain_map(120, 120, 20), [[10, 10], [10, 90], [110, 90], [110, 10]], 100)
	f.action.params.points = [[110, 10]]
	check(Journey.validate_action(f.state, f.data, f.action).get("code") == "TerrainJourneySuffixMismatch", "不能保留冻结依据却删掉必须经过的余程折点")
	f = fixture(Maps.plain_map(120, 120, 20), [[10, 10], [10, 90], [110, 90], [110, 10]], 100)
	f.action.params.route_index = 4
	check(Journey.validate_action(f.state, f.data, f.action).get("code") == "TerrainJourneySuffixMismatch", "未完成行程的索引不能超出冻结路线尾端")
	f.action.params.route_index = 1; f.action.params.travel_credit = 0.5
	check(Journey.validate_action(f.state, f.data, f.action).get("code") == "InvalidTerrainCredit", "持久预算不能用小数绕过整数成本契约")

## 只读轨迹和成本预览重复查询保持一致，调用方改返回数组也不能污染实际世界或冻结依据。
func check_query_purity() -> void:
	var map: Dictionary = Maps.plain_map(120, 40, 20); map.cells[3].terrain = "forest"
	var f: Dictionary = fixture(map, [[10, 10], [110, 10]], 100)
	var before: Dictionary = f.state.duplicate(true); var data_before: Dictionary = f.data.duplicate(true)
	var motion: Dictionary = Journey.motion(f.state, f.data, f.action)
	check(motion == Journey.motion(f.state, f.data, f.action) and f.state == before and f.data == data_before, "反复预览没有修改时钟、信用、钱包、地形或冻结路线")
	motion.end[0] = 99; motion.params.points[0][0] = 99; motion.segments[0].from[0] = 99
	check(f.state == before and f.data == data_before, "轨迹返回位置、完整参数和成本段均无输入可写别名")
	var from: Array = [10, 10]; var to: Array = [110, 10]
	var map_before: Dictionary = map.duplicate(true)
	var profile: Dictionary = Terrain.segment_profile(map, from, to)
	check(profile == Terrain.segment_profile(map, from, to), "成本变化轨迹由同一公开输入确定，不依赖随机流")
	profile.spans[0].from[0] = 77; profile.spans[-1].to[0] = 88
	check(from == [10, 10] and to == [110, 10] and map == map_before, "成本分段返回数组不引用调用方起止点或地形")
	var flags: Dictionary={"gate.test":false}
	map.cells[2].blocked_flag="gate.test"
	var query=Query.new(map,"foot",flags)
	var expected: Dictionary=Terrain.segment_profile(map,from,to,"foot",flags)
	check(query.status.ok and query.segment_profile(from,to)==expected and query.segment_cost(from,to).cost==expected.cost,"批次查询与原公开积分返回相同成本和轨迹")
	flags["gate.test"]=true; map.cells[0].terrain="water"
	check(query.segment_profile(from,to)==expected and query.point_valid(from),"一次查询冻结地图和临时封路输入，调用者随后修改不污染当前计算")
	var changed=Query.new(map,"foot",flags)
	check(not changed.point_valid(from) and not changed.segment_cost(from,to).ok,"下次查询真实采纳封路和地形变化，不使用跨半日陈旧缓存")
	map.cells.pop_back()
	var invalid=Query.new(map)
	check(not invalid.status.ok and not invalid.point_valid(from) and not invalid.segment_cost(from,to).ok,"无效地图的批次查询仍明确拒绝，性能优化不跳过定义校验")

## 多折线相遇递增冻结索引并清空被截断半日预算，接触锚点与恢复后的余程仍能保存和继续。
func check_contact_resume_and_normal_routes() -> void:
	var f: Dictionary = fixture(Maps.plain_map(160, 100, 20), [[10, 10], [60, 10], [60, 60], [120, 60]], 1000)
	var motion: Dictionary = Journey.motion(f.state, f.data, f.action)
	var b: Dictionary = {"actor_id": "b", "action_id": "action:b", "map_id": "map.test", "origin": "settlement.origin", "budget": 100, "params": {"points": [[60, 90]], "paused": true}, "segments": [{"from": [60, 40], "to": [60, 40], "start_cost": 0, "end_cost": 100, "point_index": 0}]}
	var hit: Dictionary = Contact.intersection(motion, b)
	check(not hit.is_empty() and hit.traces.a.params.route_index == 2 and hit.traces.a.params.points == [[60, 60], [120, 60]], "跨过首折点后相遇同步递增冻结索引和裁切余程")
	if hit.is_empty(): return
	advance(f)
	f.state.actors.a.location = hit.anchors.a.duplicate(true); f.action.params = hit.traces.a.params.duplicate(true)
	check(f.action.params.travel_credit == 0 and Journey.validate_action(f.state, f.data, f.action).ok, "会面回退到真实锚点后能通过同一行程存档校验")
	var result: Dictionary = advance(f)
	check(result.ok and result.completed and f.state.actors.a.location.point == [120, 60], "道别后沿剩余折线抵达，不重走已经完成的路口")
	var map: Dictionary = Maps.regional_fixture()
	var route: Dictionary = Terrain.plan(map, [190, 430], [830, 290], "foot", {"crossing.main": true})
	f = fixture(map, route.points, 1500); f.state.flags = {"crossing.main": true}
	var finished: bool = false; var valid: bool = true
	for _tick in range(30):
		motion = Journey.motion(f.state, f.data, f.action)
		if not Contact.validate_trace(motion).ok: valid = false; break
		result = advance(f)
		if not result.ok: valid = false; break
		if result.completed: finished = true; break
		if not Journey.validate_action(f.state, f.data, f.action).ok: valid = false; break
	check(valid and finished and f.state.actors.a.location.point == [830, 290], "封渡后真实多折点北桥路线逐半日保持有效轨迹与可恢复行程直到抵达")

## 执行票据仅记录真实整数前缀；默认旧出口不变，零位移、失败和返还副本不得伪造移动。
func check_execution_capture() -> void:
	var f: Dictionary=fixture(Maps.plain_map(60,40,20),[[10,10],[40,10]],100)
	var g: Dictionary=f.duplicate(true); g.action=g.state.actions["action:a"]
	var plan_id: String=g.action.params.plan_id
	var ordinary: Dictionary=Journey.step(f.state,f.data,f.action)
	var captured: Dictionary=Journey.step(g.state,g.data,g.action,true)
	check(ordinary=={"ok":true,"completed":false} and f.state==g.state,"显式执行捕获不改变原真实位置、费用、人物疲劳或默认返回形状")
	var expected: Dictionary={"format":"HM-TERRAIN-EXECUTION/1","actor_id":"a","action_id":"action:a","action_start_tick":0,"source_tick":0,"plan_id":plan_id,"map_id":"map.test","from_point":[10,10],"to_point":[20,10],"route_index_before":1,"route_index_after":1,"travel_credit_before":0,"travel_credit_after":0,"result_location":g.state.actors.a.location.duplicate(true)}
	check(captured.get("execution")==expected,"成功步在实际写点生成完整整数执行票据，不重演行旅")
	if captured.get("execution") is Dictionary:
		var before: Dictionary=g.state.duplicate(true)
		captured.execution.to_point[0]=-1; captured.execution.result_location.point[0]=-1
		check(g.state==before,"修改返回执行票据不能回写真实位置")
	f=fixture(Maps.plain_map(40,40,20),[[10,10],[11,10]],1)
	for unused in range(3):
		captured=Journey.step(f.state,f.data,f.action,true); f.state.tick+=1
		check(captured.ok and not captured.has("execution") and f.state.actors.a.location.point==[10,10],"只积预算未保留整数位移时不生成行军来源")
	f.action.params.paused=true
	var before: Dictionary=f.state.duplicate(true)
	captured=Journey.step(f.state,f.data,f.action,true)
	check(not captured.ok and not captured.has("execution") and f.state==before,"暂停失败不能留下执行票据或任何行旅变化")
	f=fixture(Maps.plain_map(40,40,20),[[10,10],[20,10],[10,10]],500)
	captured=Journey.step(f.state,f.data,f.action,true)
	check(captured.ok and captured.completed and captured.get("execution",{}).get("route_index_after")==3,"绕经真实路点回到原点仍有已执行整数前缀")
	f=fixture(Maps.plain_map(40,40,20),[[10,10],[10,10]],10)
	captured=Journey.step(f.state,f.data,f.action,true)
	check(captured.ok and captured.completed and not captured.has("execution"),"仅零长度路点衔接不冒充真实行军")
	f=fixture(Maps.plain_map(40,40,20),[[10,10],[20,10]],100)
	f.data.settlements["settlement.end"]={"entry_place":"place.end","map_xy":[20,10]}
	f.data.places={"place.end":{"id":"place.end","settlement_id":"settlement.end"}}
	f.action.params.destination="settlement.end"; f.state.facts[f.action.params.plan_id].destination="settlement.end"
	captured=Journey.step(f.state,f.data,f.action,true)
	check(captured.ok and captured.completed and captured.get("execution",{}).get("to_point")==[20,10] and captured.get("execution",{}).get("result_location")==f.state.actors.a.location and f.state.actors.a.location.kind=="Settlement","到城切换地点后仍保留真正走过的整数端点")
	f=fixture(Maps.plain_map(40,40,20),[[10,10],[20,10]],100)
	f.action.params.terrain=false; before=f.state.duplicate(true)
	captured=Travel.step(f.state,f.data,f.action,true)
	check(captured.get("code")=="TerrainMarchRequired" and f.state==before,"显式地形执行捕获不能悄悄降到旧道路分支")

## 汇报实际纯模块验证范围，不将本测试标记为图形或完整世界验收。
func run_checks() -> void:
	check_corner_and_suffix()
	check_low_budget_and_pause()
	check_profiles_and_contact_time()
	check_distant_gate()
	check_replan_and_validation()
	check_contact_resume_and_normal_routes()
	check_query_purity()
	check_execution_capture()
	print("TERRAIN_JOURNEY_CHECKS=", checks, " FAILURES=", failures)
	quit(0 if failures == 0 else 1)
