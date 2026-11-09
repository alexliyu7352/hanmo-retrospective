## 通过真实Editor节点和本地worker验证包表单，不把headless信号当鼠标验收。
extends SceneTree
var checks: int = 0
var failures: int = 0

## 场景树完成初始化后再建立实际面板及管道客户端。
func _init() -> void:
	call_deferred("_run")

## 保留每个行为断言的结果，失败时仍尽量释放作者进程。
func check(value: bool, label: String) -> void:
	checks += 1
	if not value:
		failures += 1; printerr("FAIL: ", label)

## 仅复制固定测试fixture中的普通成员，不修改原作者或event安装目录。
func copy_fixture(source: String, target: String) -> void:
	DirAccess.make_dir_recursive_absolute(target)
	for name in DirAccess.get_files_at(source):
		check(DirAccess.copy_absolute(source.path_join(name), target.path_join(name)) == OK, "fixture member copied: " + name)
	for name in DirAccess.get_directories_at(source):
		copy_fixture(source.path_join(name), target.path_join(name))

## 等待真实异步worker和面板串联snapshot完成，超时保持显式失败。
func settle(panel) -> void:
	var deadline: int = Time.get_ticks_msec() + 30000
	for _frame in range(4): await process_frame
	while panel.client.busy and Time.get_ticks_msec() < deadline: await process_frame
	for _frame in range(4): await process_frame
	check(not panel.client.busy, "worker request settled")

## 发射现有按钮的实际信号，不直接改写面板缓存或作者文件。
func press(panel, action: String) -> void:
	var control: Button = panel._buttons.get(action)
	check(control != null and not control.disabled, "button usable: " + action)
	if control != null and not control.disabled:
		control.pressed.emit(); await settle(panel)

## 通过列表和字段树的选中信号复用实际表单路径。
func select_field(panel, table: String, identifier: String, pointer: String) -> void:
	for index in range(panel.table_picker.item_count):
		if panel.table_picker.get_item_metadata(index) == table:
			panel.table_picker.select(index); panel.table_picker.item_selected.emit(index); break
	for index in range(panel.record_list.item_count):
		if panel.record_list.get_item_metadata(index) == identifier:
			panel.record_list.select(index); panel.record_list.item_selected.emit(index); break
	var selected_value: bool = table in ["texts", "flags"] and panel._field_pointer == pointer
	check(panel.selected_id == identifier and (panel._nodes.has(pointer) or selected_value), "field selectable: " + pointer)
	if panel._nodes.has(pointer):
		panel._nodes[pointer].select(0); panel.field_tree.item_selected.emit()
	await process_frame

## 选择真实manifest表单字段，后续应用由版本门和后端Schema处理。
func select_manifest(panel, field: String) -> void:
	for index in range(panel.manifest_field_picker.item_count):
		if panel.manifest_field_picker.get_item_metadata(index) == field:
			panel.manifest_field_picker.select(index); panel.manifest_field_picker.item_selected.emit(index); return
	check(false, "manifest field exists: " + field)

## 只为多入口测试准备原始fixture，不把准备数据计作用户工坊输入。
func write_fixture_json(path: String, value: Dictionary, panel) -> void:
	var file = FileAccess.open(path, FileAccess.WRITE)
	file.store_string(JSON.stringify(panel._json_integers(value))); file.close()

