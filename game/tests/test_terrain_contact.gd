## 连续地形会面回归：同步运动、暂停拦路、真实锚点、余程裁切和已完成行动均用真实Godot验证。
extends SceneTree
const Contact = preload("res://core/terrain_contact.gd")
const Terrain = preload("res://core/world_terrain.gd")
const Maps = preload("res://tests/test_world_terrain.gd")
var checks: int = 0
var failures: int = 0

## 引擎初始化后执行纯规则测试，不创建游戏窗口或读写玩家存档。
func _init() -> void:
	call_deferred("run_checks")

## 记录有行为意义的断言，失败使独立测试退出非零。
func check(value: bool, label: String) -> void:
	checks += 1
	if not value: failures += 1; printerr("FAIL: ", label)

## 创建一段完整半日的实际移动，未到达的目标可继续超出本半日落点。
static func trace(person: String, from: Array, to: Array, target: Array = [], budget: int = 100) -> Dictionary:
	return {"actor_id": person, "action_id": "action:" + person, "map_id": "map.river", "origin": "settlement.origin",
		"params": {"points": [to.duplicate() if target.is_empty() else target.duplicate()], "mode": "foot", "paused": false, "encounter_id": "", "met_actions": [], "payer_wallet_id": "wallet:" + person, "money_per_tick": 7},
		"segments": [{"from": from.duplicate(), "to": to.duplicate(), "start_cost": 0, "end_cost": budget, "point_index": 0}], "budget": budget}

## 静止者保留真实待行目标，可被路过者遇到；没有把暂停伪装成已完成目的地。
static func blocker(person: String, point: Array, target: Array) -> Dictionary:
	var result: Dictionary = trace(person, point, point, target)
	result.params.paused = true
	return result

## 对向接近在半径四的首次时刻停在各自路线上，角色读序和预算单位不改变几何结果。
func check_head_on_and_radius() -> void:
	var a: Dictionary = trace("a", [10, 50], [110, 50])
	var b: Dictionary = trace("b", [110, 50], [10, 50], [], 200)
	var hit: Dictionary = Contact.intersection(a, b)
	check(not hit.is_empty(), "不同预算的对向旅行仍按同一个半日相遇")
	if hit.is_empty(): return
	check(hit.time_num == 480000000 and hit.time_den == 1000000000, "首次接近半径四发生在半日的0.48")
	check(hit.anchors.a.point == [58, 50] and hit.anchors.b.point == [62, 50], "两人保留各自真实落点，没有吸附到共同中点")
	check(hit.anchors.a.kind == "Wilderness" and hit.anchors.a.map_id == a.map_id and hit.anchors.a.origin == a.origin, "锚点携带完整原野位置身份")
	check(Contact.intersection(b, a) == hit, "反向读取两人得到完全相同的会面结构")
	var parallel: Dictionary = trace("b", [10, 54], [110, 54])
	check(not Contact.intersection(a, parallel).is_empty(), "平行保持四单位距离属于接触范围")
	parallel = trace("b", [10, 55], [110, 55])
	check(Contact.intersection(a, parallel).is_empty(), "相差五单位不能远程接触")
	var distant_map: Dictionary = b.duplicate(true); distant_map.map_id = "map.other"
	check(Contact.intersection(a, distant_map).is_empty(), "另一地图相同坐标也不能接触")

## 两条路径必须在同一时刻接近；早到后离开的几何交点与擦肩均不能制造相遇。
func check_simultaneous_time() -> void:
	var a: Dictionary = trace("a", [10, 50], [110, 50])
	var early: Dictionary = trace("b", [60, 10], [60, 110])
	early.segments = [{"from": [60, 10], "to": [60, 110], "start_cost": 0, "end_cost": 20, "point_index": 0}, {"from": [60, 110], "to": [60, 110], "start_cost": 20, "end_cost": 100, "point_index": 1}]
	check(Contact.validate_trace(early).ok and Contact.intersection(a, early).is_empty(), "几何十字相交但先后通过时不能会面")
	var crossing: Dictionary = trace("b", [60, 0], [60, 100])
	var hit: Dictionary = Contact.intersection(a, crossing)
	check(not hit.is_empty(), "同步斜向逼近交点会在进入半径时相遇")
	if not hit.is_empty():
		check(absf(float(hit.time_num) / hit.time_den - (0.5 - sqrt(8.0) / 100.0)) < 0.000000001, "无理数交圆时刻保持十亿分之一半日精度")
		check(hit.anchors.a.point[1] == 50 and hit.anchors.b.point[0] == 60, "非对向相遇仍分别落在各自轨迹上")
	var large_a: Dictionary = trace("a", [99000, 99000], [99900, 99900])
	var large_b: Dictionary = trace("b", [99900, 99000], [99000, 99900])
	check(not Contact.intersection(large_a, large_b).is_empty(), "接近最大地图坐标时双精度计算仍能识别同步接触")

