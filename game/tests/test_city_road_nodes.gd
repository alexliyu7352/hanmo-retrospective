## 有限路点模式定点验证：真实拖放、共用连接、候选拒绝及现有建筑状态编辑的边界。
extends SceneTree
const Editor = preload("res://demos/city_road_node_editor.gd")
const Model = preload("res://ui/city_layout_model.gd")
var checks: int = 0
var failures: int = 0

## 场景就绪后验证真实控件，不以字典单测替代拖动入口。
func _init() -> void:
	call_deferred("_run")

## 每项失败进入退出码，实际纹理和底栏仍另行审图。
func check(ok: bool, label: String) -> void:
	checks += 1
	if not ok: failures += 1; printerr("FAIL: ", label)

## 使用同一画布变换派发Viewport鼠标，覆盖压住、移动、放下整条链。
func drag(editor: Control, id: String, position: Vector2) -> void:
	var start: Vector2 = editor.road_handles.project(editor.road_handles.nodes[id].position)
	var finish: Vector2 = editor.road_handles.project(position)
	var down: InputEventMouseButton = InputEventMouseButton.new(); down.button_index = MOUSE_BUTTON_LEFT; down.pressed = true; down.position = start; down.global_position = start
	root.push_input(down, true); await process_frame
	var motion: InputEventMouseMotion = InputEventMouseMotion.new(); motion.position = finish; motion.global_position = finish; motion.button_mask = MOUSE_BUTTON_MASK_LEFT
	root.push_input(motion, true); await process_frame
	var up: InputEventMouseButton = InputEventMouseButton.new(); up.button_index = MOUSE_BUTTON_LEFT; up.position = finish; up.global_position = finish
	root.push_input(up, true); await process_frame

## 对同一节点每个引用都取值，避免只验证画出来的一个手柄。
func positions(layout: Dictionary, id: String) -> Array:
	var result: Array = []
	for road in layout.roads:
		for index in range(road.node_ids.size()):
			if road.node_ids[index] == id: result.append(road.points[index])
	return result

## 路点可以改变真实路面和入口连接，但场所身份与地块必须始终不变。
func _run() -> void:
	root.size = Vector2i(1280, 800)
	var editor: Control = Editor.new(); root.add_child(editor); await process_frame; await process_frame
	check(editor._editor_ready, "路点入口装载")
	if not editor._editor_ready: quit(1); return
	var original: Dictionary = editor.document.duplicate(true); var baseline: Dictionary = editor.session.layout()
	var geometry: Dictionary = editor.canvas.describe()
	editor.toggle_road_mode(); await process_frame
	check(editor.road_mode and editor.road_handles.visible and not editor._state_panel.visible, "路点模式独占画布，状态面板收起")
	check(editor.road_handles.nodes.size() == 8 and editor.road_handles.nodes["junction.north"].uses == 2, "共有节点合并成单手柄")
	await drag(editor, "north.middle", Vector2(850, 459))
	check(editor.session.status().valid and editor.session.status().pending, "实际路点拖动产生合法候选")
	check(editor.session.layout() == baseline and editor.canvas._road_preview.has("roads"), "真实候选未先写入草案")
	check(editor.session.pending().instances == baseline.instances and editor.session.pending().plots == baseline.plots, "只改道路，不动建筑或地块")
	check(editor.canvas._road_preview.access != geometry.access, "门前连接随候选重新求交")
	editor.cancel_edit()
	check(editor.session.layout() == baseline and editor.canvas.describe() == geometry and editor.canvas._road_preview.is_empty(), "取消恢复原描述和路面")
	await drag(editor, "junction.north", Vector2(1540, 681))
	check(editor.session.status().valid, "共用连接点拖动合法")
	var shared: Array = positions(editor.session.pending(), "junction.north")
	check(shared.size() == 2 and shared[0] == shared[1] and absf(shared[0][0] - 1540) < 0.01, "两个相连路段保持同一个实际位置")
	editor.confirm_edit(); var changed: Dictionary = editor.session.layout()
	check(changed != baseline and positions(changed, "junction.north") == shared, "确定后两条真实路同步改变")
	editor.undo_edit(); check(editor.session.layout() == baseline, "撤销恢复整个连接")
	editor.redo_edit(); check(editor.session.layout() == changed, "重做同一连接")
	editor.undo_edit()
	await drag(editor, "north.middle", Vector2(775, 699))
	check(not editor.session.status().valid and editor.session.status().code == "CityUnboundRoadCrossing", "不自动补造未声明交叉口")
	check(editor._confirm_edit.disabled and editor.session.layout() == baseline, "非法候选不能确定且草案不变")
	var escape: InputEventKey = InputEventKey.new(); escape.keycode = KEY_ESCAPE; escape.pressed = true; root.push_input(escape, true); await process_frame
	check(not editor.session.status().pending and editor.canvas._road_preview.is_empty(), "Esc清除拒绝候选")
	check(not editor.session.preview_road_node("north.middle", Vector2(-1, 400)).ok, "越界路点拒绝")
	editor.cancel_edit()
	check(not editor.session.preview_road_node("north.middle", Vector2(1570, 690)).ok, "重合短段拒绝")
	editor.cancel_edit()
	check(not editor.session.preview_road_node("north.middle", Vector2(970, 440)).ok, "道路侵入建筑占地拒绝")
	editor.cancel_edit()
	var malformed: Dictionary = baseline.duplicate(true); malformed.roads[2].points[0][0] -= 1
	check(Model.resolve(editor.document, malformed, editor.registry).code == "CityRoadJunctionSplit", "坏作者连接不静默修复")
	root.size = Vector2i(800, 600); await process_frame; await process_frame
	await drag(editor, "junction.south", Vector2(1220, 832.5))
	check(editor.session.status().valid, "窄窗同一44像素命中仍可拖动")
	for id in editor.road_handles.nodes:
		var point: Vector2 = editor.road_handles.project(editor.road_handles.nodes[id].position)
		check(point.y - 22 >= editor.get_node("CityHeader").get_global_rect().end.y and point.y + 22 <= editor.get_node("CityFooter").get_global_rect().position.y and point.x >= 22 and point.x + 22 <= root.size.x, "整个路点命中区不落在栏后 " + id)
	editor.cancel_edit()
	for button in editor._toolbar.get_children():
		if button is Button and button.visible: check(button.get_global_rect().end.y <= 600, "路点模式窄窗按钮可达 " + button.name)
	check(editor._status.get_global_rect().end.y <= 600, "路点提示在窗内")
	editor.toggle_road_mode(); check(not editor.road_handles.visible and editor._state_panel.visible, "退出改路恢复状态编辑")
	check(editor.preview_visual_state("damaged").ok, "相邻状态链仍可预览")
	editor.cancel_edit(); editor.toggle_road_mode(); editor._toggle_editing()
	check(not editor.road_handles.visible and not editor.road_mode and not canvas_editing(editor), "浏览模式没有残留路点操作层")
	check(editor.document == original, "路点全过程不回写作者数据")
	print("CITY_ROAD_NODE_CHECKS=", checks, " FAILURES=", failures)
	editor.queue_free(); await process_frame; quit(0 if failures == 0 else 1)

## 只读检查画布模式，避免复核时触发任何模式切换。
func canvas_editing(editor: Control) -> bool:
	return editor.canvas.editing