## 打开真实普通包，编辑、撤销、保存重开并完成正式组合与失败诊断。
func _run() -> void:
	if "--single-author" in OS.get_cmdline_user_args():
		await _run_single_author(); return
	if "--diagnostics" in OS.get_cmdline_user_args():
		await _run_diagnostics(); return
	if "--lifecycle" in OS.get_cmdline_user_args():
		await _run_lifecycle(); return
	var project: String = ProjectSettings.globalize_path("res://").trim_suffix("/").get_base_dir()
	var folder: String = OS.get_user_data_dir().path_join("editor-packs-" + str(Time.get_ticks_usec()))
	var source: String = folder.path_join("pack")
	copy_fixture(project.path_join("tests/fixtures/content_packs/compose/new_packs/base"), source)
	var panel = load("res://addons/hanmo_author/panel.gd").new(); root.add_child(panel)
	await process_frame
	panel.path_input.text = source.path_join("pack.json")
	await press(panel, "connect")
	check(panel._is_pack() and panel.selected_entry == "content.json", "pack mode selects real content entry")
	check(panel.client.document.has("source_sha256"), "client received exact manifest token")
	check(not panel._buttons.preview.disabled and panel._buttons.save_as.disabled, "pack preview is available through its guarded preparer")
	if not panel._is_pack():
		printerr(panel.status_label.text); panel.queue_free(); await process_frame; quit(1); return
	await select_field(panel, "actors", "actor.player", "/attributes/charisma")
	panel.value_input.text = "67"; await press(panel, "apply")
	check(panel.client.document.dirty, "real edit marks pack dirty")
	await press(panel, "undo"); check(not panel.client.document.dirty, "undo restores original pack content")
	await press(panel, "redo"); check(panel.client.document.dirty, "redo restores authored edit")
	panel.compose_path_input.text = folder.path_join("preview")
	await press(panel, "prepare")
	check(panel.client.last_reply.get("error", {}).get("code") == "UNSAVED_CHANGES", "dirty compose rejected through panel")
	await press(panel, "save"); check(not panel.client.document.dirty, "pack save completed")
	await press(panel, "open")
	await select_field(panel, "actors", "actor.player", "/attributes/charisma")
	check(str(panel._field_value) == "67" or float(panel._field_value) == 67.0, "reopened pack retains field edit")
	await press(panel, "prepare")
	check(panel.client.last_reply.get("ok", false), "official pack composition succeeds")
	check(FileAccess.file_exists(folder.path_join("preview/pack-set.lock.json")), "composition published a real lock")
	check(not panel.client.last_reply.get("runtime_verified", true), "composition does not claim runtime verification")
	select_manifest(panel, "author")
	panel.manifest_value_input.text = "面板测试作者"; await press(panel, "apply_manifest")
	check(panel.pack_manifest.author == "面板测试作者" and panel.client.document.dirty, "manifest controlled field edited")
	select_manifest(panel, "namespace")
	panel.manifest_value_input.text = "Bad Namespace"; await press(panel, "apply_manifest")
	check(panel.client.last_reply.get("error", {}).get("code") == "InvalidPackManifest", "invalid manifest reports original schema error")
	check(panel.pack_manifest.namespace == "base" and panel.diagnostics.item_count > 0, "invalid manifest preserves state and diagnostics")
	await press(panel, "save")
	var saved_manifest: Dictionary = JSON.parse_string(FileAccess.get_file_as_string(source.path_join("pack.json")))
	check(saved_manifest.author == "面板测试作者", "manifest actually saved")
	var extra: Dictionary = JSON.parse_string(FileAccess.get_file_as_string(source.path_join("content.json")))
	for key in extra:
		if extra[key] is Array: extra[key] = []
		elif extra[key] is Dictionary: extra[key] = {}
	extra.texts = {"entry.two.text": "跨入口名字"}
	write_fixture_json(source.path_join("second.json"), extra, panel)
	saved_manifest.content_entries.append("second.json"); write_fixture_json(source.path_join("pack.json"), saved_manifest, panel)
	await press(panel, "open")
	if not panel.client.last_reply.get("ok", false): printerr(panel.status_label.text)
	check(panel.pack_entry_picker.item_count == 2, "all declared content entries available")
	await select_field(panel, "actors", "actor.player", "/name_key")
	var found_reference: bool = false
	for index in range(panel.reference_picker.item_count):
		if panel.reference_picker.get_item_metadata(index) == "entry.two.text":
			found_reference = "second.json" in panel.reference_picker.get_item_text(index)
			panel.reference_picker.select(index)
	check(found_reference, "cross-entry reference shows author file ownership")
	await press(panel, "apply"); await press(panel, "save")
	for index in range(panel.pack_entry_picker.item_count):
		if panel.pack_entry_picker.get_item_metadata(index) == "second.json":
			panel.pack_entry_picker.select(index); panel.pack_entry_picker.item_selected.emit(index); break
	check(panel.selected_entry == "second.json" and panel.pack.texts.has("entry.two.text"), "entry selector binds only chosen child view")
	await select_field(panel, "texts", "entry.two.text", "/value")
	panel.value_input.text = "跨入口修改"; await press(panel, "apply"); await press(panel, "save")
	check(JSON.parse_string(FileAccess.get_file_as_string(source.path_join("second.json"))).texts["entry.two.text"] == "跨入口修改", "selected child save reaches exact source")
	await press(panel, "prepare")
	check(panel.client.last_reply.get("ok", false), "multi-entry pack composes with exact reference")
	await press(panel, "create_pack")
	check(is_instance_valid(panel._create_dialog), "explicit create dialog available")
	var new_pack: String = folder.path_join("new-pack")
	panel._create_fields.directory.text = new_pack
	panel._create_fields.id.text = "draft_pack"; panel._create_fields.namespace.text = "draft_pack"
	panel._create_fields.author.text = "创建测试作者"
	panel._create_dialog.confirmed.emit(); await settle(panel)
	check(panel._is_pack() and panel.pack_manifest.id == "draft_pack", "create dialog opened actual new pack session")
	check(FileAccess.file_exists(new_pack.path_join("pack.json")) and panel.pack.actors.is_empty(), "created content is an empty author draft")
	await press(panel, "new_record")
	check(panel.pack.texts.has("texts.new"), "existing new-record control creates a draft text in selected table")
	await press(panel, "save"); await press(panel, "open")
	check(panel.pack.texts.has("texts.new"), "created pack edits survive save and reopen")
	panel.compose_path_input.text = folder.path_join("incomplete-preview")
	await press(panel, "prepare")
	check(not panel.client.last_reply.get("ok", true) and panel.diagnostics.item_count > 0, "incomplete draft cannot claim composition success")
	panel.queue_free(); await process_frame
	print("EDITOR_PACKS_CHECKS=", checks, " FAILURES=", failures)
	quit(0 if failures == 0 else 1)

