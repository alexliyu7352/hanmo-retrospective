## 用正式Main留下的有限渲染输入校准地形、路面和地点图；离线CPU组合，不是GUI或GPU截图。
extends SceneTree
const Canvas=preload("res://ui/world_map_canvas.gd")
const MapProjection=preload("res://ui/map_projection.gd")
const Strict=preload("res://core/strict_json.gd")

## 根树就绪后只读取本次明确提供的证据文件。
func _init() -> void: call_deferred("_run")

## 输入和输出均为调用者给出的独立路径，原PNG与世界不会被改写。
func argument(name: String) -> String:
	for value in OS.get_cmdline_user_args():
		if value.begins_with(name+"="): return value.trim_prefix(name+"=")
	return ""

## 用线段最近点计算CPU覆盖；几何仍来自原Projection，不增加或挪动路点。
func segment_distance(point: Vector2, a: Vector2, b: Vector2) -> float:
	var delta: Vector2=b-a
	if delta.length_squared()==0: return point.distance_to(a)
	return point.distance_to(a+delta*clampf((point-a).dot(delta)/delta.length_squared(),0,1))

## 一次给整条折线覆盖像素，避免重复盖章把半透明阴影叠成黑带；GPU抗锯齿仍不在本门内。
func stroke(target: Image, line: PackedVector2Array, color: Color, width: float) -> void:
	if line.size()<2: return
	var bounds=Rect2(line[0],Vector2.ZERO)
	for point in line: bounds=bounds.expand(point)
	var radius: float=width/2; bounds=bounds.grow(radius+1)
	for y in range(maxi(0,floori(bounds.position.y)),mini(target.get_height(),ceili(bounds.end.y))):
		for x in range(maxi(0,floori(bounds.position.x)),mini(target.get_width(),ceili(bounds.end.x))):
			var at=Vector2(x+.5,y+.5); var distance: float=INF
			for index in range(line.size()-1): distance=minf(distance,segment_distance(at,line[index],line[index+1]))
			if distance<=radius+.5:
				var ink: Color=color; ink.a*=clampf(radius+.5-distance,0,1)
				target.set_pixel(x,y,target.get_pixel(x,y).blend(ink))

## 圆点只是现有本人UI标识的CPU近似，不生成新的角色美术。
func circle(target: Image, at: Vector2, radius: float, color: Color) -> void:
	stroke(target,PackedVector2Array([at,at]),color,radius*2)

## 地点原图仅在离线输出中按生产矩形缩放叠放，原文件像素不变。
func texture_at(target: Image, texture: Texture2D, rect: Rect2) -> void:
	var source: Image=texture.get_image().duplicate()
	source.convert(Image.FORMAT_RGBA8); source.resize(maxi(1,roundi(rect.size.x)),maxi(1,roundi(rect.size.y)),Image.INTERPOLATE_LANCZOS)
	target.blend_rect(source,Rect2i(Vector2i.ZERO,source.get_size()),Vector2i(rect.position))

## 复用当前目录、Canvas映射和真实路线开闭，刻意不伪绘字体、HUD或交互窗口。
func _run() -> void:
	var input_path: String=argument("--input"); var output_path: String=argument("--output")
	if input_path.is_empty() or output_path.is_empty(): printerr("需要--input与--output"); quit(2); return
	var loaded: Dictionary=Strict.parse_text(FileAccess.get_file_as_string(input_path))
	if not loaded.get("ok",false) or loaded.value.get("kind")!="READ_ONLY_MAP_RENDER_INPUT_NOT_WORLD_STATE": printerr("输入不是正式Main有限渲染记录"); quit(1); return
	var input: Dictionary=loaded.value
	var canvas=Canvas.new(); canvas.data=input.data; canvas.view=input.view
	canvas.selected=input.selected; canvas.path=input.path; canvas.path_returning=input.path_returning
	canvas.zoom=input.zoom; canvas.pan=Vector2(input.pan[0],input.pan[1])
	root.add_child(canvas); canvas.size=Vector2(input.canvas_size[0],input.canvas_size[1]); canvas._layout()
	if canvas.map_art_status()!=input.art_status or canvas.map_art_status().get("mode")!="region": printerr("目录/画框与正式输入不符"); quit(1); return
	var output=Image.create(ceili(canvas.size.x),ceili(canvas.size.y),false,Image.FORMAT_RGBA8); output.fill(Color("d6c69e"))
	var origin: Vector2=canvas.screen_point(canvas._bounds.position); var far: Vector2=canvas.screen_point(canvas._bounds.end)
	texture_at(output,canvas._terrain_art,Rect2(origin,far-origin))
	var road_width: float=clampf(canvas.zoom,.8,1.6)
	for edge in canvas.data.routes.values():
		if not canvas.view.route_open.has(edge.id): printerr("缺原路况：",edge.id); quit(1); return
		var line: PackedVector2Array=MapProjection.points(canvas.data,edge.id)
		for i in range(line.size()): line[i]=canvas.screen_point(line[i])
		var open: bool=canvas.view.route_open[edge.id]
		stroke(output,line,Color(.22,.18,.10,.24),11*road_width)
		stroke(output,line,Color("af9265") if open else Color("995a48"),8*road_width)
		stroke(output,line,Color("d6bc87") if open else Color("bb7961"),4*road_width)
	for index in range(canvas.path.size()):
		var id: String=canvas.path[index]; var line: PackedVector2Array=MapProjection.points(canvas.data,id)
		var progress: Dictionary=canvas.view.road_progress
		if index==0 and not progress.is_empty() and progress.edge_id==id: line=MapProjection.remaining(line,progress.ratio,canvas.path_returning)
		for i in range(line.size()): line[i]=canvas.screen_point(line[i])
		stroke(output,line,Color("b68432"),5)
	var placements: Dictionary={}
	for id in canvas.data.settlements:
		var xy: Array=canvas.data.settlements[id].map_xy; var at: Vector2=canvas.screen_point(Vector2(xy[0],xy[1]))
		if canvas._symbol_art.has(id):
			var rect: Rect2=canvas._symbol_rect(id,at); texture_at(output,canvas._symbol_art[id],rect)
			placements[id]={"world":xy,"rect":[rect.position.x,rect.position.y,rect.size.x,rect.size.y]}
		else:
			circle(output,at,19,Color("eee1bd")); circle(output,at,13,Color("4e5644"))
	var hero: Vector2=canvas.screen_point(Vector2(canvas.view.observer_position[0],canvas.view.observer_position[1]))
	circle(output,hero,17,Color(.08,.20,.17,.38)); circle(output,hero,12,Color("fff4cf")); circle(output,hero,7,Color("226a61"))
	stroke(output,PackedVector2Array([hero+Vector2(0,-12),hero+Vector2(0,-36)]),Color("fff4cf"),3)
	var saved: Error=output.save_png(output_path)
	print(JSON.stringify({"ok":saved==OK,"mode":"CPU_MAP_LAYERS_NOT_GUI_NO_TEXT_OR_HUD","pack_set_digest":input.pack_set_digest,"art_status":canvas.map_art_status(),"source_identity":input.source_identity,"output":output_path,"size":[output.get_width(),output.get_height()],"placements":placements}))
	canvas.queue_free(); await process_frame; quit(0 if saved==OK else 1)
