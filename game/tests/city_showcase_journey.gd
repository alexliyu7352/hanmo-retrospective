## 完整城市试玩的一次连续实际使用路径，不重复各阶段的独立验证矩阵。
extends "res://tests/test_city_showcase.gd"
const EntryScene = preload("res://demos/city_layered_showcase.tscn")
var showcase: Control
var folder: String = ""
var captures: Array = []

## 从正式独立入口场景截真实Viewport，并保存草案/候选与视图身份。
func capture(name: String) -> void:
	await process_frame; await process_frame; await RenderingServer.frame_post_draw
	var image: Image = root.get_texture().get_image()
	if image.is_empty() or image.save_png(folder.path_join(name + ".png")) != OK: printerr("SHOWCASE_CAPTURE_FAILED"); quit(1); return
	captures.append({"name": name, "section": showcase.active_section, "workshop_layout": showcase.workshop.session.layout(), "workshop_pending": showcase.workshop.session.pending(), "workshop_status": showcase.workshop.session.status(), "camera": showcase.workshop.canvas.camera_state(), "selected": showcase.workshop.selected, "overlay_visible": showcase._overlay.visible, "overlay_rect": [showcase._overlay_panel.position.x, showcase._overlay_panel.position.y, showcase._overlay_panel.size.x, showcase._overlay_panel.size.y]})
	print("CITY_SHOWCASE_CAPTURE=", name)

## 浏览到编辑、带候选切样片再返回同一确认草案，全部入口用实际鼠标点击。
func _run() -> void:
	if DisplayServer.get_name() == "headless": printerr("CITY_GRAPHICS_REQUIRED"); quit(2); return
	folder = OS.get_environment("HANMO_CITY_OUTPUT"); if folder.is_empty(): quit(2); return
	root.size = Vector2i(1280, 800); showcase = EntryScene.instantiate(); root.add_child(showcase); await process_frame
	if showcase.current == null: quit(1); return
	if "--menu-review" in OS.get_cmdline_user_args():
		await press(showcase, "ShowcaseMenu"); await capture("menu-fitted-desktop")
		root.size = Vector2i(800, 600); await capture("menu-fitted-narrow")
		await _finish(); return
	await capture("00-default-browse-entry")
	await press(showcase.workshop, "LocalView"); await capture("01-real-local")
	await press(showcase.workshop, "Place_market_grain"); await capture("02-isolated-building")
	await press(showcase.workshop, "ReturnToDistrict"); await press(showcase.workshop, "EditMode")
	await press(showcase.workshop, "Part_awning"); await press(showcase.workshop, "ConfirmEdit")
	await capture("03-confirmed-uncovered")
	var accepted: Dictionary = showcase.workshop.session.layout(); var camera: Dictionary = showcase.workshop.canvas.camera_state()
	await press(showcase.workshop, "Part_awning"); await capture("04-unconfirmed-canopy")
	await press(showcase, "ShowcaseMenu"); await capture("05-sample-menu")
	await press(showcase, "SelectComparison"); await capture("06-first-layout-comparison")
	await press(showcase.comparison, "LayoutSquare"); await capture("07-second-layout-comparison")
	await press(showcase, "ShowcaseMenu"); await press(showcase, "SelectWorkshop")
	check(showcase.workshop.session.layout() == accepted and not showcase.workshop.session.status().pending and showcase.workshop.canvas.camera_state() == camera, "连贯路径返回同一确认草案和镜头，候选已取消")
	await capture("08-return-confirmed-workshop")
	root.size = Vector2i(800, 600); await process_frame
	await press(showcase, "ShowcaseMenu"); await press(showcase, "ShowcaseHelp"); await capture("09-narrow-completion-boundary")
	await press(showcase, "CloseShowcaseOverlay"); await press(showcase.workshop, "EditMode"); await press(showcase.workshop, "OverviewView")
	await capture("10-narrow-final-browse")
	await _finish()

## 每次真实运行单独保全，菜单窄修不重放整条使用路径。
func _finish() -> void:
	var file: FileAccess = FileAccess.open(folder.path_join("showcase-journey.json"), FileAccess.WRITE)
	file.store_string(JSON.stringify({"engine": Engine.get_version_info().string, "display_server": DisplayServer.get_name(), "entry_scene": "res://demos/city_layered_showcase.tscn", "input_checks": checks, "failures": failures, "captures": captures}, "\t")); file.close()
	print("CITY_SHOWCASE_JOURNEY_COMPLETE frames=", captures.size(), " checks=", checks, " failures=", failures)
	showcase.queue_free(); await process_frame; quit(0 if failures == 0 else 1)
