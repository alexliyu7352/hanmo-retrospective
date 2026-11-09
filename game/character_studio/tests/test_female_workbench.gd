## 女性四骨相工作台真控件与三族语意保持，明确女性无须件与拒绝历史边界。
extends SceneTree
var checks := 0
var failures: Array[String] = []

## 使用可控窄窗建立真实场景，图形鼠标与截图验收另行运行。
func _init() -> void:
	_run.call_deferred()

## 覆盖女性四结构各自选件、共同骨相跨族、缺件原子拒绝与入口像素恢复。
func _run() -> void:
	root.size = Vector2i(1280, 900)
	root.content_scale_size = Vector2i(1280, 900)
	var view = load("res://studio.tscn").instantiate()
	root.add_child(view)
	for frame in 5:
		await process_frame
	var entry: Dictionary = view.session.state()
	var pixels: PackedByteArray = view.portrait.get_data()
	_press(view, "Choice_rig_1")
	_check(view.session.field_definition("face").options.size() == 4, "女性正式登记四骨相")
	for feature in ["brows", "nose", "mouth", "eyes"]:
		_press(view, "Choice_" + feature + "_1")
	for face in 4:
		_press(view, "Choice_face_" + str(face))
		_check(view.session.state().face == face and view.session.state().rig == 1, "女性骨相真实可选")
		for feature in ["brows", "eyes", "nose", "mouth"]:
			_check(view.session.state()[feature] == 1, "女性换骨相保留 " + feature)
	var female: Dictionary = view.session.state()
	var semantics: Dictionary = view.session.semantic_state()
	var female_pixels: PackedByteArray = view.portrait.get_data()
	for rig in [0, 2]:
		_press(view, "Choice_rig_" + str(rig))
		_check(view.session.state().rig == rig and view.session.semantic_state() == semantics and view.session.state().face == 3, "三族第四骨相保留完整语意 " + str(rig))
		_press(view, "Undo")
		_check(view.session.state() == female and view.portrait.get_data() == female_pixels, "撤销切族恢复女性实际像素 " + str(rig))
	var beard: Button = view.find_child("Choice_beard_1", true, false)
	_check(beard.disabled and beard.tooltip_text.contains("胡须"), "女性未制作短须不会借男性素材")
	var female_history: Array = [view.session._undo.duplicate(true), view.session._redo.duplicate(true)]
	beard.pressed.emit()
	_check(view.session.state() == female and view.portrait.get_data() == female_pixels, "点击女性缺须件保留草案与像素")
	_check([view.session._undo, view.session._redo] == female_history, "点击女性缺须件保留完整撤销重做历史")
	var scroll: ScrollContainer = view.find_child("ChoiceScroll", true, false)
	for frame in 5:
		await process_frame
	for face in 4:
		var button: Button = view.find_child("Choice_face_" + str(face), true, false)
		_check(button.get_global_rect().end.x <= scroll.get_global_rect().end.x, "女性四项窄窗不横向溢出")
	_press(view, "Choice_rig_2")
	_check(view.session.state().face == 3 and view.session.semantic_state() == semantics, "第四骨相再次进入偏长族保持语意")
	for frame in 5:
		await process_frame
	for face in 4:
		var button: Button = view.find_child("Choice_face_" + str(face), true, false)
		_check(not button.disabled and not button.text.contains("未适配"), "偏长四骨相全部可用 " + str(face))
		_check(button.get_global_rect().end.x <= scroll.get_global_rect().end.x, "偏长四项窄窗不横向溢出 " + str(face))
	for face in [2, 3]:
		_press(view, "Choice_face_" + str(face))
		_check(view.session.state().face == face, "偏长新增骨相真按钮可选 " + str(face))
		for feature in ["brows", "eyes", "nose", "mouth"]:
			_check(view.session.semantic_state()[feature] == semantics[feature], "偏长换骨相保留独立语意 " + feature)
	for id in ["Choice_face_0", "Choice_mouth_0", "Choice_beard_1"]:
		_press(view, id)
	var bearded: Dictionary = view.session.state()
	var bearded_pixels: PackedByteArray = view.portrait.get_data()
	var bearded_history: Array = [view.session._undo.duplicate(true), view.session._redo.duplicate(true)]
	var missing: Button = view.find_child("Choice_rig_1", true, false)
	_check(missing.disabled and missing.tooltip_text.contains("胡须"), "带短须回女性族明确指出真实缺件")
	missing.pressed.emit()
	_check(view.session.state() == bearded and view.portrait.get_data() == bearded_pixels, "缺须件切族拒绝保留草案与像素")
	_check([view.session._undo, view.session._redo] == bearded_history, "缺须件切族拒绝保留完整历史")
	_press(view, "RestoreEntry")
	_check(view.session.state() == entry and view.portrait.get_data() == pixels, "恢复进入时像素不变")
	var report := {"ok": failures.is_empty(), "checks": checks, "failures": failures, "viewport": [1280, 900], "scope": "four female shapes in real controls; actual X11 mouse proof separate"}
	FileAccess.open("res://evidence/female-workbench-tests-extended.json", FileAccess.WRITE).store_string(JSON.stringify(report, "\t") + "\n")
	print(JSON.stringify(report))
	view.queue_free()
	await process_frame
	quit(0 if failures.is_empty() else 1)

## 用现有真实按钮进入同一会话校验路径，不直改内部草案。
func _press(view: Node, id: String) -> void:
	var button: Button = view.find_child(id, true, false)
	_check(button != null and not button.disabled, "可操作 " + id)
	if button != null and not button.disabled:
		button.pressed.emit()

## 独立记录问题并统一返回失败，避免某一断言掩盖其他状态错误。
func _check(condition: bool, label: String) -> void:
	checks += 1
	if not condition:
		failures.append(label)
		push_error("FEMALE_WORKBENCH_FAILED: " + label)