## 点验旧单文档表单、引用选择、另存和重开，不加载整套表现资源。
func _run_single_author() -> void:
	var project: String = ProjectSettings.globalize_path("res://").trim_suffix("/").get_base_dir()
	var folder: String = OS.get_user_data_dir().path_join("editor-pack-adjacent-" + str(Time.get_ticks_usec()))
	DirAccess.make_dir_recursive_absolute(folder)
	var source: String = folder.path_join("story.json")
	check(DirAccess.copy_absolute(project.path_join("content/foundation/two_stories.json"), source) == OK, "single author fixture copied")
	var panel = load("res://addons/hanmo_author/panel.gd").new(); root.add_child(panel)
	await process_frame
	panel.path_input.text = source; await press(panel, "connect")
	check(not panel._is_pack() and not panel._pack_controls.visible, "legacy single author mode preserved")
	check(not panel._buttons.save_as.disabled and not panel._buttons.preview.disabled, "old author actions remain enabled")
	await select_field(panel, "actors", "actor.he", "/attributes/charisma")
	panel.value_input.text = "66"; await press(panel, "apply")
	check(panel.client.document.dirty, "old field applies through original operation")
	await press(panel, "undo"); check(not panel.client.document.dirty, "old undo unchanged")
	await press(panel, "redo")
	await select_field(panel, "actors", "actor.he", "/initial_place")
	var found: bool = false
	for index in range(panel.reference_picker.item_count):
		if panel.reference_picker.get_item_metadata(index) == "east.entrance": found = true
	check(found, "old reference picker unchanged")
	await press(panel, "save_as")
	var destination: String = folder.path_join("copy.json")
	for child in panel.get_children():
		if child is FileDialog: child.file_selected.emit(destination); break
	await settle(panel)
	check(FileAccess.file_exists(destination) and not panel.client.document.dirty, "old Save As actual file written")
	panel.path_input.text = destination; await press(panel, "open")
	await select_field(panel, "actors", "actor.he", "/attributes/charisma")
	check(float(panel._field_value) == 66.0, "old reopen retains exact edit")
	panel.queue_free(); await process_frame
	print("EDITOR_PACK_SINGLE_CHECKS=", checks, " FAILURES=", failures)
	quit(0 if failures == 0 else 1)

