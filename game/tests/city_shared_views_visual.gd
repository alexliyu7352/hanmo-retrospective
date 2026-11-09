## 跨尺度视窗真实图形：同一草案在总览、局部与独立单楼之间往返，不替换效果图。
extends "res://tests/test_city_shared_views.gd"
var editor: Control
var folder: String = ""
var captures: Array = []

## 实际帧保存同一份布局与解析描述，同时记录单独的镜头意图。
func capture(name: String) -> void:
	await process_frame; await process_frame; await RenderingServer.frame_post_draw
	var image: Image = root.get_texture().get_image()
	if image.is_empty() or image.save_png(folder.path_join(name + ".png")) != OK: printerr("VIEW_CAPTURE_FAILED"); quit(1); return
	captures.append({"name": name, "layout": editor.session.layout(), "descriptor": editor.canvas.describe(), "camera": editor.canvas.camera_state(), "selected": editor.selected, "view_mode": editor.view_mode, "active_place": editor.active_place, "session": editor.session.status()})
	print("CITY_SHARED_VIEW_CAPTURE=", name)

## 视图入口和场所按钮走实际鼠标，跨尺度后候选、返回和撤销仍属同一会话。
func _run() -> void:
	if DisplayServer.get_name() == "headless": printerr("CITY_GRAPHICS_REQUIRED"); quit(2); return
	folder = OS.get_environment("HANMO_CITY_OUTPUT"); if folder.is_empty(): quit(2); return
	root.size = Vector2i(1280, 800); editor = Editor.new(); root.add_child(editor); await process_frame
	if not editor._editor_ready: quit(1); return
	await capture("00-district-overview")
	await press(editor, "LocalView"); await capture("01-real-local-neighbours")
	await press(editor, "ToggleStatePanel"); await press(editor, "Part_awning"); await capture("02-local-attachment-preview")
	await press(editor, "OverviewView"); await capture("03-overview-same-preview")
	await press(editor, "CancelEdit"); await press(editor, "ToggleStatePanel"); await press(editor, "LocalView")
	await press(editor, "EditMode"); await press(editor, "Place_market_grain"); await capture("04-isolated-building-mode")
	await press(editor, "ReturnToDistrict"); await capture("05-return-to-same-local")
	await press(editor, "EditMode"); editor.preview_visual_state("damaged"); await press(editor, "ConfirmEdit")
	await press(editor, "ToggleStatePanel"); await capture("06-local-same-damaged-building")
	await press(editor, "OverviewView"); await capture("07-overview-same-damaged-building")
	await press(editor, "UndoEdit"); await press(editor, "LocalView"); root.size = Vector2i(800, 600); await capture("08-narrow-local-view")
	var file: FileAccess = FileAccess.open(folder.path_join("shared-visual-report.json"), FileAccess.WRITE)
	file.store_string(JSON.stringify({"engine": Engine.get_version_info().string, "display_server": DisplayServer.get_name(), "captures": captures}, "\t")); file.close()
	print("CITY_SHARED_VIEW_VISUAL_COMPLETE captures=", captures.size()); editor.queue_free(); await process_frame; quit(0)
