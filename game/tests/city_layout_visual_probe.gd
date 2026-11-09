## 真正图形进程中的城市分层截图出口；无显示驱动时拒绝，不把CPU合成或空图记作窗口证据。
extends SceneTree
const Lab = preload("res://demos/city_layout_lab.gd")
var lab: Control
var folder: String = ""
var captures: Array = []

## 独立启动，输出目录由启动者指定；不触及正式场景、规则或存档。
func _init() -> void:
	call_deferred("_run")

## 每步等待真实RenderingServer完成一帧再取Viewport像素，保存状态与路径以便复核。
func capture(name: String) -> void:
	await process_frame; await process_frame
	await RenderingServer.frame_post_draw
	var image: Image = root.get_texture().get_image()
	var path: String = folder.path_join(name + ".png")
	var error: Error = image.save_png(path)
	if image.is_empty() or error != OK: printerr("CITY_CAPTURE_FAILED ", path); quit(1); return
	captures.append({"name": name, "path": path, "size": [image.get_width(), image.get_height()],
		"layout_id": lab.layout.id, "active_place": lab.active_place, "descriptor": lab.canvas.describe()})
	print("CITY_CAPTURE=", name)

## 真实图形流程覆盖两种布局、同楼施工、移除、裸地、访问返回与窄窗。
func _run() -> void:
	if DisplayServer.get_name() == "headless": printerr("CITY_GRAPHICS_REQUIRED"); quit(2); return
	folder = OS.get_environment("HANMO_CITY_OUTPUT")
	if folder.is_empty(): printerr("CITY_OUTPUT_REQUIRED"); quit(2); return
	root.size = Vector2i(1280, 800)
	lab = Lab.new(); root.add_child(lab)
	await process_frame
	if lab.canvas == null: quit(1); return
	await capture("01-long-street")
	lab.choose_layout(1); await capture("02-courtyard")
	lab.selected = "market.grain"; lab.toggle_construction(); await capture("03-construction")
	lab.enter_place("market.grain"); await capture("04-place-closeup")
	lab.return_to_district(); await capture("05-return-same-layout")
	lab.remove_building(); await capture("06-removed-building")
	lab.show_ground(); await capture("07-complete-ground")
	lab.restore_layout(); lab.selected = "market.cloth"; lab.move_building(); await capture("08-relocated-occlusion")
	lab.replace_building(); await capture("09-replaced-building")
	lab.restore_layout(); root.size = Vector2i(800, 600); await process_frame
	lab._toggle_tools(); await capture("10-narrow-tools")
	var report: Dictionary = {"engine": Engine.get_version_info().string, "display_server": DisplayServer.get_name(),
		"renderer": RenderingServer.get_current_rendering_method(), "captures": captures,
		"scope": "独立分层显示；真实世界、存档和美术用户评审不在通过范围"}
	var output: FileAccess = FileAccess.open(folder.path_join("visual-report.json"), FileAccess.WRITE)
	output.store_string(JSON.stringify(report, "\t")); output.close()
	print("CITY_VISUAL_PROBE_COMPLETE captures=", captures.size())
	lab.queue_free(); await process_frame; quit(0)
