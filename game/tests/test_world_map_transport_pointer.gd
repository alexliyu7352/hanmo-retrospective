## 复用已核Headless Viewport事件驱动，检查实际携行标记不会触发人物页或旅行。
extends "res://tests/test_world_map_presence_pointer.gd"

## 新标记沿真实Canvas摄像/命中链，世界几何仍是有限夹具。
func _run() -> void:
	var canvas=Canvas.new()
	canvas.data={"settlements":{"a":{"id":"a","kind":"city","map_xy":[0,0]},"b":{"id":"b","kind":"village","map_xy":[100,0]}},"routes":{},"texts":{}}
	canvas.view={"ok":true,"world_map_id":"","observer_position":[0,0],"route_open":{},"road_progress":{},"travel_overlay":{}}
	root.add_child(canvas);canvas.size=Vector2(1000,650);canvas.set_presence_available(true);canvas.set_transport_available(true);canvas._layout()
	for _frame in range(3):await process_frame
	var opens: Array=[];var people: Array=[];var places: Array=[];var points: Array=[]
	canvas.transport_requested.connect(func():opens.append(true));canvas.presence_requested.connect(func():people.append(true))
	canvas.destination_selected.connect(func(id: String):places.append(id));canvas.point_selected.connect(func(point: Array):points.append(point))
	check(not canvas._transport_marker.get_rect().intersects(canvas._presence_marker.get_rect()),"本人两清单入口不重叠")
	var at: Vector2=canvas._transport_marker.get_global_rect().get_center()
	move(at);mouse(at,MOUSE_BUTTON_LEFT,true);mouse(at,MOUSE_BUTTON_LEFT,false);await process_frame
	check(opens.size()==1 and people.is_empty(),"携行短按只开真实携行层")
	var before_zoom: float=canvas.zoom;var focus: Vector2=canvas.world_point(at)
	mouse(at,MOUSE_BUTTON_WHEEL_UP,true);await process_frame
	check(is_equal_approx(canvas.zoom,before_zoom*1.15) and canvas.world_point(at).distance_to(focus)<.001,"携行标记滚轮只消费一次")
	at=canvas._transport_marker.get_global_rect().get_center();var before_pan: Vector2=canvas.pan
	move(at);mouse(at,MOUSE_BUTTON_LEFT,true);move(at+Vector2(50,20),Vector2(50,20),true);await process_frame;mouse(at+Vector2(50,20),MOUSE_BUTTON_LEFT,false);await process_frame
	check(opens.size()==1 and canvas.pan.distance_to(before_pan+Vector2(50,20))<.1,"拖动携行入口仍只移动摄像机")
	canvas._transport_marker.grab_focus()
	for pressed in [true,false]:
		var key=InputEventAction.new();key.action="ui_accept";key.pressed=pressed;root.push_input(key,true)
	await process_frame
	check(opens.size()==2 and people.is_empty() and places.is_empty() and points.is_empty(),"键盘打开清单不派生旅行或其他层动作")
	canvas.queue_free();await process_frame
	print("MAP_TRANSPORT_POINTER_CHECKS=",checks," FAILURES=",failures," INPUT=HEADLESS_VIEWPORT_NOT_OS")
	quit(0 if failures==0 else 1)
