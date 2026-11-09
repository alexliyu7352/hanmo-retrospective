## 偏长族四结构工作台操作与三族语意保留，技术通过不代替96像素身份辨识。
extends SceneTree
var checks := 0
var failures: Array[String] = []

## 延后建立真实场景，限定本地临时会话而不接主游戏或存档。
func _init() -> void:
	_run.call_deferred()

## 逐项操作新结构，覆盖跨族撤销、真实缺须拒绝、窄窗边界与原像素恢复。
func _run() -> void:
	root.size = Vector2i(1280, 900)
	root.content_scale_size = Vector2i(1280, 900)
	var view = load("res://studio.tscn").instantiate()
	root.add_child(view)
	for frame in 5:
		await process_frame
	var entry: Dictionary = view.session.state()
	var entry_pixels: PackedByteArray = view.portrait.get_data()
	_press(view, "Choice_rig_2")
	_check(view.session.field_definition("face").options.size() == 4, "偏长族四结构登记")
	for field in ["brows", "eyes", "nose", "mouth"]:
		_press(view, "Choice_" + field + "_1")
	for face in 4:
		_press(view, "Choice_face_" + str(face))
		_check(view.session.state().face == face and view.session.state().rig == 2, "偏长结构实际可选")
		for field in ["brows", "eyes", "nose", "mouth"]:
			_check(view.session.state()[field] == 1, "换骨相保持独立" + field)
	var kept: Dictionary = view.session.state()
	var kept_pixels: PackedByteArray = view.portrait.get_data()
	var semantics: Dictionary = view.session.semantic_state()
	for rig in [0, 1]:
		_press(view, "Choice_rig_" + str(rig))
		_check(view.session.state().face == 3 and view.session.semantic_state() == semantics, "三族第4结构语意完整保留")
		_press(view, "Undo")
		_check(view.session.state() == kept and view.portrait.get_data() == kept_pixels, "撤销切族恢复偏长实际像素")
	_press(view, "Choice_face_0")
	_press(view, "Choice_mouth_0")
	_press(view, "Choice_beard_1")
	var bearded: Dictionary = view.session.state()
	var bearded_pixels: PackedByteArray = view.portrait.get_data()
	var history: Dictionary = view.session.history()
	var missing: Button = view.find_child("Choice_rig_1", true, false)
	_check(missing.disabled and missing.tooltip_text.contains("胡须"), "女性族缺短须明确禁用")
	missing.pressed.emit()
	_check(view.session.state() == bearded and view.portrait.get_data() == bearded_pixels and view.session.history() == history, "缺件拒绝保持状态像素历史")
	_press(view, "Choice_beard_0")
	for frame in 5:
		await process_frame
	var scroll: ScrollContainer = view.find_child("ChoiceScroll", true, false)
	for face in 4:
		var button: Button = view.find_child("Choice_face_" + str(face), true, false)
		_check(not button.disabled and button.get_global_rect().end.x <= scroll.get_global_rect().end.x, "四结构窄窗可用且不溢出")
	_press(view, "RestoreEntry")
	_check(view.session.state() == entry and view.portrait.get_data() == entry_pixels, "入口原像素恢复")
	var report := {"ok": failures.is_empty(), "checks": checks, "failures": failures, "viewport": [1280, 900], "scope": "four elongated structural options, independent choices and cross-family semantics; visual identity acceptance is separate"}
	FileAccess.open("res://evidence/long-workbench-tests.json", FileAccess.WRITE).store_string(JSON.stringify(report, "\t") + "\n")
	print(JSON.stringify(report))
	view.queue_free()
	await process_frame
	quit(0 if failures.is_empty() else 1)

## 用原控件的会话入口执行，不直写内部人物草案。
func _press(view: Node, id: String) -> void:
	var button: Button = view.find_child(id, true, false)
	_check(button != null and not button.disabled, "控件可操作 " + id)
	if button != null and not button.disabled:
		button.pressed.emit()

## 汇总全部局部失败，最终非零退出防止仅凭部分结果误报通过。
func _check(condition: bool, label: String) -> void:
	checks += 1
	if not condition:
		failures.append(label)
		push_error("LONG_WORKBENCH_FAILED: " + label)
