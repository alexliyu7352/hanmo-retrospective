## Headless Viewport真实控件分发验证：不发OS鼠标、不启动窗口，仍不称GUI验收。
extends SceneTree
const Strict=preload("res://core/strict_json.gd")
var checks: int=0
var failures: int=0

## 先让根Viewport就绪，再安装原地图控件。
func _init() -> void: call_deferred("_run")

## 每个失败保留具体输入路由，不能仅因没有崩溃判成功。
func check(value: bool,label: String) -> bool:
	checks+=1
	if not value: failures+=1; printerr("FAIL: ",label)
	return value

## 鼠标消息只进入无窗口Viewport的标准路由，不直接调用Canvas处理方法或发pressed。
func mouse(at: Vector2,kind: int,pressed: bool) -> void:
	var event=InputEventMouseButton.new(); event.position=at; event.global_position=at; event.button_index=kind; event.pressed=pressed
	event.button_mask=MOUSE_BUTTON_MASK_LEFT if pressed and kind==MOUSE_BUTTON_LEFT else 0
	root.push_input(event,true)

## 移动消息同样让Viewport决定命中目标，测试地点起手拖动的真实捕获。
func move(at: Vector2,relative: Vector2=Vector2.ZERO,held: bool=false) -> void:
	var event=InputEventMouseMotion.new(); event.position=at; event.global_position=at; event.relative=relative
	event.button_mask=MOUSE_BUTTON_MASK_LEFT if held else 0
	root.push_input(event,true)

## 当前正式地图的有限几何用于命中，无人物状态或世界命令入口。
func _run() -> void:
	var input_path: String=""
	var canvas_path: String="res://ui/world_map_canvas.gd"
	for arg in OS.get_cmdline_user_args():
		if arg.begins_with("--input="): input_path=arg.trim_prefix("--input=")
		if arg.begins_with("--canvas-source="): canvas_path=arg.trim_prefix("--canvas-source=")
	var loaded: Dictionary=Strict.parse_text(FileAccess.get_file_as_string(input_path))
	if not check(loaded.get("ok",false),"当前正式渲染输入可读"): quit(1); return
	var source: Dictionary=loaded.value; var before: Dictionary=source.duplicate(true)
	print("CANVAS_SOURCE=",canvas_path," SHA256=",FileAccess.get_file_as_string(canvas_path).sha256_text())
	var canvas=load(canvas_path).new(); canvas.data=source.data; canvas.view=source.view
	root.add_child(canvas); canvas.size=Vector2(1000,650); canvas._layout()
	for _frame in range(3): await process_frame
	var choices: Array=[]; var points: Array=[]
	canvas.destination_selected.connect(func(id): choices.append(id))
	canvas.point_selected.connect(func(point): points.append(point))
	var id: String="society::settlement.sword_yard"
	var at: Vector2=canvas.screen_point(Vector2(34,60))
	move(at); mouse(at,MOUSE_BUTTON_LEFT,true); mouse(at,MOUSE_BUTTON_LEFT,false); await process_frame
	check(choices==[id],"标准Viewport点击命中原地点且只发一次选择")
	choices.clear(); at=canvas.screen_point(Vector2(34,60)); var old_pan: Vector2=canvas.pan
	move(at); mouse(at,MOUSE_BUTTON_LEFT,true)
	move(at+Vector2(20,10),Vector2(20,10),true); await process_frame
	move(at+Vector2(80,25),Vector2(60,15),true); await process_frame
	mouse(at+Vector2(80,25),MOUSE_BUTTON_LEFT,false); await process_frame
	check(choices.is_empty() and canvas.pan.distance_to(old_pan+Vector2(80,25))<.1,"地点起手的真实分发拖动只平移，不误选")
	at=canvas.screen_point(Vector2(34,60)); var focus: Vector2=canvas.world_point(at); var old_zoom: float=canvas.zoom
	move(at); mouse(at,MOUSE_BUTTON_WHEEL_UP,true); await process_frame
	print("WHEEL_DIAGNOSTIC=",JSON.stringify({"before_zoom":old_zoom,"after_zoom":canvas.zoom,"focus_before":[focus.x,focus.y],"focus_after":[canvas.world_point(at).x,canvas.world_point(at).y],"pan":[canvas.pan.x,canvas.pan.y]}))
	check(is_equal_approx(canvas.zoom,old_zoom*1.15) and canvas.world_point(at).distance_to(focus)<.001,"地点上滚轮也走同一光标锚定缩放")
	canvas._markers[id].grab_focus()
	var key=InputEventAction.new(); key.action="ui_accept"; key.pressed=true; root.push_input(key,true)
	key=InputEventAction.new(); key.action="ui_accept"; key.pressed=false; root.push_input(key,true); await process_frame
	check(choices==[id],"鼠标按钮屏蔽未破坏键盘确认原地点")
	check(points.is_empty() and source==before,"分发输入未启用自由落点或修改有限源数据")
	canvas.queue_free(); await process_frame
	print("WORLD_MAP_POINTER_DISPATCH_CHECKS=",checks," FAILURES=",failures," INPUT=HEADLESS_VIEWPORT_NOT_OS GUI=NOT_TESTED WORLD_WRITES=0")
	quit(0 if failures==0 else 1)
