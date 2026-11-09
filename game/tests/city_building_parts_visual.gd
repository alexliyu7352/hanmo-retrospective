## 净楼体与布棚的真实合成审图：同楼近景开关、移位和未适配状态分别保全。
extends SceneTree
const Editor = preload("res://demos/city_building_parts_editor.gd")
var editor: Control
var folder: String = ""
var captures: Array = []

## 在真实窗口建立后执行，headless不得生成美术通过回执。
func _init() -> void:
	call_deferred("_run")

## 真实Viewport原帧和部件矩形一起保存，便于追查接合和漂移。
func capture(name: String) -> void:
	await process_frame; await process_frame; await RenderingServer.frame_post_draw
	var image: Image = root.get_texture().get_image()
	if image.is_empty() or image.save_png(folder.path_join(name + ".png")) != OK: printerr("PART_CAPTURE_FAILED"); quit(1); return
	captures.append({"name": name, "instance": editor.canvas.find_instance("market.grain"), "layout": editor.session.layout(), "active_place": editor.active_place, "session": editor.session.status()})
	print("CITY_PART_CAPTURE=", name)

## 按钮和布棚开关经真实Viewport输入，禁用项不会被脚本强制启用。
func press(name: String) -> void:
	var button: BaseButton = editor.find_child(name, true, false)
	if button == null or button.disabled or not button.visible: printerr("PART_BUTTON_UNAVAILABLE ", name); quit(1); return
	for pressed in [true, false]:
		var event: InputEventMouseButton = InputEventMouseButton.new(); event.button_index = MOUSE_BUTTON_LEFT; event.pressed = pressed
		event.position = button.global_position + button.size * 0.5; event.global_position = event.position
		root.push_input(event, true); await process_frame

## 同一粮铺部件开关与受控移位使用当前会话，不为截图另造一栋楼。
func _run() -> void:
	if DisplayServer.get_name() == "headless": printerr("CITY_GRAPHICS_REQUIRED"); quit(2); return
	folder = OS.get_environment("HANMO_CITY_OUTPUT"); if folder.is_empty(): quit(2); return
	root.size = Vector2i(1280, 800); editor = Editor.new(); root.add_child(editor); await process_frame
	if not editor._editor_ready: quit(1); return
	if "--contrast-review" in OS.get_cmdline_user_args():
		await capture("ui-readable-enabled")
		editor.preview_visual_state("damaged"); await press("ConfirmEdit"); root.size = Vector2i(800, 600)
		await capture("ui-readable-disabled-narrow"); await _finish(); return
	await capture("00-composed-district")
	await press("EditMode"); await press("Place_market_grain"); await capture("01-composed-closeup")
	await press("ReturnToDistrict"); await press("EditMode"); await press("Part_awning"); await capture("02-removal-preview")
	await press("ConfirmEdit"); await press("EditMode"); await press("Place_market_grain"); await capture("03-uncovered-closeup")
	await press("ReturnToDistrict"); await press("EditMode"); await press("UndoEdit"); await capture("04-decoration-restored")
	var target: Vector2 = Vector2.ZERO
	for plot in editor.layout.plots:
		if plot.id == "plot.1.1": target = Vector2(plot.anchor[0], plot.anchor[1])
	editor._dragged("market.grain", target, true); await press("ConfirmEdit"); await capture("05-moved-with-parts")
	await press("UndoEdit"); editor.preview_visual_state("damaged"); await press("ConfirmEdit"); await capture("06-incompatible-damaged")
	root.size = Vector2i(800, 600); await capture("07-narrow-disabled-part")
	await _finish()

## 记录当前运行的实际帧和数据源后正常退出，窄修与原结果分开。
func _finish() -> void:
	var file: FileAccess = FileAccess.open(folder.path_join("part-visual-report.json"), FileAccess.WRITE)
	file.store_string(JSON.stringify({"engine": Engine.get_version_info().string, "display_server": DisplayServer.get_name(), "captures": captures}, "\t")); file.close()
	print("CITY_PART_VISUAL_COMPLETE captures=", captures.size()); editor.queue_free(); await process_frame; quit(0)
