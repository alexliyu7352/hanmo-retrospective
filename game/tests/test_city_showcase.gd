## 完整入口定点：默认浏览、模态输入隔离、样片切换回滚候选并保留已确认草案。
extends SceneTree
const Showcase = preload("res://demos/city_layered_showcase.gd")
var checks: int = 0
var failures: int = 0

## 入口ready之后检查真实控件和子场景生命周期。
func _init() -> void:
	call_deferred("_run")

## 定点只看入口组织合同，完整连贯路径另跑真实图形。
func check(ok: bool, label: String) -> void:
	checks += 1
	if not ok: failures += 1; printerr("FAIL: ", label)

## 通过实际Viewport点控件，模态打开后不直接越过遮挡调用子场景。
func press(host: Control, name: String) -> void:
	var button: BaseButton = host.find_child(name, true, false)
	check(button != null and button.is_visible_in_tree() and not button.disabled, "可用控件 " + name)
	if button == null or not button.is_visible_in_tree() or button.disabled: return
	for pressed in [true, false]:
		var event: InputEventMouseButton = InputEventMouseButton.new(); event.button_index = MOUSE_BUTTON_LEFT; event.pressed = pressed
		event.position = button.global_position + button.size * 0.5; event.global_position = event.position
		root.push_input(event, true); await process_frame

## 只测试一次确认和一个未确定候选；对照样片不共享或覆盖工作台草案。
func _run() -> void:
	root.size = Vector2i(1280, 800)
	var showcase: Control = Showcase.new(); root.add_child(showcase); await process_frame; await process_frame
	check(showcase.active_section == "workshop" and not showcase.workshop.canvas.editing, "默认当前街区浏览")
	check(root.title.contains("独立城市试玩"), "窗口身份清楚区别正式Main")
	await press(showcase.workshop, "EditMode")
	showcase.workshop._preview_part(false, "awning"); await press(showcase.workshop, "ConfirmEdit")
	var accepted: Dictionary = showcase.workshop.session.layout()
	showcase.workshop._preview_part(true, "awning")
	check(showcase.workshop.session.status().pending, "已确认后又有未确认候选")
	await press(showcase, "ShowcaseMenu")
	await process_frame; await process_frame
	check(showcase._overlay_panel.get_global_rect().end.y <= 800 and showcase._overlay_panel.size.y < 600, "首开菜单按实际内容收高，无越界空白")
	check(showcase._overlay.visible and showcase.workshop.process_mode == Node.PROCESS_MODE_DISABLED, "菜单是真模态，背景不处理输入")
	var escape: InputEventKey = InputEventKey.new(); escape.keycode = KEY_ESCAPE; escape.pressed = true
	root.push_input(escape, true); await process_frame
	check(not showcase._overlay.visible and showcase.workshop.session.status().pending, "关菜单Esc不穿到背景取消候选")
	await press(showcase, "ShowcaseMenu"); await press(showcase, "SelectComparison")
	check(showcase.active_section == "comparison" and not showcase.workshop.visible and showcase.comparison.visible, "切到真实两布局样片")
	check(showcase.workshop.session.layout() == accepted and not showcase.workshop.session.status().pending, "切样片只取消候选，已确定保留")
	await press(showcase.comparison, "LayoutSquare")
	check(showcase.comparison.layout_index == 1 and showcase.workshop.session.layout() == accepted, "布局对照不覆盖工作台")
	await press(showcase, "ShowcaseMenu"); await press(showcase, "SelectWorkshop")
	check(showcase.current == showcase.workshop and showcase.workshop.session.layout() == accepted and showcase.workshop.canvas.editing, "返回同一工作台实例和已确认草案")
	root.size = Vector2i(800, 600); await process_frame; await process_frame
	await press(showcase, "ShowcaseMenu"); await press(showcase, "ShowcaseHelp"); await process_frame; await process_frame
	check(showcase._overlay_panel.get_global_rect().position.y >= 0 and showcase._overlay_panel.get_global_rect().end.y <= 600 and showcase._overlay_panel.get_global_rect().end.x <= 800, "窄窗完成边界与关闭按钮完整可见")
	await press(showcase, "CloseShowcaseOverlay")
	check(not showcase._overlay.visible and showcase.workshop.process_mode == Node.PROCESS_MODE_INHERIT, "关闭说明后当前场景恢复输入")
	print("CITY_SHOWCASE_CHECKS=", checks, " FAILURES=", failures)
	showcase.queue_free(); await process_frame; quit(0 if failures == 0 else 1)
