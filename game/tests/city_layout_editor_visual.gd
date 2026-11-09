## 可编辑街段真实图形检查：鼠标拖放和道路材质均来自当前Godot画面，不以CPU图冒充。
extends SceneTree
const Editor = preload("res://demos/city_layout_editor.gd")
var editor: Control
var folder: String = ""
var captures: Array = []

## 图形进程就绪后再读取窗口，不访问任何存档或世界对象。
func _init() -> void:
	call_deferred("_run")

## 每步等待真实渲染结束后保全像素、已接受草案和候选状态。
func capture(name: String) -> void:
	await process_frame; await process_frame; await RenderingServer.frame_post_draw
	var image: Image = root.get_texture().get_image()
	if image.is_empty() or image.save_png(folder.path_join(name + ".png")) != OK: printerr("EDITOR_CAPTURE_FAILED"); quit(1); return
	captures.append({"name": name, "viewport": [image.get_width(), image.get_height()], "session": editor.session.status(),
		"layout": editor.session.layout(), "pending": editor.session.pending()})
	print("CITY_EDITOR_CAPTURE=", name)

## 真鼠标事件经过Viewport与画布手势逻辑，候选并非直接写入目标布局。
func drag_to(id: String, target: Vector2) -> void:
	var row: Dictionary = editor.canvas.find_instance(id)
	var logical: Vector2 = Vector2(row.rect[0] + row.rect[2] * 0.65, row.rect[1] + row.rect[3] * 0.44)
	var start: Vector2 = editor.canvas.logical_to_local(logical)
	var finish: Vector2 = editor.canvas.logical_to_local(logical + target - Vector2(row.foot[0], row.foot[1]))
	var down: InputEventMouseButton = InputEventMouseButton.new(); down.button_index = MOUSE_BUTTON_LEFT; down.pressed = true; down.position = start; down.global_position = start
	root.push_input(down, true); await process_frame
	var motion: InputEventMouseMotion = InputEventMouseMotion.new(); motion.position = finish; motion.global_position = finish; motion.button_mask = MOUSE_BUTTON_MASK_LEFT
	root.push_input(motion, true); await process_frame
	var up: InputEventMouseButton = InputEventMouseButton.new(); up.button_index = MOUSE_BUTTON_LEFT; up.position = finish; up.global_position = finish
	root.push_input(up, true); await process_frame

## 场景内按钮以真实信号进入同一编辑出口，不直接更改会话私有数据。
func press(name: String) -> void:
	var button: Button = editor.find_child(name, true, false)
	if button == null or button.disabled: printerr("EDITOR_BUTTON_UNAVAILABLE ", name); quit(1); return
	button.pressed.emit(); await process_frame


## 从当前作者地块取目标，保留同一身份场景的修前后对照。
func anchor(id: String) -> Vector2:
	for plot in editor.document.layouts[0].plots:
		if plot.id == id: return Vector2(plot.anchor[0], plot.anchor[1])
	return Vector2.ZERO

## 覆盖合法拖放、非法预览、取消、道路实际变化、历史和空地身份，以及窄窗操作。
func _run() -> void:
	if DisplayServer.get_name() == "headless": printerr("CITY_GRAPHICS_REQUIRED"); quit(2); return
	folder = OS.get_environment("HANMO_CITY_OUTPUT")
	if folder.is_empty(): quit(2); return
	root.size = Vector2i(1280, 800); editor = Editor.new(); root.add_child(editor)
	await process_frame
	if not editor._editor_ready: quit(1); return
	await capture("01-editable-street")
	await drag_to("market.grain", anchor("plot.1.1")); await capture("02-valid-drag-preview")
	await press("ConfirmEdit"); await capture("03-confirmed-empty-original-plot")
	await drag_to("market.grain", anchor("plot.0.0")); await capture("04-invalid-occupied-preview")
	await press("CancelEdit"); await capture("05-cancel-restores-layout")
	await press("UndoEdit"); await capture("06-undo-original")
	await press("PreviewRoad"); await capture("07-real-road-preview")
	await press("ConfirmEdit"); await capture("08-real-road-confirmed")
	await press("UndoEdit"); await press("RedoEdit"); await capture("09-road-redone")
	editor.selected = "market.grain"; await press("PreviewRemove"); await capture("10-empty-plot-preview")
	await press("CancelEdit")
	root.size = Vector2i(800, 600); await capture("11-narrow-editor")
	var file: FileAccess = FileAccess.open(folder.path_join("editor-visual-report.json"), FileAccess.WRITE)
	file.store_string(JSON.stringify({"engine": Engine.get_version_info().string, "display_server": DisplayServer.get_name(), "captures": captures}, "\t")); file.close()
	print("CITY_EDITOR_VISUAL_COMPLETE captures=", captures.size())
	editor.queue_free(); await process_frame; quit(0)
