## 地形纯查询回归：成本、连续端点、模式、过河设施、关口与封锁改道均使用真实Godot执行。
extends SceneTree
const Terrain = preload("res://core/world_terrain.gd")
var checks: int = 0
var failures: int = 0

## 引擎初始化后执行规则断言；本脚本不启动游戏、不读写玩家存档。
func _init() -> void:
	call_deferred("run_checks")

## 记录有具体行为含义的断言，失败使测试退出非零。
func check(value: bool, label: String) -> void:
	checks += 1
	if not value: failures += 1; printerr("FAIL: ", label)

## 创建独立测试地图，格子各有自己的字典，修改某格不会污染其余地形或正式内容。
static func plain_map(width: int, height: int, size: int) -> Dictionary:
	var cells: Array = []
	for _index in range((width / size) * (height / size)): cells.append({"terrain": "plain"})
	return {"width": width, "height": height, "cell_size": size, "cells": cells}

## 与生成原画对应的1000×600样本：中央河、中段渡口、西北山关和北桥构成实际可改道的环路。
static func regional_fixture() -> Dictionary:
	var map: Dictionary = plain_map(1000, 600, 20)
	map.id = "map.terrain_fixture"
	for y in range(30):
		var river_left: int = 22 if y < 4 or y == 14 else 23
		var river_right: int = 27 if y == 14 else (28 if y >= 16 else 26)
		for x in range(50):
			var cell: Dictionary = map.cells[y * 50 + x]
			if x >= river_left and x <= river_right: cell.terrain = "water"
			elif y >= 4 and y <= 9 and x < river_left or y >= 2 and y <= 9 and x >= 33: cell.terrain = "mountain"
			elif y >= 16 and x < 9 or y >= 19 and x >= 36: cell.terrain = "forest"
			elif y == 14 or x == 9 or y == 2 and x >= 9 and x <= 30: cell.terrain = "road"
	for y in range(4, 10): map.cells[y * 50 + 9] = {"terrain": "mountain", "crossing": "pass", "blocked_flag": "gate.north"}
	for x in range(22, 28): map.cells[14 * 50 + x] = {"terrain": "water", "crossing": "ford", "blocked_flag": "crossing.main"}
	for x in range(22, 27): map.cells[2 * 50 + x] = {"terrain": "water", "crossing": "bridge", "blocked_flag": "bridge.north"}
	for point in [[30, 3], [30, 4], [30, 5], [31, 6], [32, 7], [32, 8], [31, 9], [31, 10], [30, 11], [30, 12], [30, 13]]: map.cells[point[1] * 50 + point[0]].terrain = "road"
	return map

## 第二地图使用不同尺寸和网格，浅滩关闭后可绕去桥梁；不按第一地图或聚落名称选路径。
static func crossing_fixture() -> Dictionary:
	var map: Dictionary = plain_map(160, 120, 20)
	for y in range(6): map.cells[y * 8 + 3].terrain = "water"
	for x in range(8):
		if x != 3: map.cells[8 + x].terrain = "road"
	map.cells[8 + 3] = {"terrain": "water", "crossing": "ford", "blocked_flag": "ford.closed"}
	map.cells[4 * 8 + 3] = {"terrain": "water", "crossing": "bridge", "blocked_flag": "bridge.closed"}
	return map

