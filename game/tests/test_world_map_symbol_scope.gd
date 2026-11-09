## 地点符号遵守当前内容包命名空间；只验证注册表和绘景读取，不建立或更改世界。
extends SceneTree
const Canvas=preload("res://ui/world_map_canvas.gd")
const Registry=preload("res://ui/presentation_registry.gd")
const Art=preload("res://ui/art_catalog.gd")
var checks: int=0
var failures: int=0

## 真实纹理解码需要在根树初始化后进行。
func _init() -> void: call_deferred("_run")

## 失败保留具体命名空间和回退边界。
func check(value: bool,label: String) -> bool:
	checks+=1
	if not value: failures+=1; printerr("FAIL: ",label)
	return value

## 同时存在全局与同包资产仍按地点限定ID取图，缺图保持原地图标记。
func _run() -> void:
	var city: Dictionary={"path":"res://assets/art/world-map/v0.1/city.png","entrance":[50,80]}
	var village: Dictionary={"path":"res://assets/art/world-map/v0.1/village.png","entrance":[52,80]}
	var registry=Registry.new()
	var result: Dictionary=registry.configure({"version":1,"default_background":"world_map_city","default_portrait":"placeholder","backgrounds":{"world_map_city":city,"world_map_village":village,"society::world_map_city":city,"society::world_map_village":village},"portraits":{"placeholder":city}})
	if not check(result.get("ok",false),"限定ID经真实Registry校验与解码："+str(result)): quit(1); return
	Art.commit_content({"registry":registry})
	var canvas=Canvas.new(); canvas.data={"settlements":{},"routes":{},"texts":{}}
	for pair in [["city.plain","city"],["society::city.qingchuan","city"],["society::village.hewan","village"],["other::city.missing","city"]]:
		canvas.data.settlements[pair[0]]={"id":pair[0],"kind":pair[1],"map_xy":[canvas.data.settlements.size()*100,0]}
	root.add_child(canvas); canvas.size=Vector2(800,600); await process_frame
	check(canvas._symbol_resource_id(canvas.data.settlements["society::city.qingchuan"])=="society::world_map_city","原生同包地点使用准确限定资源ID")
	check(canvas._symbol_art.has("society::city.qingchuan") and canvas._symbol_art.has("society::village.hewan"),"同包城邑和乡里都取到已注册真实图")
	check(canvas._symbol_art.has("city.plain"),"无前缀旧地点保留全局资源入口")
	check(not canvas._symbol_art.has("other::city.missing") and canvas._markers.has("other::city.missing"),"同包缺图不借全局或society资源，保留可读可点marker")
	check(canvas._symbol_anchors["society::village.hewan"]==Vector2(.52,.8),"入口锚点来自同包目录")
	canvas.queue_free(); await process_frame
	print("WORLD_MAP_SYMBOL_SCOPE_CHECKS=",checks," FAILURES=",failures," WORLD_WRITES=0")
	quit(0 if failures==0 else 1)
