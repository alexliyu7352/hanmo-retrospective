## 女性四骨相真实图形验收，复用同一鼠标命中/截图/超时助手但独立验证实际路径。
extends "res://tests/capture_medium_workbench.gd"

## 保留同轻微含笑的四代表，再同时换眼鼻、衣巾、跨族与窄窗，不使用静态图冒充操作。
func _run() -> void:
	if DisplayServer.get_name() == "headless":
		push_error("女性四骨相验收需要真实显示。")
		quit(2)
		return
	create_timer(110.0).timeout.connect(_timeout)
	root.size = Vector2i(1440, 940)
	root.content_scale_size = Vector2i(1440, 940)
	view = load("res://studio.tscn").instantiate()
	root.add_child(view)
	for frame in 6:
		await process_frame
	var entry: Dictionary = view.session.state()
	var pixels: PackedByteArray = view.portrait.get_data()
	await _click("Choice_rig_1")
	for id in ["Choice_eyes_1", "Choice_nose_1", "Choice_mouth_1"]:
		await _click(id)
	await _capture("gui-female-four-base-extended")
	await _click("Choice_face_1")
	await _click("Choice_nose_0")
	await _capture("gui-female-four-angular-extended")
	await _click("Choice_face_2")
	await _click("Choice_eyes_0")
	await _click("Choice_nose_1")
	await _capture("gui-female-four-lower-cheek-extended")
	await _click("Choice_face_3")
	await _click("Choice_nose_0")
	await _capture("gui-female-four-tapered-extended")
	await _click("Choice_face_1")
	await _click("Choice_nose_1")
	await _click("Choice_brows_1")
	assert(view.session.state().eyes == 0 and view.session.state().nose == 1 and view.session.state().mouth == 1)
	await _capture("gui-female-four-angular-open-round-extended")
	for id in ["Choice_hair_1", "Choice_outfit_1", "Choice_cloth_color_1"]:
		await _click(id)
	await _capture("gui-female-four-angular-dressed-extended")
	await _click("ViewAvatar")
	await _click("BackdropGrid")
	await _capture("gui-female-four-angular-avatar-extended")
	await _click("Choice_face_3")
	var female: Dictionary = view.session.state()
	var female_pixels: PackedByteArray = view.portrait.get_data()
	var semantics: Dictionary = view.session.semantic_state()
	for rig in [0, 2]:
		await _click("Choice_rig_" + str(rig))
		assert(view.session.state().rig == rig and view.session.state().face == 3 and view.session.semantic_state() == semantics)
		await _click("Undo")
		assert(view.session.state() == female and view.portrait.get_data() == female_pixels)
	var beard: Button = view.find_child("Choice_beard_1", true, false)
	assert(beard.disabled and beard.tooltip_text.contains("胡须"))
	var female_history: Array = [view.session._undo.duplicate(true), view.session._redo.duplicate(true)]
	await _click("Choice_beard_1", true)
	assert(view.session.state() == female and view.portrait.get_data() == female_pixels)
	assert([view.session._undo, view.session._redo] == female_history)
	root.size = Vector2i(1280, 900)
	root.content_scale_size = Vector2i(1280, 900)
	await _click("ViewHalf")
	await _click("BackdropLight")
	await _capture("gui-female-four-beard-refusal-narrow-extended")
	await _click("Choice_rig_2")
	assert(view.session.state().face == 3 and view.session.semantic_state() == semantics)
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
	await _capture("gui-female-four-cross-family-narrow-extended")
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
	await _capture("gui-female-four-bearded-cross-refusal-extended")
	await _click("RestoreEntry")
	assert(view.session.state() == entry and view.portrait.get_data() == pixels)
	var report := {"ok": true, "engine": Engine.get_version_info().string, "display": DisplayServer.get_name(), "events": events, "input_method": "InputEventMouseButton through actual X11 SceneTree viewport", "full_viewport": [1440, 940], "narrow_viewport": [1280, 900], "angular_open_eye_round_nose_simultaneous": true, "same_expression_representatives": true, "three_family_face3_semantics_retained": true, "disabled_female_beard_kept_state_pixels_history": true, "missing_beard_kept_state_pixels_history": true, "undo_restored_female_pixels": true, "entry_pixels_restored": true, "scope": "three adult fittings each expose four structural options; identity artwork acceptance is recorded separately"}
	FileAccess.open("res://evidence/female-four-gui-verification-extended.json", FileAccess.WRITE).store_string(JSON.stringify(report, "\t") + "\n")
	print("FEMALE_FOUR_GUI_EXTENDED_VERIFIED " + JSON.stringify(report))
	quit()