## 覆盖公开输入边界，不能依靠取整、默认开放或把渡口放在陆地来制造通路。
func check_validation() -> void:
	var map: Dictionary = regional_fixture()
	check(Terrain.validate_map(map).ok, "规则接受与原画对应的区域定义")
	var bad: Dictionary = map.duplicate(true); bad.width = 1001
	check(Terrain.validate_map(bad).get("code") == "InvalidMapDimensions", "宽高必须与网格完整对应")
	bad = map.duplicate(true); bad.cells.pop_back()
	check(Terrain.validate_map(bad).get("code") == "InvalidMapCells", "缺格不能被默认补成陆地")
	bad = map.duplicate(true); bad.cells[0] = {"terrain": "plain", "crossing": "bridge"}
	check(Terrain.validate_map(bad).get("code") == "InvalidCrossingTerrain", "陆地格不能伪装架桥水域")
	bad = map.duplicate(true); bad.costs = {"foot": {"road": -1}}
	check(not Terrain.validate_map(bad).ok, "负成本不能破坏寻路")
	check(not Terrain.point_valid(map, [1000, 300]) and not Terrain.point_valid(map, [-1, 300]), "图外位置拒绝，不夹回边界")
	check(not Terrain.point_valid(map, [175.1, 425]), "浮点位置不能悄悄取整")
	check(Terrain.plan(map, [175, 425], [825, 325], "flight").get("code") == "InvalidTravelMode", "未知移动模式不能获得通行权")
	check(Terrain.plan(map, [175, 425], [825, 325], "foot", {"gate.north": 1}).get("code") == "InvalidTerrainFlags", "非布尔旗标不能误判为开关状态")

## 验证地形和模式直接改变实际消耗，水路能力不因桥、渡口或配置正成本而变成陆行能力。
func check_costs_and_modes() -> void:
	var map: Dictionary = plain_map(200, 120, 20)
	for x in range(10): map.cells[2 * 10 + x].terrain = "road"
	for x in [2, 3]: map.cells[4 * 10 + x].terrain = "forest"
	check(Terrain.segment_cost(map, [10, 10], [90, 10]).get("cost") == 800, "平原直线按真实距离消耗")
	check(Terrain.segment_cost(map, [10, 50], [90, 50]).get("cost") == 640, "道路比同距离平原省力")
	check(Terrain.segment_cost(map, [10, 90], [90, 90]).get("cost") == 1120, "直线逐段累计林地与平原成本")
	check(Terrain.segment_cost(map, [10, 50], [90, 50], "army").get("cost") == 960, "军团使用自己的通行成本")
	map.costs = {"foot": {"forest": 25}}
	check(Terrain.segment_cost(map, [10, 90], [90, 90]).get("cost") == 1400, "作者覆盖成本改变同一路径的真实耗时基础")
	var river: Dictionary = regional_fixture()
	check(not Terrain.point_valid(river, [475, 275]) and Terrain.point_valid(river, [475, 275], "boat"), "无渡口水域仅水上模式允许")
	check(Terrain.point_valid(river, [475, 290]) and Terrain.point_valid(river, [475, 290], "army"), "渡口提供步行和军团的真实过河位置")
	check(Terrain.segment_cost(river, [475, 290], [525, 290]).get("cost") == 1300, "渡口的陆路消耗不是普通道路")
	check(Terrain.segment_cost(river, [475, 290], [525, 290], "boat").get("cost") == 400, "渡口仍保留连续水路")
	check(Terrain.plan(river, [475, 25], [525, 575], "boat").ok, "船沿真实中央河道贯通南北")
	river.costs = {"boat": {"plain": 1}, "foot": {"water": 1}}
	check(not Terrain.point_valid(river, [175, 425], "boat") and not Terrain.point_valid(river, [475, 275]), "正成本不能绕过陆水能力约束")

