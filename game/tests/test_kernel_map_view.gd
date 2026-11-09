## 固定原Canvas的真实绘图参数/几何/输入oracle；有限map_view只覆盖画布用途，不冒充像素或存读验收。
extends SceneTree
const Original=preload("res://tests/map_oracle/original_canvas.gd")
const Candidate=preload("res://tests/map_oracle/candidate_canvas.gd")
const Kernel=preload("res://core/world_kernel.gd")
const Main=preload("res://ui/main.gd")
const Screen=preload("res://ui/world_map_screen.gd")
const Stage=preload("res://ui/scene_stage.gd")
const Art=preload("res://ui/art_catalog.gd")
const ORACLE="be3e3f076ca5a92a178a6632e0813bdea723932688b1a09f9bd3b2e96e3d8325"
const CASES=["static_only","settlement","road_early","road_forward","road_return","road_after","following","in_army","terrain","terrain_paused","terrain_stationary","terrain_gate_closed","outside_flag"]
var checks: int=0
var failures: int=0
var leaf: GDScript

## 只计真实Kernel有限口调用，临时门面与被测对象共享同一backend而非另造世界。
class ObservedKernel extends "res://core/world_kernel.gd":
	var maps: int=0
	## 缺正式方法保持明确失败；正式方法存在时只委托实际Kernel，不复制投影算法。
	func map_view() -> Dictionary:
		maps+=1
		var actual=load("res://core/world_kernel.gd").new(); actual._backend=_backend
		return actual.call("map_view") if actual.has_method("map_view") else {"ok":false,"code":"MapViewProbeMissingAPI"}

## 等根节点就绪后再准备原图与实际控件。
func _init() -> void: call_deferred("_run")

## 所有断言独立累计，错误不能被headless退出0掩盖。
func check(value: bool,label: String) -> bool:
	checks+=1
	if not value: failures+=1; printerr("FAIL: ",label)
	return value

## 专用投影输入仅含真实读取字段；并不声称这些边界字典通过完整世界Schema。
func fixture(kind: String) -> Dictionary:
	var data: Dictionary={"settlements":{"a":{"id":"a","kind":"city","map_xy":[0,0]},"b":{"id":"b","kind":"village","map_xy":[100,0]}},"routes":{"r":{"id":"r","origin":"a","destination":"b","duration_ticks":3,"shape":[[40,70],[40,70],[80,70]],"blocked_from_tick":4,"blocked_until_tick":8}},"texts":{},"scenarios":{"scenario":{}},"world_maps":{}}
	var state: Dictionary={"revision":7,"tick":3,"scenario_id":"scenario","player_actor_id":"player","actors":{"player":{"id":"player","location":{"kind":"Settlement","settlement_id":"a","place_id":"entry"},"action_id":""}},"actions":{},"flags":{"gate":false,"bridge":true,"private.unrelated":true},"parties":{},"armies":{},"wallets":{"secret":999},"rng":{"source":"unchanged"}}
	var f: Dictionary={"state":state,"data":data,"path":[],"returning":false,"points":[]}
	if kind=="static_only": f.state={}; return f
	if kind.begins_with("road_") or kind in ["following","in_army"]:
		var location: Dictionary={"kind":"OnRoute","edge_id":"r","progress":150}
		state.actors.player.location=location; f.path=["r"]
		state.actions.travel={"kind":"travel","params":{"path":["r"],"edge_index":0,"returning":kind=="road_return","paused":false}}
		state.actors.player.action_id="travel"; f.returning=kind=="road_return"
		if kind=="road_forward": state.tick=4
		if kind=="road_after": state.tick=8
		if kind in ["following","in_army"]:
			state.actors.leader={"id":"leader","location":location.duplicate(true),"action_id":""}
			state.actors.player.action_id=""
			if kind=="following":
				state.actors.player.location={"kind":"Following","party_id":"party"}; state.parties.party={"leader_id":"leader","members":["leader","player"]}
			else:
				state.actors.player.location={"kind":"InArmy","army_id":"army"}; state.armies.army={"status":"active","commander_actor_id":"leader","member_actor_ids":["leader","player"],"prisoner_ids":[]}
	if kind.begins_with("terrain") or kind=="outside_flag":
		var cells: Array=[]
		for i in range(20): cells.append({})
		cells[6]={"crossing":"ford","blocked_flag":"gate"}; cells[7]=cells[6].duplicate()
		cells[12]={"crossing":"bridge","blocked_flag":"bridge"}; cells[13]={"crossing":"pass"}; cells[14]={"blocked_flag":"private.unrelated"}
		data.world_maps.map={"id":"map","width":100,"height":80,"cell_size":20,"cells":cells,"visual_tag":"city"}; data.scenarios.scenario.world_map_id="map"
		state.actors.player.location={"kind":"Wilderness","map_id":"map","point":[45,35],"origin":"a"}
		state.actors.player.action_id="travel"; state.actions.travel={"kind":"travel","params":{"terrain":true,"points":[[60,40],[90,70]],"paused":kind=="terrain_paused"}}
		f.points=[[45,35],[60,40],[90,70]]
		if kind=="terrain_stationary": state.actors.player.action_id=""; f.points=[]
		if kind=="terrain_gate_closed": state.flags.gate=true
		if kind=="outside_flag": state.flags["private.unrelated"]=false
	return f