## 折线接触只裁掉实际走完的路点，当前半段和后续目的地保持原样以供会话结束后续行。
func check_turns_and_resume() -> void:
	var a: Dictionary = trace("a", [10, 10], [60, 60])
	a.params.points = [[60, 10], [60, 60], [120, 60]]
	a.segments = [{"from": [10, 10], "to": [60, 10], "start_cost": 0, "end_cost": 50}, {"from": [60, 10], "to": [60, 60], "start_cost": 50, "end_cost": 100}]
	var b: Dictionary = blocker("b", [60, 40], [60, 100])
	var hit: Dictionary = Contact.intersection(a, b)
	check(not hit.is_empty(), "转过第一个路口后遇到暂停的旅人")
	if hit.is_empty(): return
	check(hit.time_num == 760000000 and hit.anchors.a.point == [60, 36], "第二条折线按累计成本计算真实时刻和落点")
	check(hit.traces.a.params.points == [[60, 60], [120, 60]], "删掉已走过的首折点并保留当前剩余段")
	check(hit.traces.b.params.points == [[60, 100]] and hit.traces.b.params.paused, "拦路者原暂停意愿和完整目标均被保留")
	check(hit.traces.a.params.payer_wallet_id == a.params.payer_wallet_id and hit.traces.a.params.money_per_tick == 7, "完整行动参数没有因生成余程而丢失")
	var restored: Dictionary = {"a": a.duplicate(true), "b": b.duplicate(true)}
	check(Contact.intersection(restored.a, restored.b) == hit, "恢复同一公开轨迹得到相同锚点与余程")
	var partial: Dictionary = trace("a", [10, 50], [50, 50], [110, 50])
	var at_end: Dictionary = blocker("b", [54, 50], [54, 100])
	hit = Contact.intersection(partial, at_end)
	check(not hit.is_empty() and hit.time_num == Contact.TIME_DEN and hit.traces.a.params.points == [[110, 50]], "半日预算用尽的局部终点仍保留尚未完成的最终目标")
	var turn: Dictionary = blocker("b", [64, 10], [64, 80])
	hit = Contact.intersection(a, turn)
	check(not hit.is_empty() and hit.time_num == 500000000 and hit.traces.a.params.points == [[60, 60], [120, 60]], "恰在转折点接触只删除已经抵达的折点")

## 自由目标和据点目标都在抵达后结束，尾段静止者不能被再次装成空余程行动。
func check_finished_and_paused() -> void:
	var finished: Dictionary = trace("a", [10, 50], [30, 50])
	finished.segments = [{"from": [10, 50], "to": [30, 50], "start_cost": 0, "end_cost": 20, "point_index": 0}, {"from": [30, 50], "to": [30, 50], "start_cost": 20, "end_cost": 100, "point_index": 1}]
	check(Contact.intersection(finished, trace("b", [110, 50], [10, 50])).is_empty(), "到达自由目标后的静止尾段不能重新启动已结束旅程")
	check(Contact.intersection(finished, blocker("b", [34, 50], [34, 100])).is_empty(), "首次接近恰好完成最后路点也不生成空余程")
	var held: Dictionary = blocker("a", [30, 50], [110, 50])
	check(not Contact.intersection(held, trace("b", [110, 50], [10, 50])).is_empty(), "仍有未走余程的原地暂停者可以拦路")
	check(Contact.intersection(held, blocker("b", [32, 50], [100, 50])).is_empty(), "两人整半日都静止时不重复产生道路会面")

