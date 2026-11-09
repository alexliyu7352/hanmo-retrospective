## 同一粮铺与地块的可逆显示链，覆盖空地身份、缺失变体拒绝、外观替换和近景连续。
extends SceneTree
const Editor = preload("res://demos/city_building_state_editor.gd")
const Model = preload("res://ui/city_layout_model.gd")
var checks: int = 0
var failures: int = 0

## 场景树与真实控件就绪后开始检查。
func _init() -> void:
	call_deferred("_run")

## 程序通过仅证明状态与身份合同，美术接合另看真实图形帧。
func check(ok: bool, label: String) -> void:
	checks += 1
	if not ok: failures += 1; printerr("FAIL: ", label)

## 只比较不应因显示状态变化的身份与位置，不把资源ID误当建筑身份。
func identity(layout: Dictionary) -> Dictionary:
	for row in layout.instances:
		if row.id == "market.grain":
			return {"id": row.id, "name": row.name, "position": row.position.duplicate(), "width": row.width, "plot_id": row.plot_id}
	return {}

## 从空地到修缮均保持同一实例；每步都可以取消再确定。
func _run() -> void:
	root.size = Vector2i(1280, 800)
	var editor: Control = Editor.new(); root.add_child(editor)
	await process_frame; await process_frame
	check(editor._editor_ready, "状态工作台真实装载")
	if not editor._editor_ready: quit(1); return
	var original: Dictionary = editor.document.duplicate(true)
	var unchanged: Dictionary = identity(editor.session.layout())
	var initial_row: Dictionary = editor.canvas.find_instance("market.grain")
	for state in ["empty", "foundation", "construction", "complete", "damaged", "repair"]:
		var accepted: Dictionary = editor.session.layout()
		var before: Dictionary = editor.canvas.describe()
		var result: Dictionary = editor.preview_visual_state(state)
		check(result.ok, "已制作状态能预览 " + state)
		check(editor.session.layout() == accepted and identity(editor.session.pending()) == unchanged, "预览未先确定且同身份 " + state)
		var row: Dictionary = editor.canvas.find_instance("market.grain")
		check(row.state == state and row.foot == initial_row.foot and row.plot_id == unchanged.plot_id, "同位置和同地块 " + state)
		if state == "empty":
			check(not row.visible and not row.occupies_ground and not row.entry_active, "空地不画不存在的建筑或入口")
			var retained: bool = false
			for plot in editor.canvas.describe().plots:
				if plot.id == unchanged.plot_id: retained = plot.place_id == unchanged.id and plot.occupant_id.is_empty()
			check(retained, "空地保留原场所绑定与地块ID")
			var roof: Vector2 = Vector2(initial_row.rect[0] + initial_row.rect[2] * 0.65, initial_row.rect[1] + initial_row.rect[3] * 0.2)
			check(editor.canvas.instance_at(editor.canvas.logical_to_local(roof)).is_empty(), "空地不保留幽灵屋顶命中")
		else:
			check(row.visible and row.entry == initial_row.entry, "非空状态维持同一入口 " + state)
		if state == "repair": check(row.asset_id == "city.grainshop_construction" and row.state_note == "复用施工部件", "修缮复用明确标注")
		editor.cancel_edit()
		check(editor.session.layout() == accepted and editor.canvas.describe() == before, "取消恢复原描述 " + state)
		editor.preview_visual_state(state); editor.confirm_edit()
		check(identity(editor.session.layout()) == unchanged and editor.canvas.find_instance("market.grain").state == state, "确定保留同楼身份 " + state)
	var repair: Dictionary = editor.session.layout()
	check(not editor.preview_building_type("tavern").ok and editor.session.layout() == repair, "缺修缮素材的外观不默认为完工")
	editor.cancel_edit(); editor.preview_visual_state("complete"); editor.confirm_edit()
	var completed: Dictionary = editor.session.layout()
	check(editor.preview_building_type("inn").ok and identity(editor.session.pending()) == unchanged, "同地块外观替换保留身份")
	editor.cancel_edit(); check(editor.session.layout() == completed, "替换候选可取消")
	editor.preview_building_type("inn"); editor.confirm_edit()
	check(editor.canvas.find_instance("market.grain").archetype == "inn" and identity(editor.session.layout()) == unchanged, "替换确定仍为同一场所")
	editor.undo_edit(); check(editor.session.layout() == completed, "撤销外观替换")
	editor.redo_edit(); check(editor.canvas.find_instance("market.grain").archetype == "inn", "重做外观替换")
	check(not editor.preview_visual_state("foundation").ok and editor.canvas.find_instance("market.grain").state == "complete", "未制作基础的外观明确拒绝")
	editor.cancel_edit(); editor.undo_edit()
	var bad_document: Dictionary = editor.document.duplicate(true)
	bad_document.archetypes.grainshop.states.damaged = "city.tree"
	var damaged: Dictionary = completed.duplicate(true)
	for row in damaged.instances:
		if row.id == "market.grain": row.state = "damaged"
	check(Model.resolve(bad_document, damaged, editor.registry).get("code") == "CityStateCanvasMismatch", "错误画布尺寸变体不拉伸凑合")
	check(editor.document == original, "全链不回写作者或原资产定义")
	editor._toggle_editing(); editor.enter_place("market.grain")
	await process_frame; await process_frame
	for button in editor._toolbar.get_children():
		if button is Button and button.visible: check(button.get_global_rect().end.y <= 800, "近景切换后按钮未裁切 " + button.name)
	check(editor._status.get_global_rect().end.y <= 800, "近景说明仍在真实窗口内")
	check(not editor._road_edit.visible and not editor.find_child("ToggleStatePanel", true, false).visible, "近景收起无用编辑命令")
	root.size = Vector2i(800, 600); await process_frame; await process_frame
	check(editor._status.get_global_rect().end.y <= 600, "窄窗近景说明可见")
	editor.return_to_district(); editor._toggle_editing(); await process_frame; await process_frame
	for button in editor._toolbar.get_children():
		if button is Button and button.visible: check(button.get_global_rect().end.y <= 600, "窄窗状态工具未被裁掉 " + button.name)
	check(editor._status.get_global_rect().end.y <= 600, "返回窄窗编辑后说明可见")
	print("CITY_BUILDING_STATE_CHECKS=", checks, " FAILURES=", failures)
	editor.queue_free(); await process_frame; quit(0 if failures == 0 else 1)
