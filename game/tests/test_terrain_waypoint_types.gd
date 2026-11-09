## 地形途经点类型回归：据点ID与自由整数终点可组合，预览不得改变输入或放宽已有拒绝规则。
extends SceneTree
const Journey = preload("res://core/terrain_journey.gd")
const Maps = preload("res://tests/test_world_terrain.gd")
var checks: int = 0
var failures: int = 0

## 延后运行纯规则测试，不启动游戏或读取玩家存档。
func _init() -> void:
	call_deferred("run_checks")

## 任一实际行为不符都令进程非零退出，脚本错误后的空返回也不能算通过。
func check(value: bool, label: String) -> void:
	checks += 1
	if not value: failures += 1; printerr("FAIL: ", label)

## 使用相邻地形测试的平原地图，复现原错误的据点坐标、预算和费用，不改正式内容。
static func fixture() -> Dictionary:
	var map: Dictionary = Maps.plain_map(200, 100, 20)
	map.budgets = {"foot": 100000, "army": 100000, "boat": 100000}
	map.money_per_tick = 1; map.fatigue_per_tick = 1
	var actor: Dictionary = {"id": "a", "alive": true, "available": true, "action_id": "", "fatigue": 0, "wallet_id": "wallet:a", "inventory_id": "inventory:a", "location": {"kind": "Settlement", "settlement_id": "city.west", "place_id": "place.west"}}
	var state: Dictionary = {"scenario_id": "scenario.test", "tick": 0, "actors": {"a": actor}, "actions": {}, "parties": {}, "flags": {}, "facts": {}, "wallets": {"wallet:a": {"balance": 100000, "reserved": {}}}, "inventories": {"inventory:a": {"lots": []}}}
	var data: Dictionary = {"world_maps": {"map.march": map}, "scenarios": {"scenario.test": {"world_map_id": "map.march"}}, "settlements": {"city.west": {"map_xy": [5, 5], "entry_place": "place.west"}, "village.east": {"map_xy": [185, 5], "entry_place": "place.east"}, "settlement.north": {"map_xy": [5, 85], "entry_place": "place.north"}}, "items": {}}
	return {"state": state, "data": data}

## 每次查询都比较完整输入；成功与失败均不能改时钟、钱包、实际位置、作者定义或参数数组。
func preview(f: Dictionary, destination: Variant, via: Variant, label: String) -> Dictionary:
	var inputs: Dictionary = {"fixture": f, "destination": destination, "via": via}
	var before: Dictionary = inputs.duplicate(true)
	var quote: Dictionary = Journey.plan(f.state, f.data, "a", destination, via)
	check(inputs == before, label + "：预览不修改任何输入")
	return quote

## 保留原始合法反例：String途经据点不能与Array终点直接比较而中断规划。
func check_mixed_destination() -> void:
	var f: Dictionary = fixture()
	var destination: Array = [5, 5]
	var via: Array = ["village.east"]
	var before: Dictionary = f.duplicate(true)
	var quote: Dictionary = preview(f, destination, via, "原始混合类型往返")
	check(quote.get("ok", false), "String途经据点与Array原点终点组合能成功规划真实往返")
	if not quote.get("ok", false): return
	check(quote.points == [[5, 5], [185, 5], [5, 5]], "同坐标终点仍按指定据点往返，不提前报AlreadyThere或省略途经点")
	check(quote.destination is Array and quote.destination == destination and quote.origin == "city.west", "自由终点保留Array形状与真实出发据点")
	check(quote.duration == 1 and quote.cost == 1 and quote.terrain, "往返仍采用既有预算和每半日费用")
	check(quote == preview(f, destination, via, "重复混合类型预览"), "相同输入重复查询给出一致报价")
	quote.destination[0] = -1; quote.points[0][0] = -2; quote.points[1][0] = -3
	check(f == before and destination == [5, 5] and via == ["village.east"], "修改返回目标或路线不污染调用方数组、真实位置和据点坐标")

## 自由点可在途经据点之外或恰好同坐标；后者不等于重复据点ID，不新增语义拒绝。
func check_other_mixed_points() -> void:
	var f: Dictionary = fixture()
	var quote: Dictionary = preview(f, [185, 85], ["settlement.north"], "途经据点后前往自由点")
	check(quote.get("ok", false) and quote.get("points", []) == [[5, 5], [5, 85], [185, 85]], "String途经点与不同Array终点按顺序连接")
	quote = preview(f, [185, 5], ["village.east"], "途经据点与自由终点同坐标")
	check(quote.get("ok", false) and quote.get("points", []) == [[5, 5], [185, 5]], "同坐标的Array终点合法，不能改成按坐标拒绝途经点")
	f.state.actors.a.location = {"kind": "Wilderness", "map_id": "map.march", "point": [5, 5], "origin": "city.west"}
	quote = preview(f, [5, 5], ["village.east"], "原野位置的混合类型往返")
	check(quote.get("ok", false) and quote.get("points", []) == [[5, 5], [185, 5], [5, 5]], "实际原野位置同样允许经据点回到自由原点")