## 原GUI输入函数接收真正事件；只记录选择信号，永不提交旅行或修改状态。
func mouse(canvas: Control,button: int,pressed: bool,point: Vector2) -> void:
	var event=InputEventMouseButton.new(); event.button_index=button; event.pressed=pressed; event.position=point
	canvas._gui_input(event)

## 原Canvas与候选用完全相同的控件尺寸、局部选择及事件；draw适配只记录参数。
func frame(script: GDScript,f: Dictionary,finite: bool) -> Dictionary:
	var before: Dictionary=f.duplicate(true)
	var canvas=script.new(); canvas.data=f.data
	if finite: canvas.view=leaf.query(f.state,f.data)
	else: canvas.state=f.state
	canvas.path=f.path.duplicate(); canvas.path_returning=f.returning; canvas.terrain_points=f.points.duplicate(true)
	canvas.selected="b"; canvas.selected_point=[30,30]; canvas.zoom=1.3; canvas.pan=Vector2(9,-4)
	root.add_child(canvas); canvas.size=Vector2(800,600); canvas._layout()
	var selections: Array=[]; var destinations: Array=[]
	canvas.point_selected.connect(func(value): selections.append(value.duplicate()))
	canvas.destination_selected.connect(func(value): destinations.append(value))
	canvas._markers.b.pressed.emit()
	mouse(canvas,MOUSE_BUTTON_LEFT,true,canvas.screen_point(Vector2(30,30)))
	mouse(canvas,MOUSE_BUTTON_LEFT,false,canvas.screen_point(Vector2(30,30)))
	for distance in [5.0,6.0]:
		mouse(canvas,MOUSE_BUTTON_LEFT,true,Vector2(100,100))
		var motion=InputEventMouseMotion.new(); motion.position=Vector2(100+distance,100); canvas._gui_input(motion)
		mouse(canvas,MOUSE_BUTTON_LEFT,false,motion.position)
	var pointer: Vector2=Vector2(220,180); var focus: Vector2=canvas.world_point(pointer)
	mouse(canvas,MOUSE_BUTTON_WHEEL_UP,true,pointer)
	check(canvas.world_point(pointer).is_equal_approx(focus),"滚轮保指向世界位置")
	canvas.trace_calls.clear(); canvas._draw()
	var markers: Dictionary={}
	for id in canvas._markers: markers[id]=canvas._markers[id].get_rect()
	var result: Dictionary=Original.trace_value({"draws":canvas.trace_calls,"bounds":canvas._bounds,"markers":markers,"selections":selections,"destinations":destinations,"zoom":canvas.zoom,"pan":canvas.pan,"round_trip":canvas.world_point(canvas.screen_point(Vector2(40,55)))})
	check(f==before,"绘制/选择/拖动不回写源")
	canvas.free(); return result

## 原正式道路命令产出当前行程，真实WorldMapScreen必须接有限view且保preview覆盖顺序。
func caller_case(selected: bool) -> void:
	var kernel=ObservedKernel.new()
	if not check(kernel.new_game("scenario.procurement","res://data/generated/procurement.hmcatalog.json").ok,"正式tiny Kernel开局"): return
	var context: Dictionary=kernel.command_context()
	check(kernel.submit({"kind":"travel","params":{"destination":"village.test"},"actor_id":context.actor_id,"request_seq":context.next_request_seq,"expected_revision":context.revision}).ok,"正式命令创建原道路行程")
	var before: Dictionary=kernel.snapshot()
	var main=Main.new()
	for node in [main.storage,main.journey_view,main._ui_font_scale,main._ambience]: main.add_child(node)
	main._ambience.add_child(main._ambience.music); main._ambience.add_child(main._ambience.effect)
	main.kernel=kernel; main._data=kernel.content().data; main._started=true
	if selected: main.journey_view.selected="city.west"
	var quote: Dictionary=main._map_quote()
	var stage=Stage.new(); root.add_child(stage); stage.size=Vector2(1280,720)
	kernel.maps=0; Screen.render(main,stage,kernel.journey_sidebar())
	var canvas: Control=stage.find_child("WorldMapCanvas",true,false)
	check(canvas!=null and kernel.maps==1,"真实屏幕只取一次有限地图view")
	var expected: Dictionary=kernel.map_view()
	check(expected.get("ok",false),"正式Kernel对合法当前行程必须返回有效有限view")
	if canvas!=null and expected.get("ok",false):
		check(canvas.view==expected,"Canvas只持有有限返回值")
		check(canvas.path==(quote.get("path",[]) if selected else expected.travel_overlay.path),"新preview覆盖与原行程fallback顺序保持")
		check(canvas.path_returning==(quote.get("returning",false) if selected else expected.travel_overlay.returning),"原折返方向保持")
	check(kernel.snapshot()==before and not main.storage._busy,"真实caller查看不改世界或Native")
	stage.free(); main.free()

