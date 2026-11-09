## 复用真实编辑面板操作助手，验证新表现ID贯通选择、保存与游戏装载。
extends "res://tests/test_editor_panel.gd"
## 真实面板与独立作者进程读取同一隔离工程，不靠内存别名绕过正式prepare。
func _run() -> void:
	var art = load("res://ui/art_catalog.gd")
	var initial: Dictionary = art.catalog().describe()
	var folder: String = OS.get_user_data_dir().path_join("presentation-check-" + str(Time.get_ticks_usec()))
	var project: String = folder.path_join("project")
	DirAccess.make_dir_recursive_absolute(folder)
	var original_root: String = ProjectSettings.globalize_path("res://").trim_suffix("/").get_base_dir()
	var output: Array = []
	var python: String = OS.get_environment("HANMO_PYTHON") if OS.has_environment("HANMO_PYTHON") else "python3"
	var created: int = OS.execute(python, PackedStringArray([original_root.path_join("tests/fixtures/presentation/editor_project.py"), project]), output, true)
	check(created == 0, "建立真实隔离作者工程 " + str(output))
	if created != 0: quit(1); return
	var extended: Dictionary = load("res://core/strict_json.gd").parse_text(FileAccess.get_file_as_string(project.path_join("game/data/presentation.json"))).value
	check(art.catalog().configure(extended).ok, "注册新场所和人物资源")
	root.size = Vector2i(1440, 900)
	var panel = load("res://addons/hanmo_author/panel.gd").new(); root.add_child(panel)
	panel.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	await process_frame
	panel.root_path = project
	panel.path_input.text = panel.root_path.path_join("content/foundation/lived_city.json")
	await press(panel, "connect")
	check(not panel.pack.is_empty(), "载入现有可玩内容")
	if panel.pack.is_empty(): panel.queue_free(); await process_frame; quit(1); return
	for spec in [["places", "west.office", "/visual_tag", "study"], ["actors", "actor.chen", "/portrait_kind", "scholar"], ["actors", "actor.chen", "/presentation_profile_id", "fixture.person"], ["actors", "actor.chen", "/presentation_profile_id", "fixture.second"], ["places", "west.office", "/scene_template_id", "fixture.room"], ["places", "west.office", "/scene_template_id", "fixture.city"]]:
		await select_record(panel, spec[0], spec[1]); await select_field(panel, spec[2])
		check(panel.reference_picker.visible, "表现字段使用资源引用选择器")
		var found: bool = false
		for i in range(panel.reference_picker.item_count):
			if panel.reference_picker.get_item_metadata(i) == spec[3]:
				panel.reference_picker.select(i); found = true
		check(found, "编辑器枚举新资源：" + spec[3])
		if not found: panel.queue_free(); await process_frame; quit(1); return
		await press(panel, "apply")
	await press(panel, "save_as")
	for child in panel.get_children():
		if child is FileDialog: child.file_selected.emit(folder.path_join("edited.json")); break
	await settle(panel)
	check(not panel.client.document.dirty, "编辑内容已真实保存")
	panel.prepared_path = folder.path_join("edited.hmcatalog.json")
	await press(panel, "prepare")
	var kernel = load("res://core/world_kernel.gd").new()
	check(kernel.new_game("scenario.lived_city", panel.prepared_path).ok, "原运行核心载入新ID内容")
	check(kernel.content().data.places["west.office"].visual_tag == "study", "新场所表现ID保留")
	check(kernel.content().data.actors["actor.chen"].portrait_kind == "scholar", "新人物表现ID保留")
	check(art.catalog().validate_content(kernel.content().data).ok, "编辑结果具备完整资源引用")
	check(kernel.content().data.actors["actor.chen"].presentation_profile_id == "fixture.second", "新方案引用保留")
	check(kernel.content().data.places["west.office"].scene_template_id == "fixture.city", "新场景引用保留")
	var baseline = load("res://core/world_kernel.gd").new()
	check(baseline.new_game("scenario.lived_city", "res://data/generated/lived_city.hmcatalog.json").ok, "原内容对照正常载入")
	var before_state: Dictionary = baseline.snapshot()
	var after_state: Dictionary = kernel.snapshot()
	check(before_state.content_digest != after_state.content_digest, "只有内容目录摘要允许随作者引用改变")
	before_state.erase("content_digest"); after_state.erase("content_digest")
	check(before_state == after_state, "全部世界事实、身份、随机流与实例计数保持相同")
	var previous: PackedByteArray = FileAccess.get_file_as_bytes(panel.prepared_path)
	panel.path_input.text = folder.path_join("edited.json"); await press(panel, "open")
	check(record(panel, "actors", "actor.chen").presentation_profile_id == "fixture.second", "真实文件重新打开保留引用")
	var manifest_path: String = project.path_join("game/data/presentation.json")
	var manifest_bytes: PackedByteArray = FileAccess.get_file_as_bytes(manifest_path)
	var broken_manifest: Dictionary = extended.duplicate(true); broken_manifest.components.erase("child.body")
	var manifest_file = FileAccess.open(manifest_path, FileAccess.WRITE); manifest_file.store_string(JSON.stringify(broken_manifest)); manifest_file.close()
	await press(panel, "prepare")
	check(not panel.client.last_reply.ok, "实际作者进程也拒绝缺组件清单")
	check(FileAccess.get_file_as_bytes(panel.prepared_path) == previous, "坏清单不替换此前有效目录")
	manifest_file = FileAccess.open(manifest_path, FileAccess.WRITE); manifest_file.store_buffer(manifest_bytes); manifest_file.close()
	# 作者协议允许坏引用草稿；选择器明确展示无效项，正式prepare不得覆盖旧文件。
	for invalid in ["missing.profile", ""]:
		check(panel.client.request("set_value", {"table": "actors", "record_id": "actor.chen", "pointer": "/presentation_profile_id", "value": invalid}), "提交无效引用草稿")
		await settle(panel)
		await select_record(panel, "actors", "actor.chen"); await select_field(panel, "/presentation_profile_id")
		check(panel.reference_picker.is_item_disabled(panel.reference_picker.selected), "未知或空引用不伪装成合法选择")
		await press(panel, "validate")
		check(panel.diagnostics.item_count > 0, "草稿诊断显示实际错误")
		for n in range(panel.diagnostics.item_count):
			if panel.diagnostics.get_item_metadata(n).path == "/actors/actor.chen/presentation_profile_id":
				panel.diagnostics.item_selected.emit(n)
				check(panel.selected_id == "actor.chen" and panel._field_pointer == "/presentation_profile_id", "引用错误可定位")
		await press(panel, "save")
		check(not panel.client.document.dirty, "无效作者草稿可真实保存")
		await press(panel, "prepare")
		check(not panel.client.last_reply.ok, "真实作者进程拒绝无效正式prepare")
		check(FileAccess.get_file_as_bytes(panel.prepared_path) == previous, "正式prepare拒绝前保留旧输出")
		await press(panel, "open")
		check(record(panel, "actors", "actor.chen").presentation_profile_id == invalid, "无效引用草稿重新打开仍在")

	panel.queue_free(); await process_frame
	check(art.catalog().configure(initial).ok, "测试结束恢复注册表")
	print("EDITOR_PRESENTATION_CHECKS=", checks, " FAILURES=", failures)
	quit(0 if failures == 0 else 1)
