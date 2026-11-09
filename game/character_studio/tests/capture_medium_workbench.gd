## 四骨相真实图形与鼠标验收，覆盖三族保语意、真实缺须拒绝与窄窗换行。
extends SceneTree
var view: Control
var events: Array[String] = []

## 只在真实显示运行，headless明确拒绝，不能伪造图形成功。
func _init() -> void:
	_run.call_deferred()

## 用实际鼠标命中逐项组合，保存原生viewport后自动释放共享桌面。
func _run() -> void:
	if DisplayServer.get_name() == "headless":
		push_error("四骨相工坊验收需要真实显示。")
		quit(2)
		return
	create_timer(100.0).timeout.connect(_timeout)
	root.size = Vector2i(1440, 940)
	root.content_scale_size = Vector2i(1440, 940)
	view = load("res://studio.tscn").instantiate()
	root.add_child(view)
	for frame in 6:
		await process_frame
	var entry: Dictionary = view.session.state()
	var pixels: PackedByteArray = view.portrait.get_data()
	await _capture("gui-medium-oval-extended")
	await _click("Choice_face_1")
	await _click("Choice_brows_1")
	await _capture("gui-medium-sturdy-extended")
	await _click("Choice_face_2")
	await _click("Choice_brows_0")
	await _click("Choice_nose_1")
	await _capture("gui-medium-compact-extended")
	await _click("Choice_face_3")
	await _click("Choice_eyes_1")
	await _click("Choice_nose_0")
	await _capture("gui-medium-diamond-extended")
	await _click("Choice_face_2")
	for id in ["Choice_brows_1", "Choice_nose_1", "Choice_mouth_1", "Choice_hair_1", "Choice_outfit_1", "Choice_cloth_color_1"]:
		await _click(id)
	assert(view.session.state().face == 2 and view.session.state().eyes == 1)
	await _capture("gui-medium-compact-dressed-extended")
	await _click("ViewAvatar")
	await _click("BackdropGrid")
	await _capture("gui-medium-compact-avatar-extended")
	var before: Dictionary = view.session.state()
	var semantics: Dictionary = view.session.semantic_state()
	await _click("Choice_face_3")
	for field in before:
		if field != "face":
			assert(view.session.state()[field] == before[field])
	var kept: Dictionary = view.session.state()
	var kept_pixels: PackedByteArray = view.portrait.get_data()
	semantics = view.session.semantic_state()
	for rig in [1, 2]:
		await _click("Choice_rig_" + str(rig))
		assert(view.session.state().rig == rig and view.session.state().face == 3 and view.session.semantic_state() == semantics)
		await _click("Undo")
		assert(view.session.state() == kept and view.portrait.get_data() == kept_pixels)
	await _click("Choice_rig_2")
	assert(view.session.state().face == 3 and view.session.semantic_state() == semantics)
	await _click("ViewHalf")
	await _click("BackdropLight")
	root.size = Vector2i(1280, 900)
	root.content_scale_size = Vector2i(1280, 900)
	for frame in 6:
		await process_frame
	var scroll: ScrollContainer = view.find_child("ChoiceScroll", true, false)
	for face in 4:
		var button: Button = view.find_child("Choice_face_" + str(face), true, false)
		assert(button.get_global_rect().end.x <= scroll.get_global_rect().end.x)
		assert(not button.disabled and not button.text.contains("未适配"))
	for face in [2, 3]:
		await _click("Choice_face_" + str(face))
		assert(view.session.state().face == face)
		for feature in ["brows", "eyes", "nose", "mouth"]:
			assert(view.session.semantic_state()[feature] == semantics[feature])
	await _capture("gui-medium-cross-family-narrow-extended")
	for id in ["Choice_face_0", "Choice_mouth_0", "Choice_beard_1"]:
		await _click(id)
	var bearded: Dictionary = view.session.state()
	var bearded_pixels: PackedByteArray = view.portrait.get_data()
	var bearded_history: Array = [view.session._undo.duplicate(true), view.session._redo.duplicate(true)]
	var missing: Button = view.find_child("Choice_rig_1", true, false)
	assert(missing.disabled and missing.tooltip_text.contains("胡须"))
	await _click("Choice_rig_1", true)
	assert(view.session.state() == bearded and view.portrait.get_data() == bearded_pixels)
	assert([view.session._undo, view.session._redo] == bearded_history)
	await _capture("gui-medium-beard-refusal-narrow-extended")
	await _click("RestoreEntry")
	assert(view.session.state() == entry and view.portrait.get_data() == pixels)
	var report := {"ok": true, "engine": Engine.get_version_info().string, "display": DisplayServer.get_name(), "events": events, "input_method": "InputEventMouseButton through actual X11 SceneTree viewport", "full_viewport": [1440, 940], "narrow_viewport": [1280, 900], "face_switch_kept_other_choices": true, "three_family_face3_semantics_retained": true, "undo_restored_medium_pixels": true, "missing_beard_kept_state_pixels_history": true, "restored_pixels_exactly": true, "scope": "three adult fittings each expose four structural options; identity artwork acceptance is recorded separately"}
	FileAccess.open("res://evidence/medium-gui-verification-extended.json", FileAccess.WRITE).store_string(JSON.stringify(report, "\t") + "\n")
	print("MEDIUM_GUI_EXTENDED_VERIFIED " + JSON.stringify(report))
	quit()

## 把按钮滚入可视区后按下及抬起，禁用态也用真实命中确认不会暗换选件。
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

## 只保存已实际提交的viewport，不接受空图或未成功写出的截图。
func _capture(id: String) -> void:
	for frame in 5:
		await process_frame
	await RenderingServer.frame_post_draw
	var image := root.get_texture().get_image()
	assert(image != null and not image.is_empty())
	assert(image.save_png("res://evidence/" + id + ".png") == OK)

## 任何流程断言中断仍有界失败退出，不能长期占着共享验证窗口。
func _timeout() -> void:
	push_error("四骨相工坊图形验收超时，未标记通过。")
	quit(2)
