## 原地图现场标记的Headless Viewport输入门；不启动窗口或注入OS事件。
extends SceneTree
const Canvas=preload("res://ui/world_map_canvas.gd")
var checks: int=0
var failures: int=0

## 等实际Viewport开始分发输入。
func _init() -> void: call_deferred("_run")

## 每项分别校验误选、重复缩放与地理锚点。
func check(value: bool,label: String) -> void:
	checks+=1
	if not value: failures+=1; printerr("FAIL: ",label)

## 鼠标由Viewport决定命中控件，不直接调用地图处理函数。
func mouse(at: Vector2,kind: int,pressed: bool) -> void:
	var event=InputEventMouseButton.new(); event.position=at; event.global_position=at; event.button_index=kind; event.pressed=pressed
	event.button_mask=MOUSE_BUTTON_MASK_LEFT if pressed and kind==MOUSE_BUTTON_LEFT else 0; root.push_input(event,true)

## 移动同样走标准控件捕获，检查标记起手拖动仍是原摄像行为。
func move(at: Vector2,relative: Vector2=Vector2.ZERO,held: bool=false) -> void:
	var event=InputEventMouseMotion.new(); event.position=at; event.global_position=at; event.relative=relative; event.button_mask=MOUSE_BUTTON_MASK_LEFT if held else 0; root.push_input(event,true)

## 合成有限几何仅用于命中边界，Canvas源码和输入分发均为实际运行代码。
func _run() -> void:
	var canvas=Canvas.new()
	canvas.data={"settlements":{"a":{"id":"a","kind":"city","map_xy":[0,0]},"b":{"id":"b","kind":"village","map_xy":[100,0]}},"routes":{},"texts":{}}
	canvas.view={"ok":true,"world_map_id":"","observer_position":[0,0],"route_open":{},"road_progress":{},"travel_overlay":{}}
	root.add_child(canvas); canvas.size=Vector2(1000,650); canvas.set_presence_available(true); canvas._layout()
	for _frame in range(3): await process_frame
	var opens: Array=[]; var places: Array=[]; var points: Array=[]
	canvas.presence_requested.connect(func(): opens.append(true))
	canvas.destination_selected.connect(func(id: String): places.append(id))
	canvas.point_selected.connect(func(point: Array): points.append(point))
	var at: Vector2=canvas._presence_marker.get_global_rect().get_center()
	move(at); mouse(at,MOUSE_BUTTON_LEFT,true); mouse(at,MOUSE_BUTTON_LEFT,false); await process_frame
	check(opens.size()==1 and places.is_empty() and points.is_empty(),"现场标记短按只打开名单，不选地点或原野")
	var before_zoom: float=canvas.zoom; var focus: Vector2=canvas.world_point(at)
	mouse(at,MOUSE_BUTTON_WHEEL_UP,true); await process_frame
	check(is_equal_approx(canvas.zoom,before_zoom*1.15) and canvas.world_point(at).distance_to(focus)<.001,"名单标记滚轮只消费一次并保持地理锚点")
	at=canvas._presence_marker.get_global_rect().get_center(); var before_pan: Vector2=canvas.pan
	move(at); mouse(at,MOUSE_BUTTON_LEFT,true); move(at+Vector2(25,10),Vector2(25,10),true); await process_frame
	move(at+Vector2(65,20),Vector2(40,10),true); await process_frame
	mouse(at+Vector2(65,20),MOUSE_BUTTON_LEFT,false); await process_frame
	check(opens.size()==1 and canvas.pan.distance_to(before_pan+Vector2(65,20))<.1,"现场标记起手拖动不误开名单")
	canvas._presence_marker.grab_focus()
	for pressed in [true,false]:
		var key=InputEventAction.new(); key.action="ui_accept"; key.pressed=pressed; root.push_input(key,true)
	await process_frame
	check(opens.size()==2,"键盘确认仍可打开同一现场名单")
	check(places.is_empty() and points.is_empty() and canvas.view.observer_position==[0,0],"全部现场标记输入不制造目标或移动人物")
	canvas.queue_free(); await process_frame
	print("MAP_PRESENCE_POINTER_CHECKS=",checks," FAILURES=",failures," INPUT=HEADLESS_VIEWPORT_NOT_OS")
	quit(0 if failures==0 else 1)
