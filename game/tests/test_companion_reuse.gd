## 整体替换内容ID和道路几何后复用同行与会面，避免只对固定人物样本成立。
extends SceneTree
const F=preload("res://tests/companion_fixture.gd")
const Strict=preload("res://core/strict_json.gd")
var checks: int=0
var failures: int=0
## 在引擎可装载内容后开始验证。
func _init() -> void:
	call_deferred("run_checks")
## 累积实际运行断言并输出失败原因。
func check(value: bool, label: String) -> void:
	checks+=1
	if not value: failures+=1; printerr("FAIL: ",label)
## 递归替换身份和引用，不改显示文本，也不修改正式作者文件。
func rewrite_ids(value: Variant, ids: Dictionary) -> Variant:
	if value is String: return ids.get(value,value)
	if value is Array: return value.map(func(child): return rewrite_ids(child,ids))
	if value is Dictionary:
		var result: Dictionary={}
		for key in value: result[ids.get(key,key)]=rewrite_ids(value[key],ids)
		return result
	return value
## 新目录、新道路与原核心组合后完成同行、交会、恢复与解散。
func run_checks() -> void:
	var header: Dictionary=Strict.parse_text(FileAccess.get_file_as_string("res://data/generated/journey_life.hmcatalog.json")).value
	var ids: Dictionary={}
	for table in load("res://core/content_catalog.gd").TABLES:
		for id in header.catalog.get(table,{}): ids[id]="alternate."+id
	header.catalog=rewrite_ids(header.catalog,ids)
	header.catalog.settlements[ids["city.west"]].map_xy=[300,400]
	header.catalog.settlements[ids["village.east"]].map_xy=[600,400]
	for road in header.catalog.routes.values(): road.shape=[[420,460]]
	header.catalog_sha256=(JSON.stringify(header.catalog,"",true,true)+"\n").sha256_text()
	var path: String=OS.get_user_data_dir().path_join("alternate-journey.json")
	var file=FileAccess.open(path,FileAccess.WRITE); file.store_string(JSON.stringify(header,"",true,true)); file.close()
	var k=load("res://core/world_kernel.gd").new()
	check(k.new_game(ids["scenario.lived_city"],path).ok,"不同身份和地图定义可载入")
	if failures: finish(); return
	check(F.send(k,"move_local",{"place_id":ids["west.tavern"]}).ok,"按新场所ID进入酒肆")
	check(F.send(k,"join_party",{"context":F.context(k,ids["role.carter"])}).ok,"按真实新岗位邀请同行")
	F.send(k,"move_local",{"place_id":ids["west.street"]})
	check(F.send(k,"travel",{"destination":ids["village.east"]}).ok,"新路线正式启程")
	check(k.advance_one_tick().ok and k.advance_one_tick().ok,"新几何道路实际交会")
	var s: Dictionary=k.snapshot()
	var e: Dictionary=load("res://core/encounter_rules.gd").pending_for(s,s.player_actor_id)
	check(not e.is_empty(),"不是依靠原人物名生成会面")
	if not e.is_empty():
		check(F.send(k,"encounter_talk",{"encounter_id":e.id}).ok,"新对白引用真实会面人物")
		check(k.restore(k.snapshot()).ok,"交谈状态可恢复")
		check(F.send(k,"encounter_leave",{"encounter_id":e.id}).ok and k.advance_one_tick().ok,"道别后继续原行程")
		check(F.send(k,"dismiss_companion",{"actor_id":ids["actor.carter"]}).ok,"到达新地点才可解散")
	finish()
## 报告本脚本范围，不混同图形或发行验收。
func finish() -> void:
	print("COMPANION_REUSE_CHECKS=",checks," FAILURES=",failures)
	quit(0 if failures==0 else 1)
