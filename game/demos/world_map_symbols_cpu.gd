## 地图资产的离线CPU拼合检查：复用Canvas真实入口与矩形，仅输出素材叠放证据，不冒充GUI视口。
extends SceneTree
const Canvas=preload("res://ui/world_map_canvas.gd")
const Registry=preload("res://ui/presentation_registry.gd")
const Art=preload("res://ui/art_catalog.gd")

## 根树就绪后读取明确传入的原地形，绝不改写原PNG。
func _init() -> void: call_deferred("_run")

## 仅接收调用者明确给出的地形文件和输出文件；无网络、世界或存档写入。
func _argument(name: String) -> String:
	for argument in OS.get_cmdline_user_args():
		if argument.begins_with(name+"="): return argument.trim_prefix(name+"=")
	return ""

## 真实注册表解码两图后，在原青川入口坐标用生产Canvas矩形CPU混合；不绘制虚假完整游戏UI。
func _run() -> void:
	var terrain_path: String=_argument("--terrain")
	var output_path: String=_argument("--output")
	if terrain_path.is_empty() or output_path.is_empty(): printerr("需要--terrain与--output路径"); quit(2); return
	var registry=Registry.new()
	var manifest: Dictionary={"version":1,"default_background":"world_map_city","default_portrait":"placeholder","backgrounds":{"world_map_city":{"path":"res://assets/art/world-map/v0.1/city.png","entrance":[50,80]},"world_map_village":{"path":"res://assets/art/world-map/v0.1/village.png","entrance":[52,80]}},"portraits":{"placeholder":{"path":"res://assets/art/world-map/v0.1/city.png"}}}
	var configured: Dictionary=registry.configure(manifest)
	if not configured.ok: printerr(configured); quit(1); return
	Art.commit_content({"registry":registry})
	var canvas=Canvas.new()
	canvas.data={"settlements":{"city":{"id":"city","kind":"city","map_xy":[190,430]},"village":{"id":"village","kind":"village","map_xy":[830,290]}},"routes":{},"texts":{},"world_maps":{"qingchuan":{"id":"qingchuan","width":1000,"height":600,"cell_size":20,"cells":[],"visual_tag":"unregistered_cpu_ground"}}}
	canvas.view={"ok":true,"world_map_id":"qingchuan","observer_position":[190,430],"route_open":{},"crossing_closed":{},"road_progress":{},"travel_overlay":{}}
	root.add_child(canvas); canvas.size=Vector2(1200,720); canvas._layout()
	var ground=Image.load_from_file(terrain_path)
	if ground==null or ground.is_empty(): printerr("原地形不可读"); quit(1); return
	ground.convert(Image.FORMAT_RGBA8); ground.resize(1200,720,Image.INTERPOLATE_LANCZOS)
	var placements: Dictionary={}
	for id in canvas._symbol_art:
		var xy: Array=canvas.data.settlements[id].map_xy
		var position: Vector2=canvas.screen_point(Vector2(xy[0],xy[1]))
		var rect: Rect2=canvas._symbol_rect(id,position)
		var source: Image=canvas._symbol_art[id].get_image().duplicate()
		source.convert(Image.FORMAT_RGBA8); source.resize(roundi(rect.size.x),roundi(rect.size.y),Image.INTERPOLATE_LANCZOS)
		ground.blend_rect(source,Rect2i(Vector2i.ZERO,source.get_size()),Vector2i(rect.position))
		placements[id]={"rect":[rect.position.x,rect.position.y,rect.size.x,rect.size.y],"world_entrance":xy,"resource_id":"world_map_"+id}
	var saved: Error=ground.save_png(output_path)
	print(JSON.stringify({"ok":saved==OK,"mode":"CPU_ASSET_COMPOSITE_NOT_GUI","output":output_path,"size":[1200,720],"placements":placements}))
	canvas.queue_free(); await process_frame; quit(0 if saved==OK else 1)