## 城门圆心附近继续使用场所交互；两人一起离开入口后仍能在原野首次有效相遇。
func check_entrances() -> void:
	var a: Dictionary = trace("a", [10, 50], [110, 50])
	var b: Dictionary = trace("b", [110, 50], [10, 50])
	check(Contact.intersection(a, b, {"map.river": [[60, 50]]}).is_empty(), "城门半径内的会面由正常到城交互接管")
	check(not Contact.intersection(a, b, {"map.other": [[60, 50]]}).is_empty(), "别的地图入口不排除当前地图的真实相遇")
	b = trace("b", [13, 50], [113, 50])
	var hit: Dictionary = Contact.intersection(a, b, {"map.river": [[10, 50]]})
	check(not hit.is_empty() and hit.time_num > 40000000, "同步离开入口后选择首次城外有效时刻")
	var entrances: Dictionary = {"map.river": [[10, 50], [20, 50]]}
	var reversed: Dictionary = {"map.river": [[20, 50], [10, 50]]}
	check(Contact.intersection(a, b, entrances) == Contact.intersection(a, b, reversed), "多个入口排除区间不依赖表的读序")

## 多人争用按接触时刻和ID唯一分配，重复行动、损坏输入以及返回对象改动都不会修改源轨迹。
func check_selection_and_purity() -> void:
	var a: Dictionary = trace("a", [10, 50], [110, 50])
	var b: Dictionary = blocker("b", [60, 50], [120, 50])
	var c: Dictionary = blocker("c", [60, 50], [120, 50])
	var source: Array = [a, b, c]; var before: Array = source.duplicate(true)
	var hits: Array = Contact.detect(source)
	check(hits.size() == 1 and hits[0].a == "a" and hits[0].b == "b", "同刻竞争按稳定人物ID选择且每人只参加一场")
	check(hits == Contact.detect([c, b, a]), "倒序读取全部旅行仍选择同一相遇")
	check(source == before, "接触查询不改位置、预算、暂停或任何行动参数")
	if not hits.is_empty():
		hits[0].traces.a.params.points[0][0] = 999
		hits[0].anchors.a.point[0] = 999
	check(source == before, "返回锚点和剩余路点与输入没有可写别名")
	b.params.met_actions = [a.action_id]
	check(Contact.intersection(a, b).is_empty(), "任一方记录的真实行动去重都阻止再次相遇")
	b.params.met_actions = []; b.params.encounter_id = "encounter:old"
	check(Contact.detect([a, b]).is_empty(), "已挂会面的行动不能同时生成第二场")
	b.params.encounter_id = ""
	check(Contact.detect([a, a.duplicate(true), b]).is_empty(), "同一人物的重复轨迹整组拒绝而非依赖读序选一条")
	c.segments[0].from = [40, 50]; c.segments[0].to = [40, 50]
	hits = Contact.detect([a, b, c])
	check(hits.size() == 1 and hits[0].b == "c", "不同时刻优先先遇到的旅人而非人物ID")

## 明确拒绝轨迹断裂、缺失时间和非法路点，不能用默认值伪造可恢复会面。
func check_invalid() -> void:
	var a: Dictionary = trace("a", [10, 50], [110, 50])
	check(Contact.validate_trace(a).ok, "完整轨迹通过校验")
	var bad: Dictionary = a.duplicate(true); bad.budget = 0
	check(not Contact.validate_trace(bad).ok, "零预算不会进入除法")
	bad = a.duplicate(true); bad.segments[0].end_cost = 99
	check(Contact.validate_trace(bad).get("code") == "IncompleteTerrainTrace", "缺少静止尾段的未覆盖时间拒绝")
	bad = a.duplicate(true); bad.segments[0].start_cost = 1
	check(not Contact.validate_trace(bad).ok, "不能忽略半日开始的一段时间")
	bad = a.duplicate(true); bad.params.points[0] = [110.5, 50]
	check(not Contact.validate_trace(bad).ok, "小数目标不能假装持久整数世界位置")
	bad = a.duplicate(true); bad.segments[0].point_index = 9
	check(not Contact.validate_trace(bad).ok, "超出余程的路点索引拒绝")
	bad = a.duplicate(true); bad.segments[0].end_cost = 50
	bad.segments.append({"from": [111, 50], "to": [120, 50], "start_cost": 50, "end_cost": 100, "point_index": 0})
	check(Contact.validate_trace(bad).get("code") == "DiscontinuousTerrainTrace", "两段之间不能瞬移一格后继续参与接触")
	check(Contact.detect([{}, 5, bad]).is_empty(), "损坏轨迹返回空候选而不写入世界")
	bad = a.duplicate(true); bad.budget = 9223372036854775807
	check(not Contact.validate_trace(bad).ok, "超大预算不能把相邻时间压缩成零跨度")
	bad = a.duplicate(true); bad.segments[0].from[0] = NAN
	check(not Contact.validate_trace(bad).ok, "临时交点也不能包含非数坐标")
	bad = a.duplicate(true); bad.segments[0].end_cost = INF
	check(not Contact.validate_trace(bad).ok, "无限成本不能进入时间求交")
	bad = a.duplicate(true); bad.budget = 4294967294; bad.segments[0].end_cost = bad.budget
	check(Contact.validate_trace(bad).ok, "信用累计后的安全整数预算上界可计算")
	bad.budget += 1; bad.segments[0].end_cost = bad.budget
	check(not Contact.validate_trace(bad).ok, "超过明确安全上界的轨迹预算拒绝")
	var carried: Dictionary = a.duplicate(true); carried.params.travel_credit = 17; carried.params.route_index = 3
	var met: Dictionary = Contact.intersection(carried, blocker("b", [60, 50], [120, 50]))
	check(not met.is_empty() and met.traces.a.params.travel_credit == 0 and carried.params.travel_credit == 17, "相遇作废未行预算且不改步初信用副本")
	check(not met.is_empty() and met.traces.a.params.route_index == 3, "同一冻结路段中途相遇保留原路段索引")
	bad = a.duplicate(true); bad.params.route_index = "1"
	check(not Contact.validate_trace(bad).ok, "坏类型的冻结索引不能进入余程加法")
	bad = a.duplicate(true); bad.params.travel_credit = -1
	check(not Contact.validate_trace(bad).ok, "轨迹不能携带负信用进入接触恢复")

