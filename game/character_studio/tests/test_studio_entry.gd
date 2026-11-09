## 代表入口与说明只围绕原会话操作，检验整组手动替换、原锁、单步历史与窄窗。
extends SceneTree
var checks := 0
var failures: Array[String] = []

## 待窗口尺寸可设置后建立真实独立工坊，普通启动入口保持原场景。
func _init() -> void:
	_run.call_deferred()

## 覆盖全部十二份明确样本，浏览/取消无变化，样本载入与独立后续编辑均可撤销。
func _run() -> void:
	root.size = Vector2i(1280, 900)
	root.content_scale_size = Vector2i(1280, 900)
	var view = load("res://studio.tscn").instantiate()
	root.add_child(view)
	for frame in 6:
		await process_frame
	_check(view._sample_catalog.ok and view._sample_catalog.samples.size() == 12, "入口读取十二样本")
	var entry: Dictionary = view.session.state()
	var original_pixels: PackedByteArray = view.portrait.get_data()
	_press(view, "OpenSamples")
	_check(view._sample_overlay.visible and view.session.state() == entry and not view.session.history().undo, "浏览不修改人物和历史")
	for frame in 4:
		await process_frame
	var panel: Control = view.find_child("SampleOverlayPanel", true, false)
	_check(Rect2(Vector2.ZERO, Vector2(1280, 900)).encloses(panel.get_global_rect()), "代表面板窄窗完整可见")
	var random_button: Control = view.find_child("Randomize", true, false)
	var before_hit: Dictionary = view.session.state()
	for pressed in [true, false]:
		var mouse := InputEventMouseButton.new()
		mouse.button_index = MOUSE_BUTTON_LEFT
		mouse.pressed = pressed
		mouse.position = random_button.get_global_rect().get_center()
		root.push_input(mouse, true)
		await process_frame
	_check(view.session.state() == before_hit and not view.session.history().undo, "覆盖层阻止误点底层随机")
	_press(view, "CloseSampleOverlay")
	_check(not view._sample_overlay.visible and view.session.state() == entry, "返回只关闭样本")
	for field in ["rig", "cloth_color", "eyes"]:
		var lock: CheckBox = view.find_child("Lock_" + field, true, false)
		lock.button_pressed = true
		_check(view.session.is_locked(field), "通过真实控件锁定 " + field)
	_press(view, "Choice_cloth_color_2")
	var before: Dictionary = view.session.state()
	var before_pixels: PackedByteArray = view.portrait.get_data()
	for sample in view._sample_catalog.samples:
		var undo_size: int = view.session._undo.size()
		_press(view, "OpenSamples")
		_press(view, "Sample_" + sample.id)
		_check(view.session.state() == sample.state and not view._sample_overlay.visible, "整组载入实际样本 " + sample.id)
		_check(view.session._undo.size() == undo_size + 1, "整组只增加一步历史 " + sample.id)
		for field in ["rig", "cloth_color", "eyes"]:
			_check(view.session.is_locked(field) and view.find_child("Lock_" + field, true, false).button_pressed, "保留原锁 " + field)
		var sample_pixels: PackedByteArray = view.portrait.get_data()
		_press(view, "Choice_eyes_" + str(1 - sample.state.eyes))
		_check(view.portrait.get_data() != sample_pixels and view.session.state().face == sample.state.face, "样本之后仍可独立编辑眼形")
		_press(view, "Undo")
		_check(view.session.state() == sample.state and view.portrait.get_data() == sample_pixels, "撤销独立编辑返回样本")
		_press(view, "Undo")
		_check(view.session.state() == before and view.portrait.get_data() == before_pixels, "一步撤销样本返回原人物")
	var history: Array = [view.session._undo.duplicate(true), view.session._redo.duplicate(true)]
	view._apply_sample("missing-sample")
	_check(view.session.state() == before and view.portrait.get_data() == before_pixels and history == [view.session._undo, view.session._redo], "未知样本原子拒绝")
	_press(view, "OpenGuide")
	for frame in 4:
		await process_frame
	panel = view.find_child("GuideOverlayPanel", true, false)
	_check(Rect2(Vector2.ZERO, Vector2(1280, 900)).encloses(panel.get_global_rect()), "操作边界说明窄窗完整可见")
	var escape := InputEventKey.new()
	escape.keycode = KEY_ESCAPE
	escape.pressed = true
	root.push_input(escape, true)
	await process_frame
	_check(not view._guide_overlay.visible and view.session.state() == before, "Escape关闭说明不改人物")
	_press(view, "RestoreEntry")
	_check(view.session.state() == entry and view.portrait.get_data() == original_pixels, "连贯入口路径后恢复原像素")
	var report := {"ok": failures.is_empty(), "checks": checks, "failures": failures, "viewport": [1280, 900], "samples": 12, "scope": "real controls, atomic complete-sample apply, retained random-only locks, independent edits and undo; actual GUI verification separate"}
	FileAccess.open("res://evidence/studio-entry-tests.json", FileAccess.WRITE).store_string(JSON.stringify(report, "\t") + "\n")
	print(JSON.stringify(report))
	view.queue_free()
	await process_frame
	quit(0 if failures.is_empty() else 1)

## 发出原控件信号进入同一事务，禁用或缺失按钮明确记录失败。
func _press(view: Node, id: String) -> void:
	var button: Button = view.find_child(id, true, false)
	_check(button != null and not button.disabled, "真实入口控件 " + id)
	if button != null and not button.disabled:
		button.pressed.emit()

## 不以部分成功终止验证，汇总所有失败后非零退出。
func _check(condition: bool, label: String) -> void:
	checks += 1
	if not condition:
		failures.append(label)
		push_error("STUDIO_ENTRY_FAILED: " + label)
