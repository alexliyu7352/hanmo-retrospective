## 四骨相真实控件与跨族缺件拒绝，检验新增选项能操作且窄窗不横向挤出。
extends SceneTree
var checks := 0
var failures: Array[String] = []

## 等根窗口可设置尺寸后建立独立工坊，无需图形设备或主世界。
func _init() -> void:
	_run.call_deferred()

## 覆盖三族四骨相语意、真实短须缺件拒绝、窄窗换行与撤销恢复像素。
func _run() -> void:
	root.size = Vector2i(1280, 900)
	root.content_scale_size = Vector2i(1280, 900)
	var view = load("res://studio.tscn").instantiate()
	root.add_child(view)
	for frame in 5:
		await process_frame
	var entry: Dictionary = view.session.state()
	var pixels: PackedByteArray = view.portrait.get_data()
	_check(view.catalog.fields.face.options.size() == 4, "工作台登记四骨相")
	for feature in ["brows", "eyes", "nose", "mouth"]:
		_press(view, "Choice_" + feature + "_1")
	for face in 4:
		_press(view, "Choice_face_" + str(face))
		_check(view.session.state().face == face, "真按钮选择骨相 " + str(face))
		for feature in ["brows", "eyes", "nose", "mouth"]:
			_check(view.session.state()[feature] == 1, "换骨相保留独立" + feature)
	var before: Dictionary = view.session.state()
	var semantics: Dictionary = view.session.semantic_state()
	var before_pixels: PackedByteArray = view.portrait.get_data()
	for rig in [1, 2]:
		_press(view, "Choice_rig_" + str(rig))
		_check(view.session.state().rig == rig and view.session.state().face == 3 and view.session.semantic_state() == semantics, "三族第四骨相保留完整语意 " + str(rig))
		_press(view, "Undo")
		_check(view.session.state() == before and view.portrait.get_data() == before_pixels, "撤销切族恢复适中原像素 " + str(rig))
	_press(view, "Choice_rig_2")
	_check(view.session.semantic_state() == semantics, "第四骨相再次进入偏长族保持语意")
	for frame in 5:
		await process_frame
	var scroll: ScrollContainer = view.find_child("ChoiceScroll", true, false)
	for face in 4:
		var button: Button = view.find_child("Choice_face_" + str(face), true, false)
		_check(button.get_global_rect().end.x <= scroll.get_global_rect().end.x, "窄窗四骨相标签不横向溢出 " + str(face))
		_check(not button.disabled and not button.text.contains("未适配"), "偏长族已制作骨相可用 " + str(face))
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
	_check(missing.disabled and missing.tooltip_text.contains("胡须"), "带真实短须切女性族明确拒绝")
	missing.pressed.emit()
	_check(view.session.state() == bearded and view.portrait.get_data() == bearded_pixels, "缺须件拒绝保留全部草案和像素")
	_check([view.session._undo, view.session._redo] == bearded_history, "拒绝切族不改撤销重做历史")
	_press(view, "RestoreEntry")
	_check(view.session.state() == entry and view.portrait.get_data() == pixels, "跨族操作后恢复原像素")
	var report := {"ok": failures.is_empty(), "checks": checks, "failures": failures, "viewport": [1280, 900], "scope": "real scene controls and layout, actual graphical mouse verification separate"}
	FileAccess.open("res://evidence/medium-workbench-tests-extended.json", FileAccess.WRITE).store_string(JSON.stringify(report, "\t") + "\n")
	print(JSON.stringify(report))
	view.queue_free()
	await process_frame
	quit(0 if failures.is_empty() else 1)

## 真控件信号经过原有会话校验，测试不能直接改会话内部字典。
func _press(view: Node, id: String) -> void:
	var button: Button = view.find_child(id, true, false)
	_check(button != null and not button.disabled, "可用控件 " + id)
	if button != null and not button.disabled:
		button.pressed.emit()

## 累积独立失败，最后以明确非零退出阻止误报完成。
func _check(condition: bool, label: String) -> void:
	checks += 1
	if not condition:
		failures.append(label)
		push_error("MEDIUM_WORKBENCH_FAILED: " + label)
