## 只核地形DTO有限文字投影，不称引擎GUI、接敌或World已验。
extends SceneTree
const Panel=preload("res://ui/military_field_terrain_panel.gd")
var checks: int=0
var failures: int=0
## 逐项保留精确失败，不把残缺来源静默补成默认地形。
func check(value: bool,label: String) -> void:
	checks+=1
	if not value: failures+=1; printerr("FAIL ",label)
## 借实际原生字段形状检查旧空页、新地形与来源失效。
func _init() -> void:
	check(Panel.lines({}).is_empty(),"旧页没有新增地形")
	var unit: Dictionary={"field_terrain":{"profile":"terrain_and_zones_05_v1","terrain":"mud","movement_cost":4,"maximum_range":1,"zone_retreat":true}}
	var before: Dictionary=unit.duplicate(true)
	var lines: Array[String]=Panel.lines(unit)
	check(lines.size()==2 and "泥地" in lines[0] and "成本 4" in lines[0],"只显示真实泥地成本")
	check("不能同时攻击" in lines[1],"控制区失败解释")
	check(unit==before,"查询不改原DTO")
	unit.field_terrain.terrain="hill";unit.field_terrain.maximum_range=5;unit.field_terrain.zone_retreat=false
	check(Panel.lines(unit).size()==1 and "5 格" in Panel.lines(unit)[0],"丘陵实际射程直接消费")
	for field in ["profile","movement_cost","maximum_range","zone_retreat"]:
		var broken: Dictionary=unit.duplicate(true);broken.field_terrain.erase(field)
		check("来源不完整" in Panel.lines(broken)[0],"缺来源字段拒绝 "+field)
	unit.field_terrain.profile="future"
	check("来源不完整" in Panel.lines(unit)[0],"未知规则拒绝")
	print("MILITARY_FIELD_TERRAIN_UI=",checks," FAILURES=",failures)
	quit(1 if failures else 0)