## 只核本门投影来源、有限键和原开闭/跟随语义；坏来源只返回已冻结的新接口错误。
func leaf_checks() -> void:
	check(leaf.query({},{} )=={"ok":false,"code":"NoWorld"},"空世界明确拒绝")
	for kind in CASES:
		if kind=="static_only": continue
		var f: Dictionary=fixture(kind); var before: Dictionary=f.duplicate(true)
		seed(601); var random_value: int=randi(); seed(601)
		var value: Dictionary=leaf.query(f.state,f.data)
		if not check(value.get("ok",false),"合法有限来源 "+kind): continue
		var keys: Array=value.keys(); keys.sort()
		check(keys==["crossing_closed","observer_id","observer_position","ok","revision","road_progress","route_open","travel_overlay","world_map_id"],"有限键白名单 "+kind)
		check(f==before and randi()==random_value,"投影不写输入或消耗随机 "+kind)
		if kind in ["following","in_army"]:
			check(value.road_progress.is_empty() and value.travel_overlay.is_empty() and value.observer_position!=[0.0,0.0],"跟随坐标不变成raw本人截线 "+kind)
		if not value.world_map_id.is_empty():
			check(value.route_open.is_empty() and value.crossing_closed=={"gate":kind=="terrain_gate_closed","bridge":true},"只摘当前地图实际引用旗标 "+kind)
		else: check(value.route_open.r==not(kind=="road_forward"),"道路原开闭端点 "+kind)
		var stable: Dictionary=value.duplicate(true); value.observer_position.clear(); value.route_open.clear(); value.travel_overlay.clear()
		check(leaf.query(f.state,f.data)==stable,"返回值不能污染后续查询 "+kind)
	for path in ["actor","location","route","map","action","flags"]:
		var f: Dictionary=fixture("terrain" if path in ["map","flags"] else "road_forward")
		match path:
			"actor": f.state.actors.player=false
			"location": f.state.actors.player.location={"kind":"Following","party_id":"missing"}
			"route": f.data.routes.r.duration_ticks=0
			"map": f.data.world_maps.erase("map")
			"action": f.state.actions.travel.params.path=false
			"flags": f.state.flags.gate=[]
		var before: Dictionary=f.duplicate(true)
		check(leaf.query(f.state,f.data)=={"ok":false,"code":"InvalidMapViewSource"} and f==before,"新坏来源协议原子拒绝 "+path)

## 固定旧输出摘要，只允许在旧源capture阶段生成一次。
func digest(value: Variant) -> String: return JSON.stringify(value,"",true,true).sha256_text()

## 原绘图/输入oracle先完成；正式门同时核有限叶、真实Kernel与唯一caller接线。
func _run() -> void:
	root.size=Vector2i(1280,720)
	var kernel=Kernel.new(); check(kernel.new_game("scenario.procurement","res://data/generated/procurement.hmcatalog.json").ok,"正式tiny装载")
	var ready: Dictionary=Art.prepare_content(kernel.content()); if not check(ready.ok,"原tiny资源通过正式准备"): quit(1); return
	Art.commit_content(ready)
	var capture: String=""
	for arg in OS.get_cmdline_user_args():
		if arg.begins_with("--capture-oracle="): capture=arg.trim_prefix("--capture-oracle=")
	var old: Dictionary={}
	for kind in CASES: old[kind]=frame(Original,fixture(kind),false)
	if not capture.is_empty():
		var file=FileAccess.open(capture,FileAccess.WRITE); file.store_string(JSON.stringify({"sha256":digest(old),"outputs":old},"\t",true,true)+"\n"); file.close()
		print("ORIGINAL_MAP_ORACLE=",digest(old))
	else:
		check(digest(old)==ORACLE,"固定旧Canvas实际绘图参数/几何/输入oracle不变")
		var available: bool=FileAccess.file_exists("res://core/views/map_view.gd")
		check(available,"有限map_view叶存在")
		check(Kernel.new().has_method("map_view"),"实际Kernel地图门面存在")
		if available:
			leaf=load("res://core/views/map_view.gd")
			if not check(leaf!=null and leaf.has_method("query"),"有限叶成功加载"): quit(1); return
			var finite: bool=false; var probe=Candidate.new()
			for property in probe.get_property_list():
				if property.name=="view": finite=true
			probe.free()
			check(finite,"实际候选Canvas接收有限view")
			if finite:
				for kind in CASES: check(frame(Candidate,fixture(kind),true)==old[kind],"候选绘图/几何/输入完整同值 "+kind)
			leaf_checks()
			caller_case(false); caller_case(true)
	print("KERNEL_MAP_VIEW_CHECKS=",checks," FAILURES=",failures)
	quit(0 if failures==0 else 1)
