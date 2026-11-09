## 原生地形观察的只读文字投影；不补地形成本、射程或控制区判定。
extends RefCounted

## 旧页无本观察时不加文字，未知或残缺新观察明确不可用。
static func lines(unit: Dictionary) -> Array[String]:
	var result: Array[String]=[]
	if not unit.has("field_terrain"): return result
	if not unit.field_terrain is Dictionary:
		return ["地形来源不完整，请重新读取战场。"]
	var terrain: Dictionary=unit.field_terrain
	if terrain.get("profile","")!="terrain_and_zones_05_v1" or not terrain.get("movement_cost") is int or not terrain.get("maximum_range") is int or not terrain.get("zone_retreat") is bool:
		return ["地形来源不完整，请重新读取战场。"]
	var names: Dictionary={"plain":"平地","road":"道路","forest":"林地","hill":"丘陵","mud":"泥地","water":"水域","wall":"城垒"}
	if not names.has(terrain.get("terrain","")): return ["地形来源不完整，请重新读取战场。"]
	result.append("当前%s：步入成本 %d，实际射程上限 %d 格。" % [names[terrain.terrain],terrain.movement_cost,terrain.maximum_range])
	if terrain.zone_retreat: result.append("起点受敌控制：只能留驻或退到相邻非受控格；退行仍须支付真实地形成本，不能同时攻击。已学佯退另走原资格。")
	return result
