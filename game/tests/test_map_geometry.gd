## 验证地图只投影实际余程，曲折道路和缩放后的命中区域必须一致。
extends SceneTree
var checks: int=0
var failures: int=0
## 延迟创建实际画布以便获得尺寸与布局结果。
func _init() -> void:
	call_deferred("_run")
## 用数值与控件位置判断，截图不能替代几何断言。
func check(value: bool,label: String) -> void:
	checks+=1
	if not value: failures+=1; printerr("FAIL: ",label)
## 同一折线截取前进和退回路线，并在真实画布验证长折线不会被裁出地图。
func _run() -> void:
	var projection=load("res://ui/map_projection.gd")
	check(projection.has_method("remaining"),"余程高亮从真实位置开始")
	if not projection.has_method("remaining"): print("MAP_GEOMETRY_CHECKS=",checks," FAILURES=",failures); quit(1); return
	var line=PackedVector2Array([Vector2(0,0),Vector2(100,0),Vector2(100,100)])
	check(projection.remaining(line,.75,false)==PackedVector2Array([Vector2(100,50),Vector2(100,100)]),"前进不高亮已经走过的路")
	check(projection.remaining(line,.75,true)==PackedVector2Array([Vector2(100,50),Vector2(100,0),Vector2(0,0)]),"退回按相反顺序显示原路径")
	check(projection.along(PackedVector2Array(),.5)==Vector2.ZERO,"空画面路径不会触发越界")
	var canvas=load("res://ui/world_map_canvas.gd").new()
	canvas.data={"settlements":{"a":{"id":"a","map_xy":[0,0],"kind":"city"},"b":{"id":"b","map_xy":[100,0],"kind":"village"}},"routes":{"r":{"id":"r","origin":"a","destination":"b","shape":[[500,600]],"duration_ticks":2}},"texts":{}}
	root.add_child(canvas); canvas.size=Vector2(800,600); await process_frame
	check(Rect2(Vector2.ZERO,canvas.size).has_point(canvas.screen_point(Vector2(500,600))),"画布边界包含道路中间折点")
	canvas.zoom=1.7; canvas.pan=Vector2(32,-17); canvas._layout()
	var marker_rect: Rect2=canvas._markers["b"].get_rect()
	var marker_point: Vector2=canvas.screen_point(Vector2(100,0))
	check(is_equal_approx(marker_rect.get_center().x,marker_point.x) and marker_rect.has_point(marker_point),"视图改变后点击区域仍以图标为横向中心并覆盖图标")
	check(marker_rect.has_point(marker_point+Vector2(0,47)),"据点名称与图标使用同一变换后的点击区域")
	canvas.queue_free(); await process_frame
	var local_map=load("res://ui/world_map_canvas.gd").new()
	local_map.data={"settlements":{},"routes":{},"texts":{}}
	for entry in [["青川",[35,60],"city"],["营地",[36,60],"camp"],["剑坪",[34,60],"village"],["城外哨",[35,59],"camp"]]:
		local_map.data.settlements[entry[0]]={"id":entry[0],"map_xy":entry[1],"kind":entry[2]}
	root.add_child(local_map); local_map.size=Vector2(800,600); await process_frame
	check(local_map.screen_point(Vector2(35,60)).distance_to(local_map.screen_point(Vector2(36,60)))>=150,"真实青川小跨度坐标不被固定60单位留白压成重叠点")
	check(not local_map._markers["青川"].get_rect().intersects(local_map._markers["剑坪"].get_rect()),"相邻实际地点保持可分辨命中区域")
	check(local_map.world_point(local_map.screen_point(Vector2(35,59))).distance_to(Vector2(35,59))<.001,"适配视野不修改原地点坐标或逆投影")
	local_map.queue_free(); await process_frame
	print("MAP_GEOMETRY_CHECKS=",checks," FAILURES=",failures)
	quit(0 if failures==0 else 1)
