## 双族真实窗口与鼠标事件验证；检验明确切族、保选件、未适配拒绝与精确恢复。
extends SceneTree
var view: Control
var events: Array[String] = []

## 等真实图形窗口可用后启动，不接受headless截图替代品。
func _init() -> void:
	_run.call_deferred()

## 用真实鼠标输入事件走完整双族操作，随后保存原生viewport并自动释放窗口。
func _run() -> void:
	if DisplayServer.get_name() == "headless":
		push_error("双族图形验证需要真实显示。")
		quit(2)
		return
	create_timer(60.0).timeout.connect(_timeout)
	root.size = Vector2i(1440, 940)
	view = load("res://studio.tscn").instantiate()
	root.add_child(view)
	for frame in 5:
		await process_frame
	var baseline: Dictionary = view.session.state()
	await _click("Choice_eyes_1")
	await _click("Choice_nose_1")
	await _click("Choice_mouth_1")
	var semantics: Dictionary = view.session.semantic_state()
	await _click("Choice_rig_1")
	assert(view.session.semantic_state() == semantics)
	assert(view.find_child("Choice_nose_1", true, false).text == "圆鼻")
	assert(view.find_child("Choice_mouth_1", true, false).text == "含笑唇")
	assert(view.session.state().rig == 1 and view.session.state().eyes == 1 and view.session.state().nose == 1 and view.session.state().mouth == 1)
	await _capture("gui-female-base")
	for id in ["Choice_face_1", "Choice_brows_1", "Choice_hair_1", "Choice_outfit_1", "Choice_cloth_color_1", "Choice_skin_1"]:
		await _click(id)
	assert(view.find_child("Choice_beard_1", true, false).disabled)
	var chosen: Dictionary = view.session.state()
	await _capture("gui-female-dressed")
	await _click("ViewAvatar")
	await _click("BackdropGrid")
	await _capture("gui-female-avatar")
	await _click("Choice_rig_0")
	for field in chosen:
		if field != "rig":
			assert(view.session.state()[field] == chosen[field])
	await _click("Undo")
	assert(view.session.state() == chosen)
	await _click("RestoreEntry")
	assert(view.session.state() == baseline)
	await _click("Choice_beard_1")
	var bearded: Dictionary = view.session.state()
	var female_button: Button = view.find_child("Choice_rig_1", true, false)
	assert(female_button.disabled and female_button.tooltip_text.contains("胡须"))
	await _click("Choice_rig_1", true)
	assert(view.session.state() == bearded)
	await _click("ViewHalf")
	await _click("BackdropLight")
	await _capture("gui-fitting-incompatible")
	await _click("RestoreEntry")
	assert(view.session.state() == baseline)
	var result := {"ok": true, "engine": Engine.get_version_info().string, "display": DisplayServer.get_name(), "viewport": [root.size.x, root.size.y], "events": events, "input_method": "InputEventMouseButton through real X11 SceneTree viewport", "retained_on_switch": true, "semantic_ids_retained": true, "missing_part_refusal_kept_state": true, "restored": true}
	FileAccess.open("res://evidence/fitting-gui-verification.json", FileAccess.WRITE).store_string(JSON.stringify(result, "\t") + "\n")
	print("FITTING_GUI_VERIFIED " + JSON.stringify(result))
	quit()

## 先把真实控件滚入可视区，再注入按下和抬起，覆盖信号直接发射遗漏的命中行为。
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

## 从真实渲染提交后读取viewport，未生成或空图直接失败。
func _capture(id: String) -> void:
	for frame in 5:
		await process_frame
	await RenderingServer.frame_post_draw
	var image := root.get_texture().get_image()
	assert(image != null and not image.is_empty())
	assert(image.save_png("res://evidence/" + id + ".png") == OK)

## 图形断言或控件流程中断时也有界退出，避免长期占用共享验证桌面。
func _timeout() -> void:
	push_error("双族图形验证超时，未标记通过。")
	quit(2)
