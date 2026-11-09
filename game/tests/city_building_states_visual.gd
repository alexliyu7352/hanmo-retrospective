## 同粮铺状态链真实窗口像素验证：菜单、确定、取消与近景共用实际工作台。
extends SceneTree
const Editor = preload("res://demos/city_building_state_editor.gd")
var editor: Control
var folder: String = ""
var captures: Array = []

## 等待真实图形进程建立；无图形模式不产生截图成功回执。
func _init() -> void:
	call_deferred("_run")

## 截取真实Viewport并绑定同一实例描述，不重新生成一张效果图替代运行结果。
func capture(name: String) -> void:
	await process_frame; await process_frame; await RenderingServer.frame_post_draw
	var image: Image = root.get_texture().get_image()
	if image.is_empty() or image.save_png(folder.path_join(name + ".png")) != OK: printerr("STATE_CAPTURE_FAILED"); quit(1); return
	var controls: Array = []
	for control in editor._toolbar.get_children() + [editor._status]:
		if not control.visible: continue
		var rect: Rect2 = control.get_global_rect()
		controls.append({"name": control.name, "rect": [rect.position.x, rect.position.y, rect.size.x, rect.size.y]})
		if rect.end.x > root.size.x or rect.end.y > root.size.y:
			printerr("STATE_CONTROL_CLIPPED ", control.name); quit(1); return
	captures.append({"name": name, "instance": editor.canvas.find_instance("market.grain"), "layout": editor.session.layout(), "active_place": editor.active_place, "session": editor.session.status(), "controls": controls, "viewport_size": [root.size.x, root.size.y]})
	print("CITY_STATE_CAPTURE=", name)

## 选择当前菜单中的已制作状态，经相同item_selected回调进入候选。
func choose_state(state: String) -> void:
	for index in range(editor._state_choice.item_count):
		if editor._state_choice.get_item_metadata(index) == state:
			if editor._state_choice.is_item_disabled(index): printerr("STATE_CHOICE_DISABLED ", state); quit(1); return
			editor._state_choice.select(index); editor._state_choice.item_selected.emit(index); await process_frame; return
	printerr("STATE_CHOICE_MISSING ", state); quit(1)

## 按钮用真实Viewport鼠标按下和抬起，不直接调用确定函数冒充可点操作。
func press(name: String) -> void:
	var button: Button = editor.find_child(name, true, false)
	if button == null or button.disabled or not button.visible: printerr("STATE_BUTTON_UNAVAILABLE ", name); quit(1); return
	for pressed in [true, false]:
		var event: InputEventMouseButton = InputEventMouseButton.new(); event.button_index = MOUSE_BUTTON_LEFT; event.pressed = pressed
		event.position = button.global_position + button.size * 0.5; event.global_position = event.position
		root.push_input(event, true); await process_frame

## 六种显示状态、缺失状态拒绝、同地块替换及两种低高形体近景逐一保全。
func _run() -> void:
	if DisplayServer.get_name() == "headless": printerr("CITY_GRAPHICS_REQUIRED"); quit(2); return
	folder = OS.get_environment("HANMO_CITY_OUTPUT")
	if folder.is_empty(): quit(2); return
	root.size = Vector2i(1280, 800); editor = Editor.new(); root.add_child(editor); await process_frame
	if not editor._editor_ready: quit(1); return
	if "--footer-review" in OS.get_cmdline_user_args():
		await _footer_review(); return
	await capture("00-original-grainshop")
	var index: int = 1
	for state in ["empty", "foundation", "construction", "complete", "damaged", "repair"]:
		await choose_state(state); await press("ConfirmEdit"); await capture("%02d-%s" % [index, state]); index += 1
	editor.preview_building_type("inn"); await capture("07-missing-state-rejected")
	await press("CancelEdit")
	await choose_state("complete"); await press("ConfirmEdit")
	editor.preview_building_type("inn"); await capture("08-same-parcel-replacement-preview")
	await press("ConfirmEdit"); await press("UndoEdit"); await capture("09-replacement-undone")
	await choose_state("foundation"); await press("ConfirmEdit"); await press("EditMode"); await press("Place_market_grain")
	await capture("10-foundation-closeup")
	await press("ReturnToDistrict"); await press("EditMode")
	await choose_state("damaged"); await press("ConfirmEdit"); await press("EditMode"); await press("Place_market_grain")
	await capture("11-damaged-closeup")
	await press("ReturnToDistrict"); await press("EditMode")
	root.size = Vector2i(800, 600); await capture("12-narrow-state-controls")
	await _finish()

## 仅复核被裁切的近景和窄窗底栏，不重复无关布局或状态矩阵。
func _footer_review() -> void:
	for state in ["foundation", "damaged"]:
		await choose_state(state); await press("ConfirmEdit"); await press("EditMode"); await press("Place_market_grain")
		await capture("footer-" + state + "-closeup")
		if state == "damaged":
			root.size = Vector2i(800, 600); await capture("footer-narrow-closeup")
		await press("ReturnToDistrict"); await press("EditMode")
	await capture("footer-narrow-editing")
	await _finish()

## 保存同一运行的原始身份描述并正常退出，供图像与可逆操作复核。
func _finish() -> void:
	var file: FileAccess = FileAccess.open(folder.path_join("state-visual-report.json"), FileAccess.WRITE)
	file.store_string(JSON.stringify({"engine": Engine.get_version_info().string, "display_server": DisplayServer.get_name(), "captures": captures}, "\t")); file.close()
	print("CITY_STATE_VISUAL_COMPLETE captures=", captures.size())
	editor.queue_free(); await process_frame; quit(0)