## 门关和渡口封锁都来自调用方当前旗标，开启查询不预测将来，关闭后不得沿缓存路线强行穿越。
func check_gates_and_routes() -> void:
	var map: Dictionary = regional_fixture()
	var path: Dictionary = Terrain.plan(map, [190, 430], [830, 290])
	check(path.ok and path.points[0] == [190, 430] and path.points[-1] == [830, 290], "区域路径保留两端真实世界坐标")
	check_path(map, path, "foot", {})
	check(not Terrain.segment_cost(map, [175, 425], [825, 425]).ok, "相同河流在渡口以外不能画直线穿越")
	var northern: Dictionary = Terrain.plan(map, [190, 430], [830, 290], "foot", {"crossing.main": true})
	check(northern.ok and northern.cost > path.cost, "主渡口关闭后沿山关和北桥绕行，增加真实通行消耗")
	check_path(map, northern, "foot", {"crossing.main": true})
	check(Terrain.plan(map, [190, 430], [830, 290], "foot", {"crossing.main": true, "bridge.north": true}).get("code") == "NoRoute", "区域两处跨河设施都关闭后确实无路")
	check(Terrain.plan(map, [190, 430], [830, 290], "foot", {"crossing.main": true, "gate.north": true}).get("code") == "NoRoute", "渡口和北山关同时关闭时不能绕过山脊偷到北桥")
	check(Terrain.plan(map, [190, 25], [190, 430], "army").ok, "山岭关口打开时军团能通过")
	check(not Terrain.point_valid(map, [190, 110], "army", {"gate.north": true}), "关闭山关自身不可驻留")
	check(Terrain.plan(map, [190, 25], [190, 430], "army", {"gate.north": true, "bridge.north": true}).get("code") == "NoRoute", "北桥也关闭时军团不能从关闭关口旁穿过不可通山岭")
	map.costs = {"foot": {"mountain": 36}}
	var open: Dictionary = Terrain.plan(map, [190, 25], [190, 430], "foot", {"bridge.north": true})
	var detour: Dictionary = Terrain.plan(map, [190, 25], [190, 430], "foot", {"gate.north": true, "bridge.north": true})
	check(open.ok and detour.ok and detour.cost > open.cost, "明示可徒步翻山后，闭关只能付出额外地形消耗绕行")
	check_path(map, detour, "foot", {"gate.north": true, "bridge.north": true})
	var second: Dictionary = crossing_fixture()
	var direct: Dictionary = Terrain.plan(second, [15, 35], [145, 35])
	var reroute: Dictionary = Terrain.plan(second, [15, 35], [145, 35], "foot", {"ford.closed": true})
	check(direct.ok and reroute.ok and reroute.cost > direct.cost, "另一尺寸地图在浅滩关闭后改走桥梁")
	check_path(second, reroute, "foot", {"ford.closed": true})
	check(Terrain.plan(second, [15, 35], [145, 35], "foot", {"ford.closed": true, "bridge.closed": true}).get("code") == "NoRoute", "两条真实跨河设施均关闭后无路可走")
	check(Terrain.segment_cost(second, [70, 85], [70, 95], "boat").get("cost") == 80, "桥梁下仍是可航水面")
	check(Terrain.segment_cost(second, [65, 90], [75, 90]).get("cost") == 80, "桥面提供独立的陆路成本")

## 任意途中点可折返或改道；相同公开输入重复查询与返回数组改动都不能修改世界、旗标或起点。
func check_continuity_and_purity() -> void:
	var map: Dictionary = crossing_fixture(); var flags: Dictionary = {"ford.closed": true}
	var from: Array = [17, 89]; var to: Array = [145, 35]
	var world: Dictionary = {"tick": 8, "revision": 9, "wallet": 721, "rng": {"route": 7781}, "flags": flags, "map": map, "position": from}
	var before: Dictionary = world.duplicate(true)
	var path: Dictionary = Terrain.plan(map, from, to, "foot", flags)
	check(path.ok and path.points[0] == from and path.points[-1] == to, "从非格心途中坐标改道，不回到原出发城门")
	var back: Dictionary = Terrain.plan(map, from, [15, 35], "foot", flags)
	check(back.ok and back.points[0] == from and back.points[-1] == [15, 35], "途中折返也从同一个真实位置开始")
	var restored: Dictionary = world.duplicate(true)
	check(path == Terrain.plan(restored.map, restored.position, to, "foot", restored.flags), "完整恢复公开输入后得到同一折线和成本")
	check(path == Terrain.plan(map, from, to, "foot", flags), "再次打开地图不重选随机路径")
	check(world == before, "寻路不修改tick、revision、钱、随机流、位置、旗标或地形")
	path.points[0][0] = 19
	check(world == before and from == [17, 89], "返回路线不是输入位置的可写别名")
	var stay: Dictionary = Terrain.plan(map, from, from, "foot", flags)
	check(stay.ok and stay.cost == 0 and stay.points == [from] and stay.segment_costs.is_empty(), "原地查询不产生隐藏距离或移动步骤")
	var odd: Dictionary = plain_map(9, 9, 3)
	check(Terrain.plan(odd, [0, 0], [8, 8]).ok, "非偶数格距和第二种坐标尺度仍能规划")