## 从真实错误组合的来源信息跳回入口和字段，不虚构诊断或放宽数值约束。
func _run_diagnostics() -> void:
	var project: String = ProjectSettings.globalize_path("res://").trim_suffix("/").get_base_dir()
	var folder: String = OS.get_user_data_dir().path_join("editor-pack-diagnostics-" + str(Time.get_ticks_usec()))
	var source: String = folder.path_join("pack")
	copy_fixture(project.path_join("tests/fixtures/content_packs/compose/new_packs/base"), source)
	var panel = load("res://addons/hanmo_author/panel.gd").new(); root.add_child(panel)
	await process_frame
	panel.path_input.text = source.path_join("pack.json"); await press(panel, "connect")
	await select_field(panel, "actors", "actor.player", "/attributes/charisma")
	panel.value_input.text = "101"; await press(panel, "apply"); await press(panel, "save")
	await select_field(panel, "texts", "office.hello", "/value")
	panel.compose_path_input.text = folder.path_join("invalid-preview"); await press(panel, "prepare")
	check(panel.client.last_reply.get("error", {}).get("code") == "InvalidPackSource", "actual schema failure reported")
	check(panel.diagnostics.item_count > 0, "source diagnostic visible")
	if panel.diagnostics.item_count > 0:
		panel.diagnostics.select(0); panel.diagnostics.item_selected.emit(0)
	check(panel.selected_entry == "content.json" and panel.selected_id == "actor.player" and panel._field_pointer == "/attributes/charisma", "diagnostic resolves exact original author field")
	check(not FileAccess.file_exists(folder.path_join("invalid-preview/pack-set.lock.json")), "invalid content did not publish a lock")
	panel.queue_free(); await process_frame
	print("EDITOR_PACK_DIAGNOSTIC_CHECKS=", checks, " FAILURES=", failures)
	quit(0 if failures == 0 else 1)

## 仅验证面板进程生命周期门；不重复准备器的真实内容工程启动或GUI验收。
func _run_lifecycle() -> void:
	var project: String = ProjectSettings.globalize_path("res://").trim_suffix("/").get_base_dir()
	var folder: String = OS.get_user_data_dir().path_join("editor-pack-lifecycle-" + str(Time.get_ticks_usec()))
	var source: String = folder.path_join("pack")
	copy_fixture(project.path_join("tests/fixtures/content_packs/compose/new_packs/base"), source)
	var panel = load("res://addons/hanmo_author/panel.gd").new(); root.add_child(panel)
	await process_frame
	panel.path_input.text = source.path_join("pack.json"); await press(panel, "connect")
	check(panel._preview_available(), "actual fixed engine version accepted")
	check(not panel._buttons.preview.disabled and panel.scenario_picker.item_count > 0, "preview operation and qualified scenario exposed")
	check(str(panel.scenario_picker.get_item_metadata(0)).begins_with("base::"), "pack preview selects qualified scenario")
	var child_script: String = folder.path_join("lifecycle-child.gd")
	var stream = FileAccess.open(child_script, FileAccess.WRITE)
	stream.store_string("extends SceneTree\nfunc _init():\n await create_timer(0.5).timeout\n quit()\n"); stream.close()
	var pid: int = OS.create_process(OS.get_executable_path(), PackedStringArray(["--headless", "--path", ProjectSettings.globalize_path("res://"), "--script", child_script]))
	check(pid > 0 and OS.is_process_running(pid), "bounded real child process started")
	panel._preview_pid = pid
	var sequence: int = panel.client._sequence
	await press(panel, "preview"); await press(panel, "preview")
	check(panel.client._sequence == sequence and panel._preview_pid == pid, "repeated preview clicks cannot replace a live process or start preparation")
	var deadline: int = Time.get_ticks_msec() + 5000
	while OS.is_process_running(pid) and Time.get_ticks_msec() < deadline: await process_frame
	check(not OS.is_process_running(pid) and OS.get_process_exit_code(pid) == 0, "old process exits normally without kill")
	check(panel._preview_available() and panel._preview_pid == -1, "normal exit allows a later explicit preview")
	panel._preview_context = {"document_id": panel.client.document.document_id, "revision": panel.client.document.revision, "source_sha256": panel.client.document.source_sha256}
	check(panel._preview_context_current(), "unchanged author session remains eligible")
	await select_field(panel, "actors", "actor.player", "/attributes/charisma")
	panel.value_input.text = "67"; await press(panel, "apply")
	check(not panel._preview_context_current(), "changed pack revision blocks a late preview result")
	panel._preview_import_pid = OS.get_process_id()
	sequence = panel.client._sequence
	panel.prepare_content(true)
	check(panel.client._sequence == sequence, "an active import blocks overlapping preparation")
	panel._preview_import_pid = -1
	panel._preview_import_pid = pid
	panel._preview_pending = {"import_log": folder.path_join("missing-import.log")}
	panel._process(0.0)
	check(panel._preview_pid == -1 and "未启动游戏" in panel.status_label.text, "missing or failed import evidence cannot launch a game")
	panel.queue_free(); await process_frame
	print("EDITOR_PACK_LIFECYCLE_CHECKS=", checks, " FAILURES=", failures)
	quit(0 if failures == 0 else 1)
