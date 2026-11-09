## 可编辑街段定点回归：真实输入、草案事务、道路像素描述与地块身份，不装载世界。
extends SceneTree
const Editor = preload("res://demos/city_layout_editor.gd")
var checks: int = 0
var failures: int = 0

## 场景树就绪后运行，避免只校验字典而遗漏控件输入。
func _init() -> void:
	call_deferred("_run")

## 失败进入退出码，窗口像素由另一个真实图形进程保全。
func check(ok: bool, label: String) -> void:
	checks += 1
	if not ok: failures += 1; printerr("FAIL: ", label)

## 通过真实Viewport派发鼠标，按住屋面再拖到空地块，不直接调用会话冒充输入。
func drag(editor: Control, id: String, target: Vector2) -> void:
	var row: Dictionary = editor.canvas.find_instance(id)
	var start_logical: Vector2 = Vector2(row.rect[0] + row.rect[2] * 0.65, row.rect[1] + row.rect[3] * 0.44)
	var start: Vector2 = editor.canvas.logical_to_local(start_logical)
	var finish: Vector2 = editor.canvas.logical_to_local(start_logical + target - Vector2(row.foot[0], row.foot[1]))
	var down: InputEventMouseButton = InputEventMouseButton.new(); down.button_index = MOUSE_BUTTON_LEFT; down.pressed = true; down.position = start; down.global_position = start
	root.push_input(down, true); await process_frame
	var motion: InputEventMouseMotion = InputEventMouseMotion.new(); motion.position = finish; motion.global_position = finish; motion.button_mask = MOUSE_BUTTON_MASK_LEFT
	root.push_input(motion, true); await process_frame
	var up: InputEventMouseButton = InputEventMouseButton.new(); up.button_index = MOUSE_BUTTON_LEFT; up.pressed = false; up.position = finish; up.global_position = finish
	root.push_input(up, true); await process_frame


## 测试从同一作者地块读取目标，视觉重排不靠旧屏幕坐标猜测位置。
func anchor(editor: Control, id: String) -> Vector2:
	for plot in editor.document.layouts[0].plots:
		if plot.id == id: return Vector2(plot.anchor[0], plot.anchor[1])
	return Vector2.ZERO

## 逐项检查候选、确定、取消和历史，且旧样板与作者输入始终不变。
func _run() -> void:
	root.size = Vector2i(1280, 800)
	var editor: Control = Editor.new(); root.add_child(editor)
	await process_frame; await process_frame
	check(editor._editor_ready, "编辑入口实际装载")
	if not editor._editor_ready: printerr("EDITOR_BOOT_REASON ",editor._status.text); quit(1); return
	var baseline: Dictionary = editor.session.layout(); var original: Dictionary = editor.document.duplicate(true)
	check(editor.canvas.describe().roads.size() == 3 and editor.canvas.describe().plots.size() == 6, "三个真实道路层与六地块")
	await drag(editor, "market.grain", anchor(editor, "plot.1.1"))
	check(editor.session.status().pending and editor.session.status().valid, "真实鼠标拖放产生合法候选")
	check(editor.session.layout() == baseline and not editor.canvas._preview.is_empty(), "放下仍未确定且有真实半透明预览")
	var key: InputEventKey = InputEventKey.new(); key.keycode = KEY_ESCAPE; key.pressed = true; root.push_input(key, true); await process_frame
	check(editor.session.layout() == baseline and not editor.session.status().pending and editor.canvas._preview.is_empty(), "Esc取消恢复原草案和像素层")
	await drag(editor, "market.grain", anchor(editor, "plot.1.1")); editor.undo_edit()
	check(editor.session.layout() == baseline and not editor.session.status().pending and editor.canvas._preview.is_empty(), "没有历史时撤销也同步取消候选像素")
	await drag(editor, "market.grain", anchor(editor, "plot.1.1")); editor.confirm_edit(); await process_frame
	var changed: Dictionary = editor.session.layout()
	check(changed != baseline and editor.canvas.find_instance("market.grain").plot_id == "plot.1.1", "确定后同一建筑迁入空地块")
	var empty: bool = false
	for plot in editor.canvas.describe().plots:
		if plot.id == "plot.0.1": empty = plot.occupant_id.is_empty()
	check(empty, "原地块保持同ID并变成空地")
	editor.undo_edit(); check(editor.session.layout() == baseline, "撤销恢复原地块绑定")
	editor.redo_edit(); check(editor.session.layout() == changed, "重做恢复已确定绑定")
	editor.undo_edit()
	var invalid: Dictionary = editor.session.preview_move("market.grain", anchor(editor, "plot.0.0"))
	check(not invalid.ok, "占用地块或相交占地拒绝")
	check(not editor.session.confirm().ok and editor.session.layout() == baseline, "非法确定不改已接受草案")
	editor.cancel_edit()
	editor.preview_road_shift()
	check(editor.session.status().valid and editor.canvas._road_preview.get("id") == "road.north", "道路候选实际进入绘图层")
	check(editor.session.layout() == baseline, "道路预览未先写入草案")
	editor.confirm_edit()
	check(editor.canvas.describe().roads[0].points != baseline.roads[0].points, "确定道路偏移后实际折线已改变")
	editor.undo_edit(); check(editor.session.layout() == baseline, "撤销道路回到原几何")
	editor.selected = "market.grain"; editor.preview_removal()
	check(editor.canvas.find_instance("market.grain").is_empty() and editor.canvas.describe().plots.size() == 6, "删除预览显示空地但不删除地块")
	editor.cancel_edit(); check(not editor.canvas.find_instance("market.grain").is_empty(), "取消删除恢复建筑")
	editor._toggle_editing(); editor.enter_place("market.grain")
	check(editor.active_place == "market.grain" and editor._road_edit.disabled and editor._undo_edit.disabled, "近景不能误调用街区编辑")
	root.push_input(key, true); await process_frame
	check(editor.active_place.is_empty(), "近景Esc正常返回街区，不使标题和画布分离")
	editor._toggle_editing()
	check(editor.document == original, "全部编辑未写作者输入")
	root.size = Vector2i(800, 600); await process_frame; await process_frame
	for button in editor._toolbar.get_children():
		if button is Button and button.visible:
			check(button.get_global_rect().end.x <= 800 and button.get_global_rect().end.y <= 600, "窄窗操作仍可到达 " + button.name)
	check(editor.session.start(original, baseline, editor.registry).ok, "新会话清空旧编辑历史")
	for index in range(40):
		var result: Dictionary = editor.session.preview_move("market.grain", anchor(editor, "plot.0.1") + Vector2(index % 2, 0))
		if not result.ok or not editor.session.confirm().ok: failures += 1; printerr("FAIL: 历史压力输入未通过")
	check(editor.session.status().history_size <= 32, "编辑历史有界，不因反复拖放无限增长")
	editor.session.undo(); check(editor.session.status().redo, "撤销后存在重做分支")
	editor.session.preview_move("market.grain", anchor(editor, "plot.0.1") + Vector2(5, 0)); editor.session.confirm()
	check(not editor.session.status().redo, "新确定清空旧重做分支")
	var before_close: Dictionary = editor.session.layout()
	root.remove_child(editor); editor.confirm_edit(); editor.undo_edit()
	check(editor.session.layout() == before_close and not editor._editor_ready, "离树后的迟到按钮不改草案")
	print("CITY_LAYOUT_EDITOR_CHECKS=", checks, " FAILURES=", failures)
	editor.queue_free(); await process_frame; quit(0 if failures == 0 else 1)
