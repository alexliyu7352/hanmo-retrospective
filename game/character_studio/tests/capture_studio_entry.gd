## 独立人物工坊收口路径：从正式场景进入，样本、独立编辑、撤销与说明连贯验证。
extends "res://tests/capture_medium_workbench.gd"

## 实际鼠标走一次普通用户入口，不以静态样本图代替交互和原生viewport。
func _run() -> void:
	if DisplayServer.get_name() == "headless":
		push_error("人物入口收口需要真实显示。")
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
	await _capture("gui-entry-start")
	await _click("OpenGuide")
	await _capture("gui-entry-guide")
	await _click("CloseGuideOverlay")
	assert(view.session.state() == entry)
	await _click("Lock_rig")
	await _click("Lock_eyes")
	await _click("OpenSamples")
	await _capture("gui-entry-samples")
	var female: Dictionary = view._sample_catalog.samples[5]
	await _click("Sample_" + female.id)
	assert(view.session.state() == female.state and view.session.is_locked("rig") and view.session.is_locked("eyes"))
	var female_pixels: PackedByteArray = view.portrait.get_data()
	await _capture("gui-entry-female-loaded")
	for id in ["Choice_eyes_0", "Choice_nose_1", "Choice_hair_1", "Choice_outfit_1"]:
		await _click(id)
	await _capture("gui-entry-female-edited")
	for step in 4:
		await _click("Undo")
	assert(view.session.state() == female.state and view.portrait.get_data() == female_pixels)
	await _click("Undo")
	assert(view.session.state() == entry and view.portrait.get_data() == pixels)
	assert(view.session.is_locked("rig") and view.session.is_locked("eyes"))
	await _capture("gui-entry-sample-undo")
	await _click("OpenSamples")
	var long_sample: Dictionary = view._sample_catalog.samples[10]
	await _click("Sample_" + long_sample.id)
	assert(view.session.state() == long_sample.state)
	await _click("ViewAvatar")
	await _click("BackdropGrid")
	await _capture("gui-entry-long-avatar")
	root.size = Vector2i(1280, 900)
	root.content_scale_size = Vector2i(1280, 900)
	await _click("OpenSamples")
	await _capture("gui-entry-samples-narrow")
	await _click("CloseSampleOverlay")
	assert(view.session.state() == long_sample.state)
	await _click("OpenGuide")
	await _capture("gui-entry-guide-narrow")
	await _click("CloseGuideOverlay")
	await _click("RestoreEntry")
	assert(view.session.state() == entry and view.portrait.get_data() == pixels)
	var report := {"ok": true, "display": DisplayServer.get_name(), "engine": Engine.get_version_info().string, "events": events, "input_method": "InputEventMouseButton through actual X11 SceneTree viewport", "viewports": [[1440, 940], [1280, 900]], "samples_loaded_whole": true, "locks_retained_but_manual_sample_replaces": true, "independent_edits_after_sample": true, "one_undo_restores_before_sample": true, "guide_cancel_kept_state": true, "entry_pixels_restored": true, "scope": "standalone studio entry closure; existing twelve sampled adult identities, no new art/age/body/save/Main scope"}
	FileAccess.open("res://evidence/studio-entry-gui-verification.json", FileAccess.WRITE).store_string(JSON.stringify(report, "\t") + "\n")
	print("STUDIO_ENTRY_GUI_VERIFIED " + JSON.stringify(report))
	quit()
