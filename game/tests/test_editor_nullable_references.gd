## 点验真实包表单的可空引用，不把节点信号测试称为用户鼠标验收。
extends "res://tests/test_editor_packs.gd"

## 从真实控件元数据选值；null与空字符串和字符串null保持不同身份。
func choose_reference(panel, value: Variant) -> void:
	var found: bool = false
	for index in range(panel.reference_picker.item_count):
		var candidate: Variant = panel.reference_picker.get_item_metadata(index)
		if typeof(candidate) == typeof(value) and candidate == value:
			check(not panel.reference_picker.is_item_disabled(index), "reference choice enabled: " + str(value))
			panel.reference_picker.select(index); found = true; break
	check(found, "exact reference choice exists: " + str(value))
	if found: await press(panel, "apply")

## 只核Schema允许null的选项，不将无效占位项当作可用的空引用。
func has_null_choice(panel) -> bool:
	for index in range(panel.reference_picker.item_count):
		if panel.reference_picker.get_item_metadata(index) == null and not panel.reference_picker.is_item_disabled(index): return true
	return false

## 通过表选择信号新增草稿，避免在测试里直接制造表单默认值。
func select_table(panel, table: String) -> void:
	for index in range(panel.table_picker.item_count):
		if panel.table_picker.get_item_metadata(index) == table:
			panel.table_picker.select(index); panel.table_picker.item_selected.emit(index); return
	check(false, "table exists: " + table)

## 复现旧空串并往返真实引用与JSON null，再核空包新增和保存重开。
func _run() -> void:
	var project: String = ProjectSettings.globalize_path("res://").trim_suffix("/").get_base_dir()
	var folder: String = OS.get_user_data_dir().path_join("editor-nullable-" + str(Time.get_ticks_usec()))
	var source: String = folder.path_join("pack")
	copy_fixture(project.path_join("tests/fixtures/content_packs/compose/new_packs/base"), source)
	var panel = load("res://addons/hanmo_author/panel.gd").new(); root.add_child(panel)
	await process_frame
	# 只在复制的测试作者中复刻旧默认值；冻结工坊作者从不改写。
	var content: Dictionary = JSON.parse_string(FileAccess.get_file_as_string(source.path_join("content.json")))
	content.places[0].parent_place = ""
	write_fixture_json(source.path_join("content.json"), content, panel)
	panel.path_input.text = source.path_join("pack.json"); await press(panel, "connect")
	check(panel._is_pack(), "actual pack session opened")
	await select_field(panel, "places", "west.street", "/parent_place")
	check(panel._field_value == "" and has_null_choice(panel), "old empty-string draft exposes null without silently replacing value")
	await choose_reference(panel, null)
	check(panel._field_value == null, "old empty-string draft is repaired as JSON null")
	var cases: Array = [
		["places", "west.office", "/parent_place", "west.street"],
		["actors", "actor.chen", "/dialogue_id", "dialog.office"],
		["roles", "role.office", "/organization_id", "org.office"],
	]
	for sample in cases:
		await select_field(panel, sample[0], sample[1], sample[2])
		check(panel._field_value == sample[3], "opening preserves existing reference: " + sample[2])
		check(panel.reference_picker.visible and not panel.value_input.visible and has_null_choice(panel), "nullable reference keeps picker metadata: " + sample[2])
		await choose_reference(panel, null)
		check(panel._field_value == null and panel.reference_picker.get_item_metadata(panel.reference_picker.selected) == null, "null stays selected after worker snapshot: " + sample[2])
		await choose_reference(panel, sample[3])
		check(panel._field_value == sample[3], "null can become a real reference: " + sample[2])
		await choose_reference(panel, null)
	await press(panel, "undo"); check(panel._field_value == "org.office", "nullable edit undo retains old reference")
	await press(panel, "redo"); check(panel._field_value == null, "nullable edit redo retains JSON null")
	for sample in [["actors", "actor.player", "/initial_place"], ["roles", "role.office", "/place_id"], ["organizations", "org.office", "/leader_actor"]]:
		await select_field(panel, sample[0], sample[1], sample[2])
		check(panel.reference_picker.visible and not has_null_choice(panel), "nonnullable reference has no null choice: " + sample[2])
	await press(panel, "save"); await press(panel, "open")
	content = JSON.parse_string(FileAccess.get_file_as_string(source.path_join("content.json")))
	check(content.places[0].parent_place == null and content.places[1].parent_place == null and content.actors[1].dialogue_id == null and content.roles[0].organization_id == null, "saved file contains JSON null for each changed nullable reference")
	for sample in cases:
		await select_field(panel, sample[0], sample[1], sample[2])
		check(panel._field_value == null and has_null_choice(panel), "reopen retains nullable picker: " + sample[2])
	await press(panel, "create_pack")
	var draft: String = folder.path_join("empty-pack")
	panel._create_fields.directory.text = draft
	panel._create_fields.id.text = "nullable_draft"; panel._create_fields.namespace.text = "nullable_draft"
	panel._create_fields.author.text = "可空引用节点测试"
	panel._create_dialog.confirmed.emit(); await settle(panel)
	check(panel._is_pack() and panel.pack_manifest.id == "nullable_draft", "empty draft created through real form")
	for sample in [["places", "/parent_place"], ["actors", "/dialogue_id"], ["roles", "/organization_id"]]:
		select_table(panel, sample[0]); await press(panel, "new_record")
		await select_field(panel, sample[0], sample[0] + ".new", sample[1])
		check(panel._field_value == null and has_null_choice(panel), "new nullable draft defaults to JSON null: " + sample[1])
		check(panel.reference_picker.get_item_metadata(panel.reference_picker.selected) == null, "new draft selects its legal null value: " + sample[1])
	await press(panel, "save"); await press(panel, "open")
	content = JSON.parse_string(FileAccess.get_file_as_string(draft.path_join("content.json")))
	check(content.places[0].parent_place == null and content.actors[0].dialogue_id == null and content.roles[0].organization_id == null, "new draft null defaults survive real save and reopen")
	check(content.places[0].settlement_id == "" and content.actors[0].initial_place == "", "nonnullable missing references keep visible incomplete draft values")
	panel.queue_free(); await process_frame
	print("EDITOR_NULLABLE_REFERENCE_CHECKS=", checks, " FAILURES=", failures)
	quit(0 if failures == 0 else 1)
