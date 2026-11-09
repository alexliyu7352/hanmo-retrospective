## 有限路点编辑的实际Viewport证据，复用定点检查的鼠标拖动入口。
extends "res://tests/test_city_road_nodes.gd"
var editor: Control
var folder: String = ""
var captures: Array = []

## 实际帧、节点与当前事务同存，失败不输出完成回执。
func capture(name: String) -> void:
	await process_frame; await process_frame; await RenderingServer.frame_post_draw
	var image: Image = root.get_texture().get_image()
	if image.is_empty() or image.save_png(folder.path_join(name + ".png")) != OK: printerr("ROAD_CAPTURE_FAILED"); quit(1); return
	for control in editor._toolbar.get_children() + [editor._status]:
		if control.visible and control.get_global_rect().end.y > root.size.y: printerr("ROAD_CONTROL_CLIPPED ", control.name); quit(1); return
	captures.append({"name": name, "layout": editor.session.layout(), "pending": editor.session.pending(), "session": editor.session.status(), "access": editor.canvas.describe().access, "road_preview": editor.canvas._road_preview.duplicate(true)})
	print("CITY_ROAD_CAPTURE=", name)

## 按钮仍经真实Viewport按下抬起，不直接调用确定函数。
func press(name: String) -> void:
	var button: Button = editor.find_child(name, true, false)
	if button == null or button.disabled or not button.visible: printerr("ROAD_BUTTON_UNAVAILABLE ", name); quit(1); return
	for pressed in [true, false]:
		var event: InputEventMouseButton = InputEventMouseButton.new(); event.button_index = MOUSE_BUTTON_LEFT; event.pressed = pressed
		event.position = button.global_position + button.size * 0.5; event.global_position = event.position
		root.push_input(event, true); await process_frame

## 只覆盖本片的路点候选、连接、拒绝、恢复和窄窗，复用已核状态图与其他布局证据。
func _run() -> void:
	if DisplayServer.get_name() == "headless": printerr("CITY_GRAPHICS_REQUIRED"); quit(2); return
	folder = OS.get_environment("HANMO_CITY_OUTPUT"); if folder.is_empty(): quit(2); return
	root.size = Vector2i(1280, 800); editor = Editor.new(); root.add_child(editor); await process_frame
	if not editor._editor_ready: quit(1); return
	if "--safe-frame-review" in OS.get_cmdline_user_args():
		await press("PreviewRoad"); await capture("safe-desktop-road-nodes")
		root.size = Vector2i(800, 600); await capture("safe-narrow-road-nodes")
		await drag(editor, "junction.south", Vector2(1220, 832.5)); await capture("safe-narrow-junction-preview")
		await press("CancelEdit"); await press("PreviewRoad"); await capture("safe-building-controls-restored")
		await _finish(); return
	await press("PreviewRoad"); await capture("00-road-nodes")
	await drag(editor, "north.middle", Vector2(850, 459)); await capture("01-node-preview")
	await press("CancelEdit"); await capture("02-node-cancelled")
	await drag(editor, "junction.north", Vector2(1540, 681)); await capture("03-shared-junction-preview")
	await press("ConfirmEdit"); await capture("04-shared-junction-confirmed")
	await press("UndoEdit"); await capture("05-junction-undone")
	await drag(editor, "north.middle", Vector2(775, 699)); await capture("06-undeclared-crossing-rejected")
	await press("CancelEdit"); root.size = Vector2i(800, 600); await capture("07-narrow-road-controls")
	await press("PreviewRoad"); await capture("08-building-controls-restored")
	await _finish()

## 原始运行身份与候选一起保存，局部复核也保留独立退出回执。
func _finish() -> void:
	var file: FileAccess = FileAccess.open(folder.path_join("road-visual-report.json"), FileAccess.WRITE)
	file.store_string(JSON.stringify({"engine": Engine.get_version_info().string, "display_server": DisplayServer.get_name(), "captures": captures}, "\t")); file.close()
	print("CITY_ROAD_VISUAL_COMPLETE captures=", captures.size()); editor.queue_free(); await process_frame; quit(0)
