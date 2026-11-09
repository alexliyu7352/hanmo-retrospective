## 验证同源检索、重叠地点与真实Viewport输入；不模拟旅行成功或OS图形操作。
extends SceneTree
const Canvas=preload("res://ui/world_map_canvas.gd")
const PlaceIndex=preload("res://ui/world_map_place_index.gd")
const MarkerLayout=preload("res://ui/world_map_marker_layout.gd")
const Places=preload("res://ui/world_map_places_panel.gd")
var checks: int=0
var failures: int=0

## 等Viewport初始化后再实际分发事件。
func _init() -> void: call_deferred("_run")

## 断言具体边界，失败时返回非零而不把运行完成当作通过。
func check(value: bool,label: String) -> void:
	checks+=1
	if not value: failures+=1; printerr("FAIL: ",label)

## 鼠标由无窗口Viewport分发，按钮不能自行决定命中后直接发信号。
func click(at: Vector2) -> void:
	var motion=InputEventMouseMotion.new(); motion.position=at; motion.global_position=at; root.push_input(motion,true)
	for pressed in [true,false]:
		var event=InputEventMouseButton.new(); event.position=at; event.global_position=at; event.button_index=MOUSE_BUTTON_LEFT; event.pressed=pressed
		event.button_mask=MOUSE_BUTTON_MASK_LEFT if pressed else 0; root.push_input(event,true)

## 密集与同名输入使用有限合成作者几何，声明为UI用例，不注册为正式历史地点。
func _run() -> void:
	var data: Dictionary={"settlements":{},"routes":{},"texts":{}}
	for index in range(70):
		var id: String="place.%03d"%index
		data.settlements[id]={"id":id,"kind":"city","name_key":id,"map_xy":[index%2,index%3]}
		data.texts[id]="同名城" if index<2 else "地点%03d"%index
	var before: Dictionary=data.duplicate(true)
	var page: Dictionary=PlaceIndex.query(data,"")
	check(page.rows.size()==32 and page.total==70 and page.has_more,"检索分页有限且不丢总数")
	var second: Dictionary=PlaceIndex.query(data,"",[],32)
	check(second.rows.size()==32 and page.rows[-1].id!=second.rows[0].id,"下一页不重复首屏结果")
	var same: Dictionary=PlaceIndex.query(data,"同名城")
	check(same.rows.size()==2 and same.rows[0].id!=same.rows[1].id,"同名城不合并或反推目标ID")
	check(PlaceIndex.query(data,"",["place.001","missing"]).rows.size()==1,"限定组只保留现有作者地点")
	check(PlaceIndex.query(data,"不存在").rows.is_empty(),"空搜索没有虚构候选")
	var rows: Array=[{"id":"a","point":Vector2(100,100),"hit":Rect2(70,70,100,100),"priority":0},{"id":"b","point":Vector2(101,101),"hit":Rect2(71,71,100,100),"priority":3},{"id":"off","point":Vector2(2000,2000),"hit":Rect2(1990,1990,20,20),"priority":9}]
	var layout: Dictionary=MarkerLayout.arrange(rows,Rect2(0,0,1000,650))
	check(layout.groups.keys()==["b"] and layout.groups.b.ids==["a","b"] and layout.groups.b.point==Vector2(101,101),"选点优先、屏外剔除且不把真实点改到质心")
	rows.reverse(); check(layout==MarkerLayout.arrange(rows,Rect2(0,0,1000,650)),"作者遍历顺序不改变分组")
	var canvas=Canvas.new(); canvas.data=data; canvas.view={"ok":true,"world_map_id":"","observer_position":[0,0],"route_open":{},"road_progress":{},"travel_overlay":{}}
	canvas.selected="place.001"; root.add_child(canvas); canvas.size=Vector2(1000,650); canvas._layout()
	for _frame in range(3): await process_frame
	var picked: Array=[]; var groups: Array=[]; var points: Array=[]
	canvas.destination_selected.connect(func(id: String): picked.append(id))
	canvas.destinations_requested.connect(func(ids: Array): groups.append(ids))
	canvas.point_selected.connect(func(point: Array): points.append(point))
	var selected_at: Vector2=canvas.screen_point(Vector2(1,1))
	click(selected_at); await process_frame
	check(picked.is_empty() and groups.size()==1 and "place.001" in groups[0] and points.is_empty(),"Viewport点击密集组只展开，不误选代表城或原野")
	canvas._markers["place.001"].grab_focus()
	for pressed in [true,false]:
		var key=InputEventAction.new(); key.action="ui_accept"; key.pressed=pressed; root.push_input(key,true)
	await process_frame
	check(groups.size()==2 and picked.is_empty(),"键盘确认遵循同一重叠组选择")
	var focus: Vector2=canvas.world_point(Vector2(430,280)); canvas.zoom_at(8,Vector2(430,280))
	check(canvas.zoom>3 and canvas.world_point(Vector2(430,280)).distance_to(focus)<.001,"更深缩放保持原投影光标锚定")
	var panel=Places.new(); panel.data=data; root.add_child(panel); panel.position=Vector2(0,0); panel.size=Vector2(360,400)
	panel.destination_selected.connect(func(id: String): picked.append(id)); panel.open_candidates(["place.000","place.001"])
	await process_frame
	check(panel._items.item_count==2 and panel._items.get_item_metadata(0)!=panel._items.get_item_metadata(1),"同名候选保留两条稳定ID")
	panel._items.grab_focus(); panel._items.select(1)
	for pressed in [true,false]:
		var key=InputEventKey.new(); key.keycode=KEY_ENTER; key.pressed=pressed; root.push_input(key,true)
	await process_frame
	check(picked==["place.001"],"实际列表回车选择原作者ID且只触发一次")
	panel.hide(); check(data==before,"查找、缩放、关闭均未改作者坐标或世界数据")
	panel.queue_free(); canvas.queue_free(); await process_frame
	print("WORLD_MAP_PLACES_CHECKS=",checks," FAILURES=",failures," INPUT=HEADLESS_VIEWPORT_NOT_OS WORLD_WRITES=0")
	quit(0 if failures==0 else 1)