## 把真实寻路输出转换成一整段半日轨迹，仅用于验证不同地图的实际通行与锚点恢复。
static func route_trace(person: String, route: Dictionary) -> Dictionary:
	var result: Dictionary = trace(person, route.points[0], route.points[-1], [], route.cost)
	result.params.points = route.points.slice(1).duplicate(true)
	result.segments = []
	var cost: int = 0
	for index in range(route.segment_costs.size()):
		result.segments.append({"from": route.points[index].duplicate(), "to": route.points[index + 1].duplicate(), "start_cost": cost, "end_cost": cost + route.segment_costs[index], "point_index": index})
		cost += route.segment_costs[index]
	return result

## 使用最终原画样本与另一尺寸地图的真实折线，验证整数锚点及余程仍满足同一地形规则。
func check_real_terrain_paths() -> void:
	for sample in [{"map": Maps.regional_fixture(), "from": [190, 430], "to": [830, 290], "flags": {}}, {"map": Maps.regional_fixture(), "from": [190, 430], "to": [830, 290], "flags": {"crossing.main": true}}, {"map": Maps.crossing_fixture(), "from": [15, 35], "to": [145, 35], "flags": {"ford.closed": true}}]:
		var forward: Dictionary = Terrain.plan(sample.map, sample.from, sample.to, "foot", sample.flags)
		var back: Dictionary = Terrain.plan(sample.map, sample.to, sample.from, "foot", sample.flags)
		check(forward.ok and back.ok, "样本两方向都从真实整数入口沿当前开放地形出发")
		if not forward.ok or not back.ok: continue
		var hit: Dictionary = Contact.intersection(route_trace("a", forward), route_trace("b", back))
		check(not hit.is_empty(), "实际渡口或封渡绕桥路线中发生同步接触")
		if hit.is_empty(): continue
		for person in ["a", "b"]:
			var here: Array = hit.anchors[person].point
			check(Terrain.point_valid(sample.map, here, "foot", sample.flags), "实际接触整数锚点仍处于开放地形")
			var valid: bool = true
			for target in hit.traces[person].params.points:
				if not Terrain.segment_cost(sample.map, here, target, "foot", sample.flags).ok: valid = false
				here = target
			check(valid, "从接触锚点恢复保留余程不会跨越封闭地形")

## 执行完整纯规则回归并报告实际结果，本脚本不把规则通过写成图形流程验收。
func run_checks() -> void:
	check_head_on_and_radius()
	check_simultaneous_time()
	check_turns_and_resume()
	check_finished_and_paused()
	check_entrances()
	check_selection_and_purity()
	check_invalid()
	check_real_terrain_paths()
	print("TERRAIN_CONTACT_CHECKS=", checks, " FAILURES=", failures)
	quit(0 if failures == 0 else 1)