## 同为据点ID时保留原规则；起点可作途经点，终点和重复途经ID仍须拒绝。
func check_string_destinations() -> void:
	var f: Dictionary = fixture()
	var quote: Dictionary = preview(f, "village.east", ["settlement.north"], "String途经点与String终点")
	check(quote.get("ok", false), "不同String途经点和String终点仍合法")
	if quote.get("ok", false):
		check(quote.destination is String and quote.destination == "village.east" and quote.points == [[5, 5], [5, 85], [185, 5]], "据点终点保留ID形状且路线经过指定据点")
	quote = preview(f, "city.west", ["village.east"], "String原点往返")
	check(quote.get("ok", false) and quote.get("points", []) == [[5, 5], [185, 5], [5, 5]], "原本支持的String原点往返保持合法")
	quote = preview(f, "village.east", ["city.west"], "起点作为途经点")
	check(quote.get("ok", false) and quote.get("points", []) == [[5, 5], [185, 5]], "不借类型修复套用道路规划器的起点排除规则")
	for via in [["village.east"], ["settlement.north", "settlement.north"], ["missing"]]:
		quote = preview(f, "village.east", via, "String途经ID拒绝边界")
		check(quote == {"ok": false, "code": "InvalidWaypoints"}, "终点ID、重复途经ID和未知途经ID仍返回InvalidWaypoints")
	quote = preview(f, "missing", [], "未知String终点")
	check(quote == {"ok": false, "code": "InvalidDestination"}, "未知String终点仍被明确拒绝")

## Array仅是终点的支持形状，不能反向把Array途经点当成新功能或遗漏后续坏途经元素。
func check_invalid_waypoints() -> void:
	var f: Dictionary = fixture()
	var invalid_vias: Array = [null, "settlement.north", [[5, 85]], [17], ["settlement.north", [185, 5]], ["settlement.north", "settlement.north"], ["missing"]]
	for destination in ["village.east", [185, 5]]:
		for via in invalid_vias:
			var quote: Dictionary = preview(f, destination, via, "无效途经参数" + str(via))
			check(quote == {"ok": false, "code": "InvalidWaypoints"}, "两种合法终点均拒绝Array途经点及原有错误途经输入")

## 不带途经点的两种终点沿用成功、AlreadyThere和InvalidDestination的原有边界与错误顺序。
func check_destination_boundaries() -> void:
	var f: Dictionary = fixture()
	for destination in ["village.east", [185, 5]]:
		var quote: Dictionary = preview(f, destination, [], "无途经点合法目标")
		check(quote.get("ok", false) and quote.get("points", []) == [[5, 5], [185, 5]], "两种合法终点在空途经列表下都成功")
	for destination in ["city.west", [5, 5]]:
		var quote: Dictionary = preview(f, destination, [], "无途经点原地目标")
		check(quote == {"ok": false, "code": "AlreadyThere"}, "不绕路且目标在原地时仍返回AlreadyThere")
	for destination in [[], [185], [185, 5, 0], [185.0, 5], [185, 5.0], [200, 5], null]:
		var quote: Dictionary = preview(f, destination, [], "无效目标" + str(destination))
		check(quote == {"ok": false, "code": "InvalidDestination"}, "错误形状、浮点和越界终点仍被拒绝，不靠类型修复转换输入")
	var quote: Dictionary = preview(f, "missing", [[5, 85]], "目标先于途经元素校验")
	check(quote == {"ok": false, "code": "InvalidDestination"}, "无效终点仍优先于途经元素错误")
	quote = preview(f, "missing", "settlement.north", "途经容器先于目标校验")
	check(quote == {"ok": false, "code": "InvalidWaypoints"}, "错误途经容器仍优先于无效终点")

## 本回归只覆盖途经点类型边界，不冒充完整行旅或世界验收。
func run_checks() -> void:
	check_mixed_destination()
	check_other_mixed_points()
	check_string_destinations()
	check_invalid_waypoints()
	check_destination_boundaries()
	print("TERRAIN_WAYPOINT_TYPE_CHECKS=", checks, " FAILURES=", failures)
	quit(0 if failures == 0 else 1)
