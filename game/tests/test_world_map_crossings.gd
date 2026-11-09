## 作者关渡连通块与真实Viewport选择：不存在的平均点、拖动误选和重复wheel必须被排除。
extends SceneTree
const Crossings=preload("res://ui/world_map_crossings.gd")
const Canvas=preload("res://ui/world_map_canvas.gd")
var checks: int=0
var failures: int=0

## 待根Viewport就绪再创建真实地图。
func _init() -> void: call_deferred("_run")

## 局部门失败逐项记录，不代替实际原生跨关旅行。
func check(value: bool,label: String) -> void:
	checks+=1
	if not value: failures+=1; printerr("FAIL: ",label)

## 合成地形只用于核作者地块的组合边界，不登记为正式地理。
func terrain(width: int,height: int,cell: int=1) -> Dictionary:
	var cells: Array=[]
	for _index in range((width/cell)*(height/cell)): cells.append({})
	return {"id":"map","width":width,"height":height,"cell_size":cell,"cells":cells}

## 输入统一交Viewport命中系统，不直接调用Crossing处理函数。
func mouse(at: Vector2,kind: int,pressed: bool) -> void:
	var event=InputEventMouseButton.new(); event.position=at; event.global_position=at; event.button_index=kind; event.pressed=pressed
	event.button_mask=MOUSE_BUTTON_MASK_LEFT if pressed and kind==MOUSE_BUTTON_LEFT else 0; root.push_input(event,true)

## 使用真实控件捕获验证标记起手平移。
func move(at: Vector2,relative: Vector2=Vector2.ZERO,held: bool=false) -> void:
	var event=InputEventMouseMotion.new(); event.position=at; event.global_position=at; event.relative=relative; event.button_mask=MOUSE_BUTTON_MASK_LEFT if held else 0; root.push_input(event,true)

## 同旗标异地不合成假桥；整数点必须在原地块，之后只触发原点选候选。
func _run() -> void:
	var map: Dictionary=terrain(12,6)
	for index in [14,15,57]: map.cells[index]={"crossing":"bridge","blocked_flag":"gate"}
	var before: Dictionary=map.duplicate(true); var built: Dictionary=Crossings.build(map)
	check(built.ok and built.rows.size()==2,"同flag的分离通行块保留两个原地理位置")
	for row in built.rows:
		check(row.point[0] is int and row.point[1] is int and row.point[1]*12+row.point[0] in row.cells,"代表点在真实整数通行地块内")
	var ring: Dictionary=terrain(3,3)
	for index in [0,1,2,3,5,6,7,8]: ring.cells[index]={"crossing":"pass","blocked_flag":"pass"}
	var outline: Dictionary=Crossings.build(ring)
	check(outline.ok and outline.rows.size()==1 and outline.rows[0].point!=[1,1],"连通环形关渡不把无源中心洞作为目标")
	var diagonal: Dictionary=terrain(3,3); diagonal.cells[0]={"crossing":"ford","blocked_flag":"f"}; diagonal.cells[4]=diagonal.cells[0].duplicate()
	check(Crossings.build(diagonal).rows.size()==2,"仅角接触地块不被虚构为同一连通面")
	var bad: Dictionary=terrain(3,3); bad.cells.pop_back(); check(not Crossings.build(bad).ok,"不完整地形不能产生假通行标记")
	var canvas=Canvas.new(); canvas.data={"world_maps":{"map":map},"settlements":{"town":{"id":"town","kind":"city","map_xy":[11,5]}},"routes":{},"texts":{}}
	canvas.view={"ok":true,"world_map_id":"map","observer_position":[0,0],"crossing_closed":{"gate":true},"road_progress":{},"travel_overlay":{}}
	root.add_child(canvas); canvas.size=Vector2(1000,650); canvas._layout()
	for _frame in range(3): await process_frame
	check(canvas._crossing_markers.size()==2 and canvas._crossing_label(canvas._crossings["14"])=="桥梁 · 封闭","实际画布只显示当前同源关闭旗标")
	var points: Array=[]; var places: Array=[]
	canvas.point_selected.connect(func(point: Array): points.append(point))
	canvas.destination_selected.connect(func(id: String): places.append(id))
	var at: Vector2=canvas._crossing_markers["14"].get_global_rect().get_center()
	move(at); mouse(at,MOUSE_BUTTON_LEFT,true); mouse(at,MOUSE_BUTTON_LEFT,false); await process_frame
	check(points==[built.rows[0].point] and places.is_empty(),"闭塞标记点选只交原实际地块候选，不伪装已经通关")
	var focus: Vector2=canvas.world_point(at); var zoom: float=canvas.zoom
	mouse(at,MOUSE_BUTTON_WHEEL_UP,true); await process_frame
	check(is_equal_approx(canvas.zoom,zoom*1.15) and canvas.world_point(at).distance_to(focus)<.001,"关渡名牌wheel只缩放一次且保持原地理锚点")
	at=canvas._crossing_markers["14"].get_global_rect().get_center(); var pan: Vector2=canvas.pan
	move(at); mouse(at,MOUSE_BUTTON_LEFT,true); move(at+Vector2(30,10),Vector2(30,10),true); await process_frame
	move(at+Vector2(60,20),Vector2(30,10),true); await process_frame; mouse(at+Vector2(60,20),MOUSE_BUTTON_LEFT,false); await process_frame
	check(points.size()==1 and canvas.pan.distance_to(pan+Vector2(60,20))<.1,"关渡起手拖动不误选通行点")
	canvas._crossing_markers["14"].grab_focus()
	for pressed in [true,false]:
		var key=InputEventAction.new(); key.action="ui_accept"; key.pressed=pressed; root.push_input(key,true)
	await process_frame
	check(points.size()==2 and points[1]==built.rows[0].point,"键盘确认同样只交真实地块")
	canvas.view.crossing_closed={}; check(canvas._crossing_label(canvas._crossings["14"])=="桥梁 · 未明","缺状态来源不宣称开放")
	check(map==before,"提取、选择、缩放和拖动均不改作者地理")
	canvas.queue_free(); await process_frame
	print("WORLD_MAP_CROSSINGS_CHECKS=",checks," FAILURES=",failures," INPUT=HEADLESS_VIEWPORT_NOT_OS NATIVE_TRAVEL=NOT_RUN")
	quit(0 if failures==0 else 1)
