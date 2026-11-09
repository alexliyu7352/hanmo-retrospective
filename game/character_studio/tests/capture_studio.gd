## 真实图形后端工坊验证；只在官方桌面图形slot运行，输出原生窗口像素与交互回执。
extends SceneTree
var view: Control
var events: Array[String] = []

## 等图形场景树建立后测试，不能以headless空纹理冒充截图。
func _init() -> void:
	_run.call_deferred()

## 使用真实场景控件完成选件、恢复与头像检查，然后自动退出释放窗口。
func _run() -> void:
	if DisplayServer.get_name() == "headless":
		push_error("图形验证需要真实显示后端。")
		quit(2)
		return
	root.size = Vector2i(1440, 940)
	view = load("res://studio.tscn").instantiate()
	root.add_child(view)
	await _capture("gui-default")
	var baseline: Dictionary = view.session.state()
	for pair in [["eyes", 1], ["brows", 1], ["nose", 1], ["mouth", 1], ["face", 1], ["outfit", 1], ["hair", 1], ["cloth_color", 2], ["skin", 1]]:
		var button: Button = view.find_child("Choice_" + pair[0] + "_" + str(pair[1]), true, false)
		assert(button != null and not button.disabled)
		button.pressed.emit()
		events.append("choose:" + pair[0] + "=" + str(pair[1]))
	await _capture("gui-dressed")
	view.find_child("ViewAvatar", true, false).pressed.emit()
	view.find_child("BackdropGrid", true, false).pressed.emit()
	view.find_child("ChoiceScroll", true, false).scroll_vertical = 9999
	await _capture("gui-avatar-colors")
	view.find_child("RestoreEntry", true, false).pressed.emit()
	assert(view.session.state() == baseline)
	var result := {"ok": true, "engine": Engine.get_version_info().string, "display": DisplayServer.get_name(), "viewport": [root.size.x, root.size.y], "events": events, "restored": true, "screenshots": ["gui-default.png", "gui-dressed.png", "gui-avatar-colors.png"]}
	var file := FileAccess.open("res://evidence/gui-verification.json", FileAccess.WRITE)
	file.store_string(JSON.stringify(result, "\t") + "\n")
	print("PORTRAIT_GUI_VERIFIED " + JSON.stringify(result))
	quit()

## 等真实渲染提交后保存原始viewport，既不重画UI也不拼接代用截图。
func _capture(id: String) -> void:
	for frame in 5:
		await process_frame
	await RenderingServer.frame_post_draw
	var image := root.get_texture().get_image()
	assert(image != null and not image.is_empty())
	assert(image.save_png("res://evidence/" + id + ".png") == OK)
