## 真Registry与区域Canvas联合窄门；冻结字段来自中央ea8924c，不替代正式包或GUI验收。
extends SceneTree
const Canvas=preload("res://ui/world_map_canvas.gd")
const Registry=preload("res://ui/presentation_registry.gd")
const Art=preload("res://ui/art_catalog.gd")
var checks: int=0
var failures: int=0

## 真实图像与控件在根树初始化后加载。
func _init() -> void: call_deferred("_run")

## 失败输出实际合同，必须同时检查脚本日志。
func check(value: bool,label: String) -> bool:
	checks+=1
	if not value: failures+=1; printerr("FAIL: ",label)
	return value

## 独立测试目录只用真实已有PNG和冻结表现字段，不替换运行包身份。
func manifest(frame: Array=[32,58,6,4]) -> Dictionary:
	return {"version":2,"default_background":"society::world_map_region","default_portrait":"placeholder","backgrounds":{"society::world_map_region":{"path":"res://assets/art/world-map/v0.2/qingchuan-region.png","map_world_rect":frame.duplicate()},"society::world_map_city":{"path":"res://assets/art/world-map/v0.1/city.png","entrance":[50,80]},"society::world_map_village":{"path":"res://assets/art/world-map/v0.1/village.png","entrance":[52,80]}},"portraits":{"placeholder":{"path":"res://assets/art/world-map/v0.1/city.png"}},"components":{},"character_profiles":{},"scene_templates":{}}

## 几何来自当前青川样本原点位，动态规则或隐藏人物均不由夹具提供。
func geography() -> Dictionary:
	return {"settlements":{"society::city":{"id":"society::city","kind":"city","map_xy":[35,60]},"society::yard":{"id":"society::yard","kind":"village","map_xy":[34,60]}},"routes":{"r":{"id":"r","origin":"society::city","destination":"society::yard","duration_ticks":1}},"texts":{}}

## 只建立当前地图控制，完整世界、时间、钱包与知识均不存在。
func make_canvas(data: Dictionary) -> Control:
	var canvas=Canvas.new(); canvas.data=data
	canvas.view={"ok":true,"world_map_id":"","observer_position":[35,60],"route_open":{"r":true},"crossing_closed":{},"road_progress":{},"travel_overlay":{}}
	root.add_child(canvas); canvas.size=Vector2(960,720); canvas._layout(); return canvas

## 正常装载、失败原目录保留、同包映射、坏覆盖/比例回退及地形旧入口分别检查。
func _run() -> void:
	var registry=Registry.new(); var candidate: Dictionary=manifest()
	var initial: Dictionary=registry.configure(candidate)
	if not check(initial.get("ok",false),"冻结Schema与GD Contract接受真实区域背景字段："+str(initial)): quit(1); return
	var retained: Dictionary=registry.describe()
	for frame in [[32,58,0,4],[32,58,6,-1],[32,58,6],[32,58,true,4],[32,58,6,INF]]:
		var result: Dictionary=registry.configure(manifest(frame))
		check(not result.get("ok",false) and registry.describe()==retained,"坏矩形拒绝且原有效目录不变："+str(frame))
	var portrait_bad: Dictionary=manifest(); portrait_bad.portraits.placeholder.map_world_rect=[32,58,6,4]
	check(not registry.configure(portrait_bad).get("ok",false) and registry.describe()==retained,"肖像不得携带地图画框，失败不切目录")
	Art.commit_content({"registry":registry})
	var data: Dictionary=geography(); var before: Dictionary=data.duplicate(true)
	var canvas=make_canvas(data); await process_frame
	check(canvas.map_art_status().get("mode")=="region" and canvas._bounds==Rect2(32,58,6,4),"同包地面实际经Registry绑定到非零原点")
	check(canvas._terrain_art!=null and canvas._terrain_art.get_width()==1536,"实际读取1536像素原图，不使用占位纹理")
	check(data==before,"绑定不改地点、原路段或作者点位")
	var points: Array=[]; canvas.point_selected.connect(func(point): points.append(point))
	var mouse=InputEventMouseButton.new(); mouse.button_index=MOUSE_BUTTON_LEFT; mouse.position=canvas.screen_point(Vector2(34.5,60)); mouse.pressed=true; canvas._gui_input(mouse)
	mouse.pressed=false; canvas._gui_input(mouse)
	check(points.is_empty() and canvas._world_map.is_empty(),"区域地面不启用原野落点或自由地形行走")
	canvas.queue_free(); await process_frame
	var outside: Dictionary=geography(); outside.routes.r.shape=[[40,60]]
	canvas=make_canvas(outside); await process_frame
	check(canvas.map_art_status().get("code")=="MapArtBoundsMismatch" and canvas._terrain_art==null,"真实道路折点出框时降级，不裁路径或挪坐标")
	check(Rect2(Vector2.ZERO,canvas.size).has_point(canvas.screen_point(Vector2(40,60))),"降级仍按真实路段全范围取景")
	canvas.queue_free(); await process_frame
	check(registry.configure(manifest([32,58,6,3])).get("ok",false),"有限正矩形可入目录，由消费者校验实际像素比例")
	canvas=make_canvas(geography()); await process_frame
	check(canvas.map_art_status().get("code")=="MapArtAspectMismatch" and canvas._terrain_art==null,"比例不符不能拉伸原图")
	canvas.queue_free(); await process_frame
	check(registry.configure(candidate).get("ok",false),"重新装载原合法目录")
	var mixed: Dictionary=geography(); mixed.settlements["other::camp"]={"id":"other::camp","kind":"camp","map_xy":[36,60]}
	canvas=make_canvas(mixed); await process_frame
	check(canvas.map_art_status().get("mode")=="legacy" and canvas._terrain_art==null,"混合地理namespace不猜用society图")
	canvas.queue_free(); await process_frame
	var terrain: Dictionary=geography(); terrain.world_maps={"old":{"id":"old","width":100,"height":80,"cell_size":20,"cells":[],"visual_tag":"society::world_map_region"}}
	canvas=Canvas.new(); canvas.data=terrain; canvas.view={"ok":false,"world_map_id":"old"}; root.add_child(canvas); canvas.size=Vector2(960,720); await process_frame
	check(canvas._bounds==Rect2(0,0,100,80) and canvas.map_art_status().get("mode")=="terrain","既有world_map宽高与规则入口优先，不被新背景画框替换")
	canvas.queue_free(); await process_frame
	print("WORLD_MAP_REGION_REGISTRY_CHECKS=",checks," FAILURES=",failures," GUI=NOT_TESTED WORLD_WRITES=0")
	quit(0 if failures==0 else 1)
