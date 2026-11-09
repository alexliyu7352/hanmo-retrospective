## 第三成人族的真实图形操作；检查安全镜头、跨族语意、缺件拒绝与精确恢复。
extends SceneTree
var view: Control
var events: Array[String] = []

## 等场景树可用再运行，避免以headless像素替代实际窗口验证。
func _init() -> void:
	_run.call_deferred()

## 通过真正的鼠标命中操作三族控件，自动拍图并有界退出共享桌面。
func _run() -> void:
	if DisplayServer.get_name() == "headless":
		push_error("第三族图形验证需要真实显示。")
		quit(2)
		return
	create_timer(75.0).timeout.connect(_timeout)
	root.size = Vector2i(1440, 940)
	view = load("res://studio.tscn").instantiate()
	root.add_child(view)
	for frame in 5:
		await process_frame
	var baseline: Dictionary = view.session.state()
	var original_pixels: PackedByteArray = view.portrait.get_data()
	await _click("Choice_eyes_1")
	await _click("Choice_rig_2")
	assert(view.session.state().rig == 2)
	await _capture("gui-long-base")
	for id in ["Choice_face_1", "Choice_brows_1", "Choice_nose_1", "Choice_mouth_1", "Choice_hair_1", "Choice_outfit_1", "Choice_cloth_color_1"]:
		await _click(id)
	var dressed: Dictionary = view.session.state()
	var semantics: Dictionary = view.session.semantic_state()
	assert(semantics.nose == "round" and semantics.mouth == "smile")
	await _capture("gui-long-dressed")
	await _click("ViewAvatar")
	await _click("BackdropGrid")
	await _capture("gui-long-avatar")
	for rig in [1, 0, 2]:
		await _click("Choice_rig_" + str(rig))
		assert(view.session.semantic_state() == semantics)
	assert(view.session.state() == dressed)
	await _click("RestoreEntry")
	assert(view.session.state() == baseline and view.portrait.get_data() == original_pixels)
	await _click("Choice_rig_2")
	await _click("Choice_beard_1")
	await _click("Choice_hair_1")
	await _click("Choice_outfit_1")
	var bearded: Dictionary = view.session.state()
	var female: Button = view.find_child("Choice_rig_1", true, false)
	assert(female.disabled and female.tooltip_text.contains("胡须"))
	await _click("Choice_rig_1", true)
	assert(view.session.state() == bearded)
	await _click("ViewHalf")
	await _click("BackdropLight")
	await _capture("gui-long-beard-refusal")
	await _click("Choice_rig_0")
	assert(view.session.state().beard == 1)
	await _click("Undo")
	assert(view.session.state() == bearded)
	await _click("RestoreEntry")
	assert(view.session.state() == baseline and view.portrait.get_data() == original_pixels)
	var result := {"ok": true, "engine": Engine.get_version_info().string, "display": DisplayServer.get_name(), "viewport": [root.size.x, root.size.y], "events": events, "input_method": "InputEventMouseButton through real X11 SceneTree viewport", "semantic_ids_retained_across_three": true, "missing_part_refusal_kept_state": true, "source_portrait_crop": [54, 24, 920, 1380], "avatar_crop": [115, 9, 306, 306], "restored_pixels_exactly": true, "identity_scope": "three adult fitting families; 12 distinct identities not accepted"}
	FileAccess.open("res://evidence/long-gui-verification.json", FileAccess.WRITE).store_string(JSON.stringify(result, "\t") + "\n")
	print("LONG_GUI_VERIFIED " + JSON.stringify(result))
	quit()

## 先滚动到控件再注入按下和抬起，检验禁用态及真实命中而非直发信号。
func _click(id: String, allow_disabled: bool = false) -> void:
	var control: Control = view.find_child(id, true, false)
	assert(control != null)
	if control is Button and not allow_disabled:
		assert(not control.disabled)
	var scroll: ScrollContainer = view.find_child("ChoiceScroll", true, false)
	if scroll.is_ancestor_of(control):
		scroll.ensure_control_visible(control)
	for frame in 3:
		await process_frame
	var point := control.get_global_rect().get_center()
	for pressed in [true, false]:
		var event := InputEventMouseButton.new()
		event.button_index = MOUSE_BUTTON_LEFT
		event.pressed = pressed
		event.position = point
		root.push_input(event, true)
		await process_frame
	events.append("click:" + id)

## 从实际渲染提交后的viewport保存原图，空图或写出失败会中止本次验收。
func _capture(id: String) -> void:
	for frame in 5:
		await process_frame
	await RenderingServer.frame_post_draw
	var image := root.get_texture().get_image()
	assert(image != null and not image.is_empty())
	assert(image.save_png("res://evidence/" + id + ".png") == OK)

## 断言中断时自动失败退出，避免持续占用共享桌面或冒称图形通过。
func _timeout() -> void:
	push_error("第三族图形验证超时，未标记通过。")
	quit(2)
