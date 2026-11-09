## 验证通用道路规划器能处理第二拓扑、途经点、封锁和可配置费用。
extends SceneTree
var checks: int = 0
var failures: int = 0
## 延后执行以读取真实脚本资源。
func _init() -> void:
	call_deferred("_run")
## 明确报告缺失行为，不让缺失模块静默通过。
func check(ok: bool, label: String) -> void:
	checks += 1
	if not ok: failures += 1; printerr("FAIL: ",label)
## 制作与现有城名无关的小型路网，所有边均有合法时耗。
func fixture() -> Dictionary:
	return {"settlements":{"a":{},"b":{},"c":{},"d":{}},"routes":{
		"ab":{"id":"ab","origin":"a","destination":"b","duration_ticks":2,"cost_per_tick":11},
		"bc":{"id":"bc","origin":"b","destination":"c","duration_ticks":1,"blocked_until_tick":5},
		"ad":{"id":"ad","origin":"a","destination":"d","duration_ticks":2},
		"dc":{"id":"dc","origin":"d","destination":"c","duration_ticks":3}}}
## 查验路径、开通边界和坏输入，并保证预览不改定义。
func _run() -> void:
	var path: String = "res://core/route_graph.gd"
	check(FileAccess.file_exists(path),"存在纯道路规划器")
	if not FileAccess.file_exists(path): print("ROUTE_GRAPH_CHECKS=",checks," FAILURES=",failures); quit(1); return
	var graph = load(path)
	var data: Dictionary = fixture()
	var copy: Dictionary = data.duplicate(true)
	var route: Dictionary = graph.plan(data,"a","c",0,[])
	check(route.ok and route.path == ["ad","dc"] and route.duration == 5, "当前封路选择替代道路")
	check(route.ok and route.cost == 40, "缺省费用沿用每半日8钱")
	route = graph.plan(data,"a","c",5,[])
	check(route.ok and route.path == ["ab","bc"] and route.cost == 30, "开通边界生效且费用逐边累计")
	route = graph.plan(data,"a","c",5,["d"])
	check(route.ok and route.path == ["ad","dc"], "途经点参与真实规划")
	check(not graph.plan(data,"a","c",0,["b"]).ok, "指定不可通行途经路线返回失败")
	check(not graph.plan(data,"a","c",0,["unknown"]).ok, "拒绝未知途经点")
	check(not graph.plan(data,"a","c",0,"b").ok, "拒绝错误途经数据类型")
	check(not graph.plan(data,"a","c",0,["a"]).ok, "拒绝以起点反复绕行")
	check(not graph.plan(data,"c","a",0,[]).ok, "不把单向边当作双向")
	check(graph.plan(data,"a","a",0,[]).path.is_empty(), "内部零距离查询合法")
	check(data == copy, "所有查询不写入路网")
	var travel = load("res://core/travel_rules.gd")
	data.routes.ab.duration_ticks = 4
	var state: Dictionary={"tick":5,"actors":{"hero":{"location":{"kind":"OnRoute","edge_id":"ab","progress":100,"origin":"a"}}}}
	var amended: Dictionary=travel.replan(state,data,"hero","c",["b"])
	check(amended.ok and amended.path == ["ab","bc"] and amended.duration == 4,"道路另一端作为途经点可顺路满足")
	print("ROUTE_GRAPH_CHECKS=",checks," FAILURES=",failures)
	quit(0 if failures == 0 else 1)
