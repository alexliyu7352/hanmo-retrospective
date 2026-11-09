## 绘图层级回归：有意保留整画布不透明场景层，证明其不能盖住宿主HUD和候选名单。
extends "res://tests/test_visual_entrypoints.gd"
var capture_dir: String=""
var pixel_checks: int=0

## 每个CanvasItem的实际z必须局限于本子树顺序，不能越过后绘制的宿主控件。
func check_local_order(host: Control) -> void:
	var hud: Control=main.find_child("WorldStatus",true,false)
	check(hud!=null and hud.is_visible_in_tree(),"真实HUD节点仍在活树且可见")
	check(not host.is_greater_than(hud),"整个场景子树位于宿主HUD之前")
	var layered: Control=host.find_child("LayeredScene",true,false)
	for node in layered.find_children("*","CanvasItem",true,false):
		check(node.z_index==0,"场景内部树序不引入跨宿主z偏移："+str(node.name))
	var roster: Control=host.find_child("SceneCandidateScroll",true,false)
	check(roster!=null and roster.is_greater_than(layered),"同宿主名单位于全部场景绘图之后")

## 用屏幕区域的真实像素差异识别文字/按钮是否被整色层完全遮住，不以节点visible冒充可见。
func varied(image: Image, rect: Rect2) -> bool:
	var bounded: Rect2i=Rect2i(rect).intersection(Rect2i(Vector2i.ZERO,image.get_size()))
	if bounded.size.x<8 or bounded.size.y<8: return false
	var colors: Dictionary={}
	for y in range(bounded.position.y,bounded.end.y,2):
		for x in range(bounded.position.x,bounded.end.x,2):
			colors[image.get_pixel(x,y).to_rgba32()]=true
			if colors.size()>12: return true
	return false

## 只有真实图形后端采样Viewport像素；headless明确只验证节点次序，不作视觉通过声明。
func check_pixels(label: String, host: Control) -> void:
	if DisplayServer.get_name()=="headless": return
	await RenderingServer.frame_post_draw
	var image: Image=root.get_texture().get_image()
	check(image!=null and not image.is_empty(),"图形Viewport提供真实像素 "+label)
	if image==null or image.is_empty(): return
	var hud: Control=main.find_child("WorldStatus",true,false)
	var roster: Control=host.find_child("SceneCandidateScroll",true,false)
	check(varied(image,hud.get_global_rect()),"HUD文字像素未被场景层覆盖 "+label)
	check(varied(image,roster.get_global_rect()),"候选名单像素未被场景层覆盖 "+label)
	pixel_checks+=2
	if not capture_dir.is_empty():
		DirAccess.make_dir_recursive_absolute(capture_dir)
		check(image.save_png(capture_dir.path_join(label+".png"))==OK,"保存未改写Viewport截图 "+label)

## 启程、开图返回、进入庄园均走现有Main/Kernel；只在测试注册表提供刻意极端的不透明图层。
func _run() -> void:
	for argument in OS.get_cmdline_user_args():
		if argument.begins_with("--capture-dir="): capture_dir=argument.trim_prefix("--capture-dir=")
	root.size=Vector2i(1280,800); prepare()
	main=Main.new(); main.kernel=RecordingKernel.new(); root.add_child(main)
	main.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT); await settle()
	check(main.start_content("scenario.alpha_life","res://data/generated/alpha_life.hmcatalog.json").ok,"正式Main创建旧玩法世界")
	main._data.places["west.street"].scene_template_ids={"city":"entry.scene.city"}
	main._data.places["west.estate"].scene_template_ids={"interior":"entry.scene.interior"}
	main._render(); await settle()
	main._open_map(); main._map_select("village.east"); main._close_map(); await settle()
	var before: Dictionary=main.kernel.snapshot()
	var host: Control=main.find_child("SceneHost",true,false)
	check_local_order(host); await check_pixels("city-after-map",host)
	check(main.kernel.snapshot()==before,"图层查询和实际像素采样不改世界")
	var request: Dictionary=request_for(host,"move_local","west.estate")
	check(not request.is_empty() and main._scene_request(request,host).ok,"正式完整请求进入真实河西庄园")
	await settle()
	host=main.find_child("SceneHost",true,false)
	check(main.kernel.snapshot().actors["actor.player"].location.place_id=="west.estate","庄园位置来自正式命令结果")
	check_local_order(host); await check_pixels("estate-after-entry",host)
	await click(find_button("command_kind","move_local",host))
	check(main.kernel.snapshot().actors["actor.player"].location.place_id=="west.street","可见名单鼠标返回仍有效")
	main.queue_free(); await settle()
	for key in original_settings: ProjectSettings.set_setting(key,original_settings[key])
	Art.catalog().configure(original_manifest)
	for path in [catalog_path,fixed_path]: DirAccess.remove_absolute(path)
	DirAccess.remove_absolute(folder)
	print("SCENE_DRAW_ORDER_PIXELS=",pixel_checks," BACKEND=",DisplayServer.get_name())
	print("SCENE_DRAW_ORDER_CHECKS=",checks," FAILURES=",failures)
	quit(0 if failures==0 else 1)
