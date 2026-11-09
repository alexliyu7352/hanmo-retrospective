## 同数据跨尺度验证：只换投影、局部保留邻楼、真实命中和独立单楼往返。
extends SceneTree
const Editor = preload("res://demos/city_shared_views.gd")
var checks: int = 0
var failures: int = 0

## 等待真实控件建立后检查镜头及会话。
func _init() -> void:
	call_deferred("_run")

## 检查只证明坐标和交互合同，图像裁切和可读性另看实际运行帧。
func check(ok: bool, label: String) -> void:
	checks += 1
	if not ok: failures += 1; printerr("FAIL: ", label)

## 控件通过实际鼠标入口派发，避免只调用视图函数而遗漏点击区域。
func press(editor: Control, name: String) -> void:
	var button: BaseButton = editor.find_child(name, true, false)
	check(button != null and not button.disabled and button.visible, "控件可用 " + name)
	if button == null or button.disabled or not button.visible: return
	for pressed in [true, false]:
		var event: InputEventMouseButton = InputEventMouseButton.new(); event.button_index = MOUSE_BUTTON_LEFT; event.pressed = pressed
		event.position = button.global_position + button.size * 0.5; event.global_position = event.position
		root.push_input(event, true); await process_frame

## 视图间任何建筑、道路、部件状态都来自同一描述，只有屏幕投影改变。
func _run() -> void:
	root.size = Vector2i(1280, 800)
	var editor: Control = Editor.new(); root.add_child(editor); await process_frame; await process_frame
	check(editor._editor_ready, "共享视图工作台装载")
	if not editor._editor_ready: quit(1); return
	var accepted: Dictionary = editor.session.layout(); var description: Dictionary = editor.canvas.describe()
	var overview: Transform2D = editor.canvas.canvas_transform()
	await press(editor, "LocalView")
	var local: Transform2D = editor.canvas.canvas_transform(); var camera: Dictionary = editor.canvas.camera_state()
	check(local != overview and editor.view_mode == "local", "局部仅改变实际画布投影")
	check(editor.session.layout() == accepted and editor.canvas.describe() == description, "没有复制或移动建筑/道路/地块")
	check(editor.canvas.describe().layers.size() == 5 and editor.canvas.describe().roads.size() == 3, "局部仍保留全部邻楼及路网数据")
	var hidden_label: Button = editor.find_child("Place_market_lodging", true, false)
	check(not hidden_label.visible, "画面外客舍标签不被硬夹到屏幕边缘")
	var row: Dictionary = editor.canvas.find_instance("market.grain")
	var roof: Vector2 = Vector2(row.rect[0] + row.rect[2] * 0.65, row.rect[1] + row.rect[3] * 0.4)
	var screen: Vector2 = editor.canvas.logical_to_local(roof)
	check(editor.canvas.instance_at(screen) == "market.grain" and local.affine_inverse().basis_xform(screen - local.origin).distance_to(roof) < 0.001, "局部命中与绘制使用同一逆变换")
	editor._preview_part(false, "awning"); var candidate: Dictionary = editor.canvas.describe()
	await press(editor, "OverviewView")
	check(editor.session.status().pending and editor.canvas.describe() == candidate and editor.session.layout() == accepted, "跨视图不偷偷确定或丢弃部件候选")
	await press(editor, "LocalView"); await press(editor, "CancelEdit")
	check(editor.canvas.describe() == description and editor.canvas.camera_state() == camera, "取消恢复同一状态但保留镜头")
	editor.preview_visual_state("damaged"); await press(editor, "ConfirmEdit")
	var damaged: Dictionary = editor.canvas.describe(); var changed: Dictionary = editor.session.layout()
	await press(editor, "OverviewView")
	check(editor.canvas.describe() == damaged and editor.session.layout() == changed, "总览读取相同破损状态")
	await press(editor, "LocalView"); await press(editor, "UndoEdit")
	check(editor.canvas.describe() == description and editor.canvas.camera_state() == camera, "局部撤销恢复同一对象且不跳镜头")
	await press(editor, "EditMode"); await press(editor, "Place_market_grain")
	check(editor.active_place == "market.grain" and editor._title.text.contains("单楼展示") and not editor._views_panel.visible, "独立单楼明确区分并收起街区视图入口")
	check(editor.canvas.describe().layers.size() == 1, "已有独立单楼模式保留")
	await press(editor, "ReturnToDistrict")
	check(editor.view_mode == "local" and editor.canvas.camera_state() == camera and editor.canvas.describe() == description and editor.selected == "market.grain", "单楼返回恢复真实局部/原选择/原草案")
	await press(editor, "OverviewView")
	check(editor.canvas.canvas_transform() == overview and editor.selected == "market.grain", "返回总览变换与选中对象可预期")
	await press(editor, "EditMode"); await press(editor, "LocalView"); await press(editor, "PreviewRoad")
	check(editor.road_mode and editor.view_mode == "overview" and editor.canvas.camera_state().is_empty() and editor._local_button.disabled, "路点编辑明确回完整安全总览")
	await press(editor, "PreviewRoad"); await press(editor, "LocalView")
	root.size = Vector2i(800, 600); await process_frame; await process_frame
	check(editor.canvas.describe() == description and editor.view_mode == "local", "窄窗缩放不改布局和当前视图")
	check(editor._views_panel.get_global_rect().end.x <= 800 and editor._views_panel.get_global_rect().end.y <= 600, "窄窗视图按钮完整可达")
	check(editor.canvas.instance_at(editor.canvas.logical_to_local(roof)) == "market.grain", "窄窗局部仍命中同一屋顶")
	var behind: Dictionary = editor.canvas.find_instance("market.store")
	var behind_point: Vector2 = editor.canvas.logical_to_local(Vector2(behind.rect[0] + behind.rect[2] * 0.65, behind.rect[1] + behind.rect[3] * 0.4))
	check(editor.get_node("CityFooter").get_global_rect().has_point(behind_point) and editor.canvas.instance_at(behind_point) == "market.store", "底栏后确有可命中的邻楼像素")
	var selected_before: String = editor.selected
	for pressed in [true, false]:
		var blocked: InputEventMouseButton = InputEventMouseButton.new(); blocked.button_index = MOUSE_BUTTON_LEFT; blocked.pressed = pressed
		blocked.position = behind_point; blocked.global_position = behind_point; root.push_input(blocked, true); await process_frame
	check(editor.selected == selected_before and not editor.session.status().pending, "实际点击不穿过底栏选择或拖动后方邻楼")
	var previous: Dictionary = editor.canvas.camera_state()
	check(not editor.canvas.set_camera(Vector2(NAN, 20), 1.8) and not editor.canvas.set_camera(Vector2(400, 400), 0.5) and editor.canvas.camera_state() == previous, "坏视窗参数拒绝且不动现画面")
	print("CITY_SHARED_VIEW_CHECKS=", checks, " FAILURES=", failures)
	editor.queue_free(); await process_frame; quit(0 if failures == 0 else 1)