## 阻挡检查必须覆盖直线全部地形与角点；内部八邻接不能斜穿两块禁行地形的交角。
func check_edges_and_weighted_choice() -> void:
	var corner: Dictionary = plain_map(40, 40, 20)
	corner.cells[1].terrain = "mountain"; corner.cells[2].terrain = "mountain"
	check(not Terrain.segment_cost(corner, [10, 10], [30, 30]).ok and not Terrain.segment_cost(corner, [30, 30], [10, 10]).ok, "斜向及反向都不能擦角跨障碍")
	check(not Terrain.point_valid(corner, [20, 20]) and Terrain.plan(corner, [10, 10], [30, 30]).get("code") == "NoRoute", "格角不是可驻留的穿墙漏洞")
	var narrow: Dictionary = plain_map(400, 20, 2)
	for y in range(10): narrow.cells[y * 200 + 80].terrain = "water"
	check(not Terrain.segment_cost(narrow, [1, 1], [399, 19]).ok, "长直线不会采样跳过两单位宽水障")
	var weighted: Dictionary = plain_map(300, 150, 50)
	for x in range(6): weighted.cells[x].terrain = "road"
	for x in [2, 3]: weighted.cells[6 + x].terrain = "forest"
	weighted.costs = {"foot": {"road": 2, "forest": 120}}
	var route: Dictionary = Terrain.plan(weighted, [25, 75], [275, 75])
	var straight: Dictionary = Terrain.segment_cost(weighted, [25, 75], [275, 75])
	check(route.ok and straight.ok and route.cost < straight.cost, "寻路按地形消耗选更省力的道路，不总是最短直线")
	check_path(weighted, route, "foot", {})

## 使用公开直线查询复核返回的全部路段，证明预览与可提交折线同源且成本可核对。
func check_path(map: Dictionary, path: Dictionary, mode: String, flags: Dictionary) -> void:
	check(path.ok, "待复核路径实际存在")
	if not path.ok: return
	var total: int = 0; var valid: bool = true
	for index in range(1, path.points.size()):
		var segment: Dictionary = Terrain.segment_cost(map, path.points[index - 1], path.points[index], mode, flags)
		valid = valid and segment.ok and segment.get("cost") == path.segment_costs[index - 1]
		total += segment.get("cost", 0)
	check(valid and total == path.cost, "每条折线均合法且总成本等于同源各段之和")

## 运行所有有界规则检查，可显式导出原画对应fixture；不会把测试地图写入正式作者内容。
func run_checks() -> void:
	check_validation(); check_costs_and_modes(); check_gates_and_routes(); check_continuity_and_purity(); check_edges_and_weighted_choice()
	for arg in OS.get_cmdline_user_args():
		if arg.begins_with("--fixture-out="):
			var path: String = arg.trim_prefix("--fixture-out=")
			var file: FileAccess = FileAccess.open(path, FileAccess.WRITE)
			check(file != null, "可写入显式指定的隔离fixture文件")
			if file != null: file.store_string(JSON.stringify(regional_fixture(), "  ")); file.close()
	print("WORLD_TERRAIN_CHECKS=", checks, " FAILURES=", failures)
	quit(0 if failures == 0 else 1)
