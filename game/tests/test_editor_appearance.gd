## 真实作者进程与面板验证预览、明确固定、版本守卫、撤销及保存重开；不修改正式作者源。
extends "res://tests/test_editor_panel.gd"
const Art = preload("res://ui/art_catalog.gd")
const Strict = preload("res://core/strict_json.gd")
var panel: Control
var original_manifest: Dictionary

## 从真实下拉选项选择已经制作的脸型，并保留源定义到明确固定动作。
func choose_face(value: String) -> void:
	var control: OptionButton = panel._appearance_picker._choices.face_shape
	for index in range(control.item_count):
		if control.get_item_metadata(index) == value:
			check(not control.disabled and not control.is_item_disabled(index), "作者草案真实选项可用")
			control.select(index); control.item_selected.emit(index); return
	check(false, "作者草案选项存在")

## 在隔离工程上完成所有作者写入，并通过正式prepare与Kernel恢复验证实际保存结果。
func _run() -> void:
	original_manifest = Art.catalog().describe()
	var folder: String = OS.get_user_data_dir().path_join("appearance-editor-" + str(Time.get_ticks_usec()))
	var project: String = folder.path_join("project")
	DirAccess.make_dir_recursive_absolute(folder)
	var root_path: String = ProjectSettings.globalize_path("res://").trim_suffix("/").get_base_dir()
	var output: Array = []
	var python: String = OS.get_environment("HANMO_PYTHON") if OS.has_environment("HANMO_PYTHON") else "python3"
	var created: int = OS.execute(python, PackedStringArray([root_path.path_join("tests/fixtures/appearance/editor_project.py"), project]), output, true)
	check(created == 0, "建立真实隔离外貌作者工程 " + str(output))
	if created != 0: quit(1); return
	var manifest: Dictionary = Strict.parse_text(FileAccess.get_file_as_string(project.path_join("game/data/presentation.json"))).value
	check(Art.catalog().configure(manifest).ok, "作者与预览加载同一隔离表现清单")
	root.size = Vector2i(1440, 900)
	panel = load("res://addons/hanmo_author/panel.gd").new(); root.add_child(panel)
	panel.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	await process_frame
	panel.root_path = project; panel.path_input.text = project.path_join("content/foundation/lived_city.json")
	await press(panel, "connect")
	if panel.pack.is_empty(): check(false, "作者源成功加载"); await finish(); return
	await select_record(panel, "actors", "actor.he")
	var source_before: Dictionary = panel.pack.duplicate(true)
	var revision: Variant = panel.client.document.revision
	await press(panel, "appearance_preview")
	check(is_instance_valid(panel._appearance_picker), "作者外貌入口打开共享picker")
	if not is_instance_valid(panel._appearance_picker): await finish(); return
	choose_face("angular")
	check(panel.pack == source_before and panel.client.document.revision == revision and not panel.client.document.dirty, "选件不写作者源或撤销历史")
	var stale: Control = panel._appearance_picker
	panel._appearance_picker.cancel_selection()
	stale.confirmed.emit({"base_features": {}, "attire_style": "plain"})
	await settle(panel)
	check(panel.pack == source_before and panel._appearance_window == null, "取消及迟到确认不写源并关闭窗口")
	await press(panel, "appearance_preview")
	choose_face("angular"); panel._appearance_picker.confirm_selection(); await settle(panel)
	check(panel.pack == source_before and record(panel, "actors", "actor.he").appearance_definition.kind == "generated", "普通确认只结束预览，不把generated自动改fixed")
	await press(panel, "appearance_preview")
	choose_face("angular")
	var chosen: Dictionary = panel._appearance_picker.draft()
	stale = panel._appearance_picker
	var fixed_button: Button = button(panel, "appearance_fix")
	check(fixed_button != null and not fixed_button.disabled, "明确固定动作可用")
	fixed_button.pressed.emit(); fixed_button.pressed.emit(); stale.confirmed.emit(chosen)
	await settle(panel)
	var fixed: Dictionary = record(panel, "actors", "actor.he").appearance_definition
	check(fixed.kind == "fixed" and fixed.base_features == chosen.base_features and fixed.attire_style == "plain", "只有明确固定动作经set_value写实际选择")
	check(panel.client.document.revision == revision + 1, "重复固定与旧信号只有一次作者事务")
	await press(panel, "undo")
	check(panel.pack == source_before, "一次撤销完整恢复候选生成定义")
	await press(panel, "redo")
	check(record(panel, "actors", "actor.he").appearance_definition == fixed, "一次重做恢复实际固定组合")
	await press(panel, "appearance_preview")
	check(panel._appearance_picker._choices.face_shape.disabled and button(panel, "appearance_fix") == null, "fixed作者定义预览不虚构可选候选或重复固定按钮")
	panel._appearance_window.close_requested.emit(); await settle(panel)
	check(record(panel, "actors", "actor.he").appearance_definition == fixed, "窗口关闭不改作者定义")
	await select_record(panel, "actors", "actor.chen")
	await press(panel, "appearance_preview")
	check(panel._appearance_picker._choices.face_shape.disabled and button(panel, "appearance_fix") == null, "历史固定人物没有解锁入口")
	var late_confirmation: Callable = panel._confirm_appearance.bind(panel._appearance_generation)
	await select_record(panel, "actors", "actor.he")
	late_confirmation.call(chosen); await settle(panel)
	check(panel._appearance_window == null and record(panel, "actors", "actor.he").appearance_definition == fixed, "换人物后旧确认不写新目标")
	await press(panel, "save_as")
	var saved: String = folder.path_join("appearance-saved.json")
	for child in panel.get_children():
		if child is FileDialog: child.file_selected.emit(saved); break
	await settle(panel)
	check(FileAccess.file_exists(saved) and not panel.client.document.dirty, "外貌作者源实际保存")
	panel.path_input.text = saved; await press(panel, "open")
	check(record(panel, "actors", "actor.he").appearance_definition == fixed, "重开真实文件保留完整实际组合")
	panel.prepared_path = folder.path_join("appearance.hmcatalog.json")
	await press(panel, "prepare")
	check(panel.client.last_reply.ok and FileAccess.file_exists(panel.prepared_path), "同一作者准备器接受保存的固定定义")
	var kernel = load("res://core/world_kernel.gd").new()
	check(kernel.new_game("scenario.lived_city", panel.prepared_path).ok, "正式Kernel载入作者实际固定选择")
	if not kernel.snapshot().is_empty():
		var actual: Dictionary = kernel.snapshot().actors["actor.he"].appearance
		check(actual.base_features == chosen.base_features and actual.source == "fixed", "作者fixed不会在运行时重抽")
	await select_record(panel, "actors", "actor.he")
	await press(panel, "appearance_preview")
	check(panel._appearance_picker.draft() == chosen, "保存重开作者预览不重新生成外貌")
	panel._appearance_picker.close()
	await finish()

## 释放作者窗口和进程并恢复本进程清单；隔离文件不作为生产登记。
func finish() -> void:
	if is_instance_valid(panel): panel.queue_free(); await process_frame
	check(Art.catalog().configure(original_manifest).ok, "恢复原进程表现清单")
	print("EDITOR_APPEARANCE_CHECKS=", checks, " FAILURES=", failures)
	quit(0 if failures == 0 else 1)
