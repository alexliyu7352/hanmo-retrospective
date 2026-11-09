## 验证六角空间的真实邻接、地形路径与对称射界，不用绘制方格代替六角规则。
extends SceneTree
const Hex=preload("res://core/military_hex.gd")
const F=preload("res://tests/military_fixture.gd")
var checks: int=0
var failures: int=0

## 在纯脚本环境执行有限空间样本，不打开玩家存档或编辑作者内容。
func _init() -> void:
	call_deferred("run_checks")

## 逐项保留失败原因，进程退出码反映任何一项几何不变量破坏。
func check(value: bool, label: String) -> void:
	checks+=1
	if not value: failures+=1; printerr("FAIL: ",label)

## 检查两个地图、不同代价、阻挡、控制区与坐标边界，确保预览结果可直接用于规则移动。
func run_checks() -> void:
	var data: Dictionary=F.data(); var board: Dictionary=data.battlefields["field.open"]
	var troop: Dictionary=data.troop_types["troop.foot"]
	check(Hex.validate(board).ok and Hex.validate(data.battlefields["field.ford"]).ok,"两种六角场地通过同一检查")
	var neighbors: Array=Hex.neighbors([0,0])
	check(neighbors.size()==6 and [1,-1] in neighbors and not [1,1] in neighbors,"轴向坐标只有六邻接而非方格八邻接")
	for neighbor in neighbors: check(Hex.distance([0,0],neighbor)==1 and Hex.distance(neighbor,[0,0])==1,"六方向距离对称")
	check(Hex.distance([0,0],[2,2])==4 and Hex.distance([-2,1],[1,-2])==3,"正负轴向坐标均用立方距离")
	check(not Hex.point([0.5,1]) and not Hex.point([0]) and not Hex.point([1025,0]),"小数和越界坐标拒绝")
	var duplicate: Dictionary=board.duplicate(true); duplicate.cells.append(duplicate.cells[0].duplicate())
	check(Hex.validate(duplicate).code=="DuplicateHexCell","重复坐标不被后写覆盖")
	var unknown: Dictionary=board.duplicate(true); unknown.cells[0].terrain="sand_magic"
	check(Hex.validate(unknown).code=="InvalidHexCell","未知地形清楚失败")
	var one_side: Dictionary=board.duplicate(true); one_side.spawns[1].side=0
	check(Hex.validate(one_side).code=="MissingHexSpawnSide","地图须有两侧可配置出生位置")
	var blocked_entry: Dictionary=board.duplicate(true); Hex.cell_at(blocked_entry,[0,1]).terrain="wall"
	check(Hex.validate(blocked_entry).code=="BlockedHexEntry","出生入口不能位于完整城墙中")
	var original: Dictionary=board.duplicate(true)
	var path: Dictionary=Hex.path(board,[0,1],[2,0],troop,2)
	check(path.ok and path.cost==2 and path.path[0]==[0,1] and path.path[-1]==[2,0],"六角对角移动与预览路径一致")
	check(not Hex.path(board,[0,1],[3,0],troop,2).ok,"超出移动预算拒绝")
	check(not Hex.path(board,[0,1],[1,1],troop,2,[[1,1]]).ok,"不可进入已有部队占格")
	check(not Hex.path(board,[0,1],[2,1],troop,2,[],[[1,1],[1,0]]).ok,"进入敌控制区后不能继续穿过")
	check(Hex.path(board,[0,1],[1,1],troop,2,[],[[1,1]]).ok,"可进入控制区并停步")
	var forest: Dictionary=board.duplicate(true); Hex.cell_at(forest,[1,1]).terrain="forest"
	check(Hex.movement_cost(forest,[1,1],troop)==2 and not Hex.path(forest,[0,1],[1,1],troop,1).ok,"森林成本真实消耗两点移动")
	check(Hex.movement_cost(forest,[1,1],data.troop_types["troop.horse"])==3,"骑兵使用自己的森林代价")
	check(not Hex.line_of_sight(forest,[0,1],[2,1]),"森林阻断穿越射界")
	Hex.cell_at(forest,[1,1]).blocks_sight=false
	check(Hex.line_of_sight(forest,[0,1],[2,1]) and Hex.cover(forest,[1,1])==20,"作者可开放疏林视线并保留掩护")
	var hill: Dictionary=board.duplicate(true); Hex.cell_at(hill,[1,1]).terrain="hill"
	check(not Hex.line_of_sight(hill,[0,1],[2,1]),"低地射线不能穿过较高山脊")
	Hex.cell_at(hill,[0,1]).terrain="hill"
	check(Hex.line_of_sight(hill,[0,1],[2,1]),"高处射手能够越过同高的中间地势")
	var wall: Dictionary=board.duplicate(true); Hex.cell_at(wall,[1,1]).terrain="wall"
	check(Hex.movement_cost(wall,[1,1],troop)<0 and not Hex.line_of_sight(wall,[0,1],[2,1]),"墙同时阻挡移动与视线")
	var boundary: Dictionary=board.duplicate(true); Hex.cell_at(boundary,[1,0]).blocks_sight=true
	check(not Hex.line_of_sight(boundary,[0,0],[1,1]) and not Hex.line_of_sight(boundary,[1,1],[0,0]),"沿两格边界的射线不能单向穿墙")
	check(not Hex.line_of_sight(board,[0,0],[20,20]),"场外坐标不被当成透明地形")
	var ford: Dictionary=data.battlefields["field.ford"]
	var crossing: Dictionary=Hex.path(ford,[1,1],[3,1],troop,2)
	check(crossing.ok and [2,1] in crossing.path and Hex.movement_cost(ford,[2,0],troop)<0,"第二场地只能从真实渡口跨越水域")
	check(board==original and path==Hex.path(board,[0,1],[2,0],troop,2),"路径查询输入不变且重复结果一致")
	print("MILITARY_HEX_CHECKS=",checks," FAILURES=",failures)
	quit(0 if failures==0 else 1)
