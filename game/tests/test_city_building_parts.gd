## 布棚独立层验证：同父楼体、关闭后净楼体、状态兼容、撤销和实际alpha命中。
extends SceneTree
const Editor = preload("res://demos/city_building_parts_editor.gd")
var checks: int = 0
var failures: int = 0

## 场景初始化后读取真实部件和控件。
func _init() -> void:
	call_deferred("_run")

## 定点失败进入退出码，画面自然接合仍由实际大图审查。
func check(ok: bool, label: String) -> void:
	checks += 1
	if not ok: failures += 1; printerr("FAIL: ", label)

## 按同一实例查部件描述，不把另一个建筑的纹理误算成装饰。
func part(row: Dictionary, id: String) -> Dictionary:
	for item in row.parts:
		if item.id == id: return item
	return {}

## 部件配置与状态共用同一历史，所有像素命中仍返回粮铺实例。
func _run() -> void:
	root.size = Vector2i(1280, 800)
	var editor: Control = Editor.new(); root.add_child(editor); await process_frame; await process_frame
	check(editor._editor_ready, "门面工作台装载")
	if not editor._editor_ready: quit(1); return
	check(editor.find_child("Part_awning", true, false).get_theme_color("font_color").get_luminance() < 0.2, "纸色底部件文字使用深色")
	check(editor.find_child("Part_awning", true, false).get_theme_color("font_disabled_color").get_luminance() < 0.45, "未适配部件提示仍有可读对比")
	var original: Dictionary = editor.document.duplicate(true); var before: Dictionary = editor.session.layout()
	var row: Dictionary = editor.canvas.find_instance("market.grain")
	check(row.parts.size() == 2 and part(row, "body").asset_id == "city.grainshop_uncovered", "主体使用真正净楼体")
	check(part(row, "awning").visible and part(row, "awning").asset_id == "city.grainshop_awning", "布棚是不同透明资源")
	editor._preview_part(false, "awning")
	check(not part(editor.canvas.find_instance("market.grain"), "awning").visible and editor.session.layout() == before, "开关仅预览，不先确定")
	editor.cancel_edit(); check(editor.canvas.find_instance("market.grain") == row, "取消恢复原部件和主体")
	editor._preview_part(false, "awning"); editor.confirm_edit()
	var uncovered: Dictionary = editor.canvas.find_instance("market.grain")
	check(part(uncovered, "body") == part(row, "body") and not part(uncovered, "awning").visible, "关闭只移除布棚，不替换整栋效果图")
	editor.undo_edit(); check(part(editor.canvas.find_instance("market.grain"), "awning").visible, "撤销恢复独立布棚")
	editor.redo_edit(); check(not part(editor.canvas.find_instance("market.grain"), "awning").visible, "重做恢复净楼体")
	editor.undo_edit()
	for state in ["empty", "foundation", "construction", "damaged", "repair"]:
		editor.preview_visual_state(state); editor.confirm_edit(); await process_frame
		var changed: Dictionary = editor.canvas.find_instance("market.grain")
		check(changed.parts.size() == 1 and not changed.attachment_options[0].supported, "未适配状态不叠完整布棚 " + state)
		check(editor.find_child("Part_awning", true, false).disabled and not editor.session.preview_attachment("market.grain", "awning", true).ok, "未适配状态界面与会话都拒绝 " + state)
	editor.preview_visual_state("complete"); editor.confirm_edit()
	check(part(editor.canvas.find_instance("market.grain"), "awning").visible, "回到完工按原选择恢复布棚")
	var target: Vector2 = Vector2.ZERO
	for plot in editor.layout.plots:
		if plot.id == "plot.1.1": target = Vector2(plot.anchor[0], plot.anchor[1])
	var moved: Dictionary = editor.session.preview_move("market.grain", target)
	check(moved.ok, "带部件同楼可移到空地块")
	editor.confirm_edit(); var shifted: Dictionary = editor.canvas.find_instance("market.grain")
	var delta: Vector2 = Vector2(shifted.foot[0] - row.foot[0], shifted.foot[1] - row.foot[1])
	for id in ["body", "awning"]:
		var a: Dictionary = part(row, id); var b: Dictionary = part(shifted, id)
		check(Vector2(b.rect[0] - a.rect[0], b.rect[1] - a.rect[1]).distance_to(delta) < 0.01, "整体移位同一个变换 " + id)
	editor.undo_edit()
	var awning: Dictionary = part(row, "awning"); var image: Image = editor.registry.texture("backgrounds", awning.asset_id).get_image()
	if image.is_compressed(): image.decompress()
	var source_pixel: Vector2 = Vector2(image.get_width() * 0.5, image.get_height() * 0.33)
	var point: Vector2 = Vector2(awning.rect[0], awning.rect[1]) + source_pixel / Vector2(image.get_size()) * Vector2(awning.rect[2], awning.rect[3])
	check(image.get_pixelv(Vector2i(source_pixel)).a > 0.7 and editor.canvas.instance_at(editor.canvas.logical_to_local(point)) == "market.grain", "布棚实心像素命中同一粮铺")
	check(image.get_pixel(image.get_width() / 2, image.get_height() * 3 / 4).a < 0.05, "两支杆之间是真透明")
	editor._preview_part(false, "awning"); editor.confirm_edit(); editor.preview_building_type("inn")
	check(editor.session.status().valid and not editor.session.pending().instances[1].has("attachments"), "换型重设不兼容部件配置，仍可取消")
	editor.cancel_edit(); root.size = Vector2i(800, 600); await process_frame; await process_frame
	check(editor._state_panel.get_global_rect().end.y <= editor.get_node("CityFooter").get_global_rect().position.y, "窄窗部件页不压底栏")
	editor._part_tabs.current_tab = 0; await process_frame; await process_frame
	check(editor._state_panel.get_global_rect().end.y <= editor.get_node("CityFooter").get_global_rect().position.y, "窄窗状态页也不压底栏")
	check(editor.document == original, "操作不回写作者素材定义")
	print("CITY_BUILDING_PART_CHECKS=", checks, " FAILURES=", failures)
	editor.queue_free(); await process_frame; quit(0 if failures == 0 else 1)
