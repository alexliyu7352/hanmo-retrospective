## 仅点验依赖表单的字段与真实协议接线；不启动GUI试玩或世界预览。
extends "res://tests/test_editor_packs.gd"

## 提交显式行数据，所有读取和版本检查仍交同一作者worker。
func choose_dependency(panel, path: String, version: String) -> void:
	panel._buttons.dependencies.pressed.emit()
	check(panel._dependency_rows.size() == 1, "one declared dependency row")
	if panel._dependency_rows.size() != 1: return
	var row: Dictionary = panel._dependency_rows[0]
	row.path.text = path; row.id.text = "base"; row.version.text = version
	panel._dependency_dialog.confirmed.emit(); await settle(panel)

## 只执行一条选根、引用、失败恢复和初态候选接线，不重复既有工坊门。
func _run() -> void:
	var project: String = ProjectSettings.globalize_path("res://").trim_suffix("/").get_base_dir()
	var folder: String = OS.get_user_data_dir().path_join("dependency-fields-" + str(Time.get_ticks_usec()))
	for name in ["base", "addon"]:
		copy_fixture(project.path_join("tests/fixtures/content_packs/compose/new_packs/" + name), folder.path_join(name))
	var panel = load("res://addons/hanmo_author/panel.gd").new(); root.add_child(panel)
	await process_frame
	panel.path_input.text = folder.path_join("addon/pack.json"); await press(panel, "connect")
	check(panel._is_pack() and panel._dependency_tables.is_empty(), "missing dependency does not invent a context")
	if not panel._is_pack():
		printerr(panel.status_label.text); panel.queue_free(); await process_frame; quit(1); return
	await choose_dependency(panel, folder.path_join("base/pack.json"), "1.0.0")
	check(panel.client.document.dependencies.size() == 1 and not panel._dependency_tables.is_empty(), "exact roots populate the read-only context")
	check(panel.pack_documents.keys() == ["content.json"], "dependency is not a writable content entry")
	await select_field(panel, "actors", "actor.extension", "/initial_place")
	var selected: int = -1
	for index in range(panel.reference_picker.item_count):
		if panel.reference_picker.get_item_metadata(index) == "base::west.office":
			selected = index
			check("只读依赖" in panel.reference_picker.get_item_text(index), "qualified candidate shows read-only owner")
	check(selected >= 0, "dependency location is selectable")
	if selected >= 0: panel.reference_picker.select(selected); await press(panel, "apply")
	check(panel._field_value == "base::west.office", "actual edit sends the qualified reference")
	var revision: Variant = panel.client.document.revision
	await choose_dependency(panel, folder.path_join("base/pack.json"), "9.0.0")
	check(panel.client.document.revision == revision and panel._dependency_tables.is_empty(), "failed selection preserves revision and clears old candidates")
	await choose_dependency(panel, folder.path_join("base/pack.json"), "1.0.0")
	check(not panel._dependency_tables.is_empty(), "successful reselection restores current context")
	panel._request("add_record", {"table": "scenarios", "record": {"id": "scenario.addon", "name_key": "base::name.scenario.procurement", "player_actor": "base::actor.player", "seed_text": "dependency-fields"}})
	await settle(panel)
	await select_field(panel, "scenarios", "scenario.addon", "/player_actor")
	panel._buttons.scenario_snapshot.pressed.emit(); await settle(panel)
	check(is_instance_valid(panel._snapshot_dialog), "existing initial-state form opens with dependency definitions")
	var found_actor: bool = false
	for row in panel._snapshot_rows:
		if row.table == "actors" and row.id == "base::actor.player": found_actor = true
	check(found_actor, "initial-state selection includes the dependency actor")
	panel._dismiss_scenario_snapshot()
	panel.queue_free(); await process_frame
	print("EDITOR_DEPENDENCY_FIELDS_CHECKS=", checks, " FAILURES=", failures)
	quit(0 if failures == 0 else 1)
